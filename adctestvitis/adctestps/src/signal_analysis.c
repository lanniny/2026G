#include "signal_analysis.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI_F                    3.14159265358979323846f
#define TWO_PI_F                (2.0f * PI_F)
#define FFT_HALF_SIZE           (APP_FRAME_SAMPLES / 2U)
#define FUNDAMENTAL_COVERAGE    0.75f
#define FUNDAMENTAL_MIN_POWER   0.0025f

typedef struct {
    uint32_t bin;
    float refined_bin;
    float power;
} PeakCandidate;

static float TimeSignal[APP_FRAME_SAMPLES];
static float FftReal[APP_FRAME_SAMPLES];
static float FftImag[APP_FRAME_SAMPLES];
static float Window[APP_FRAME_SAMPLES];
static float SpectrumPower[FFT_HALF_SIZE + 1U];
static float NoiseWork[FFT_HALF_SIZE + 1U];
static float WindowSum;
static uint32_t AnalysisInitialized;

static float maximum_float(float a, float b)
{
    return (a > b) ? a : b;
}

static float clamp_float(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static int compare_float(const void *left, const void *right)
{
    float a = *(const float *)left;
    float b = *(const float *)right;

    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

static int compare_peak_frequency(const void *left, const void *right)
{
    const PeakCandidate *a = (const PeakCandidate *)left;
    const PeakCandidate *b = (const PeakCandidate *)right;

    if (a->refined_bin < b->refined_bin) {
        return -1;
    }
    if (a->refined_bin > b->refined_bin) {
        return 1;
    }
    return 0;
}

void signal_analysis_init(void)
{
    uint32_t i;

    WindowSum = 0.0f;
    for (i = 0U; i < APP_FRAME_SAMPLES; i++) {
        Window[i] =
            0.5f -
            0.5f * cosf(TWO_PI_F * (float)i /
                        (float)(APP_FRAME_SAMPLES - 1U));
        WindowSum += Window[i];
    }

    AnalysisInitialized = 1U;
}

static void fft_radix2(float *real, float *imag)
{
    uint32_t i;
    uint32_t j = 0U;
    uint32_t length;

    for (i = 1U; i < APP_FRAME_SAMPLES; i++) {
        uint32_t bit = APP_FRAME_SAMPLES >> 1U;

        while ((j & bit) != 0U) {
            j ^= bit;
            bit >>= 1U;
        }
        j ^= bit;

        if (i < j) {
            float temporary = real[i];
            real[i] = real[j];
            real[j] = temporary;

            temporary = imag[i];
            imag[i] = imag[j];
            imag[j] = temporary;
        }
    }

    for (length = 2U;
         length <= APP_FRAME_SAMPLES;
         length <<= 1U) {
        uint32_t half_length = length >> 1U;
        float angle = -TWO_PI_F / (float)length;
        float step_real = cosf(angle);
        float step_imag = sinf(angle);
        uint32_t block;

        for (block = 0U;
             block < APP_FRAME_SAMPLES;
             block += length) {
            float twiddle_real = 1.0f;
            float twiddle_imag = 0.0f;
            uint32_t offset;

            for (offset = 0U; offset < half_length; offset++) {
                uint32_t even_index = block + offset;
                uint32_t odd_index = even_index + half_length;
                float odd_real =
                    real[odd_index] * twiddle_real -
                    imag[odd_index] * twiddle_imag;
                float odd_imag =
                    real[odd_index] * twiddle_imag +
                    imag[odd_index] * twiddle_real;
                float even_real = real[even_index];
                float even_imag = imag[even_index];
                float next_twiddle_real;

                real[even_index] = even_real + odd_real;
                imag[even_index] = even_imag + odd_imag;
                real[odd_index] = even_real - odd_real;
                imag[odd_index] = even_imag - odd_imag;

                next_twiddle_real =
                    twiddle_real * step_real -
                    twiddle_imag * step_imag;
                twiddle_imag =
                    twiddle_real * step_imag +
                    twiddle_imag * step_real;
                twiddle_real = next_twiddle_real;
            }
        }
    }
}

static float refine_peak_bin(uint32_t bin)
{
    float left = logf(SpectrumPower[bin - 1U] + 1.0e-30f);
    float center = logf(SpectrumPower[bin] + 1.0e-30f);
    float right = logf(SpectrumPower[bin + 1U] + 1.0e-30f);
    float denominator = left - 2.0f * center + right;
    float delta = 0.0f;

    if (fabsf(denominator) > 1.0e-20f) {
        delta = 0.5f * (left - right) / denominator;
    }

    return (float)bin + clamp_float(delta, -0.5f, 0.5f);
}

static void add_peak_candidate(PeakCandidate *peaks,
                               uint32_t *peak_count,
                               uint32_t bin)
{
    PeakCandidate candidate;

    candidate.bin = bin;
    candidate.refined_bin = refine_peak_bin(bin);
    candidate.power = SpectrumPower[bin];

    if (*peak_count < APP_MAX_PEAK_CANDIDATES) {
        peaks[*peak_count] = candidate;
        (*peak_count)++;
    }
    else {
        uint32_t weakest = 0U;
        uint32_t i;

        for (i = 1U; i < *peak_count; i++) {
            if (peaks[i].power < peaks[weakest].power) {
                weakest = i;
            }
        }

        if (candidate.power > peaks[weakest].power) {
            peaks[weakest] = candidate;
        }
    }
}

static uint32_t find_spectral_peaks(PeakCandidate *peaks,
                                    float *maximum_power,
                                    float *median_noise_power)
{
    uint32_t minimum_bin =
        (uint32_t)ceilf(APP_ANALYSIS_MIN_HZ / APP_FFT_BIN_HZ);
    uint32_t maximum_bin =
        (uint32_t)floorf(APP_ANALYSIS_MAX_HZ / APP_FFT_BIN_HZ);
    uint32_t maximum_power_bin = minimum_bin;
    uint32_t noise_count = 0U;
    uint32_t peak_count = 0U;
    uint32_t bin;
    float threshold;

    for (bin = 0U; bin <= FFT_HALF_SIZE; bin++) {
        float real = FftReal[bin];
        float imag = FftImag[bin];

        SpectrumPower[bin] = real * real + imag * imag;
    }

    *maximum_power = 0.0f;
    for (bin = minimum_bin; bin <= maximum_bin; bin++) {
        NoiseWork[noise_count++] = SpectrumPower[bin];

        if (SpectrumPower[bin] > *maximum_power) {
            *maximum_power = SpectrumPower[bin];
            maximum_power_bin = bin;
        }
    }

    qsort(NoiseWork,
          noise_count,
          sizeof(NoiseWork[0]),
          compare_float);
    *median_noise_power = NoiseWork[noise_count / 2U];

    threshold = maximum_float(*median_noise_power * 25.0f,
                              *maximum_power * 1.0e-8f);

    for (bin = minimum_bin + 1U; bin < maximum_bin; bin++) {
        if (SpectrumPower[bin] >= threshold &&
            SpectrumPower[bin] > SpectrumPower[bin - 1U] &&
            SpectrumPower[bin] >= SpectrumPower[bin + 1U]) {
            add_peak_candidate(peaks, &peak_count, bin);
        }
    }

    {
        uint32_t maximum_present = 0U;
        uint32_t peak_index;

        for (peak_index = 0U;
             peak_index < peak_count;
             peak_index++) {
            if (peaks[peak_index].bin == maximum_power_bin) {
                maximum_present = 1U;
                break;
            }
        }

        if (maximum_present == 0U) {
            add_peak_candidate(peaks,
                               &peak_count,
                               maximum_power_bin);
        }
    }

    if (peak_count == 0U) {
        add_peak_candidate(peaks, &peak_count, maximum_power_bin);
    }

    qsort(peaks,
          peak_count,
          sizeof(peaks[0]),
          compare_peak_frequency);
    return peak_count;
}

static float choose_fundamental(const PeakCandidate *peaks,
                                uint32_t peak_count,
                                float maximum_power)
{
    float total_power = 0.0f;
    float selected_frequency = 0.0f;
    uint32_t selected_index = 0U;
    uint32_t candidate_index;

    for (candidate_index = 0U;
         candidate_index < peak_count;
         candidate_index++) {
        total_power += peaks[candidate_index].power;
        if (peaks[candidate_index].power >
            peaks[selected_index].power) {
            selected_index = candidate_index;
        }
    }

    /*
     * Select the lowest significant peak whose integer harmonics account for
     * most of the detected spectral energy.  This avoids assuming that the
     * largest peak is always the fundamental.
     */
    for (candidate_index = 0U;
         candidate_index < peak_count;
         candidate_index++) {
        float candidate_frequency =
            peaks[candidate_index].refined_bin * APP_FFT_BIN_HZ;
        float matched_power = 0.0f;
        uint32_t peak_index;

        if (peaks[candidate_index].power <
            maximum_power * FUNDAMENTAL_MIN_POWER) {
            continue;
        }

        for (peak_index = 0U;
             peak_index < peak_count;
             peak_index++) {
            float peak_frequency =
                peaks[peak_index].refined_bin * APP_FFT_BIN_HZ;
            float ratio = peak_frequency / candidate_frequency;
            uint32_t harmonic = (uint32_t)floorf(ratio + 0.5f);
            float expected_frequency =
                candidate_frequency * (float)harmonic;
            float tolerance_hz = 1.5f * APP_FFT_BIN_HZ;

            if (harmonic >= 1U &&
                harmonic <= APP_MAX_COMPONENTS &&
                fabsf(peak_frequency - expected_frequency) <=
                    tolerance_hz) {
                matched_power += peaks[peak_index].power;
            }
        }

        if (total_power > 0.0f &&
            matched_power / total_power >= FUNDAMENTAL_COVERAGE) {
            selected_index = candidate_index;
            break;
        }
    }

    selected_frequency =
        peaks[selected_index].refined_bin * APP_FFT_BIN_HZ;

    /*
     * Improve f0 by averaging matched peak_frequency/harmonic estimates.
     */
    {
        float weighted_frequency = 0.0f;
        float weight_sum = 0.0f;
        uint32_t peak_index;

        for (peak_index = 0U;
             peak_index < peak_count;
             peak_index++) {
            float peak_frequency =
                peaks[peak_index].refined_bin * APP_FFT_BIN_HZ;
            float ratio = peak_frequency / selected_frequency;
            uint32_t harmonic = (uint32_t)floorf(ratio + 0.5f);

            if (harmonic >= 1U &&
                harmonic <= APP_MAX_COMPONENTS &&
                fabsf(peak_frequency -
                      selected_frequency * (float)harmonic) <=
                    1.5f * APP_FFT_BIN_HZ) {
                weighted_frequency +=
                    (peak_frequency / (float)harmonic) *
                    peaks[peak_index].power;
                weight_sum += peaks[peak_index].power;
            }
        }

        if (weight_sum > 0.0f) {
            selected_frequency = weighted_frequency / weight_sum;
        }
    }

    return selected_frequency;
}

static int solve_three_by_three(float matrix[3][4], float solution[3])
{
    uint32_t column;

    for (column = 0U; column < 3U; column++) {
        uint32_t pivot = column;
        uint32_t row;

        for (row = column + 1U; row < 3U; row++) {
            if (fabsf(matrix[row][column]) >
                fabsf(matrix[pivot][column])) {
                pivot = row;
            }
        }

        if (fabsf(matrix[pivot][column]) < 1.0e-12f) {
            return -1;
        }

        if (pivot != column) {
            uint32_t item;

            for (item = column; item < 4U; item++) {
                float temporary = matrix[column][item];
                matrix[column][item] = matrix[pivot][item];
                matrix[pivot][item] = temporary;
            }
        }

        {
            float divisor = matrix[column][column];
            uint32_t item;

            for (item = column; item < 4U; item++) {
                matrix[column][item] /= divisor;
            }
        }

        for (row = 0U; row < 3U; row++) {
            float factor;
            uint32_t item;

            if (row == column) {
                continue;
            }

            factor = matrix[row][column];
            for (item = column; item < 4U; item++) {
                matrix[row][item] -=
                    factor * matrix[column][item];
            }
        }
    }

    solution[0] = matrix[0][3];
    solution[1] = matrix[1][3];
    solution[2] = matrix[2][3];
    return 0;
}

static int fit_tone(float frequency_hz,
                    float *amplitude_peak,
                    float *phase_rad)
{
    float angle_step = TWO_PI_F * frequency_hz / APP_SAMPLE_RATE_HZ;
    float step_cos = cosf(angle_step);
    float step_sin = sinf(angle_step);
    float cosine = 1.0f;
    float sine = 0.0f;
    float sum_y = 0.0f;
    float sum_c = 0.0f;
    float sum_s = 0.0f;
    float sum_cc = 0.0f;
    float sum_ss = 0.0f;
    float sum_cs = 0.0f;
    float sum_yc = 0.0f;
    float sum_ys = 0.0f;
    float matrix[3][4];
    float solution[3];
    uint32_t i;

    for (i = 0U; i < APP_FRAME_SAMPLES; i++) {
        float y = TimeSignal[i];
        float next_cosine;

        sum_y += y;
        sum_c += cosine;
        sum_s += sine;
        sum_cc += cosine * cosine;
        sum_ss += sine * sine;
        sum_cs += cosine * sine;
        sum_yc += y * cosine;
        sum_ys += y * sine;

        next_cosine = cosine * step_cos - sine * step_sin;
        sine = cosine * step_sin + sine * step_cos;
        cosine = next_cosine;

        if ((i & 1023U) == 1023U) {
            float norm =
                1.0f / sqrtf(cosine * cosine + sine * sine);
            cosine *= norm;
            sine *= norm;
        }
    }

    matrix[0][0] = (float)APP_FRAME_SAMPLES;
    matrix[0][1] = sum_c;
    matrix[0][2] = sum_s;
    matrix[0][3] = sum_y;
    matrix[1][0] = sum_c;
    matrix[1][1] = sum_cc;
    matrix[1][2] = sum_cs;
    matrix[1][3] = sum_yc;
    matrix[2][0] = sum_s;
    matrix[2][1] = sum_cs;
    matrix[2][2] = sum_ss;
    matrix[2][3] = sum_ys;

    if (solve_three_by_three(matrix, solution) != 0) {
        return -1;
    }

    *amplitude_peak =
        sqrtf(solution[1] * solution[1] +
              solution[2] * solution[2]);
    *phase_rad = atan2f(-solution[2], solution[1]);
    return 0;
}

static void build_reconstructed_waveform(MeasurementResult *result)
{
    uint32_t point;
    float minimum = result->dc_volts;
    float maximum = result->dc_volts;

    for (point = 0U; point < APP_WAVEFORM_POINTS; point++) {
        result->waveform_one_period[point] = result->dc_volts;
    }

    {
        uint32_t component_index;

        for (component_index = 0U;
             component_index < result->component_count;
             component_index++) {
            const SignalComponent *component =
                &result->components[component_index];
            float phase_step =
                TWO_PI_F * (float)component->harmonic /
                (float)APP_WAVEFORM_POINTS;
            float oscillator_cos = cosf(component->phase_rad);
            float oscillator_sin = sinf(component->phase_rad);
            float step_cos = cosf(phase_step);
            float step_sin = sinf(phase_step);

            for (point = 0U;
                 point < APP_WAVEFORM_POINTS;
                 point++) {
                float next_cos;

                result->waveform_one_period[point] +=
                    component->amplitude_peak_v * oscillator_cos;

                next_cos =
                    oscillator_cos * step_cos -
                    oscillator_sin * step_sin;
                oscillator_sin =
                    oscillator_cos * step_sin +
                    oscillator_sin * step_cos;
                oscillator_cos = next_cos;
            }
        }
    }

    minimum = result->waveform_one_period[0];
    maximum = result->waveform_one_period[0];
    for (point = 1U; point < APP_WAVEFORM_POINTS; point++) {
        float value = result->waveform_one_period[point];

        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
    }

    result->peak_to_peak_volts = maximum - minimum;
}

int signal_analyze(const uint16_t *adc_codes,
                   const Calibration *calibration,
                   MeasurementResult *result)
{
    PeakCandidate peaks[APP_MAX_PEAK_CANDIDATES];
    float maximum_power;
    float median_noise_power;
    float fundamental_peak;
    float component_limit;
    float sum;
    float sum_square;
    float minimum_voltage;
    float maximum_voltage;
    uint32_t peak_count;
    uint32_t i;

    if (adc_codes == NULL ||
        calibration == NULL ||
        result == NULL) {
        return -1;
    }

    if (AnalysisInitialized == 0U) {
        signal_analysis_init();
    }

    memset(result, 0, sizeof(*result));
    result->minimum_code = 0xffffU;
    result->maximum_code = 0U;
    if (calibration->is_calibrated == 0U) {
        result->flags |= MEASUREMENT_FLAG_UNCALIBRATED;
    }

    sum = 0.0f;
    minimum_voltage =
        calibration_code_to_volts(calibration, adc_codes[0]);
    maximum_voltage = minimum_voltage;

    for (i = 0U; i < APP_FRAME_SAMPLES; i++) {
        uint16_t code = adc_codes[i] & APP_ADC_CODE_MASK;
        float voltage =
            calibration_code_to_volts(calibration, code);

        TimeSignal[i] = voltage;
        sum += voltage;

        if (code < result->minimum_code) {
            result->minimum_code = code;
        }
        if (code > result->maximum_code) {
            result->maximum_code = code;
        }
        if (code <= APP_ADC_CLIP_LOW_CODE ||
            code >= APP_ADC_CLIP_HIGH_CODE) {
            result->clipped_sample_count++;
        }
        if (voltage < minimum_voltage) {
            minimum_voltage = voltage;
        }
        if (voltage > maximum_voltage) {
            maximum_voltage = voltage;
        }
    }

    result->dc_volts = sum / (float)APP_FRAME_SAMPLES;
    result->raw_peak_to_peak_volts =
        maximum_voltage - minimum_voltage;
    if (result->clipped_sample_count != 0U) {
        result->flags |= MEASUREMENT_FLAG_CLIPPED;
    }

    sum_square = 0.0f;
    for (i = 0U; i < APP_FRAME_SAMPLES; i++) {
        TimeSignal[i] -= result->dc_volts;
        sum_square += TimeSignal[i] * TimeSignal[i];
        FftReal[i] = TimeSignal[i] * Window[i];
        FftImag[i] = 0.0f;
    }
    result->raw_rms_ac_volts =
        sqrtf(sum_square / (float)APP_FRAME_SAMPLES);

    fft_radix2(FftReal, FftImag);
    peak_count =
        find_spectral_peaks(peaks,
                            &maximum_power,
                            &median_noise_power);

    fundamental_peak =
        2.0f * sqrtf(maximum_power) / WindowSum;
    if (!isfinite(fundamental_peak) ||
        fundamental_peak < APP_NO_SIGNAL_PEAK_V) {
        result->flags |= MEASUREMENT_FLAG_NO_SIGNAL;
        return 0;
    }

    result->fundamental_hz =
        choose_fundamental(peaks, peak_count, maximum_power);

    if (result->fundamental_hz < APP_ANALYSIS_MIN_HZ ||
        result->fundamental_hz > APP_ANALYSIS_MAX_HZ) {
        result->flags |= MEASUREMENT_FLAG_ANALYSIS_ERROR;
        return -1;
    }

    /*
     * First fit the fundamental so the relative component threshold is
     * based on its calibrated input amplitude.
     */
    {
        float amplitude;
        float phase;

        if (fit_tone(result->fundamental_hz,
                     &amplitude,
                     &phase) != 0) {
            result->flags |= MEASUREMENT_FLAG_ANALYSIS_ERROR;
            return -1;
        }

        amplitude *=
            calibration_amplitude_correction(
                calibration,
                result->fundamental_hz);
        component_limit =
            maximum_float(APP_COMPONENT_MIN_PEAK_V,
                          amplitude *
                          APP_COMPONENT_RELATIVE_LIMIT);
    }

    result->rms_ac_volts = 0.0f;
    for (i = 1U; i <= APP_MAX_COMPONENTS; i++) {
        float frequency =
            result->fundamental_hz * (float)i;
        float amplitude;
        float phase;

        if (frequency > APP_ANALYSIS_MAX_HZ +
                        0.5f * APP_FFT_BIN_HZ) {
            break;
        }

        if (fit_tone(frequency, &amplitude, &phase) != 0) {
            result->flags |= MEASUREMENT_FLAG_ANALYSIS_ERROR;
            return -1;
        }

        amplitude *=
            calibration_amplitude_correction(calibration,
                                             frequency);

        if (i == 1U || amplitude >= component_limit) {
            SignalComponent *component =
                &result->components[result->component_count];

            component->harmonic = i;
            component->frequency_hz = frequency;
            component->amplitude_peak_v = amplitude;
            component->phase_rad = phase;
            result->component_count++;
            result->rms_ac_volts +=
                0.5f * amplitude * amplitude;
        }
    }

    result->rms_ac_volts = sqrtf(result->rms_ac_volts);
    result->rms_total_volts =
        sqrtf(result->rms_ac_volts * result->rms_ac_volts +
              result->dc_volts * result->dc_volts);

    {
        float noise_bin_peak =
            2.0f * sqrtf(median_noise_power) / WindowSum;

        if (noise_bin_peak > 1.0e-12f &&
            result->component_count > 0U) {
            result->snr_db =
                20.0f *
                log10f(result->components[0].amplitude_peak_v /
                       noise_bin_peak);
        }
        else {
            result->snr_db = 120.0f;
        }
    }

    if (result->snr_db < APP_LOW_SNR_DB) {
        result->flags |= MEASUREMENT_FLAG_LOW_SNR;
    }

    build_reconstructed_waveform(result);
    return 0;
}
