#include "hardware/pio.h"

// INPUT CLOCK SOURECE REFERENCE
//#define CLK_SRC_XOSC                  // internal 12MHz TCXO
#define CLK_SRC_XOSC_PLL                // PLL sourced from internal TCXO
//#define CLK_SRC_EXT_CLOCK             // external clock on PIN20, max. 50 MHz, set clk_src_freq below appropriately
#define CLK_EXT_MHZ 10             // change if CLK_SRC_EXT_CLOCK defined

// PLL SETTINGS
#define SYS_PLL_FREQ 240*MHZ              // slightly overclocked from default 125 MHz; NOTE not arbitrary freq possible. See RP2040 datasheet
#define CORE_VOLTAGE VREG_VOLTAGE_1_20    // consider higher core voltage for higher PLL, default is 1.1V

// OUTPUT SETTINGS
#define OUTPUT_TIMEMARK
//#define OUTPUT_FREQUENCY
//#define OUTPUT_CYCLE_COUNT

#define AVG_PERIODS 1                   // number of periods to average; has to at least 1, for steady results use odd numbers 1, 3, 5...

// INPUTS wiring
#define SM_COUNT 2
#define INPUT_SIGNALA_GPIO 5
#define INPUT_SIGNALA_LEDGPIO 4
#define INPUT_SIGNALB_GPIO 6
#define INPUT_SIGNALB_LEDGPIO 7
#define INPUT_SIGNALC_GPIO 9
#define INPUT_SIGNALC_LEDGPIO 8
#define INPUT_SIGNALD_GPIO 13
#define INPUT_SIGNALD_LEDGPIO 12

// CLOCK references wiring
#define CLKREF_GNSS_GPIO 20
#define CLKREF_EXT_GPIO 11
#define DIVCLK_GPIO 21 
#define CLKREF_EXT_INDICATOR_GPIO 14


// GNSS wiring
#define GNSS_BAUDRATE 9600
#define GNSS_TXGPIO 1
#define GNSS_RXGPIO 0
#define GNSS_LOCKED_INDICATOR_GPIO 15



// DIVIDER SWITCHes
#define SW1_GPIO 16
#define SW2_GPIO 17
#define SW3_GPIO 18
#define SW4_GPIO 191


// DIVIDER FREQUENCIES
#define DIVF_XOSC 0
#define DIVF_1 1
#define DIVF_10 2
#define DIVF_100 3
#define DIVF_500 4
#define DIVF_1k 5
#define DIVF_2k 6
#define DIVF_1M 7

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
};



