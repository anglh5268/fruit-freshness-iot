#include "as7341.h"
#include "sensor_bus.h"

#include <stdbool.h>
#include <stddef.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define AS7341_I2C_ADDRESS       0x39

#define AS7341_REG_ENABLE        0x80
#define AS7341_REG_ATIME         0x81
#define AS7341_REG_ID            0x92
#define AS7341_REG_CH0_DATA_L    0x95
#define AS7341_REG_STATUS2       0xA3
#define AS7341_REG_CFG0          0xA9
#define AS7341_REG_CFG1          0xAA
#define AS7341_REG_CFG6          0xAF
#define AS7341_REG_CONFIG        0x70
#define AS7341_REG_LED           0x74
#define AS7341_REG_ASTEP_L       0xCA
#define AS7341_REG_ASTEP_H       0xCB

#define AS7341_EXPECTED_ID       0x24

#define AS7341_ENABLE_PON        (1U << 0)
#define AS7341_ENABLE_SP_EN      (1U << 1)
#define AS7341_ENABLE_SMUX_EN    (1U << 4)

#define AS7341_STATUS2_AVALID    (1U << 6)

#define AS7341_SMUX_COMMAND_MASK 0x18
#define AS7341_SMUX_COMMAND_WRITE 0x10

#define AS7341_GAIN_MASK         0x1F
#define AS7341_GAIN_128X 8

#define AS7341_CFG0_REG_BANK     (1U << 4)
#define AS7341_CONFIG_LED_SEL    (1U << 3)
#define AS7341_LED_ACT           (1U << 7)
#define AS7341_LED_DRIVE_MASK    0x7F
#define AS7341_LED_SETTLE_MS     5


static const char *TAG = "AS7341";

static i2c_master_dev_handle_t s_as7341_handle = NULL;
static bool s_initialized = false;


/* F1、F2、F3、F4、Clear、NIR */
static const uint8_t s_smux_low[20] = {
    0x30, 0x01, 0x00, 0x00, 0x00,
    0x42, 0x00, 0x00, 0x50, 0x00,
    0x00, 0x00, 0x20, 0x04, 0x00,
    0x30, 0x01, 0x50, 0x00, 0x06
};


/* F5、F6、F7、F8、Clear、NIR */
static const uint8_t s_smux_high[20] = {
    0x00, 0x00, 0x00, 0x40, 0x02,
    0x00, 0x10, 0x03, 0x50, 0x10,
    0x03, 0x00, 0x00, 0x00, 0x24,
    0x00, 0x00, 0x50, 0x00, 0x06
};


static esp_err_t as7341_write_register(
    uint8_t register_address,
    uint8_t value)
{
    if (s_as7341_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buffer[2] = {
        register_address,
        value
    };

    return i2c_master_transmit(
        s_as7341_handle,
        buffer,
        sizeof(buffer),
        1000
    );
}


static esp_err_t as7341_read_register(
    uint8_t register_address,
    uint8_t *value)
{
    if (s_as7341_handle == NULL || value == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return i2c_master_transmit_receive(
        s_as7341_handle,
        &register_address,
        1,
        value,
        1,
        1000
    );
}


static esp_err_t as7341_read_block(
    uint8_t start_register,
    uint8_t *data,
    size_t data_length)
{
    if (s_as7341_handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return i2c_master_transmit_receive(
        s_as7341_handle,
        &start_register,
        1,
        data,
        data_length,
        1000
    );
}


static esp_err_t as7341_write_mask(
    uint8_t register_address,
    uint8_t mask,
    uint8_t value)
{
    uint8_t old_value = 0;

    esp_err_t err = as7341_read_register(
        register_address,
        &old_value
    );

    if (err != ESP_OK) {
        return err;
    }

    uint8_t new_value =
        (old_value & (uint8_t)(~mask)) |
        (value & mask);

    return as7341_write_register(
        register_address,
        new_value
    );
}


static esp_err_t as7341_wait_smux(void)
{
    for (int attempt = 0; attempt < 100; attempt++) {
        uint8_t enable_value = 0;

        esp_err_t err = as7341_read_register(
            AS7341_REG_ENABLE,
            &enable_value
        );

        if (err != ESP_OK) {
            return err;
        }

        if ((enable_value & AS7341_ENABLE_SMUX_EN) == 0) {
            return ESP_OK;
        }

        vTaskDelay(1);
    }

    ESP_LOGE(TAG, "SMUX operation timed out");

    return ESP_ERR_TIMEOUT;
}


static esp_err_t as7341_wait_data_ready(void)
{
    for (int attempt = 0; attempt < 200; attempt++) {
        uint8_t status2 = 0;

        esp_err_t err = as7341_read_register(
            AS7341_REG_STATUS2,
            &status2
        );

        if (err != ESP_OK) {
            return err;
        }

        if ((status2 & AS7341_STATUS2_AVALID) != 0) {
            return ESP_OK;
        }

        vTaskDelay(1);
    }

    ESP_LOGE(TAG, "Spectral measurement timed out");

    return ESP_ERR_TIMEOUT;
}


static esp_err_t as7341_configure_smux(
    const uint8_t configuration[20])
{
    esp_err_t err;

    /* 修改配置前必须停止光谱测量 */
    err = as7341_write_mask(
        AS7341_REG_ENABLE,
        AS7341_ENABLE_SP_EN,
        0
    );

    if (err != ESP_OK) {
        return err;
    }

    /* 选择“从RAM写入SMUX”命令 */
    err = as7341_write_mask(
        AS7341_REG_CFG6,
        AS7341_SMUX_COMMAND_MASK,
        AS7341_SMUX_COMMAND_WRITE
    );

    if (err != ESP_OK) {
        return err;
    }

    /* SMUX配置寄存器地址为0x00至0x13 */
    for (uint8_t index = 0; index < 20; index++) {
        err = as7341_write_register(
            index,
            configuration[index]
        );

        if (err != ESP_OK) {
            return err;
        }
    }

    /* 让芯片执行SMUX配置 */
    err = as7341_write_mask(
        AS7341_REG_ENABLE,
        AS7341_ENABLE_SMUX_EN,
        AS7341_ENABLE_SMUX_EN
    );

    if (err != ESP_OK) {
        return err;
    }

    return as7341_wait_smux();
}


static esp_err_t as7341_start_and_read_adc(
    uint16_t adc_values[6])
{
    esp_err_t err = as7341_write_mask(
        AS7341_REG_ENABLE,
        AS7341_ENABLE_SP_EN,
        AS7341_ENABLE_SP_EN
    );

    if (err != ESP_OK) {
        return err;
    }

    err = as7341_wait_data_ready();

    if (err != ESP_OK) {
        return err;
    }

    uint8_t raw_data[12];

    err = as7341_read_block(
        AS7341_REG_CH0_DATA_L,
        raw_data,
        sizeof(raw_data)
    );

    if (err != ESP_OK) {
        return err;
    }

    for (int channel = 0; channel < 6; channel++) {
        adc_values[channel] =
            (uint16_t)raw_data[channel * 2] |
            ((uint16_t)raw_data[channel * 2 + 1] << 8);
    }

    return ESP_OK;
}


esp_err_t as7341_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (s_as7341_handle == NULL) {
        i2c_master_bus_handle_t bus_handle =
            sensor_bus_get_handle();

        if (bus_handle == NULL) {
            ESP_LOGE(TAG, "I2C bus is not initialized");
            return ESP_ERR_INVALID_STATE;
        }

        i2c_device_config_t device_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = AS7341_I2C_ADDRESS,
            .scl_speed_hz = 400000,
        };

        esp_err_t err = i2c_master_bus_add_device(
            bus_handle,
            &device_config,
            &s_as7341_handle
        );

        if (err != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Failed to add I2C device: %s",
                esp_err_to_name(err)
            );

            return err;
        }
    }

    uint8_t chip_id = 0;

    esp_err_t err = as7341_read_register(
        AS7341_REG_ID,
        &chip_id
    );

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to read ID register: %s",
            esp_err_to_name(err)
        );

        return err;
    }

    ESP_LOGI(
        TAG,
        "ID register 0x92 = 0x%02X",
        chip_id
    );

    if ((chip_id & 0xFC) != AS7341_EXPECTED_ID) {
        ESP_LOGE(TAG, "Unexpected chip ID");
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* 开启内部振荡器和电源 */
    err = as7341_write_mask(
        AS7341_REG_ENABLE,
        AS7341_ENABLE_PON,
        AS7341_ENABLE_PON
    );

    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(1);

    /*
     * 积分时间：
     * (ATIME + 1) × (ASTEP + 1) × 2.78us
     * = 10 × 600 × 2.78us
     * ≈ 16.7ms
     */
    err = as7341_write_register(
        AS7341_REG_ATIME,
        9
    );

    if (err != ESP_OK) {
        return err;
    }

    uint16_t astep = 299;

    err = as7341_write_register(
        AS7341_REG_ASTEP_L,
        (uint8_t)(astep & 0xFF)
    );

    if (err != ESP_OK) {
        return err;
    }

    err = as7341_write_register(
        AS7341_REG_ASTEP_H,
        (uint8_t)(astep >> 8)
    );

    if (err != ESP_OK) {
        return err;
    }

    /* 设置光谱ADC增益为64倍 */
    err = as7341_write_mask(
        AS7341_REG_CFG1,
        AS7341_GAIN_MASK,
        AS7341_GAIN_128X
    );

    if (err != ESP_OK) {
        return err;
    }

    s_initialized = true;

    ESP_LOGI(
        TAG,
        "Initialization completed: integration 8.34ms, gain 128x"
    );

    return ESP_OK;
}


esp_err_t as7341_read_spectrum(
    as7341_spectral_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t low_adc[6];
    uint16_t high_adc[6];

    esp_err_t err = as7341_configure_smux(
        s_smux_low
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure low SMUX");
        return err;
    }

    err = as7341_start_and_read_adc(low_adc);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read F1-F4");
        return err;
    }

    err = as7341_configure_smux(
        s_smux_high
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure high SMUX");
        return err;
    }

    err = as7341_start_and_read_adc(high_adc);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read F5-F8");
        return err;
    }

    /* 测量完成后停止光谱引擎 */
    err = as7341_write_mask(
        AS7341_REG_ENABLE,
        AS7341_ENABLE_SP_EN,
        0
    );

    if (err != ESP_OK) {
        return err;
    }

    data->f1_415nm = low_adc[0];
    data->f2_445nm = low_adc[1];
    data->f3_480nm = low_adc[2];
    data->f4_515nm = low_adc[3];

    data->f5_555nm = high_adc[0];
    data->f6_590nm = high_adc[1];
    data->f7_630nm = high_adc[2];
    data->f8_680nm = high_adc[3];

    data->clear = high_adc[4];
    data->nir = high_adc[5];

    return ESP_OK;
}


esp_err_t as7341_set_led(
    bool on,
    uint8_t drive)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (drive > AS7341_LED_DRIVE_MASK) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t original_cfg0 = 0;

    esp_err_t err = as7341_read_register(
        AS7341_REG_CFG0,
        &original_cfg0
    );

    if (err != ESP_OK) {
        return err;
    }

    /* 切换到0x60~0x74寄存器区。 */
    err = as7341_write_register(
        AS7341_REG_CFG0,
        original_cfg0 | AS7341_CFG0_REG_BANK
    );

    if (err == ESP_OK) {
        /* 允许LED寄存器控制LDR恒流下拉端。 */
        err = as7341_write_mask(
            AS7341_REG_CONFIG,
            AS7341_CONFIG_LED_SEL,
            AS7341_CONFIG_LED_SEL
        );
    }

    if (err == ESP_OK) {
        const uint8_t led_value =
            (on ? AS7341_LED_ACT : 0U) |
            (drive & AS7341_LED_DRIVE_MASK);

        err = as7341_write_register(
            AS7341_REG_LED,
            led_value
        );
    }

    /* 无论LED配置是否成功，都尝试恢复原寄存器区。 */
    esp_err_t restore_err = as7341_write_register(
        AS7341_REG_CFG0,
        original_cfg0
    );

    if (err != ESP_OK) {
        return err;
    }

    if (restore_err != ESP_OK) {
        return restore_err;
    }

    ESP_LOGD(
        TAG,
        "Fill LED %s, drive=%u (approximately %u mA)",
        on ? "ON" : "OFF",
        (unsigned int)drive,
        (unsigned int)(4U + 2U * drive)
    );

    return ESP_OK;
}


static uint16_t as7341_subtract_u16(
    uint16_t illuminated,
    uint16_t ambient)
{
    return illuminated > ambient
        ? (uint16_t)(illuminated - ambient)
        : 0;
}


static void as7341_subtract_spectrum(
    const as7341_spectral_data_t *ambient,
    const as7341_spectral_data_t *illuminated,
    as7341_spectral_data_t *net)
{
    net->f1_415nm = as7341_subtract_u16(
        illuminated->f1_415nm,
        ambient->f1_415nm
    );
    net->f2_445nm = as7341_subtract_u16(
        illuminated->f2_445nm,
        ambient->f2_445nm
    );
    net->f3_480nm = as7341_subtract_u16(
        illuminated->f3_480nm,
        ambient->f3_480nm
    );
    net->f4_515nm = as7341_subtract_u16(
        illuminated->f4_515nm,
        ambient->f4_515nm
    );
    net->f5_555nm = as7341_subtract_u16(
        illuminated->f5_555nm,
        ambient->f5_555nm
    );
    net->f6_590nm = as7341_subtract_u16(
        illuminated->f6_590nm,
        ambient->f6_590nm
    );
    net->f7_630nm = as7341_subtract_u16(
        illuminated->f7_630nm,
        ambient->f7_630nm
    );
    net->f8_680nm = as7341_subtract_u16(
        illuminated->f8_680nm,
        ambient->f8_680nm
    );
    net->clear = as7341_subtract_u16(
        illuminated->clear,
        ambient->clear
    );
    net->nir = as7341_subtract_u16(
        illuminated->nir,
        ambient->nir
    );
}


esp_err_t as7341_read_reflectance(
    as7341_spectral_data_t *ambient,
    as7341_spectral_data_t *illuminated,
    as7341_spectral_data_t *net,
    uint8_t led_drive)
{
    if (ambient == NULL ||
        illuminated == NULL ||
        net == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (led_drive > AS7341_LED_DRIVE_MASK) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = as7341_set_led(false, led_drive);

    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(AS7341_LED_SETTLE_MS));

    err = as7341_read_spectrum(ambient);

    if (err != ESP_OK) {
        return err;
    }

    err = as7341_set_led(true, led_drive);

    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(AS7341_LED_SETTLE_MS));

    err = as7341_read_spectrum(illuminated);

    /* 即使开灯采集失败，也必须尝试关灯。 */
    esp_err_t led_off_err = as7341_set_led(
        false,
        led_drive
    );

    if (err != ESP_OK) {
        return err;
    }

    if (led_off_err != ESP_OK) {
        return led_off_err;
    }

    as7341_subtract_spectrum(
        ambient,
        illuminated,
        net
    );

    return ESP_OK;
}
