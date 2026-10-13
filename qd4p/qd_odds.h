/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_odds.h
 * @brief   What are the odds? The chance of every result of a roll, and of
 *          passing its target or getting each of its game outcomes.
 *
 *     qd_parsed_t p;
 *     qd_odds_t odds;
 *     qd_parse("atk adv + 5 vs 15", &p, &err);
 *     qd_odds(&p.roll, &odds);
 *     // odds.pass 0.7975, odds.exact true, odds.mean 18.825
 *     // odds.level[QD_LVL_CRIT + 4] 0.0975: the chance of a critical hit
 *
 * HOW: three ways, the first that fits:
 *   1. Sums and success counts of plain dice (no keep, advantage,
 *      exploding or rerolls) are worked out exactly, die by die
 *      (convolution). Fast even for dozens of dice.
 *   2. Anything else small enough is worked out exactly by trying every
 *      possible roll once, each weighted by its chance: 4d6k3 is 1,296
 *      rolls, 1d20 adv 400, coc 65 b2 1,000. Up to QD_ODDS_EXACT_RUNS.
 *   3. Otherwise it is simulated: QD_ODDS_SAMPLES rolls (fewer for very
 *      big rolls), and the answer is marked approximate. Exploding dice
 *      that can explode again (ie) are always simulated.
 * Working out odds never changes the rolls that come after: the generator
 * is put back as it was, and nothing is counted in stats for nerds.
 *
 * THE VALUE: the total, or the successes for a roll that counts them.
 * For a roll set ("6 x 4d6k3") the odds are for one roll.
 */
#ifndef QD_ODDS_H
#define QD_ODDS_H

#include <stdbool.h>
#include <stdint.h>

#include "qd_config.h"
#include "qd_roll.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    QD_ODDS_EXACT_SUM = 1,              /**< exact, by convolution */
    QD_ODDS_EXACT_ALL,                  /**< exact, by trying every roll */
    QD_ODDS_SIMULATED,                  /**< approximate, by simulation */
};

typedef struct {
    uint8_t  method;                    /**< QD_ODDS_* */
    bool     exact;
    uint32_t runs;                      /**< rolls tried (0 for a convolution) */
    bool     counting;                  /**< the value is successes, not the total */
    double   mean, sd;                  /**< average value and its spread */
    int32_t  lo, hi;                    /**< lowest and highest value possible (simulated: seen) */
    bool     judged;                    /**< a target or game system: pass and level mean something */
    double   pass;                      /**< chance of passing */
    double   level[9];                  /**< chance of outcome level L at [L + 4] (L = -4..4) */
    int32_t  first;                     /**< dist[0] is the chance of the value `first` */
    uint16_t bins;                      /**< values in dist */
    float    dist[QD_ODDS_BINS];        /**< chance of each value first .. first + bins - 1 */
    double   below, above;              /**< chance of values outside dist (very wide rolls) */
} qd_odds_t;

/** Work out the odds of a roll. */
qd_err_t qd_odds(const qd_roll_t *r, qd_odds_t *out);

/** The chance the value is `v` or more (exact within dist; outside it the
 *  tails are counted whole). */
double qd_odds_at_least(const qd_odds_t *o, int32_t v);

/** ... and the chance it is exactly `v`. */
double qd_odds_exactly(const qd_odds_t *o, int32_t v);

#ifdef __cplusplus
}
#endif

#endif /* QD_ODDS_H */
