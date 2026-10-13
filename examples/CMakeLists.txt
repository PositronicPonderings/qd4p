# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  Example programs. Each is its own build target and .uf2
#  (build/examples/qd4p_<name>.uf2), and talks over USB serial.
#    qd4p_console     try everything from a serial terminal
#    qd4p_hello_dice  1  press a key, roll a d20
#    qd4p_character   2  D&D ability scores, built in code
#    qd4p_attack      3  a D&D attack and its damage
#    qd4p_savage      4  Savage Worlds: initiative, raises, bennies
#    qd4p_loot        5  a random table in the app, without the parser
#    qd4p_dice_box    6  type any roll
#    qd4p_odds        7  compare choices by their odds
#  docs/manual/03-examples.md walks through them.
# ---------------------------------------------------------------------------
add_executable(qd4p_console console.c tables/table.c)   # tables/: an example table system
target_link_libraries(qd4p_console pico_stdlib qd4p m)
target_compile_options(qd4p_console PRIVATE -Wall -Wextra)
pico_enable_stdio_usb(qd4p_console 1)
pico_enable_stdio_uart(qd4p_console 0)
pico_add_extra_outputs(qd4p_console)

foreach(name hello_dice character attack savage loot dice_box odds)
  add_executable(qd4p_${name} ${name}.c)
  target_link_libraries(qd4p_${name} pico_stdlib qd4p)
  target_compile_options(qd4p_${name} PRIVATE -Wall -Wextra)
  pico_enable_stdio_usb(qd4p_${name} 1)
  pico_enable_stdio_uart(qd4p_${name} 0)
  pico_add_extra_outputs(qd4p_${name})
endforeach()
