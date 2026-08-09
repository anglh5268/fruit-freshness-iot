#ifndef MATERIAL_CLASSIFIER_H
#define MATERIAL_CLASSIFIER_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "as7341.h"

#define MATERIAL_CLASSIFIER_MIN_DISTANCE_MM 15U
#define MATERIAL_CLASSIFIER_MAX_DISTANCE_MM 25U

typedef struct {
    bool valid;
    uint8_t class_index;
    const char *label;
    float confidence;
} material_classification_t;

/* Run the exported StandardScaler + multiclass logistic regression model. */
esp_err_t material_classifier_predict(
    const as7341_spectral_data_t *spectrum,
    material_classification_t *result
);

#endif /* MATERIAL_CLASSIFIER_H */
