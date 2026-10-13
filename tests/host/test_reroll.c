/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_reroll.c
 * @brief   D6: rerolls after the fact (whole roll, chosen dice, dice
 *          matching a test, better or worse of two) and roll sets.
 */
#include <math.h>
#include <string.h>

#include "qd4p.h"
#include "qd_test.h"

static qd_parsed_t p;
static qd_result_t res, copy;
static qd_parse_error_t err;

static long kept_sum(const qd_result_t *r)
{
    long s = 0;
    for (int i = 0; i < r->dice_count; i++)
        if (!(r->dice[i].flags & QD_DIE_OUT)) s += r->dice[i].value;
    return s;
}

/* --- Whole roll ----------------------------------------------------------- */
static void test_all(void)
{
    qd_parse("sw8 + 1", &p, &err);
    qd_roll_run(&p.roll, &res);
    int32_t first = res.total;
    CHECK(qd_reroll_all(&p.roll, &res) == QD_OK && res.was_total == first && res.rerolls == 1 &&
          (res.flags & QD_RES_REROLLED), "a benny: rolled again, the old total kept");
    int32_t second = res.total;
    qd_reroll_all(&p.roll, &res);
    CHECK(res.rerolls == 2 && res.was_total == second, "rerolled twice: counts, and remembers the last");
}

/* --- Chosen dice ------------------------------------------------------------ */
static void test_chosen(void)
{
    qd_parse("4d6 k3", &p, &err);
    int ok = 1, draws_ok = 1;
    for (uint64_t seed = 1; seed <= 300; seed++) {
        qd_rng_seed_fixed(seed);
        qd_roll_run(&p.roll, &res);
        /* the reroll's draw is simply the next one from the generator */
        qd_rng_seed_fixed(seed);
        qd_roll_run(&p.roll, &copy);
        int16_t next = (int16_t)qd_rng_between(1, 6);

        qd_rng_seed_fixed(seed);
        qd_roll_run(&p.roll, &res);
        int16_t old[4];
        for (int i = 0; i < 4; i++) old[i] = res.dice[i].value;
        int32_t before = res.total;
        uint16_t which = 2;
        ok &= qd_reroll_dice(&p.roll, &res, &which, 1) == QD_OK;
        draws_ok &= res.dice[2].value == next;
        ok &= res.dice[2].was == old[2] && (res.dice[2].flags & QD_DIE_CHANGED) &&
              res.dice[0].value == old[0] && res.dice[1].value == old[1] && res.dice[3].value == old[3];
        /* keep 3 worked out again: total = sum of the three highest */
        int v[4] = { res.dice[0].value, res.dice[1].value, res.dice[2].value, res.dice[3].value };
        int lo = v[0], sum = 0;
        for (int i = 0; i < 4; i++) { sum += v[i]; if (v[i] < lo) lo = v[i]; }
        ok &= res.total == sum - lo && res.total == kept_sum(&res) && res.was_total == before &&
              res.rerolls == 1;
    }
    CHECK(ok, "4d6 k3, reroll one die: old value kept, keep 3 worked out again");
    CHECK(draws_ok, "the reroll is the generator's next draw (same in replay)");

    /* A second reroll of the same die keeps its first value in `was`. */
    qd_rng_seed_fixed(9);
    qd_roll_run(&p.roll, &res);
    int16_t original = res.dice[1].value;
    uint16_t w = 1;
    qd_reroll_dice(&p.roll, &res, &w, 1);
    qd_reroll_dice(&p.roll, &res, &w, 1);
    CHECK(res.dice[1].was == original && res.rerolls == 2, "twice rerolled: was is the first roll");

    /* Bad choices change nothing. */
    qd_roll_run(&p.roll, &res);
    copy = res;
    uint16_t bad[2] = { 1, 40 };
    CHECK(qd_reroll_dice(&p.roll, &res, bad, 2) == QD_ERR_REROLL && !memcmp(&res, &copy, sizeof(res)),
          "an index past the dice: refused, nothing changed");
    qd_parse("1d20 adv", &p, &err);
    qd_roll_run(&p.roll, &res);
    uint16_t aside = (res.dice[0].flags & QD_DIE_SET_ASIDE) ? 0 : 1;
    CHECK(qd_reroll_dice(&p.roll, &res, &aside, 1) == QD_ERR_REROLL, "a set-aside die can't be rerolled");
    qd_parse("d% b1", &p, &err);
    qd_roll_run(&p.roll, &res);
    w = 0;
    CHECK(qd_reroll_dice(&p.roll, &res, &w, 1) == QD_ERR_REROLL, "percentile dice: reroll the whole roll");
    qd_parse("150d6", &p, &err);
    qd_roll_run(&p.roll, &res);
    CHECK(qd_reroll_dice(&p.roll, &res, &w, 1) == QD_ERR_REROLL, "not in summary mode");

    /* Rerolling a die that exploded sets its explosion aside. */
    qd_parse("3d6 ie", &p, &err);
    bool found = false;
    for (uint64_t seed = 1; seed < 500 && !found; seed++) {
        qd_rng_seed_fixed(seed);
        qd_roll_run(&p.roll, &res);
        if (res.dice[0].value != 6) continue;
        found = true;
        w = 0;
        qd_reroll_dice(&p.roll, &res, &w, 1);
        bool children_out = true;
        int k = 0;
        for (int i = 3; i < res.dice_count; i++) {
            /* everything descended from die 0 is set aside */
            int a = i;
            while (res.dice[a].parent != QD_NO_PARENT && res.dice[a].parent != 0) a = res.dice[a].parent;
            if (res.dice[a].parent == 0) { children_out &= !!(res.dice[i].flags & QD_DIE_REROLLED); k++; }
        }
        CHECK(children_out && k > 0 && res.total == kept_sum(&res),
              "3d6 ie: rerolling an exploded 6 sets its %d explosion dice aside", k);
    }
    CHECK(found, "found a roll with an exploded first die");
}

/* --- Dice matching a test --------------------------------------------------- */
static void test_matching(void)
{
    qd_parse("6d6 t6", &p, &err);
    int ok = 1;
    for (int i = 0; i < 2000; i++) {
        qd_roll_run(&p.roll, &res);
        int16_t before[6];
        int not6 = 0;
        for (int d = 0; d < 6; d++) { before[d] = res.dice[d].value; not6 += before[d] != 6; }
        int n = qd_reroll_matching(&p.roll, &res, -1, qd_test_ne(6));
        int sixes = 0;
        for (int d = 0; d < 6; d++) {
            sixes += res.dice[d].value == 6;
            if (before[d] == 6) ok &= res.dice[d].value == 6 && !(res.dice[d].flags & QD_DIE_CHANGED);
            else ok &= (res.dice[d].flags & QD_DIE_CHANGED) && res.dice[d].was == before[d];
        }
        ok &= n == not6 && res.successes == sixes;
    }
    CHECK(ok, "Year Zero push: every die but the 6s rerolled, successes counted again");

    qd_parse("base:3d6 t6 + gear:2d6 t6", &p, &err);
    qd_roll_run(&p.roll, &res);
    int16_t gear[2] = { res.dice[3].value, res.dice[4].value };
    qd_reroll_matching(&p.roll, &res, 0, qd_test_le(5));
    CHECK(res.dice[3].value == gear[0] && res.dice[4].value == gear[1], "one pool only: the others untouched");

    /* Vampire willpower: up to three regular dice, never Hunger dice. */
    qd_parse("vtm 6 h2 vs 3", &p, &err);
    qd_outcome_t o;
    ok = 1;
    for (int i = 0; i < 2000; i++) {
        qd_roll_run(&p.roll, &res);
        uint16_t pick[3];
        int n = 0;
        for (int d = 0; d < res.pools[0].listed && n < 3; d++)
            if (res.dice[d].value < 6) pick[n++] = (uint16_t)d;
        if (n) qd_reroll_dice(&p.roll, &res, pick, n);
        qd_sys_outcome(&p.roll, &res, &o);
        int six = 0, tens = 0;
        for (int d = 0; d < res.dice_count; d++) { six += res.dice[d].value >= 6; tens += res.dice[d].value == 10; }
        ok &= o.value == six + 2 * (tens / 2) && o.pass == (o.value >= 3);
    }
    CHECK(ok, "Vampire willpower: outcome judged again from the new dice");
}

/* --- Better or worse of two ------------------------------------------------- */
static void test_keep(void)
{
    qd_parse("1d20", &p, &err);
    static uint32_t counts[21];
    memset(counts, 0, sizeof(counts));
    const long N = 400000;
    int ok = 1;
    for (long i = 0; i < N; i++) {
        qd_roll_run(&p.roll, &res);
        int32_t first = res.total;
        bool took = qd_reroll_keep(&p.roll, &res, true);
        ok &= res.total >= res.was_total && res.rerolls == 1 &&
              (took ? res.was_total == first : (res.total == first && (res.flags & QD_RES_KEPT_FIRST)));
        counts[res.total]++;
    }
    double chi = 0;
    for (int v = 1; v <= 20; v++) {
        double e = (2 * v - 1) / 400.0 * N;
        chi += (counts[v] - e) * (counts[v] - e) / e;
    }
    CHECK(ok, "keep the better: never lower than the other roll, the other remembered");
    CHECK(chi < 43.82, "luck on a d20 is the better of two d20s: chi-square %.1f", chi);

    qd_parse("6d10 t7", &p, &err);
    ok = 1;
    for (int i = 0; i < 5000; i++) {
        qd_roll_run(&p.roll, &res);
        qd_reroll_keep(&p.roll, &res, false);
        ok &= res.successes <= res.was_successes;
    }
    CHECK(ok, "keep the worse, on a success roll, compares successes");
}

/* --- Roll sets ---------------------------------------------------------------- */
static int calls;
static void count_call(int index, const qd_result_t *r, void *ctx)
{
    (void)r;
    calls += index >= 0 && ctx == &calls;
}

static void test_sets(void)
{
    qd_set_result_t s;
    CHECK(qd_parse("(6x4d6)k3", &p, &err) && p.set && p.repeat == 6 && p.set_keep == QD_KEEP_HIGH &&
          p.set_keep_n == 3 && p.roll.pools[0].keep == QD_KEEP_ALL,
          "(6x4d6)k3: six rolls of 4d6, keep the 3 best");
    CHECK(qd_parse("6x4d6k3", &p, &err) && !p.set && p.repeat == 6 && p.roll.pools[0].keep == QD_KEEP_HIGH &&
          p.roll.pools[0].keep_n == 3, "6x4d6k3: x binds loosest - six rolls of 4d6k3");

    qd_parse("(6x4d6)k3", &p, &err);
    int ok = 1;
    calls = 0;
    for (int t = 0; t < 2000; t++) {
        qd_set_run(&p, &s, t == 0 ? count_call : NULL, &calls);
        int kept = 0;
        long sum = 0, lowest_kept = 1000, highest_dropped = -1;
        for (int k = 0; k < s.count; k++) {
            ok &= s.value[k] >= 4 && s.value[k] <= 24;
            if (s.kept[k]) { kept++; sum += s.value[k]; if (s.value[k] < lowest_kept) lowest_kept = s.value[k]; }
            else if (s.value[k] > highest_dropped) highest_dropped = s.value[k];
        }
        ok &= s.count == 6 && kept == 3 && s.total == sum && lowest_kept >= highest_dropped;
    }
    CHECK(ok && calls == 6, "the 3 best of 6 kept, total their sum; called once per roll");

    qd_parse("(Stats) (7 x 4d6k3) k6 ! classic", &p, &err);
    ok = 1;
    for (int t = 0; t < 2000; t++) {
        qd_set_run(&p, &s, NULL, NULL);
        int kept = 0, lo = 99, sum = 0;
        for (int k = 0; k < 7; k++) { kept += s.kept[k]; sum += s.value[k]; if (s.value[k] < lo) lo = s.value[k]; }
        ok &= kept == 6 && s.total == sum - lo;
    }
    CHECK(ok && !strcmp(p.label, "Stats") && !strcmp(p.comment, "classic"), "seven stats, drop the lowest");

    qd_parse("(5 x 6d10 t8) kl2", &p, &err);
    qd_set_run(&p, &s, NULL, NULL);
    CHECK(s.counting && s.value[0] >= 0 && s.value[0] <= 6, "a set of success rolls keeps by successes");

    qd_parse("6 x 4d6k3", &p, &err);
    qd_set_run(&p, &s, NULL, NULL);
    ok = s.count == 6;
    for (int k = 0; k < 6; k++) ok &= s.kept[k];
    CHECK(ok, "a plain repeat keeps every roll");

    static const struct { const char *in, *out; } fmt[] = {
        { "(6x4d6)k3",                      "(6 x 4d6) k3" },
        { "(Stats) (7x4d6k3)k6 ! classic",  "(Stats) (7 x 4d6 k3) k6 ! classic" },
        { "(3 x 1d20+5 vs 15)",             "(3 x 1d20 + 5 vs 15)" },
        { "(4 x sw8) dh1",                  "(4 x sw8) dh1" },
        { "((Fire) 3x8d6)d1",               "(Fire) (3 x 8d6) d1" },
    };
    for (size_t c = 0; c < sizeof(fmt) / sizeof(fmt[0]); c++) {
        char out[96], twice[96];
        bool good = qd_parse(fmt[c].in, &p, &err);
        qd_format(&p, out, sizeof(out));
        static qd_parsed_t q;
        good = good && qd_parse(out, &q, &err);
        qd_format(&q, twice, sizeof(twice));
        CHECK(good && !strcmp(out, fmt[c].out) && !strcmp(out, twice),
              "'%s' written '%s' (wanted '%s')", fmt[c].in, out, fmt[c].out);
    }

    static const struct { const char *text; int col; const char *says; } bad[] = {
        { "(6x4d6)k7",    8, "only 6 rolls" },
        { "(6x4d6) t3",   9, "after a set" },
        { "(6x4d6",       1, "missing ')' for this set" },
        { "(6x4d6)k3 + 2", 11, "after a set" },
    };
    for (size_t c = 0; c < sizeof(bad) / sizeof(bad[0]); c++) {
        bool parsed_ok = qd_parse(bad[c].text, &p, &err);
        CHECK(!parsed_ok && err.pos + 1 == bad[c].col && strstr(err.message, bad[c].says),
              "'%s': column %d '%s'", bad[c].text, err.pos + 1, err.message);
    }
    CHECK(!qd_parse("3 x (6x4d6)k3", &p, &err), "a set can't be repeated again");
}

int main(void)
{
    qd_rng_init_hw();
    test_all();
    test_chosen();
    test_matching();
    test_keep();
    test_sets();
    return qd_test_done("rerolls and sets");
}
