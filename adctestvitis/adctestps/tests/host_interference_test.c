#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "signal_analysis.h"

#define TWO_PI_D 6.28318530717958647692
#define RAW_SAMPLE_RATE_HZ 64000000.0
#define RAW_DECIMATION 16U

/*
 * Requirement-3 test signal:
 *   ub = 40 mV peak at 100 kHz + 10 mV peak at 300 kHz
 *   ub peak-to-peak = 100 mV
 *   uj = 100 mV peak (200 mVpp)
 */
#define UB_FUNDAMENTAL_HZ 100000.0f
#define UB_H1_PEAK_V      0.040f
#define UB_H3_PEAK_V      0.010f
#define UB_VPP_V          0.100f
#define UB_RMS_V          0.0291548f
#define UJ_PEAK_V         0.100f

static uint16_t Samples[APP_FRAME_SAMPLES];

static void make_calibration(Calibration *calibration)
{
    calibration_load_defaults(calibration);
}

/*
 * Generate the raw 64 MS/s timing positions selected by the PL decimator.
 * interference_gain models the residual gain of the external analog
 * low-pass filter at fj:
 *   1.0  = no suppression
 *   0.01 = 40 dB suppression
 */
static void generate_decimated_frame(const Calibration *calibration,
                                     float interference_hz,
                                     float interference_gain)
{
    uint32_t output_index;

    for (output_index = 0U;
         output_index < APP_FRAME_SAMPLES;
         output_index++) {
        uint64_t raw_index =
            (uint64_t)output_index * RAW_DECIMATION + 15U;
        double t = (double)raw_index / RAW_SAMPLE_RATE_HZ;
        double voltage =
            1.0 +
            UB_H1_PEAK_V *
                cos(TWO_PI_D * UB_FUNDAMENTAL_HZ * t) +
            UB_H3_PEAK_V *
                cos(TWO_PI_D * 3.0 * UB_FUNDAMENTAL_HZ * t) +
            UJ_PEAK_V * interference_gain *
                cos(TWO_PI_D * interference_hz * t);
        long code = lround(
            calibration->zero_code +
            voltage / calibration->input_volts_per_code);

        if (code < 0L) {
            code = 0L;
        }
        if (code > (long)APP_ADC_CODE_MAX) {
            code = (long)APP_ADC_CODE_MAX;
        }
        Samples[output_index] = (uint16_t)code;
    }
}

static int numerical_requirements_pass(const MeasurementResult *result)
{
    const SignalComponent *h1 = NULL;
    const SignalComponent *h3 = NULL;
    uint32_t i;

    for (i = 0U; i < result->component_count; i++) {
        if (result->components[i].harmonic == 1U) {
            h1 = &result->components[i];
        }
        if (result->components[i].harmonic == 3U) {
            h3 = &result->components[i];
        }
    }

    if (result->flags != 0U ||
        fabsf(result->fundamental_hz - UB_FUNDAMENTAL_HZ) > 1000.0f ||
        fabsf(result->peak_to_peak_volts - UB_VPP_V) > 0.005f ||
        fabsf(result->rms_ac_volts - UB_RMS_V) > 0.005f ||
        h1 == NULL ||
        h3 == NULL ||
        fabsf(h1->amplitude_peak_v - UB_H1_PEAK_V) > 0.005f ||
        fabsf(h3->amplitude_peak_v - UB_H3_PEAK_V) > 0.005f) {
        return 0;
    }

    return 1;
}

static int run_interference_case(const char *name,
                                 const Calibration *calibration,
                                 float interference_hz,
                                 float interference_gain,
                                 int expected_to_pass)
{
    MeasurementResult result;
    int status;
    int passed;

    generate_decimated_frame(calibration,
                             interference_hz,
                             interference_gain);
    status = signal_analyze(Samples, calibration, &result);
    passed = status == 0 && numerical_requirements_pass(&result);

    printf("\nCASE %s\n", name);
    printf("  fj=%.3f MHz residual_gain=%.6f (%.2f dB)\n",
           interference_hz / 1.0e6f,
           interference_gain,
           20.0f * log10f(interference_gain));
    printf("  status=%d flags=0x%08x components=%u\n",
           status, result.flags, result.component_count);
    printf("  f0=%.3f Hz Vpp=%.6f V RMSac=%.6f V\n",
           result.fundamental_hz,
           result.peak_to_peak_volts,
           result.rms_ac_volts);

    if (passed != expected_to_pass) {
        printf("  RESULT FAIL: expected %s, observed %s\n",
               expected_to_pass ? "PASS" : "FAIL",
               passed ? "PASS" : "FAIL");
        return 1;
    }

    printf("  RESULT %s (as expected)\n",
           passed ? "PASS" : "FAIL");
    return 0;
}

int main(void)
{
    Calibration calibration;
    int failures = 0;

    make_calibration(&calibration);
    signal_analysis_init();

    /*
     * 3.9 MHz aliases to 100 kHz after 4 MS/s sample selection.  It is a
     * legal fj >= 1 MHz and directly corrupts the wanted fundamental when
     * no analog low-pass filter is present.
     */
    failures += run_interference_case(
        "no LPF: 3.9 MHz aliases onto the 100 kHz fundamental",
        &calibration,
        3900000.0f,
        1.0f,
        0);

    /*
     * A behavioral hardware-filter target of at least 40 dB stop-band
     * attenuation leaves at most 1 mV peak from the 200 mVpp interferer.
     */
    failures += run_interference_case(
        "LPF target: 3.9 MHz with 40 dB suppression",
        &calibration,
        3900000.0f,
        0.01f,
        1);

    failures += run_interference_case(
        "LPF target: minimum 1.0 MHz with 40 dB suppression",
        &calibration,
        1000000.0f,
        0.01f,
        1);

    printf("\nINTERFERENCE_TEST_%s failures=%d\n",
           failures == 0 ? "PASS" : "FAIL",
           failures);
    return failures == 0 ? 0 : 1;
}
