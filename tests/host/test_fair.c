/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* test_fair.c - fair mode: SHA-256, the words, the key and fingerprint,
 * numbered actions, and checking a session afterwards. The fingerprints
 * and action states are compared with fair_reference.py (Python hashlib). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qd4p.h"
#include "fair_goldens.h"

static int checks, failures;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("FAIL line %d: ", __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

static void hex(const uint8_t *b, int n, char *out)
{
    for (int i = 0; i < n; i++) sprintf(out + 2 * i, "%02x", b[i]);
}

static void sha_is(const void *data, size_t len, const char *want, const char *what)
{
    uint8_t h[32];
    char got[65];
    qd_sha256(data, len, h);
    hex(h, 32, got);
    CHECK(!strcmp(got, want), "SHA-256 of %s: %s", what, got);
}

static void test_sha256(void)
{
    sha_is("", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "nothing");
    sha_is("abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "abc");
    const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    sha_is(two, strlen(two), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
           "the 448-bit message (padding spills into a second block)");
    static char million[1000000];
    memset(million, 'a', sizeof(million));
    sha_is(million, sizeof(million), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
           "a million a's");
    /* every length round a block edge, against the one-shot of a copy */
    char buf[130];
    for (int i = 0; i < 130; i++) buf[i] = (char)i;
    uint8_t a[32], b[32];
    for (int n = 50; n < 130; n++) {
        char copy[130];
        memcpy(copy, buf, (size_t)n);
        qd_sha256(buf, (size_t)n, a);
        qd_sha256(copy, (size_t)n, b);
        CHECK(!memcmp(a, b, 32), "length %d is stable", n);
    }
}

static void test_words(void)
{
    int bad = 0;
    for (int i = 0; i < 256; i++) {
        const char *w = qd_fair_word((uint8_t)i);
        size_t n = strlen(w);
        if (n < 3 || n > 6) bad++;
        for (size_t k = 0; k < n; k++) if (w[k] < 'a' || w[k] > 'z') bad++;
        for (int j = 0; j < i; j++)
            if (!strncmp(w, qd_fair_word((uint8_t)j), 3)) bad++;
        if (i && strcmp(qd_fair_word((uint8_t)(i - 1)), w) >= 0) bad++;
    }
    CHECK(!bad, "256 words, 3-6 letters a-z, in order, no shared first three letters (%d problems)", bad);

    uint8_t all[256], back[256];
    for (int i = 0; i < 256; i++) all[i] = (uint8_t)i;
    static char text[256 * 7 + 1];
    qd_fair_words(all, 256, text, sizeof(text));
    CHECK(qd_fair_read_words(text, back, 256) == 256 && !memcmp(all, back, 256),
          "every word reads back as its byte");

    uint8_t b[8];
    CHECK(qd_fair_read_words("Amber, OTT-can  delt\temb fern", b, 8) == 6 &&
          !strcmp(qd_fair_word(b[0]), "amber") && !strcmp(qd_fair_word(b[1]), "otter") &&
          !strcmp(qd_fair_word(b[2]), "canyon") && !strcmp(qd_fair_word(b[3]), "delta") &&
          !strcmp(qd_fair_word(b[4]), "ember") && !strcmp(qd_fair_word(b[5]), "fern"),
          "any case, commas and hyphens, three letters or more");
    CHECK(qd_fair_read_words("amber ca otter", b, 8) == -2, "two letters aren't enough: word 2");
    CHECK(qd_fair_read_words("amber otter zzz", b, 8) == -3, "not a word: word 3");
    CHECK(qd_fair_read_words("amberx", b, 8) == -1, "a word with extra letters isn't one");
    CHECK(qd_fair_read_words("amber7", b, 8) == -1, "a digit stuck on isn't a word");
    CHECK(qd_fair_read_words("", b, 8) == 0, "nothing is no words");
    CHECK(qd_fair_read_words("amber otter canyon", b, 2) == 2, "stops at max");

    char small[12];
    uint8_t three[3] = { 0, 1, 2 };
    int n = qd_fair_words(three, 3, small, sizeof(small));
    CHECK(n == 11 && !strcmp(small, "acorn actor"), "a short buffer holds whole words only: '%s'", small);
}

static void test_goldens(void)
{
    char words[QD_FAIR_TEXT];
    for (size_t k = 0; k < sizeof(fair_keys) / sizeof(fair_keys[0]); k++) {
        uint8_t fp[QD_FAIR_PRINT_BYTES];
        qd_fair_print_of(fair_keys[k].key, fp);
        CHECK(!memcmp(fp, fair_keys[k].print, 6), "key %d: fingerprint as Python's", (int)k);
        qd_fair_start_key(fair_keys[k].key);
        qd_fair_fingerprint_words(words, sizeof(words));
        CHECK(!strcmp(words, fair_keys[k].words), "key %d: '%s', Python '%s'", (int)k, words,
              fair_keys[k].words);
    }
    int bad = 0;
    for (size_t i = 0; i < sizeof(fair_actions) / sizeof(fair_actions[0]); i++) {
        const fair_action_golden_t *g = &fair_actions[i];
        uint32_t st[4];
        qd_fair_start_key(fair_keys[g->key].key);
        qd_fair_begin_at(g->n);
        qd_rng_save(st);
        if (memcmp(st, g->state, sizeof(st))) { bad++; printf("  key %d #%u: state differs\n", g->key, g->n); }
        for (int k = 0; k < 4; k++)
            if (qd_rng_next32() != g->next[k]) { bad++; printf("  key %d #%u: output %d differs\n", g->key, g->n, k); }
    }
    CHECK(!bad, "every action's generator state and first outputs as Python's");
    qd_fair_end(NULL);
}

/* Roll some notation, for comparing sessions. */
static int32_t roll(const char *text, int32_t dice[8])
{
    static qd_parsed_t p;
    static qd_result_t r;
    qd_parse_error_t err;
    if (!qd_parse(text, &p, &err)) return -9999;
    qd_roll_run(&p.roll, &r);
    for (int i = 0; i < 8; i++) dice[i] = i < r.dice_count ? r.dice[i].value : 0;
    return r.total;
}

static void test_session(void)
{
    CHECK(!qd_fair_is_on() && qd_fair_begin() == 0, "off: begin does nothing");
    CHECK(!qd_fair_end(NULL), "off: nothing to end");
    char none[8] = "x";
    CHECK(qd_fair_fingerprint_words(none, sizeof(none)) == 0 && !none[0], "off: no fingerprint");

    qd_rng_init_hw();
    CHECK(qd_fair_start(), "a session starts");
    CHECK(!qd_fair_start(), "and won't start twice");
    CHECK(qd_fair_is_on() && qd_rng_is_held() && !qd_rng_is_replay(), "the generator is held");
    char print[QD_FAIR_TEXT];
    int len = qd_fair_fingerprint_words(print, sizeof(print));
    uint8_t parsed[8];
    CHECK(len > 0 && qd_fair_read_words(print, parsed, 8) == 6, "a six-word fingerprint: %s", print);
    CHECK(qd_fair_count() == 0, "no action yet");

    /* three actions; the second is interrupted by everything that would
     * normally change the generator, and by an odds calculation */
    int32_t live[3][8], live_total[3];
    CHECK(qd_fair_begin() == 1, "first action is #1");
    live_total[0] = roll("4d6k3", live[0]);
    CHECK(qd_fair_begin() == 2, "then #2");
    uint32_t before[4], after[4];
    qd_rng_save(before);
    CHECK(!qd_rng_stir("noise", 5), "stirring is ignored");
    qd_rng_init_hw();
    qd_rng_seed_fixed(42);
    qd_rng_save(after);
    CHECK(!memcmp(before, after, sizeof(before)) && !qd_rng_is_replay(),
          "hardware reseeds and replay mode are ignored");
    CHECK(!qd_fair_begin_at(1), "a live session can't go back to #1");
    static qd_parsed_t op;
    static qd_odds_t odds;
    qd_parse_error_t err;
    qd_parse("4d6k3", &op, &err);
    qd_odds(&op.roll, &odds);
    live_total[1] = roll("2d20 + 1d8", live[1]);
    CHECK(qd_fair_begin() == 3 && qd_fair_count() == 3, "then #3");
    live_total[2] = roll("10d10 t8", live[2]);

    uint8_t key[QD_FAIR_KEY_BYTES], zero[QD_FAIR_KEY_BYTES] = { 0 };
    CHECK(qd_fair_end(key) && memcmp(key, zero, sizeof(key)), "the end reveals the key");
    CHECK(!qd_fair_is_on() && !qd_rng_is_held() && qd_rng_stir("x", 1), "and lets go of the generator");

    /* checking: the revealed key, as typed back from its words */
    char kw[QD_FAIR_TEXT];
    qd_fair_words(key, QD_FAIR_KEY_BYTES, kw, sizeof(kw));
    uint8_t typed[QD_FAIR_KEY_BYTES];
    CHECK(qd_fair_read_words(kw, typed, QD_FAIR_KEY_BYTES) == 16 && !memcmp(typed, key, 16),
          "16 key words read back: %s", kw);
    qd_fair_start_key(typed);
    char again[QD_FAIR_TEXT];
    qd_fair_fingerprint_words(again, sizeof(again));
    CHECK(!strcmp(again, print), "the key gives the fingerprint shown at the start");
    int32_t d[8];
    /* out of order, and #2 without the stirring and odds in between */
    CHECK(qd_fair_begin_at(3) && roll("10d10 t8", d) == live_total[2] && !memcmp(d, live[2], sizeof(d)),
          "#3 checked on its own");
    CHECK(qd_fair_begin_at(2) && roll("2d20 + 1d8", d) == live_total[1] && !memcmp(d, live[1], sizeof(d)),
          "#2 checked: nothing in between changed it");
    CHECK(qd_fair_begin_at(1) && roll("4d6k3", d) == live_total[0] && !memcmp(d, live[0], sizeof(d)),
          "#1 checked");
    CHECK(qd_fair_begin() == 2, "after #1, begin carries on at #2");
    CHECK(roll("2d20 + 1d8", d) == live_total[1], "and #2 is the same again");

    /* a different key gives different dice and a different fingerprint */
    typed[0] ^= 1;
    qd_fair_start_key(typed);
    qd_fair_fingerprint_words(again, sizeof(again));
    qd_fair_begin_at(3);
    int32_t other = roll("10d10 t8", d);
    CHECK(strcmp(again, print) && (other != live_total[2] || memcmp(d, live[2], sizeof(d))),
          "one bit of the key changes everything");
    qd_fair_end(NULL);
}

int main(void)
{
    test_sha256();
    test_words();
    test_goldens();
    test_session();
    printf("fair mode: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
