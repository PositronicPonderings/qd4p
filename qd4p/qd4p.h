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
 *     dice and decks   qd_die_*, qd_deck_*        (D2)
 *     roll engine      qd_roll_*     (D3)
 *     parser           qd_parse      (D4)
 *     game systems     qd_sys_*      (D5)
 *     extras           rerolls, sets, tables, odds, stats, fair mode
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

#ifdef __cplusplus
extern "C" {
#endif

/** Library version as numbers, for #if checks. */
#define QD_VERSION_MAJOR 0
#define QD_VERSION_MINOR 0
#define QD_VERSION_PATCH 2

/** Library version as text, e.g. "0.0.1". */
const char *qd_version(void);

#ifdef __cplusplus
}
#endif

#endif /* QD4P_H */
