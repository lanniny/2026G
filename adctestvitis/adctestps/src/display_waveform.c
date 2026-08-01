#include "display_waveform.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define DISPLAY_PI_F      3.14159265358979323846f
#define DISPLAY_TWO_PI_F  (2.0f * DISPLAY_PI_F)
#define DISPLAY_CODE_LOW  16U
#define DISPLAY_CODE_SPAN 223U

static float wrap_waveform_position(float position)
{
    const float point_count = (float)APP_WAVEFORM_POINTS;

    while (position < 0.0f) {
        position += point_count;
    }
    while (position >= point_count) {
        position -= point_count;
    }
    return position;
}

int display_waveform_make(
    const MeasurementResult *result,
    uint8_t period_count,
    uint8_t output[DISPLAY_WAVEFORM_POINT_COUNT])
{
    float minimum;
    float maximum;
    float span;
    float anchor_position = 0.0f;
    uint32_t point;
    uint32_t component;

    if (output == NULL) {
        return 0;
    }
    memset(output, 128, DISPLAY_WAVEFORM_POINT_COUNT);

    if (result == NULL ||
        (period_count != 1U && period_count != 3U)) {
        return 0;
    }

    minimum = result->waveform_one_period[0];
    maximum = minimum;
    if (!isfinite(minimum)) {
        return 0;
    }
    for (point = 1U; point < APP_WAVEFORM_POINTS; point++) {
        float sample = result->waveform_one_period[point];

        if (!isfinite(sample)) {
            return 0;
        }
        if (sample < minimum) {
            minimum = sample;
        }
        if (sample > maximum) {
            maximum = sample;
        }
    }

    span = maximum - minimum;
    if (!isfinite(span) || span < 1.0e-9f) {
        return 0;
    }

    /* Anchor H1 at its rising zero crossing so capture phase cannot slide. */
    for (component = 0U;
         component < result->component_count &&
         component < APP_MAX_COMPONENTS;
         component++) {
        if (result->components[component].harmonic == 1U &&
            isfinite(result->components[component].phase_rad)) {
            anchor_position =
                (-0.5f * DISPLAY_PI_F -
                 result->components[component].phase_rad) *
                (float)APP_WAVEFORM_POINTS / DISPLAY_TWO_PI_F;
            break;
        }
    }

    for (point = 0U; point < DISPLAY_WAVEFORM_POINT_COUNT; point++) {
        float source_position =
            anchor_position +
            (float)point * (float)period_count *
            (float)APP_WAVEFORM_POINTS /
            (float)DISPLAY_WAVEFORM_POINT_COUNT;
        uint32_t index0;
        uint32_t index1;
        float fraction;
        float sample;
        float normalized;
        uint32_t code;

        source_position = wrap_waveform_position(source_position);
        index0 = (uint32_t)source_position;
        index1 = (index0 + 1U) % APP_WAVEFORM_POINTS;
        fraction = source_position - (float)index0;
        sample = result->waveform_one_period[index0] +
            fraction *
            (result->waveform_one_period[index1] -
             result->waveform_one_period[index0]);
        normalized = (sample - minimum) / span;
        if (normalized < 0.0f) {
            normalized = 0.0f;
        }
        if (normalized > 1.0f) {
            normalized = 1.0f;
        }

        code = DISPLAY_CODE_LOW +
            (uint32_t)(normalized * (float)DISPLAY_CODE_SPAN + 0.5f);
        if (code > 255U) {
            code = 255U;
        }
        output[point] = (uint8_t)code;
    }

    return 1;
}
