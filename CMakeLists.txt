cmake_minimum_required(VERSION 3.13)
include(/Users/dorsic/Development/Pico/pico-sdk/external/pico_sdk_import.cmake)

project(picoPET)
pico_sdk_init()

add_executable(picoPET 
    picoPET.c 
    extClk.c
    counter.c
    picoPET_sp.pio
    picoPET_mp.pio
)

pico_generate_pio_header(picoPET ${CMAKE_CURRENT_LIST_DIR}/picoPET_sp.pio)
pico_generate_pio_header(picoPET ${CMAKE_CURRENT_LIST_DIR}/picoPET_mp.pio)
pico_generate_pio_header(picoPET ${CMAKE_CURRENT_LIST_DIR}/indicator_led.pio)


target_sources(picoPET PRIVATE picoPET.c 
    extClk.c
    counter.c
)

target_link_libraries(picoPET
    pico_stdlib
    pico_stdio_uart
    pico_divider
    pico_multicore
    pico_double
    hardware_timer
    hardware_clocks
    hardware_pio
    hardware_pll
    hardware_xosc
    hardware_vreg
    hardware_irq
    hardware_uart
)

pico_enable_stdio_usb(picoPET 1)
pico_enable_stdio_uart(picoPET 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(picoPET)
