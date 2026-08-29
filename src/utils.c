#include "utils.h"
#include <stm32f10x_rcc.h>

volatile uint32_t ms_ticks = 0;  // Counter for milliseconds

void SysTick_Handler(void) {
    ms_ticks++;  // Increment counter every millisecond
}


void systick_init(){
    SysTick_Config(SystemCoreClock / 1000);
}

void clock_init(){
    // 72 Mhz
    RCC_DeInit();
    RCC_HSEConfig(RCC_HSE_ON);
    while (RCC_WaitForHSEStartUp() == ERROR);

    RCC_HCLKConfig(RCC_SYSCLK_Div1); /// HCLK = SYSCLK
    RCC_PCLK2Config(RCC_HCLK_Div1); /// PCLK2 = HCLK
    RCC_PCLK1Config(RCC_HCLK_Div2); /// PCLK1 = HCLK/2
    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9); /// PLLCLK = 8MHz * 9 = 72 MHz

    RCC_PLLCmd(ENABLE); // Enable PLL
    while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET); /// Wait till PLL is ready
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while(RCC_GetSYSCLKSource() != 0x08); /// Wait till PLL is used as system clock source  

    systick_init();
}


void delay_ms(uint32_t ms) {
    uint32_t target = ms_ticks + ms;
    while (ms_ticks < target);
}

uint32_t get_ticks_ms(){
    return ms_ticks;
}
