# Elxie hardware testbench — a separate ESP-IDF app that checks every sensor and
# actuator before the main firmware goes on. Uses the main firmware's pin map
# (blockcode/firmware/main/board_config.h) and its HAL, so it tests exactly those pins.
#   Run TESTBENCH.bat in the project root (build + flash + live log).
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../blockcode/firmware/components/u8g2")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(elxie_testbench)
