#include "adc.h"

#include <string.h>
#include <stdio.h>

#include "stm32f10x_conf.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "misc.h"

__attribute__((weak)) void OnADC(uint16_t *, int){

}

static uint16_t samples[ADC_N_SAMPLES*2];

typedef struct {
    volatile uint16_t head;
    volatile uint16_t tail;
    uint16_t buffer[RING_N_SAMPLES];
} RingBuffer;

RingBuffer adc_ring = { .head = 0, .tail = 0 };

void RingBuffer_Write(uint16_t value) {
    RingBuffer *rb = &adc_ring;
    rb->buffer[rb->head] = value;
    rb->head = (rb->head + 1) % RING_N_SAMPLES;

    // Optional: handle overflow by advancing tail
    if (rb->head == rb->tail) {
        rb->tail = (rb->tail + 1) % RING_N_SAMPLES;
    }
}

int RingBuffer_Read(uint16_t *value) {
    RingBuffer *rb = &adc_ring;
    if (rb->head == rb->tail){
        return 0; // Buffer empty
    }

    *value = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RING_N_SAMPLES;
    return 1;
}

void PushSamples(int pos, int n){
    uint16_t *p = &samples[pos];
    for (int i=0;i<n;i++){
        RingBuffer_Write(p[i]);
    }
}

void DMA1_Channel1_IRQHandler(void){
    if (DMA_GetITStatus(DMA1_IT_HT1)) {
        PushSamples(0, ADC_N_SAMPLES);
        OnADC(&samples[0],ADC_N_SAMPLES);
        DMA_ClearITPendingBit(DMA1_IT_HT1);
        
    }

    if (DMA_GetITStatus(DMA1_IT_TC1)) {
        PushSamples(ADC_N_SAMPLES, ADC_N_SAMPLES);
        OnADC(&samples[ADC_N_SAMPLES],ADC_N_SAMPLES);
        DMA_ClearITPendingBit(DMA1_IT_TC1);
    }
}


void ADC1_Init(){
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);



    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 2;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 1, ADC_SampleTime_71Cycles5);//ADC_SampleTime_71Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_5, 2, ADC_SampleTime_71Cycles5);//ADC_SampleTime_71Cycles5);

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

void DMA1_Init(){
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    DMA_InitTypeDef DMA_InitStructure;
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&samples[0];
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = ADC_N_SAMPLES * 2;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    DMA_ITConfig(DMA1_Channel1, DMA_IT_HT | DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA1_Channel1, ENABLE);
}

void NVIC_DMA1_Init(void){
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    NVIC_InitTypeDef nvic;
    nvic.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

void ADC1_DMA1_Init(){
    NVIC_DMA1_Init();
    DMA1_Init();
    ADC1_Init();
}

