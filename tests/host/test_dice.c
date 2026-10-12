/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_dice.c
 * @brief   D2: standard, custom (weighted, labelled), Fudge and
 *          percentile dice, including Call of Cthulhu bonus/penalty dice.
 *
 * Goldens come from rng_reference.py (seed 42). Distributions are checked
 * by chi-square at p = 0.001 against expectations worked out exactly.
 */
#include <string.h>

#include "qd4p.h"
#include "qd_test.h"

static double chi_square_vs(const uint32_t *counts, const double *expect_share, int bins,
                            uint32_t total)
{
    double chi = 0;
    for (int i = 0; i < bins; i++) {
        if (expect_share[i] <= 0) continue;
        double e = expect_share[i] * total, d = counts[i] - e;
        chi += d * d / e;
    }
    return chi;
}

static const qd_face_t loaded_faces[6] = {
    { 1, 1, "" }, { 2, 1, "" }, { 3, 1, "" }, { 4, 1, "" }, { 5, 1, "" }, { 6, 3, "" },
};

static void test_golden(void)
{
    static const int loaded42[20] = { 4, 6, 1, 5, 6, 5, 6, 5, 6, 6, 6, 3, 6, 5, 2, 3, 5, 3, 5, 3 };
    static const int fudge42[5]   = { 0, 1, 2, 0, 0 };
    qd_die_t d;

    qd_die_custom(&d, loaded_faces, 6);
    qd_rng_seed_fixed(42);
    for (int i = 0; i < 20; i++) {
        int v = qd_die_roll(&d).value;
        CHECK(v == loaded42[i], "loaded d6 roll %d: %d, reference %d", i, v, loaded42[i]);
    }

    qd_die_fudge(&d);
    qd_rng_seed_fixed(42);
    for (int i = 0; i < 5; i++) {
        int sum = 0;
        for (int k = 0; k < 4; k++) sum += qd_die_roll(&d).value;
        CHECK(sum == fudge42[i], "4dF roll %d: %d, reference %d", i, sum, fudge42[i]);
    }

    /* CoC: units, tens dice, kept, result */
    static const struct { int units, n, tens[3], kept, result; } b2[4] = {
        { 4, 3, { 9, 0, 5 }, 1, 4 }, { 6, 3, { 5, 6, 5 }, 0, 56 },
        { 8, 3, { 6, 7, 2 }, 2, 28 }, { 8, 3, { 5, 1, 3 }, 1, 18 },
    }, p1[4] = {
        { 4, 2, { 9, 0 }, 0, 94 }, { 5, 2, { 6, 5 }, 0, 65 },
        { 6, 2, { 5, 8 }, 1, 86 }, { 6, 2, { 7, 2 }, 0, 76 },
    };
    qd_percentile_t p;
    qd_rng_seed_fixed(42);
    for (int i = 0; i < 4; i++) {
        qd_roll_percentile(2, &p);
        int ok = p.units == b2[i].units && p.tens_count == b2[i].n && p.kept == b2[i].kept &&
                 p.result == b2[i].result;
        for (int k = 0; k < b2[i].n; k++) ok &= p.tens[k] == b2[i].tens[k];
        CHECK(ok, "percentile b2 roll %d: got %d (kept %d), reference %d", i, p.result, p.kept,
              b2[i].result);
    }
    qd_rng_seed_fixed(42);
    for (int i = 0; i < 4; i++) {
        qd_roll_percentile(-1, &p);
        int ok = p.units == p1[i].units && p.tens_count == p1[i].n && p.kept == p1[i].kept &&
                 p.result == p1[i].result;
        for (int k = 0; k < p1[i].n; k++) ok &= p.tens[k] == p1[i].tens[k];
        CHECK(ok, "percentile p1 roll %d: got %d, reference %d", i, p.result, p1[i].result);
    }
}

static void test_standard(void)
{
    qd_die_t d;
    CHECK(!qd_die_standard(&d, 0), "a 0-sided die is refused");
    CHECK(!qd_die_standard(&d, 32768) && qd_die_standard(&d, 32767), "up to 32767 sides (int16 values)");
    qd_die_standard(&d, 7);                              /* DCC d7 */
    static uint32_t counts[7];
    memset(counts, 0, sizeof(counts));
    int ok = 1;
    for (int i = 0; i < 700000; i++) {
        qd_face_roll_t r = qd_die_roll(&d);
        ok &= r.value == r.face + 1 && r.value >= 1 && r.value <= 7;
        counts[r.face]++;
    }
    CHECK(ok, "standard d7: value = face + 1, always 1..7");
    double share[7];
    for (int i = 0; i < 7; i++) share[i] = 1.0 / 7;
    double chi = chi_square_vs(counts, share, 7, 700000);
    CHECK(chi < 22.46, "d7 chi-square %.1f (limit 22.5)", chi);
    CHECK(qd_die_face_ppm(&d, 0) == 142857, "d7 face chance is 1/7");
}

static void test_custom(void)
{
    qd_die_t d;
    CHECK(!qd_die_custom(&d, loaded_faces, 0), "zero faces refused");
    qd_die_custom(&d, loaded_faces, 6);
    CHECK(d.weighted && d.total_weight == 8, "loaded die: weighted, 8 tickets");
    CHECK(qd_die_face_ppm(&d, 5) == 375000, "the 6 comes up 3/8 of the time");

    static uint32_t counts[6];
    memset(counts, 0, sizeof(counts));
    qd_rng_init_hw();
    for (int i = 0; i < 800000; i++) counts[qd_die_roll(&d).face]++;
    double share[6] = { 0.125, 0.125, 0.125, 0.125, 0.125, 0.375 };
    double chi = chi_square_vs(counts, share, 6, 800000);
    CHECK(chi < 20.52, "loaded d6 matches its weights: chi-square %.1f", chi);
    printf("  loaded d6: the six came up %.2f%% (expected 37.50%%)\n", counts[5] * 100.0 / 800000);

    /* Duplicate values and labels stay apart by face. */
    static const qd_face_t odd[4] = { { 3, 1, "a" }, { 3, 1, "b" }, { 0, 2, "+" }, { -2, 1, "" } };
    qd_die_custom(&d, odd, 4);
    CHECK(qd_die_face_value(&d, 1) == 3 && !strcmp(qd_die_face_label(&d, 1), "b"),
          "face 1 is value 3 labelled b");
    CHECK(!strcmp(qd_die_face_label(&d, 2), "+") && qd_die_face_value(&d, 3) == -2,
          "labels and negative values kept");
    memset(counts, 0, sizeof(counts));
    for (int i = 0; i < 500000; i++) counts[qd_die_roll(&d).face]++;
    double share4[4] = { 0.2, 0.2, 0.4, 0.2 };
    chi = chi_square_vs(counts, share4, 4, 500000);
    CHECK(chi < 16.27, "duplicate-value die matches its weights: chi-square %.1f", chi);

    /* Weight 0 is treated as 1; overlong labels are cut. */
    qd_face_t z[2] = { { 1, 0, "" }, { 2, 1, "" } };
    strcpy(z[1].label, "abcd");
    qd_die_custom(&d, z, 2);
    CHECK(!d.weighted && d.total_weight == 2, "weight 0 becomes 1");

    qd_die_fudge(&d);
    CHECK(d.sides == 6 && qd_die_face_value(&d, 0) == -1 && !strcmp(qd_die_face_label(&d, 4), "+"),
          "Fudge die: -1 -1 0 0 +1 +1 with labels");
}

/* Exact distribution of percentile dice with `bonus`, by trying every
 * combination of units and tens dice. */
static void exact_percentile(int bonus, double share[101])
{
    int n = 1 + (bonus < 0 ? -bonus : bonus);
    memset(share, 0, 101 * sizeof(double));
    int combos = 1;
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
}

static void test_percentile(void)
{
    for (int bonus = -3; bonus <= 3; bonus++) {
        static uint32_t counts[101];
        double share[101];
        memset(counts, 0, sizeof(counts));
        exact_percentile(bonus, share);
        int ok = 1;
        qd_percentile_t p;
        for (int i = 0; i < 1000000; i++) {
            uint8_t v = qd_roll_percentile(bonus, &p);
            ok &= v >= 1 && v <= 100 && p.tens_count == 1 + (bonus < 0 ? -bonus : bonus);
            counts[v]++;
        }
        CHECK(ok, "bonus %d: results 1..100, right number of tens dice", bonus);
        double chi = chi_square_vs(counts, share, 101, 1000000);
        CHECK(chi < 148.23, "bonus %+d matches the exact distribution: chi-square %.1f", bonus, chi);
        double mean = 0;
        for (int v = 1; v <= 100; v++) mean += v * (double)counts[v] / 1000000;
        printf("  d100 %s%d: average %.1f, chi-square %.1f vs exact\n",
               bonus > 0 ? "bonus " : bonus < 0 ? "penalty " : "plain ",
               bonus < 0 ? -bonus : bonus, mean, chi);
    }
    qd_percentile_t p;
    qd_roll_percentile(7, &p);
    CHECK(p.bonus == 3 && p.tens_count == 4, "bonus is capped at 3");
    qd_roll_percentile(-9, &p);
    CHECK(p.bonus == -3 && p.tens_count == 4, "penalty is capped at 3");
}

int main(void)
{
    test_golden();
    qd_rng_init_hw();
    test_standard();
    test_custom();
    test_percentile();
    return qd_test_done("dice");
}
