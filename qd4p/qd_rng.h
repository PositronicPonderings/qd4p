/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_rng.h
 * @brief   Randomness: the one generator every roll comes from.
 *
 * QUICK START
 *
 *     qd_rng_init_hw();                  // once at boot (Pico: hardware seed)
 *     uint32_t d20 = qd_rng_between(1, 20);
 *
 * HOW IT WORKS
 *   A small, fast pseudo-random generator (xoshiro128**) holds 128 bits of
 *   state. At boot that state is filled from the RP2350's hardware random
 *   source, so every power-up starts somewhere new and unguessable. From
 *   then on each number comes from the state in a few nanoseconds.
 *
 *   Anything can be STIRRED in at any time: a typed number, a light-sensor
 *   reading, the microseconds between encoder clicks. Stirring mixes new
 *   bits into the state; it can only add unpredictability, never remove it,
 *   so even a poor source is safe to use.
 *
 * REPLAY MODE
 *   qd_rng_seed_fixed(42) replaces the state with one made from the number
 *   42, so the same rolls come out every time, on the Pico and on a PC. For
 *   tests, bug reports and teaching. Stirring is ignored while replay mode
 *   is on (it would break the replay); qd_rng_init_hw() leaves it.
 *
 * FAIR MODE
 *   qd_fair.h holds the generator for a fair session: each action's rolls
 *   come from a secret key and the action's number, and stirring, seeding
 *   and replay are all ignored until the session ends.
 *
 * UNBIASED NUMBERS
 *   "random % 6" is very slightly unfair: 2^32 isn't a multiple of 6, so the
 *   low faces come up a hair more often. qd_rng_range() rejects the few
 *   leftover values instead, so every face is exactly equally likely.
 */
#ifndef QD_RNG_H
#define QD_RNG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Numbers ------------------------------------------------------------- */

/** The next 32 random bits. */
uint32_t qd_rng_next32(void);

/** A whole number from 0 to n-1, every value equally likely. n = 0 gives 0. */
uint32_t qd_rng_range(uint32_t n);

/** A whole number from lo to hi inclusive, every value equally likely.
 *  qd_rng_between(1, 6) is a d6. If lo > hi they are swapped. */
int32_t qd_rng_between(int32_t lo, int32_t hi);

/* --- Seeding and stirring ------------------------------------------------- */

/** Seed from the hardware random source and leave replay mode. On the Pico
 *  this uses the RP2350 TRNG (qd_entropy_pico.c). On a PC build it uses
 *  the clock and the address of a stack variable: fine for trying things,
 *  not for anything that matters. Call once at boot; calling again later
 *  just stirs in fresh hardware randomness. Ignored in fair mode. */
void qd_rng_init_hw(void);

/** Replay mode: replace the state with one built from `seed`, so the same
 *  sequence follows on any machine. Stirring is ignored until
 *  qd_rng_init_hw() is called. Ignored in fair mode (end the session
 *  first: qd_fair_end). */
void qd_rng_seed_fixed(uint64_t seed);

/** True while replay mode is on. */
bool qd_rng_is_replay(void);

/** Mix bytes into the state. Returns false (and does nothing) in replay
 *  mode and fair mode. Any bytes will do: a typed seed, sensor readings, timings. */
bool qd_rng_stir(const void *bytes, size_t len);

/** Mix one 32-bit value in. Same as qd_rng_stir(&v, 4). */
bool qd_rng_stir32(uint32_t v);

/** Copy the generator's 128-bit state out, e.g. to save it with a game or
 *  to replay a stretch of rolls exactly. */
void qd_rng_save(uint32_t state[4]);

/** Put a saved state back. Replay mode is left as it was. An all-zero
 *  state (which would get stuck) is quietly made non-zero. */
void qd_rng_restore(const uint32_t state[4]);

/** Fair mode's hold (qd_fair.h uses these; an app calls qd_fair_*, not
 *  these). While held, the state is set only by qd_rng_hold(): stirring,
 *  qd_rng_init_hw() and qd_rng_seed_fixed() are ignored, so nothing can
 *  change a fair session's rolls. qd_rng_save()/qd_rng_restore() still
 *  work (qd_odds borrows the generator and puts it back). */
void qd_rng_hold(const uint32_t state[4]);
void qd_rng_release(void);
bool qd_rng_is_held(void);

/** Scripted draws, for working out exact odds: while a script is set,
 *  qd_rng_range(n) returns the script's next pick (and notes n) instead of
 *  a random number; past the end of the script it returns 0 and extends
 *  it. qd_odds uses this to visit every possible roll once. Not for
 *  ordinary use. */
typedef struct {
    uint32_t *pick;                     /**< the values to return, in order */
    uint32_t *size;                     /**< filled in: the n each draw asked for */
    uint16_t  len;                      /**< picks set so far */
    uint16_t  cap;                      /**< room in pick[] and size[] */
    uint16_t  pos;                      /**< draws made in this run */
    bool      over;                     /**< a run needed more than cap draws */
} qd_rng_script_t;

/** Start (or, with NULL, stop) scripted draws. */
void qd_rng_script(qd_rng_script_t *script);

/** How many qd_rng_stir calls have changed the state since the last seed
 *  (shown by apps as "entropy added: 37 times"). */
uint32_t qd_rng_stir_count(void);

/* --- Entropy sources ------------------------------------------------------
 * A standard way to describe anything that produces unpredictable readings,
 * so new randomness hardware is one small file. Fill in a struct and hand
 * it to qd_rng_stir_source(). begin/end may be NULL.
 *
 *     static uint32_t read_light(void *ctx) { return adc_read(); }
 *     static const qd_entropy_source_t light = { "Light sensor", NULL, read_light, NULL, NULL };
 *     qd_rng_stir_source(&light, 64);   // 64 readings stirred in
 */
typedef struct {
    const char *name;               /**< shown in menus, e.g. "Light sensor" */
    bool (*begin)(void *ctx);       /**< start up the source; false = unavailable */
    uint32_t (*sample)(void *ctx);  /**< one raw reading; quality may be poor */
    void (*end)(void *ctx);         /**< shut the source down */
    void *ctx;                      /**< passed to the functions above */
} qd_entropy_source_t;

/** Take `samples` readings from a source and stir each one in, together
 *  with how long the reading took on the microsecond clock where available.
 *  Returns the number of readings stirred (0 if the source failed to begin
 *  or replay mode is on). */
uint32_t qd_rng_stir_source(const qd_entropy_source_t *src, uint32_t samples);

/* --- Platform hooks (supplied by qd_entropy_pico.c or the host build) ---- */

/** Fill `out` with `words` 32-bit values of the best randomness available. */
void qd_platform_seed(uint32_t *out, size_t words);

/** A free-running microsecond counter (0 if the platform has none). */
uint32_t qd_platform_micros(void);

#ifdef __cplusplus
}
#endif

#endif /* QD_RNG_H */
