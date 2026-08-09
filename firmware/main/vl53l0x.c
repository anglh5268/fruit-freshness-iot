#include "vl53l0x.h"
#include "sensor_bus.h"

#include <stdbool.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "vl53l0x_api.h"


#define VL53L0X_I2C_ADDRESS       0x29
#define VL53L0X_REG_MODEL_ID      0xC0
#define VL53L0X_EXPECTED_MODEL_ID 0xEE
/* 固定距离修正：传感器读数比实际值大约多30 mm */
#define VL53L0X_DISTANCE_CORRECTION_MM 30

static const char *TAG = "VL53L0X";

static i2c_master_dev_handle_t s_i2c_handle = NULL;
static VL53L0X_Dev_t s_device;
static bool s_initialized = false;


static esp_err_t check_api_status(
    const char *operation,
    VL53L0X_Error status)
{
    if (status == VL53L0X_ERROR_NONE) {
        return ESP_OK;
    }

    ESP_LOGE(
        TAG,
        "%s failed, ST status = %d",
        operation,
        (int)status
    );

    return ESP_FAIL;
}


static esp_err_t read_model_id(uint8_t *model_id)
{
    if (s_i2c_handle == NULL || model_id == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t register_address = VL53L0X_REG_MODEL_ID;

    return i2c_master_transmit_receive(
        s_i2c_handle,
        &register_address,
        1,
        model_id,
        1,
        1000
    );
}


esp_err_t vl53l0x_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (s_i2c_handle == NULL) {
        i2c_master_bus_handle_t bus_handle =
            sensor_bus_get_handle();

        if (bus_handle == NULL) {
            ESP_LOGE(TAG, "I2C bus is not initialized");
            return ESP_ERR_INVALID_STATE;
        }

        i2c_device_config_t device_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = VL53L0X_I2C_ADDRESS,
            .scl_speed_hz = 400000,
        };

        esp_err_t err = i2c_master_bus_add_device(
            bus_handle,
            &device_config,
            &s_i2c_handle
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

    uint8_t model_id = 0;

    esp_err_t err = read_model_id(&model_id);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to read model ID: %s",
            esp_err_to_name(err)
        );

        return err;
    }

    ESP_LOGI(
        TAG,
        "Model ID register 0xC0 = 0x%02X",
        model_id
    );

    if (model_id != VL53L0X_EXPECTED_MODEL_ID) {
        ESP_LOGE(
            TAG,
            "Unexpected model ID, expected 0x%02X",
            VL53L0X_EXPECTED_MODEL_ID
        );

        return ESP_ERR_INVALID_RESPONSE;
    }

    memset(&s_device, 0, sizeof(s_device));

    s_device.I2cDevAddr = VL53L0X_I2C_ADDRESS;
    s_device.comms_speed_khz = 400;
    s_device.i2c_handle = s_i2c_handle;

    VL53L0X_Error status;

    status = VL53L0X_DataInit(&s_device);
    err = check_api_status("DataInit", status);

    if (err != ESP_OK) {
        return err;
    }

    status = VL53L0X_StaticInit(&s_device);
    err = check_api_status("StaticInit", status);

    if (err != ESP_OK) {
        return err;
    }

    uint8_t vhv_settings = 0;
    uint8_t phase_cal = 0;

    status = VL53L0X_PerformRefCalibration(
        &s_device,
        &vhv_settings,
        &phase_cal
    );

    err = check_api_status(
        "PerformRefCalibration",
        status
    );

    if (err != ESP_OK) {
        return err;
    }

    uint32_t ref_spad_count = 0;
    uint8_t is_aperture_spads = 0;

    status = VL53L0X_PerformRefSpadManagement(
        &s_device,
        &ref_spad_count,
        &is_aperture_spads
    );

    err = check_api_status(
        "PerformRefSpadManagement",
        status
    );

    if (err != ESP_OK) {
        return err;
    }

    status = VL53L0X_SetDeviceMode(
        &s_device,
        VL53L0X_DEVICEMODE_SINGLE_RANGING
    );

    err = check_api_status("SetDeviceMode", status);

    if (err != ESP_OK) {
        return err;
    }
status =
    VL53L0X_SetMeasurementTimingBudgetMicroSeconds(
        &s_device,
        20000
    );

err = check_api_status(
    "SetMeasurementTimingBudget",
    status
);

if (err != ESP_OK) {
    return err;
}
    s_initialized = true;

    ESP_LOGI(
        TAG,
        "Initialization completed, reference SPAD count = %lu",
        (unsigned long)ref_spad_count
    );

    return ESP_OK;
}

esp_err_t vl53l0x_calibrate_offset(
    uint16_t known_distance_mm,
    int32_t *offset_um)
{
    if (known_distance_mm == 0 || offset_um == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * ST API要求校准距离采用16.16定点格式。
     * 100 mm需要转换为100 << 16。
     */
    FixPoint1616_t calibration_distance =
        (FixPoint1616_t)(
            (uint32_t)known_distance_mm << 16
        );

    VL53L0X_Error status =
        VL53L0X_PerformOffsetCalibration(
            &s_device,
            calibration_distance,
            offset_um
        );

    esp_err_t err = check_api_status(
        "PerformOffsetCalibration",
        status
    );

    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGW(
        TAG,
        "Offset calibration result = %ld um",
        (long)*offset_um
    );

    return ESP_OK;
}
esp_err_t vl53l0x_read_distance(uint16_t *distance_mm)
{
    if (distance_mm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    VL53L0X_RangingMeasurementData_t measurement;
    memset(&measurement, 0, sizeof(measurement));

    VL53L0X_Error status =
        VL53L0X_PerformSingleRangingMeasurement(
            &s_device,
            &measurement
        );

    esp_err_t err = check_api_status(
        "Single ranging measurement",
        status
    );

    if (err != ESP_OK) {
        return err;
    }

    if (measurement.RangeStatus != 0) {
        ESP_LOGW(
            TAG,
            "Invalid measurement: range status = %u",
            (unsigned int)measurement.RangeStatus
        );

        return ESP_ERR_INVALID_RESPONSE;
    }

   /*
 * 应用固定软件修正。
 * 先判断大小，避免无符号整数相减产生下溢。
 */
if (measurement.RangeMilliMeter >=
    VL53L0X_DISTANCE_CORRECTION_MM) {

    *distance_mm =
        measurement.RangeMilliMeter -
        VL53L0X_DISTANCE_CORRECTION_MM;
} else {
    *distance_mm = 0;
}

return ESP_OK;
}