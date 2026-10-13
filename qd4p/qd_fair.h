/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_fair.h
 * @brief   Fair mode: rolls the players can check after the session.
 *
 * QUICK START
 *
 *     char words[QD_FAIR_TEXT];
 *     qd_fair_start();                         // a new secret key
 *     qd_fair_fingerprint_words(words, sizeof(words));
 *     show(words);                             // "amber otter canyon delta ember fern"
 *
 *     uint32_t n = qd_fair_begin();            // before each action: #1, #2, ...
 *     qd_roll_run(&roll, &res);                // its dice come from the key and n
 *
 *     uint8_t key[QD_FAIR_KEY_BYTES];
 *     qd_fair_end(key);                        // the session is over: reveal the key
 *     qd_fair_words(key, QD_FAIR_KEY_BYTES, words, sizeof(words));
 *     show(words);                             // 16 words
 *
 * THE PROMISE
 *   At the start, the device picks a secret 128-bit key and shows its
 *   FINGERPRINT: six words worked out from the key with SHA-256. Everyone
 *   writes the fingerprint down. Each action (a roll, a reroll, a deal, a
 *   table roll) gets the next NUMBER, and its dice come only from the key
 *   and that number. At the end the key is revealed. Anyone can then:
 *
 *     1. check that the key gives the fingerprint written down at the start
 *        (so the key wasn't swapped during the session), and
 *     2. replay any action from its number, on a PC, and see the same dice
 *        (so nothing was changed after it was rolled).
 *
 *   Each action can be checked on its own: rolls nobody wrote down don't
 *   matter, and a roll can't be redone with a different number.
 *
 *   What it doesn't promise: the key is the device's alone, so a device
 *   with doctored firmware could choose a key it likes before the session.
 *   Fair mode shows the device didn't change anything DURING the session;
 *   trusting the device's firmware is a separate matter. docs/fair-mode.md
 *   has the details.
 *
 * WHILE FAIR MODE IS ON
 *   The generator is held: stirring, qd_rng_init_hw() and replay mode are
 *   ignored. Anything rolled without calling qd_fair_begin() first carries
 *   on from the last action's numbers: it is still fixed by the key, but
 *   can only be checked by replaying that whole action.
 *
 * THE MATHS (for writing another checker)
 *   fingerprint = first 6 bytes of SHA-256("QD4P fair 1 print" || key)
 *   action n    = generator state from the first 16 bytes of
 *                 SHA-256("QD4P fair 1 roll" || key || n), n as 4 bytes,
 *                 little-endian; the 16 bytes are read as four
 *                 little-endian 32-bit words, xoshiro128** state s[0..3].
 *   The strings are plain ASCII with no terminating 0.
 *   Words: byte b is word b of the list in qd_fair.c (256 words).
 */
#ifndef QD_FAIR_H
#define QD_FAIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QD_FAIR_KEY_BYTES    16     /**< the secret key: 128 bits, 16 words */
#define QD_FAIR_PRINT_BYTES  6      /**< the fingerprint: 48 bits, 6 words */
#define QD_FAIR_TEXT         120    /**< room for 16 words as text, with spaces */

/** Start a session with a new secret key: from the hardware random source
 *  and everything stirred into the generator so far. Returns false (and
 *  changes nothing) if a session is already running. */
bool qd_fair_start(void);

/** Start a session with a key you already have: for checking a session
 *  after its key was revealed, and for tests. Replaces any running session. */
void qd_fair_start_key(const uint8_t key[QD_FAIR_KEY_BYTES]);

/** True while a session is running. */
bool qd_fair_is_on(void);

/** Before each action: the next number (1, 2, 3, ...), with the generator
 *  set for it. Returns 0 (and does nothing) when fair mode is off. */
uint32_t qd_fair_begin(void);

/** Checking: set the generator for action `n` (and carry on from n + 1).
 *  Only for a session started with qd_fair_start_key(): on a live session
 *  it would let an action be redone, so it does nothing and returns false. */
bool qd_fair_begin_at(uint32_t n);

/** The number of the last action begun (0 before the first). */
uint32_t qd_fair_count(void);

/** The fingerprint's 6 bytes, or false when fair mode is off. */
bool qd_fair_fingerprint(uint8_t out[QD_FAIR_PRINT_BYTES]);

/** The fingerprint as six words ("amber otter canyon delta ember fern").
 *  Returns the length, or 0 when fair mode is off. */
int qd_fair_fingerprint_words(char *buf, int size);

/** End the session: copy the key out (key may be NULL), let go of the
 *  generator and reseed it from the hardware. Returns false if no session
 *  was running. */
bool qd_fair_end(uint8_t key[QD_FAIR_KEY_BYTES]);

/** The fingerprint of any key: for a checker to compare with the one
 *  written down at the start. */
void qd_fair_print_of(const uint8_t key[QD_FAIR_KEY_BYTES], uint8_t out[QD_FAIR_PRINT_BYTES]);

/* --- Words ---------------------------------------------------------------- */

/** Word `b` of the list (256 words, a-z, 3 to 6 letters; no two start
 *  with the same three letters). */
const char *qd_fair_word(uint8_t b);

/** Bytes as words, separated by spaces. Returns the length written. */
int qd_fair_words(const uint8_t *bytes, int n, char *buf, int size);

/** Words back to bytes. Any case; spaces, commas or hyphens between; a
 *  word may be cut to its first 3 (or more) letters. Returns how many
 *  bytes were read, or -(k + 1) if the k-th word (from 0) isn't one. */
int qd_fair_read_words(const char *text, uint8_t *out, int max);

/* --- SHA-256 ---------------------------------------------------------------- */

/** SHA-256 of `len` bytes (FIPS 180-4). Fair mode's hash; here for tests
 *  and anything else that needs one. */
void qd_sha256(const void *data, size_t len, uint8_t out[32]);

#ifdef __cplusplus
}
#endif

#endif /* QD_FAIR_H */
