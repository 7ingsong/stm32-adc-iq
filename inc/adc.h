#ifndef _PERIPHERAL
#define _PERIPHERAL
#include <stdint.h>

#define ADC_N_SAMPLES (9*1024)

void adc_init();
void on_adc(uint16_t *, int);
void adc_dispatch();
#endif