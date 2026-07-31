#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdint.h>

#define CALIBRATION_RESPONSE_POINTS 8U

typedef struct {
    float zero_code;
    float input_volts_per_code;
    float dc_offset_volts;
    uint32_t response_count;
    float response_frequency_hz[CALIBRATION_RESPONSE_POINTS];
    float response_amplitude_correction[CALIBRATION_RESPONSE_POINTS];
    uint32_t is_calibrated;
} Calibration;

void calibration_load_defaults(Calibration *calibration);
float calibration_code_to_volts(const Calibration *calibration,
                                uint16_t adc_code);
float calibration_amplitude_correction(const Calibration *calibration,
                                       float frequency_hz);

#endif
