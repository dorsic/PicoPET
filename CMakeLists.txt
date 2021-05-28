cmake_minimum_required(VERSION 3.12)
include(/Users/dorsic/Development/Pico/pico-sdk/external/pico_sdk_import.cmake)

project(picoPET)
pico_sdk_init()

add_executable(picoPET 
    picoPET.c 
    picoPET.pio
    picoDIV.pio
)

# pico_generate_pio_header(picoPET ${CMAKE_CURRENT_LIST_DIR}/picoDIV.pio)
pico_generate_pio_header(picoPET ${CMAKE_CURRENT_LIST_DIR}/picoPET.pio)

target_sources(picoPET PRIVATE picoPET.c)

target_link_libraries(picoPET
    pico_stdlib
    hardware_clocks
    hardware_pio
)

pico_enable_stdio_usb(picoPET 0)
pico_enable_stdio_uart(picoPET 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(picoPET)
