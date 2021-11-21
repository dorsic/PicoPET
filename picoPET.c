/*
    PicoPET is a simple time interval counter that outputs number of clk_sys cycles 
    between two pulses and outputs to USB/serial port. It has a resolution of 2 system clock cycles.

    USB and UART output is available but USB works only if clk_sys is above 48 MHz. 
    If below only serial output is working. You may change the output in CMakeLists.txt file.
    Serial output uses default 115200 baud rate.
    
    Options for clocks are:
        - external clock up to 50MHz connected to GPIO20 (pin 26)
        - internal XOSC (12 MHz)
        - internal sys_pll (125 MHz, may be overclocked to >250 MHz based on particular RP2040 chip)
    By default onboard LED blinks each second or 10M external clock cycles.
    It uses 32bit register for the counter. Configured with 10MHz external clock 
    maximum interval between pulses must be less than (2^32 * 2) * 100ns = 859s


    WIRING:
        - ChA input - GPIO5, (INPUT_SIGNALA_GPIO)
        - ChB input - GPIO6, (INPUT_SIGNALB_GPIO)
        - ChC input - GPIO9, (INPUT_SIGNALC_GPIO)
        - ChD input - GPIO13, (INPUT_SIGNALD_GPIO)
        - EXT CLOCK input pin GPIO20 (pin 26) (max. 50 MHz, 3.3V square)
        - Serial output pin GPIO0 (pin 1)

    Version:
    26-May-2021  Marek Dorsic (.md)
    21-Now-2021  Marek Dorsic (.md) - the Hammond enclosure version
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "picoPET.h"
#include "pico/stdlib.h"
#include "pico/stdio_uart.h"
#include "pico/divider.h"
#include "pico/int64_ops.h"
#include "pico/multicore.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/pll.h"
#include "hardware/xosc.h"
#include "hardware/vreg.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "picoPET_sp.pio.h"
#include "picoPET_mp.pio.h"
#include "indicator_led.pio.h"
#include "extClk.h"
#include "counter.h"

// CORE 1 - initialization and monitoring

uint xosc_mhz = XOSC_MHZ;
uint32_t clk_src_freq = XOSC_MHZ * MHZ;
uint div_freq = 1;
struct PetInput inputs[SM_COUNT];

static int uart_gnss_rxstate = 0;
static int gnss_state = -1;        // -1 - unknown, 0 - not fixed, 1 - GNSS FIX
static int ext_clk_state = 0;     // -1 - not connected, 0 - connected, 1 - connected and used
static bool indleds_state = false;       // 0 - LOW, 1 - HIGH (used for blinking with timer)
static char uart_buff[8];


void pins_init() {
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);

    gpio_init(CLKREF_EXT_INDICATOR_GPIO);
    gpio_set_dir(CLKREF_EXT_INDICATOR_GPIO, GPIO_OUT);
    gpio_init(GNSS_LOCKED_INDICATOR_GPIO);
    gpio_set_dir(GNSS_LOCKED_INDICATOR_GPIO, GPIO_OUT);

    gpio_init(INPUT_SIGNALA_GPIO);
    gpio_set_dir(INPUT_SIGNALA_GPIO, GPIO_IN);
    gpio_init(INPUT_SIGNALA_LEDGPIO);
    gpio_set_dir(INPUT_SIGNALA_LEDGPIO, GPIO_OUT);
    gpio_init(INPUT_SIGNALB_GPIO);
    gpio_set_dir(INPUT_SIGNALB_GPIO, GPIO_IN);
    gpio_init(INPUT_SIGNALB_LEDGPIO);
    gpio_set_dir(INPUT_SIGNALB_LEDGPIO, GPIO_OUT);
    gpio_init(INPUT_SIGNALC_GPIO);
    gpio_set_dir(INPUT_SIGNALC_GPIO, GPIO_IN);
    gpio_init(INPUT_SIGNALC_LEDGPIO);
    gpio_set_dir(INPUT_SIGNALC_LEDGPIO, GPIO_OUT);
    gpio_init(INPUT_SIGNALD_GPIO);
    gpio_set_dir(INPUT_SIGNALD_GPIO, GPIO_IN);
    gpio_init(INPUT_SIGNALD_LEDGPIO);
    gpio_set_dir(INPUT_SIGNALD_LEDGPIO, GPIO_OUT);

    uint8_t swio[] = {SW1_GPIO, SW2_GPIO, SW3_GPIO, SW4_GPIO};
    for (uint8_t i=1; i < 4; i++) {
        gpio_init(swio[i]);
        gpio_set_dir(swio[i], GPIO_IN);
        gpio_pull_up(swio[i]);
    }

    gpio_set_function(GNSS_TXGPIO, GPIO_FUNC_UART);
    gpio_set_function(GNSS_RXGPIO, GPIO_FUNC_UART);
}

uint xosc_div_setting() {
    uint8_t s1 = gpio_get(SW2_GPIO);
    uint8_t s2 = gpio_get(SW3_GPIO);
    uint8_t s3 = gpio_get(SW4_GPIO);
    uint8_t s = s1 + (s2 << 1) + (s3 << 2);
    printf("Switch Setting %d (%d%d%d) \n", s, s1, s2, s3);
    switch (s) {
        case DIVF_XOSC:
            return XOSC_MHZ;
        case DIVF_1:
            return 1;
        case DIVF_10:
            return 10;
        case DIVF_100:
            return 100;
        case DIVF_500:
            return 500;
        case DIVF_1k:
            return 1000;
        case DIVF_2k:
            return 2000;
        case DIVF_1M:
            return MHZ;
        default:
            return 12345;
    }
}

void configure_clocks(uint div_freq) {
    uint xosc_divider = xosc_mhz*MHZ/div_freq;

    #if defined CLK_SRC_XOSC
        xosc_init();
        clk_src_freq = XOSC_MHZ*MHZ;  // 12MHz is the internal XOSC
        clock_configure(clk_ref, CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC, 0, clk_src_freq, clk_src_freq);
        clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF, CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, clk_src_freq, clk_src_freq);
        clock_gpio_init(DIVCLK_GPIO, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, xosc_divider);
        clock_configure(clk_adc, 0, CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0, XOSC_MHZ*MHZ, XOSC_MHZ*MHZ);
        pll_deinit(pll_sys);
    #elif defined CLK_SRC_EXT_CLOCK
        // configure the clk_ref to run from pin GPIO20 (pin 26)
        clock_configure_gpin(clk_ref, CLKREF_GNSS_GPIO, clk_src_freq, clk_src_freq);
        clock_configure_gpin(clk_sys, CLKREF_GNSS_GPIO, clk_src_freq, clk_src_freq);
        clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0, clk_src_freq, clk_src_freq);
        clock_configure(clk_adc, 0, CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0, XOSC_MHZ*MHZ, XOSC_MHZ*MHZ);
        clock_gpio_init(DIVCLK_GPIO, CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0, xosc_divider);
        xosc_disable();
        pll_deinit(pll_sys);
    #else
        xosc_init();
        clk_src_freq = SYS_PLL_FREQ;
        set_sys_clock_khz(SYS_PLL_FREQ/1000, true);
        clock_gpio_init(DIVCLK_GPIO, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, xosc_divider);
        clock_configure_gpin(clk_adc, CLKREF_GNSS_GPIO, XOSC_MHZ*MHZ, XOSC_MHZ*MHZ);
//        clock_configure(clk_adc, 0, CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0, XOSC_MHZ*MHZ, XOSC_MHZ*MHZ);
    #endif
}

void countpet_forever(PIO pio, uint sm, uint offset, uint pin) {
    #if AVG_PERIODS == 1
        picopet_sp_program_init(pio, sm, offset, pin);
    #else
        picopet_mp_program_init(pio, sm, offset, pin);
    #endif
    pio_sm_set_enabled(pio, sm, true);
    pio->txf[sm] = AVG_PERIODS-1; 
}

void led_indicate_forever(PIO pio, uint sm, uint offset, uint pin, uint led_pin) {
    indicator_led_program_init(pio, sm, offset, pin, led_pin);
    pio_sm_set_enabled(pio, sm, true);
    pio->txf[sm] = clk_src_freq/20;         // max led blinking frequency will be 10 Hz
}

void configure_pios() {
    uint off[2];
    uint lind[2];
    #if AVG_PERIODS == 1
        for (uint8_t i = 0; i < SM_COUNT; i++) {
            inputs[i].pio = (i % 2 == 0)? pio0: pio1;
            off[i%2] = pio_add_program(inputs[i].pio, &picopet_sp_program);
            lind[i%2] = pio_add_program(inputs[i].pio, &indicator_led_program);
        }
    #else
        for (uint8_t i = 0; i < SM_COUNT; i++) {
            inputs[i].pio = (i % 2 == 0)? pio0: pio1;
            off[i%2] = pio_add_program(inputs[i].pio, &picopet_mp_program);
            lind[i%2] = pio_add_program(inputs[i].pio, &indicator_led_program);
        }
    #endif
    for (uint8_t i = 0; i < SM_COUNT; i++) {
        inputs[i].smc = ((2*i) / 2) - (i%2);
        inputs[i].smi = ((2*i) / 2) - (i%2) + 1;
        countpet_forever(inputs[i].pio, inputs[i].smc, off[i%2], inputs[i].input_gpio);
        led_indicate_forever(inputs[i].pio, inputs[i].smi, lind[i%2], inputs[i].input_gpio, inputs[i].led_gpio);
    }
}

void on_uart_rx() {
    while (uart_is_readable(uart0)) {
        char ch = uart_getc(uart0);
//        printf("%c", ch);
        switch (uart_gnss_rxstate) {
            case 0:            
                    uart_gnss_rxstate = (ch =='$') ? 1: 0;
                    break;
            case 1 ... 5:
                    uart_buff[uart_gnss_rxstate-1] = ch;
                    uart_buff[uart_gnss_rxstate] = '\0';
                    uart_gnss_rxstate++;
                    break;
            case 10 ... 15:
                    uart_gnss_rxstate = (ch ==',') ? uart_gnss_rxstate+1: uart_gnss_rxstate;
                    uart_gnss_rxstate = (ch =='$') ? 1: uart_gnss_rxstate;
                    break;
            case 16:
                    gnss_state = ((uint8_t)ch)-48;
//                    printf("\ngnss_state %u\n", gnss_state);
                    uart_gnss_rxstate = 0;
            default:
                    uart_gnss_rxstate = 0;

        }
        if (uart_gnss_rxstate == 6) {
            if  (strcmp((char*)"GNGGA", uart_buff)==0) {
                uart_gnss_rxstate = 10;   // we are on the start of our $GPGGA sentence
            } else {
                uart_gnss_rxstate = 0;
            }
        }
    }
}

// CORE 1 - monitoring

void switch_time_base(bool external) {
    xosc_mhz = (external)? CLK_EXT_MHZ: XOSC_MHZ;
    clk_src_freq = SYS_PLL_FREQ/XOSC_MHZ * xosc_mhz;
    printf("Switching timebase to %u MHz, \n", xosc_mhz, external);
    set_xosc_freq(xosc_mhz, div_freq);
    printf("Switched timebase to %u MHz, %i \n", xosc_mhz, external);
}

bool update_status_leds_timer_callback(struct repeating_timer *t) {
    indleds_state = !indleds_state;
    bool gnssled = (gnss_state == 1)? 1: (gnss_state == 0)? indleds_state: 0;
    bool extclkled = (ext_clk_state == 1)? 1: (ext_clk_state == 0)? indleds_state: 0;
    gpio_put(GNSS_LOCKED_INDICATOR_GPIO, gnssled);
    gpio_put(CLKREF_EXT_INDICATOR_GPIO, extclkled);
}

void monitor() {
    // configure GNSS serial port for reading
    stdin_uart_init();
    uart_set_baudrate (uart0, GNSS_BAUDRATE);
    uart_set_hw_flow(uart0, false, false);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart0, false);
    irq_set_exclusive_handler(UART0_IRQ, on_uart_rx);
    irq_set_enabled(UART0_IRQ, true);    
    uart_set_irq_enables(uart0, true, false);

    struct repeating_timer timer;
    add_repeating_timer_ms(-100, update_status_leds_timer_callback, NULL, &timer);

    int clk_ext_verify = 0;
    while (true) {
        // check ext ref signal availability
        if (ext_clock_available()) {
            clk_ext_verify ++;
            // do not change state immediately, only after some iterations confirm it
            if (clk_ext_verify >= 10) {
                ext_clk_state = (ext_clk_state == 1)? 1: 0;
                clk_ext_verify = 0;
            }
        } else {
            clk_ext_verify--;
            if (clk_ext_verify <= -10) {
                clk_ext_verify = 0;
                ext_clk_state = -1;
            }
        }

        // do we need to check timebase, because manual switch selection changed?
        int tb = get_timebase();
        if ((tb > 0) && (ext_clk_state >= 0)) {
            // if internal clk used change to ext
            printf("Switch timebase to ext, %u -> 1\n", ext_clk_state);
            switch_time_base(true);
            ext_clk_state = 1;
        } else if (tb < 0) {
            // if ext clk used change to internal
            printf("Switch timebase to int, %u\n", ext_clk_state);
            switch_time_base(false);
            ext_clk_state = -1;
        }
    }
}

// CORE 0 - timemarking/counter
int main() {
    vreg_set_voltage(CORE_VOLTAGE);
    clocks_init();
    stdio_init_all();
    pins_init();
    sleep_ms(2000);
    div_freq = xosc_div_setting();
    printf("Div Freq %d\n", div_freq);
    configure_clocks(div_freq);
    stdio_init_all();
    inputs_init();
    configure_pios();
    sleep_ms(2000);             // wait 1s to allow connecting USB
    printf("Div Freq %d\n", div_freq);


    multicore_launch_core1(monitor);
    do_count();
}
