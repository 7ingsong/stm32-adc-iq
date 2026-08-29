#ifndef _PERIPHERAL
#define _PERIPHERAL
#include <stdint.h>


#define USB_N_SAMPLES (2*15)

#define RING_N_SAMPLES (USB_N_SAMPLES*10*100)
#define ADC_N_SAMPLES (USB_N_SAMPLES*10)

int RingBuffer_Read(uint16_t *data);
void RingBuffer_Write(uint16_t data);
void ADC1_DMA1_Init();
void OnADC(uint16_t *, int);

#endif