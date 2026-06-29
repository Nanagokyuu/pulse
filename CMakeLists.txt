cmake_minimum_required(VERSION 3.20)
project(pulse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic -O2)

find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
find_package(Curses REQUIRED)

add_executable(pulse
    src/main.cpp
    src/utils/format.cpp
    src/smc/smc.cpp
    src/io_report/io_report.cpp
    src/io_report/io_report_power.cpp
    src/io_report/io_report_freq.cpp
    src/sensors/temperature.cpp
    src/sensors/cpu.cpp
    src/sensors/memory.cpp
    src/sensors/battery.cpp
    src/powermetrics/powermetrics.cpp
    src/ui/ncurses.cpp
)

target_include_directories(pulse PRIVATE src)

target_link_libraries(pulse PRIVATE
    ${IOKIT_FRAMEWORK}
    ${COREFOUNDATION_FRAMEWORK}
    ${CURSES_LIBRARIES}
)
