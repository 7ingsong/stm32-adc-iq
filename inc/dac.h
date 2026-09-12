#ifndef _DAC
#define _DAC
#include <stdint.h>

#define DAC_N_SAMPLES 64

void dac_init();
void dac_dispatch();
void on_dac(uint32_t *buf, int n);

#endif
