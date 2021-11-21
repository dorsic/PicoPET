#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/divider.h"
#include "picoPET.h"
#include "pico/double.h"

uint8_t first_sensed_input = 255;
extern uint clk_src_freq;
extern struct PetInput inputs[];


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

void do_count() {
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
                    double fc = ufix2double(clk_src_freq, 0) * ufix2double(AVG_PERIODS, 0)/ufix2double(clk_cor, 0);
//                    double fc = ((double)clk_src_freq*(double)AVG_PERIODS)/((double)clk_cor);
                    printf("%.9f\t %s\n", fc, inputs[i].name);
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
                    inputs[i].ts = ufix642double(div, 0) + ufix642double(rem, 0)/ufix2double(clk_src_freq, 0);
//                    inputs[i].ts = (double)div + (double)((uint32_t)rem)/(double)clk_src_freq;
                    printf("%.9f\t %s\n", inputs[i].ts, inputs[i].name);
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
