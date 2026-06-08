cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(redbox C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(${PROJECT_NAME}
    src/main.c
    src/usb-desc.c
    src/Pico_WS2812/WS2812.cpp
    src/led.cpp
    # src/pio_i2c.c
)

target_compile_definitions(${PROJECT_NAME} PRIVATE PICO_ENTER_USB_BOOT_ON_EXIT=1)
target_compile_definitions(${PROJECT_NAME} PRIVATE PICO_PRINTF_ALWAYS_INCLUDED=1)


target_include_directories(${PROJECT_NAME} PRIVATE
    src/
)

# Include the various pico libraries we need
target_link_libraries(${PROJECT_NAME}
    tinyusb_device
    tinyusb_board
    pico_multicore
    pico_stdlib
    hardware_pio
    hardware_i2c
)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/src/Pico_WS2812/WS2812.pio)


pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)


# Creates additional output files
pico_add_extra_outputs(${PROJECT_NAME})
