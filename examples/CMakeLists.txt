# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  Example programs. Each is its own build target and .uf2.
#    qd4p_console   try QD4P from a USB serial terminal
# ---------------------------------------------------------------------------
add_executable(qd4p_console console.c tables/table.c)   # tables/: an example table system
target_link_libraries(qd4p_console pico_stdlib qd4p m)
target_compile_options(qd4p_console PRIVATE -Wall -Wextra)
pico_enable_stdio_usb(qd4p_console 1)
pico_enable_stdio_uart(qd4p_console 0)
pico_add_extra_outputs(qd4p_console)
