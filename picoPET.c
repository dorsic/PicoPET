/*
    PicoPET is a simple time interval counter that outputs number of clk_sys cycles 
    between two pulses on GPIO16 (pin 21) to USB/serial port. It has a resolution of 2 clock cycles.

    USB output can be configured and works only if clk_sys is above 48 MHz. 
    If below only serial output is working. You may change the output in CMakeLists.txt file.
    Serial output uses 115200 baud rate.
    
    Options for clocks are:
        - external clock up to 50MHz connected to GPIO20 (pin 26)
        - internal XOSC (12 MHz)
        - internal sys_pll (125 MHz, may be overclocked to >250 MHz based on particular RP2040 chip)
    By default onboard LED blinks each second or 10M external clock cycles.
    It uses 32bit register for the counter. Configured with 10MHz external clock 
    maximum interval between pulses must be less than (2^32 * 2) * 100ns = 859s


    WIRING:
        - INPUT pulse pin GPIO16 (pin 21, max 3.3V)
        - EXT CLOCK input pin GPIO20 (pin 26) (max. 50 MHz, 3.3V square)
        - Serial output pin GPIO1 (pin 2)

    Version:
    26-May-2021  Marek Dorsic (.md)
*/

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/pll.h"
#include "picoDIV.pio.h"
#include "picoPET.pio.h"
#include "hardware/xosc.h"
//#include <tusb.h>

#define CLK_SRC_EXT_CLOCK 1
#define CLK_SRC_XOSC 2
#define CLK_SRC_SYS_PLL_125M 3
#define CLK_SRC_EXT_CLOCK_PLL_125M 4

uint clock_source = CLK_SRC_EXT_CLOCK_PLL_125M;

uint clock_freq = 10000000; // 10 MHz; change this if using external clock with differenct frequency
uint pulse_len = 100000;    // number of clock cycles of the output pulse length
                            // 100000 cycles * 100ns (for 10MHz) = 10ms

void configure_clocks() {
    if (clock_source == CLK_SRC_XOSC) {
        xosc_init();
        clock_freq = 12000000;  // 12MHz is the internal XOSC
        pulse_len = 120000;     // 10ms pulse
        clock_configure(clk_ref, CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC, 0, clock_freq, clock_freq);
        clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF, CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, clock_freq, clock_freq);
        clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, clock_freq, clock_freq);
        clock_gpio_init(21, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, 1);
        pll_deinit(pll_sys);
    } else if (clock_source == CLK_SRC_EXT_CLOCK) {
        // configure the clk_ref to run from pin GPIO20 (pin 26)
        clock_configure_gpin(clk_ref, 20, clock_freq, clock_freq);
        clock_configure_gpin(clk_sys, 20, clock_freq, clock_freq);
        clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0, clock_freq, clock_freq);
        clock_gpio_init(21, CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0, 1);    
        xosc_disable();
        pll_deinit(pll_sys);
    } else if (clock_source == CLK_SRC_EXT_CLOCK_PLL_125M) {
        set_sys_clock_khz(125000, true);
        clock_configure_gpin(clk_ref, 20, clock_freq, clock_freq);      // clock_freq is set at the beginning of the script        
        clock_freq = 125 * MHZ;  // 125 MHz 
    } else {
        xosc_init();
        clock_freq = 125000000;  // 125 MHz
        pulse_len = 1250000;     // 10ms pulse
        set_sys_clock_khz(125000, true);
    }
}

void countpet_forever(PIO pio, uint sm, uint offset) {
    picopet_program_init(pio, sm, offset);
    pio_sm_set_enabled(pio, sm, true);
}

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint total_clk, uint pulse_clk) {
    picodiv_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);
    
    pio->txf[sm] = pulse_clk - 3;                  // write number of HIGH clock cycles to PIO register
    pio->txf[sm] = total_clk - pulse_clk - 3;      // write number off LOW clock cycles to PIO register
}

void configure_pios() {
    // program the PIOs
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &picopet_program);
    countpet_forever(pio, 0, offset);

    pio = pio1;
    offset = pio_add_program(pio, &picodiv_program);
    blink_pin_forever(pio, 1, offset, 25, clock_freq, pulse_len);       // 1 PPS output on GPIO25 (LED)
}

int main() {
    configure_clocks();
    configure_pios();

    // initialize USB/Serial
    stdio_init_all();

    while (true) {
        uint32_t clk_cnt = pio_sm_get_blocking(pio0, 0);            // read the register from ASM code
        printf("%u\n", (clk_cnt+3)*2);                              // write output to USB/serial
    }
    
}
