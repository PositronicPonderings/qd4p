# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  QD4P - QuickDice 4 Pico: builds the library and the example programs for
#  the Pico 2 / Pico 2 W.
#
#  The library itself is the qd4p/ folder; to use it in your own project,
#  copy only that folder (see qd4p/CMakeLists.txt).
#
#  PC tests don't use this file: sh tests/host/run_tests.sh
#
#  Opening this folder with the Raspberry Pi Pico VS Code extension: the
#  extension may offer to add its own settings block at the top of this
#  file. That's fine; keep it above the lines below, but keep the four SPDX
#  lines first: the AI-disclosure check reads only the first few lines.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BOARD pico2 CACHE STRING "Board type")

include(pico_sdk_import.cmake)
project(qd4p_dev C CXX ASM)
pico_sdk_init()

add_subdirectory(qd4p)        # the library: target qd4p
add_subdirectory(examples)    # qd4p_console, ...
