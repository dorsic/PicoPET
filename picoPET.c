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
        - Serial output pin GPIO0 (pin 1)

    Version:
    26-May-2021  Marek Dorsic (.md)
*/

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/pll.h"
#include "picoPET_sp.pio.h"
#include "picoPET_mp.pio.h"
#include "hardware/xosc.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"


// INPUT CLOCK SOURECE REFERENCE
//#define CLK_SRC_XOSC                  // internal 12MHz TCXO
#define CLK_SRC_XOSC_PLL               // PLL sourced from internal TCXO
//#define CLK_SRC_EXT_CLOCK             // external clock on PIN20, max. 50 MHz, set clk_src_freq below appropriately
//#define CLK_SRC_EXT_CLOCK_PLL           // external clock on XIN with PLL, needs HW modification, max. 25 MHz

// PLL SETTINGS
#define SYS_PLL_FREQ 200*MHZ              // slightly overclocked from default 125 MHz; NOTE not arbitrary freq possible. See RP2040 datasheet
#define CORE_VOLTAGE VREG_VOLTAGE_1_10    // consider higher core voltage for higher PLL, default is 1.1V

// OUTPUT SETTINGS
#define OUTPUT_FREQUENCY
//#define OUTPUT_TIMEMARK
//#define OUTPUT_CYCLE_COUNT

#define AVG_PERIODS 1                   // number of periods to average; has to at least 1, for steady results use odd numbers 1, 3, 5...

#define INPUT_SIGNAL_GPIO 16
#define LED_PIN 25

uint clk_src_freq = 10*MHZ;             // change this only if CLK_SRC_EXT_CLOCK defined

void configure_clocks() {
    #if defined CLK_SRC_XOSC
        xosc_init();
        clk_src_freq = 12000000;  // 12MHz is the internal XOSC
        clock_configure(clk_ref, CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC, 0, clk_src_freq, clk_src_freq);
        clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF, CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, clk_src_freq, clk_src_freq);
//        clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, clk_src_freq, clk_src_freq);
        clock_gpio_init(21, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, 1);    // put the TCXO clock on pin 21
        pll_deinit(pll_sys);
    #elif defined CLK_SRC_EXT_CLOCK
        // configure the clk_ref to run from pin GPIO20 (pin 26)
        clock_configure_gpin(clk_ref, 20, clk_src_freq, clk_src_freq);
        clock_configure_gpin(clk_sys, 20, clk_src_freq, clk_src_freq);
        clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0, clk_src_freq, clk_src_freq);
        clock_gpio_init(21, CLOCKS_CLK_GPOUT1_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0, 1);   // put the external ref. clock on pin 21
        xosc_disable();
        pll_deinit(pll_sys);
    #elif defined CLK_SRC_XOSC_PLL
        xosc_init();
        clk_src_freq = SYS_PLL_FREQ;  // 125 MHz 
        set_sys_clock_khz(SYS_PLL_FREQ/1000, true);
        clock_gpio_init(21, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, 1);
    #else   //CLK_SRC_EXT_CLOCK_PLL
        xosc_init();
        clk_src_freq = SYS_PLL_FREQ;
        set_sys_clock_khz(clk_src_freq/1000, true);
        clock_gpio_init(21, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, 1000);     // put the external ref. clock on pin 21
    #endif
}

void countpet_forever(PIO pio, uint sm, uint offset, uint pin, uint led_pin) {
    #if AVG_PERIODS == 1
        picopet_sp_program_init(pio, sm, offset, pin, led_pin);
    #else
        picopet_mp_program_init(pio, sm, offset, pin, led_pin);
    #endif
    pio_sm_set_enabled(pio, sm, true);
    pio->txf[sm] = AVG_PERIODS-1; 
}

void configure_pios() {
    // program the PIOs
    PIO pio = pio0;
    #if AVG_PERIODS == 1
        uint offset = pio_add_program(pio, &picopet_sp_program);
    #else
        uint offset = pio_add_program(pio, &picopet_mp_program);
    #endif
    countpet_forever(pio, 0, offset, INPUT_SIGNAL_GPIO, LED_PIN);
}

int main() {
    vreg_set_voltage(CORE_VOLTAGE);
    xosc_init();
    clocks_init();
    configure_clocks();
    configure_pios();

    // initialize USB/Serial
    stdio_init_all();
    uint64_t tm = 0;
    while (true) {
        uint32_t clk_cnt = pio_sm_get_blocking(pio0, 0);            // read the register from ASM code
        clk_cnt = ~clk_cnt;                                         // negate the received value
        uint32_t clk_cor = 0;
        // PIO CALIBRATION CORRECTIONS
        #if AVG_PERIODS == 1
//            clk_cnt = (clk_cnt+3)*2;
            clk_cor = (clk_cnt+2)*2;
        #else
            clk_cor = 2*(clk_cnt + 1.5*AVG_PERIODS + 1.5);
        #endif
        #if defined OUTPUT_CYCLE_COUNT
            printf("%u\n", clk_cnt);
        #elif defined OUTPUT_FREQUENCY
//            double fs = ((double)clk_src_freq*(double)AVG_PERIODS)/((double)clk_cnt);
            double fc = ((double)clk_src_freq*(double)AVG_PERIODS)/((double)clk_cor);
//            printf("@%u\t %u\t %u\t %.6f\t %.6f\n", clk_src_freq, clk_cnt, clk_cor, fs, fc);
            printf("%.6f\n", fc);
        #else
            tm = tm + clk_cnt;
            printf("%llu\n", tm);
        #endif

    }
    
}
