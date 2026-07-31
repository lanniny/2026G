#ifndef ACQUISITION_H
#define ACQUISITION_H

#include <stdint.h>

#include "xstatus.h"

int acquisition_init(void);
int acquisition_capture(uint16_t *samples, uint32_t sample_count);
int acquisition_recover(void);
uint32_t acquisition_pl_status(void);
uint32_t acquisition_dma_status(void);
void acquisition_print_diagnostics(const char *prefix);

#endif
