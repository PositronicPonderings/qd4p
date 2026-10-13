/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_config.h
 * @brief   QD4P's compile-time limits. Change them here, or override any of
 *          them from your build (e.g. -DQD_MAX_KEPT_DICE=50) without editing
 *          this file.
 *
 * WHY LIMITS?
 *   QD4P never calls malloc. Every result, deck and compiled roll has a
 *   fixed size worked out from these numbers, so memory use is known at
 *   build time and nothing can run out at the table. The limits also stop
 *   runaway rolls: "1d1 explode forever" stops at QD_MAX_EXPLOSIONS instead
 *   of hanging the device.
 *
 * The defaults are sized about ten times past what a busy table needs.
 */
#ifndef QD_CONFIG_H
#define QD_CONFIG_H

/* --- Rolls --------------------------------------------------------------- */

/** Dice kept individually in a result (face, value, flags for each). Past
 *  this a roll switches to summary mode: totals, counts per face, min, max
 *  and mean stay exact; the per-die list stops growing. */
#ifndef QD_MAX_KEPT_DICE
#define QD_MAX_KEPT_DICE        100
#endif

/** Dice rolled in one roll, explosions included. A roll that would need
 *  more is refused with an error before it starts. */
#ifndef QD_MAX_ROLL_DICE
#define QD_MAX_ROLL_DICE        1000
#endif

/** Extra dice one die may add by exploding. The chain stops here and the
 *  result is flagged. */
#ifndef QD_MAX_EXPLOSIONS
#define QD_MAX_EXPLOSIONS       100
#endif

/** Matching sets reported per pool ("sets": pairs, triples ...). */
#ifndef QD_MAX_SETS
#define QD_MAX_SETS             8
#endif

/** User-defined buckets ("<3", "=6", ...) per roll. */
#ifndef QD_MAX_BUCKETS
#define QD_MAX_BUCKETS          8
#endif

/** Pools (dice of one kind) in one roll: "1d20 + 1d4 + 2d6" is three. */
#ifndef QD_MAX_POOLS
#define QD_MAX_POOLS            8
#endif

/** Explode / implode / reroll steps on one pool. */
#ifndef QD_MAX_POOL_STEPS
#define QD_MAX_POOL_STEPS       4
#endif

/** Numbers, pool totals and operators in a roll's sum. */
#ifndef QD_MAX_ROLL_OPS
#define QD_MAX_ROLL_OPS         24
#endif

/** Digits in digit dice (dd666 has three). */
#ifndef QD_MAX_DIGITS
#define QD_MAX_DIGITS           4
#endif

/** Characters in a pool's group name ("stress"). */
#ifndef QD_MAX_NAME
#define QD_MAX_NAME             7
#endif

/* --- Notation (the parser) ---------------------------------------------- */

/** Characters in a roll's label: "(Sneak attack) 1d20+7". */
#ifndef QD_MAX_ROLL_LABEL
#define QD_MAX_ROLL_LABEL       23
#endif

/** Characters in a roll's comment: "1d20+7 ! flanking the ogre". */
#ifndef QD_MAX_COMMENT
#define QD_MAX_COMMENT          47
#endif

/** Most repeats listed one by one in "6 x 4d6k3", and most rolls in a
 *  set "(6 x 4d6) k3". */
#ifndef QD_MAX_REPEAT
#define QD_MAX_REPEAT           20
#endif

/** Most repeats of a mass roll ("400 x atk vs 12"): counted, not listed. */
#ifndef QD_MAX_MASS
#define QD_MAX_MASS             1000
#endif

/** Characters in a custom die's name: "2d{Loaded d6}". */
#ifndef QD_MAX_DIE_NAME
#define QD_MAX_DIE_NAME         15
#endif

/** Brackets inside brackets: "((1d6+1)*2)" is two deep. */
#ifndef QD_MAX_NESTING
#define QD_MAX_NESTING          8
#endif

/* --- Dice in text (qd_text.h) -------------------------------------------- */

/** Inline rolls kept as the working of one qd_expand() call. */
#ifndef QD_EXPAND_ROLLS
#define QD_EXPAND_ROLLS         16
#endif

/* --- Stats for nerds and odds ------------------------------------------- */

/** Kinds of die tracked by stats (d6, d20, a custom die ...). */
#ifndef QD_STATS_KINDS
#define QD_STATS_KINDS          16
#endif

/** Faces counted one by one per kind (bigger dice: totals only). */
#ifndef QD_STATS_FACES
#define QD_STATS_FACES          100
#endif

/** Exact odds: most rolls tried one by one before simulating instead. */
#ifndef QD_ODDS_EXACT_RUNS
#define QD_ODDS_EXACT_RUNS      50000
#endif

/** Simulated odds: rolls to try ... */
#ifndef QD_ODDS_SAMPLES
#define QD_ODDS_SAMPLES         10000
#endif

/** ... unless they would need more dice than this altogether. */
#ifndef QD_ODDS_DICE_BUDGET
#define QD_ODDS_DICE_BUDGET     500000L
#endif

/** Exact sums: widest range of totals worked out (4 float buffers of it). */
#ifndef QD_ODDS_SPAN
#define QD_ODDS_SPAN            1024
#endif

/** Values in an odds result's distribution. */
#ifndef QD_ODDS_BINS
#define QD_ODDS_BINS            256
#endif

/* --- Dice and decks ------------------------------------------------------ */

/** Faces on a custom die (matches the Dice Roller settings store). */
#ifndef QD_MAX_FACES
#define QD_MAX_FACES            30
#endif

/** Cards in one deck: tarot (78), doubled decks, custom decks. */
#ifndef QD_MAX_CARDS
#define QD_MAX_CARDS            255
#endif

/** Characters in a face label or card label. */
#ifndef QD_MAX_LABEL
#define QD_MAX_LABEL            4
#endif

#endif /* QD_CONFIG_H */
