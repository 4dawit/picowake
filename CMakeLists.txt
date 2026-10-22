# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w)
include(pico_sdk_import.cmake)
project(picowake C CXX ASM)

if(NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/secrets.h)
    message(FATAL_ERROR "secrets.h not found. Copy secrets.example.h to secrets.h and fill in your Wi-Fi details.")
endif()
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(picowake main.c usb_descriptors.c)
target_include_directories(picowake PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(picowake
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    tinyusb_device
    tinyusb_board)
pico_add_extra_outputs(picowake)
