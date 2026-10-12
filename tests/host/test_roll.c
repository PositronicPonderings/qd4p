/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_roll.c
 * @brief   D3: the roll engine - stages, sums, counting, buckets, limits.
 *
 * The goldens come from rng_reference.py, which run_tests.sh runs first to
 * write build/roll_goldens.h: 17 kinds of pool x 25 seeds, compared die by
 * die (value, flags, which die it came from) and on every total.
 */
#include <math.h>
#include <string.h>

#include "qd4p.h"
#include "qd_test.h"

typedef struct {
    int spec, seed, n, total, successes, failures;
    const int16_t *values;
    const uint16_t *flags, *parents;
} golden_t;

#include "roll_goldens.h"

static const qd_face_t loaded_faces[6] = {
    { 1, 1, "" }, { 2, 1, "" }, { 3, 1, "" }, { 4, 1, "" }, { 5, 1, "" }, { 6, 3, "" },
};
static qd_die_t loaded;

/* The same pools as GOLDEN_POOLS in rng_reference.py, in the same order. */
static void build_golden(int spec, qd_roll_t *r)
{
    qd_roll_init(r);
    qd_pool_t *p = NULL;
    switch (spec) {
    case 0:  p = qd_roll_dice(r, 4, 6);  qd_pool_keep(p, QD_KEEP_HIGH, 3); break;
    case 1:  p = qd_roll_dice(r, 10, 6); qd_pool_explode(p, qd_test_ge(6), false);
             qd_pool_keep(p, QD_KEEP_HIGH, 8); break;
    case 2:  p = qd_roll_dice(r, 6, 6);  qd_pool_explode(p, qd_test_max(), true); break;
    case 3:  p = qd_roll_dice(r, 8, 10); qd_pool_success(p, qd_test_ge(8));
             qd_pool_failure(p, qd_test_le(1)); qd_pool_explode(p, qd_test_max(), true); break;
    case 4:  p = qd_roll_dice(r, 2, 20); qd_pool_advantage(p, 1); break;
    case 5:  p = qd_roll_dice(r, 3, 20); qd_pool_advantage(p, -2); break;
    case 6:  p = qd_roll_dice(r, 6, 6);  qd_pool_reroll(p, qd_test_le(2), false); break;
    case 7:  p = qd_roll_dice(r, 6, 6);  qd_pool_reroll(p, qd_test_le(2), true); break;
    case 8:  p = qd_roll_dice(r, 4, 10); qd_pool_implode(p, qd_test_min());
             qd_pool_explode(p, qd_test_max(), false); break;
    case 9:  p = qd_roll_dice(r, 5, 10); qd_pool_keep(p, QD_KEEP_MIDDLE, 3); break;
    case 10: p = qd_roll_dice(r, 6, 10); qd_pool_success(p, qd_test_ge(7));
             qd_pool_twice(p, qd_test_ge(10)); break;
    case 11: p = qd_roll_digits(r, 5, "66"); qd_pool_keep(p, QD_DROP_HIGH, 1); break;
    case 12: p = qd_roll_dice(r, 12, 4); qd_pool_explode(p, qd_test_max(), true); break;
    case 13: p = qd_roll_dice(r, 10, 6); qd_pool_reroll(p, qd_test_le(3), true);
             qd_pool_keep(p, QD_KEEP_HIGH, 5); break;
    case 14: p = qd_roll_dice(r, 20, 6); qd_pool_explode(p, qd_test_ge(5), false);
             qd_pool_reroll(p, qd_test_eq(1), false); qd_pool_implode(p, qd_test_le(2));
             qd_pool_keep(p, QD_DROP_LOW, 4); break;
    case 15: p = qd_roll_custom(r, 8, &loaded); qd_pool_reroll(p, qd_test_ge(5), false);
             qd_pool_keep(p, QD_KEEP_LOW, 4); break;
    case 16: p = qd_roll_dice(r, 4, 1000); qd_pool_advantage(p, 3); break;
    }
}

static void test_goldens(void)
{
    const uint16_t tracked = QD_DIE_DROPPED | QD_DIE_SET_ASIDE | QD_DIE_REROLLED |
                             QD_DIE_EXPLODED | QD_DIE_IMPLODED | QD_DIE_REPLACEMENT;
    static qd_roll_t r;
    static qd_result_t res;
    int bad_specs = 0;
    for (size_t g = 0; g < sizeof(goldens) / sizeof(goldens[0]); g++) {
        const golden_t *x = &goldens[g];
        build_golden(x->spec, &r);
        qd_rng_seed_fixed((uint64_t)x->seed);
        qd_err_t e = qd_roll_run(&r, &res);
        bool ok = e == QD_OK && res.pools[0].rolled == x->n && res.total == x->total &&
                  res.pools[0].successes == x->successes && res.pools[0].failures == x->failures;
        for (int i = 0; ok && i < x->n && i < res.dice_count; i++)
            ok = res.dice[i].value == x->values[i] &&
                 (res.dice[i].flags & tracked) == x->flags[i] &&
                 res.dice[i].parent == x->parents[i];
        if (!ok && bad_specs++ < 5)
            printf("  golden spec %d seed %d: total %ld rolled %d (reference %d, %d)\n",
                   x->spec, x->seed, (long)res.total, res.pools[0].rolled, x->total, x->n);
        CHECK(ok, "golden spec %d seed %d matches the reference", x->spec, x->seed);
    }
    printf("  %d pool kinds x %d seeds compared die by die with rng_reference.py\n",
           GOLDEN_SPECS, GOLDEN_SEEDS);
}

/* --- Sums ---------------------------------------------------------------- */
static void test_sums(void)
{
    static qd_roll_t r;
    static qd_result_t res;

    qd_roll_init(&r);                                    /* 3d6 + 1d4 + 2 */
    qd_roll_dice(&r, 3, 6);
    qd_roll_dice(&r, 1, 4);
    qd_roll_add(&r, 2);
    int ok = 1;
    for (int i = 0; i < 2000; i++) {
        qd_roll_run(&r, &res);
        ok &= res.total == res.pools[0].total + res.pools[1].total + 2 &&
              res.total >= 6 && res.total <= 24 && res.rolled == 4;
    }
    CHECK(ok, "3d6 + 1d4 + 2 adds up, 6..24");

    qd_roll_init(&r);                                    /* (1d20 - 1d4 - 3) * 2 / 3 */
    qd_roll_dice(&r, 1, 20);
    qd_roll_dice_minus(&r, 1, 4);
    qd_roll_add(&r, -3);
    qd_roll_mul(&r, 2);
    qd_roll_div(&r, 3);
    ok = 1;
    for (int i = 0; i < 2000; i++) {
        qd_roll_run(&r, &res);
        ok &= res.total == (res.pools[0].total - res.pools[1].total - 3) * 2 / 3;
    }
    CHECK(ok, "minus dice, * and / (rounding toward zero)");

    qd_roll_init(&r);                                    /* - 1d6 on its own */
    qd_roll_dice_minus(&r, 1, 6);
    qd_roll_run(&r, &res);
    CHECK(res.total == -res.pools[0].total && res.total < 0, "a roll can start with minus dice");

    qd_roll_init(&r);                                    /* 10 / (1d1 - 1): runtime zero */
    qd_roll_push(&r, QD_OP_NUMBER, 10);
    qd_pool_t *p = qd_roll_new_pool(&r);
    p->count = 1; p->sides = 1;
    qd_roll_push(&r, QD_OP_POOL, 0);
    qd_roll_push(&r, QD_OP_NUMBER, 1);
    qd_roll_push(&r, QD_OP_SUB, 0);
    qd_roll_push(&r, QD_OP_DIV, 0);
    CHECK(qd_roll_run(&r, &res) == QD_OK && res.total == 0 && (res.flags & QD_RES_DIV_ZERO),
          "dividing by a roll of 0 gives 0 and a flag");

    qd_roll_init(&r);
    qd_roll_dice(&r, 1, 6);
    qd_roll_div(&r, 0);
    CHECK(qd_roll_check(&r) == QD_ERR_DIV_ZERO, "dividing by the number 0 is refused");

    qd_roll_init(&r);                                    /* 1000d6 * 2000000000 */
    qd_roll_dice(&r, 1000, 6);
    qd_roll_mul(&r, 2000000000);
    qd_roll_run(&r, &res);
    CHECK(res.total == INT32_MAX && (res.flags & QD_RES_CLAMPED), "huge totals are clamped and flagged");

    qd_roll_init(&r);
    qd_roll_new_pool(&r)->count = 1;
    r.pools[0].sides = 6;
    qd_roll_push(&r, QD_OP_ADD, 0);
    CHECK(qd_roll_check(&r) == QD_ERR_SUM, "an operator with nothing to add is refused");
}

/* --- Limits and errors ---------------------------------------------------- */
static void test_errors(void)
{
    static qd_roll_t r;
    static qd_result_t res;
    int ok;

    qd_roll_init(&r);
    CHECK(qd_roll_run(&r, &res) == QD_ERR_NO_DICE, "a roll with no pools is refused");
    qd_roll_init(&r);
    qd_roll_dice(&r, 0, 6);
    CHECK(qd_roll_check(&r) == QD_ERR_NO_DICE, "0d6 is refused");
    qd_roll_init(&r);
    qd_roll_dice(&r, 3, 0);
    CHECK(qd_roll_check(&r) == QD_ERR_NO_DICE, "3d0 is refused");

    qd_roll_init(&r);
    qd_roll_dice(&r, QD_MAX_ROLL_DICE, 6);
    CHECK(qd_roll_check(&r) == QD_OK, "%d dice are fine", QD_MAX_ROLL_DICE);
    qd_pool_advantage(&r.pools[0], 1);
    CHECK(qd_roll_check(&r) == QD_ERR_TOO_MANY_DICE, "one more (advantage) is refused");
    qd_roll_init(&r);
    qd_roll_dice(&r, 600, 6);
    qd_roll_dice(&r, 401, 6);
    CHECK(qd_roll_check(&r) == QD_ERR_TOO_MANY_DICE, "the limit is for the whole roll");

    qd_roll_init(&r);
    qd_pool_explode(qd_roll_dice(&r, 1, 6), qd_test_ge(1), true);
    CHECK(qd_roll_check(&r) == QD_ERR_FOREVER, "1d6 ie1 would never stop: refused");
    qd_roll_init(&r);
    qd_pool_reroll(qd_roll_dice(&r, 1, 1), qd_test_max(), true);
    CHECK(qd_roll_check(&r) == QD_ERR_FOREVER, "1d1 ir would never stop: refused");
    qd_roll_init(&r);
    qd_pool_explode(qd_roll_dice(&r, 1, 1), qd_test_max(), false);
    CHECK(qd_roll_run(&r, &res) == QD_OK && res.total == 2, "1d1 e explodes once: 2");

    qd_roll_init(&r);
    CHECK(!qd_roll_digits(&r, 1, "60") && qd_roll_check(&r) == QD_ERR_DIGITS, "dd60 is refused");
    qd_roll_init(&r);
    qd_roll_digits(&r, 20, "61");
    ok = qd_roll_run(&r, &res) == QD_OK && (res.warnings & QD_WARN_ONE_SIDED);
    for (int i = 0; ok && i < 20; i++) ok = res.dice[i].value % 10 == 1;
    CHECK(ok, "dd61 rolls 11..61 with a one-sided warning");
    qd_roll_init(&r);
    qd_roll_dice(&r, 3, 1);
    CHECK(qd_roll_run(&r, &res) == QD_OK && res.total == 3 && (res.warnings & QD_WARN_ONE_SIDED),
          "3d1 is 3, with a warning");
    qd_roll_init(&r);
    qd_pool_keep(qd_roll_dice(&r, 4, 6), QD_KEEP_HIGH, 0);
    CHECK(qd_roll_run(&r, &res) == QD_OK && res.total == 0 && (res.warnings & QD_WARN_KEEP_NONE),
          "4d6 k0 keeps nothing, with a warning");
    qd_roll_init(&r);
    qd_roll_dice(&r, 4, 6);
    CHECK(qd_roll_warnings(&r) == 0 && !*qd_warn_text(0), "an ordinary roll has no warnings");
    qd_roll_init(&r);
    CHECK(!qd_roll_digits(&r, 1, "66666") && r.error == QD_ERR_DIGITS, "five digits is refused");
    qd_roll_init(&r);
    qd_pool_explode(qd_roll_digits(&r, 1, "66"), qd_test_max(), false);
    CHECK(qd_roll_check(&r) == QD_ERR_DIGITS, "digit dice don't explode");

    qd_roll_init(&r);
    for (int i = 0; i < QD_MAX_POOLS; i++) qd_roll_dice(&r, 1, 6);
    CHECK(qd_roll_check(&r) == QD_OK && !qd_roll_dice(&r, 1, 6) &&
          qd_roll_check(&r) == QD_ERR_FULL, "%d pools fit, one more is refused", QD_MAX_POOLS);
    qd_pool_t *p = &r.pools[0];
    for (int i = 0; i < QD_MAX_POOL_STEPS; i++) qd_pool_reroll(p, qd_test_eq(1), false);
    CHECK(!qd_pool_reroll(p, qd_test_eq(1), false), "%d steps per pool", QD_MAX_POOL_STEPS);

    for (int e = 0; e <= QD_ERR_DIV_ZERO; e++)
        CHECK(strlen(qd_err_text((qd_err_t)e)) > 1, "error %d has a message", e);
}

static void test_caps(void)
{
    /* A die that is a 2 255 times in 256: ie2 almost always runs to the cap. */
    static const qd_face_t sticky_faces[2] = { { 1, 1, "" }, { 2, 255, "" } };
    static qd_die_t sticky;
    qd_die_custom(&sticky, sticky_faces, 2);
    static qd_roll_t r;
    static qd_result_t res;

    qd_roll_init(&r);
    qd_pool_explode(qd_roll_custom(&r, 1, &sticky), qd_test_max(), true);
    int capped = 0, ok = 1;
    for (int i = 0; i < 50; i++) {
        qd_roll_run(&r, &res);
        ok &= res.rolled <= 1 + QD_MAX_EXPLOSIONS;
        capped += !!(res.flags & QD_RES_CAPPED);
    }
    CHECK(ok && capped > 10, "explosion chains stop at %d extra dice (capped %d of 50)",
          QD_MAX_EXPLOSIONS, capped);

    qd_roll_init(&r);                       /* 20 such dice: the roll-wide cap */
    qd_pool_explode(qd_roll_custom(&r, 20, &sticky), qd_test_max(), true);
    qd_roll_run(&r, &res);
    CHECK(res.rolled == QD_MAX_ROLL_DICE && (res.flags & QD_RES_CAPPED) &&
          (res.flags & QD_RES_SUMMARY) && res.dice_count == QD_MAX_KEPT_DICE,
          "a whole roll stops at %d dice: rolled %d", QD_MAX_ROLL_DICE, res.rolled);

    qd_roll_init(&r);
    qd_pool_reroll(qd_roll_custom(&r, 1, &sticky), qd_test_max(), true);
    ok = 1;
    for (int i = 0; i < 50; i++) {
        qd_roll_run(&r, &res);
        int live = 0;
        for (int d = 0; d < res.dice_count; d++) live += !(res.dice[d].flags & QD_DIE_OUT);
        /* a full chain is 101 dice; the list holds 100, so the last may not show */
        ok &= res.pools[0].kept == 1 && (res.dice_count == res.rolled ? live == 1 : live <= 1);
    }
    CHECK(ok, "a capped reroll chain still leaves exactly one die in play");
}

/* --- Counting, buckets, summary mode ---------------------------------------- */
static void test_counting(void)
{
    static qd_roll_t r;
    static qd_result_t res;

    qd_roll_init(&r);                                    /* 150d6 [<3, =6] */
    qd_pool_t *p = qd_roll_dice(&r, 150, 6);
    qd_pool_success(p, qd_test_ge(5));
    qd_pool_botch(p, qd_test_eq(1));
    qd_roll_bucket(&r, qd_test_lt(3));
    qd_roll_bucket(&r, qd_test_eq(6));
    qd_roll_run(&r, &res);
    const qd_pool_result_t *pr = &res.pools[0];
    long sum = 0;
    int fc = 0;
    for (int f = 0; f < 6; f++) { sum += (long)(f + 1) * pr->face_count[f]; fc += pr->face_count[f]; }
    CHECK((res.flags & QD_RES_SUMMARY) && res.dice_count == QD_MAX_KEPT_DICE && pr->listed == 100,
          "150 dice: summary mode, first 100 listed");
    CHECK(sum == res.total && fc == 150, "summary totals stay exact: %ld = %ld", sum, (long)res.total);
    CHECK(res.successes == pr->face_count[4] + pr->face_count[5] && res.botches == pr->face_count[0],
          "successes and botches counted over all 150");
    CHECK(res.buckets[0] == pr->face_count[0] + pr->face_count[1] &&
          res.buckets[1] == pr->face_count[5] &&
          res.buckets[2] == pr->face_count[2] + pr->face_count[3] + pr->face_count[4],
          "buckets <3, =6 and everything else");
    CHECK(res.flags & QD_RES_COUNTING, "a success test marks the roll as counting");
    CHECK(pr->min >= 1 && pr->max <= 6 && pr->min <= pr->max, "min and max");

    qd_die_t fudge;                                      /* 4dF ["+"] */
    qd_die_fudge(&fudge);
    qd_roll_init(&r);
    qd_roll_custom(&r, 4, &fudge);
    qd_roll_bucket(&r, qd_test_label("+"));
    int ok = 1;
    for (int i = 0; i < 1000; i++) {
        qd_roll_run(&r, &res);
        int plus = 0;
        for (int d = 0; d < 4; d++) plus += res.dice[d].value == 1;
        ok &= res.buckets[0] == plus && res.total >= -4 && res.total <= 4;
    }
    CHECK(ok, "4dF: a bucket on the \"+\" label");

    qd_roll_init(&r);                                    /* 1d20: natural 20 and 1 */
    qd_roll_dice(&r, 1, 20);
    int nat20 = 0, nat1 = 0;
    ok = 1;
    for (int i = 0; i < 20000; i++) {
        qd_roll_run(&r, &res);
        uint16_t f = res.dice[0].flags;
        nat20 += !!(f & QD_DIE_MAX);
        nat1 += !!(f & QD_DIE_MIN);
        ok &= !!(f & QD_DIE_MAX) == (res.total == 20) && !!(f & QD_DIE_MIN) == (res.total == 1);
    }
    CHECK(ok && nat20 > 800 && nat1 > 800, "natural 20s (%d) and 1s (%d) are flagged", nat20, nat1);

    char buf[16];
    qd_test_t t = qd_test_ge(8);
    CHECK(!strcmp(qd_test_text(&t, buf, sizeof(buf)), ">=8"), "test text >=8");
    t = qd_test_range(3, 5);
    CHECK(!strcmp(qd_test_text(&t, buf, sizeof(buf)), "3-5"), "test text 3-5");
}

/* Big pools: the sort that keep/drop uses, checked against face counts. */
static void test_big_keep(void)
{
    static qd_roll_t r, all;
    static qd_result_t res, res_all;
    int ok = 1;
    for (int trial = 0; trial < 50; trial++) {
        uint64_t seed = 1000 + (uint64_t)trial;
        int n = 1 + trial * 19;                     /* 1 .. 932 kept of 1000 */
        qd_roll_init(&r);
        qd_pool_keep(qd_roll_dice(&r, 1000, 20), trial % 2 ? QD_KEEP_LOW : QD_KEEP_HIGH, (uint16_t)n);
        qd_roll_init(&all);
        qd_roll_dice(&all, 1000, 20);
        qd_rng_seed_fixed(seed); qd_roll_run(&r, &res);
        qd_rng_seed_fixed(seed); qd_roll_run(&all, &res_all);
        /* the n highest (or lowest) of the same 1000 dice, from their face counts */
        long want = 0;
        int left = n;
        for (int k = 0; k < 20 && left; k++) {
            int f = trial % 2 ? k : 19 - k;
            int take = res_all.pools[0].face_count[f] < left ? res_all.pools[0].face_count[f] : left;
            want += (long)take * (f + 1);
            left -= take;
        }
        ok &= res.total == want && res.pools[0].kept == n;
    }
    CHECK(ok, "1000d20 keeping 1..932 highest or lowest matches the face counts");
}

/* --- Distributions ---------------------------------------------------------- */
static void test_distributions(void)
{
    static qd_roll_t r;
    static qd_result_t res;
    static uint32_t counts[200];
    double exact[200];
    const int N = 400000;

    /* 4d6 k3, against the exact distribution from all 1296 rolls. */
    memset(exact, 0, sizeof(exact));
    for (int a = 1; a <= 6; a++) for (int b = 1; b <= 6; b++)
        for (int c = 1; c <= 6; c++) for (int d = 1; d <= 6; d++) {
            int lo = a; if (b < lo) lo = b; if (c < lo) lo = c; if (d < lo) lo = d;
            exact[a + b + c + d - lo] += 1.0 / 1296;
        }
    qd_roll_init(&r);
    qd_pool_keep(qd_roll_dice(&r, 4, 6), QD_KEEP_HIGH, 3);
    memset(counts, 0, sizeof(counts));
    for (int i = 0; i < N; i++) { qd_roll_run(&r, &res); counts[res.total]++; }
    double chi = 0, mean = 0;
    for (int v = 3; v <= 18; v++) {
        double e = exact[v] * N;
        chi += (counts[v] - e) * (counts[v] - e) / e;
        mean += v * (double)counts[v] / N;
    }
    CHECK(chi < 37.70, "4d6 k3 matches the exact distribution: chi-square %.1f (limit 37.7)", chi);
    printf("  4d6 k3: average %.3f (exact 12.245), chi-square %.1f\n", mean, chi);

    /* 1d20 adv1 is the better of two d20: P(v) = (2v - 1) / 400. */
    qd_roll_init(&r);
    qd_pool_advantage(qd_roll_dice(&r, 1, 20), 1);
    memset(counts, 0, sizeof(counts));
    for (int i = 0; i < N; i++) { qd_roll_run(&r, &res); counts[res.total]++; }
    chi = 0;
    for (int v = 1; v <= 20; v++) {
        double e = (2 * v - 1) / 400.0 * N;
        chi += (counts[v] - e) * (counts[v] - e) / e;
    }
    CHECK(chi < 43.82, "1d20 adv1 = the better of two d20: chi-square %.1f (limit 43.8)", chi);

    /* 1d6 ie: average 3.5 * 6/5 = 4.2. */
    qd_roll_init(&r);
    qd_pool_explode(qd_roll_dice(&r, 1, 6), qd_test_max(), true);
    double s = 0, s2 = 0;
    for (int i = 0; i < N; i++) { qd_roll_run(&r, &res); s += res.total; s2 += (double)res.total * res.total; }
    mean = s / N;
    double sd = sqrt(s2 / N - mean * mean);
    CHECK(fabs(mean - 4.2) < 5 * sd / sqrt(N), "1d6 ie averages 4.2: %.4f", mean);

    /* dd66: all 36 results equally likely. */
    qd_roll_init(&r);
    qd_roll_digits(&r, 1, "66");
    memset(counts, 0, sizeof(counts));
    int ok = 1;
    for (int i = 0; i < 360000; i++) {
        qd_roll_run(&r, &res);
        int t = res.total / 10, u = res.total % 10;
        ok &= t >= 1 && t <= 6 && u >= 1 && u <= 6;
        counts[(t - 1) * 6 + u - 1]++;
    }
    chi = 0;
    for (int i = 0; i < 36; i++) chi += (counts[i] - 10000.0) * (counts[i] - 10000.0) / 10000.0;
    CHECK(ok && chi < 66.62, "dd66: digits 1-6, all 36 equally likely: chi-square %.1f", chi);

    /* 2d6 r1 (reroll 1s once): a die shows 1 with chance 1/36. */
    qd_roll_init(&r);
    qd_pool_reroll(qd_roll_dice(&r, 2, 6), qd_test_eq(1), false);
    long ones = 0, kept = 0;
    for (int i = 0; i < N; i++) {
        qd_roll_run(&r, &res);
        for (int d = 0; d < res.dice_count; d++)
            if (!(res.dice[d].flags & QD_DIE_OUT)) { kept++; ones += res.dice[d].value == 1; }
    }
    double share = (double)ones / kept;
    CHECK(kept == 2L * N && fabs(share - 1 / 36.0) < 0.0015, "r1: ones left %.4f (expect 0.0278)", share);
}

int main(void)
{
    qd_die_custom(&loaded, loaded_faces, 6);
    test_goldens();
    qd_rng_init_hw();
    test_sums();
    test_errors();
    test_caps();
    test_counting();
    test_big_keep();
    test_distributions();
    return qd_test_done("roll engine");
}
