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

### ToDo
- Blink onboard LED with ASM side-set technique. Now is independent of the input pulses. For external clock it blinks every 10M cycles.
- Allow configuring input signal GPIO pin.


