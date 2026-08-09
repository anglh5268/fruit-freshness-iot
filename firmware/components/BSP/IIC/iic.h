#ifndef __IIC_H
#define __IIC_H

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"


typedef struct _i2c_obj_t
{
    i2c_port_num_t port;
    gpio_num_t scl;
    gpio_num_t sda;

    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t device_handle;
    uint16_t device_address;

    esp_err_t init_flag;
} i2c_obj_t;


typedef struct _i2c_buf_t
{
    size_t len;
    uint8_t *buf;
} i2c_buf_t;


#define I2C_FLAG_READ       0x01
#define I2C_FLAG_STOP       0x02
#define I2C_FLAG_WRITE      0x04

/* 板载XL9555使用I2C0 */
#define IIC0_SDA_GPIO_PIN   GPIO_NUM_41
#define IIC0_SCL_GPIO_PIN   GPIO_NUM_42

/* 外接传感器使用I2C1，这里不会重复初始化 */
#define IIC1_SDA_GPIO_PIN   GPIO_NUM_4
#define IIC1_SCL_GPIO_PIN   GPIO_NUM_5

#define IIC_FREQ            400000

extern i2c_obj_t iic_master[2];

i2c_obj_t iic_init(uint8_t iic_port);

esp_err_t i2c_transfer(
    i2c_obj_t *self,
    uint16_t addr,
    size_t n,
    i2c_buf_t *bufs,
    unsigned int flags
);

#endif