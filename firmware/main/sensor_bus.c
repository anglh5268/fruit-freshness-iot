#include "sensor_bus.h"

#include "driver/gpio.h"
#include "esp_log.h"

#define SENSOR_I2C_PORT       I2C_NUM_1
#define SENSOR_I2C_SDA_GPIO   GPIO_NUM_4
#define SENSOR_I2C_SCL_GPIO   GPIO_NUM_5

static const char *TAG = "SENSOR_BUS";

static i2c_master_bus_handle_t s_bus_handle = NULL;

esp_err_t sensor_bus_init(void)
{
    if (s_bus_handle != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = SENSOR_I2C_PORT,
        .sda_io_num = SENSOR_I2C_SDA_GPIO,
        .scl_io_num = SENSOR_I2C_SCL_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(
        &bus_config,
        &s_bus_handle
    );

    if (err == ESP_OK) {
        ESP_LOGI(
            TAG,
            "I2C initialized: SDA=GPIO%d, SCL=GPIO%d",
            SENSOR_I2C_SDA_GPIO,
            SENSOR_I2C_SCL_GPIO
        );
    } else {
        ESP_LOGE(
            TAG,
            "I2C initialization failed: %s",
            esp_err_to_name(err)
        );
    }

    return err;
}

i2c_master_bus_handle_t sensor_bus_get_handle(void)
{
    return s_bus_handle;
}

void sensor_bus_scan(void)
{
    if (s_bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus is not initialized");
        return;
    }

    int found_count = 0;

    ESP_LOGI(TAG, "Starting I2C scan");

    for (uint16_t address = 0x08;
         address <= 0x77;
         address++) {

        esp_err_t err = i2c_master_probe(
            s_bus_handle,
            address,
            50
        );

        if (err == ESP_OK) {
            ESP_LOGI(
                TAG,
                "Found device at address 0x%02X",
                address
            );
            found_count++;
        } else if (err == ESP_ERR_TIMEOUT) {
            ESP_LOGE(
                TAG,
                "I2C timeout at address 0x%02X",
                address
            );
            ESP_LOGE(
                TAG,
                "Check SDA, SCL, GND and pull-up resistors"
            );
            return;
        }
    }

    ESP_LOGI(
        TAG,
        "Scan finished, found %d device(s)",
        found_count
    );
}