# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  The size check: one small Pico 2 program per QD4P feature, built exactly
#  as an app would build them (the SDK's normal Release settings), so the
#  difference from base.c is what that feature costs. measure.py builds
#  them and writes the tables in docs/SIZES.md; it's the easy way in.
#
#      python3 tests/size/measure.py            (needs PICO_SDK_PATH and
#                                                 arm-none-eabi-gcc)
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(PICO_BOARD pico2 CACHE STRING "Board type")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../../pico_sdk_import.cmake)
project(qd4p_size C CXX ASM)
pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../qd4p qd4p)
if(QD_DEFINES)                       # measure.py --define NAME=VALUE: smaller limits
  target_compile_definitions(qd4p PUBLIC ${QD_DEFINES})
endif()
set(TABLES ${CMAKE_CURRENT_LIST_DIR}/../../examples/tables)

foreach(p base rng dice deck engine systems parser rerolls odds stats text
          tables_own tables_example fair)
  add_executable(${p} ${p}.c)
  if(p STREQUAL "tables_example")
    target_sources(${p} PRIVATE ${TABLES}/table.c)
    target_include_directories(${p} PRIVATE ${TABLES} ${CMAKE_CURRENT_BINARY_DIR})
  endif()
  if(p STREQUAL "base")
    target_link_libraries(${p} pico_stdlib)
  else()
    target_link_libraries(${p} pico_stdlib qd4p)
  endif()
  pico_enable_stdio_usb(${p} 1)
  pico_enable_stdio_uart(${p} 0)
  pico_add_extra_outputs(${p})
endforeach()
