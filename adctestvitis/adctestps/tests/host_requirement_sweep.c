#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "signal_analysis.h"

#define TWO_PI_F 6.28318530717958647692f
#define MAX_TEST_TONES 3U

typedef struct {
    uint32_t harmonic;
    float peak_v;
} TestTone;

typedef struct {
    const char *name;
    float fundamental_hz;
    float dc_v;
    float expected_vpp_v;
    float expected_rms_ac_v;
    uint32_t tone_count;
    TestTone tones[MAX_TEST_TONES];
} TestCase;

static uint16_t Samples[APP_FRAME_SAMPLES];

static void make_calibration(Calibration *calibration)
{
    calibration_load_defaults(calibration);
}

static void generate_samples(const TestCase *test,
                             const Calibration *calibration)
{
    uint32_t sample;

    for (sample = 0U; sample < APP_FRAME_SAMPLES; sample++) {
        float t = (float)sample / APP_SAMPLE_RATE_HZ;
        float voltage = test->dc_v;
        uint32_t tone;
        long code;

        for (tone = 0U; tone < test->tone_count; tone++) {
            voltage +=
                test->tones[tone].peak_v *
                cosf(TWO_PI_F *
                     test->fundamental_hz *
                     (float)test->tones[tone].harmonic *
                     t);
        }

        code = lroundf(
            calibration->zero_code +
            voltage / calibration->input_volts_per_code);
        if (code < 0L) {
            code = 0L;
        }
        if (code > (long)APP_ADC_CODE_MAX) {
            code = (long)APP_ADC_CODE_MAX;
        }
        Samples[sample] = (uint16_t)code;
    }
}

static int within(float actual, float expected, float limit)
{
    return fabsf(actual - expected) <= limit;
}

static int run_case(const TestCase *test,
                    const Calibration *calibration)
{
    MeasurementResult result;
    uint32_t tone;
    int failures = 0;
    int analyze_status;

    generate_samples(test, calibration);
    analyze_status = signal_analyze(Samples, calibration, &result);

    printf("\nCASE %s\n", test->name);
    printf("  analyze_status=%d flags=0x%08x components=%u\n",
           analyze_status, result.flags, result.component_count);

    if (analyze_status != 0) {
        printf("  RESULT FAIL: analysis rejected the test signal\n");
        return 1;
    }

    printf("  f0    actual=%10.3f Hz expected=%10.3f Hz error=%8.3f Hz\n",
           result.fundamental_hz,
           test->fundamental_hz,
           fabsf(result.fundamental_hz - test->fundamental_hz));
    printf("  Vpp   actual=%10.6f V  expected=%10.6f V  error=%8.6f V\n",
           result.peak_to_peak_volts,
           test->expected_vpp_v,
           fabsf(result.peak_to_peak_volts - test->expected_vpp_v));
    printf("  RMSac actual=%10.6f V  expected=%10.6f V  error=%8.6f V\n",
           result.rms_ac_volts,
           test->expected_rms_ac_v,
           fabsf(result.rms_ac_volts - test->expected_rms_ac_v));

    failures += result.flags != 0U;
    failures += !within(result.fundamental_hz,
                        test->fundamental_hz,
                        1000.0f);
    failures += !within(result.peak_to_peak_volts,
                        test->expected_vpp_v,
                        0.005f);
    failures += !within(result.rms_ac_volts,
                        test->expected_rms_ac_v,
                        0.005f);
    failures += result.component_count != test->tone_count;

    for (tone = 0U; tone < test->tone_count; tone++) {
        const SignalComponent *found = NULL;
        uint32_t component;

        for (component = 0U;
             component < result.component_count;
             component++) {
            if (result.components[component].harmonic ==
                test->tones[tone].harmonic) {
                found = &result.components[component];
                break;
            }
        }

        if (found == NULL) {
            printf("  H%u    missing\n", test->tones[tone].harmonic);
            failures++;
        }
        else {
            float error =
                fabsf(found->amplitude_peak_v -
                      test->tones[tone].peak_v);
            printf("  H%u    actual=%10.6f V  expected=%10.6f V"
                   "  error=%8.6f V\n",
                   test->tones[tone].harmonic,
                   found->amplitude_peak_v,
                   test->tones[tone].peak_v,
                   error);
            failures += error > 0.005f;
        }
    }

    printf("  RESULT %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}

int main(void)
{
    static const TestCase cases[] = {
        {
            "lower boundary: 10 kHz, 100 mVpp, H2",
            10000.0f,
            1.0f,
            0.100f,
            0.0364434f,
            2U,
            {{1U, 0.050f}, {2U, 0.0125f}}
        },
        {
            "typical: 50 kHz, 200 mVpp, H2",
            50000.0f,
            1.0f,
            0.200f,
            0.0728869f,
            2U,
            {{1U, 0.100f}, {2U, 0.025f}}
        },
        {
            "upper boundary: 100 kHz, 250 mVpp, H2",
            100000.0f,
            1.0f,
            0.250f,
            0.0911086f,
            2U,
            {{1U, 0.125f}, {2U, 0.03125f}}
        },
        {
            "two harmonics: H1 + H3 + H5",
            30000.0f,
            1.0f,
            0.200f,
            0.0519615f,
            3U,
            {{1U, 0.070f}, {3U, 0.020f}, {5U, 0.010f}}
        },
        {
            "R2 lower boundary: 10 kHz, 50 mVpp, H2",
            10000.0f,
            1.0f,
            0.050f,
            0.0182217f,
            2U,
            {{1U, 0.025f}, {2U, 0.00625f}}
        },
        {
            "R2 upper frequency: 250 kHz + 500 kHz",
            250000.0f,
            1.0f,
            0.200f,
            0.0728869f,
            2U,
            {{1U, 0.100f}, {2U, 0.025f}}
        },
        {
            "R2 upper amplitude: 100/300/500 kHz, 250 mVpp",
            100000.0f,
            1.0f,
            0.250f,
            0.0649519f,
            3U,
            {{1U, 0.0875f}, {3U, 0.025f}, {5U, 0.0125f}}
        }
    };
    Calibration calibration;
    uint32_t i;
    int failed_cases = 0;

    make_calibration(&calibration);
    signal_analysis_init();

    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        failed_cases += run_case(&cases[i], &calibration);
    }

    printf("\nREQUIREMENT_SWEEP_%s failed_cases=%d\n",
           failed_cases == 0 ? "PASS" : "FAIL",
           failed_cases);
    return failed_cases == 0 ? 0 : 1;
}
