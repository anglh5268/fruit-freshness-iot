#ifndef AS7341_H
#define AS7341_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"


typedef struct {
    uint16_t f1_415nm;
    uint16_t f2_445nm;
    uint16_t f3_480nm;
    uint16_t f4_515nm;
    uint16_t f5_555nm;
    uint16_t f6_590nm;
    uint16_t f7_630nm;
    uint16_t f8_680nm;
    uint16_t clear;
    uint16_t nir;
} as7341_spectral_data_t;


/* 初始化AS7341 */
esp_err_t as7341_init(void);

/* 读取全部光谱通道 */
esp_err_t as7341_read_spectrum(
    as7341_spectral_data_t *data
);

/*
 * 控制模块板载白色补光灯。
 * drive范围为0~127，电流约为4mA + 2mA * drive。
 * 首次测试建议drive=4（约12mA）。
 */
esp_err_t as7341_set_led(
    bool on,
    uint8_t drive
);

/*
 * 完成一次补光反射采集：
 * ambient为关灯数据，illuminated为开灯数据，
 * net为max(illuminated - ambient, 0)。
 */
esp_err_t as7341_read_reflectance(
    as7341_spectral_data_t *ambient,
    as7341_spectral_data_t *illuminated,
    as7341_spectral_data_t *net,
    uint8_t led_drive
);

#endif
