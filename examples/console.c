/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    console.c
 * @brief   qd4p_console: try QD4P from a serial terminal.
 *
 * Flash qd4p_console.uf2, open the Pico's USB serial port (VS Code's Serial
 * Monitor, or `screen /dev/ttyACM0 115200`), press Enter, type `help`.
 *
 * Each milestone adds its commands to the table at the bottom of this file;
 * the line editing and command lookup above it stay the same.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"

#include "qd4p.h"
#include "qd_entropy_pico.h"

/* ------------------------------------------------------------------------ */
/* Commands                                                                 */
/* ------------------------------------------------------------------------ */
typedef struct {
    const char *name;
    const char *help;
    void (*run)(char *args);
} command_t;

static void cmd_help(char *args);

static void cmd_version(char *args)
{
    (void)args;
    printf("QD4P %s\n", qd_version());
}

static void cmd_limits(char *args)
{
    (void)args;
    printf("dice kept individually  %d (past this: summary mode)\n", QD_MAX_KEPT_DICE);
    printf("dice per roll           %d\n", QD_MAX_ROLL_DICE);
    printf("explosions per die      %d\n", QD_MAX_EXPLOSIONS);
    printf("buckets per roll        %d\n", QD_MAX_BUCKETS);
    printf("faces per custom die    %d\n", QD_MAX_FACES);
    printf("cards per deck          %d\n", QD_MAX_CARDS);
}

/* --- D1: randomness ------------------------------------------------------ */

/* Next whole number from an argument string, or `fallback` if there isn't one. */
static long arg_num(char **args, long fallback)
{
    char *end;
    long v = strtol(*args, &end, 0);
    if (end == *args) return fallback;
    while (*end == ' ') end++;
    *args = end;
    return v;
}

static void cmd_rng(char *args)
{
    (void)args;
    printf("mode: %s, entropy stirred in %lu times since the last seed\n",
           qd_rng_is_replay() ? "REPLAY (same numbers every time)" : "live",
           (unsigned long)qd_rng_stir_count());
}

static void cmd_seed(char *args)
{
    long n = arg_num(&args, -1);
    if (n < 0) { printf("usage: seed <number>   (replay mode; 'live' to leave)\n"); return; }
    qd_rng_seed_fixed((uint64_t)n);
    printf("replay mode, seed %ld\n", n);
}

static void cmd_live(char *args)
{
    (void)args;
    qd_rng_init_hw();
    printf("live: seeded from the hardware TRNG\n");
}

static void cmd_next(char *args)
{
    long n = arg_num(&args, 8);
    for (long i = 0; i < n; i++)
        printf("%lu%s", (unsigned long)qd_rng_next32(), (i % 4 == 3 || i == n - 1) ? "\n" : "  ");
}

/* Chi-square limit at p = 0.001 (Wilson-Hilferty approximation). */
static double chi_limit(int df)
{
    double k = 2.0 / (9.0 * df);
    double t = 1.0 - k + 3.090 * sqrt(k);
    return df * t * t * t;
}

static void cmd_test(char *args)
{
    long sides = arg_num(&args, 6), rolls = arg_num(&args, 600000);
    if (sides < 2 || sides > 100 || rolls < sides * 10) {
        printf("usage: test <sides 2-100> <rolls>   e.g. test 6 600000\n"); return;
    }
    static uint32_t counts[100];
    memset(counts, 0, sizeof(counts));
    uint32_t t0 = time_us_32();
    for (long i = 0; i < rolls; i++)
        counts[qd_rng_between(1, (int32_t)sides) - 1]++;
    uint32_t us = time_us_32() - t0;
    double expect = (double)rolls / sides, chi = 0;
    for (long f = 0; f < sides; f++) {
        double d = counts[f] - expect;
        chi += d * d / expect;
        if (sides <= 20) printf("  %2ld: %7lu  %+.3f%%\n", f + 1, (unsigned long)counts[f],
                                100.0 * (counts[f] - expect) / expect);
    }
    double limit = chi_limit((int)sides - 1);
    printf("chi-square %.1f, limit %.1f: %s\n", chi, limit,
           chi < limit ? "PASS (looks fair)" : "FAIL (try again; 1 in 1000 fair runs fail)");
    printf("%ld rolls in %lu ms (%.0f per second)\n", rolls, (unsigned long)(us / 1000),
           rolls * 1e6 / us);
}

static void cmd_stir(char *args)
{
    if (!*args) { printf("usage: stir <any text>\n"); return; }
    printf(qd_rng_stir(args, strlen(args)) ? "stirred in\n" : "ignored: replay mode is on\n");
}

static void cmd_trng(char *args)
{
    long n = arg_num(&args, 16);
    printf("%lu TRNG readings stirred in\n",
           (unsigned long)qd_rng_stir_source(&qd_entropy_trng, (uint32_t)n));
}

static void cmd_adc(char *args)
{
    long gpio = arg_num(&args, -1), n = arg_num(&args, 64);
    if (gpio < 26 || gpio > 29) {
        printf("usage: adc <gpio 26-29> [samples]   (something must be wired to the pin)\n");
        return;
    }
    static qd_entropy_adc_t adc;
    printf("%lu ADC readings from GPIO %ld stirred in\n",
           (unsigned long)qd_rng_stir_source(qd_entropy_adc(&adc, (unsigned)gpio), (uint32_t)n),
           gpio);
}

static void cmd_tap(char *args)
{
    long seconds = arg_num(&args, 5);
    printf("tap any keys for %ld seconds (each key's timing is stirred in)...\n", seconds);
    uint32_t taps = 0, end = time_us_32() + (uint32_t)seconds * 1000000u;
    while ((int32_t)(end - time_us_32()) > 0)
        if (getchar_timeout_us(1000) != PICO_ERROR_TIMEOUT) {
            qd_entropy_event();
            taps++;
        }
    printf("%lu taps stirred in%s\n", (unsigned long)taps,
           qd_rng_is_replay() ? " (ignored: replay mode is on)" : "");
}

/* --- D2: dice and decks --------------------------------------------------- */

static void cmd_pct(char *args)
{
    int bonus = 0;
    while (*args == 'b' || *args == 'p' || *args == 'B' || *args == 'P') {
        char kind = *args++;
        long n = arg_num(&args, 1);
        bonus += (kind == 'b' || kind == 'B') ? (int)n : -(int)n;
    }
    long count = arg_num(&args, 1);
    for (long i = 0; i < count; i++) {
        qd_percentile_t p;
        qd_roll_percentile(bonus, &p);
        printf("units %d, tens", p.units);
        for (int k = 0; k < p.tens_count; k++)
            printf(k == p.kept && p.tens_count > 1 ? " [%d0]" : " %d0", p.tens[k]);
        printf("  -> %d%s\n", p.result,
               p.bonus > 0 ? " (bonus: best kept)" : p.bonus < 0 ? " (penalty: worst kept)" : "");
    }
}

/* --- D3: the roll engine --------------------------------------------------
 *
 * A stand-in reader so rolls can be typed before the parser (D4) exists.
 * Words are separated by spaces and apply to the dice before them:
 *
 *   4d6 k3    10d6 e6 k8 + 4    4d10 t8 ie10 f1    1d20 adv1 + 5
 *   dd66      4dF =+            100d6 <3 =6        1d20 - 1d4 * 2
 *
 * The parser will read the same words (and more) without needing spaces.
 */
static qd_roll_t con_roll;
static qd_die_t con_fudge;

/* Read a whole number at *s; returns false if there isn't one. */
static bool read_int(const char **s, long *out)
{
    char *end;
    long v = strtol(*s, &end, 10);
    if (end == *s) return false;
    *out = v;
    *s = end;
    return true;
}

/* The pool just added was added to the total; subtract it instead. */
static void make_minus(void)
{
    qd_op_t *last = &con_roll.ops[con_roll.op_count - 1];
    if (last->kind == QD_OP_ADD) last->kind = QD_OP_SUB;
    else qd_roll_push(&con_roll, QD_OP_NEG, 0);     /* it was the first thing */
}

/* A dice word: 4d6, d20, d%, 4dF, dd66, 3dd666. Adds a pool. */
static bool dice_word(const char *w, bool minus)
{
    const char *s = w;
    long count = 1;
    if (*s >= '0' && *s <= '9') read_int(&s, &count);
    if (*s != 'd' || count < 1 || count > 65535) return false;
    s++;
    qd_pool_t *p;
    if (*s == 'd') {
        p = qd_roll_digits(&con_roll, (uint16_t)count, s + 1);
        if (p && minus) make_minus();
        return p != NULL;
    }
    if ((*s == 'F' || *s == 'f') && !s[1]) {
        p = qd_roll_custom(&con_roll, (uint16_t)count, &con_fudge);
        if (p && minus) make_minus();
        return p != NULL;
    }
    long sides;
    if (*s == '%' && !s[1]) sides = 100;
    else if (!read_int(&s, &sides) || *s || sides < 1 || sides > 32767) return false;
    p = minus ? qd_roll_dice_minus(&con_roll, (uint16_t)count, (uint16_t)sides)
              : qd_roll_dice(&con_roll, (uint16_t)count, (uint16_t)sides);
    return p != NULL;
}

/* A test after a word like e, t or a bucket: a number, or the default. */
static bool number_test(const char *s, qd_test_t dflt, bool le, qd_test_t *t)
{
    long n;
    if (!*s) { *t = dflt; return true; }
    if (!read_int(&s, &n) || *s) return false;
    *t = le ? qd_test_le((int)n) : qd_test_ge((int)n);
    return true;
}

/* A bucket: <3  <=3  >4  >=4  =6  !=1  =+ (a label) */
static bool bucket_word(const char *w)
{
    static const struct { const char *sym; qd_test_t (*make)(int); } ops[] = {
        { "<=", qd_test_le }, { ">=", qd_test_ge }, { "!=", qd_test_ne },
        { "<", qd_test_lt }, { ">", qd_test_gt }, { "=", qd_test_eq },
    };
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        size_t n = strlen(ops[i].sym);
        if (strncmp(w, ops[i].sym, n)) continue;
        const char *s = w + n;
        long v;
        if (read_int(&s, &v) && !*s) return qd_roll_bucket(&con_roll, ops[i].make((int)v));
        if (!strcmp(ops[i].sym, "=") && *s && strlen(s) <= QD_MAX_LABEL)
            return qd_roll_bucket(&con_roll, qd_test_label(s));
        return false;
    }
    return false;
}

/* A modifier on the last pool: adv dis e ie i r ir rg irg k kl km d dh t tl ds f b */
static bool modifier_word(qd_pool_t *p, const char *w)
{
    qd_test_t t;
    long n = 1;
    const char *s;
#define WORD(prefix) (!strncmp(w, prefix, strlen(prefix)) && (s = w + strlen(prefix)))
    if (WORD("adv") || WORD("dis")) {
        if (*s && (!read_int(&s, &n) || *s)) return false;
        qd_pool_advantage(p, w[0] == 'a' ? (int)n : -(int)n);
        return true;
    }
    if (WORD("irg")) return number_test(s, qd_test_max(), false, &t) && qd_pool_reroll(p, t, true);
    if (WORD("ir"))  return number_test(s, qd_test_min(), true, &t) && qd_pool_reroll(p, t, true);
    if (WORD("ie"))  return number_test(s, qd_test_max(), false, &t) && qd_pool_explode(p, t, true);
    if (WORD("i"))   return number_test(s, qd_test_min(), true, &t) && qd_pool_implode(p, t);
    if (WORD("e"))   return number_test(s, qd_test_max(), false, &t) && qd_pool_explode(p, t, false);
    if (WORD("rg"))  return number_test(s, qd_test_max(), false, &t) && qd_pool_reroll(p, t, false);
    if (WORD("r"))   return number_test(s, qd_test_min(), true, &t) && qd_pool_reroll(p, t, false);
    if (WORD("tl"))  { if (!number_test(s, qd_test_min(), true, &t)) return false; qd_pool_success(p, t); return true; }
    if (WORD("t"))   { if (!number_test(s, qd_test_max(), false, &t)) return false; qd_pool_success(p, t); return true; }
    if (WORD("ds"))  {
        bool le = p->success.op == QD_TEST_LE;
        if (!number_test(s, p->success, le, &t)) return false;
        qd_pool_twice(p, t);
        return true;
    }
    if (WORD("f"))   { if (!number_test(s, qd_test_min(), true, &t)) return false; qd_pool_failure(p, t); return true; }
    if (WORD("b"))   { if (!number_test(s, qd_test_min(), true, &t)) return false; qd_pool_botch(p, t); return true; }
    static const struct { const char *w; qd_keep_t how; } keeps[] = {
        { "kl", QD_KEEP_LOW }, { "km", QD_KEEP_MIDDLE }, { "k", QD_KEEP_HIGH },
        { "dh", QD_DROP_HIGH }, { "d", QD_DROP_LOW },
    };
    for (size_t i = 0; i < sizeof(keeps) / sizeof(keeps[0]); i++)
        if (WORD(keeps[i].w)) {
            if (*s && (!read_int(&s, &n) || *s || n < 0)) return false;
            qd_pool_keep(p, keeps[i].how, (uint16_t)n);
            return true;
        }
#undef WORD
    return false;
}

/* Read a whole line into con_roll. Returns false and names the word it
 * couldn't read. */
static bool read_roll(char *line)
{
    qd_roll_init(&con_roll);
    char pending = 0;                       /* + - * / waiting for its number or dice */
    for (char *w = strtok(line, " ,[]"); w; w = strtok(NULL, " ,[]")) {
        /* "+5", "-1d4", "*2": split the operator from what follows */
        if (strchr("+-*/", *w) && !pending) {
            pending = *w++;
            if (!*w) continue;
        }
        if (pending) {
            const char *s = w;
            long n;
            if (read_int(&s, &n) && !*s) {
                if (!con_roll.op_count) { printf("start with dice: '%c%s'\n", pending, w); return false; }
                if (pending == '+') qd_roll_add(&con_roll, (int32_t)n);
                else if (pending == '-') qd_roll_add(&con_roll, -(int32_t)n);
                else if (pending == '*') qd_roll_mul(&con_roll, (int32_t)n);
                else qd_roll_div(&con_roll, (int32_t)n);
            } else if ((pending == '+' || pending == '-') && dice_word(w, pending == '-')) {
            } else {
                printf("can't read '%c%s'\n", pending, w);
                return false;
            }
            pending = 0;
            continue;
        }
        if (strchr("<>=!", *w)) {
            if (!bucket_word(w)) { printf("can't read the bucket '%s'\n", w); return false; }
            continue;
        }
        if (!con_roll.pool_count) {
            if (!dice_word(w, false)) { printf("can't read the dice '%s'\n", w); return false; }
            continue;
        }
        if (!modifier_word(&con_roll.pools[con_roll.pool_count - 1], w)) {
            printf("can't read '%s' (add more dice with +, e.g. + 1d4)\n", w);
            return false;
        }
    }
    if (pending) { printf("'%c' needs a number or dice after it\n", pending); return false; }
    return true;
}

/* One die as text: 5  (5) dropped  ~2~ rerolled  !6 from an explosion
 * -7 imploded (subtracted)  8* success  10** double  1x failure  b botch */
static void print_die(const qd_pool_t *p, const qd_die_result_t *d)
{
    char v[16];
    const char *label = p->die ? qd_die_face_label(p->die, d->face) : "";
    if (*label) snprintf(v, sizeof(v), "[%s]", label);
    else snprintf(v, sizeof(v), "%d", d->value);
    uint16_t f = d->flags;
    printf(" %s%s%s%s%s%s%s%s%s", (f & QD_DIE_EXPLODED) ? "!" : "", (f & QD_DIE_IMPLODED) ? "-" : "",
           (f & QD_DIE_REROLLED) ? "~" : (f & (QD_DIE_DROPPED | QD_DIE_SET_ASIDE)) ? "(" : "", v,
           (f & QD_DIE_REROLLED) ? "~" : (f & (QD_DIE_DROPPED | QD_DIE_SET_ASIDE)) ? ")" : "",
           (f & QD_DIE_TWICE) ? "**" : (f & QD_DIE_SUCCESS) ? "*" : "",
           (f & QD_DIE_FAILURE) ? "x" : "", (f & QD_DIE_BOTCH) ? "b" : "",
           (f & QD_DIE_CAPPED) ? "(cap)" : "");
}

/* Print a die, then the dice it led to, so chains read left to right. */
static void print_tree(const qd_pool_t *p, const qd_die_result_t *dice, int n, int i, int depth)
{
    print_die(p, &dice[i]);
    if (depth > 120) return;
    for (int j = i + 1; j < n; j++)
        if (dice[j].parent == i) print_tree(p, dice, n, j, depth + 1);
}

static void print_result(const qd_roll_t *r, const qd_result_t *res)
{
    for (int k = 0; k < res->pool_count; k++) {
        const qd_pool_t *p = &r->pools[k];
        const qd_pool_result_t *pr = &res->pools[k];
        if (p->digits) {
            printf("%udd", p->count);
            for (int d = 0; d < p->digits; d++) printf("%u", p->digit_sides[d]);
        } else if (p->die) {
            printf("%u%s", p->count, p->die == &con_fudge ? "dF" : "d?");
        } else {
            printf("%ud%u", p->count, p->sides);
        }
        printf(":");
        const qd_die_result_t *dice = &res->dice[pr->first];
        for (int i = 0; i < pr->listed; i++)
            if (dice[i].parent == QD_NO_PARENT) print_tree(p, dice, pr->listed, i, 0);
        if (pr->listed < pr->rolled) printf(" ... %u more", pr->rolled - pr->listed);
        printf("  = %ld", (long)pr->total);
        if (pr->rolled > 20 && pr->kept)
            printf("  (%u kept, low %d, high %d, average %.2f)", pr->kept, pr->min, pr->max,
                   (double)pr->total / pr->kept);
        printf("\n");
        if (pr->listed < pr->rolled && pr->face_counts) {
            printf("  faces:");
            for (int f = 0; f < (p->die ? p->die->sides : p->sides); f++)
                if (pr->face_count[f]) printf(" %d:%u", f + 1, pr->face_count[f]);
            printf("\n");
        }
    }
    printf("total %ld", (long)res->total);
    if (res->flags & QD_RES_COUNTING) printf(", successes %d", res->successes);
    if (res->failures) printf(" (%d failure%s taken off)", res->failures, res->failures == 1 ? "" : "s");
    if (res->botches) printf(", botches %d", res->botches);
    printf("\n");
    if (res->bucket_count) {
        printf("buckets:");
        for (int b = 0; b < res->bucket_count; b++) {
            char t[16];
            printf(" %s:%u", qd_test_text(&r->buckets[b].test, t, sizeof(t)), res->buckets[b]);
        }
        printf(" other:%u\n", res->buckets[res->bucket_count]);
    }
    if (res->flags & QD_RES_SUMMARY)  printf("(summary: over %d dice, only the first %d listed)\n",
                                             QD_MAX_KEPT_DICE, QD_MAX_KEPT_DICE);
    if (res->flags & QD_RES_CAPPED)   printf("(an explosion or reroll chain hit the limit)\n");
    if (res->flags & QD_RES_DIV_ZERO) printf("(divided by zero: that step gave 0)\n");
    if (res->flags & QD_RES_CLAMPED)  printf("(total too big: clamped)\n");
}

static void cmd_roll(char *args)
{
    if (!*args) {
        printf("usage: roll 4d6 k3 | 10d6 e6 k8 + 4 | 4d10 t8 ie10 f1 | dd66 | 4dF =+\n"
               "  (or type the roll on its own; see 'notation')\n");
        return;
    }
    if (!read_roll(args)) return;
    static qd_result_t res;
    uint32_t t0 = time_us_32();
    qd_err_t e = qd_roll_run(&con_roll, &res);
    uint32_t us = time_us_32() - t0;
    if (e) { printf("can't roll that: %s\n", qd_err_text(e)); return; }
    if (res.warnings) printf("note: %s\n", qd_warn_text(res.warnings));
    print_result(&con_roll, &res);
    if (res.rolled > 100) printf("(%u dice in %lu us)\n", res.rolled, (unsigned long)us);
}

static void cmd_notation(char *args)
{
    (void)args;
    printf(
        "dice      4d6  d20  d%%  4dF  dd66 (tens d6, units d6)  dd666  dd61\n"
        "maths     + 5   - 1d4   * 2   / 2      (left to right for now)\n"
        "advantage adv1  dis1        extra die, set aside lowest/highest first\n"
        "explode   e (on max)  e5 (5+)  ie (again and again)  ie5\n"
        "implode   i (on min)  i2 (2 or less): roll one more and subtract it\n"
        "reroll    r (min)  r2 (2 or less)  ir2 (until not)  rg5  irg5 (5+)\n"
        "keep      k3 (highest)  kl3 (lowest)  km3 (middle)  d1 (drop lowest)  dh1\n"
        "count     t8 (8+ succeeds)  tl3 (3 or less)  ds10 (10s count twice)\n"
        "          f1 (1s take a success off)  b1 (count botches)\n"
        "buckets   <3  <=3  >4  >=4  =6  !=1  =+ (label)\n"
        "shown as  5 kept  (5) dropped  ~2~ rerolled  !6 exploded  -7 subtracted\n"
        "          8* success  10** double  1x failure  b botch\n");
}

/* custom <count> <face> <face> ...   face = value[*weight][/label] */
static void cmd_custom(char *args)
{
    long count = arg_num(&args, 0);
    qd_face_t faces[QD_MAX_FACES];
    int n = 0;
    while (*args && n < QD_MAX_FACES) {
        char *end;
        memset(&faces[n], 0, sizeof(faces[n]));
        faces[n].value = (int16_t)strtol(args, &end, 10);
        if (end == args) break;
        faces[n].weight = 1;
        if (*end == '*') faces[n].weight = (uint8_t)strtol(end + 1, &end, 10);
        if (*end == '/') {
            int k = 0;
            for (end++; *end && *end != ' ' && k < QD_MAX_LABEL; end++) faces[n].label[k++] = *end;
            while (*end && *end != ' ') end++;
        }
        while (*end == ' ') end++;
        args = end;
        n++;
    }
    qd_die_t die;
    if (count < 1 || !qd_die_custom(&die, faces, (uint16_t)n)) {
        printf("usage: custom <rolls> <face> ...   face = value[*weight][/label]\n"
               "   e.g. custom 20 1 2 3 4 5 6*3          (a six three times as likely)\n"
               "        custom 100000 1 2 3 4 5 6*3      (big counts: a fairness table)\n");
        return;
    }
    if (count <= 60) {
        for (long i = 0; i < count; i++) {
            qd_face_roll_t r = qd_die_roll(&die);
            const char *l = qd_die_face_label(&die, r.face);
            if (*l) printf("%s ", l); else printf("%d ", r.value);
        }
        printf("\n");
        return;
    }
    static uint32_t counts[QD_MAX_FACES];
    memset(counts, 0, sizeof(counts));
    for (long i = 0; i < count; i++) counts[qd_die_roll(&die).face]++;
    double chi = 0;
    for (int f = 0; f < n; f++) {
        /* double first: on the Pico long is 32 bits, and rolls x ppm overflows */
        double expect = (double)count * qd_die_face_ppm(&die, (uint16_t)f) / 1e6;
        chi += (counts[f] - expect) * (counts[f] - expect) / expect;
        printf("  face %2d (%d%s%s): %6.2f%%  expected %6.2f%%\n", f + 1, faces[f].value,
               *faces[f].label ? "/" : "", faces[f].label,
               100.0 * counts[f] / count, qd_die_face_ppm(&die, (uint16_t)f) / 1e4);
    }
    double limit = chi_limit(n - 1);
    printf("chi-square %.1f, limit %.1f: %s\n", chi, limit, chi < limit ? "PASS" : "FAIL");
}

static qd_deck_t deck;
static bool deck_ready;

static void deck_status(void)
{
    printf("%d cards, %d in the deck, %d out; reshuffle:%s%s%s%s%s\n", deck.count,
           qd_deck_remaining(&deck), qd_deck_out_count(&deck),
           deck.policy & QD_RESHUFFLE_EVERY_DRAW ? " every-draw" : "",
           deck.policy & QD_RESHUFFLE_WHEN_EMPTY ? " when-empty" : "",
           deck.policy & QD_RESHUFFLE_AFTER_TRIGGER ? " after-joker" : "",
           deck.policy ? "" : " on demand only",
           deck.triggered ? "  [joker out: next deal reshuffles]" : "");
}

static void cmd_deck(char *args)
{
    if (!strncmp(args, "poker", 5)) {
        args += 5;
        while (*args == ' ') args++;
        long jokers = arg_num(&args, 0);
        if (!qd_deck_poker(&deck, (int)jokers)) { printf("0 to 4 jokers\n"); return; }
        deck_ready = true;
    } else if (!strncmp(args, "tarot", 5)) {
        qd_deck_tarot(&deck);
        deck_ready = true;
    } else if (!strncmp(args, "sw", 2)) {
        qd_deck_poker(&deck, 2);
        deck.policy = QD_RESHUFFLE_AFTER_TRIGGER | QD_RESHUFFLE_WHEN_EMPTY;
        deck_ready = true;
    } else if (*args) {
        printf("usage: deck poker [jokers] | deck tarot | deck sw (Savage Worlds) | deck\n");
        return;
    }
    if (!deck_ready) { printf("no deck yet: deck poker 2, deck tarot or deck sw\n"); return; }
    deck_status();
}

static void cmd_deal(char *args)
{
    if (!deck_ready) { printf("no deck yet: deck poker 2, deck tarot or deck sw\n"); return; }
    long n = arg_num(&args, 1);
    if (n < 1 || n > 255) { printf("usage: deal [cards]\n"); return; }
    static qd_card_draw_t hand[255];
    bool reshuffled = (deck.policy & QD_RESHUFFLE_AFTER_TRIGGER) && deck.triggered;
    int got = qd_deck_deal(&deck, hand, (int)n);
    if (reshuffled) printf("(reshuffled: a joker came up last deal)\n");
    for (int i = 0; i < got; i++) {
        char name[32];
        printf("  %-4s %s\n", hand[i].card->label, qd_card_name(hand[i].card, name, sizeof(name)));
    }
    if (got < n) printf("deck empty after %d cards (shuffle, or policy empty)\n", got);
    deck_status();
}

static void cmd_shuffle(char *args)
{
    (void)args;
    if (!deck_ready) { printf("no deck yet\n"); return; }
    qd_deck_shuffle(&deck);
    deck_status();
}

static void cmd_policy(char *args)
{
    if (!deck_ready) { printf("no deck yet\n"); return; }
    uint8_t p = 0;
    if (strstr(args, "every")) p |= QD_RESHUFFLE_EVERY_DRAW;
    if (strstr(args, "empty")) p |= QD_RESHUFFLE_WHEN_EMPTY;
    if (strstr(args, "joker")) p |= QD_RESHUFFLE_AFTER_TRIGGER;
    if (!*args) { printf("usage: policy [every] [empty] [joker] | policy none\n"); return; }
    deck.policy = p;
    deck_status();
}

static const command_t commands[] = {
    { "help",    "this list",                                   cmd_help    },
    { "version", "which QD4P this is",                          cmd_version },
    { "limits",  "the compile-time limits in use",              cmd_limits  },
    { "rng",     "generator mode and how often it was stirred", cmd_rng     },
    { "seed",    "seed <n>: replay mode, same numbers every time", cmd_seed },
    { "live",    "leave replay: reseed from the hardware",      cmd_live    },
    { "next",    "next [n]: raw 32-bit numbers",                cmd_next    },
    { "test",    "test <sides> <rolls>: fairness check",        cmd_test    },
    { "stir",    "stir <text>: mix your own input in",          cmd_stir    },
    { "trng",    "trng [n]: stir in n hardware TRNG readings",  cmd_trng    },
    { "adc",     "adc <gpio> [n]: stir in ADC noise",           cmd_adc     },
    { "tap",     "tap [seconds]: stir in your key timings",     cmd_tap     },
    { "roll",    "roll 4d6 k3 (or just type 4d6 k3)",           cmd_roll    },
    { "notation","what you can type in a roll",                 cmd_notation },
    { "pct",     "pct [b2|p1] [n]: d100, CoC bonus/penalty dice", cmd_pct   },
    { "custom",  "custom <rolls> 1 2 6*3 0/+ ...: your own die", cmd_custom  },
    { "deck",    "deck poker [jokers] | tarot | sw: new deck",  cmd_deck    },
    { "deal",    "deal [n]: one deal of n cards",               cmd_deal    },
    { "shuffle", "put every card back",                         cmd_shuffle },
    { "policy",  "policy every/empty/joker/none: reshuffle rules", cmd_policy },
};
#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static void cmd_help(char *args)
{
    (void)args;
    for (size_t i = 0; i < COMMAND_COUNT; i++)
        printf("  %-8s %s\n", commands[i].name, commands[i].help);
}

/* ------------------------------------------------------------------------ */
/* Line input and dispatch                                                  */
/* ------------------------------------------------------------------------ */
static void run_line(char *line)
{
    while (*line == ' ') line++;
    if (!*line) return;
    char *args = strchr(line, ' ');
    if (args) {
        *args++ = '\0';
        while (*args == ' ') args++;
    } else {
        args = line + strlen(line);
    }
    for (size_t i = 0; i < COMMAND_COUNT; i++)
        if (!strcmp(line, commands[i].name)) {
            commands[i].run(args);
            return;
        }
    /* A roll typed on its own: "4d6 k3", "d20", "dd66", "4dF". */
    if ((*line >= '0' && *line <= '9') || *line == 'd' || *line == '-') {
        if (*args) line[strlen(line)] = ' ';    /* put the line back together */
        cmd_roll(line);
        return;
    }
    printf("unknown command '%s' - try help\n", line);
}

int main(void)
{
    stdio_init_all();
    for (int i = 0; i < 30 && !stdio_usb_connected(); i++)   /* up to 3 s for the PC */
        sleep_ms(100);
    qd_rng_init_hw();                                         /* hardware seed at boot */
    qd_die_fudge(&con_fudge);
    printf("\nQD4P %s console - type help\n> ", qd_version());

    char line[128];
    size_t n = 0;
    for (;;) {
        int c = getchar_timeout_us(10000);
        if (c == PICO_ERROR_TIMEOUT) continue;
        if (c == '\r' || c == '\n') {
            printf("\n");
            line[n] = '\0';
            run_line(line);
            n = 0;
            printf("> ");
        } else if ((c == 8 || c == 127) && n > 0) {
            n--;
            printf("\b \b");
        } else if (c >= 0x20 && c < 0x7F && n < sizeof(line) - 1) {
            line[n++] = (char)c;
            putchar(c);
        }
    }
}
