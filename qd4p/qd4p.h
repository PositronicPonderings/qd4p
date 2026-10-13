/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd4p.h
 * @brief   QD4P - QuickDice 4 Pico. The one header a program includes.
 *
 *     #include "qd4p.h"
 *
 *     printf("QD4P %s\n", qd_version());
 *
 * QD4P rolls dice, deals cards and works out what a roll means in a game
 * (raises, success levels, criticals). It is built in layers, each usable
 * without the ones above it:
 *
 *     randomness       qd_rng_*      (D1; Pico sources in qd_entropy_pico.h)
 *     dice and decks   qd_die_*, qd_roll_percentile, qd_deck_*  (D2)
 *     roll engine      qd_roll_*     (D3)
 *     parser           qd_parse, qd_format  (D4; notation in docs/notation.md)
 *     game systems     qd_sys_*, qd_sys_outcome  (D5)
 *     rerolls, sets    qd_reroll_*, qd_set_run  (D6)
 *     stats, odds      qd_stats_*, qd_odds  (D7)
 *     dice in text     qd_expand, qd_roll_text, qd_pick_range  (D8;
 *                      a table system built on them: examples/tables)
 *     fair mode        qd_fair_*     (D9: rolls players can check afterwards)
 *
 * Everything except the hardware entropy sources is portable C11: it runs
 * on a PC as well as the Pico, which is how it is tested.
 *
 * NAMING: every public function, type and constant starts with qd_ / QD_.
 */
#ifndef QD4P_H
#define QD4P_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "qd_config.h"
#include "qd_rng.h"
#include "qd_dice.h"
#include "qd_deck.h"
#include "qd_roll.h"
#include "qd_parse.h"
#include "qd_sys.h"
#include "qd_stats.h"
#include "qd_odds.h"
#include "qd_text.h"
#include "qd_fair.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Library version as numbers, for #if checks. */
#define QD_VERSION_MAJOR 0
#define QD_VERSION_MINOR 0
#define QD_VERSION_PATCH 10

/** Library version as text, e.g. "0.0.1". */
const char *qd_version(void);

#ifdef __cplusplus
}
#endif

#endif /* QD4P_H */
