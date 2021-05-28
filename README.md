# PicoPET
A simple time stamper.

Let Raspberry Pi Pico count the number of cycles between two rising edges of an clock. 
Resulting time mark of each rising edge, or clock cycle count between adjacent rising edges or resulting measured frequency is written to serial or USB output.

Different external/internal clocks can be used. See following table with compiled sample UF2 file links for outputting rising edge time marks:

| Clock | Frequency | Resolution | Output | UF2 binary |
| ----- | :-------: | :--------: | :----: | ---------- |
| external | 10 MHz | 200 ns | serial | [picoPET_e10M_serial.uf2](build/picoPET_e10M_serial.uf2) |
| external + internal PPL | 10 / 125 MHz | 16 ns | USB | [picoPET_e10M_125pll_USB.uf2](build/picoPET_e10M_125pll_USB.uf2) |
| iternal XOSC | 12 MHz | 166 ns | serial | [picoPET_i12M_serial.uf2](build/picoPET_i12M_serial.uf2) |
| internal PLL | 125 MHz | 16 ns | USB | [picoPET_i125M_USB.uf2](build/picoPET_i125M_USB.uf2) |

RP2040 allows external clock in range 1-15 MHz. GPIO pins are able to handle up to 50 MHz. See [RP2040 datasheet](https://datasheets.raspberrypi.org/rp2040/rp2040-datasheet.pdf) for details.

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
In the `picoPET.c` file uncomment one of the appropriate clock sources, or feel free to implement you own.

```
//#define CLK_SRC_EXT_CLOCK
//#define CLK_SRC_XOSC
#define CLK_SRC_SYS_PLL_125M
//#define CLK_SRC_EXT_CLOCK_PLL_125M
```

When using external clock (CLK_SRC_EXT_CLOCK) with other than 10MHz frequency, change also the clock_freq variable
```
uint clock_freq = 10000000; // 10 MHz; change this if using external clock with differenct frequency
```

#### Setting output type
In the `picoPET.c` file uncomment one of the appropriate output types, or feel free to implement you own.
```
#define OUTPUT_TIMEMARK
//#define OUTPUT_CYCLE_COUNT
//#define OUTPUT_FREQUENCY
```
#### Changing the pinout
You can change the input signal pin and the LED pin by changing INPUT_SIGNAL_GPIO and LED_PIN constants.

```
#define INPUT_SIGNAL_GPIO 16
#define LED_PIN 25
```

NOTE: External clock must be connected to GPIO20 (default) or GPIO22.


