# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  QD4P - QuickDice 4 Pico
#
#  It stands alone: it needs only the Pico SDK. Copy this folder into your
#  project and, in your project's CMakeLists.txt:
#      add_subdirectory(qd4p)
#      target_link_libraries(my_app pico_stdlib qd4p)
#  then #include "qd4p.h".
#
#  Limits (dice per roll, cards per deck, ...) are in qd_config.h; override
#  any of them with target_compile_definitions(qd4p PUBLIC QD_MAX_...=n).
# ---------------------------------------------------------------------------
add_library(qd4p STATIC
    qd_version.c
)
target_include_directories(qd4p PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(qd4p PUBLIC pico_stdlib)
