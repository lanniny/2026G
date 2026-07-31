#include "calibration.h"

#include "app_config.h"

void calibration_load_defaults(Calibration *calibration)
{
    static const float frequency_hz[CALIBRATION_RESPONSE_POINTS] = {
        10000.0f, 50000.0f, 100000.0f, 200000.0f,
        300000.0f, 400000.0f, 500000.0f, 600000.0f
    };
    static const float amplitude_correction[CALIBRATION_RESPONSE_POINTS] = {
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f
    };
    uint32_t i;

    calibration->zero_code = APP_DEFAULT_ADC_ZERO_CODE;
    calibration->input_volts_per_code =
        APP_DEFAULT_ADC_POLARITY *
        APP_DEFAULT_ADC_SPAN_VPP /
        (APP_ADC_CODE_RANGE * APP_DEFAULT_FRONTEND_GAIN);
    calibration->dc_offset_volts = 0.0f;
    calibration->response_count = CALIBRATION_RESPONSE_POINTS;
    calibration->is_calibrated = 1U;

    for (i = 0U; i < CALIBRATION_RESPONSE_POINTS; i++) {
        calibration->response_frequency_hz[i] = frequency_hz[i];
        calibration->response_amplitude_correction[i] =
            amplitude_correction[i];
    }
}

float calibration_code_to_volts(const Calibration *calibration,
                                uint16_t adc_code)
{
    return (((float)adc_code - calibration->zero_code) *
            calibration->input_volts_per_code) -
           calibration->dc_offset_volts;
}

float calibration_amplitude_correction(const Calibration *calibration,
                                       float frequency_hz)
{
    uint32_t i;

    if (calibration->response_count == 0U) {
        return 1.0f;
    }

    if (frequency_hz <= calibration->response_frequency_hz[0]) {
        return calibration->response_amplitude_correction[0];
    }

    for (i = 1U; i < calibration->response_count; i++) {
        float f0 = calibration->response_frequency_hz[i - 1U];
        float f1 = calibration->response_frequency_hz[i];

        if (frequency_hz <= f1) {
            float fraction = (frequency_hz - f0) / (f1 - f0);
            float c0 =
                calibration->response_amplitude_correction[i - 1U];
            float c1 =
                calibration->response_amplitude_correction[i];

            return c0 + fraction * (c1 - c0);
        }
    }

    return calibration->response_amplitude_correction[
        calibration->response_count - 1U];
}
