#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "signal_analysis.h"

#define EXPECTED_FUNDAMENTAL_HZ 50000.0f
#define EXPECTED_DC_V           1.0f
#define EXPECTED_VPP_V          0.200f
#define EXPECTED_AC_RMS_V       0.0728869f
#define EXPECTED_H1_PEAK_V      0.100f
#define EXPECTED_H2_PEAK_V      0.025f

static int check_close(const char *name,
                       float actual,
                       float expected,
                       float tolerance)
{
    float error = fabsf(actual - expected);

    printf("%-22s actual=% .9f expected=% .9f error=% .9f limit=% .9f %s\n",
           name,
           actual,
           expected,
           error,
           tolerance,
           error <= tolerance ? "PASS" : "FAIL");
    return error <= tolerance ? 0 : 1;
}

static int load_samples(const char *path, uint16_t *samples)
{
    FILE *input = fopen(path, "r");
    uint32_t count = 0U;
    unsigned int code;

    if (input == NULL) {
        fprintf(stderr, "Could not open PL sample file: %s\n", path);
        return -1;
    }

    while (count < APP_FRAME_SAMPLES &&
           fscanf(input, "%u", &code) == 1) {
        if (code > APP_ADC_CODE_MASK) {
            fprintf(stderr, "Invalid ADC code %u at index %u\n",
                    code, count);
            fclose(input);
            return -1;
        }
        samples[count++] = (uint16_t)code;
    }

    fclose(input);

    if (count != APP_FRAME_SAMPLES) {
        fprintf(stderr, "Expected %u samples, loaded %u\n",
                APP_FRAME_SAMPLES, count);
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    static uint16_t samples[APP_FRAME_SAMPLES];
    Calibration calibration;
    MeasurementResult result;
    const SignalComponent *h1 = NULL;
    const SignalComponent *h2 = NULL;
    const char *sample_path =
        argc > 1 ? argv[1] : "pl_samples.txt";
    uint32_t i;
    int failures = 0;

    if (load_samples(sample_path, samples) != 0) {
        return 2;
    }

    calibration_load_defaults(&calibration);

    signal_analysis_init();
    if (signal_analyze(samples, &calibration, &result) != 0) {
        fprintf(stderr, "signal_analyze returned an error, flags=0x%08x\n",
                result.flags);
        return 3;
    }

    printf("flags=0x%08x min=%u max=%u clipped=%u components=%u snr=%.2f dB\n",
           result.flags,
           result.minimum_code,
           result.maximum_code,
           result.clipped_sample_count,
           result.component_count,
           result.snr_db);

    failures += result.flags != 0U;
    failures += result.clipped_sample_count != 0U;
    failures += check_close("fundamental_hz",
                            result.fundamental_hz,
                            EXPECTED_FUNDAMENTAL_HZ,
                            1000.0f);
    failures += check_close("peak_to_peak_v",
                            result.peak_to_peak_volts,
                            EXPECTED_VPP_V,
                            0.005f);
    failures += check_close("rms_ac_v",
                            result.rms_ac_volts,
                            EXPECTED_AC_RMS_V,
                            0.005f);
    failures += check_close("dc_v",
                            result.dc_volts,
                            EXPECTED_DC_V,
                            0.005f);

    for (i = 0U; i < result.component_count; i++) {
        if (result.components[i].harmonic == 1U) {
            h1 = &result.components[i];
        }
        if (result.components[i].harmonic == 2U) {
            h2 = &result.components[i];
        }
    }

    if (h1 == NULL || h2 == NULL) {
        fprintf(stderr, "Required H1/H2 components were not both detected\n");
        failures++;
    }
    else {
        failures += check_close("H1 peak_v",
                                h1->amplitude_peak_v,
                                EXPECTED_H1_PEAK_V,
                                0.005f);
        failures += check_close("H2 peak_v",
                                h2->amplitude_peak_v,
                                EXPECTED_H2_PEAK_V,
                                0.005f);
    }

    if (result.component_count != 2U) {
        fprintf(stderr, "Expected exactly 2 components, got %u\n",
                result.component_count);
        failures++;
    }

    if (failures != 0) {
        printf("PS_ANALYSIS_FAIL failures=%d\n", failures);
        return 1;
    }

    printf("PS_ANALYSIS_PASS\n");
    return 0;
}
