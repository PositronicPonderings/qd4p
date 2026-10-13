/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_fair.c
 * @brief   Fair mode: a secret key, its fingerprint, numbered actions.
 *
 * Portable C: SHA-256 is done in software (about 20 us a block on the
 * Pico 2, and fair mode needs one or two per action).
 */
#include "qd_fair.h"

#include <ctype.h>
#include <string.h>

#include "qd_rng.h"

/* ========================================================================
 * SHA-256 (FIPS 180-4), written plainly: 64-byte blocks, 64 rounds each.
 * ======================================================================== */
typedef struct {
    uint32_t h[8];
    uint8_t  block[64];
    uint32_t used;                          /* bytes waiting in block[] */
    uint64_t total;                         /* bytes so far */
} sha_t;

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static inline uint32_t ror(uint32_t x, int k) { return (x >> k) | (x << (32 - k)); }

static void sha_block(sha_t *c, const uint8_t *p)
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 |
               (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3];
    uint32_t e = c->h[4], f = c->h[5], g = c->h[6], h = c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & b) ^ (a & cc) ^ (b & cc));
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

static void sha_init(sha_t *c)
{
    static const uint32_t start[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    memcpy(c->h, start, sizeof(start));
    c->used = 0;
    c->total = 0;
}

static void sha_add(sha_t *c, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    c->total += len;
    while (len) {
        uint32_t take = 64 - c->used;
        if (take > len) take = (uint32_t)len;
        memcpy(c->block + c->used, p, take);
        c->used += take; p += take; len -= take;
        if (c->used == 64) {
            sha_block(c, c->block);
            c->used = 0;
        }
    }
}

static void sha_done(sha_t *c, uint8_t out[32])
{
    uint64_t bits = c->total * 8;
    uint8_t pad = 0x80;
    sha_add(c, &pad, 1);
    pad = 0;
    while (c->used != 56)
        sha_add(c, &pad, 1);
    uint8_t len[8];
    for (int i = 0; i < 8; i++) len[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha_add(c, len, 8);
    for (int i = 0; i < 8; i++)
        for (int k = 0; k < 4; k++)
            out[4 * i + k] = (uint8_t)(c->h[i] >> (24 - 8 * k));
}

void qd_sha256(const void *data, size_t len, uint8_t out[32])
{
    sha_t c;
    sha_init(&c);
    sha_add(&c, data, len);
    sha_done(&c, out);
}

/* ========================================================================
 * The words: 256 short, familiar words, one per byte value. No two share
 * their first three letters, so three letters are always enough to type.
 * Changing this list changes every fingerprint: don't.
 * ======================================================================== */
static const char *const words[256] = {
    /*   0 */ "acorn", "actor", "agent", "alarm", "album", "alley", "amber", "anchor",
    /*   8 */ "angle", "ankle", "apple", "apron", "arch", "arrow", "aspen", "atlas",
    /*  16 */ "attic", "autumn", "award", "axis", "badge", "bagel", "baker", "banjo",
    /*  24 */ "barn", "basket", "beach", "bell", "bench", "berry", "bike", "bird",
    /*  32 */ "bloom", "blue", "boat", "book", "bottle", "bowl", "bread", "brick",
    /*  40 */ "brush", "bubble", "bugle", "bulb", "butter", "cabin", "cactus", "camel",
    /*  48 */ "canyon", "card", "castle", "cave", "cedar", "chair", "cherry", "cider",
    /*  56 */ "clay", "cliff", "cloud", "coal", "coffee", "coin", "comet", "cookie",
    /*  64 */ "copper", "coral", "crab", "cream", "crown", "cube", "cycle", "daisy",
    /*  72 */ "dart", "dawn", "deer", "delta", "dish", "door", "dove", "dragon",
    /*  80 */ "dream", "drum", "duck", "dune", "dust", "eagle", "earth", "easel",
    /*  88 */ "echo", "edge", "elbow", "ember", "falcon", "farm", "fence", "fern",
    /*  96 */ "field", "finch", "fire", "fish", "flag", "flute", "foam", "forest",
    /* 104 */ "frog", "fruit", "fudge", "garden", "gate", "gecko", "giant", "globe",
    /* 112 */ "goat", "gold", "goose", "grape", "green", "grill", "grove", "guitar",
    /* 120 */ "gull", "hammer", "hawk", "hazel", "heart", "hedge", "herb", "hill",
    /* 128 */ "hippo", "hive", "honey", "hook", "horse", "hotel", "house", "igloo",
    /* 136 */ "iris", "iron", "island", "jazz", "jelly", "jewel", "juice", "jungle",
    /* 144 */ "kayak", "kitten", "kiwi", "knight", "knot", "koala", "lake", "lamp",
    /* 152 */ "lava", "lawn", "leaf", "lemon", "lilac", "lime", "lion", "llama",
    /* 160 */ "lodge", "lotus", "lunch", "magnet", "maple", "mask", "melon", "metal",
    /* 168 */ "mint", "mirror", "monkey", "moon", "moss", "moth", "nest", "night",
    /* 176 */ "north", "novel", "oasis", "ocean", "olive", "onion", "opal", "orange",
    /* 184 */ "orbit", "otter", "panda", "paper", "parrot", "pasta", "peach", "pencil",
    /* 192 */ "piano", "pine", "pizza", "planet", "plum", "poem", "pony", "quail",
    /* 200 */ "queen", "rabbit", "rain", "raven", "rice", "river", "robin", "rocket",
    /* 208 */ "rose", "ruby", "ruler", "sail", "sand", "scarf", "scout", "seal",
    /* 216 */ "seed", "shell", "ship", "shoe", "silver", "sled", "snail", "snow",
    /* 224 */ "soap", "sock", "sofa", "spark", "spoon", "star", "stone", "straw",
    /* 232 */ "sugar", "swan", "table", "tent", "tiger", "toast", "topaz", "torch",
    /* 240 */ "tower", "tree", "tulip", "twig", "vase", "wagon", "wand", "water",
    /* 248 */ "wave", "wheat", "wizard", "wolf", "wood", "yacht", "yarn", "zebra",
};

const char *qd_fair_word(uint8_t b) { return words[b]; }

int qd_fair_words(const uint8_t *bytes, int n, char *buf, int size)
{
    int len = 0;
    if (size > 0) buf[0] = '\0';
    for (int i = 0; i < n; i++) {
        const char *w = words[bytes[i]];
        int need = (int)strlen(w) + (i > 0);
        if (len + need >= size) break;
        if (i > 0) buf[len++] = ' ';
        strcpy(buf + len, w);
        len += (int)strlen(w);
    }
    return len;
}

int qd_fair_read_words(const char *text, uint8_t *out, int max)
{
    int n = 0;
    const char *p = text;
    for (;;) {
        while (*p == ' ' || *p == ',' || *p == '-' || *p == '\t') p++;
        if (!*p || n >= max) return n;
        char w[8];
        int len = 0;
        while (isalpha((unsigned char)*p)) {
            if (len < 7) w[len] = (char)tolower((unsigned char)*p);
            len++; p++;
        }
        if (len < 3 || len > 6 || (*p && *p != ' ' && *p != ',' && *p != '-' && *p != '\t'))
            return -(n + 1);
        int found = -1;
        for (int b = 0; b < 256 && found < 0; b++)
            if (!strncmp(words[b], w, (size_t)len)) found = b;
        if (found < 0) return -(n + 1);
        out[n++] = (uint8_t)found;
    }
}

/* ========================================================================
 * The session
 * ======================================================================== */
static const char PRINT_TAG[] = "QD4P fair 1 print";
static const char ROLL_TAG[]  = "QD4P fair 1 roll";

static uint8_t  key[QD_FAIR_KEY_BYTES];
static bool     on;
static bool     checking;                   /* started from a known key */
static uint32_t count;                      /* the last action begun */

void qd_fair_print_of(const uint8_t k[QD_FAIR_KEY_BYTES], uint8_t out[QD_FAIR_PRINT_BYTES])
{
    sha_t c;
    uint8_t h[32];
    sha_init(&c);
    sha_add(&c, PRINT_TAG, sizeof(PRINT_TAG) - 1);
    sha_add(&c, k, QD_FAIR_KEY_BYTES);
    sha_done(&c, h);
    memcpy(out, h, QD_FAIR_PRINT_BYTES);
}

/* Hold the generator at action n's state. */
static void set_action(uint32_t n)
{
    sha_t c;
    uint8_t h[32], num[4] = { (uint8_t)n, (uint8_t)(n >> 8), (uint8_t)(n >> 16), (uint8_t)(n >> 24) };
    sha_init(&c);
    sha_add(&c, ROLL_TAG, sizeof(ROLL_TAG) - 1);
    sha_add(&c, key, QD_FAIR_KEY_BYTES);
    sha_add(&c, num, 4);
    sha_done(&c, h);
    uint32_t st[4];
    for (int i = 0; i < 4; i++)
        st[i] = (uint32_t)h[4 * i] | (uint32_t)h[4 * i + 1] << 8 |
                (uint32_t)h[4 * i + 2] << 16 | (uint32_t)h[4 * i + 3] << 24;
    qd_rng_hold(st);
    count = n;
}

bool qd_fair_start(void)
{
    if (on)
        return false;
    /* The key: fresh hardware randomness and the generator's own output
     * (which carries everything stirred in so far), hashed together. */
    uint32_t mix[8];
    qd_platform_seed(mix, 4);
    for (int i = 4; i < 8; i++) mix[i] = qd_rng_next32();
    uint8_t h[32];
    qd_sha256(mix, sizeof(mix), h);
    memcpy(key, h, QD_FAIR_KEY_BYTES);
    memset(mix, 0, sizeof(mix));
    memset(h, 0, sizeof(h));
    on = true;
    checking = false;
    set_action(0);                          /* held from now on; #1 is the first action */
    return true;
}

void qd_fair_start_key(const uint8_t k[QD_FAIR_KEY_BYTES])
{
    memcpy(key, k, QD_FAIR_KEY_BYTES);
    on = true;
    checking = true;
    set_action(0);
}

bool qd_fair_is_on(void) { return on; }

uint32_t qd_fair_begin(void)
{
    if (!on)
        return 0;
    set_action(count + 1);
    return count;
}

bool qd_fair_begin_at(uint32_t n)
{
    if (!on || !checking)
        return false;
    set_action(n);
    return true;
}

uint32_t qd_fair_count(void) { return on ? count : 0; }

bool qd_fair_fingerprint(uint8_t out[QD_FAIR_PRINT_BYTES])
{
    if (!on)
        return false;
    qd_fair_print_of(key, out);
    return true;
}

int qd_fair_fingerprint_words(char *buf, int size)
{
    uint8_t fp[QD_FAIR_PRINT_BYTES];
    if (!qd_fair_fingerprint(fp)) {
        if (size > 0) buf[0] = '\0';
        return 0;
    }
    return qd_fair_words(fp, QD_FAIR_PRINT_BYTES, buf, size);
}

bool qd_fair_end(uint8_t k[QD_FAIR_KEY_BYTES])
{
    if (!on)
        return false;
    if (k) memcpy(k, key, QD_FAIR_KEY_BYTES);
    memset(key, 0, sizeof(key));
    on = false;
    checking = false;
    count = 0;
    qd_rng_release();
    qd_rng_init_hw();                       /* the old state is public now: stir in fresh */
    return true;
}
