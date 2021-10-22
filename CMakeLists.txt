cmake_minimum_required(VERSION 3.12)
include(/Users/dorsic/Development/Pico/pico-sdk-10MHz/external/pico_sdk_import.cmake)

project(picoPET)
pico_sdk_init()

add_executable(picoPET 
    picoPET.c 
    picoPET_sp.pio
    picoPET_mp.pio
#    picoDIV.pio
)

# pico_generate_pio_header(picoPET ${CMAKE_CURRENT_LIST_DIR}/picoDIV.pio)
pico_generate_pio_header(picoPET ${CMAKE_CURRENT_LIST_DIR}/picoPET_sp.pio)
pico_generate_pio_header(picoPET ${CMAKE_CURRENT_LIST_DIR}/picoPET_mp.pio)

target_sources(picoPET PRIVATE picoPET.c)

target_link_libraries(picoPET
    pico_stdlib
    pico_divider
    hardware_timer
    hardware_clocks
    hardware_pio
    hardware_vreg
)

pico_enable_stdio_usb(picoPET 1)
pico_enable_stdio_uart(picoPET 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(picoPET)
