# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE MinSizeRel)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico_booty C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(pico_booty
pico_booty.cpp
parallel.pio
payload.s
)

pico_set_program_name(pico_booty "pico_booty")
pico_set_program_version(pico_booty "0.1")

# no_flash means the target is to run from RAM
#pico_set_binary_type(pico_booty no_flash)

# Generate PIO header
pico_generate_pio_header(pico_booty ${CMAKE_CURRENT_LIST_DIR}/parallel.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pico_booty 0)
pico_enable_stdio_usb(pico_booty 1)

# Add the standard library to the build
target_link_libraries(pico_booty
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(pico_booty PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(pico_booty 
        hardware_dma
        hardware_pio
        )

#pico_set_binary_type(pico_booty copy_to_ram)

pico_add_extra_outputs(pico_booty)

