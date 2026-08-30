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
#include "command.h"
#include "transport.h"
#include "utils.h"

typedef struct __attribute__((packed)) {
    uint32_t overflow;
    uint32_t tx;
} iq_stream_response_t;

static iq_stream_response_t g_resp = {
    .overflow = 0,
    .tx = 0,
};

typedef struct __attribute__((packed)) {
    uint32_t request_size;
    uint32_t overflow;
    uint32_t tx_usb_overflow;
    uint32_t rx_usb_overflow;
} iq_stream_tx_response_t;


#define IQ_PENDING_HALF0 0x01
#define IQ_PENDING_HALF1 0x02
static uint8_t s_iq_usb_stream_seq = 0;
static volatile uint8_t g_iq_pending_mask_rx = 0;

uint16_t g_adc_samples[ADC_N_SAMPLES*2];

void handle_iq_stream(const frame_t* frame) {
    if (frame->command.len != 1) {
        command_send_error(frame->command.seq, ERR_BAD_PAYLOAD, frame->command.len);
        return;
    }

    if (frame->payload[0] == 0) {
        // iq_stop();
        return;
    } else if (frame->payload[0] == 1) {
        // iq_start();
        return;
    } else if (frame->payload[0] == 2) {
    } else {
        command_send_error(frame->command.seq, ERR_BAD_PAYLOAD, frame->payload[0]);
        return;
    }

    g_resp.overflow = transport_get_tx_overflow();
    command_send(RESP_IQ_STREAM, frame->command.seq, (uint8_t*)&g_resp, sizeof(g_resp));
}


void DMA1_Channel1_IRQHandler(void){
    if (DMA_GetITStatus(DMA1_IT_HT1)) {
        g_iq_pending_mask_rx |= IQ_PENDING_HALF0;
        DMA_ClearITPendingBit(DMA1_IT_HT1);
        
    }

    if (DMA_GetITStatus(DMA1_IT_TC1)) {
        g_iq_pending_mask_rx |= IQ_PENDING_HALF1;
        DMA_ClearITPendingBit(DMA1_IT_TC1);
    }
}

static void send_iq_data(const uint8_t* data, uint16_t len) {
    int k = len / FRAME_MAX_PAYLOAD;
    for (int i = 0; i < k; i++) {
        command_send(RESP_IQ_DATA, s_iq_usb_stream_seq++, &data[i * FRAME_MAX_PAYLOAD],
                     FRAME_MAX_PAYLOAD);
    }

    if (len % FRAME_MAX_PAYLOAD) {
        command_send(RESP_IQ_DATA, s_iq_usb_stream_seq++, &data[k * FRAME_MAX_PAYLOAD],
                     len % FRAME_MAX_PAYLOAD);
    }
}

__attribute__((weak)) void OnADC(uint16_t *buf, int n){
    // __disable_irq();
    send_iq_data((const uint8_t*)buf, n*2);
    // __enable_irq();
}

void iq_dispatch(void) {
    uint8_t mask_rx;

    __disable_irq();
    mask_rx = g_iq_pending_mask_rx;
    g_iq_pending_mask_rx = 0;
    __enable_irq();


    if (mask_rx & IQ_PENDING_HALF0) {
        OnADC(&g_adc_samples[0], ADC_N_SAMPLES);
        led_control(1);
    }

    if (mask_rx & IQ_PENDING_HALF1) {
        OnADC(&g_adc_samples[sizeof(g_adc_samples)/2], ADC_N_SAMPLES);
        led_control(0);
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

    ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 1, ADC_SampleTime_239Cycles5);//ADC_SampleTime_71Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_5, 2, ADC_SampleTime_239Cycles5);//ADC_SampleTime_71Cycles5);

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
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&g_adc_samples[0];
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = sizeof(g_adc_samples);
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

