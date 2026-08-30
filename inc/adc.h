#ifndef _PERIPHERAL
#define _PERIPHERAL
#include <stdint.h>

#define ADC_N_SAMPLES (1024)

void ADC1_DMA1_Init();
void OnADC(uint16_t *, int);
void iq_dispatch();
#endif