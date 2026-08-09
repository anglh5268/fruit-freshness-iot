#ifndef FRESHNESS_CLASSIFIER_H
#define FRESHNESS_CLASSIFIER_H

#include <stdbool.h>
#include <stdint.h>

#include "as7341.h"
#include "esp_err.h"

#define FRESHNESS_POSITION_COUNT 4U
#define FRESHNESS_FRAMES_PER_POSITION 20U
#define FRESHNESS_REMOVAL_SAMPLE_COUNT 5U
#define FRESHNESS_MEAN_RISK_THRESHOLD 0.5f
#define FRESHNESS_LOGISTIC_LOCAL_RISK_THRESHOLD 0.8f
#define FRESHNESS_LOCAL_LOGISTIC_AGREEMENT_THRESHOLD 0.45f
#define FRESHNESS_LOCAL_POSITION_GAP_THRESHOLD 0.25f

typedef enum {
    FRESHNESS_STAGE_IDLE = 0,
    FRESHNESS_STAGE_WAITING_POSITION,
    FRESHNESS_STAGE_COLLECTING,
    FRESHNESS_STAGE_POSITION_DONE,
    FRESHNESS_STAGE_COMPLETE,
} freshness_stage_t;

typedef enum {
    FRESHNESS_COMMAND_NONE = 0,
    FRESHNESS_COMMAND_CONFIRM,
    FRESHNESS_COMMAND_RETRY,
    FRESHNESS_COMMAND_RESET,
} freshness_command_t;

typedef enum {
    FRESHNESS_RISK_LOW = 0,
    FRESHNESS_RISK_MEDIUM,
    FRESHNESS_RISK_HIGH,
} freshness_risk_level_t;

typedef struct {
    freshness_stage_t stage;
    uint8_t current_position;
    uint8_t completed_positions;
    uint8_t frames_collected;
    float position_probabilities[FRESHNESS_POSITION_COUNT];

    bool removal_confirmed;
    bool cloud_requested;
    bool result_valid;
    bool is_risk;
    bool local_anomaly_detected;
    uint8_t max_risk_position;
    freshness_risk_level_t risk_level;
    float mean_risk_probability;
    float max_position_risk_probability;
    float risk_probability;
    float confidence;
} freshness_classification_t;

void freshness_classifier_reset(void);

esp_err_t freshness_classifier_handle_command(
    freshness_command_t command,
    bool measurement_valid
);

esp_err_t freshness_classifier_update(
    bool measurement_valid,
    const as7341_spectral_data_t *spectrum,
    freshness_classification_t *result
);

esp_err_t freshness_classifier_predict_position(
    const as7341_spectral_data_t *mean_spectrum,
    float *risk_probability
);

const char *freshness_classifier_label(
    const freshness_classification_t *result
);

const char *freshness_classifier_risk_level_text(
    freshness_risk_level_t level
);

#endif /* FRESHNESS_CLASSIFIER_H */
