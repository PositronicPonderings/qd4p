/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_parse.h
 * @brief   Dice notation: text in, a roll ready for qd_roll_run() out.
 *
 *     qd_parsed_t p;
 *     qd_parse_error_t err;
 *     qd_result_t res;
 *
 *     if (qd_parse("4d6k3 + 2", &p, &err))
 *         qd_roll_run(&p.roll, &res);
 *     else
 *         printf("column %d: %s\n", err.pos + 1, err.message);
 *
 * Parse once, run as often as you like. The full notation, with examples
 * of everything, is in docs/notation.md. In short:
 *
 *     4d6  d20  d%  4dF  dd66  2d{Loaded}        dice
 *     + - * / ( )                                maths, usual precedence
 *     adv1 dis1                                  advantage, disadvantage
 *     e e5 ie ie5   i i2   r r2 ir2 rg5 irg5     explode, implode, reroll
 *     k3 kl3 km3 d1 dh1                          keep, drop
 *     t8 tl3 ds10 f1 b1                          successes, failures, botches
 *     [<3, =6, ="+", 3-5]                        buckets
 *     base: stress:                              group names
 *     (Label)   ! comment   6 x roll   roll ; roll
 *     (6 x 4d6) k3   (7 x 4d6k3) k6             a set: keep or drop whole rolls
 *     6 x 4d6k3                                  x binds loosest: six rolls of 4d6k3
 *
 * Spaces are optional and case doesn't matter: "4D6K3" is "4d6 k3".
 * After dice, "d1" means "drop the lowest 1": write "+1d1" for a die.
 */
#ifndef QD_PARSE_H
#define QD_PARSE_H

#include <stdbool.h>
#include <stdint.h>

#include "qd_config.h"
#include "qd_roll.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One parsed roll: the roll itself plus what was written around it. */
typedef struct {
    qd_roll_t roll;
    uint16_t  repeat;                           /**< "6 x 4d6k3": 6. Otherwise 1. Over
                                                     QD_MAX_REPEAT: a mass roll */
    char      label[QD_MAX_ROLL_LABEL + 1];     /**< "(Bite) 1d20+5": "Bite" */
    char      comment[QD_MAX_COMMENT + 1];      /**< "1d20 ! flanking": "flanking" */
    char      die_names[QD_MAX_POOLS][QD_MAX_DIE_NAME + 1];  /**< d{name} per pool, "" if none */
    bool      set;                              /**< "(6 x 4d6) k3": the repeats are a set */
    uint8_t   set_keep;                         /**< qd_keep_t applied to the set's rolls */
    uint16_t  set_keep_n;
} qd_parsed_t;

/** A roll set: every repeat's value (its total, or its successes if it
 *  counts them) and which were kept. */
typedef struct {
    uint8_t  count;                             /**< repeats rolled */
    bool     counting;                          /**< values are successes */
    int32_t  total;                             /**< sum of the kept values */
    int32_t  value[QD_MAX_REPEAT];
    bool     kept[QD_MAX_REPEAT];
} qd_set_result_t;

/** Called with each repeat's full result as it is rolled (index from 0). */
typedef void (*qd_set_each_t)(int index, const qd_result_t *res, void *ctx);

/** Where and what the problem is. pos counts from 0, so show pos + 1 as a
 *  column. len is how many characters to underline (at least 1). */
typedef struct {
    uint16_t pos, len;
    char     message[64];
} qd_parse_error_t;

/** Custom dice by name: "2d{Loaded}" asks this function for "Loaded".
 *  Return the die (it must stay alive while the roll is used) or NULL if
 *  there is no such die. Names match however the app likes; the parser
 *  passes the text between the braces, spaces trimmed. */
typedef const qd_die_t *(*qd_die_lookup_t)(const char *name, void *ctx);

/** Set the lookup for d{name}. NULL turns custom dice off. */
void qd_parse_set_lookup(qd_die_lookup_t fn, void *ctx);

/** Parse one roll. A ';' in the text is an error: use qd_parse_line. */
bool qd_parse(const char *text, qd_parsed_t *out, qd_parse_error_t *err);

/** Parse up to `max` rolls separated by ';' ("2d6 ; 3d8 ; 1d20").
 *  Returns how many, or -1 with `err` filled in. */
int qd_parse_line(const char *text, qd_parsed_t *out, int max, qd_parse_error_t *err);

/** Roll every repeat of a parsed roll ("6 x 4d6k3": six; a plain roll:
 *  one), then apply the set's keep or drop ("(7 x 4d6k3) k6": the best
 *  six). Ties: as for dice, "lowest" is the earliest and "highest" the
 *  latest. `each` may be NULL. */
qd_err_t qd_set_run(const qd_parsed_t *p, qd_set_result_t *out, qd_set_each_t each, void *ctx);

/** A mass roll: many repeats counted rather than listed ("400 x atk vs 12"). */
typedef struct {
    uint16_t count;                             /**< rolls made */
    bool     counting;                          /**< values are successes */
    int64_t  sum;                               /**< of the values */
    int32_t  lo, hi;                            /**< lowest and highest value */
    bool     judged;                            /**< a target or system: passes and levels count */
    uint16_t passes;
    uint16_t level[9];                          /**< rolls with outcome level L at [L + 4] */
} qd_mass_result_t;

/** Roll a parsed roll `repeat` times (up to QD_MAX_MASS) and count: the
 *  sum, lowest and highest value, passes and each outcome level. The same
 *  rules as one roll, just many of them: a horde's attacks, a volley. */
qd_err_t qd_mass_run(const qd_parsed_t *p, qd_mass_result_t *out);

/** Write a parsed roll back as tidy notation ("(Bite) 1d20 + 5 ! note").
 *  Parsing what it writes gives the same roll. Returns the length. */
int qd_format(const qd_parsed_t *p, char *buf, int size);

/** Just one pool, as notation ("4d6k3", "base:4d6t6"). */
int qd_format_pool(const qd_parsed_t *p, int pool, char *buf, int size);

#ifdef __cplusplus
}
#endif

#endif /* QD_PARSE_H */
