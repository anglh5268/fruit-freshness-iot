#include "vl53l0x_platform.h"

#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static VL53L0X_Error convert_i2c_error(esp_err_t err)
{
    if (err == ESP_OK) {
        return VL53L0X_ERROR_NONE;
    }

    return VL53L0X_ERROR_CONTROL_INTERFACE;
}


VL53L0X_Error VL53L0X_LockSequenceAccess(VL53L0X_DEV Dev)
{
    (void)Dev;
    return VL53L0X_ERROR_NONE;
}


VL53L0X_Error VL53L0X_UnlockSequenceAccess(VL53L0X_DEV Dev)
{
    (void)Dev;
    return VL53L0X_ERROR_NONE;
}


VL53L0X_Error VL53L0X_WriteMulti(
    VL53L0X_DEV Dev,
    uint8_t index,
    uint8_t *pdata,
    uint32_t count)
{
    if (Dev == NULL || Dev->i2c_handle == NULL) {
        return VL53L0X_ERROR_CONTROL_INTERFACE;
    }

    if (count == 0) {
        return VL53L0X_ERROR_NONE;
    }

    if (pdata == NULL) {
        return VL53L0X_ERROR_CONTROL_INTERFACE;
    }

    uint8_t *buffer = malloc((size_t)count + 1U);

    if (buffer == NULL) {
        return VL53L0X_ERROR_CONTROL_INTERFACE;
    }

    buffer[0] = index;
    memcpy(&buffer[1], pdata, count);

    esp_err_t err = i2c_master_transmit(
        Dev->i2c_handle,
        buffer,
        (size_t)count + 1U,
        1000
    );

    free(buffer);

    return convert_i2c_error(err);
}


VL53L0X_Error VL53L0X_ReadMulti(
    VL53L0X_DEV Dev,
    uint8_t index,
    uint8_t *pdata,
    uint32_t count)
{
    if (Dev == NULL ||
        Dev->i2c_handle == NULL ||
        pdata == NULL) {
        return VL53L0X_ERROR_CONTROL_INTERFACE;
    }

    if (count == 0) {
        return VL53L0X_ERROR_NONE;
    }

    esp_err_t err = i2c_master_transmit_receive(
        Dev->i2c_handle,
        &index,
        1,
        pdata,
        count,
        1000
    );

    return convert_i2c_error(err);
}


VL53L0X_Error VL53L0X_WrByte(
    VL53L0X_DEV Dev,
    uint8_t index,
    uint8_t data)
{
    return VL53L0X_WriteMulti(Dev, index, &data, 1);
}


VL53L0X_Error VL53L0X_WrWord(
    VL53L0X_DEV Dev,
    uint8_t index,
    uint16_t data)
{
    uint8_t buffer[2] = {
        (uint8_t)(data >> 8),
        (uint8_t)(data & 0xFF)
    };

    return VL53L0X_WriteMulti(Dev, index, buffer, 2);
}


VL53L0X_Error VL53L0X_WrDWord(
    VL53L0X_DEV Dev,
    uint8_t index,
    uint32_t data)
{
    uint8_t buffer[4] = {
        (uint8_t)(data >> 24),
        (uint8_t)(data >> 16),
        (uint8_t)(data >> 8),
        (uint8_t)(data & 0xFF)
    };

    return VL53L0X_WriteMulti(Dev, index, buffer, 4);
}


VL53L0X_Error VL53L0X_RdByte(
    VL53L0X_DEV Dev,
    uint8_t index,
    uint8_t *data)
{
    return VL53L0X_ReadMulti(Dev, index, data, 1);
}


VL53L0X_Error VL53L0X_RdWord(
    VL53L0X_DEV Dev,
    uint8_t index,
    uint16_t *data)
{
    if (data == NULL) {
        return VL53L0X_ERROR_CONTROL_INTERFACE;
    }

    uint8_t buffer[2];

    VL53L0X_Error status =
        VL53L0X_ReadMulti(Dev, index, buffer, 2);

    if (status == VL53L0X_ERROR_NONE) {
        *data = ((uint16_t)buffer[0] << 8) |
                (uint16_t)buffer[1];
    }

    return status;
}


VL53L0X_Error VL53L0X_RdDWord(
    VL53L0X_DEV Dev,
    uint8_t index,
    uint32_t *data)
{
    if (data == NULL) {
        return VL53L0X_ERROR_CONTROL_INTERFACE;
    }

    uint8_t buffer[4];

    VL53L0X_Error status =
        VL53L0X_ReadMulti(Dev, index, buffer, 4);

    if (status == VL53L0X_ERROR_NONE) {
        *data = ((uint32_t)buffer[0] << 24) |
                ((uint32_t)buffer[1] << 16) |
                ((uint32_t)buffer[2] << 8) |
                (uint32_t)buffer[3];
    }

    return status;
}


VL53L0X_Error VL53L0X_UpdateByte(
    VL53L0X_DEV Dev,
    uint8_t index,
    uint8_t AndData,
    uint8_t OrData)
{
    uint8_t old_data;

    VL53L0X_Error status =
        VL53L0X_RdByte(Dev, index, &old_data);

    if (status != VL53L0X_ERROR_NONE) {
        return status;
    }

    uint8_t new_data = (old_data & AndData) | OrData;

    return VL53L0X_WrByte(Dev, index, new_data);
}


VL53L0X_Error VL53L0X_PollingDelay(VL53L0X_DEV Dev)
{
    (void)Dev;

    vTaskDelay(pdMS_TO_TICKS(10));

    return VL53L0X_ERROR_NONE;
}