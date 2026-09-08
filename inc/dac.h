#ifndef _DAC
#define _DAC
#include <stdint.h>

#define DAC_N_SAMPLES 32

void dac_init();
void dac_dispatch();
void on_dac(uint32_t *buf, int n);

#endif
