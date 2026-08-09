#include "material_classifier.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "fruit_material_model.h"


static void spectrum_to_features(
    const as7341_spectral_data_t *spectrum,
    float features[MATERIAL_MODEL_FEATURE_COUNT]
)
{
    features[0] = (float)spectrum->f1_415nm;
    features[1] = (float)spectrum->f2_445nm;
    features[2] = (float)spectrum->f3_480nm;
    features[3] = (float)spectrum->f4_515nm;
    features[4] = (float)spectrum->f5_555nm;
    features[5] = (float)spectrum->f6_590nm;
    features[6] = (float)spectrum->f7_630nm;
    features[7] = (float)spectrum->f8_680nm;
    features[8] = (float)spectrum->clear;
    features[9] = (float)spectrum->nir;
}


esp_err_t material_classifier_predict(
    const as7341_spectral_data_t *spectrum,
    material_classification_t *result
)
{
    if (spectrum == NULL || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));

    float features[MATERIAL_MODEL_FEATURE_COUNT];
    float standardized[MATERIAL_MODEL_FEATURE_COUNT];
    float kernels[MATERIAL_MODEL_SUPPORT_VECTOR_COUNT];
    uint8_t votes[MATERIAL_MODEL_CLASS_COUNT] = {0};
    float confidence_sums[MATERIAL_MODEL_CLASS_COUNT] = {0.0f};
    float scores[MATERIAL_MODEL_CLASS_COUNT] = {0.0f};

    spectrum_to_features(spectrum, features);

    for (size_t feature = 0;
         feature < MATERIAL_MODEL_FEATURE_COUNT;
         ++feature) {
        standardized[feature] =
            (features[feature] - MATERIAL_SCALER_MEAN[feature]) /
            MATERIAL_SCALER_SCALE[feature];
    }

    for (size_t support_index = 0;
         support_index < MATERIAL_MODEL_SUPPORT_VECTOR_COUNT;
         ++support_index) {
        float squared_distance = 0.0f;
        for (size_t feature = 0;
             feature < MATERIAL_MODEL_FEATURE_COUNT;
             ++feature) {
            float difference =
                standardized[feature] -
                MATERIAL_SVM_SUPPORT_VECTORS[support_index][feature];
            squared_distance += difference * difference;
        }
        kernels[support_index] =
            expf(-MATERIAL_SVM_GAMMA * squared_distance);
    }

    size_t pair_index = 0;
    for (size_t first_class = 0;
         first_class < MATERIAL_MODEL_CLASS_COUNT;
         ++first_class) {
        for (size_t second_class = first_class + 1;
             second_class < MATERIAL_MODEL_CLASS_COUNT;
             ++second_class) {
            float margin = MATERIAL_SVM_INTERCEPT[pair_index];
            const size_t first_start =
                MATERIAL_SVM_SUPPORT_START[first_class];
            const size_t second_start =
                MATERIAL_SVM_SUPPORT_START[second_class];

            for (size_t offset = 0;
                 offset < MATERIAL_SVM_SUPPORT_COUNT[first_class];
                 ++offset) {
                const size_t support_index = first_start + offset;
                margin +=
                    MATERIAL_SVM_DUAL_COEF[second_class - 1][support_index] *
                    kernels[support_index];
            }

            for (size_t offset = 0;
                 offset < MATERIAL_SVM_SUPPORT_COUNT[second_class];
                 ++offset) {
                const size_t support_index = second_start + offset;
                margin +=
                    MATERIAL_SVM_DUAL_COEF[first_class][support_index] *
                    kernels[support_index];
            }

            if (margin >= 0.0f) {
                votes[first_class]++;
            } else {
                votes[second_class]++;
            }
            confidence_sums[first_class] += margin;
            confidence_sums[second_class] -= margin;
            pair_index++;
        }
    }

    uint8_t best_class = 0;
    float best_score = -INFINITY;
    for (size_t class_index = 0;
         class_index < MATERIAL_MODEL_CLASS_COUNT;
         ++class_index) {
        const float confidence_sum = confidence_sums[class_index];
        scores[class_index] =
            (float)votes[class_index] +
            confidence_sum /
                (3.0f * (fabsf(confidence_sum) + 1.0f));

        if (scores[class_index] > best_score) {
            best_score = scores[class_index];
            best_class = (uint8_t)class_index;
        }
    }

    /* Stable softmax: subtract the largest score before expf(). */
    float probability_sum = 0.0f;
    for (size_t class_index = 0;
         class_index < MATERIAL_MODEL_CLASS_COUNT;
         ++class_index) {
        probability_sum += expf(scores[class_index] - best_score);
    }

    if (!isfinite(probability_sum) || probability_sum <= 0.0f) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    result->valid = true;
    result->class_index = best_class;
    result->label = MATERIAL_MODEL_CLASSES[best_class];
    result->confidence = 1.0f / probability_sum;

    return ESP_OK;
}
