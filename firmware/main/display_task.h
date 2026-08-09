#ifndef D1F3E86D_4D75_4F4E_B3F2_E14F1B5830B8
#define D1F3E86D_4D75_4F4E_B3F2_E14F1B5830B8
#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include "esp_err.h"


/**
 * @brief 启动LCD显示任务
 *
 * 显示任务每50ms读取一次最新传感器数据。
 */
esp_err_t display_task_start(void);


#endif

#endif /* D1F3E86D_4D75_4F4E_B3F2_E14F1B5830B8 */
