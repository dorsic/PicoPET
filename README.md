# PicoPET
A simple time stamper, frequency counter.

Let Raspberry Pi Pico count the number of clock cycles between two (or more) rising edges of an input signal. 
Resulting time mark of each rising edge, or clock count between adjacent rising edges or measured frequency is written to serial or USB output.

The code allows for different external/internal clock sources to be used. 

| Clock | Frequency | Resolution | Output |
| ----- | :-------: | :--------: | :----: |
| iternal XOSC | 12 MHz | 166 ns | serial |
| internal XOSC with PLL | 125 MHz (default) | 16 ns (@125MHz) | serial or USB |
| external | 1-50 MHz | 200 ns (@10MHz) | serial |
| external with PPL<sup>*</sup> | 125 MHz (default) | 16 ns (@125MHz| serial or USB |

<sup>*</sup> _Using PLL with external clock source requires hardware modifications and Pico SDK modifications. On the HW side it is required to remove onboard XOSC and to connect the external clock to XIN pin of the RP2040 chip. External clock source has to have squared 3.3V signal up to 25 MHz (by experience). To upload a new code 12 MHz is required. See additional notes below._

RP2040 allows external clock in range 1-15 MHz. GPIO pins are able to handle up to 50 MHz. See [RP2040 datasheet](https://datasheets.raspberrypi.org/rp2040/rp2040-datasheet.pdf) for details.
Internal PLL can be sourced only from the onboard XOSC.

## Wiring

- Connect input pulse signal to GPIO16 (pin 21).
- Connect external clock input to GPIO20 (pin 26).
- Connect to GPIO0 (pin 1) for serial output (115200 bauds).
- Clock output available at GPIO21 (pin 27) for configurations with external 10 MHz and internal XOSC clocks.

Note all Raspberry Pi Pico GPIOs are 3.3V only. Do not connect 5V signals unless you known what you are doing.

## Compile your own version
To compile your own build you need to have installed C/C++ RPi Pico toolchain. Consult [Getting started guide](https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf) on how to prepare your environment.

Then consider following source code modifications:

#### Setting USB or serial output
In the `CMakeLists.txt` file enable USB nd disable UART for USB output or vice versa by modification of these 2 lines

```
pico_enable_stdio_usb(picoPET 1)
pico_enable_stdio_uart(picoPET 0)
```
Set 1 for enabling the output interface and 0 for disabling.

NOTE: Using of USB requires system clock of 48 MHz or above. It will not work e.g. external 10 MHz without not using the internal PLL.

#### Setting clock source
In the `picoPET.c` file uncomment one of the appropriate clock sources or feel free to implement you own.

```
//#define CLK_SRC_XOSC
#define CLK_SRC_XOSC_PLL
//#define CLK_SRC_EXT_CLOCK
//#define CLK_SRC_EXT_CLOCK_PLL
```

When using external clock (CLK_SRC_EXT_CLOCK) with other than 10MHz frequency, change also the uint `clk_src_freq` variable.
```
uint clk_src_freq = 10*MHZ;             // change this only if CLK_SRC_EXT_CLOCK defined
```

#### Setting output type
In the `picoPET.c` file uncomment one of the appropriate output types or feel free to implement you own.
```
#define OUTPUT_FREQUENCY
//#define OUTPUT_TIMEMARK
//#define OUTPUT_CYCLE_COUNT
```

#### Number of averaging periods
More the one period of the input signal can be sensed and thus increasing the gate time and resolution. The number of input signal periods is configured by `AVG_PERIODS` constant in the `picoPET.c` file.

```
#define AVG_PERIODS 1                   // number of periods to average; has to at least 1, for steady results use odd numbers 1, 3, 5...
```


#### Changing the pinout
You can change the input signal pin and the LED pin by changing INPUT_SIGNAL_GPIO and LED_PIN constants.

```
#define INPUT_SIGNAL_GPIO 16
#define LED_PIN 25
```

NOTE: External clock must be connected to GPIO20 (default) or GPIO22.


## Using external clock with PLL
Using external clock with PLL requires connecting the external clock signal to RP2040's XIN pin and also modify the Pico-SDK and set new frequency.

#### HW modification
Connecting external clock to XIN is best achieved by removing the onboard XOSC and bypassing the XOSC ouput pad with nearest (e.g. GPIO10) pin.
To upload new code to the Pico 12 MHz signal has to be used. 
Convenient, reasonable stable 12 MHz 3.3V square signal can be obtained e.g. from the ublox GNSS modules. These use 48 MHz internal clock and 12 MHz is natural number divider so jitter is minimalized.
![](IMG_6690.jpg | width=200)

#### Pico-SDK modification
New XOSC frequency of the external clock has to be configured in the Pico SDK. Make a new copy of the SDK and modify these files

###### \<pico-sdk-path\>/src/host/pico_platform/include/hardware/platform_defs.h
Change the `XOSC_MHZ` constant from 12 MHz to frequency of your external clock (e.g. 10 MHz)
```
#define XOSC_MHZ 10
```

###### \<pico-sdk-path\>/src/rp2040/hardware_regs/include/hardware/platform_defs.h
Change the `XOSC_MHZ` constant from 12 MHz to frequency of your external clock (e.g. 10 MHz)
```
#define XOSC_MHZ _u(10)
```

###### \<pico-sdk-path\>/src/rp2_common/hardware_clocks/clocks.c
The clock.c hardwires the 12 MHz frequency of onboard XOSC and we want to make it variable refering `XOSC_MHZ` constant. 
Locate the `clocks_init()` procedure update the configuration of `clk_ref` by substiting `12 * MHZ` constant with `XOSC_MHZ`. Change the line
```
    clock_configure(clk_ref,
                    CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC,
                    0, // No aux mux
                    12 * MHZ,
                    12 * MHZ);
```
to 
```
    clock_configure(clk_ref,
                    CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC,
                    0, // No aux mux
                    XOSC_MHZ,
                    XOSC_MHZ);
```
