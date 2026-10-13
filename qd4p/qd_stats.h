/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_stats.h
 * @brief   Stats for nerds: every die rolled, tallied by kind, and a
 *          "is this die behaving?" check.
 *
 *     static qd_stats_t stats;
 *     qd_stats_clear(&stats);
 *     qd_stats_watch(&stats);            // from now on, every die is counted
 *     ...                                // roll as usual
 *     qd_stats_report_t rep;
 *     qd_stats_report(&stats.kind[0], &rep);
 *     // rep.mean 10.62 vs rep.expect_mean 10.5, rep.verdict QD_STATS_FAIR
 *
 * WHAT COUNTS: every die the roll engine rolls - kept, dropped, set aside,
 * exploded, rerolled - because every one of them is a real roll of that
 * die. Percentile dice count as two d10s, digit dice (dd66) as one die per
 * digit. Dice are told apart by their sides, or by which custom die they
 * are (a loaded d6 is not a d6).
 *
 * THE CHECK: a chi-square test of the face counts against what a fair (or,
 * for a weighted die, a correctly weighted) die would give. A fair die
 * fails it about 1 time in 1,000, so one "odd" is a curiosity; the same
 * die odd again and again is worth a look. It needs about 5 rolls per face
 * before it says anything (30 for a d6, 100 for a d20).
 */
#ifndef QD_STATS_H
#define QD_STATS_H

#include <stdbool.h>
#include <stdint.h>

#include "qd_config.h"
#include "qd_dice.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One kind of die. */
typedef struct {
    uint16_t       sides;               /**< faces */
    const qd_die_t *die;                /**< the custom die, or NULL for a standard one */
    uint32_t       count;               /**< rolls */
    int64_t        sum;
    uint64_t       sum_sq;              /**< sum of value squared */
    int16_t        min, max;            /**< lowest and highest seen */
    bool           faces;               /**< face[] is kept (sides <= QD_STATS_FACES) */
    uint32_t       face[QD_STATS_FACES];/**< rolls per face index */
} qd_die_stats_t;

typedef struct {
    uint8_t        kinds;
    uint32_t       untracked;           /**< rolls of kinds past QD_STATS_KINDS */
    qd_die_stats_t kind[QD_STATS_KINDS];
} qd_stats_t;

/** Start again from nothing. */
void qd_stats_clear(qd_stats_t *s);

/** Count every die the engine rolls into `s` (NULL stops). Rolls made
 *  while working out odds are never counted. */
void qd_stats_watch(qd_stats_t *s);

/** Count one die by hand (what qd_stats_watch does for each die). */
void qd_stats_add(qd_stats_t *s, uint16_t sides, const qd_die_t *die, uint16_t face, int16_t value);

/** The kind for a standard die of `sides` (die NULL) or a custom die, or
 *  NULL if it hasn't been rolled. */
const qd_die_stats_t *qd_stats_find(const qd_stats_t *s, uint16_t sides, const qd_die_t *die);

enum {
    QD_STATS_TOO_FEW = 0,               /**< not enough rolls to judge yet */
    QD_STATS_FAIR,                      /**< behaving */
    QD_STATS_ODD,                       /**< the counts are unlikely for a fair die (p < 0.001) */
    QD_STATS_NO_FACES,                  /**< too many sides to check face by face */
};

typedef struct {
    double   mean, expect_mean;         /**< average roll, and what a fair die averages */
    double   sd, expect_sd;             /**< spread, and a fair die's */
    double   chi, limit;                /**< chi-square, and the p = 0.001 limit */
    int      df;                        /**< degrees of freedom (faces - 1) */
    uint32_t highest, lowest;           /**< rolls of the top and bottom face ("natural 20s") */
    uint8_t  verdict;                   /**< QD_STATS_* */
} qd_stats_report_t;

/** Work out the report for one kind. */
void qd_stats_report(const qd_die_stats_t *k, qd_stats_report_t *r);

/** The chi-square value a fair die exceeds 1 time in 1,000, for `df`
 *  degrees of freedom: table values up to 30, then the Wilson-Hilferty
 *  approximation (within 0.3%). */
double qd_chi_limit(int df);

#ifdef __cplusplus
}
#endif

#endif /* QD_STATS_H */
