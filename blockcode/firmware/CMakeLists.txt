# Elxie Block Code firmware — ESP-IDF project (v5.3 or newer)
#   idf.py set-target esp32s3
#   idf.py build flash monitor
# (U8g2 in components/u8g2 already defaults to 16-bit coordinates.)
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(elxie_block_code)
