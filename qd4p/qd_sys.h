/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_sys.h
 * @brief   Game systems: shortcuts that build the right roll, and the
 *          outcome rules that say what the result means.
 *
 *     qd_parsed_t p;
 *     qd_result_t res;
 *     qd_outcome_t out;
 *
 *     qd_parse("sw8 + 1 vs 4", &p, &err);        // Savage Worlds, d8 trait
 *     qd_roll_run(&p.roll, &res);
 *     qd_sys_outcome(&p.roll, &res, &out);
 *     // out.text "Success, 1 raise", out.detail "1 ace", out.degrees 1
 *
 * The same rolls can be built in C without the parser:
 *
 *     qd_roll_init(&roll);
 *     qd_sys_sw(&roll, 8, true);                 // d8 trait + wild d6
 *     qd_roll_add(&roll, 1);
 *     qd_roll_vs(&roll, 4);
 *
 * SYSTEMS (notation: docs/manual/05-notation.md, "Game systems")
 *
 *   any roll  ... vs N        pass if the total (or the successes) is N or more
 *   D&D 5e    atk/chk/save    a d20, adv/dis, nat 20/1 (attacks only: auto hit/miss)
 *   Savage W. sw4..sw12       trait + wild die, both explode, best counts; TN 4;
 *                             raises every 4; snake eyes; x for an Extra
 *   CoC 7e    coc 65 b1       d% with bonus/penalty dice; Regular/Hard/Extreme,
 *                             Critical, Fumble; vs hard / vs extreme
 *   FATE      fate +2         4dF + skill, the ladder; vs N gives shifts
 *   VtM 5e    vtm 7 h2        pool with hunger dice; criticals, messy criticals,
 *                             bestial failures
 *   DCC       dcc d20 +1d     dice chain steps; natural max crits, natural 1 fumbles
 *
 * OUTCOME LEVELS: every system's outcome has a `level`, ordered so that a
 * higher level is a better result, with 0 meaning "no judgement" and
 * negative levels failures. The enums below name them.
 */
#ifndef QD_SYS_H
#define QD_SYS_H

#include <stdbool.h>
#include <stdint.h>

#include "qd_roll.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    QD_SYS_NONE = 0,                    /**< plain notation (vs N still judged) */
    QD_SYS_DND,                         /**< D&D 5th edition */
    QD_SYS_SW,                          /**< Savage Worlds (SWADE) */
    QD_SYS_COC,                         /**< Call of Cthulhu 7th edition */
    QD_SYS_FATE,                        /**< FATE Core / Accelerated */
    QD_SYS_VTM,                         /**< Vampire: the Masquerade 5th edition */
    QD_SYS_DCC,                         /**< Dungeon Crawl Classics */
} qd_sys_id_t;

/** D&D roll kinds (qd_system_t.kind). */
enum { QD_DND_ATTACK = 1, QD_DND_CHECK, QD_DND_SAVE };

/** CoC difficulty (the target of "vs regular / hard / extreme"). */
enum { QD_COC_REGULAR = 1, QD_COC_HARD, QD_COC_EXTREME };

/** Outcome levels. Shared where systems agree; each system uses a subset. */
enum {
    /* plain, D&D, DCC, Savage Worlds */
    QD_LVL_CRIT_FAIL = -2,              /**< SW critical failure, DCC fumble */
    QD_LVL_FAIL      = -1,
    QD_LVL_NONE      = 0,
    QD_LVL_PASS      = 1,
    QD_LVL_RAISE     = 2,               /**< SW: success with a raise or more */
    QD_LVL_CRIT      = 3,               /**< D&D critical hit, DCC critical */

    /* Call of Cthulhu */
    QD_COC_FUMBLE    = -2,
    QD_COC_FAILURE   = -1,
    QD_COC_REGULAR_SUCCESS = 1,
    QD_COC_HARD_SUCCESS    = 2,
    QD_COC_EXTREME_SUCCESS = 3,
    QD_COC_CRITICAL        = 4,

    /* FATE (with "vs") */
    QD_FATE_FAIL     = -1,
    QD_FATE_TIE      = 1,
    QD_FATE_SUCCESS  = 2,
    QD_FATE_STYLE    = 3,               /**< success with style: 3+ shifts */

    /* Vampire 5e */
    QD_VTM_BESTIAL   = -3,              /**< failed with a 1 on a hunger die */
    QD_VTM_TOTAL_FAIL = -2,             /**< no successes at all */
    QD_VTM_FAIL      = -1,
    QD_VTM_WIN       = 1,
    QD_VTM_MESSY     = 2,               /**< critical with a 10 on a hunger die */
    QD_VTM_CRITICAL  = 3,               /**< a pair of 10s */
};

/** What a result means. */
typedef struct {
    uint8_t  system;                    /**< qd_sys_id_t */
    int8_t   level;                     /**< see the enums above */
    bool     judged;                    /**< a target was known, so `pass` means something */
    bool     pass;
    bool     critical, fumble;          /**< natural 20 / snake eyes / 01 / pair of 10s ... */
    int32_t  value;                     /**< what was judged: the total, or successes */
    int32_t  target;
    int32_t  margin;                    /**< value - target, when judged */
    int16_t  degrees;                   /**< SW raises, FATE shifts, VtM margin, aces ... */
    char     text[40];                  /**< the headline: "Extreme success" ("" if nothing to say) */
    char     detail[64];                /**< more: "natural 20", "Good (+3)", "2 aces" */
} qd_outcome_t;

/* --- Building (the parser uses these too) ------------------------------- */
/* Each starts a fresh roll with the system's dice. Add maths (qd_roll_add,
 * more pools) and a target (qd_roll_vs) afterwards if you like. */

/** Judge the roll against N: "vs N". */
void qd_roll_vs(qd_roll_t *r, int32_t target);

/** D&D 5e: 1d20 with advantage (adv > 0) or disadvantage (adv < 0). kind
 *  QD_DND_ATTACK, _CHECK or _SAVE. */
bool qd_sys_dnd(qd_roll_t *r, int kind, int adv);

/** Savage Worlds: a trait die (4, 6, 8, 10 or 12 sides) and, for a Wild
 *  Card, a d6 wild die; both explode, the better counts. Target 4 unless
 *  qd_roll_vs says otherwise. */
bool qd_sys_sw(qd_roll_t *r, int trait_sides, bool wild_card);

/** Call of Cthulhu 7e: d% against `skill` (1-99... up to 999 for odd
 *  cases), with bonus (> 0) or penalty (< 0) dice, up to 3. A target from
 *  qd_roll_vs is the difficulty: QD_COC_REGULAR, _HARD or _EXTREME. */
bool qd_sys_coc(qd_roll_t *r, int skill, int bonus);

/** FATE: 4dF. Add the skill with qd_roll_add. */
bool qd_sys_fate(qd_roll_t *r);

/** Vampire 5e: a pool of d10s of which `hunger` are hunger dice. */
bool qd_sys_vtm(qd_roll_t *r, int pool, int hunger);

/** DCC: `count` dice of `sides`, moved `steps` along the dice chain
 *  (+1 is the next die up). Returns false if `sides` isn't on the chain. */
bool qd_sys_dcc(qd_roll_t *r, int count, int sides, int steps);

/** The DCC dice chain: d3 d4 d5 d6 d7 d8 d10 d12 d14 d16 d20 d24 d30.
 *  Returns the die `steps` along from `sides` (stopping at the ends), or 0
 *  if `sides` isn't on the chain. */
int qd_dcc_chain(int sides, int steps);

/* --- Judging --------------------------------------------------------------- */

/** Work out what `res` means for roll `r`. Returns false (and a level of
 *  QD_LVL_NONE) when there is nothing to judge: plain notation with no
 *  "vs". */
bool qd_sys_outcome(const qd_roll_t *r, const qd_result_t *res, qd_outcome_t *out);

/** A short name for an outcome level of this roll's system: "Critical
 *  hit", "Raise", "Hard success", "Messy critical" ... ("" if none). For
 *  showing odds: qd_odds_t.level[L + 4] is the chance of level L. */
const char *qd_sys_level_name(const qd_roll_t *r, int level);

/** "D&D 5e", "Savage Worlds", ... ("" for none). */
const char *qd_sys_name(int id);

/** The FATE ladder: 3 -> "Good". -2..+8 have names; beyond, the nearest
 *  end ("Legendary", "Terrible"). */
const char *qd_fate_ladder(int value);

#ifdef __cplusplus
}
#endif

#endif /* QD_SYS_H */
