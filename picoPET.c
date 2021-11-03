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
#include "pico/divider.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/pll.h"
#include "hardware/xosc.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "picoPET_sp.pio.h"
#include "picoPET_mp.pio.h"
#include "indicator_led.pio.h"


// INPUT CLOCK SOURECE REFERENCE
//#define CLK_SRC_XOSC                  // internal 12MHz TCXO
#define CLK_SRC_XOSC_PLL               // PLL sourced from internal TCXO
//#define CLK_SRC_EXT_CLOCK             // external clock on PIN20, max. 50 MHz, set clk_src_freq below appropriately
//#define CLK_SRC_EXT_CLOCK_PLL           // external clock on XIN with PLL, needs HW modification, max. 25 MHz

// PLL SETTINGS
#define SYS_PLL_FREQ 200*MHZ              // slightly overclocked from default 125 MHz; NOTE not arbitrary freq possible. See RP2040 datasheet
#define CORE_VOLTAGE VREG_VOLTAGE_1_20    // consider higher core voltage for higher PLL, default is 1.1V

// OUTPUT SETTINGS
//#define OUTPUT_FREQUENCY
#define OUTPUT_TIMEMARK
//#define OUTPUT_CYCLE_COUNT

#define AVG_PERIODS 1                   // number of periods to average; has to at least 1, for steady results use odd numbers 1, 3, 5...

#define SM_COUNT 4
#define INPUT_SIGNALA_GPIO 4
#define INPUT_SIGNALA_LEDGPIO 5
#define INPUT_SIGNALB_GPIO 6
#define INPUT_SIGNALB_LEDGPIO 25
#define INPUT_SIGNALC_GPIO 8
#define INPUT_SIGNALC_LEDGPIO 9
#define INPUT_SIGNALD_GPIO 12
#define INPUT_SIGNALD_LEDGPIO 13

struct PetInput
{
    uint input_gpio;
    uint led_gpio;
    char* name;
    PIO pio;
    uint smc;
    uint smi;
    uint64_t tm;
    double ts;
} inputs[SM_COUNT];

uint clk_src_freq = 10*MHZ;             // change this only if CLK_SRC_EXT_CLOCK defined
uint8_t first_sensed_input = 255;

void inputs_init() {
    inputs[0].input_gpio = INPUT_SIGNALA_GPIO;
    inputs[0].led_gpio = INPUT_SIGNALA_LEDGPIO;
    inputs[0].name = "ChA";
    if (SM_COUNT > 1) {
        inputs[1].input_gpio = INPUT_SIGNALB_GPIO;
        inputs[1].led_gpio = INPUT_SIGNALB_LEDGPIO;
        inputs[1].name = "ChB";
    }
    if (SM_COUNT > 2) {
        inputs[2].input_gpio = INPUT_SIGNALC_GPIO;
        inputs[2].led_gpio = INPUT_SIGNALC_LEDGPIO;
        inputs[2].name = "ChC";
    }
    if (SM_COUNT > 3) {
        inputs[3].input_gpio = INPUT_SIGNALD_GPIO;
        inputs[3].led_gpio = INPUT_SIGNALD_LEDGPIO;
        inputs[3].name = "ChD";
    }
    for (uint8_t i = 0; i < SM_COUNT; i++) {
        inputs[i].tm = 0;
        inputs[i].ts = 0.0;
    }
}

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
        clk_src_freq = SYS_PLL_FREQ;
        set_sys_clock_khz(SYS_PLL_FREQ/1000, true);
        clock_gpio_init(21, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, XOSC_MHZ*MHZ);
    #else   //CLK_SRC_EXT_CLOCK_PLL
        xosc_init();
        clk_src_freq = SYS_PLL_FREQ;
        set_sys_clock_khz(clk_src_freq/1000, true);
        clock_gpio_init(21, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, 1000);     // put the external ref. clock on pin 21
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
    // program the PIOs
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
//        countpet_forever(inputs[i].pio, inputs[i].smc, off[i], inputs[i].input_gpio);
//        led_indicate_forever(inputs[i].pio, inputs[i].smi, lind[i], inputs[i].input_gpio, inputs[i].led_gpio);
    }
}


int main() {
    vreg_set_voltage(CORE_VOLTAGE);
    xosc_init();
    clocks_init();
    configure_clocks();
    stdio_init_all();

    inputs_init();
    configure_pios();
    sleep_ms(2000);             // wait 1s to allow connecting USB

    #if defined OUTPUT_CYCLE_COUNT          // write header to output
        printf("COUNT\t CHANNEL\n");    
    #elif defined OUTPUT_FREQUENCY
        printf("FREQ\t CHANNEL\n");
    #elif defined OUTPUT_TIMEMARK
        printf("TIMEMARK\t CHANNEL\n");
    #else
        printf("DEBUG\n");
    #endif

    while (true) {
        for (uint8_t i = 0; i < SM_COUNT; i++) {
            if (!pio_sm_is_rx_fifo_empty(inputs[i].pio, inputs[i].smc)) {
                uint32_t clk_cnt = pio_sm_get(inputs[i].pio, inputs[i].smc);            // read the register from ASM code
                clk_cnt = ~clk_cnt;                                // negate the received value
                uint32_t clk_cor = 0;
                // PIO CALIBRATION CORRECTIONS
                #if AVG_PERIODS == 1
                    clk_cor = (clk_cnt+2)*2;
                #else
                    clk_cor = 2*(clk_cnt + 1.5*AVG_PERIODS + 1.5);
                #endif
                #if defined OUTPUT_CYCLE_COUNT
                    printf("%u\t %s\n", clk_cor, inputs[i].name);
                #elif defined OUTPUT_FREQUENCY
                    double fc = ((double)clk_src_freq*(double)AVG_PERIODS)/((double)clk_cor);
                    printf("%.6f\t %s\n", fc, inputs[i].name);
                #elif defined OUTPUT_TIMEMARK
                    if (first_sensed_input == 255) {
                        // save the first sensed impulse input for later use
                        // this happens only once
                        first_sensed_input = i;
                    }
                    if (inputs[i].tm == 0 && first_sensed_input != i && first_sensed_input != 255) {
                        // offset the start of the timescale by the timemark of the first sensed input
                        // this happens only once for each input
                        inputs[i].tm = inputs[first_sensed_input].tm;
                    }
                    uint64_t rem;
                    uint64_t div;
                    inputs[i].tm += clk_cor;
                    div = divmod_u64u64_rem(inputs[i].tm, (uint64_t) clk_src_freq, &rem);
                    inputs[i].ts = (double)div + (double)((uint32_t)rem)/(double)clk_src_freq;
                    printf("%.9f\t %s\n", i, inputs[i].ts, inputs[i].name);
                #else
                    // this is a DEBUG output
                    uint32_t cnt = MHZ;
                    tm[i] = tm[i] + clk_cor;
                    uint64_t rem;
                    uint64_t div;
                    div = divmod_u64u64_rem(tm[i], (uint64_t) clk_src_freq, &rem);
                    ts[i] = (double)div + (double)((uint32_t)rem)/(double)clk_src_freq;
                    double fs = ((double)clk_src_freq*(double)AVG_PERIODS)/((double)clk_cnt);
                    double fc = ((double)clk_src_freq*(double)AVG_PERIODS)/((double)clk_cor);
                    printf("@%u\t %i\t %u\t %u\t %.6f\t %.6f\t %u\t %.10f\t", clk_src_freq, i, clk_cnt, clk_cor, fs, fc, tm[i], ts[i]);
                    for (uint8_t x = 0; x < SM_COUNT; x++) {
                        printf("%u\t %u\t %.10f\t ", x, tm[x], ts[x]);
                    }
                #endif
            }
        }
    }
}
