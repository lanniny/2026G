#ifndef APP_PRINT_H
#define APP_PRINT_H

#include <stdint.h>

#include "signal_analysis.h"

void app_print_banner(const Calibration *calibration);
void app_print_measurement(uint32_t frame_number,
                           const MeasurementResult *result);
void app_print_raw_frame(uint32_t frame_number,
                         const uint16_t *samples,
                         uint32_t sample_count);

#endif
