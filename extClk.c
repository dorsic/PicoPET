#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/resets.h"
#include "hardware/xosc.h"
#include "hardware/pll.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "picoPET.h"

void e_pll_init(PLL pll, uint xosc_mhz, uint refdiv, uint vco_freq, uint post_div1, uint post_div2) {
    uint32_t ref_mhz = xosc_mhz / refdiv;

    // What are we multiplying the reference clock by to get the vco freq
    // (The regs are called div, because you divide the vco output and compare it to the refclk)
    uint32_t fbdiv = vco_freq / (ref_mhz * MHZ);
/// \end::pll_init_calculations[]

    // fbdiv
    assert(fbdiv >= 16 && fbdiv <= 320);

    // Check divider ranges
    assert((post_div1 >= 1 && post_div1 <= 7) && (post_div2 >= 1 && post_div2 <= 7));

    // post_div1 should be >= post_div2
    // from appnote page 11
    // postdiv1 is designed to operate with a higher input frequency
    // than postdiv2
    assert(post_div2 <= post_div1);

    // Check that reference frequency is no greater than vco / 16
    assert(ref_mhz <= (vco_freq / 16));

    // div1 feeds into div2 so if div1 is 5 and div2 is 2 then you get a divide by 10
    uint32_t pdiv = (post_div1 << PLL_PRIM_POSTDIV1_LSB) |
                    (post_div2 << PLL_PRIM_POSTDIV2_LSB);

/// \tag::pll_init_finish[]
    if ((pll->cs & PLL_CS_LOCK_BITS) &&
        (refdiv == (pll->cs & PLL_CS_REFDIV_BITS)) &&
        (fbdiv  == (pll->fbdiv_int & PLL_FBDIV_INT_BITS)) &&
        (pdiv   == (pll->prim & (PLL_PRIM_POSTDIV1_BITS & PLL_PRIM_POSTDIV2_BITS)))) {
        // do not disrupt PLL that is already correctly configured and operating
        return;
    }

    uint32_t pll_reset = (pll_usb_hw == pll) ? RESETS_RESET_PLL_USB_BITS : RESETS_RESET_PLL_SYS_BITS;
    reset_block(pll_reset);
    unreset_block_wait(pll_reset);

    // Load VCO-related dividers before starting VCO
    pll->cs = refdiv;
    pll->fbdiv_int = fbdiv;

    // Turn on PLL
    uint32_t power = PLL_PWR_PD_BITS | // Main power
                     PLL_PWR_VCOPD_BITS; // VCO Power

    hw_clear_bits(&pll->pwr, power);

    // Wait for PLL to lock
    while (!(pll->cs & PLL_CS_LOCK_BITS)) tight_loop_contents();

    // Set up post dividers
    pll->prim = pdiv;

    // Turn on post divider
    hw_clear_bits(&pll->pwr, PLL_PWR_POSTDIVPD_BITS);
/// \end::pll_init_finish[]
}

void set_xosc_freq(uint new_xosc_mhz, uint div_freq) {
    uint xosc_divider = new_xosc_mhz*MHZ/div_freq;

    watchdog_start_tick(new_xosc_mhz);
    clock_configure(clk_ref, CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC, 0, new_xosc_mhz * MHZ, new_xosc_mhz * MHZ);
    clock_gpio_init(DIVCLK_GPIO, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, xosc_divider);
    e_pll_init(pll_usb, new_xosc_mhz, 1, 480 * MHZ, 5, 2);
   
    // CLK peri is clocked from clk_sys so need to change clk_peri's freq
    clock_configure(clk_peri, 0, CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, 48 * MHZ, 48 * MHZ);
    // Re init uart now that clk_peri has changed
    stdio_init_all();
}

int get_timebase() {
    // returns 0 if correct timebase is used (frequency count returns 12kHz)
    // returns 1 if internal 12MHz timebase used, but external should be (frequency count returns 8.333 kHz)
    // returns -1 if external 10MHz timebase used, but internal should be (frequency count return 14 kHz)
    // returns true, if the ADC reading is 12 MHz, so we have good time base selected
    int32_t f = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);    
    f -= XOSC_MHZ * 1000; 
//    printf("dF %i kHz\n", f);
    if (abs(f) < 10) {
        return 0;
    }
    return (f>0)? 1: -1;
}

bool ext_clock_available() {
    uint16_t cnt = 0;
    for (uint16_t i = 0; i < 100; i++) {
        cnt += (uint8_t)gpio_get(CLKREF_EXT_GPIO);
    }
    return cnt > 10;
}