/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_odds.c
 * @brief   D7: odds (against answers worked out by hand) and stats for
 *          nerds (tallies, fair and unfair dice).
 */
#include <math.h>
#include <string.h>

#include "qd4p.h"
#include "qd_test.h"

static qd_parsed_t p;
static qd_parse_error_t err;
static qd_odds_t o;

static bool odds_of(const char *text)
{
    if (!qd_parse(text, &p, &err)) {
        printf("  can't parse '%s': %s\n", text, err.message);
        return false;
    }
    return qd_odds(&p.roll, &o) == QD_OK;
}

static bool close_to(double a, double b, double tol) { return fabs(a - b) <= tol; }

static const qd_face_t loaded_faces[6] = {
    { 1, 1, "" }, { 2, 1, "" }, { 3, 1, "" }, { 4, 1, "" }, { 5, 1, "" }, { 6, 3, "" },
};
static qd_die_t loaded;
static const qd_die_t *lookup(const char *name, void *ctx)
{
    (void)ctx;
    return !strcmp(name, "loaded") ? &loaded : NULL;
}

/* --- Exact sums ------------------------------------------------------------- */
static void test_sums(void)
{
    CHECK(odds_of("3d6") && o.method == QD_ODDS_EXACT_SUM && o.exact && o.lo == 3 && o.hi == 18 &&
          close_to(o.mean, 10.5, 1e-6) && close_to(o.sd, sqrt(35.0 / 4), 1e-5) &&
          close_to(qd_odds_exactly(&o, 10), 27.0 / 216, 1e-6) && close_to(qd_odds_at_least(&o, 10), 0.625, 1e-6),
          "3d6: exact; 10 is 27 in 216, 10 or more 62.5%%");
    CHECK(odds_of("1d20 + 5 vs 15") && o.judged && close_to(o.pass, 0.55, 1e-6), "1d20+5 vs 15: 55%%");
    CHECK(odds_of("(1d8+2)*2") && o.lo == 6 && o.hi == 20 && close_to(o.mean, 13, 1e-6) &&
          qd_odds_exactly(&o, 7) == 0 && close_to(qd_odds_exactly(&o, 8), 0.125, 1e-6),
          "(1d8+2)*2: even numbers 6-20");
    CHECK(odds_of("1d20 - 1d4") && close_to(o.mean, 8, 1e-6) && o.lo == -3 && o.hi == 19, "1d20 - 1d4");
    CHECK(odds_of("2 * -1d6 + 3*1d4") && close_to(o.mean, -7 + 7.5, 1e-6), "negative and multiplied terms");
    CHECK(odds_of("6d10 t8 f1") && o.counting && o.method == QD_ODDS_EXACT_SUM && close_to(o.mean, 1.2, 1e-6) &&
          o.lo == -6 && o.hi == 6, "6d10 t8 f1: successes, exactly (0.3 up, 0.1 down per die)");
    CHECK(odds_of("5d10 t7 ds10") && close_to(o.mean, 5 * 0.5, 1e-6), "doubles counted (7-9 one, 10 two)");
    CHECK(odds_of("dd66") && close_to(o.mean, 38.5, 1e-5) && close_to(qd_odds_exactly(&o, 11), 1.0 / 36, 1e-6) &&
          qd_odds_exactly(&o, 17) == 0, "dd66: 36 results");
    CHECK(odds_of("4dF") && close_to(o.mean, 0, 1e-6) && close_to(o.sd, sqrt(8.0 / 3), 1e-5) &&
          close_to(qd_odds_exactly(&o, 4), 1.0 / 81, 1e-6), "4dF: +4 is 1 in 81");
    CHECK(odds_of("2d{loaded}") && close_to(o.mean, 2 * 33.0 / 8, 1e-5) &&
          close_to(qd_odds_exactly(&o, 12), 9.0 / 64, 1e-6), "a loaded die's weights");
    CHECK(odds_of("100d6") && o.method == QD_ODDS_EXACT_SUM && close_to(o.mean, 350, 1e-3) &&
          close_to(o.sd, sqrt(100 * 35.0 / 12), 1e-2) && o.lo == 100 && o.hi == 600,
          "100d6: still exact (average %.5f), 100 to 600", o.mean);
    CHECK(odds_of("d%") && close_to(o.mean, 50.5, 1e-5), "d%% is 1-100");
}

/* --- Exact by trying every roll ----------------------------------------------- */
static void test_every_roll(void)
{
    CHECK(odds_of("4d6 k3") && o.method == QD_ODDS_EXACT_ALL && o.runs == 1296 &&
          close_to(o.mean, 15869.0 / 1296, 1e-6) && close_to(qd_odds_exactly(&o, 18), 21.0 / 1296, 1e-7),
          "4d6 k3: all 1,296 rolls, average 12.245, 18 is 21 in 1,296");
    CHECK(odds_of("atk adv + 5 vs 15") && o.exact && o.runs == 400 && close_to(o.pass, 1 - 0.45 * 0.45, 1e-9) &&
          close_to(o.level[QD_LVL_CRIT + 4], 1 - 0.95 * 0.95, 1e-9),
          "atk adv + 5 vs 15: hits 79.75%%, crits 9.75%%");
    CHECK(odds_of("1d20 dis") && close_to(o.mean, 7.175, 1e-9), "disadvantage averages 7.175");

    /* CoC with one bonus die: every result's chance, against counting by hand */
    CHECK(odds_of("coc 50 b1") && o.exact && o.runs == 1000, "coc 50 b1: 1,000 rolls");
    double extreme = 0, fumble = 0;
    for (int u = 0; u < 10; u++)
        for (int a = 0; a < 10; a++)
            for (int b = 0; b < 10; b++) {
                int va = a * 10 + u, vb = b * 10 + u;
                if (!va) va = 100;
                if (!vb) vb = 100;
                int v = va < vb ? va : vb;
                extreme += (v != 1 && v <= 10) / 1000.0;
                fumble += (v == 100) / 1000.0;
            }
    CHECK(close_to(o.level[QD_COC_EXTREME_SUCCESS + 4], extreme, 1e-9) &&
          close_to(o.level[QD_COC_FUMBLE + 4], fumble, 1e-9), "coc 50 b1: extreme %.3f, fumble %.4f", extreme, fumble);

    CHECK(odds_of("fate + 2 vs 3") && o.exact, "fate is exact");
    double p_ge1 = 0;
    for (int c = 0; c < 81; c++) {
        int x = c, s = 0;
        for (int k = 0; k < 4; k++, x /= 3) s += x % 3 - 1;
        p_ge1 += (s >= 1) / 81.0;
    }
    CHECK(close_to(o.pass, p_ge1, 1e-9), "fate + 2 vs 3 passes on 4dF of +1 or more: %.4f", p_ge1);
    CHECK(odds_of("dcc d20 +1d vs 20") && close_to(o.pass, 5.0 / 24, 1e-9), "dcc: a d24 rolls 20+ 5 in 24");
    CHECK(odds_of("5d6 k3") && o.method == QD_ODDS_EXACT_ALL && o.runs == 7776, "5d6 k3: all 7,776 rolls");
}

/* --- Simulated --------------------------------------------------------------- */
static void test_simulated(void)
{
    CHECK(odds_of("sw8") && o.method == QD_ODDS_SIMULATED && !o.exact && o.runs == QD_ODDS_SAMPLES &&
          close_to(o.pass, 0.8125, 5 * sqrt(0.8125 * 0.1875 / QD_ODDS_SAMPLES)),
          "sw8 (exploding): simulated, success %.4f (exact 0.8125)", o.pass);
    CHECK(odds_of("1d6 ie") && close_to(o.mean, 4.2, 0.12), "1d6 ie: average %.3f (exact 4.2)", o.mean);
    CHECK(odds_of("vtm 8 h3") && !o.exact, "vtm 8 h3: 10^8 rolls, so simulated");
    CHECK(odds_of("300d6") && o.method == QD_ODDS_SIMULATED && o.runs <= QD_ODDS_SAMPLES &&
          close_to(o.mean, 1050, 3), "300d6: too wide to convolve, simulated (%u runs)", o.runs);

    /* Odds never touch the rolls that follow, in replay or out. */
    qd_rng_seed_fixed(42);
    odds_of("sw8 + 1 vs 6");
    odds_of("4d6 k3");
    CHECK(qd_rng_next32() == 1776835114u, "after odds, seed 42 still gives its first number");
}

/* --- Stats for nerds ------------------------------------------------------------ */
static void test_stats(void)
{
    static qd_stats_t st;
    static qd_result_t res;
    qd_stats_report_t rep;
    qd_stats_clear(&st);
    qd_stats_watch(&st);

    qd_rng_seed_fixed(7);
    qd_parse("1000d6", &p, &err);
    for (int i = 0; i < 6; i++) qd_roll_run(&p.roll, &res);
    const qd_die_stats_t *k = qd_stats_find(&st, 6, NULL);
    qd_stats_report(k, &rep);
    CHECK(k && k->count == 6000 && rep.verdict == QD_STATS_FAIR && close_to(rep.mean, 3.5, 0.08) &&
          close_to(rep.expect_sd, sqrt(35.0 / 12), 1e-9),
          "6,000 d6: average %.3f, chi-square %.1f (limit %.1f), fair", rep.mean, rep.chi, rep.limit);

    qd_parse("2d{loaded}", &p, &err);
    for (int i = 0; i < 3000; i++) qd_roll_run(&p.roll, &res);
    k = qd_stats_find(&st, 6, &loaded);
    qd_stats_report(k, &rep);
    CHECK(k && k->count == 6000 && close_to(rep.expect_mean, 33.0 / 8, 1e-9) && rep.verdict == QD_STATS_FAIR,
          "a loaded die is its own kind, judged against its weights: average %.3f", rep.mean);

    /* A d6 that rolls like the loaded one is caught. */
    qd_stats_t bad;
    qd_stats_clear(&bad);
    qd_stats_watch(NULL);
    qd_die_t d;
    qd_die_custom(&d, loaded_faces, 6);
    for (int i = 0; i < 3000; i++) {
        qd_face_roll_t f = qd_die_roll(&d);
        qd_stats_add(&bad, 6, NULL, f.face, f.value);
    }
    qd_stats_report(&bad.kind[0], &rep);
    CHECK(rep.verdict == QD_STATS_ODD && rep.highest > 1000, "a 'd6' rolling sixes 3 in 8: odd (chi %.0f)", rep.chi);
    qd_stats_watch(&st);

    qd_parse("coc 50 b1", &p, &err);
    uint32_t before = qd_stats_find(&st, 10, NULL) ? qd_stats_find(&st, 10, NULL)->count : 0;
    qd_roll_run(&p.roll, &res);
    CHECK(qd_stats_find(&st, 10, NULL)->count == before + 3, "d%% with a bonus die is three d10s");
    qd_parse("dd36", &p, &err);
    uint32_t d3 = 0;
    qd_roll_run(&p.roll, &res);
    d3 = qd_stats_find(&st, 3, NULL)->count;
    CHECK(d3 == 1, "digit dice count each digit's die (a d3 here)");

    qd_parse("3d1000", &p, &err);
    qd_roll_run(&p.roll, &res);
    qd_stats_report(qd_stats_find(&st, 1000, NULL), &rep);
    CHECK(rep.verdict == QD_STATS_NO_FACES && close_to(rep.expect_mean, 500.5, 1e-9), "a d1000: totals only");
    qd_parse("1d20", &p, &err);
    qd_roll_run(&p.roll, &res);
    qd_stats_report(qd_stats_find(&st, 20, NULL), &rep);
    CHECK(rep.verdict == QD_STATS_TOO_FEW, "one d20: too few to judge");

    uint32_t six_before = qd_stats_find(&st, 6, NULL)->count;
    odds_of("sw6 + 1");
    CHECK(qd_stats_find(&st, 6, NULL)->count == six_before, "working out odds counts nothing");
    qd_stats_watch(NULL);

    CHECK(close_to(qd_chi_limit(5), 20.515, 0.001) && close_to(qd_chi_limit(19), 43.82, 0.001) &&
          close_to(qd_chi_limit(99), 148.23, 0.5), "the 1-in-1,000 limits: %.3f %.3f %.2f",
          qd_chi_limit(5), qd_chi_limit(19), qd_chi_limit(99));
}

int main(void)
{
    qd_die_custom(&loaded, loaded_faces, 6);
    qd_parse_set_lookup(lookup, NULL);
    qd_rng_init_hw();
    test_sums();
    test_every_roll();
    test_simulated();
    test_stats();
    return qd_test_done("odds and stats");
}
