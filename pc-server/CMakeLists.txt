cmake_minimum_required(VERSION 3.20)
project(WiFiGamepadServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for ultra-low latency & small binary
if (MSVC)
    add_compile_options(/O2 /Oi /Ot /GL /Gy /W3 /utf-8 /EHsc /wd4828)
    add_link_options(/LTCG /OPT:REF /OPT:ICF)
endif()

add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/vigem/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/vigem
)

set(SOURCES
    src/main.cpp
    src/gui_window.cpp
    src/vigem_controller.cpp
    src/udp_receiver.cpp
    src/metrics.cpp
    deps/vigem/ViGEmClient.cpp
    resources/app.rc
)

add_executable(WiFiGamepadServer WIN32 ${SOURCES})

# Required Windows libraries
target_link_libraries(WiFiGamepadServer PRIVATE
    ws2_32
    setupapi
    avrt
    winmm
    dwmapi
    gdiplus
    msimg32
    shell32
)
