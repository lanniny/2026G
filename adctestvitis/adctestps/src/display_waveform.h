#ifndef DISPLAY_WAVEFORM_H
#define DISPLAY_WAVEFORM_H

#include <stdint.h>

#include "signal_analysis.h"

#define DISPLAY_WAVEFORM_POINT_COUNT 506U

/* Build one or three stable periods as TJC waveform-component byte codes. */
int display_waveform_make(
    const MeasurementResult *result,
    uint8_t period_count,
    uint8_t output[DISPLAY_WAVEFORM_POINT_COUNT]);

#endif
