#include "iic.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"


#define IIC_TIMEOUT_MS 1000

static const char *TAG = "BSP_IIC";

i2c_obj_t iic_master[2] = {0};


static esp_err_t iic_prepare_device(
    i2c_obj_t *self,
    uint16_t address
)
{
    if (self == NULL ||
        self->bus_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (self->device_handle != NULL &&
        self->device_address == address) {
        return ESP_OK;
    }

    if (self->device_handle != NULL) {
        esp_err_t err =
            i2c_master_bus_rm_device(
                self->device_handle
            );

        if (err != ESP_OK) {
            return err;
        }

        self->device_handle = NULL;
    }

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = IIC_FREQ,
    };

    esp_err_t err =
        i2c_master_bus_add_device(
            self->bus_handle,
            &device_config,
            &self->device_handle
        );

    if (err == ESP_OK) {
        self->device_address = address;
    }

    return err;
}


i2c_obj_t iic_init(uint8_t iic_port)
{
    uint8_t index =
        (iic_port == I2C_NUM_0) ? 0 : 1;

    if (iic_master[index].bus_handle != NULL &&
        iic_master[index].init_flag == ESP_OK) {
        return iic_master[index];
    }

    memset(
        &iic_master[index],
        0,
        sizeof(iic_master[index])
    );

    iic_master[index].port =
        (iic_port == I2C_NUM_0) ?
        I2C_NUM_0 : I2C_NUM_1;

    if (iic_master[index].port == I2C_NUM_0) {
        iic_master[index].sda =
            IIC0_SDA_GPIO_PIN;
        iic_master[index].scl =
            IIC0_SCL_GPIO_PIN;
    } else {
        iic_master[index].sda =
            IIC1_SDA_GPIO_PIN;
        iic_master[index].scl =
            IIC1_SCL_GPIO_PIN;
    }

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = iic_master[index].port,
        .scl_io_num = iic_master[index].scl,
        .sda_io_num = iic_master[index].sda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err =
        i2c_new_master_bus(
            &bus_config,
            &iic_master[index].bus_handle
        );

    iic_master[index].init_flag = err;

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "I2C port %d initialization failed: %s",
            (int)iic_master[index].port,
            esp_err_to_name(err)
        );
    }

    return iic_master[index];
}


esp_err_t i2c_transfer(
    i2c_obj_t *self,
    uint16_t addr,
    size_t n,
    i2c_buf_t *bufs,
    unsigned int flags
)
{
    if (self == NULL ||
        bufs == NULL ||
        n == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err =
        iic_prepare_device(self, addr);

    if (err != ESP_OK) {
        return err;
    }

    if (flags & I2C_FLAG_READ) {
        if (flags & I2C_FLAG_WRITE) {
            if (n != 2) {
                return ESP_ERR_INVALID_ARG;
            }

            return i2c_master_transmit_receive(
                self->device_handle,
                bufs[0].buf,
                bufs[0].len,
                bufs[1].buf,
                bufs[1].len,
                IIC_TIMEOUT_MS
            );
        }

        if (n != 1) {
            return ESP_ERR_INVALID_ARG;
        }

        return i2c_master_receive(
            self->device_handle,
            bufs[0].buf,
            bufs[0].len,
            IIC_TIMEOUT_MS
        );
    }

    size_t total_length = 0;

    for (size_t i = 0; i < n; i++) {
        total_length += bufs[i].len;
    }

    if (total_length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *tx_buffer =
        malloc(total_length);

    if (tx_buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    size_t offset = 0;

    for (size_t i = 0; i < n; i++) {
        memcpy(
            tx_buffer + offset,
            bufs[i].buf,
            bufs[i].len
        );

        offset += bufs[i].len;
    }

    err = i2c_master_transmit(
        self->device_handle,
        tx_buffer,
        total_length,
        IIC_TIMEOUT_MS
    );

    free(tx_buffer);

    return err;
}