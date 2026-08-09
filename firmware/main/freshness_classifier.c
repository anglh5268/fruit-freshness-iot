#include "freshness_classifier.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "nect_freshness_model.h"

static const char *TAG = "FRESHNESS";

typedef struct {
    freshness_classification_t public_result;
    uint64_t sums[10];
    float logistic_probabilities[FRESHNESS_POSITION_COUNT];
    float local_forest_probabilities[FRESHNESS_POSITION_COUNT];
    uint8_t removal_samples;
} freshness_session_t;


static freshness_session_t s_session;


static void clear_accumulator(void)
{
    memset(s_session.sums, 0, sizeof(s_session.sums));
    s_session.public_result.frames_collected = 0;
}


static void add_spectrum(const as7341_spectral_data_t *spectrum)
{
    s_session.sums[0] += spectrum->f1_415nm;
    s_session.sums[1] += spectrum->f2_445nm;
    s_session.sums[2] += spectrum->f3_480nm;
    s_session.sums[3] += spectrum->f4_515nm;
    s_session.sums[4] += spectrum->f5_555nm;
    s_session.sums[5] += spectrum->f6_590nm;
    s_session.sums[6] += spectrum->f7_630nm;
    s_session.sums[7] += spectrum->f8_680nm;
    s_session.sums[8] += spectrum->clear;
    s_session.sums[9] += spectrum->nir;
    s_session.public_result.frames_collected++;
}


static esp_err_t predict_logistic_features(
    const float features[FRESHNESS_MODEL_FEATURE_COUNT],
    float *risk_probability
)
{
    float logit = FRESHNESS_RISK_BIAS;
    for (size_t index = 0;
         index < FRESHNESS_MODEL_FEATURE_COUNT;
         ++index) {
        if (FRESHNESS_SCALER_SCALE[index] <= 0.0f) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        const float standardized =
            (features[index] - FRESHNESS_SCALER_MEAN[index]) /
            FRESHNESS_SCALER_SCALE[index];
        logit += standardized * FRESHNESS_RISK_WEIGHTS[index];
    }

    if (logit >= 0.0f) {
        *risk_probability = 1.0f / (1.0f + expf(-logit));
    } else {
        const float exponential = expf(logit);
        *risk_probability = exponential / (1.0f + exponential);
    }
    if (!isfinite(*risk_probability)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}


static esp_err_t predict_local_forest_features(
    const float features[FRESHNESS_MODEL_FEATURE_COUNT],
    float *risk_probability
)
{
    float probability_sum = 0.0f;
    for (uint16_t tree_index = 0;
         tree_index < FRESHNESS_LOCAL_FOREST_TREE_COUNT;
         ++tree_index) {
        int16_t node_index =
            (int16_t)FRESHNESS_LOCAL_FOREST_ROOTS[tree_index];
        uint16_t visited = 0;

        while (node_index >= 0 &&
               (uint16_t)node_index < FRESHNESS_LOCAL_FOREST_NODE_COUNT &&
               visited < FRESHNESS_LOCAL_FOREST_NODE_COUNT) {
            const freshness_local_forest_node_t *node =
                &FRESHNESS_LOCAL_FOREST_NODES[node_index];
            if (node->feature < 0) {
                probability_sum += node->risk_probability;
                break;
            }
            if ((uint8_t)node->feature >= FRESHNESS_MODEL_FEATURE_COUNT) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            node_index =
                features[(uint8_t)node->feature] <= node->threshold
                    ? node->left
                    : node->right;
            visited++;
        }
        if (node_index < 0 ||
            (uint16_t)node_index >= FRESHNESS_LOCAL_FOREST_NODE_COUNT ||
            visited >= FRESHNESS_LOCAL_FOREST_NODE_COUNT) {
            return ESP_ERR_INVALID_RESPONSE;
        }
    }

    *risk_probability =
        probability_sum / (float)FRESHNESS_LOCAL_FOREST_TREE_COUNT;
    if (!isfinite(*risk_probability)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}


static esp_err_t predict_features(
    const float features[FRESHNESS_MODEL_FEATURE_COUNT],
    float *logistic_probability,
    float *local_forest_probability
)
{
    esp_err_t error = predict_logistic_features(
        features,
        logistic_probability
    );
    if (error != ESP_OK) {
        return error;
    }
    return predict_local_forest_features(
        features,
        local_forest_probability
    );
}


static esp_err_t predict_accumulated_position(
    float *logistic_probability,
    float *local_forest_probability
)
{
    if (s_session.sums[8] == 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    /*
     * Python训练使用 mean(channel) / mean(Clear)。两个均值的帧数相同，
     * 因此直接使用 sum(channel) / sum(Clear)，可避免整数均值的截断误差。
     */
    const float clear_sum = (float)s_session.sums[8];
    const float features[FRESHNESS_MODEL_FEATURE_COUNT] = {
        (float)s_session.sums[0] / clear_sum,
        (float)s_session.sums[1] / clear_sum,
        (float)s_session.sums[2] / clear_sum,
        (float)s_session.sums[3] / clear_sum,
        (float)s_session.sums[4] / clear_sum,
        (float)s_session.sums[5] / clear_sum,
        (float)s_session.sums[6] / clear_sum,
        (float)s_session.sums[7] / clear_sum,
        (float)s_session.sums[9] / clear_sum,
    };
    return predict_features(
        features,
        logistic_probability,
        local_forest_probability
    );
}


static void finish_session(void)
{
    float probability_sum = 0.0f;
    float maximum = 0.0f;
    float second_maximum = 0.0f;
    float maximum_logistic = 0.0f;
    float maximum_local_forest = 0.0f;
    uint8_t maximum_position = 1;

    for (uint8_t index = 0; index < FRESHNESS_POSITION_COUNT; ++index) {
        const float logistic_probability =
            s_session.logistic_probabilities[index];
        const float local_forest_probability =
            s_session.local_forest_probabilities[index];
        const float probability =
            s_session.public_result.position_probabilities[index];
        probability_sum += logistic_probability;
        maximum_logistic = fmaxf(
            maximum_logistic,
            logistic_probability
        );
        maximum_local_forest = fmaxf(
            maximum_local_forest,
            local_forest_probability
        );
        if (index == 0 || probability > maximum) {
            second_maximum = maximum;
            maximum = probability;
            maximum_position = (uint8_t)(index + 1);
        } else if (probability > second_maximum) {
            second_maximum = probability;
        }
    }

    freshness_classification_t *result = &s_session.public_result;
    result->mean_risk_probability =
        probability_sum / (float)FRESHNESS_POSITION_COUNT;
    result->max_position_risk_probability = maximum;
    result->max_risk_position = maximum_position;
    const uint8_t maximum_index =
        (uint8_t)(maximum_position - 1U);
    const bool local_models_agree =
        s_session.logistic_probabilities[maximum_index] >=
            FRESHNESS_LOCAL_LOGISTIC_AGREEMENT_THRESHOLD &&
        s_session.local_forest_probabilities[maximum_index] >=
            FRESHNESS_LOCAL_FOREST_RISK_THRESHOLD;
    const bool local_position_is_distinct =
        maximum - second_maximum >=
            FRESHNESS_LOCAL_POSITION_GAP_THRESHOLD;
    result->local_anomaly_detected =
        maximum_logistic >= FRESHNESS_LOGISTIC_LOCAL_RISK_THRESHOLD ||
        (local_models_agree && local_position_is_distinct);
    result->is_risk =
        result->mean_risk_probability >= FRESHNESS_MEAN_RISK_THRESHOLD ||
        result->local_anomaly_detected;
    result->risk_probability = result->is_risk
        ? fmaxf(result->mean_risk_probability, maximum)
        : result->mean_risk_probability;
    result->confidence = result->is_risk
        ? result->risk_probability
        : 1.0f - result->risk_probability;

    if (!result->is_risk) {
        result->risk_level = FRESHNESS_RISK_LOW;
    } else if (result->risk_probability < 0.7f) {
        result->risk_level = FRESHNESS_RISK_MEDIUM;
    } else {
        result->risk_level = FRESHNESS_RISK_HIGH;
    }

    result->result_valid = true;
    result->stage = FRESHNESS_STAGE_COMPLETE;
    ESP_LOGI(
        TAG,
        "Final mean_logistic=%.3f max_combined=%.3f second=%.3f P%u "
        "forest_max=%.3f local=%u result=%s",
        result->mean_risk_probability,
        result->max_position_risk_probability,
        second_maximum,
        (unsigned int)result->max_risk_position,
        maximum_local_forest,
        result->local_anomaly_detected ? 1U : 0U,
        result->is_risk ? "risk" : "fresh"
    );
}


void freshness_classifier_reset(void)
{
    memset(&s_session, 0, sizeof(s_session));
    s_session.public_result.stage = FRESHNESS_STAGE_IDLE;
    s_session.public_result.current_position = 1;
    s_session.public_result.max_risk_position = 1;
}


esp_err_t freshness_classifier_handle_command(
    freshness_command_t command,
    bool measurement_valid
)
{
    freshness_classification_t *state = &s_session.public_result;

    switch (command) {
        case FRESHNESS_COMMAND_NONE:
            return ESP_OK;

        case FRESHNESS_COMMAND_RESET:
            freshness_classifier_reset();
            return ESP_OK;

        case FRESHNESS_COMMAND_CONFIRM:
            if (state->stage == FRESHNESS_STAGE_COMPLETE) {
                state->cloud_requested = true;
                return ESP_OK;
            }

            if (!measurement_valid) {
                return ESP_ERR_INVALID_STATE;
            }

            if (state->stage == FRESHNESS_STAGE_IDLE ||
                state->stage == FRESHNESS_STAGE_WAITING_POSITION) {
                clear_accumulator();
                state->stage = FRESHNESS_STAGE_COLLECTING;
                return ESP_OK;
            }

            if (state->stage == FRESHNESS_STAGE_POSITION_DONE &&
                state->removal_confirmed &&
                state->current_position < FRESHNESS_POSITION_COUNT) {
                state->current_position++;
                state->removal_confirmed = false;
                clear_accumulator();
                state->stage = FRESHNESS_STAGE_COLLECTING;
                return ESP_OK;
            }
            return ESP_ERR_INVALID_STATE;

        case FRESHNESS_COMMAND_RETRY:
            if (state->stage == FRESHNESS_STAGE_COLLECTING) {
                clear_accumulator();
                state->stage = FRESHNESS_STAGE_WAITING_POSITION;
                return ESP_OK;
            }

            if (state->stage == FRESHNESS_STAGE_POSITION_DONE) {
                const uint8_t index =
                    (uint8_t)(state->current_position - 1);
                state->position_probabilities[index] = 0.0f;
                s_session.logistic_probabilities[index] = 0.0f;
                s_session.local_forest_probabilities[index] = 0.0f;
                state->completed_positions = index;
                state->removal_confirmed = false;
                clear_accumulator();
                state->stage = FRESHNESS_STAGE_WAITING_POSITION;
                return ESP_OK;
            }
            return ESP_ERR_INVALID_STATE;

        default:
            return ESP_ERR_INVALID_ARG;
    }
}


esp_err_t freshness_classifier_predict_position(
    const as7341_spectral_data_t *mean_spectrum,
    float *risk_probability
)
{
    if (mean_spectrum == NULL || risk_probability == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (mean_spectrum->clear == 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const float clear = (float)mean_spectrum->clear;
    const float features[FRESHNESS_MODEL_FEATURE_COUNT] = {
        (float)mean_spectrum->f1_415nm / clear,
        (float)mean_spectrum->f2_445nm / clear,
        (float)mean_spectrum->f3_480nm / clear,
        (float)mean_spectrum->f4_515nm / clear,
        (float)mean_spectrum->f5_555nm / clear,
        (float)mean_spectrum->f6_590nm / clear,
        (float)mean_spectrum->f7_630nm / clear,
        (float)mean_spectrum->f8_680nm / clear,
        (float)mean_spectrum->nir / clear,
    };

    float logistic_probability = 0.0f;
    float local_forest_probability = 0.0f;
    esp_err_t error = predict_features(
        features,
        &logistic_probability,
        &local_forest_probability
    );
    if (error == ESP_OK) {
        *risk_probability = fmaxf(
            logistic_probability,
            local_forest_probability
        );
    }
    return error;
}


esp_err_t freshness_classifier_update(
    bool measurement_valid,
    const as7341_spectral_data_t *spectrum,
    freshness_classification_t *result
)
{
    if (result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (measurement_valid && spectrum == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    freshness_classification_t *state = &s_session.public_result;

    if (state->stage == FRESHNESS_STAGE_COMPLETE) {
        *result = s_session.public_result;
        return ESP_OK;
    }

    if (state->stage == FRESHNESS_STAGE_POSITION_DONE) {
        if (!measurement_valid) {
            if (s_session.removal_samples <
                FRESHNESS_REMOVAL_SAMPLE_COUNT) {
                s_session.removal_samples++;
            }
            if (s_session.removal_samples >=
                FRESHNESS_REMOVAL_SAMPLE_COUNT) {
                state->removal_confirmed = true;
            }
        } else if (!state->removal_confirmed) {
            s_session.removal_samples = 0;
        }
        *result = s_session.public_result;
        return ESP_OK;
    }

    if (state->stage != FRESHNESS_STAGE_COLLECTING ||
        !measurement_valid) {
        *result = s_session.public_result;
        return ESP_OK;
    }

    add_spectrum(spectrum);
    if (state->frames_collected >= FRESHNESS_FRAMES_PER_POSITION) {
        float logistic_probability = 0.0f;
        float local_forest_probability = 0.0f;
        esp_err_t error = predict_accumulated_position(
            &logistic_probability,
            &local_forest_probability
        );
        if (error != ESP_OK) {
            clear_accumulator();
            *result = s_session.public_result;
            return error;
        }

        const uint8_t position_index =
            (uint8_t)(state->current_position - 1);
        s_session.logistic_probabilities[position_index] =
            logistic_probability;
        s_session.local_forest_probabilities[position_index] =
            local_forest_probability;
        state->position_probabilities[position_index] = fmaxf(
            logistic_probability,
            local_forest_probability
        );
        ESP_LOGI(
            TAG,
            "P%u logistic=%.3f local_forest=%.3f combined=%.3f",
            (unsigned int)state->current_position,
            logistic_probability,
            local_forest_probability,
            state->position_probabilities[position_index]
        );
        state->completed_positions = state->current_position;
        clear_accumulator();

        if (state->completed_positions >= FRESHNESS_POSITION_COUNT) {
            finish_session();
        } else {
            state->stage = FRESHNESS_STAGE_POSITION_DONE;
            state->removal_confirmed = false;
            s_session.removal_samples = 0;
        }
    }

    *result = s_session.public_result;
    return ESP_OK;
}


const char *freshness_classifier_label(
    const freshness_classification_t *result
)
{
    if (result == NULL || !result->result_valid) {
        return "pending";
    }
    return result->is_risk ? "risk" : "fresh";
}


const char *freshness_classifier_risk_level_text(
    freshness_risk_level_t level
)
{
    switch (level) {
        case FRESHNESS_RISK_LOW:
            return "low";
        case FRESHNESS_RISK_MEDIUM:
            return "medium";
        case FRESHNESS_RISK_HIGH:
            return "high";
        default:
            return "unknown";
    }
}
