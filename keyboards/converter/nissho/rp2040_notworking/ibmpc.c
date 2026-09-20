#include "ibmpc.h"
#include "ringbuf.h"
#include "quantum.h"
#include "timer.h"
#include "wait.h"
#include "debug.h"

volatile uint8_t ibmpc_error = IBMPC_ERR_NONE;
volatile uint8_t ibmpc_isr_debug = 0;
uint8_t ibmpc_protocol = IBMPC_PROTOCOL_AT;

static ringbuf_t rb;

void ibmpc_host_enable(void) {
    // Configure pins
    setPinInputHigh(IBMPC_CLOCK_PIN);
    setPinInputHigh(IBMPC_DATA_PIN);
    setPinInputHigh(IBMPC_RESET_PIN);
    rb.head = 0;
    rb.tail = 0;
    ibmpc_error = IBMPC_ERR_NONE;
    ibmpc_isr_debug = 0;
}

void ibmpc_host_disable(void) {
    // Disable communication lines
    clock_lo();
    data_hi();
}

void ibmpc_host_isr_clear(void) {
    rb.head = 0;
    rb.tail = 0;
}

uint8_t ibmpc_host_recv(void) {
    if (ringbuf_is_empty(&rb)) {
        return (uint8_t)-1;
    }

    uint8_t data = ringbuf_dequeue(&rb);
    return data;
}

uint8_t ibmpc_host_send(uint8_t data) {
    // Simulate transmission (no real keyboard response expected here)
    // Should be adapted for actual PS/2 host write support if needed
    return 0;
}

void ibmpc_host_set_led(uint8_t led) {
    // Typical IBM PC-compatible keyboard LED command
    ibmpc_host_send(0xED);
    wait_us(500);  // wait for ACK or device to be ready
    ibmpc_host_send(led);
}

extern ringbuf_t rb;

void ibmpc_host_init(void) {
    ringbuf_init(&rb);
/*
    setPinInputHigh(IBMPC_CLOCK_PIN);  // idle high with pull-up
    setPinInputHigh(IBMPC_DATA_PIN);
    setPinOutput(IBMPC_RESET_PIN);     // set reset to output (low later)
    writePinHigh(IBMPC_RESET_PIN);
 */
}
