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
    qd_rng.c              # generator, unbiased ranges, stirring (portable)
    qd_dice.c             # standard, custom, percentile dice (portable)
    qd_deck.c             # poker, tarot and custom decks (portable)
    qd_roll.c             # roll engine: pools, modifiers, sums, counting (portable)
    qd_parse.c            # dice notation to a roll and back (portable)
    qd_sys.c              # game systems: shortcuts and outcomes (portable)
    qd_stats.c            # stats for nerds: tallies per die, fairness (portable)
    qd_odds.c             # odds: exact or simulated (portable)
    qd_entropy_pico.c     # Pico-only: hardware seed, TRNG/ADC/event sources
)
target_include_directories(qd4p PUBLIC ${CMAKE_CURRENT_LIST_DIR})
# pico_rand: the hardware seed (RP2350 TRNG); hardware_adc: the ADC source.
# A program that never uses the ADC source doesn't carry its code.
target_link_libraries(qd4p PUBLIC pico_stdlib pico_rand hardware_adc)
