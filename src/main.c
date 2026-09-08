#include "utils.h"
#include "adc.h"
#include "transport.h"
#include "utils.h"
#include "command.h"
#include "dac.h"

static void handle_ping(const frame_t* frame) {
    uint8_t payload[]={'P','O','N','G'};
    command_send(RESP_ACK, frame->command.seq, payload, sizeof(payload));
}

void command_handler(const frame_t* frame) {
    switch (frame->command.cmd) {
        case CMD_PING:
            handle_ping(frame);
            break;
        default:
            command_send_error(frame->command.seq, ERR_BAD_COMMAND, frame->command.cmd);
            break;
    }
}

int main() {
    clock_init();
    led_init();
    // transport_init();
    
    
    // ADC1_DMA1_Init();
    dac_init();
    
    // led_control(1);
    while (1){
        
        // delay_ms(1000);
    }
    // dac_process();

    // led_control(1);
    // while (1){
    //     command_dispatch(command_handler);
    //     iq_dispatch();
    // }

    return 0;
}
