#include "utils.h"
#include "adc.h"
#include "transport.h"
#include "utils.h"
#include "dac.h"
#include "iq.h"

int main() {
    
    clock_init();
    led_init();
    transport_init();
    iq_init();
    
    adc_init();
    dac_init();
    
    while (1){
        adc_dispatch();
        dac_dispatch();
        iq_dispatch();
    }

    return 0;
}
