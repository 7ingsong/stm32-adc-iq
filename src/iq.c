#include "iq.h"
#include "dac.h"
#include "utils.h"
#include "command.h"
#include "fifo.h"
#include "transport.h"

typedef struct __attribute__((packed)) {
    uint32_t overflow;
    uint32_t tx;
} iq_stream_response_t;

typedef struct __attribute__((packed)) {
    uint32_t free_space;
    uint32_t overflow_dac;
    uint32_t tx_usb_overflow;
    uint32_t rx_usb_overflow;
} iq_stream_tx_response_t;

static iq_stream_tx_response_t resp_iq_stream_tx = {
    .free_space = 0,
    .overflow_dac = 0,
    .tx_usb_overflow = 0,
    .rx_usb_overflow = 0
};

static fifo_t fifo_dac;
static uint8_t fifo_buffer_dac[DAC_N_SAMPLES * 4 * 2 + 1];


[[maybe_unused]]static const uint16_t sine_12bit[DAC_N_SAMPLES] = {
                      2047, 2447, 2831, 3185, 3498, 3750, 3939, 4056, 4095, 4056,
                      3939, 3750, 3495, 3185, 2831, 2447, 2047, 1647, 1263, 909, 
                      599, 344, 155, 38, 0, 38, 155, 344, 599, 909, 1263, 1647};

[[maybe_unused]]static uint8_t half = 0;

[[maybe_unused]] static uint32_t dual_sine_12bit[DAC_N_SAMPLES];


void send_iq_data(const uint8_t* data, uint16_t len) {
    static uint8_t iq_usb_stream_seq = 0;

    int k = len / FRAME_MAX_PAYLOAD;
    for (int i = 0; i < k; i++) {
        command_send(RESP_IQ_DATA, iq_usb_stream_seq++, &data[i * FRAME_MAX_PAYLOAD],
                     FRAME_MAX_PAYLOAD);
    }

    if (len % FRAME_MAX_PAYLOAD) {
        command_send(RESP_IQ_DATA, iq_usb_stream_seq++, &data[k * FRAME_MAX_PAYLOAD],
                     len % FRAME_MAX_PAYLOAD);
    }
}

static int toggle = 0;

void on_adc(uint16_t *buf, int n){
    // led_control(toggle);
    toggle^=1;

    send_iq_data((const uint8_t*)buf, n*2);
}


void on_dac(uint32_t *buf, int n) {
    int size = n * 4;
    int filled_space = fifo_get_filled(&fifo_dac);
    if (filled_space >= size) {
        fifo_read(&fifo_dac, (uint8_t*)buf, size);
    }

    resp_iq_stream_tx.free_space = fifo_get_free_space(&fifo_dac);
    resp_iq_stream_tx.tx_usb_overflow = transport_get_tx_overflow();
    resp_iq_stream_tx.rx_usb_overflow = transport_get_rx_overflow();

    led_control(half);
    half^=1;
}


void iq_init() {
    fifo_init(&fifo_dac, (uint8_t*)fifo_buffer_dac, sizeof(fifo_buffer_dac));

    for (int idx = 0; idx < DAC_N_SAMPLES; idx++) {
        dual_sine_12bit[idx] = (sine_12bit[idx] << 16) + (sine_12bit[idx]);
    }
}

static void handle_ping(const frame_t* frame) {
    uint8_t payload[]={'P','O','N','G'};
    command_send(RESP_ACK, frame->command.seq, payload, sizeof(payload));
}

void command_handler(const frame_t* frame) {
    switch (frame->command.cmd) {
        case CMD_PING:
            handle_ping(frame);
            break;
        case CMD_IQ_STREAM_TX:
            fifo_write(&fifo_dac, frame->payload, frame->command.len);
            resp_iq_stream_tx.overflow_dac = fifo_dac.overflow;
            resp_iq_stream_tx.free_space = fifo_get_free_space(&fifo_dac);
            resp_iq_stream_tx.tx_usb_overflow = transport_get_tx_overflow();
            resp_iq_stream_tx.rx_usb_overflow = transport_get_rx_overflow();
            command_send(RESP_IQ_STREAM_TX, frame->command.seq, (const uint8_t*)&resp_iq_stream_tx, sizeof(resp_iq_stream_tx));
            break;
        default:
            command_send_error(frame->command.seq, ERR_BAD_COMMAND, frame->command.cmd);
            break;
    }
}

void iq_dispatch() {
    command_dispatch(command_handler);
}