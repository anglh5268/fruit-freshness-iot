#include "display_task.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lcd.h"
#include "cloud_client.h"
#include "cloud_task.h"
#include "sensor_task.h"
#include "utf8_lcd.h"


#define DISPLAY_PERIOD_MS 50

static TaskHandle_t s_display_task_handle = NULL;


static const char *cloud_state_text(cloud_task_state_t state)
{
    switch (state) {
        case CLOUD_TASK_STATE_DISABLED:
            return "DISABLED";
        case CLOUD_TASK_STATE_WAITING_WIFI:
            return "WAIT WIFI";
        case CLOUD_TASK_STATE_READY:
            return "READY";
        case CLOUD_TASK_STATE_ANALYZING:
            return "ANALYZING";
        case CLOUD_TASK_STATE_SUCCESS:
            return "SUCCESS";
        case CLOUD_TASK_STATE_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}


static uint16_t cloud_state_color(cloud_task_state_t state)
{
    switch (state) {
        case CLOUD_TASK_STATE_READY:
        case CLOUD_TASK_STATE_SUCCESS:
            return GREEN;
        case CLOUD_TASK_STATE_ANALYZING:
            return BLUE;
        case CLOUD_TASK_STATE_ERROR:
            return RED;
        default:
            return GRAY;
    }
}


static uint16_t cloud_risk_color(const char *risk)
{
    if (risk == NULL) {
        return GRAY;
    }
    if (strcmp(risk, "high") == 0) {
        return RED;
    }
    if (strcmp(risk, "medium") == 0) {
        return YELLOW;
    }
    if (strcmp(risk, "low") == 0) {
        return GREEN;
    }
    return GRAY;
}


static const char *freshness_risk_text_chinese(
    freshness_risk_level_t risk_level
)
{
    switch (risk_level) {
        case FRESHNESS_RISK_LOW:
            return "低风险";
        case FRESHNESS_RISK_MEDIUM:
            return "中风险";
        case FRESHNESS_RISK_HIGH:
            return "高风险";
        default:
            return "等待结果";
    }
}


static const char *freshness_conclusion_text_chinese(
    freshness_risk_level_t risk_level
)
{
    switch (risk_level) {
        case FRESHNESS_RISK_LOW:
            return "状态正常";
        case FRESHNESS_RISK_MEDIUM:
            return "建议关注";
        case FRESHNESS_RISK_HIGH:
            return "腐坏风险高";
        default:
            return "等待结果";
    }
}


static const char *cloud_conclusion_text_chinese(const char *risk)
{
    if (risk != NULL && strcmp(risk, "high") == 0) {
        return "结论：腐坏风险高";
    }
    if (risk != NULL && strcmp(risk, "medium") == 0) {
        return "结论：建议关注";
    }
    return "结论：状态正常";
}


/* 显示5位数值，数据无效时显示----- */
static void display_draw_workflow_layout(void)
{
    lcd_clear(WHITE);
    lcd_show_utf8_wrapped(
        104, 6, 112, 16,
        "油桃新鲜度检测",
        DARKBLUE,
        WHITE
    );
    lcd_fill(0, 29, 319, 30, BLUE);

    lcd_show_utf8_wrapped(12, 42, 80, 16, "当前状态", GRAY, WHITE);
    lcd_show_utf8_wrapped(12, 70, 80, 16, "检测位置", GRAY, WHITE);
    lcd_show_utf8_wrapped(12, 98, 80, 16, "检测距离", GRAY, WHITE);
    lcd_show_string(184, 98, 24, 16, 16, "mm", GRAY);

    lcd_draw_rectangle(8, 126, 311, 143, GRAY);
    lcd_fill(0, 220, 319, 221, LGRAY);
}


static void display_draw_workflow_snapshot(
    const sensor_snapshot_t *snapshot,
    const cloud_task_status_t *cloud_status
)
{
    char status[64] = "准备就绪";
    char position[96];
    char line1[128] = "";
    char line2[128] = "";
    char footer[128] = "KEY0 开始   KEY2 数据   KEY3 重置";
    uint16_t status_color = BLUE;
    uint16_t result_color = BLUE;
    unsigned int progress_percent = 0;

    snprintf(
        position,
        sizeof(position),
        "P%u / %u",
        (unsigned int)snapshot->freshness.current_position,
        (unsigned int)FRESHNESS_POSITION_COUNT
    );

    switch (snapshot->freshness.stage) {
        case FRESHNESS_STAGE_IDLE:
            strlcpy(status, "准备就绪", sizeof(status));
            strlcpy(position, "等待开始", sizeof(position));
            strlcpy(line1, "请将油桃放在15-25mm处", sizeof(line1));
            strlcpy(line2, "按KEY0开始检测", sizeof(line2));
            break;

        case FRESHNESS_STAGE_WAITING_POSITION:
            strlcpy(status, "等待采集", sizeof(status));
            snprintf(
                line1,
                sizeof(line1),
                "请将第%u个检测面对准传感器",
                (unsigned int)snapshot->freshness.current_position
            );
            strlcpy(line2, "距离合适后按KEY0采集", sizeof(line2));
            strlcpy(
                footer,
                "KEY0 采集   KEY1 取消   KEY2 数据",
                sizeof(footer)
            );
            break;

        case FRESHNESS_STAGE_COLLECTING:
            strlcpy(status, "正在采集", sizeof(status));
            status_color = MAGENTA;
            progress_percent =
                (unsigned int)snapshot->freshness.frames_collected * 100U /
                FRESHNESS_FRAMES_PER_POSITION;
            if (snapshot->distance_valid &&
                snapshot->distance_mm >= MATERIAL_CLASSIFIER_MIN_DISTANCE_MM &&
                snapshot->distance_mm <= MATERIAL_CLASSIFIER_MAX_DISTANCE_MM) {
                strlcpy(line1, "请保持油桃静止", sizeof(line1));
                snprintf(
                    line2,
                    sizeof(line2),
                    "采集进度 %u / %u",
                    (unsigned int)snapshot->freshness.frames_collected,
                    (unsigned int)FRESHNESS_FRAMES_PER_POSITION
                );
            } else {
                strlcpy(line1, "距离不合适，请调整", sizeof(line1));
                strlcpy(line2, "目标距离：15-25mm", sizeof(line2));
                status_color = RED;
            }
            strlcpy(
                footer,
                "KEY1 取消或重测   KEY2 数据",
                sizeof(footer)
            );
            break;

        case FRESHNESS_STAGE_POSITION_DONE: {
            const uint8_t index =
                (uint8_t)(snapshot->freshness.current_position - 1);
            const unsigned int risk_percent = (unsigned int)(
                snapshot->freshness.position_probabilities[index] *
                100.0f + 0.5f
            );
            snprintf(
                status,
                sizeof(status),
                "第%u面完成",
                (unsigned int)snapshot->freshness.current_position
            );
            status_color = GREEN;
            progress_percent = risk_percent;
            snprintf(
                position,
                sizeof(position),
                "本面异常概率 %u%%",
                risk_percent
            );
            if (snapshot->freshness.removal_confirmed) {
                snprintf(
                    line1,
                    sizeof(line1),
                    "请旋转到第%u检测面",
                    (unsigned int)(snapshot->freshness.current_position + 1)
                );
                strlcpy(
                    line2,
                    "放好后按KEY0继续",
                    sizeof(line2)
                );
            } else {
                strlcpy(line1, "请先移开油桃", sizeof(line1));
                strlcpy(
                    line2,
                    "移出检测范围后再换面",
                    sizeof(line2)
                );
            }
            strlcpy(
                footer,
                "KEY0 下一面   KEY1 重测   KEY2 数据",
                sizeof(footer)
            );
            break;
        }

        case FRESHNESS_STAGE_COMPLETE: {
            const unsigned int confidence = (unsigned int)(
                snapshot->freshness.confidence * 100.0f + 0.5f
            );
            const unsigned int mean_risk = (unsigned int)(
                snapshot->freshness.mean_risk_probability * 100.0f + 0.5f
            );
            const unsigned int max_risk = (unsigned int)(
                snapshot->freshness.max_position_risk_probability *
                100.0f + 0.5f
            );
            strlcpy(
                status,
                freshness_conclusion_text_chinese(
                    snapshot->freshness.risk_level
                ),
                sizeof(status)
            );
            status_color = cloud_risk_color(
                freshness_classifier_risk_level_text(
                    snapshot->freshness.risk_level
                )
            );
            result_color = status_color;
            snprintf(
                position,
                sizeof(position),
                "%s  置信度%u%%",
                freshness_risk_text_chinese(snapshot->freshness.risk_level),
                confidence
            );
            progress_percent = mean_risk;
            snprintf(
                line1,
                sizeof(line1),
                "整体风险%u%%  P%u风险%u%%",
                mean_risk,
                (unsigned int)snapshot->freshness.max_risk_position,
                max_risk
            );
            if (snapshot->freshness.local_anomaly_detected) {
                snprintf(
                    line2,
                    sizeof(line2),
                    "局部异常：请重点检查P%u",
                    (unsigned int)snapshot->freshness.max_risk_position
                );
            } else {
                strlcpy(
                    line2,
                    "四个检测面结果一致",
                    sizeof(line2)
                );
            }
            strlcpy(
                footer,
                snapshot->freshness.cloud_requested
                    ? "AI请求已提交   KEY3 新检测"
                    : "KEY0 AI报告   KEY3 新检测",
                sizeof(footer)
            );

            if (snapshot->freshness.cloud_requested &&
                cloud_status != NULL) {
                switch (cloud_status->state) {
                    case CLOUD_TASK_STATE_DISABLED:
                        strlcpy(
                            line2,
                            "AI服务未配置",
                            sizeof(line2)
                        );
                        break;
                    case CLOUD_TASK_STATE_WAITING_WIFI:
                        strlcpy(
                            line2,
                            "正在等待网络",
                            sizeof(line2)
                        );
                        break;
                    case CLOUD_TASK_STATE_READY:
                        strlcpy(
                            line2,
                            "AI请求已提交",
                            sizeof(line2)
                        );
                        break;
                    default:
                        break;
                }
            }
            break;
        }

        default:
            strlcpy(status, "状态异常", sizeof(status));
            status_color = RED;
            break;
    }

    if (progress_percent > 100U) {
        progress_percent = 100U;
    }

    lcd_fill(104, 40, 311, 61, WHITE);
    lcd_show_utf8_wrapped(
        104, 42, 208, 16,
        status,
        status_color,
        WHITE
    );

    lcd_fill(104, 70, 311, 91, WHITE);
    lcd_show_utf8_wrapped(
        104, 72, 208, 16,
        position,
        result_color,
        WHITE
    );

    lcd_fill(104, 96, 183, 115, WHITE);
    if (snapshot->distance_valid) {
        lcd_show_num(104, 98, snapshot->distance_mm, 3, 16, BLUE);
    } else {
        lcd_show_string(104, 98, 72, 16, 16, "---", RED);
    }

    lcd_fill(9, 127, 310, 142, WHITE);
    if (progress_percent > 0U) {
        const uint16_t bar_end =
            (uint16_t)(9U + (300U * progress_percent / 100U));
        lcd_fill(
            9,
            127,
            bar_end,
            142,
            snapshot->freshness.stage == FRESHNESS_STAGE_COMPLETE
                ? result_color
                : BLUE
        );
    }

    lcd_fill(8, 152, 311, 171, WHITE);
    lcd_show_utf8_wrapped(8, 154, 304, 16, line1, BLACK, WHITE);
    lcd_fill(8, 178, 311, 197, WHITE);
    lcd_show_utf8_wrapped(
        8, 180, 304, 16,
        line2,
        snapshot->freshness.local_anomaly_detected ? RED : DARKBLUE,
        WHITE
    );
    lcd_fill(8, 200, 311, 217, WHITE);
    if (snapshot->freshness.stage == FRESHNESS_STAGE_COMPLETE) {
        lcd_show_utf8_wrapped(
            8, 202, 304, 16,
            "边缘AI本地判定  断网可用",
            GRAY,
            WHITE
        );
    }
    lcd_fill(0, 222, 319, 239, WHITE);
    lcd_show_utf8_wrapped(8, 223, 304, 16, footer, DARKBLUE, WHITE);
}


static void display_u16(
    uint16_t x,
    uint16_t y,
    uint16_t value,
    bool valid
)
{
    lcd_fill(
        x,
        y,
        x + 47,
        y + 15,
        WHITE
    );

    if (valid) {
        lcd_show_num(
            x,
            y,
            value,
            5,
            16,
            BLUE
        );
    } else {
        lcd_show_string(
            x,
            y,
            48,
            16,
            16,
            "-----",
            RED
        );
    }
}


/* 绘制不会变化的标题和通道名称 */
static void display_draw_layout(void)
{
    lcd_clear(WHITE);

    lcd_show_string(
        8, 4,
        304, 24,
        24,
        "AS7341 + VL53L0X",
        DARKBLUE
    );

    lcd_fill(
        0, 32,
        319, 33,
        BLUE
    );

    lcd_show_string(
        8, 42,
        80, 16,
        16,
        "Distance:",
        BLACK
    );

    lcd_show_string(
        144, 42,
        32, 16,
        16,
        "mm",
        BLACK
    );

    lcd_show_string(
        8, 68,
        64, 16,
        16,
        "F1 415:",
        BLACK
    );

    lcd_show_string(
        160, 68,
        64, 16,
        16,
        "F2 445:",
        BLACK
    );

    lcd_show_string(
        8, 90,
        64, 16,
        16,
        "F3 480:",
        BLACK
    );

    lcd_show_string(
        160, 90,
        64, 16,
        16,
        "F4 515:",
        BLACK
    );

    lcd_show_string(
        8, 112,
        64, 16,
        16,
        "F5 555:",
        BLACK
    );

    lcd_show_string(
        160, 112,
        64, 16,
        16,
        "F6 590:",
        BLACK
    );

    lcd_show_string(
        8, 134,
        64, 16,
        16,
        "F7 630:",
        BLACK
    );

    lcd_show_string(
        160, 134,
        64, 16,
        16,
        "F8 680:",
        BLACK
    );

    lcd_show_string(
        8, 156,
        64, 16,
        16,
        "Clear:",
        BLACK
    );

    lcd_show_string(
        160, 156,
        56, 16,
        16,
        "NIR:",
        BLACK
    );

    lcd_show_string(
        8, 182,
        64, 16,
        16,
        "Sample:",
        BLACK
    );

    lcd_show_string(
        176, 182,
        120, 16,
        16,
        "Period:50ms",
        GRAY
    );

    lcd_fill(0, 226, 319, 227, LGRAY);
}


static void display_draw_cloud_footer(const cloud_task_status_t *status)
{
    lcd_fill(0, 228, 319, 239, WHITE);
    lcd_show_string(8, 228, 48, 12, 12, "Cloud:", BLACK);
    lcd_show_string(
        56,
        228,
        120,
        12,
        12,
        (char *)cloud_state_text(status->state),
        cloud_state_color(status->state)
    );
}


static void display_draw_cloud_page(const cloud_task_status_t *status)
{
    lcd_clear(WHITE);

    if (status->state == CLOUD_TASK_STATE_SUCCESS &&
        status->report_valid) {
        lcd_show_utf8_wrapped(
            92, 6, 144, 16,
            "AI辅助分析报告",
            DARKBLUE,
            WHITE
        );
        lcd_fill(0, 28, 319, 29, BLUE);

        const char *risk = status->report.risk_level;
        lcd_show_utf8_wrapped(
            16, 38, 192, 16,
            cloud_conclusion_text_chinese(risk),
            cloud_risk_color(risk),
            WHITE
        );

        lcd_show_utf8_wrapped(
            208, 38, 64, 16,
            "置信度",
            GRAY,
            WHITE
        );
        lcd_show_num(
            272,
            38,
            status->freshness_confidence_percent,
            3,
            16,
            MAGENTA
        );
        lcd_show_string(304, 38, 16, 16, 16, "%", MAGENTA);

        lcd_show_utf8_wrapped(
            16, 68, 64, 16,
            "分析：",
            DARKBLUE,
            WHITE
        );
        lcd_show_utf8_wrapped(
            16, 88, 288, 48,
            status->report.summary,
            BLACK,
            WHITE
        );

        lcd_show_utf8_wrapped(
            16, 142, 64, 16,
            "建议：",
            DARKBLUE,
            WHITE
        );
        lcd_show_utf8_wrapped(
            16, 162, 288, 48,
            status->report.advice,
            BLACK,
            WHITE
        );

        lcd_fill(0, 216, 319, 217, LGRAY);
        lcd_show_utf8_wrapped(
            8, 222, 144, 16,
            "DeepSeek辅助",
            BLUE,
            WHITE
        );
        lcd_show_utf8_wrapped(
            192, 222, 120, 16,
            "KEY3 新检测",
            GRAY,
            WHITE
        );
        return;
    }

    lcd_show_utf8_wrapped(
        112, 8, 96, 16,
        "AI辅助分析",
        DARKBLUE,
        WHITE
    );
    lcd_fill(0, 31, 319, 32, BLUE);

    const char *risk = status->freshness_risk_level;
    char local_result[96];
    snprintf(
        local_result,
        sizeof(local_result),
        "本地结论：%s  %u%%",
        strcmp(risk, "high") == 0
            ? "高风险"
            : strcmp(risk, "medium") == 0
                ? "中风险"
                : "低风险",
        status->freshness_confidence_percent
    );
    lcd_show_utf8_wrapped(
        56, 52, 224, 16,
        local_result,
        cloud_risk_color(risk),
        WHITE
    );

    if (status->state == CLOUD_TASK_STATE_ERROR) {
        lcd_show_utf8_wrapped(
            104, 92, 112, 16,
            "AI分析失败",
            RED,
            WHITE
        );
        lcd_show_utf8_wrapped(
            40, 126, 240, 32,
            "请检查网络，系统将自动重试",
            BLACK,
            WHITE
        );
        lcd_show_string(
            64, 174, 192, 16, 16,
            (char *)status->error_text,
            RED
        );
    } else {
        lcd_show_utf8_wrapped(
            88, 92, 144, 16,
            "正在生成中文报告",
            BLUE,
            WHITE
        );
        lcd_show_utf8_wrapped(
            80, 126, 160, 16,
            "正在连接 DeepSeek",
            BLACK,
            WHITE
        );
        lcd_draw_rectangle(40, 164, 279, 181, GRAY);
        lcd_fill(41, 165, 192, 180, LIGHTBLUE);
        lcd_show_utf8_wrapped(
            104, 194, 112, 16,
            "请稍候",
            GRAY,
            WHITE
        );
    }

    lcd_fill(0, 218, 319, 219, LGRAY);
    lcd_show_utf8_wrapped(
        192, 222, 120, 16,
        "KEY3 新检测",
        GRAY,
        WHITE
    );
}


/* 把一个传感器快照显示到屏幕 */
static void display_draw_snapshot(
    const sensor_snapshot_t *snapshot
)
{
    display_u16(
        88,
        42,
        snapshot->distance_mm,
        snapshot->distance_valid
    );

    display_u16(
        72,
        68,
        snapshot->spectrum.f1_415nm,
        snapshot->spectrum_valid
    );

    display_u16(
        224,
        68,
        snapshot->spectrum.f2_445nm,
        snapshot->spectrum_valid
    );

    display_u16(
        72,
        90,
        snapshot->spectrum.f3_480nm,
        snapshot->spectrum_valid
    );

    display_u16(
        224,
        90,
        snapshot->spectrum.f4_515nm,
        snapshot->spectrum_valid
    );

    display_u16(
        72,
        112,
        snapshot->spectrum.f5_555nm,
        snapshot->spectrum_valid
    );

    display_u16(
        224,
        112,
        snapshot->spectrum.f6_590nm,
        snapshot->spectrum_valid
    );

    display_u16(
        72,
        134,
        snapshot->spectrum.f7_630nm,
        snapshot->spectrum_valid
    );

    display_u16(
        224,
        134,
        snapshot->spectrum.f8_680nm,
        snapshot->spectrum_valid
    );

    display_u16(
        72,
        156,
        snapshot->spectrum.clear,
        snapshot->spectrum_valid
    );

    display_u16(
        224,
        156,
        snapshot->spectrum.nir,
        snapshot->spectrum_valid
    );

    /* 清除旧的样本编号 */
    lcd_fill(
        72, 182,
        159, 197,
        WHITE
    );

    lcd_show_num(
        72,
        182,
        snapshot->sample_number,
        10,
        16,
        MAGENTA
    );

    /* 清除旧状态 */
    lcd_fill(
        8, 208,
        311, 225,
        WHITE
    );

    if (!snapshot->spectrum_valid) {
        lcd_show_string(
            8, 208,
            240, 16,
            16,
            "SPECTRUM INVALID",
            RED
        );
    } else if (!snapshot->distance_valid) {
        lcd_show_string(
            8, 208,
            240, 16,
            16,
            "DISTANCE INVALID",
            RED
        );
    } else if (!snapshot->classification.valid) {
        lcd_show_string(
            8, 208,
            304, 16,
            16,
            "MOVE TO 15-25mm",
            RED
        );
    } else {
        char freshness_text[40];
        uint16_t freshness_color = BLUE;

        if (snapshot->freshness.result_valid) {
            snprintf(
                freshness_text,
                sizeof(freshness_text),
                "Fresh:%s %u%%",
                freshness_classifier_label(&snapshot->freshness),
                (unsigned int)(
                    snapshot->freshness.confidence * 100.0f + 0.5f
                )
            );
            freshness_color =
                snapshot->freshness.is_risk ? RED : GREEN;
        } else {
            switch (snapshot->freshness.stage) {
                case FRESHNESS_STAGE_IDLE:
                    snprintf(
                        freshness_text,
                        sizeof(freshness_text),
                        "KEY0 START TEST"
                    );
                    break;
                case FRESHNESS_STAGE_WAITING_POSITION:
                    snprintf(
                        freshness_text,
                        sizeof(freshness_text),
                        "P%u READY - KEY0",
                        (unsigned int)snapshot->freshness.current_position
                    );
                    break;
                case FRESHNESS_STAGE_COLLECTING:
                    snprintf(
                        freshness_text,
                        sizeof(freshness_text),
                        "P%u SCAN %u/%u",
                        (unsigned int)snapshot->freshness.current_position,
                        (unsigned int)snapshot->freshness.frames_collected,
                        (unsigned int)FRESHNESS_FRAMES_PER_POSITION
                    );
                    break;
                case FRESHNESS_STAGE_POSITION_DONE:
                    snprintf(
                        freshness_text,
                        sizeof(freshness_text),
                        snapshot->freshness.removal_confirmed
                            ? "P%u DONE - ROTATE"
                            : "P%u DONE - REMOVE",
                        (unsigned int)snapshot->freshness.current_position
                    );
                    freshness_color = GREEN;
                    break;
                default:
                    strlcpy(
                        freshness_text,
                        "FRESHNESS WAIT",
                        sizeof(freshness_text)
                    );
                    break;
            }
        }

        lcd_show_string(
            8,
            208,
            304,
            16,
            16,
            freshness_text,
            freshness_color
        );
    }
}


static void display_task(void *argument)
{
    (void)argument;

    uint32_t last_sample_number = 0;
    uint32_t last_cloud_revision = 0;
    bool showing_cloud_page = false;
    bool showing_detail_page = false;

    display_draw_workflow_layout();

    while (1) {
        sensor_snapshot_t snapshot;
        cloud_task_status_t cloud_status;

        bool received =
            sensor_task_get_latest(&snapshot);
        bool cloud_received =
            cloud_task_get_status(&cloud_status);

        bool request_cloud_page =
            cloud_received &&
            received &&
            snapshot.freshness.cloud_requested &&
            (cloud_status.state == CLOUD_TASK_STATE_ANALYZING ||
             cloud_status.state == CLOUD_TASK_STATE_SUCCESS ||
             cloud_status.state == CLOUD_TASK_STATE_ERROR);

        if (request_cloud_page) {
            if (!showing_cloud_page ||
                cloud_status.revision != last_cloud_revision) {
                display_draw_cloud_page(&cloud_status);
                showing_cloud_page = true;
                last_cloud_revision = cloud_status.revision;
            }
            vTaskDelay(pdMS_TO_TICKS(DISPLAY_PERIOD_MS));
            continue;
        }

        if (showing_cloud_page) {
            if (received && snapshot.ui_show_sensor_details) {
                display_draw_layout();
                showing_detail_page = true;
            } else {
                display_draw_workflow_layout();
                showing_detail_page = false;
            }
            last_sample_number = 0;
            showing_cloud_page = false;
        }

        if (received &&
            snapshot.ui_show_sensor_details != showing_detail_page) {
            if (snapshot.ui_show_sensor_details) {
                display_draw_layout();
            } else {
                display_draw_workflow_layout();
            }
            showing_detail_page = snapshot.ui_show_sensor_details;
            last_sample_number = 0;
        }

        if (showing_detail_page &&
            cloud_received &&
            cloud_status.revision != last_cloud_revision) {
            display_draw_cloud_footer(&cloud_status);
            last_cloud_revision = cloud_status.revision;
        }

        /*
         * 显示任务每50ms检查一次。
         * 只有出现新的100ms传感器样本时才重绘，
         * 避免屏幕闪烁。
         */
        if (received &&
            snapshot.sample_number != 0 &&
            snapshot.sample_number !=
                last_sample_number) {

            if (showing_detail_page) {
                display_draw_snapshot(&snapshot);
            } else {
                display_draw_workflow_snapshot(
                    &snapshot,
                    cloud_received ? &cloud_status : NULL
                );
            }

            last_sample_number =
                snapshot.sample_number;
        }

        /*
         * Always block after drawing.  A full LCD refresh can take longer
         * than DISPLAY_PERIOD_MS; vTaskDelayUntil() would then repeatedly
         * try to catch up and could starve the CPU idle task/watchdog.
         */
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_PERIOD_MS));
    }
}


esp_err_t display_task_start(void)
{
    if (s_display_task_handle != NULL) {
        return ESP_OK;
    }

    BaseType_t result =
        xTaskCreate(
            display_task,
            "display_task",
            4096,
            NULL,
            4,
            &s_display_task_handle
        );

    if (result != pdPASS) {
        s_display_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
