#!/bin/sh
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  The QD4P console on a PC: the same console.c and library as the device,
#  built with a C compiler. Its main use is checking a fair-mode session:
#
#      sh examples/pc/build.sh            (writes examples/pc/qd4p_pc)
#      examples/pc/qd4p_pc                (type commands, as on the device)
#      examples/pc/qd4p_pc < session.txt  (or run a file of them)
#
#  Needs: a C compiler (cc / gcc / clang) on Linux, macOS or WSL.
#  OUT=path/to/program sh build.sh  writes it somewhere else (the tests do).
# ---------------------------------------------------------------------------
cd "$(dirname "$0")" || exit 1
L=../../qd4p
CC=${CC:-cc}
OUT=${OUT:-qd4p_pc}
$CC -std=c11 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -I. -I$L -I.. \
    -o "$OUT" ../console.c ../tables/table.c pc_hardware.c \
    $(ls $L/*.c | grep -v '_pico\.c$') -lm && echo "built $OUT"
