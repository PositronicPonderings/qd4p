/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_rng.c
 * @brief   D1: the generator, unbiased ranges, replay, stirring, sources.
 *
 * Golden numbers come from rng_reference.py, an independent Python version
 * written from the published algorithms. The same numbers appear on the
 * Pico console after `seed 42`, which checks the device too.
 *
 * Statistical checks use chi-square at p = 0.001: a correct generator fails
 * one about once in a thousand runs, a biased one fails every time.
 */
#include <stdint.h>
#include <string.h>

#include "qd4p.h"
#include "qd_test.h"

/* Chi-square of counts against a uniform expectation. */
static double chi_square(const uint32_t *counts, int bins, uint32_t total)
{
    double expect = (double)total / bins, chi = 0;
    for (int i = 0; i < bins; i++) {
        double d = counts[i] - expect;
        chi += d * d / expect;
    }
    return chi;
}

static void test_golden(void)
{
    static const uint32_t next42[8] = { 1776835114u, 4165204688u, 17111135u, 2317295270u,
                                        2792088233u, 2554630222u, 2940343271u, 2244566231u };
    static const int d6_42[20] = { 3, 6, 1, 4, 4, 4, 5, 4, 5, 5, 5, 2, 5, 4, 1, 3, 4, 3, 4, 3 };

    qd_rng_seed_fixed(42);
    for (int i = 0; i < 8; i++) {
        uint32_t v = qd_rng_next32();
        CHECK(v == next42[i], "seed 42, output %d: got %u, reference %u", i, v, next42[i]);
    }
    qd_rng_seed_fixed(42);
    for (int i = 0; i < 20; i++) {
        int v = qd_rng_between(1, 6);
        CHECK(v == d6_42[i], "seed 42, d6 roll %d: got %d, reference %d", i, v, d6_42[i]);
    }
}

static void test_replay(void)
{
    uint32_t a[1000];
    qd_rng_seed_fixed(7);
    for (int i = 0; i < 1000; i++) a[i] = qd_rng_next32();
    qd_rng_seed_fixed(7);
    int same = 1;
    for (int i = 0; i < 1000; i++) same &= (qd_rng_next32() == a[i]);
    CHECK(same, "the same seed replays the same 1000 numbers");
    CHECK(qd_rng_is_replay(), "replay mode is on after seed_fixed");
    CHECK(!qd_rng_stir("x", 1) && !qd_rng_stir32(5), "stirring is refused in replay mode");
    qd_rng_seed_fixed(7);
    CHECK(qd_rng_next32() == a[0], "a refused stir changed nothing");

    qd_rng_seed_fixed(8);
    CHECK(qd_rng_next32() != a[0], "a different seed gives a different stream");

    qd_rng_init_hw();
    CHECK(!qd_rng_is_replay(), "init_hw leaves replay mode");
    CHECK(qd_rng_stir32(1) && qd_rng_stir_count() == 1, "stirring works again, and is counted");
}

static void test_uniform(void)
{
    static const struct { uint32_t sides, rolls; double limit; } cases[] = {
        { 6, 6000000, 20.52 },      /* chi-square limits at p = 0.001 */
        { 20, 2000000, 43.82 },
        { 100, 1000000, 148.23 },
        { 3, 3000000, 13.82 },
    };
    qd_rng_init_hw();
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        static uint32_t counts[100];
        memset(counts, 0, sizeof(counts));
        uint32_t n = cases[c].sides;
        for (uint32_t i = 0; i < cases[c].rolls; i++) {
            int32_t v = qd_rng_between(1, (int32_t)n);
            if (v < 1 || v > (int32_t)n) { CHECK(0, "d%u gave %d", n, v); return; }
            counts[v - 1]++;
        }
        double chi = chi_square(counts, (int)n, cases[c].rolls);
        CHECK(chi < cases[c].limit, "d%u over %u rolls: chi-square %.1f, limit %.1f",
              n, cases[c].rolls, chi, cases[c].limit);
        printf("  d%-3u %8u rolls  chi-square %6.1f (limit %.1f)\n",
               n, cases[c].rolls, chi, cases[c].limit);
    }
}

static void test_no_modulo_bias(void)
{
    /* With "% n", n = 3 x 2^30 makes values below 2^30 twice as likely
     * (half the draws instead of a third). Rejection keeps it at a third. */
    const uint32_t n = 0xC0000000u;
    uint32_t low = 0, total = 300000;
    for (uint32_t i = 0; i < total; i++)
        if (qd_rng_range(n) < 0x40000000u) low++;
    double share = (double)low / total;
    CHECK(share > 0.323 && share < 0.343, "share below 2^30 is %.4f, should be 0.3333", share);
    printf("  range(3 x 2^30): %.4f of draws in the first third (biased would be 0.5)\n", share);
}

static void test_bounds(void)
{
    static const uint32_t ns[] = { 1, 2, 7, 1000, 0x80000001u, 0xFFFFFFFEu, 0xFFFFFFFFu };
    for (size_t i = 0; i < sizeof(ns) / sizeof(ns[0]); i++) {
        int ok = 1;
        for (int k = 0; k < 20000; k++) ok &= qd_rng_range(ns[i]) < ns[i];
        CHECK(ok, "range(%u) stays below %u", ns[i], ns[i]);
    }
    CHECK(qd_rng_range(0) == 0, "range(0) is 0");
    int ok = 1;
    for (int k = 0; k < 20000; k++) {
        int32_t v = qd_rng_between(-5, 5);
        ok &= v >= -5 && v <= 5;
    }
    CHECK(ok, "between(-5, 5) stays in range");
    ok = 1;
    for (int k = 0; k < 2000; k++) {
        int32_t v = qd_rng_between(6, 1);
        ok &= v >= 1 && v <= 6;
    }
    CHECK(ok, "between(6, 1) swaps its ends");
    (void)qd_rng_between(INT32_MIN, INT32_MAX);         /* must not crash or loop */
    CHECK(qd_rng_between(4, 4) == 4, "between(4, 4) is 4");
}

static void test_bits(void)
{
    uint32_t ones[32] = { 0 }, total = 1000000;
    for (uint32_t i = 0; i < total; i++) {
        uint32_t v = qd_rng_next32();
        for (int b = 0; b < 32; b++) ones[b] += (v >> b) & 1;
    }
    int ok = 1;
    for (int b = 0; b < 32; b++) {
        double f = (double)ones[b] / total;
        if (f < 0.497 || f > 0.503) { ok = 0; CHECK(0, "bit %d set %.4f of the time", b, f); }
    }
    CHECK(ok, "every bit is set about half the time");
}

static void test_stir(void)
{
    uint32_t base[4], after[4];
    qd_rng_init_hw();
    qd_rng_save(base);

    qd_rng_stir("abc", 3);
    uint32_t a = qd_rng_next32();
    qd_rng_restore(base);
    qd_rng_stir("abd", 3);
    uint32_t b = qd_rng_next32();
    qd_rng_restore(base);
    qd_rng_stir("abc", 3);
    uint32_t a2 = qd_rng_next32();
    CHECK(a != b, "one changed byte gives a different stream");
    CHECK(a == a2, "stirring is deterministic from a given state");

    qd_rng_restore(base);
    uint32_t plain = qd_rng_next32();
    CHECK(plain != a, "stirring changes the stream");

    qd_rng_restore(base);
    qd_rng_stir("ab", 2); qd_rng_stir("c", 1);
    uint32_t split = qd_rng_next32();
    CHECK(split != a, "\"ab\" then \"c\" differs from \"abc\"");

    uint32_t zero[4] = { 0, 0, 0, 0 };
    qd_rng_restore(zero);
    qd_rng_save(after);
    CHECK(after[0] | after[1] | after[2] | after[3], "an all-zero state is made non-zero");
    CHECK(qd_rng_next32() != 0 || qd_rng_next32() != 0, "and the generator runs");
}

/* A fake entropy source that counts how it is used. */
typedef struct { int begun, ended, samples; bool fail; } fake_t;
static bool fake_begin(void *c) { fake_t *f = c; f->begun++; return !f->fail; }
static uint32_t fake_sample(void *c) { fake_t *f = c; return (uint32_t)++f->samples; }
static void fake_end(void *c) { ((fake_t *)c)->ended++; }

static void test_sources(void)
{
    fake_t f = { 0, 0, 0, false };
    qd_entropy_source_t src = { "Fake", fake_begin, fake_sample, fake_end, &f };

    qd_rng_init_hw();
    uint32_t before = qd_rng_stir_count();
    uint32_t n = qd_rng_stir_source(&src, 16);
    CHECK(n == 16 && f.samples == 16 && f.begun == 1 && f.ended == 1,
          "16 samples: stirred %u, sampled %d, begin %d, end %d", n, f.samples, f.begun, f.ended);
    CHECK(qd_rng_stir_count() == before + 16, "each sample counts as a stir");

    f.fail = true;
    CHECK(qd_rng_stir_source(&src, 4) == 0 && f.samples == 16, "a source that fails to begin is skipped");

    f.fail = false;
    qd_rng_seed_fixed(1);
    CHECK(qd_rng_stir_source(&src, 4) == 0, "sources are refused in replay mode");

    qd_entropy_source_t bare = { "Bare", NULL, fake_sample, NULL, &f };
    qd_rng_init_hw();
    CHECK(qd_rng_stir_source(&bare, 3) == 3, "begin and end are optional");
}

int main(void)
{
    test_golden();
    test_replay();
    test_uniform();
    test_no_modulo_bias();
    test_bounds();
    test_bits();
    test_stir();
    test_sources();
    return qd_test_done("randomness");
}
