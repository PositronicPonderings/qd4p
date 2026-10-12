/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_rng.c
 * @brief   The generator (xoshiro128**), unbiased ranges, seeding, stirring.
 *
 * Portable C: no Pico code here. The two platform hooks (hardware seed and
 * microsecond clock) live in qd_entropy_pico.c / qd_entropy_host.c.
 */
#include "qd_rng.h"

/* ========================================================================
 * xoshiro128** (Blackman & Vigna, public domain algorithm)
 *
 * 128 bits of state, 32-bit output, period 2^128 - 1, passes the standard
 * statistical test batteries, and costs a handful of shifts and adds per
 * number. The one rule: the state must never be all zeros (it would stay
 * zero forever), which keep_nonzero() below enforces.
 * ======================================================================== */
static uint32_t s[4] = { 0x9E3779B9u, 0x243F6A88u, 0xB7E15162u, 0x6A09E667u };
static bool     replay;
static uint32_t stirs;

static inline uint32_t rotl(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }

uint32_t qd_rng_next32(void)
{
    const uint32_t result = rotl(s[1] * 5u, 7) * 9u;
    const uint32_t t = s[1] << 9;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 11);
    return result;
}

static void keep_nonzero(void)
{
    if (!(s[0] | s[1] | s[2] | s[3]))
        s[0] = 0x9E3779B9u;
}

/* ========================================================================
 * Unbiased ranges (Lemire's method)
 *
 * Multiply a random 32-bit x by n as a 64-bit number: the top 32 bits are a
 * value 0..n-1. A few x values would make some results slightly more
 * likely; they're recognised by the low 32 bits and rerolled. Rerolls are
 * rare (at most n in 2^32), so it's usually one multiply and no division.
 * ======================================================================== */
uint32_t qd_rng_range(uint32_t n)
{
    if (n == 0)
        return 0;
    uint64_t m = (uint64_t)qd_rng_next32() * n;
    uint32_t low = (uint32_t)m;
    if (low < n) {
        uint32_t threshold = (0u - n) % n;          /* 2^32 mod n */
        while (low < threshold) {
            m = (uint64_t)qd_rng_next32() * n;
            low = (uint32_t)m;
        }
    }
    return (uint32_t)(m >> 32);
}

int32_t qd_rng_between(int32_t lo, int32_t hi)
{
    if (lo > hi) { int32_t t = lo; lo = hi; hi = t; }
    uint64_t span = (uint64_t)((int64_t)hi - (int64_t)lo) + 1u;
    if (span > 0xFFFFFFFFu)                          /* the whole int32 range */
        return (int32_t)qd_rng_next32();
    return (int32_t)((int64_t)lo + qd_rng_range((uint32_t)span));
}

/* ========================================================================
 * Mixing
 *
 * mix32 is the finaliser from MurmurHash3: every input bit affects every
 * output bit about half the time. Stirred words are mixed and XORed into
 * the state, then the generator is run a few steps so the new bits spread
 * through all 128 bits. XORing in something independent of the state can
 * never make the state more predictable, which is why stirring is safe
 * whatever the source.
 * ======================================================================== */
static uint32_t mix32(uint32_t h)
{
    h ^= h >> 16; h *= 0x85EBCA6Bu;
    h ^= h >> 13; h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

static void absorb(uint32_t word, uint32_t position)
{
    s[position & 3] ^= mix32(word ^ (position * 0x9E3779B9u));
}

static void diffuse(void)
{
    keep_nonzero();
    for (int i = 0; i < 8; i++)
        (void)qd_rng_next32();
}

bool qd_rng_stir(const void *bytes, size_t len)
{
    if (replay || !bytes || !len)
        return false;
    const uint8_t *p = (const uint8_t *)bytes;
    uint32_t position = 0;     /* depends only on state + input, so a saved
                                  state replays stirs exactly too */
    for (size_t i = 0; i < len; i += 4) {
        uint32_t w = 0;
        for (size_t b = 0; b < 4 && i + b < len; b++)
            w |= (uint32_t)p[i + b] << (8 * b);
        absorb(w, position++);
    }
    absorb((uint32_t)len, position);                  /* "ab"+"c" differs from "a"+"bc" */
    diffuse();
    stirs++;
    return true;
}

bool qd_rng_stir32(uint32_t v)
{
    return qd_rng_stir(&v, sizeof(v));
}

uint32_t qd_rng_stir_count(void) { return stirs; }

void qd_rng_save(uint32_t state[4])
{
    for (int i = 0; i < 4; i++) state[i] = s[i];
}

void qd_rng_restore(const uint32_t state[4])
{
    for (int i = 0; i < 4; i++) s[i] = state[i];
    keep_nonzero();
}

/* ========================================================================
 * Seeding
 * ======================================================================== */

/* splitmix64: turns one 64-bit seed into a stream of well-mixed values. */
static uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

void qd_rng_seed_fixed(uint64_t seed)
{
    uint64_t x = seed;
    uint64_t a = splitmix64(&x), b = splitmix64(&x);
    s[0] = (uint32_t)a; s[1] = (uint32_t)(a >> 32);
    s[2] = (uint32_t)b; s[3] = (uint32_t)(b >> 32);
    keep_nonzero();
    replay = true;
    stirs = 0;
}

bool qd_rng_is_replay(void) { return replay; }

void qd_rng_init_hw(void)
{
    uint32_t hw[4];
    qd_platform_seed(hw, 4);
    if (replay) {                                     /* leaving replay: start afresh */
        for (int i = 0; i < 4; i++) s[i] = hw[i];
        replay = false;
        stirs = 0;
        diffuse();
    } else {                                          /* already live: add to what's there */
        qd_rng_stir(hw, sizeof(hw));
    }
}

/* ========================================================================
 * Entropy sources
 * ======================================================================== */
uint32_t qd_rng_stir_source(const qd_entropy_source_t *src, uint32_t samples)
{
    if (!src || !src->sample || replay)
        return 0;
    if (src->begin && !src->begin(src->ctx))
        return 0;
    uint32_t done = 0;
    for (uint32_t i = 0; i < samples; i++) {
        uint32_t t0 = qd_platform_micros();
        uint32_t v = src->sample(src->ctx);
        uint32_t pair[2] = { v, qd_platform_micros() - t0 };   /* reading + its timing */
        if (qd_rng_stir(pair, sizeof(pair)))
            done++;
    }
    if (src->end)
        src->end(src->ctx);
    return done;
}
