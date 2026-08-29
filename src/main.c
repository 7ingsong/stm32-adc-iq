#include <stdio.h>
#include <string.h>
#include <math.h>
#include "si5351.h"
#include "i2c.h"
#include "ssd1306.h"
#include "ec11.h"
#include "utils.h"
#include "flash.h"
#include "adc.h"
#include "usb_istr.h"
#include "usb_lib.h"
#include "usbd.h"
#include "si5351-ray.h"



#define MAX_FREQ 500000000
#define CH0 0
#define CONFIG_MAGIC 0x12345678
#define BACK_NAME "<--"
#define N_CHANNELS 1

typedef void (*callback_t)(void);

typedef struct {
    int frequency;
    int strength;
    int power;
} Channel;

typedef struct{
    uint32_t magic;
    Channel channels[N_CHANNELS];
} Config;

typedef struct MenuItem {
    const char *name;
    struct MenuItem *parent;
    struct MenuItem *children;
    uint8_t num_children;
    void (*action)(void); // NULL if submenu
    void (*render)(void);
} MenuItem;

void si5351Restart();
void configWrite();
void drawMenu();


#define MAX_DIGIT_INDEX 9
int digit_index = 6;

Channel channels[N_CHANNELS]={
    {.frequency = 3000000, .power = SI5351_DRIVE_STRENGTH_8MA, .power = 0},
};

void actionSelectFreqBack(){
    si5351Restart();
}

void drawMenu(){
    int offset = 0;
    char buf[50];
    sprintf(buf, "%3.3d.%3.3d.%3.3d", channels[CH0].frequency/1000000,(channels[CH0].frequency/1000)%1000, channels[CH0].frequency%1000);
    
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString(buf, Font_11x18, White);
    if (digit_index>2){
        offset+=11;
    }

    if (digit_index>5){
        offset+=11;
    }

    ssd1306_SetCursor(((MAX_DIGIT_INDEX+1)*11)-(digit_index*11+offset), 20);
    ssd1306_WriteString("^", Font_11x18, White);
    ssd1306_UpdateScreen();
    si5351Restart();
}

void inputHandler(){
    if (!direction && !button){
        return ;
    }

    ssd1306_Fill(Black);

    if (direction){
        channels[CH0].frequency-=direction*pow(10,digit_index);
        if (channels[CH0].frequency>MAX_FREQ){
            channels[CH0].frequency = MAX_FREQ;
        }

        if (channels[CH0].frequency<0){
            channels[CH0].frequency = 0;
        }

    }

    if (button){
        digit_index=(digit_index+1)%MAX_DIGIT_INDEX;
    }

    drawMenu();

    direction = 0;
    button = 0;
}


void configWrite(){
    Config config;
    config.magic = CONFIG_MAGIC;
    memcpy(config.channels, channels, sizeof(channels));
    Flash_Write(&config, sizeof(config));
}

int configRead(){
    Config config;
    Flash_Read(&config, sizeof(config));
    if (config.magic == CONFIG_MAGIC){
        memcpy(channels, config.channels, sizeof(channels));
        return 1;
    }
    return 0;
}

void si5351Restart2(){
    si5351aSetFrequency(channels[CH0].frequency);
}

void si5351Restart(){
    const int32_t correction = 978;
    si5351_Init(correction);

    si5351PLLConfig_t pll_conf;
    si5351OutputConfig_t out_conf;
    int32_t Fclk = channels[CH0].frequency;

    si5351_CalcIQ(Fclk, &pll_conf, &out_conf);

    /*
    * `phaseOffset` is a 7bit value, calculated from Fpll, Fclk and desired phase shift.
    * To get N° phase shift the value should be round( (N/360)*(4*Fpll/Fclk) )
    * Two channels should use the same PLL to make it work. There are other restrictions.
    * Please see AN619 for more details.
    *
    * si5351_CalcIQ() chooses PLL and MS parameters so that:
    *   Fclk in [1.4..100] MHz
    *   out_conf.div in [9..127]
    *   out_conf.num = 0
    *   out_conf.denum = 1
    *   Fpll = out_conf.div * Fclk.
    * This automatically gives 90° phase shift between two channels if you pass
    * 0 and out_conf.div as a phaseOffset for these channels.
    */
    uint8_t phaseOffset = (uint8_t)out_conf.div;
    si5351_SetupOutput(0, SI5351_PLL_A, SI5351_DRIVE_STRENGTH_2MA, &out_conf, 0);
    si5351_SetupOutput(1, SI5351_PLL_A, SI5351_DRIVE_STRENGTH_2MA, &out_conf, phaseOffset);

    /*
    * The order is important! Setup the channels first, then setup the PLL.
    * Alternatively you could reset the PLL after setting up PLL and channels.
    * However since _SetupPLL() always resets the PLL this would only cause
    * sending extra I2C commands.
    */
    si5351_SetupPLL(SI5351_PLL_A, &pll_conf);
    si5351_SetupPLL(SI5351_PLL_A, &pll_conf);
    si5351_EnableOutputs((1<<0) | (1<<1));
}

uint16_t *usb_samples;
int n_tx=0;
int n_index=0;
int in_progress = 0;
int configured =0;
uint16_t usb_data[USB_N_SAMPLES+1];


void SendAdcToUsb(){
    
    if (configured == 0){
        return ;
    }

    if (in_progress==1){
        return ;
    }

    int n=0;
    for (int i=0;i<USB_N_SAMPLES;i++){
        if (!RingBuffer_Read(&usb_data[i])){
            break;
        }
        n++;
    }
    if (n>0){
        in_progress = 1;
        CDC_Send_DATA ((unsigned char*)usb_data, n*2);
    }
}


/*
void SendAdcToUsb2(){
    if (n_index<n_tx){
        usb_buf[0] = 0xAA;
        memcpy(&usb_buf[1],(unsigned char*)&usb_samples[n_index], USB_N_SAMPLES*2);
        usb_buf[61] = 0x55;
        //CDC_Send_DATA ((unsigned char*)&usb_samples[n_index], USB_N_SAMPLES*2);
        CDC_Send_DATA ((unsigned char*)&usb_buf[0], 62);
        n_index += USB_N_SAMPLES;
    }
}
*/

void OnADC(uint16_t *buf, int n){
    n_tx = n;
    n_index = 0;
    usb_samples = buf;
    SendAdcToUsb(); // start transmisstion
}

void OnUsbAddressed(){
}

void OnUsbAttached(){
}

void OnUsbTransmitted(){
    in_progress = 0;
    SendAdcToUsb(); // continue transmisstion
}

void OnUsbUnconnected(){
    in_progress = 0;
    configured = 0;
}

void OnUsbConfigured(){
    in_progress = 0;
    configured = 1;
}


int main() {
    configured = 0;
    in_progress = 0;
    clock_init();
    
    ADC1_DMA1_Init();

    Set_System();
    Set_USBClock();
    USB_Interrupts_Config();
    USB_Init();

    while (1){
    }

    return 0;
}
