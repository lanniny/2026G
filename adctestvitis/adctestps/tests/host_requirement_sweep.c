#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "signal_analysis.h"

#define PI_F 3.14159265358979323846f
#define TWO_PI_F 6.28318530717958647692f
#define MAX_TEST_TONES 4U
#define AMPLITUDE_ERROR_LIMIT_V 0.001f
#define PHASE_ERROR_LIMIT_RAD 0.10f
#define PHASE_SWEEP_ERROR_LIMIT_RAD 0.15f
#define REFERENCE_WAVEFORM_POINTS 16384U

typedef struct {
    uint32_t harmonic;
    float peak_v;
    float phase_rad;
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
                     t + test->tones[tone].phase_rad);
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

static float phase_error(float actual, float expected)
{
    float error = fmodf(actual - expected + PI_F, TWO_PI_F);

    if (error < 0.0f) {
        error += TWO_PI_F;
    }
    return fabsf(error - PI_F);
}

static float reference_peak_to_peak(const TestCase *test)
{
    float minimum = 1.0e30f;
    float maximum = -1.0e30f;
    uint32_t point;

    for (point = 0U; point < REFERENCE_WAVEFORM_POINTS; point++) {
        float base_phase =
            TWO_PI_F * (float)point /
            (float)REFERENCE_WAVEFORM_POINTS;
        float value = test->dc_v;
        uint32_t tone;

        for (tone = 0U; tone < test->tone_count; tone++) {
            value += test->tones[tone].peak_v *
                cosf(base_phase *
                     (float)test->tones[tone].harmonic +
                     test->tones[tone].phase_rad);
        }
        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
    }

    return maximum - minimum;
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
    printf("  analyze_status=%d flags=0x%08x components=%u f0=%.3f Hz\n",
           analyze_status,
           result.flags,
           result.component_count,
           result.fundamental_hz);

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
            float amplitude_error =
                fabsf(found->amplitude_peak_v -
                      test->tones[tone].peak_v);
            float tone_phase_error =
                phase_error(found->phase_rad,
                            test->tones[tone].phase_rad);
            printf("  H%u    actual=%10.6f V  expected=%10.6f V"
                   "  error=%8.6f V  phase_error=%7.4f rad\n",
                   test->tones[tone].harmonic,
                   found->amplitude_peak_v,
                   test->tones[tone].peak_v,
                   amplitude_error,
                   tone_phase_error);
            failures += amplitude_error > AMPLITUDE_ERROR_LIMIT_V;
            failures += tone_phase_error > PHASE_ERROR_LIMIT_RAD;
        }
    }

    printf("  RESULT %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}

static int run_rejected_case(const TestCase *test,
                             const Calibration *calibration)
{
    MeasurementResult result;
    int analyze_status;
    int passed;

    generate_samples(test, calibration);
    analyze_status = signal_analyze(Samples, calibration, &result);
    passed =
        analyze_status != 0 &&
        (result.flags & MEASUREMENT_FLAG_ANALYSIS_ERROR) != 0U;

    printf("\nCASE %s\n", test->name);
    printf("  analyze_status=%d flags=0x%08x f0=%10.3f Hz\n",
           analyze_status, result.flags, result.fundamental_hz);
    printf("  RESULT %s\n", passed ? "PASS" : "FAIL");
    return passed ? 0 : 1;
}

static int run_component_limit_case(const Calibration *calibration)
{
    static const TestCase test = {
        "retain H1 plus the two strongest harmonics",
        20000.0f,
        0.0f,
        0.0f,
        0.0f,
        4U,
        {{1U, 0.050f, 0.1f},
         {2U, 0.020f, -0.4f},
         {3U, 0.003f, 0.8f},
         {4U, 0.010f, -1.2f}}
    };
    MeasurementResult result;
    uint32_t component;
    uint32_t found_h1 = 0U;
    uint32_t found_h2 = 0U;
    uint32_t found_h3 = 0U;
    uint32_t found_h4 = 0U;
    int analyze_status;
    int passed;

    generate_samples(&test, calibration);
    analyze_status = signal_analyze(Samples, calibration, &result);
    for (component = 0U;
         component < result.component_count;
         component++) {
        uint32_t harmonic = result.components[component].harmonic;

        found_h1 += harmonic == 1U;
        found_h2 += harmonic == 2U;
        found_h3 += harmonic == 3U;
        found_h4 += harmonic == 4U;
    }

    passed =
        analyze_status == 0 &&
        result.flags == 0U &&
        result.component_count == APP_MAX_SIGNAL_COMPONENTS &&
        found_h1 == 1U &&
        found_h2 == 1U &&
        found_h3 == 0U &&
        found_h4 == 1U;

    printf("\nCASE %s\n", test.name);
    printf("  status=%d flags=0x%08x count=%u H1=%u H2=%u H3=%u H4=%u\n",
           analyze_status,
           result.flags,
           result.component_count,
           found_h1,
           found_h2,
           found_h3,
           found_h4);
    printf("  RESULT %s\n", passed ? "PASS" : "FAIL");
    return passed ? 0 : 1;
}

static int run_phase_probability_sweeps(const Calibration *calibration)
{
    TestCase tests[] = {
        {
            "weak H1 with strong H2/H4",
            10000.0f,
            0.0f,
            0.0f,
            0.0f,
            3U,
            {{1U, 0.005f, 0.0f},
             {2U, 0.090f, 0.0f},
             {4U, 0.025f, 0.0f}}
        },
        {
            "minimum high-order H49/H50",
            10000.0f,
            0.0f,
            0.0f,
            0.0f,
            3U,
            {{1U, 0.020f, 0.0f},
             {49U, 0.006f, 0.0f},
             {50U, 0.005f, 0.0f}}
        }
    };
    const uint32_t phases_per_case = 32U;
    float maximum_frequency_error = 0.0f;
    float maximum_amplitude_error = 0.0f;
    float maximum_phase_error = 0.0f;
    uint32_t failed_vectors = 0U;
    uint32_t test_index;

    for (test_index = 0U;
         test_index < sizeof(tests) / sizeof(tests[0]);
         test_index++) {
        uint32_t phase_index;

        for (phase_index = 0U;
             phase_index < phases_per_case;
             phase_index++) {
            MeasurementResult result;
            float base_phase =
                -PI_F + TWO_PI_F *
                ((float)phase_index + 0.25f) /
                (float)phases_per_case;
            float frequency_error;
            uint32_t tone;
            int vector_failed = 0;
            int analyze_status;

            for (tone = 0U; tone < tests[test_index].tone_count; tone++) {
                tests[test_index].tones[tone].phase_rad =
                    fmodf(base_phase * (0.73f + 0.41f * (float)tone) +
                          0.37f * (float)tone,
                          TWO_PI_F);
            }

            generate_samples(&tests[test_index], calibration);
            analyze_status = signal_analyze(Samples, calibration, &result);
            frequency_error =
                fabsf(result.fundamental_hz -
                      tests[test_index].fundamental_hz);
            if (frequency_error > maximum_frequency_error) {
                maximum_frequency_error = frequency_error;
            }

            if (analyze_status != 0 ||
                result.flags != 0U ||
                result.component_count != tests[test_index].tone_count ||
                frequency_error > 5.0f) {
                vector_failed = 1;
            }

            for (tone = 0U; tone < tests[test_index].tone_count; tone++) {
                const SignalComponent *found = NULL;
                uint32_t component;

                for (component = 0U;
                     component < result.component_count;
                     component++) {
                    if (result.components[component].harmonic ==
                        tests[test_index].tones[tone].harmonic) {
                        found = &result.components[component];
                        break;
                    }
                }

                if (found == NULL) {
                    vector_failed = 1;
                }
                else {
                    float amplitude_error =
                        fabsf(found->amplitude_peak_v -
                              tests[test_index].tones[tone].peak_v);
                    float tone_phase_error =
                        phase_error(found->phase_rad,
                                    tests[test_index].tones[tone].phase_rad);

                    if (amplitude_error > maximum_amplitude_error) {
                        maximum_amplitude_error = amplitude_error;
                    }
                    if (tone_phase_error > maximum_phase_error) {
                        maximum_phase_error = tone_phase_error;
                    }
                    if (amplitude_error > AMPLITUDE_ERROR_LIMIT_V ||
                        tone_phase_error > PHASE_SWEEP_ERROR_LIMIT_RAD) {
                        vector_failed = 1;
                    }
                }
            }

            failed_vectors += vector_failed != 0;
        }
    }

    printf("\nCASE 500 Hz grid low-frequency phase probability sweep\n");
    printf("  vectors=%u failed=%u max_f_error=%.3f Hz"
           " max_amp_error=%.6f V max_phase_error=%.4f rad\n",
           (unsigned int)(phases_per_case *
                          (sizeof(tests) / sizeof(tests[0]))),
           (unsigned int)failed_vectors,
           maximum_frequency_error,
           maximum_amplitude_error,
           maximum_phase_error);
    printf("  RESULT %s\n", failed_vectors == 0U ? "PASS" : "FAIL");
    return failed_vectors == 0U ? 0 : 1;
}

static uint32_t next_deterministic_value(uint32_t *state)
{
    *state = *state * 1664525U + 1013904223U;
    return *state;
}

static float next_phase(uint32_t *state)
{
    uint32_t value = next_deterministic_value(state) >> 8U;

    return -PI_F + TWO_PI_F *
           ((float)value / 16777216.0f);
}

static int run_full_grid_requirement_sweep(const Calibration *calibration)
{
    const uint32_t vector_count = 160U;
    uint32_t state = 0x2208500U;
    uint32_t failed_vectors = 0U;
    float maximum_frequency_error = 0.0f;
    float maximum_amplitude_error = 0.0f;
    float maximum_phase_error = 0.0f;
    float maximum_vpp_error = 0.0f;
    uint32_t vector;

    for (vector = 0U; vector < vector_count; vector++) {
        TestCase test;
        MeasurementResult result;
        uint32_t grid_index;
        uint32_t maximum_harmonic;
        uint32_t tone;
        float expected_rms_square = 0.0f;
        float frequency_error;
        float vpp_error;
        int analyze_status;
        int vector_failed = 0;

        memset(&test, 0, sizeof(test));
        test.name = "full 500 Hz fundamental grid sweep";
        grid_index = 20U +
            next_deterministic_value(&state) % 481U;
        test.fundamental_hz =
            (float)grid_index * APP_FUNDAMENTAL_GRID_HZ;
        maximum_harmonic =
            (uint32_t)floorf(APP_ANALYSIS_MAX_HZ /
                             test.fundamental_hz);
        test.tone_count =
            maximum_harmonic >= 3U &&
            (next_deterministic_value(&state) & 1U) != 0U ?
                3U : 2U;

        test.tones[0].harmonic = 1U;
        test.tones[1].harmonic =
            2U + next_deterministic_value(&state) %
                 (maximum_harmonic - 1U);
        if (test.tone_count == 3U) {
            do {
                test.tones[2].harmonic =
                    2U + next_deterministic_value(&state) %
                         (maximum_harmonic - 1U);
            } while (test.tones[2].harmonic ==
                     test.tones[1].harmonic);
        }

        switch (vector % 3U) {
        case 0U:
            test.tones[0].peak_v = 0.005f;
            test.tones[1].peak_v = 0.060f;
            test.tones[2].peak_v = 0.020f;
            break;
        case 1U:
            test.tones[0].peak_v = 0.060f;
            test.tones[1].peak_v = 0.005f;
            test.tones[2].peak_v = 0.007f;
            break;
        default:
            test.tones[0].peak_v = 0.040f;
            test.tones[1].peak_v = 0.020f;
            test.tones[2].peak_v = 0.010f;
            break;
        }

        for (tone = 0U; tone < test.tone_count; tone++) {
            test.tones[tone].phase_rad = next_phase(&state);
            expected_rms_square +=
                0.5f * test.tones[tone].peak_v *
                test.tones[tone].peak_v;
        }
        test.expected_rms_ac_v = sqrtf(expected_rms_square);
        test.expected_vpp_v = reference_peak_to_peak(&test);

        generate_samples(&test, calibration);
        analyze_status = signal_analyze(Samples, calibration, &result);
        frequency_error =
            fabsf(result.fundamental_hz - test.fundamental_hz);
        if (frequency_error > maximum_frequency_error) {
            maximum_frequency_error = frequency_error;
        }
        vpp_error =
            fabsf(result.peak_to_peak_volts - test.expected_vpp_v);
        if (vpp_error > maximum_vpp_error) {
            maximum_vpp_error = vpp_error;
        }

        if (analyze_status != 0 ||
            result.flags != 0U ||
            result.component_count != test.tone_count ||
            frequency_error > 5.0f ||
            vpp_error > 0.005f ||
            fabsf(result.rms_ac_volts - test.expected_rms_ac_v) >
                AMPLITUDE_ERROR_LIMIT_V) {
            vector_failed = 1;
        }

        for (tone = 0U; tone < test.tone_count; tone++) {
            const SignalComponent *found = NULL;
            uint32_t component;

            for (component = 0U;
                 component < result.component_count;
                 component++) {
                if (result.components[component].harmonic ==
                    test.tones[tone].harmonic) {
                    found = &result.components[component];
                    break;
                }
            }

            if (found == NULL) {
                vector_failed = 1;
            }
            else {
                float amplitude_error =
                    fabsf(found->amplitude_peak_v -
                          test.tones[tone].peak_v);
                float tone_phase_error =
                    phase_error(found->phase_rad,
                                test.tones[tone].phase_rad);

                if (amplitude_error > maximum_amplitude_error) {
                    maximum_amplitude_error = amplitude_error;
                }
                if (tone_phase_error > maximum_phase_error) {
                    maximum_phase_error = tone_phase_error;
                }
                if (amplitude_error > AMPLITUDE_ERROR_LIMIT_V ||
                    tone_phase_error > PHASE_SWEEP_ERROR_LIMIT_RAD) {
                    vector_failed = 1;
                }
            }
        }

        if (vector_failed != 0 && failed_vectors < 8U) {
            printf("  FAIL vector=%u f0=%.1f Hz max_h=%u"
                   " status=%d flags=0x%08x components=%u/%u\n",
                   (unsigned int)vector,
                   test.fundamental_hz,
                   (unsigned int)maximum_harmonic,
                   analyze_status,
                   result.flags,
                   result.component_count,
                   test.tone_count);
        }
        failed_vectors += vector_failed != 0;
    }

    printf("\nCASE full 10-250 kHz fundamental grid requirement sweep\n");
    printf("  vectors=%u failed=%u max_f_error=%.3f Hz"
           " max_amp_error=%.6f V max_phase_error=%.4f rad"
           " max_vpp_error=%.6f V\n",
           (unsigned int)vector_count,
           (unsigned int)failed_vectors,
           maximum_frequency_error,
           maximum_amplitude_error,
           maximum_phase_error,
           maximum_vpp_error);
    printf("  RESULT %s\n", failed_vectors == 0U ? "PASS" : "FAIL");
    return failed_vectors == 0U ? 0 : 1;
}

int main(void)
{
    static const TestCase cases[] = {
        {
            "pure low-frequency high-amplitude sine: no false harmonics",
            10000.0f,
            0.0f,
            0.250f,
            0.0883883f,
            1U,
            {{1U, 0.125f, 0.0f}}
        },
        {
            "pure 11.1 kHz high-amplitude sine at arbitrary phase",
            11100.0f,
            0.0f,
            0.250f,
            0.0883883f,
            1U,
            {{1U, 0.125f, 1.1f}}
        },
        {
            "official 10.5 kHz example grid: H1/H3/H4",
            10500.0f,
            0.0f,
            0.1163036f,
            0.0324037f,
            3U,
            {{1U, 0.040f, 0.2f},
             {3U, 0.020f, -1.1f},
             {4U, 0.010f, 0.7f}}
        },
        {
            "minimum 5 mV H1 below strong H2/H4",
            10000.0f,
            0.0f,
            0.1980214f,
            0.0661438f,
            3U,
            {{1U, 0.005f, 0.4f},
             {2U, 0.090f, -0.7f},
             {4U, 0.025f, 1.3f}}
        },
        {
            "10 kHz upper harmonic orders: H49/H50",
            10000.0f,
            0.0f,
            0.0560567f,
            0.0151822f,
            3U,
            {{1U, 0.020f, 0.1f},
             {49U, 0.006f, -1.2f},
             {50U, 0.005f, 0.8f}}
        },
        {
            "500 Hz grid with 1250 ppm sample-clock ratio offset",
            10012.5f,
            0.0f,
            0.1338168f,
            0.0364966f,
            3U,
            {{1U, 0.050f, 0.3f},
             {24U, 0.010f, -1.0f},
             {32U, 0.008f, 0.7f}}
        },
        {
            "high-frequency clock offset uses continuous fallback",
            200250.0f,
            0.0f,
            0.1068654f,
            0.0360555f,
            2U,
            {{1U, 0.050f, 0.3f},
             {2U, 0.010f, -1.0f}}
        },
        {
            "10 kHz high-order amplitude and phase: H24/H48",
            10000.0f,
            0.0f,
            0.1239241f,
            0.0363043f,
            3U,
            {{1U, 0.050f, 0.3f},
             {24U, 0.010f, -1.0f},
             {48U, 0.006f, -2.0f}}
        },
        {
            "lower boundary: 10 kHz, 100 mVpp, H2",
            10000.0f,
            1.0f,
            0.100f,
            0.0364434f,
            2U,
            {{1U, 0.050f, 0.0f}, {2U, 0.0125f, 0.0f}}
        },
        {
            "typical: 50 kHz, 200 mVpp, H2",
            50000.0f,
            1.0f,
            0.200f,
            0.0728869f,
            2U,
            {{1U, 0.100f, 0.0f}, {2U, 0.025f, 0.0f}}
        },
        {
            "upper boundary: 100 kHz, 250 mVpp, H2",
            100000.0f,
            1.0f,
            0.250f,
            0.0911086f,
            2U,
            {{1U, 0.125f, 0.0f}, {2U, 0.03125f, 0.0f}}
        },
        {
            "two harmonics: H1 + H3 + H5",
            30000.0f,
            1.0f,
            0.200f,
            0.0519615f,
            3U,
            {{1U, 0.070f, 0.0f},
             {3U, 0.020f, 0.0f},
             {5U, 0.010f, 0.0f}}
        },
        {
            "R2 lower boundary: 10 kHz, 50 mVpp, H2",
            10000.0f,
            1.0f,
            0.050f,
            0.0182217f,
            2U,
            {{1U, 0.025f, 0.0f}, {2U, 0.00625f, 0.0f}}
        },
        {
            "R2 upper frequency: 250 kHz + 500 kHz",
            250000.0f,
            1.0f,
            0.200f,
            0.0728869f,
            2U,
            {{1U, 0.100f, 0.0f}, {2U, 0.025f, 0.0f}}
        },
        {
            "R2 upper amplitude: 100/300/500 kHz, 250 mVpp",
            100000.0f,
            1.0f,
            0.250f,
            0.0649519f,
            3U,
            {{1U, 0.0875f, 0.0f},
             {3U, 0.025f, 0.0f},
             {5U, 0.0125f, 0.0f}}
        }
    };
    static const TestCase rejected_cases[] = {
        {
            "below analysis range: 9.9 kHz must not clamp to 10 kHz",
            9900.0f,
            0.0f,
            0.200f,
            0.0707107f,
            1U,
            {{1U, 0.100f, 0.4f}}
        },
        {
            "above fundamental range: 250.5 kHz cannot contain a harmonic",
            250500.0f,
            0.0f,
            0.200f,
            0.0707107f,
            1U,
            {{1U, 0.100f, -0.8f}}
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
    for (i = 0U;
         i < sizeof(rejected_cases) / sizeof(rejected_cases[0]);
         i++) {
        failed_cases +=
            run_rejected_case(&rejected_cases[i], &calibration);
    }
    failed_cases += run_component_limit_case(&calibration);
    failed_cases += run_phase_probability_sweeps(&calibration);
    failed_cases += run_full_grid_requirement_sweep(&calibration);

    printf("\nREQUIREMENT_SWEEP_%s failed_cases=%d\n",
           failed_cases == 0 ? "PASS" : "FAIL",
           failed_cases);
    return failed_cases == 0 ? 0 : 1;
}
