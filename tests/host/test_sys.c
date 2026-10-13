/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_sys.c
 * @brief   D5: game systems. Outcome rates against probabilities worked out
 *          by hand (or by trying every combination), and every outcome
 *          re-checked against the dice it came from.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "qd4p.h"
#include "qd_test.h"

static qd_parsed_t p;
static qd_result_t res;
static qd_outcome_t out;
static qd_parse_error_t err;

/* Parse, roll, judge. */
static void go(void)
{
    qd_roll_run(&p.roll, &res);
    qd_sys_outcome(&p.roll, &res, &out);
}

/* An observed rate within 5 standard errors of the exact one. */
static bool near(long hits, long n, double exact)
{
    double rate = (double)hits / n, se = sqrt(exact * (1 - exact) / n);
    return fabs(rate - exact) <= 5 * se + 1e-9;
}

#define N 200000L

/* --- Plain "vs" ------------------------------------------------------------ */
static void test_plain(void)
{
    CHECK(qd_parse("1d20+5 vs 15", &p, &err), "1d20+5 vs 15 parses");
    int ok = 1;
    for (int i = 0; i < 2000; i++) {
        go();
        ok &= out.judged && out.pass == (res.total >= 15) && out.margin == res.total - 15 &&
              !strcmp(out.text, out.pass ? "Pass" : "Fail");
    }
    CHECK(ok, "plain vs: the total must reach the target");

    qd_parse("6d10 t8 vs 3", &p, &err);
    ok = 1;
    for (int i = 0; i < 2000; i++) {
        go();
        ok &= out.pass == (res.successes >= 3) && out.value == res.successes;
    }
    CHECK(ok, "plain vs on a success roll judges the successes");

    qd_parse("2d6", &p, &err);
    qd_roll_run(&p.roll, &res);
    CHECK(!qd_sys_outcome(&p.roll, &res, &out) && out.level == QD_LVL_NONE, "nothing to judge without vs");
}

/* --- D&D 5e ---------------------------------------------------------------- */
static void test_dnd(void)
{
    qd_parse("atk adv + 5 vs 15", &p, &err);
    long crits = 0;
    int ok = 1;
    for (long i = 0; i < N; i++) {
        go();
        int nat = 0;
        for (int d = 0; d < res.dice_count; d++)
            if (!(res.dice[d].flags & QD_DIE_OUT)) nat = res.dice[d].value;
        int hi = res.dice[0].value > res.dice[1].value ? res.dice[0].value : res.dice[1].value;
        bool want = nat == 20 || (nat != 1 && res.total >= 15);
        ok &= nat == hi && res.total == nat + 5 && out.pass == want &&
              (nat == 20) == (out.level == QD_LVL_CRIT) && out.critical == (nat == 20);
        crits += nat == 20;
    }
    CHECK(ok, "atk adv: the higher d20 counts; nat 20 hits, nat 1 misses, else total vs AC");
    CHECK(near(crits, N, 1 - 0.95 * 0.95), "advantage crits 9.75%% of the time: %.3f%%", 100.0 * crits / N);

    qd_parse("atk + 30 vs 15", &p, &err);
    long misses = 0;
    for (long i = 0; i < N; i++) { go(); misses += !out.pass; }
    CHECK(near(misses, N, 0.05), "+30 still misses on a natural 1: %.2f%%", 100.0 * misses / N);

    qd_parse("chk - 30 vs 5", &p, &err);
    ok = 1;
    for (int i = 0; i < 5000; i++) { go(); ok &= !out.pass; }
    CHECK(ok, "a check gets no automatic success on a natural 20");
    qd_parse("save dis + 2", &p, &err);
    go();
    CHECK(p.roll.pools[0].advantage == -1 && !out.judged, "save dis without a target only reports");
}

/* --- Savage Worlds --------------------------------------------------------- */
static void test_savage(void)
{
    qd_parse("sw8", &p, &err);
    long ok_n = 0, raise = 0, snake = 0;
    int ok = 1;
    for (long i = 0; i < N; i++) {
        go();
        int t = res.pools[0].total, w = res.pools[1].total, best = t > w ? t : w;
        bool se = res.dice[res.pools[0].first].value == 1 && res.dice[res.pools[1].first].value == 1;
        ok &= res.total == best && out.fumble == se && out.pass == (!se && best >= 4) &&
              out.degrees == (out.pass ? (best - 4) / 4 : 0);
        ok_n += out.pass;
        raise += out.degrees >= 1;
        snake += se;
    }
    CHECK(ok, "sw8: best of trait and wild die, TN 4, a raise per 4, snake eyes");
    /* Trait d8 below 4: 3/8. Wild d6 below 4: 3/6. Below 8: d8 7/8 (an 8
     * explodes past it); d6 5/6 + 1/36 (6 then 1 is 7). Snake eyes 1/48. */
    CHECK(near(ok_n, N, 1 - 3.0 / 8 * 3.0 / 6), "success %.3f%% (exact 81.250%%)", 100.0 * ok_n / N);
    CHECK(near(raise, N, 1 - 7.0 / 8 * 31.0 / 36), "a raise %.3f%% (exact 24.653%%)", 100.0 * raise / N);
    CHECK(near(snake, N, 1.0 / 48), "snake eyes %.3f%% (exact 2.083%%)", 100.0 * snake / N);

    qd_parse("sw6 x + 1 vs 6", &p, &err);
    ok = 1;
    for (int i = 0; i < 5000; i++) {
        go();
        ok &= p.roll.pool_count == 1 && res.total == res.pools[0].total + 1 &&
              out.pass == (res.total >= 6) && !out.fumble;
    }
    CHECK(ok, "an Extra rolls no wild die; vs changes the TN");
    CHECK(!qd_parse("sw7", &p, &err) && strstr(err.message, "d4, d6, d8"), "sw7 is refused");
}

/* --- Call of Cthulhu ----------------------------------------------------------- */
static int coc_level(int v, int skill)
{
    if (v == 1) return QD_COC_CRITICAL;
    if (v == 100 || (skill < 50 && v >= 96)) return QD_COC_FUMBLE;
    if (v <= skill / 5) return QD_COC_EXTREME_SUCCESS;
    if (v <= skill / 2) return QD_COC_HARD_SUCCESS;
    if (v <= skill) return QD_COC_REGULAR_SUCCESS;
    return QD_COC_FAILURE;
}

static void test_coc(void)
{
    for (int bonus = -2; bonus <= 2; bonus++) {
        /* exact chance of each result 1..100 with this many bonus/penalty dice */
        double share[101] = { 0 };
        int n = 1 + abs(bonus), combos = 1;
        for (int i = 0; i < n; i++) combos *= 10;
        for (int u = 0; u < 10; u++)
            for (int c = 0; c < combos; c++) {
                int best = -1, x = c;
                for (int i = 0; i < n; i++, x /= 10) {
                    int v = (x % 10) * 10 + u;
                    if (!v) v = 100;
                    if (best < 0 || (bonus > 0 && v < best) || (bonus < 0 && v > best)) best = v;
                }
                share[best] += 1.0 / (10.0 * combos);
            }
        int skill = 45;
        double exact[7] = { 0 };
        for (int v = 1; v <= 100; v++) exact[coc_level(v, skill) + 2] += share[v];

        char text[32];
        snprintf(text, sizeof(text), "coc %d %c%d", skill, bonus < 0 ? 'p' : 'b', abs(bonus));
        qd_parse(text, &p, &err);
        long seen[7] = { 0 };
        int ok = 1;
        for (long i = 0; i < N; i++) {
            go();
            int v = res.pools[0].total;
            ok &= out.level == coc_level(v, skill) && out.value == v &&
                  res.pools[0].rolled == (uint16_t)n;
            seen[out.level + 2]++;
        }
        bool close = true;
        for (int l = 0; l < 7; l++)
            if (exact[l] > 0) close &= near(seen[l], N, exact[l]);
        CHECK(ok && close, "%s: every level right, and each as often as it should be", text);
    }

    qd_parse("coc 60 vs hard", &p, &err);
    int ok = 1;
    for (int i = 0; i < 5000; i++) {
        go();
        int v = res.pools[0].total;
        bool want = v == 1 || (v <= 30 && v != 100);
        ok &= out.pass == want;
    }
    CHECK(ok, "coc 60 vs hard needs 30 or less");
    CHECK(!qd_parse("coc 60 + 5", &p, &err) && strstr(err.message, "no maths"), "no maths after coc");
    CHECK(!qd_parse("coc 60 vs 30", &p, &err) && strstr(err.message, "regular, hard"), "coc vs is a level");
    CHECK(!qd_parse("coc 60 b4", &p, &err), "at most 3 bonus dice");
}

/* --- FATE --------------------------------------------------------------------- */
static void test_fate(void)
{
    CHECK(!strcmp(qd_fate_ladder(3), "Good") && !strcmp(qd_fate_ladder(-2), "Terrible") &&
          !strcmp(qd_fate_ladder(8), "Legendary") && !strcmp(qd_fate_ladder(12), "Legendary") &&
          !strcmp(qd_fate_ladder(0), "Mediocre"), "the ladder");
    qd_parse("fate + 2 vs 3", &p, &err);
    int ok = 1;
    for (int i = 0; i < 5000; i++) {
        go();
        int s = res.total - 3;
        int want = s < 0 ? QD_FATE_FAIL : s == 0 ? QD_FATE_TIE : s < 3 ? QD_FATE_SUCCESS : QD_FATE_STYLE;
        ok &= out.level == want && out.degrees == s && res.total >= -2 && res.total <= 6;
    }
    CHECK(ok, "fate: shifts, tie, success with style");
    qd_parse("fate + 1", &p, &err);
    go();
    CHECK(!out.judged && strstr(out.text, qd_fate_ladder(res.total)), "without vs: the ladder ('%s')", out.text);
}

/* --- Vampire 5e ------------------------------------------------------------------ */
static void test_vtm(void)
{
    qd_parse("vtm 5 h2", &p, &err);
    long crit = 0, messy = 0, bestial = 0;
    int ok = 1;
    for (long i = 0; i < N; i++) {
        go();
        int tens = 0, htens = 0, hones = 0, six = 0;
        for (int d = 0; d < res.dice_count; d++) {
            int v = res.dice[d].value;
            bool hunger = res.dice[d].pool == 1;
            tens += v == 10;
            six += v >= 6;
            htens += hunger && v == 10;
            hones += hunger && v == 1;
        }
        int s = six + 2 * (tens / 2);
        ok &= out.value == s && out.critical == (tens >= 2);
        crit += tens >= 2;
        messy += out.level == QD_VTM_MESSY;
        bestial += out.level == QD_VTM_BESTIAL;
        ok &= (out.level == QD_VTM_MESSY) == (tens >= 2 && htens > 0);
        ok &= (out.level == QD_VTM_BESTIAL) == (s == 0 && hones > 0);
    }
    /* Exact: three regular dice, two hunger, each a 10 one time in ten. */
    double p_crit = 0, p_messy = 0;
    for (int r = 0; r <= 3; r++)
        for (int h = 0; h <= 2; h++) {
            double pr = (r == 0 ? 0.729 : r == 1 ? 0.243 : r == 2 ? 0.027 : 0.001) *
                        (h == 0 ? 0.81 : h == 1 ? 0.18 : 0.01);
            if (r + h >= 2) { p_crit += pr; if (h) p_messy += pr; }
        }
    CHECK(ok, "vtm: successes with crit pairs, messy and bestial match the dice");
    CHECK(near(crit, N, p_crit), "a critical %.3f%% (exact %.3f%%)", 100.0 * crit / N, 100 * p_crit);
    CHECK(near(messy, N, p_messy), "messy %.3f%% (exact %.3f%%)", 100.0 * messy / N, 100 * p_messy);
    /* Without a target, bestial means no successes (all five 1-5) and a hunger 1. */
    double p_bestial = pow(0.5, 5) * (1 - 0.8 * 0.8);
    CHECK(near(bestial, N, p_bestial), "bestial %.3f%% (exact %.3f%%)", 100.0 * bestial / N, 100 * p_bestial);

    qd_parse("vtm 6 h2 vs 3", &p, &err);
    ok = 1;
    for (int i = 0; i < 20000; i++) {
        go();
        int hones = 0;
        for (int d = 0; d < res.dice_count; d++) hones += res.dice[d].pool == 1 && res.dice[d].value == 1;
        ok &= out.pass == (out.value >= 3);
        if (!out.pass) ok &= (out.level == QD_VTM_BESTIAL) == (hones > 0);
    }
    CHECK(ok, "vtm vs 3: a failure with a hunger 1 is bestial");
    qd_parse("vtm 2 h5", &p, &err);
    CHECK(p.roll.pool_count == 1 && p.roll.pools[0].count == 2 && !strcmp(p.roll.pools[0].name, "hunger"),
          "hunger can't exceed the pool");
    CHECK(!qd_parse("vtm 5 h6", &p, &err), "hunger is at most 5");
}

/* --- DCC ------------------------------------------------------------------------- */
static void test_dcc(void)
{
    CHECK(qd_dcc_chain(20, 1) == 24 && qd_dcc_chain(20, -2) == 14 && qd_dcc_chain(30, 3) == 30 &&
          qd_dcc_chain(3, -1) == 3 && qd_dcc_chain(9, 0) == 0 && qd_dcc_chain(8, 1) == 10,
          "the dice chain");
    qd_parse("dcc d20 +1d + 2 vs 15", &p, &err);
    CHECK(p.roll.pools[0].sides == 24, "dcc d20 +1d rolls a d24");
    int ok = 1;
    for (int i = 0; i < 20000; i++) {
        go();
        int nat = res.dice[0].value;
        bool want = nat == 24 || (nat != 1 && res.total >= 15);
        ok &= res.total == nat + 2 && out.pass == want && out.critical == (nat == 24) && out.fumble == (nat == 1);
    }
    CHECK(ok, "dcc: natural max crits, natural 1 fumbles, else the total");
    qd_parse("dcc d16 -1d +1d4", &p, &err);
    CHECK(p.roll.pools[0].sides == 14 && p.roll.pool_count == 2 && p.roll.pools[1].sides == 4,
          "a chain step, then an ordinary extra die");
    CHECK(!qd_parse("dcc d9", &p, &err) && strstr(err.message, "dice chain"), "d9 isn't on the chain");
}

/* --- Percentile pools and matching sets -------------------------------------------- */
static void test_pools(void)
{
    qd_parse("d% b2", &p, &err);
    qd_rng_seed_fixed(42);
    qd_roll_run(&p.roll, &res);
    qd_percentile_t pc;
    qd_rng_seed_fixed(42);
    qd_roll_percentile(2, &pc);
    int aside = 0;
    for (int d = 1; d < res.dice_count; d++) aside += !!(res.dice[d].flags & QD_DIE_SET_ASIDE);
    CHECK(res.total == pc.result && res.rolled == 3 && aside == 2 && res.dice[0].value == pc.result,
          "d%% b2 is the CoC roll: %d (direct %d), the other two tens dice set aside", (int)res.total, pc.result);

    qd_parse("5d6 sets", &p, &err);
    int ok = 1;
    for (int i = 0; i < 5000; i++) {
        qd_roll_run(&p.roll, &res);
        int count[7] = { 0 }, want = 0;
        for (int d = 0; d < 5; d++) count[res.dice[d].value]++;
        for (int v = 1; v <= 6; v++) want += count[v] >= 2;
        const qd_pool_result_t *pr = &res.pools[0];
        ok &= pr->set_count == want;
        for (int s = 0; s < pr->set_count; s++) {
            ok &= count[pr->sets[s].value] == pr->sets[s].size;
            if (s) ok &= pr->sets[s - 1].size > pr->sets[s].size ||
                         (pr->sets[s - 1].size == pr->sets[s].size && pr->sets[s - 1].value > pr->sets[s].value);
        }
    }
    CHECK(ok, "5d6 sets: every pair or better found, biggest first");
}

/* One qd_roll_t, built again for each roll: every builder starts afresh,
 * whatever was in the roll before (a damage roll, another system). */
static void test_reuse(void)
{
    static qd_roll_t r;
    static qd_result_t res;
    bool ok = true;
    for (int i = 0; i < 50 && ok; i++) {
        ok &= qd_sys_dnd(&r, QD_DND_ATTACK, i % 3 - 1);
        qd_roll_add(&r, 5);
        qd_roll_vs(&r, 15);
        ok &= qd_roll_run(&r, &res) == QD_OK && res.total >= 6 && res.total <= 25;
        qd_roll_init(&r);                                   /* a damage roll in between */
        qd_roll_dice(&r, 2, 8);
        ok &= qd_roll_run(&r, &res) == QD_OK;
        ok &= qd_sys_sw(&r, 8, true) && qd_roll_run(&r, &res) == QD_OK && r.sys.id == QD_SYS_SW;
        ok &= qd_sys_coc(&r, 50, 1) && qd_roll_run(&r, &res) == QD_OK && !r.sys.has_target;
        ok &= qd_sys_fate(&r) && qd_sys_vtm(&r, 6, 2) && qd_sys_dcc(&r, 1, 20, 1) &&
              qd_roll_run(&r, &res) == QD_OK && r.pool_count == 1;
    }
    CHECK(ok, "each builder starts a fresh roll: one qd_roll_t, reused for every roll");
}

int main(void)
{
    qd_rng_init_hw();
    test_reuse();
    test_plain();
    test_dnd();
    test_savage();
    test_coc();
    test_fate();
    test_vtm();
    test_dcc();
    test_pools();
    return qd_test_done("game systems");
}
