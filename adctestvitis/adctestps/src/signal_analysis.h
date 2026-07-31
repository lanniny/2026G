#ifndef SIGNAL_ANALYSIS_H
#define SIGNAL_ANALYSIS_H

#include <stdint.h>

#include "app_config.h"
#include "calibration.h"

#define MEASUREMENT_FLAG_NO_SIGNAL       (1U << 0)
#define MEASUREMENT_FLAG_CLIPPED         (1U << 1)
#define MEASUREMENT_FLAG_LOW_SNR         (1U << 2)
#define MEASUREMENT_FLAG_UNCALIBRATED    (1U << 3)
#define MEASUREMENT_FLAG_ANALYSIS_ERROR  (1U << 4)

typedef struct {
    uint32_t harmonic;
    float frequency_hz;
    float amplitude_peak_v;
    float phase_rad;
} SignalComponent;

typedef struct {
    uint32_t flags;
    uint16_t minimum_code;
    uint16_t maximum_code;
    uint32_t clipped_sample_count;

    float dc_volts;
    float fundamental_hz;
    float peak_to_peak_volts;
    float rms_ac_volts;
    float rms_total_volts;
    float raw_rms_ac_volts;
    float raw_peak_to_peak_volts;
    float snr_db;

    uint32_t component_count;
    SignalComponent components[APP_MAX_COMPONENTS];

    /* One phase-aligned fundamental period for the future display driver. */
    float waveform_one_period[APP_WAVEFORM_POINTS];
} MeasurementResult;

void signal_analysis_init(void);
int signal_analyze(const uint16_t *adc_codes,
                   const Calibration *calibration,
                   MeasurementResult *result);

#endif
