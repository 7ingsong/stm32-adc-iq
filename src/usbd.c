#include "usb_lib.h"
#include "usbd.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include <stm32f10x_exti.h>
#include <misc.h>

#define RCC_APB2Periph_ALLGPIO              (RCC_APB2Periph_GPIOA \
                                              | RCC_APB2Periph_GPIOB \
                                              | RCC_APB2Periph_GPIOC \
                                              | RCC_APB2Periph_GPIOD \
                                              | RCC_APB2Periph_GPIOE )

#define USB_DISCONNECT                      GPIOB  
#define USB_DISCONNECT_PIN                  GPIO_Pin_14
#define RCC_APB2Periph_GPIO_DISCONNECT      RCC_APB2Periph_GPIOB


EXTI_InitTypeDef EXTI_InitStructure;

__IO uint32_t bDeviceState = UNCONNECTED; /* USB device status */
__IO uint32_t EP[8];

struct
{
    __IO RESUME_STATE eState;
    __IO uint8_t bESOFcnt;
}ResumeS;

__IO uint32_t remotewakeupon=0;

RESULT PowerOn(void)
{
    uint16_t wRegVal;

#if !defined (USE_NUCLEO)
    /*** cable plugged-in ? ***/
    USB_Cable_Config(ENABLE);
#endif

    /*** CNTR_PWDN = 0 ***/
    wRegVal = CNTR_FRES;
    _SetCNTR(wRegVal);

    /* The following sequence is recommended:
    1- FRES = 0
    2- Wait until RESET flag = 1 (polling)
    3- clear ISTR register */

    /*** CNTR_FRES = 0 ***/
    wInterrupt_Mask = 0;

    _SetCNTR(wInterrupt_Mask);

    /* Wait until RESET flag = 1 (polling) */
    while((_GetISTR()&ISTR_RESET) == 1);

    /*** Clear pending interrupts ***/
    SetISTR(0);

    /*** Set interrupt mask ***/
    wInterrupt_Mask = CNTR_RESETM | CNTR_SUSPM | CNTR_WKUPM;
    _SetCNTR(wInterrupt_Mask);

    return USB_SUCCESS;
}

RESULT PowerOff()
{
    /* disable all interrupts and force USB reset */
    _SetCNTR(CNTR_FRES);

    /* clear interrupt status register */
    _SetISTR(0);

#if !defined (USE_NUCLEO)
    /* Disable the Pull-Up*/
    USB_Cable_Config(DISABLE);
#endif
    /* switch-off device */
    _SetCNTR(CNTR_FRES + CNTR_PDWN);
    /* sw variables reset */
    /* ... */

    return USB_SUCCESS;
}

void Suspend(void)
{
    uint32_t i =0;
    uint16_t wCNTR;

    /* suspend preparation */
    /* ... */

    /*Store CNTR value */
    wCNTR = _GetCNTR();

    /* This a sequence to apply a force RESET to handle a robustness case */

    /*Store endpoints registers status */
    for (i=0;i<8;i++) EP[i] = _GetENDPOINT(i);

    /* unmask RESET flag */
    wCNTR|=CNTR_RESETM;
    _SetCNTR(wCNTR);

    /*apply FRES */
    wCNTR|=CNTR_FRES;
    _SetCNTR(wCNTR);

    /*clear FRES*/
    wCNTR&=~CNTR_FRES;
    _SetCNTR(wCNTR);

    /*poll for RESET flag in ISTR*/
    while((_GetISTR()&ISTR_RESET) == 0);

    /* clear RESET flag in ISTR */
    _SetISTR((uint16_t)CLR_RESET);

    /*restore Enpoints*/
    for (i=0;i<8;i++)
        _SetENDPOINT(i, EP[i]);

    /* Now it is safe to enter macrocell in suspend mode */
    wCNTR |= CNTR_FSUSP;
    _SetCNTR(wCNTR);

    /* force low-power mode in the macrocell */
    wCNTR = _GetCNTR();
    wCNTR |= CNTR_LPMODE;
    _SetCNTR(wCNTR);
}

void Resume_Init(void)
{
    uint16_t wCNTR;
    /* restart the clocks */
    /* CNTR_LPMODE = 0 */
    wCNTR = _GetCNTR();
    wCNTR &= (~CNTR_LPMODE);
    _SetCNTR(wCNTR);
    /* reset FSUSP bit */
    _SetCNTR(IMR_MSK);
}

void Resume(RESUME_STATE eResumeSetVal)
{
    uint16_t wCNTR;

    if (eResumeSetVal != RESUME_ESOF)
        ResumeS.eState = eResumeSetVal;
    switch (ResumeS.eState)
    {
        case RESUME_EXTERNAL:
        if (remotewakeupon ==0)
        {
            Resume_Init();
            ResumeS.eState = RESUME_OFF;
        }
        else /* RESUME detected during the RemoteWAkeup signalling => keep RemoteWakeup handling*/
        {
            ResumeS.eState = RESUME_ON;
        }
        break;
        case RESUME_INTERNAL:
            Resume_Init();
            ResumeS.eState = RESUME_START;
            remotewakeupon = 1;
        break;
        case RESUME_LATER:
            ResumeS.bESOFcnt = 2;
            ResumeS.eState = RESUME_WAIT;
        break;
        case RESUME_WAIT:
            ResumeS.bESOFcnt--;
            if (ResumeS.bESOFcnt == 0)
                ResumeS.eState = RESUME_START;
        break;
        case RESUME_START:
            wCNTR = _GetCNTR();
            wCNTR |= CNTR_RESUME;
            _SetCNTR(wCNTR);
            ResumeS.eState = RESUME_ON;
            ResumeS.bESOFcnt = 10;
        break;
        case RESUME_ON:
            ResumeS.bESOFcnt--;
            if (ResumeS.bESOFcnt == 0)
            {
                wCNTR = _GetCNTR();
                wCNTR &= (~CNTR_RESUME);
                _SetCNTR(wCNTR);
                ResumeS.eState = RESUME_OFF;
                remotewakeupon = 0;
            }
        break;
        case RESUME_OFF:
        case RESUME_ESOF:
        default:
            ResumeS.eState = RESUME_OFF;
        break;
    }
}

void USB_Cable_Config(FunctionalState NewState)
{
    if (NewState != DISABLE)
    {
        GPIO_ResetBits(USB_DISCONNECT, USB_DISCONNECT_PIN);
    }
    else
    {
        GPIO_SetBits(USB_DISCONNECT, USB_DISCONNECT_PIN);
    }
}

void USB_Interrupts_Config(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = USBWakeUp_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_Init(&NVIC_InitStructure);   
}

void Set_USBClock(void)
{
    RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);
}

void Set_System(void){
    GPIO_InitTypeDef  GPIO_InitStructure;  
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}