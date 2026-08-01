#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "display_waveform.h"

#define TEST_PI_F     3.14159265358979323846f
#define TEST_TWO_PI_F (2.0f * TEST_PI_F)

static void make_result(MeasurementResult *result,
                        uint32_t harmonic,
                        float fundamental_phase,
                        float harmonic_phase)
{
    uint32_t point;

    memset(result, 0, sizeof(*result));
    result->fundamental_hz = 10000.0f;
    result->component_count = 2U;
    result->components[0].harmonic = 1U;
    result->components[0].frequency_hz = 10000.0f;
    result->components[0].amplitude_peak_v = 0.005f;
    result->components[0].phase_rad = fundamental_phase;
    result->components[1].harmonic = harmonic;
    result->components[1].frequency_hz = 10000.0f * (float)harmonic;
    result->components[1].amplitude_peak_v = 0.080f;
    result->components[1].phase_rad = harmonic_phase;

    for (point = 0U; point < APP_WAVEFORM_POINTS; point++) {
        float phase = TEST_TWO_PI_F * (float)point /
                      (float)APP_WAVEFORM_POINTS;

        result->waveform_one_period[point] =
            result->components[0].amplitude_peak_v *
                cosf(phase + fundamental_phase) +
            result->components[1].amplitude_peak_v *
                cosf((float)harmonic * phase + harmonic_phase);
    }
}

static double bin_magnitude(
    const uint8_t samples[DISPLAY_WAVEFORM_POINT_COUNT],
    uint32_t bin)
{
    double real = 0.0;
    double imaginary = 0.0;
    uint32_t point;

    for (point = 0U; point < DISPLAY_WAVEFORM_POINT_COUNT; point++) {
        double angle = TEST_TWO_PI_F * (double)bin * (double)point /
                       (double)DISPLAY_WAVEFORM_POINT_COUNT;
        double value = (double)samples[point] - 128.0;

        real += value * cos(angle);
        imaginary -= value * sin(angle);
    }
    return sqrt(real * real + imaginary * imaginary);
}

static uint32_t dominant_bin(
    const uint8_t samples[DISPLAY_WAVEFORM_POINT_COUNT])
{
    uint32_t best_bin = 1U;
    double best_magnitude = 0.0;
    uint32_t bin;

    for (bin = 1U; bin < DISPLAY_WAVEFORM_POINT_COUNT / 2U; bin++) {
        double magnitude = bin_magnitude(samples, bin);

        if (magnitude > best_magnitude) {
            best_magnitude = magnitude;
            best_bin = bin;
        }
    }
    return best_bin;
}

static int run_harmonic_case(uint32_t harmonic, uint8_t period_count)
{
    MeasurementResult result;
    uint8_t samples[DISPLAY_WAVEFORM_POINT_COUNT];
    uint32_t expected_bin = harmonic * (uint32_t)period_count;
    uint32_t actual_bin;

    make_result(&result, harmonic, 0.37f, -0.81f);
    if (!display_waveform_make(&result, period_count, samples)) {
        printf("H%u period=%u generation FAIL\n",
               (unsigned int)harmonic,
               (unsigned int)period_count);
        return 1;
    }
    actual_bin = dominant_bin(samples);
    printf("H%u period=%u dominant=%u expected=%u %s\n",
           (unsigned int)harmonic,
           (unsigned int)period_count,
           (unsigned int)actual_bin,
           (unsigned int)expected_bin,
           actual_bin == expected_bin ? "PASS" : "FAIL");
    return actual_bin == expected_bin ? 0 : 1;
}

static int run_phase_anchor_case(void)
{
    MeasurementResult first;
    MeasurementResult shifted;
    uint8_t first_samples[DISPLAY_WAVEFORM_POINT_COUNT];
    uint8_t shifted_samples[DISPLAY_WAVEFORM_POINT_COUNT];
    const uint32_t harmonic = 16U;
    const float phase_shift = 0.413f;
    uint32_t point;
    uint32_t maximum_delta = 0U;

    make_result(&first, harmonic, 0.27f, -0.62f);
    make_result(&shifted,
                harmonic,
                0.27f + phase_shift,
                -0.62f + (float)harmonic * phase_shift);
    if (!display_waveform_make(&first, 3U, first_samples) ||
        !display_waveform_make(&shifted, 3U, shifted_samples)) {
        printf("phase anchor generation FAIL\n");
        return 1;
    }

    for (point = 0U; point < DISPLAY_WAVEFORM_POINT_COUNT; point++) {
        uint32_t delta = first_samples[point] > shifted_samples[point] ?
            (uint32_t)(first_samples[point] - shifted_samples[point]) :
            (uint32_t)(shifted_samples[point] - first_samples[point]);
        if (delta > maximum_delta) {
            maximum_delta = delta;
        }
    }

    printf("phase anchor max_delta=%u %s\n",
           (unsigned int)maximum_delta,
           maximum_delta <= 2U ? "PASS" : "FAIL");
    return maximum_delta <= 2U ? 0 : 1;
}

int main(void)
{
    static const uint32_t harmonics[] = {4U, 8U, 16U, 49U, 50U};
    static const uint8_t periods[] = {1U, 3U};
    uint32_t harmonic;
    uint32_t period;
    int failures = 0;

    for (period = 0U; period < sizeof(periods) / sizeof(periods[0]);
         period++) {
        for (harmonic = 0U;
             harmonic < sizeof(harmonics) / sizeof(harmonics[0]);
             harmonic++) {
            failures += run_harmonic_case(harmonics[harmonic],
                                          periods[period]);
        }
    }
    failures += run_phase_anchor_case();

    printf("DISPLAY_WAVEFORM_%s failures=%d points=%u\n",
           failures == 0 ? "PASS" : "FAIL",
           failures,
           DISPLAY_WAVEFORM_POINT_COUNT);
    return failures == 0 ? 0 : 1;
}
