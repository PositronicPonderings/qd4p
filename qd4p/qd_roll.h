/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_roll.h
 * @brief   The roll engine: pools of dice, modifiers, maths, counting.
 *
 * QUICK START (no parser needed)
 *
 *     qd_roll_t roll;
 *     qd_result_t res;
 *
 *     qd_roll_init(&roll);
 *     qd_pool_t *p = qd_roll_dice(&roll, 4, 6);         // 4d6
 *     qd_pool_keep(p, QD_KEEP_HIGH, 3);                  // k3
 *     qd_roll_run(&roll, &res);                          // res.total 3..18
 *
 *     qd_roll_init(&roll);                                // 10d6 e6 k8 +4
 *     p = qd_roll_dice(&roll, 10, 6);
 *     qd_pool_explode(p, qd_test_ge(6), false);
 *     qd_pool_keep(p, QD_KEEP_HIGH, 8);
 *     qd_roll_add(&roll, 4);
 *
 *     qd_roll_init(&roll);                                // 4d10 t8 ie10 f1
 *     p = qd_roll_dice(&roll, 4, 10);
 *     qd_pool_success(p, qd_test_ge(8));
 *     qd_pool_explode(p, qd_test_max(), true);
 *     qd_pool_failure(p, qd_test_le(1));
 *     // res.successes is successes minus failures
 *
 * A ROLL is one or more POOLS (dice of one kind, with their modifiers) and
 * a little sum that combines the pool totals with numbers: "3d6 + 1d4 + 2".
 * Build it once, run it as often as you like; every run is a fresh roll.
 * The parser (D4) builds exactly the same thing from text.
 *
 * STAGES, ALWAYS IN THIS ORDER, whatever order the modifiers were added in
 *   1. Roll the pool, plus one extra die per level of advantage or
 *      disadvantage; then set aside that many lowest (advantage) or highest
 *      (disadvantage) dice. Set-aside dice never explode.
 *   2. Explode, implode and reroll steps, in the order they were added.
 *      Each step looks at every die still in play.
 *   3. Keep or drop. This can keep or drop a die that came from an
 *      explosion.
 *   4. Pool total = kept dice minus imploded dice.
 *   5. The sum: pool totals and numbers, + - * /.
 *   6. Counting (successes, failures, botches) and buckets, over the kept
 *      dice. Imploded dice are never counted.
 *
 * WORDS
 *   explode   a die that matches rolls one more die, which joins the pool.
 *             Once (e): the new die doesn't explode. Forever (ie): it can,
 *             up to QD_MAX_EXPLOSIONS extra dice per die.
 *   implode   a die that matches rolls one more die, which is SUBTRACTED
 *             from the total. It never joins the pool: it can't be kept,
 *             dropped or counted. (Cyberpunk: 1d10 i1 e10.)
 *   reroll    a die that matches is set aside and rolled again. Once (r),
 *             or until it stops matching (ir).
 *   keep/drop k3 keeps the 3 highest, kl2 the 2 lowest, km2 the 2 middle,
 *             d1 drops the lowest, dh1 the highest.
 *
 * TIES: when dice tie, "lowest" picks the earliest-rolled of them and
 * "highest" the latest-rolled. Totals don't depend on it; which die is
 * marked dropped does, and this makes it the same every time.
 *
 * MEMORY: no malloc. A qd_roll_t is about 0.6 KB and a qd_result_t about
 * 2 KB. The engine also has one working buffer of QD_MAX_ROLL_DICE dice
 * (10 KB) shared by all rolls, so run rolls from one core at a time.
 */
#ifndef QD_ROLL_H
#define QD_ROLL_H

#include <stdbool.h>
#include <stdint.h>

#include "qd_config.h"
#include "qd_dice.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Tests: "is this die a 6?", "is it 3 or lower?"
 * ======================================================================== */

typedef enum {
    QD_TEST_NONE = 0,   /**< never matches; "not set" */
    QD_TEST_GE,         /**< value >= a */
    QD_TEST_LE,         /**< value <= a */
    QD_TEST_GT,         /**< value >  a */
    QD_TEST_LT,         /**< value <  a */
    QD_TEST_EQ,         /**< value == a */
    QD_TEST_NE,         /**< value != a */
    QD_TEST_RANGE,      /**< a <= value <= b */
    QD_TEST_MAX,        /**< the die's highest face (a natural 20 on a d20) */
    QD_TEST_MIN,        /**< the die's lowest face */
    QD_TEST_LABEL,      /**< the face's label is `label` (custom dice) */
} qd_test_op_t;

typedef struct {
    uint8_t op;                         /**< a qd_test_op_t */
    int16_t a, b;
    char    label[QD_MAX_LABEL + 1];
} qd_test_t;

qd_test_t qd_test_ge(int a);
qd_test_t qd_test_le(int a);
qd_test_t qd_test_gt(int a);
qd_test_t qd_test_lt(int a);
qd_test_t qd_test_eq(int a);
qd_test_t qd_test_ne(int a);
qd_test_t qd_test_range(int a, int b);
qd_test_t qd_test_max(void);
qd_test_t qd_test_min(void);
qd_test_t qd_test_label(const char *label);

/** Does a face pass the test? `label` may be "" and `lo`/`hi` are the
 *  die's lowest and highest values (for QD_TEST_MIN / QD_TEST_MAX). */
bool qd_test_face(const qd_test_t *t, int16_t value, const char *label, int16_t lo, int16_t hi);

/** The test as short text: ">=6", "<=1", "=max", "3-5", "\"+\"". */
char *qd_test_text(const qd_test_t *t, char *buf, int size);

/* ========================================================================
 * Pools
 * ======================================================================== */

typedef enum {
    QD_STEP_EXPLODE = 1,
    QD_STEP_IMPLODE,
    QD_STEP_REROLL,
} qd_step_kind_t;

typedef struct {
    uint8_t   kind;                     /**< a qd_step_kind_t */
    bool      forever;                  /**< ie / ir: keep going */
    qd_test_t test;
} qd_step_t;

typedef enum {
    QD_KEEP_ALL = 0,
    QD_KEEP_HIGH,                       /**< k3: keep the 3 highest */
    QD_KEEP_LOW,                        /**< kl3 */
    QD_KEEP_MIDDLE,                     /**< km3 */
    QD_DROP_LOW,                        /**< d1: drop the lowest */
    QD_DROP_HIGH,                       /**< dh1 */
} qd_keep_t;

/** Dice of one kind and what to do with them. Made by qd_roll_dice() and
 *  friends; change it with the qd_pool_*() functions. */
typedef struct {
    uint16_t  count;                    /**< dice to roll, 1 .. QD_MAX_ROLL_DICE */
    uint16_t  sides;                    /**< standard die: faces 1..sides */
    const qd_die_t *die;                /**< custom die, or NULL (must stay alive) */
    uint8_t   digits;                   /**< digit dice: how many digits (0 = not) */
    uint8_t   digit_sides[QD_MAX_DIGITS];/**< digit dice: sides of each, left to right */
    int8_t    advantage;                /**< + advantage levels, - disadvantage */
    uint8_t   step_count;
    qd_step_t steps[QD_MAX_POOL_STEPS]; /**< explode / implode / reroll, in order */
    uint8_t   keep;                     /**< a qd_keep_t */
    uint16_t  keep_n;
    qd_test_t success, twice, failure, botch;   /**< counting; op NONE = off */
    bool      hidden;                   /**< DM-only: apps hide it from players */
    char      name[QD_MAX_NAME + 1];    /**< group name, "" if none ("base", "stress") */
} qd_pool_t;

/** Advantage (n > 0) or disadvantage (n < 0): roll |n| extra dice and set
 *  aside the |n| lowest or highest before anything explodes. */
void qd_pool_advantage(qd_pool_t *p, int n);

/** Add an explode, implode or reroll step (up to QD_MAX_POOL_STEPS).
 *  Returns false if the pool has no room. */
bool qd_pool_explode(qd_pool_t *p, qd_test_t when, bool forever);
bool qd_pool_implode(qd_pool_t *p, qd_test_t when);
bool qd_pool_reroll(qd_pool_t *p, qd_test_t when, bool forever);

/** Keep or drop n dice (stage 3). Keeping more dice than there are keeps them all. */
void qd_pool_keep(qd_pool_t *p, qd_keep_t how, uint16_t n);

/** Counting. Each kept die passing `success` is a success; passing `twice`
 *  as well makes it two (Exalted's 10s). Each passing `failure` takes one
 *  success away (World of Darkness 1s). Botches are counted on their own. */
void qd_pool_success(qd_pool_t *p, qd_test_t t);
void qd_pool_twice(qd_pool_t *p, qd_test_t t);
void qd_pool_failure(qd_pool_t *p, qd_test_t t);
void qd_pool_botch(qd_pool_t *p, qd_test_t t);

/* ========================================================================
 * Rolls
 * ======================================================================== */

/** The sum is kept as a short program: numbers and pool totals are
 *  pushed, operators combine the top two. "3d6 + 2 * 2" in the order the
 *  builder adds things is (3d6 + 2) * 2; the parser uses the push
 *  functions directly to get brackets and precedence right. */
typedef enum {
    QD_OP_POOL = 1,                     /**< push pool[arg]'s total */
    QD_OP_NUMBER,                       /**< push arg */
    QD_OP_ADD, QD_OP_SUB, QD_OP_MUL, QD_OP_DIV,
    QD_OP_NEG,                          /**< negate the top */
} qd_op_kind_t;

typedef struct {
    uint8_t kind;                       /**< a qd_op_kind_t */
    int32_t arg;
} qd_op_t;

/** A bucket: count the kept dice that pass a test. Dice can land in more
 *  than one bucket; dice in none are counted in "everything else". */
typedef struct {
    qd_test_t test;
} qd_bucket_t;

typedef struct {
    uint8_t     pool_count;
    qd_pool_t   pools[QD_MAX_POOLS];
    uint8_t     op_count;
    qd_op_t     ops[QD_MAX_ROLL_OPS];
    uint8_t     bucket_count;
    qd_bucket_t buckets[QD_MAX_BUCKETS];
    uint8_t     error;                  /**< set by a builder that ran out of room */
} qd_roll_t;

/** Start an empty roll. */
void qd_roll_init(qd_roll_t *r);

/** Add NdS to the total (returns the new pool, or NULL if the roll is full). */
qd_pool_t *qd_roll_dice(qd_roll_t *r, uint16_t count, uint16_t sides);

/** Subtract NdS from the total ("1d20 - 1d4"). */
qd_pool_t *qd_roll_dice_minus(qd_roll_t *r, uint16_t count, uint16_t sides);

/** Add N of a custom die (Fudge, loaded, labelled). The die must stay alive. */
qd_pool_t *qd_roll_custom(qd_roll_t *r, uint16_t count, const qd_die_t *die);

/** Add N digit dice: "66" is a tens d6 and a units d6 read as 11..66,
 *  "666" three d6 read as 111..666, "36" a d3 then a d6. Each digit's die
 *  has 1 to 9 sides (a 1 always shows 1, and earns a warning); up to
 *  QD_MAX_DIGITS digits. Digit dice can be kept,
 *  dropped and counted but not exploded, imploded or rerolled. */
qd_pool_t *qd_roll_digits(qd_roll_t *r, uint16_t count, const char *digits);

/** Arithmetic on the running total: + n (n may be negative), * n, / n.
 *  Division rounds toward zero, like C: 7 / 2 is 3, -7 / 2 is -3. */
void qd_roll_add(qd_roll_t *r, int32_t n);
void qd_roll_mul(qd_roll_t *r, int32_t n);
void qd_roll_div(qd_roll_t *r, int32_t n);

/** Add a bucket (up to QD_MAX_BUCKETS). */
bool qd_roll_bucket(qd_roll_t *r, qd_test_t t);

/** Low level, for the parser: add a pool without touching the sum, and
 *  push to the sum program yourself. */
qd_pool_t *qd_roll_new_pool(qd_roll_t *r);
bool qd_roll_push(qd_roll_t *r, qd_op_kind_t kind, int32_t arg);

/* ========================================================================
 * Results
 * ======================================================================== */

/** Flags on one die in a result. */
enum {
    QD_DIE_DROPPED   = 1 << 0,          /**< not kept (keep/drop) */
    QD_DIE_SET_ASIDE = 1 << 1,          /**< discarded by advantage/disadvantage */
    QD_DIE_REROLLED  = 1 << 2,          /**< replaced by a reroll; doesn't count */
    QD_DIE_EXPLODED  = 1 << 3,          /**< this die came from an explosion */
    QD_DIE_IMPLODED  = 1 << 4,          /**< this die is subtracted; never counted */
    QD_DIE_REPLACEMENT = 1 << 5,        /**< this die came from a reroll */
    QD_DIE_MAX       = 1 << 6,          /**< highest face (natural 20) */
    QD_DIE_MIN       = 1 << 7,          /**< lowest face (natural 1) */
    QD_DIE_SUCCESS   = 1 << 8,
    QD_DIE_TWICE     = 1 << 9,          /**< counted as two successes */
    QD_DIE_FAILURE   = 1 << 10,
    QD_DIE_BOTCH     = 1 << 11,
    QD_DIE_CAPPED    = 1 << 12,         /**< its chain hit QD_MAX_EXPLOSIONS */
};

/** Dice not in play: they don't count toward the total, successes or buckets. */
#define QD_DIE_OUT (QD_DIE_DROPPED | QD_DIE_SET_ASIDE | QD_DIE_REROLLED | QD_DIE_IMPLODED)

#define QD_NO_PARENT 0xFFFF

/** One die in a result. */
typedef struct {
    int16_t  value;
    uint16_t face;                      /**< face index (digit dice: the value) */
    uint16_t flags;                     /**< QD_DIE_* */
    uint16_t parent;                    /**< the die (index within its pool) that
                                             exploded, imploded or was rerolled
                                             into this one; QD_NO_PARENT if none */
    uint8_t  pool;
} qd_die_result_t;

/** One pool's share of a result. */
typedef struct {
    int32_t  total;                     /**< kept dice minus imploded dice */
    uint16_t rolled;                    /**< every die rolled, set-aside and rerolled included */
    uint16_t kept;                      /**< dice in play after keep/drop */
    int16_t  min, max;                  /**< lowest and highest kept die */
    int16_t  successes;                 /**< doubles counted twice, failures not taken off */
    int16_t  failures, botches;
    uint16_t first;                     /**< where its dice start in qd_result_t.dice */
    uint16_t listed;                    /**< how many of its dice are listed there */
    bool     face_counts;               /**< face_count is filled in (<= QD_MAX_FACES faces) */
    uint16_t face_count[QD_MAX_FACES];  /**< kept dice per face index */
} qd_pool_result_t;

/** Result flags. */
enum {
    QD_RES_SUMMARY  = 1 << 0,           /**< more than QD_MAX_KEPT_DICE dice: only the
                                             first ones are listed; every number is exact */
    QD_RES_CAPPED   = 1 << 1,           /**< an explosion or reroll chain was cut short */
    QD_RES_DIV_ZERO = 1 << 2,           /**< divided by zero; that step gave 0 */
    QD_RES_CLAMPED  = 1 << 3,           /**< the total didn't fit in 32 bits */
    QD_RES_COUNTING = 1 << 4,           /**< some pool counts successes: show those */
};

typedef struct {
    int32_t  total;                     /**< the sum */
    int16_t  successes;                 /**< all pools: successes minus failures */
    int16_t  failures, botches;
    uint16_t flags;                     /**< QD_RES_* */
    uint16_t warnings;                  /**< QD_WARN_*: rolled, but worth a note */
    uint16_t rolled;                    /**< every die rolled */
    uint8_t  pool_count;
    qd_pool_result_t pools[QD_MAX_POOLS];
    uint8_t  bucket_count;
    uint16_t buckets[QD_MAX_BUCKETS + 1];   /**< [bucket_count] = everything else */
    uint16_t dice_count;                /**< dice listed below */
    qd_die_result_t dice[QD_MAX_KEPT_DICE];
} qd_result_t;

/** Warnings: the roll is allowed, but probably not what was meant. */
enum {
    QD_WARN_ONE_SIDED = 1 << 0,         /**< a d1 (or a 1 in digit dice) always shows 1 */
    QD_WARN_KEEP_NONE = 1 << 1,         /**< k0 / kl0 / km0 keeps nothing */
};

/** What went wrong. */
typedef enum {
    QD_OK = 0,
    QD_ERR_FULL,                        /**< a builder ran out of room (pools, steps, ops, buckets) */
    QD_ERR_NO_DICE,                     /**< a pool with 0 dice or a 0-sided die */
    QD_ERR_TOO_MANY_DICE,               /**< more than QD_MAX_ROLL_DICE before exploding */
    QD_ERR_DIGITS,                      /**< bad digit dice, or exploding/rerolling them */
    QD_ERR_FOREVER,                     /**< ie / ir that matches every face */
    QD_ERR_SUM,                         /**< the sum program doesn't add up */
    QD_ERR_DIV_ZERO,                    /**< divide by the number 0 */
} qd_err_t;

/** A short description of an error, for showing to a person. */
const char *qd_err_text(qd_err_t e);

/** Warnings for a roll (also put in qd_result_t.warnings by qd_roll_run). */
uint16_t qd_roll_warnings(const qd_roll_t *r);

/** Text for the first warning set in `w`, or "" if none. */
const char *qd_warn_text(uint16_t w);

/** Check a roll without rolling it. */
qd_err_t qd_roll_check(const qd_roll_t *r);

/** Roll it. On an error `res` is cleared and nothing is rolled. */
qd_err_t qd_roll_run(const qd_roll_t *r, qd_result_t *res);

/** The lowest and highest single value a pool's die can show. */
void qd_pool_range(const qd_pool_t *p, int16_t *lo, int16_t *hi);

#ifdef __cplusplus
}
#endif

#endif /* QD_ROLL_H */
