cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

project(udpih C CXX ASM)

# Initialize the SDK
pico_sdk_init()

set(UDPIH_SOURCES
    main.c
    usb.c
    ../common/device.c
)

set(UDPIH_LIBS
    pico_stdlib
    hardware_resets
    hardware_irq
)

add_compile_options(
    # not supported on the pico (reduces binary size)
    -DNO_HIGH_SPEED
)

include_directories(
    ../common
)

add_executable(udpih ${UDPIH_SOURCES})

target_link_libraries(udpih PRIVATE ${UDPIH_LIBS})

pico_add_extra_outputs(udpih)

pico_set_program_name(udpih "udpih")
pico_set_program_description(udpih "Wii U USB Host Stack exploit")
pico_set_program_url(udpih "https://github.com/GaryOderNichts/udpih")
pico_set_program_version(udpih "0")
