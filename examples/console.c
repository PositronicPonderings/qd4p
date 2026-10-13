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
 *
 * The same file builds on a PC (examples/pc/build.sh): there it is the
 * fair-mode checker, with every command working exactly as on the device.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"

#include "qd4p.h"
#include "qd_entropy_pico.h"
#include "tables/table.h"          /* the example table system (not part of QD4P) */
#include "tables/sample_tables.h"  /* built from sample_tables.txt by tables/tables.py */

/* Echo: on the device each key is echoed as it's typed. The PC build
 * (examples/pc) turns that off - a terminal echoes for itself - and echoes
 * whole lines only when they come from a file, so a log shows what ran. */
#ifndef ECHO_KEYS
#define ECHO_KEYS  1
#endif
#ifndef ECHO_LINES
#define ECHO_LINES 0
#endif

/* ------------------------------------------------------------------------ */
/* Output that fits 80 columns                                              */
/* ------------------------------------------------------------------------ */
/* Long lines (a hundred dice, a long label) are built up with w_add(), then
 * w_line() prints them broken at spaces so no line passes column 80. Lines
 * after the first start with `indent` spaces, so they read as one item. */
#include <stdarg.h>
#define WRAP_AT 80
static char wbuf[4096];
static int  wlen;

static void w_add(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(wbuf + wlen, sizeof(wbuf) - (size_t)wlen, fmt, ap);
    va_end(ap);
    if (n > 0) wlen += n;
    if (wlen > (int)sizeof(wbuf) - 1) wlen = (int)sizeof(wbuf) - 1;
}

static void w_line(int indent)
{
    int col = 0, i = 0;
    while (i < wlen) {
        /* the next piece: any spaces, then a run of non-spaces */
        int a = i;
        while (i < wlen && wbuf[i] == ' ') i++;
        int b = i;
        while (i < wlen && wbuf[i] != ' ') i++;
        int spaces = b - a, word = i - b;
        if (col > indent && col + spaces + word > WRAP_AT) {
            printf("\n%*s", indent, "");
            col = indent;
            spaces = 0;                     /* a new line starts with the word */
        }
        printf("%.*s%.*s", spaces, wbuf + a, word, wbuf + b);
        col += spaces + word;
    }
    printf("\n");
    wlen = 0;
}

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

/* --- D9: fair mode, the part every rolling command uses ---------------------
 *
 * Each command that rolls calls fair_mark() right before it rolls. In a fair
 * session that takes the next action number and prints it ("fair #17").
 * When checking a revealed key, a line may start with its number, "#17
 * 4d6k3", and fair_mark() uses that number instead.
 */
static uint32_t fair_at;               /* "#17 ..." when checking: 17, else 0 */

static void fair_mark(void)
{
    if (!qd_fair_is_on()) return;
    uint32_t n;
    if (fair_at) { qd_fair_begin_at(fair_at); n = fair_at; fair_at = 0; }
    else n = qd_fair_begin();
    printf("fair #%lu\n", (unsigned long)n);
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
    uint32_t stirred = qd_rng_stir_count();
    printf("mode: %s\n", qd_fair_is_on() ? "FAIR (rolls come from the session key)" :
           qd_rng_is_replay() ? "REPLAY (same numbers every time)" : "live");
    printf("entropy stirred in %lu time%s since the last seed\n", (unsigned long)stirred,
           stirred == 1 ? "" : "s");
}

static void cmd_seed(char *args)
{
    long n = arg_num(&args, -1);
    if (n < 0) { printf("usage: seed <number>   (replay mode; 'live' to leave)\n"); return; }
    if (qd_fair_is_on()) { printf("ignored: fair mode is on ('fair end' first)\n"); return; }
    qd_rng_seed_fixed((uint64_t)n);
    printf("replay mode, seed %ld\n", n);
}

static void cmd_live(char *args)
{
    (void)args;
    if (qd_fair_is_on()) { printf("ignored: fair mode is on ('fair end' first)\n"); return; }
    qd_rng_init_hw();
    printf("live: seeded from the hardware TRNG\n");
}

static void cmd_next(char *args)
{
    long n = arg_num(&args, 8);
    for (long i = 0; i < n; i++)
        printf("%lu%s", (unsigned long)qd_rng_next32(), (i % 4 == 3 || i == n - 1) ? "\n" : "  ");
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
    double limit = qd_chi_limit((int)sides - 1);
    printf("chi-square %.1f, limit %.1f: %s\n", chi, limit,
           chi < limit ? "PASS (looks fair)" : "FAIL (try again; 1 in 1000 fair runs fail)");
    printf("%ld rolls in %lu ms (%.0f per second)\n", rolls, (unsigned long)(us / 1000),
           rolls * 1e6 / us);
}

static void cmd_stir(char *args)
{
    if (!*args) { printf("usage: stir <any text>\n"); return; }
    printf(qd_rng_stir(args, strlen(args)) ? "stirred in\n" :
           qd_fair_is_on() ? "ignored: fair mode is on\n" : "ignored: replay mode is on\n");
}

static void cmd_trng(char *args)
{
    if (qd_fair_is_on()) { printf("ignored: fair mode is on\n"); return; }
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
    if (qd_fair_is_on()) { printf("ignored: fair mode is on\n"); return; }
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
           qd_fair_is_on() ? " (ignored: fair mode is on)" :
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
    fair_mark();
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

/* --- D3 and D4: the roll engine, through the parser ------------------------
 *
 * Type any roll on its own line: 4d6k3, (Bite) 1d20+5 ! note, 6 x 4d6k3,
 * 2d6; 3d8. 'notation' gives a summary; docs/manual/05-notation.md has everything.
 */
static qd_die_t con_custom;          /* the last die made with 'custom', as d{custom} */
static bool con_custom_ready;

static const qd_die_t *con_lookup(const char *name, void *ctx)
{
    (void)ctx;
    return (con_custom_ready && !strcmp(name, "custom")) ? &con_custom : NULL;
}

/* One die as text: 5  (5) dropped  ~2~ rerolled  !6 from an explosion
 * -7 imploded (subtracted)  8* success  10** double  1x failure  b botch */
static bool die_glue;                  /* print_die without its space ("#3:5") */

static void print_die(const qd_pool_t *p, const qd_die_result_t *d)
{
    char v[16];
    const char *label = p->die ? qd_die_face_label(p->die, d->face) : "";
    if (*label) snprintf(v, sizeof(v), "[%s]", label);
    else if (d->flags & QD_DIE_CHANGED) snprintf(v, sizeof(v), "%d->%d", d->was, d->value);
    else snprintf(v, sizeof(v), "%d", d->value);
    uint16_t f = d->flags;
    w_add("%s%s%s%s%s%s%s%s%s%s", die_glue ? "" : " ", (f & QD_DIE_EXPLODED) ? "!" : "", (f & QD_DIE_IMPLODED) ? "-" : "",
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

static void print_result(const qd_parsed_t *ps, const qd_result_t *res)
{
    const qd_roll_t *r = &ps->roll;
    for (int k = 0; k < res->pool_count; k++) {
        const qd_pool_t *p = &r->pools[k];
        const qd_pool_result_t *pr = &res->pools[k];
        char name[64];
        qd_format_pool(ps, k, name, sizeof(name));
        w_add("  %s:", name);
        const qd_die_result_t *dice = &res->dice[pr->first];
        for (int i = 0; i < pr->listed; i++)
            if (dice[i].parent == QD_NO_PARENT) print_tree(p, dice, pr->listed, i, 0);
        if (pr->listed < pr->rolled) w_add(" ... %u more", pr->rolled - pr->listed);
        w_add("  = %ld", (long)pr->total);
        w_line(4);
        if (pr->rolled > 20 && pr->kept) {
            w_add("    (%u kept, low %d, high %d, average %.2f)", pr->kept, pr->min, pr->max,
                  (double)pr->total / pr->kept);
            w_line(6);
        }
        if (p->sets) {
            w_add("    sets:");
            if (!pr->set_count) w_add(" none");
            for (int k2 = 0; k2 < pr->set_count; k2++)
                w_add(" %ux%d", pr->sets[k2].size, pr->sets[k2].value);
            w_line(6);
        }
        if (pr->listed < pr->rolled && pr->face_counts) {
            w_add("    faces:");
            for (int f = 0; f < (p->die ? p->die->sides : p->sides); f++)
                if (pr->face_count[f]) w_add(" %d:%u", f + 1, pr->face_count[f]);
            w_line(6);
        }
    }
    printf("  total %ld", (long)res->total);
    if (res->flags & QD_RES_COUNTING) printf(", successes %d", res->successes);
    if (res->failures) printf(" (%d failure%s taken off)", res->failures, res->failures == 1 ? "" : "s");
    if (res->botches) printf(", botches %d", res->botches);
    printf("\n");
    if (res->bucket_count) {
        w_add("  buckets:");
        for (int b = 0; b < res->bucket_count; b++) {
            char t[16];
            w_add(" %s:%u", qd_test_text(&r->buckets[b].test, t, sizeof(t)), res->buckets[b]);
        }
        w_add(" other:%u", res->buckets[res->bucket_count]);
        w_line(4);
    }
    if (res->flags & QD_RES_SUMMARY)  printf("  (summary: over %d dice, only the first %d listed)\n",
                                             QD_MAX_KEPT_DICE, QD_MAX_KEPT_DICE);
    if (res->flags & QD_RES_CAPPED)   printf("  (an explosion or reroll chain hit the limit)\n");
    if (res->flags & QD_RES_DIV_ZERO) printf("  (divided by zero: that step gave 0)\n");
    if (res->flags & QD_RES_CLAMPED)  printf("  (total too big: clamped)\n");
}

/* Show where a parse went wrong:
 *     4d6 k3 +
 *             ^ the roll stops too soon ... */
static void print_error(const char *text, const qd_parse_error_t *err)
{
    /* A line too long for 80 columns is shown as a window of it, "..." at
     * the cut ends, placed so the carets land under the problem. */
    int len = (int)strlen(text), from = 0, to = len, room = WRAP_AT - 8;
    if (len > room) {
        from = err->pos - room / 3;
        if (from < 0) from = 0;
        to = from + room;
        if (to > len) { to = len; from = to - room; }
    }
    int shift = from > 0 ? 3 : 0;                   /* the "..." in front */
    printf("  %s%.*s%s\n  ", from > 0 ? "..." : "", to - from, text + from, to < len ? "..." : "");
    for (int i = from - shift; i < err->pos; i++) putchar(' ');
    int marks = err->len;
    if (err->pos + marks > to) marks = to - err->pos;
    if (marks < 1) marks = 1;
    for (int i = 0; i < marks; i++) putchar('^');
    if (err->pos - from + shift + marks + 3 + (int)strlen(err->message) > WRAP_AT - 2)
        printf("\n  ");                            /* the message on its own line */
    printf(" %s\n", err->message);
}

/* The last single roll, for 'last' and 'reroll'. */
static qd_parsed_t last_roll;
static qd_result_t last_res;
static bool have_last;

static void print_outcome(const qd_parsed_t *ps, const qd_result_t *res)
{
    qd_outcome_t o;
    if (qd_sys_outcome(&ps->roll, res, &o) && (o.text[0] || o.detail[0])) {
        if (o.text[0] && o.detail[0]) printf("  => %s  (%s)\n", o.text, o.detail);
        else printf("  => %s\n", o.text[0] ? o.text : o.detail);
    }
}

/* Each roll of a repeat or set, as it is rolled. */
static void print_each(int index, const qd_result_t *res, void *ctx)
{
    const qd_parsed_t *ps = (const qd_parsed_t *)ctx;
    printf(" #%d\n", index + 1);
    print_result(ps, res);
    print_outcome(ps, res);
}

static void cmd_roll(char *args)
{
    if (!*args) {
        printf("usage: roll 4d6k3   (or type the roll on its own; 'notation' for help)\n");
        return;
    }
    static qd_parsed_t rolls[4];
    qd_parse_error_t err;
    int n = qd_parse_line(args, rolls, 4, &err);
    if (n < 0) { print_error(args, &err); return; }
    fair_mark();                            /* one number for the whole line */
    for (int k = 0; k < n; k++) {
        const qd_parsed_t *ps = &rolls[k];
        char tidy[160];
        qd_format(ps, tidy, sizeof(tidy));
        w_add("%s", tidy);
        w_line(2);
        uint16_t w = qd_roll_warnings(&ps->roll);
        if (w) printf("  note: %s\n", qd_warn_text(w));
        if (ps->repeat > QD_MAX_REPEAT) {
            /* a mass roll: counted, not listed */
            qd_mass_result_t m;
            uint32_t t0 = time_us_32();
            qd_mass_run(ps, &m);
            uint32_t us = time_us_32() - t0;
            printf("  %u rolls: %s %.2f on average, lowest %ld, highest %ld  (%lu ms)\n", m.count,
                   m.counting ? "successes" : "total", (double)m.sum / m.count, (long)m.lo, (long)m.hi,
                   (unsigned long)(us / 1000));
            if (m.judged) {
                printf("  passed %u (%.1f%%)\n", m.passes, 100.0 * m.passes / m.count);
                for (int l = 8; l >= 0; l--) {
                    const char *name = qd_sys_level_name(&ps->roll, l - 4);
                    if (m.level[l] && *name) printf("    %-20s %5u\n", name, m.level[l]);
                }
            }
            continue;
        }
        if (ps->repeat > 1 || ps->set) {
            /* a repeat or a set: each roll, then the set's line (wrapped at 80) */
            qd_set_result_t s;
            qd_set_run(ps, &s, print_each, (void *)ps);
            printf("  set:");
            int col = 7;
            for (int i = 0; i < s.count; i++) {
                char v[16];
                int n = snprintf(v, sizeof(v), s.kept[i] ? " %ld" : " (%ld)", (long)s.value[i]);
                if (col + n > 76) { printf("\n      "); col = 6; }
                printf("%s", v);
                col += n;
            }
            printf("  = %ld%s\n", (long)s.total, s.counting ? " successes" : "");
            continue;
        }
        uint32_t t0 = time_us_32();
        qd_roll_run(&ps->roll, &last_res);
        uint32_t us = time_us_32() - t0;
        last_roll = *ps;
        have_last = true;
        print_result(ps, &last_res);
        print_outcome(ps, &last_res);
        if (last_res.rolled > 100) printf("  (%u dice in %lu us)\n", last_res.rolled, (unsigned long)us);
    }
}

/* last: the last roll again, with every die numbered for 'reroll' */
static void cmd_last(char *args)
{
    (void)args;
    if (!have_last) { printf("nothing rolled yet\n"); return; }
    char tidy[160];
    qd_format(&last_roll, tidy, sizeof(tidy));
    w_add("%s", tidy);
    w_line(2);
    w_add(" ");
    for (int i = 0; i < last_res.dice_count; i++) {
        w_add(" #%d:", i + 1);                /* "#3:5": dice numbers for 'reroll 3' */
        die_glue = true;
        print_die(&last_roll.roll.pools[last_res.dice[i].pool], &last_res.dice[i]);
        die_glue = false;
    }
    w_line(2);
    print_result(&last_roll, &last_res);
    print_outcome(&last_roll, &last_res);
}

/* reroll                the whole last roll (a benny, a push)
 * reroll 2 5            dice #2 and #5 (numbers from 'last')
 * reroll !=6            every die in play that passes the test (Year Zero push)
 * reroll best / worst   roll again and keep the better / worse of the two (luck) */
static void cmd_reroll(char *args)
{
    if (!have_last) { printf("nothing rolled yet\n"); return; }
    fair_mark();
    qd_err_t e = QD_OK;
    bool keep = false;
    while (*args == ' ') args++;
    if (!*args) {
        e = qd_reroll_all(&last_roll.roll, &last_res);
    } else if (!strncmp(args, "best", 4) || !strncmp(args, "worst", 5)) {
        bool took = qd_reroll_keep(&last_roll.roll, &last_res, args[0] == 'b');
        keep = true;
        printf("  rolled %ld and %ld: kept the %s\n", (long)last_res.total, (long)last_res.was_total,
               took ? "new roll" : "first roll");
    } else if (isdigit((unsigned char)*args)) {
        uint16_t which[QD_MAX_KEPT_DICE];
        int n = 0;
        char *s = args;
        while (*s && n < QD_MAX_KEPT_DICE) {
            long v = strtol(s, &s, 10);
            if (v < 1) break;
            which[n++] = (uint16_t)(v - 1);
            while (*s == ' ' || *s == ',' || *s == '#') s++;
        }
        e = qd_reroll_dice(&last_roll.roll, &last_res, which, n);
    } else {
        /* a test: read it as a bucket and use that */
        static qd_parsed_t t;
        char text[48];
        qd_parse_error_t err;
        snprintf(text, sizeof(text), "1d6 [%s]", args);
        if (!qd_parse(text, &t, &err) || t.roll.bucket_count != 1) {
            printf("usage: reroll | reroll 2 5 | reroll !=6 | reroll best | reroll worst\n");
            return;
        }
        int n = qd_reroll_matching(&last_roll.roll, &last_res, -1, t.roll.buckets[0].test);
        if (n < 0) e = QD_ERR_REROLL;
        else printf("  %d dice rerolled\n", n);
    }
    if (e) { printf("can't: %s (see 'last' for the dice numbers)\n", qd_err_text(e)); return; }
    print_result(&last_roll, &last_res);
    print_outcome(&last_roll, &last_res);
    if (!keep)
        printf("  (rerolled %u time%s; before: total %ld)\n", last_res.rerolls,
               last_res.rerolls == 1 ? "" : "s", (long)last_res.was_total);
}

/* parse <text>: show how a roll is read, without rolling it */
static void cmd_parse(char *args)
{
    static qd_parsed_t rolls[4];
    qd_parse_error_t err;
    int n = qd_parse_line(args, rolls, 4, &err);
    if (n < 0) { print_error(args, &err); return; }
    for (int k = 0; k < n; k++) {
        const qd_parsed_t *ps = &rolls[k];
        char tidy[160];
        qd_format(ps, tidy, sizeof(tidy));
        w_add("%s", tidy);
        w_line(2);
        if (ps->repeat > 1) printf("  repeat %u times\n", ps->repeat);
        if (ps->label[0]) printf("  label   \"%s\"\n", ps->label);
        if (ps->comment[0]) printf("  comment \"%s\"\n", ps->comment);
        for (int p = 0; p < ps->roll.pool_count; p++) {
            char t[64];
            qd_format_pool(ps, p, t, sizeof(t));
            printf("  dice %d  %s\n", p + 1, t);
        }
        printf("  sum    ");
        static const char *const op[] = { "", "dice", "", "+", "-", "*", "/", "negate" };
        for (int i = 0; i < ps->roll.op_count; i++) {
            const qd_op_t *o = &ps->roll.ops[i];
            if (o->kind == QD_OP_POOL) printf(" [dice %ld]", (long)o->arg + 1);
            else if (o->kind == QD_OP_NUMBER) printf(" %ld", (long)o->arg);
            else printf(" %s", op[o->kind]);
        }
        printf("   (worked out left to right on a stack)\n");
    }
}

static void cmd_notation(char *args)
{
    (void)args;
    printf(
        "dice      4d6  d20  d%%  4dF  dd66 (tens d6, units d6)  2d{custom}\n"
        "maths     + - * / ( )   usual order: 1d20 - 1d4 * 2 is 1d20 - (1d4 * 2)\n"
        "advantage adv1  dis1        extra die, set aside lowest/highest first\n"
        "explode   e (on max)  e5 (5+)  ie (again and again)  ie5\n"
        "implode   i (on min)  i2 (2 or less): roll one more and subtract it\n"
        "reroll    r (min)  r2 (2 or less)  ir2 (until not)  rg5  irg5 (5+)\n"
        "keep      k3 (highest)  kl3 (lowest)  km3 (middle)  d1 (drop lowest)  dh1\n"
        "count     t8 (8+ succeeds)  tl3 (3 or less)  ds10 (10s count twice)\n"
        "          f1 (1s take a success off)  b1 (count botches)\n"
        "buckets   [<3, =6, 3-5, =max, =+]   at the end of a roll\n"
        "groups    base:4d6 t6 + stress:2d6 t6\n"
        "around    (Label) 1d20+5 ! comment    6 x 4d6k3    2d6 ; 3d8 (up to 4)\n"
        "sets      (6 x 4d6) k3 keeps the 3 best rolls; (7 x 4d6k3) k6 drops the worst\n"
        "target    ... vs 15   pass if the total (or successes) reaches 15\n"
        "sets      5d10 sets   pairs, triples ...      d%% b2 / p1: bonus, penalty dice\n"
        "systems   atk adv + 5 vs 15   chk + 3   save dis + 2         (D&D 5e)\n"
        "          sw8 + 1 vs 6   sw6 x (Extra)                         (Savage Worlds)\n"
        "          coc 65 b1   coc 40 p2 vs hard                       (Call of Cthulhu)\n"
        "          fate + 2 vs 3   vtm 7 h2 vs 3   dcc d20 +1d + 2 vs 15\n"
        "spaces    optional; capitals fine. After dice d1 means drop: +1d1 for a die\n"
        "shown as  5 kept  (5) dropped  ~2~ rerolled  !6 exploded  -7 subtracted\n"
        "          8* success  10** double  1x failure  b botch  2->5 rerolled after\n"
        "'parse <roll>' shows how a roll is read without rolling it.\n");
}

/* --- D7: stats for nerds and odds --------------------------------------- */
static qd_stats_t con_stats;

static void bar(double share, double biggest, int width)
{
    int n = biggest > 0 ? (int)(share / biggest * width + 0.5) : 0;
    for (int i = 0; i < n; i++) putchar('#');
}

/* odds <roll>: the chance of every result, and of each outcome */
static void cmd_odds(char *args)
{
    static qd_parsed_t p;
    static qd_odds_t o;
    qd_parse_error_t err;
    if (!*args) { printf("usage: odds 4d6k3 | odds atk adv + 5 vs 15 | odds sw8 vs 6\n"); return; }
    if (!qd_parse(args, &p, &err)) { print_error(args, &err); return; }
    char tidy[160];
    qd_format(&p, tidy, sizeof(tidy));
    uint32_t t0 = time_us_32();
    qd_err_t e = qd_odds(&p.roll, &o);
    uint32_t us = time_us_32() - t0;
    if (e) { printf("can't: %s\n", qd_err_text(e)); return; }
    printf("%s%s\n", tidy, p.repeat > 1 || p.set ? "   (odds for one of the rolls)" : "");
    if (o.method == QD_ODDS_EXACT_SUM) printf("  exact (worked out die by die)");
    else if (o.method == QD_ODDS_EXACT_ALL) printf("  exact (all %lu rolls tried)", (unsigned long)o.runs);
    else if (o.method == QD_ODDS_NORMAL) printf("  bell curve: exact average and spread, chances very close");
    else printf("  simulated (%lu rolls): close, not exact", (unsigned long)o.runs);
    printf(", %lu ms\n", (unsigned long)(us / 1000));
    printf("  %s %.2f on average, spread %.2f, from %ld to %ld\n", o.counting ? "successes:" : "total:",
           fabs(o.mean) < 0.005 ? 0.0 : o.mean, o.sd, (long)o.lo, (long)o.hi);

    /* The chart covers where 99.99% of results fall: one row per value, or
     * about 20 rows of grouped values. */
    int i0 = 0, i1 = o.bins;
    double run = o.below;
    while (i0 < o.bins && run + o.dist[i0] < 0.00005) run += o.dist[i0++];
    run = o.above;
    while (i1 > i0 && run + o.dist[i1 - 1] < 0.00005) run += o.dist[--i1];
    int n = i1 - i0;
    int group = n <= 40 ? 1 : (n + 19) / 20;
    int rows = (n + group - 1) / group;
    static double row[40];
    double biggest = 0;
    for (int i = 0; i < rows; i++) {
        row[i] = 0;
        for (int j = i0 + i * group; j < i0 + (i + 1) * group && j < i1; j++) row[i] += o.dist[j];
        if (row[i] > biggest) biggest = row[i];
    }
    long span = (long)group * o.width;
    for (int i = 0; i < rows; i++) {
        long from = (long)o.first + (long)i0 * o.width + (long)i * span;
        char range[48];
        if (span == 1) snprintf(range, sizeof(range), "%ld", from);
        else snprintf(range, sizeof(range), "%ld to %ld", from, from + span - 1);
        printf("  %22s", range);
        printf("%6.2f%%  ", 100.0 * row[i]);
        bar(row[i], biggest, 40);
        printf("\n");
    }
    if (o.bins > 40 || o.width > 1) {
        static const int pct[] = { 5, 25, 50, 75, 95 };
        double run = o.below;
        int k = 0;
        printf("  ");
        for (int i = 0; i < o.bins && k < 5; i++) {
            run += o.dist[i];
            while (k < 5 && run * 100 >= pct[k])
                printf("%d%%: %ld  ", pct[k++], (long)(o.first + (long)i * o.width));
        }
        printf("\n");
    }
    if (o.judged) {
        printf("  pass %.2f%%\n", 100.0 * o.pass);
        for (int l = 8; l >= 0; l--) {
            const char *name = qd_sys_level_name(&p.roll, l - 4);
            if (o.level[l] > 0 && *name) printf("    %-20s %6.2f%%\n", name, 100.0 * o.level[l]);
        }
    }
}

static const char *kind_name(const qd_die_stats_t *k, char *buf, int size)
{
    if (!k->die) snprintf(buf, (size_t)size, "d%u", k->sides);
    else if (k->die == qd_fudge_die()) snprintf(buf, (size_t)size, "dF");
    else if (k->die == &con_custom) snprintf(buf, (size_t)size, "d{custom}");
    else snprintf(buf, (size_t)size, "custom die");
    return buf;
}

/* stats | stats 20 (a d20's faces) | stats clear */
static void cmd_stats(char *args)
{
    if (!strncmp(args, "clear", 5)) {
        qd_stats_clear(&con_stats);
        printf("cleared\n");
        return;
    }
    long want = arg_num(&args, 0);
    if (!con_stats.kinds) { printf("no dice rolled yet\n"); return; }
    for (int i = 0; i < con_stats.kinds; i++) {
        const qd_die_stats_t *k = &con_stats.kind[i];
        if (want && (k->die || k->sides != want)) continue;
        qd_stats_report_t r;
        qd_stats_report(k, &r);
        char name[16];
        printf("%-9s %6lu rolls  average %.2f (fair %.2f)  spread %.2f (%.2f)\n",
               kind_name(k, name, sizeof(name)), (unsigned long)k->count, r.mean, r.expect_mean, r.sd, r.expect_sd);
        if (r.verdict == QD_STATS_NO_FACES) continue;
        printf("          top face %lu, bottom %lu; ", (unsigned long)r.highest, (unsigned long)r.lowest);
        if (r.verdict == QD_STATS_TOO_FEW)
            printf("too few to judge (about %d needed)\n", 5 * k->sides);
        else
            printf("chi-square %.1f (limit %.1f): %s\n", r.chi, r.limit,
                   r.verdict == QD_STATS_FAIR ? "looks fair" : "odd, 1 in 1000 for a fair die");
        if (want && k->faces) {
            uint32_t most = 0;
            for (int f = 0; f < k->sides; f++) if (k->face[f] > most) most = k->face[f];
            for (int f = 0; f < k->sides; f++) {
                printf("  %4d %6lu  ", f + 1, (unsigned long)k->face[f]);
                bar(k->face[f], most, 40);
                printf("\n");
            }
        }
    }
    if (con_stats.untracked) printf("(%lu rolls of other dice not tracked)\n", (unsigned long)con_stats.untracked);
}

/* --- D8: random tables (the example in tables/, built on qd_text.h) ------ */
static tbl_set_t con_tables;

/* tables: the built-in sample tables */
static void cmd_tables(char *args)
{
    (void)args;
    for (int k = 0; k < con_tables.count; k++)
        printf("  %d  %-20s %-6s %d entries\n", k + 1, tbl_name(&con_tables, k),
               tbl_dice(&con_tables, k), tbl_entries(&con_tables, k));
    printf("roll one with: table <name or number>   (e.g. table forest)\n");
}

/* table <name or number>: roll on a table, with the working */
static void cmd_table(char *args)
{
    int k = -1;
    if (isdigit((unsigned char)*args)) {
        k = atoi(args) - 1;
    } else if (*args) {
        k = tbl_find(&con_tables, args);
        /* or the first table whose name starts with what was typed */
        for (int i = 0; k < 0 && i < con_tables.count; i++)
            if (!strncasecmp(tbl_name(&con_tables, i), args, strlen(args))) k = i;
    }
    if (k < 0 || k >= con_tables.count) { printf("no such table - try 'tables'\n"); return; }
    fair_mark();
    static tbl_result_t r;
    tbl_roll(&con_tables, k, &r);
    printf("%s\n", tbl_name(&con_tables, k));
    for (int i = 0; i < r.steps; i++) {
        const tbl_step_t *s = &r.step[i];
        printf("  %*s", 2 * s->depth, "");
        if (s->kind == TBL_STEP_INLINE) printf("{%s}: %ld\n", s->what, (long)s->value);
        else printf("%s%s %s: %ld\n", s->kind == TBL_STEP_AGAIN ? "again: " : "",
                    tbl_name(&con_tables, s->table), s->what, (long)s->value);
    }
    printf("  => %s\n", r.text);
    if (r.flags & TBL_DEEP) printf("  (links went too deep; stopped)\n");
    if (r.flags & TBL_CUT)  printf("  (result cut short)\n");
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
    if (count < 0 || !qd_die_custom(&die, faces, (uint16_t)n)) {
        printf("usage: custom <rolls> <face> ...   face = value[*weight][/label]\n"
               "   e.g. custom 20 1 2 3 4 5 6*3          (a six three times as likely)\n"
               "        custom 100000 1 2 3 4 5 6*3      (big counts: a fairness table)\n"
               "        custom 0 1 2 3 4 5 6*3           (keep it as d{custom}, no rolls)\n");
        return;
    }
    con_custom = die;                       /* now usable as d{custom} in rolls */
    con_custom_ready = true;
    if (count == 0) { printf("saved: roll it with d{custom}, e.g. 3d{custom} k2\n"); return; }
    fair_mark();
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
    double limit = qd_chi_limit(n - 1);
    printf("chi-square %.1f, limit %.1f: %s\n", chi, limit, chi < limit ? "PASS" : "FAIL");
}

static qd_deck_t deck;
static bool deck_ready;

static void deck_status(void)
{
    printf("%d cards, %d in the deck, %d out; reshuffle:%s%s%s%s\n", deck.count,
           qd_deck_remaining(&deck), qd_deck_out_count(&deck),
           deck.policy & QD_RESHUFFLE_EVERY_DRAW ? " every-draw" : "",
           deck.policy & QD_RESHUFFLE_WHEN_EMPTY ? " when-empty" : "",
           deck.policy & QD_RESHUFFLE_AFTER_TRIGGER ? " after-joker" : "",
           deck.policy ? "" : " on demand only");
    if (deck.triggered && (deck.policy & QD_RESHUFFLE_AFTER_TRIGGER))
        printf("a joker is out: the next deal reshuffles first\n");
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
    fair_mark();
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

/* --- D9: fair mode ---------------------------------------------------------
 *
 * fair                  start a session (or, if one is on, show it)
 * fair end              end it and show the key
 * fair check <words>    check a revealed key: then "#17 4d6k3" repeats #17
 */
static uint8_t fair_print_at_start[QD_FAIR_PRINT_BYTES];
static uint8_t key_part[QD_FAIR_KEY_BYTES];  /* 'fair check' with the key split over lines */
static int     key_part_n;
static void    fair_check_key(const uint8_t key[QD_FAIR_KEY_BYTES]);
static bool    fair_checking;          /* the key was typed in: checking, not live */

/* 16 words on two lines of 8 */
static void print_key(const uint8_t *key)
{
    char line[QD_FAIR_TEXT];
    qd_fair_words(key, 8, line, sizeof(line));
    printf("  %s\n", line);
    qd_fair_words(key + 8, 8, line, sizeof(line));
    printf("  %s\n", line);
}

static void cmd_fair(char *args)
{
    char words[QD_FAIR_TEXT];
    if (!*args) {
        if (qd_fair_is_on()) {
            qd_fair_fingerprint_words(words, sizeof(words));
            if (fair_checking) {
                printf("checking a fair session; its fingerprint: %s\n", words);
                printf("type actions as #3 4d6k3; 'fair end' when done\n");
            } else {
                printf("fair mode is on; fingerprint: %s\n", words);
                printf("last action #%lu; 'fair end' shows the key\n", (unsigned long)qd_fair_count());
            }
            return;
        }
        qd_fair_start();
        fair_checking = false;
        qd_fair_fingerprint(fair_print_at_start);
        qd_fair_fingerprint_words(words, sizeof(words));
        printf("fair mode on. The fingerprint - everyone write it down now:\n");
        printf("  %s\n", words);
        printf("Each roll, reroll, deal and table roll is numbered from #1.\n");
        printf("'fair end' shows the key; check any action with it afterwards.\n");
        return;
    }
    if (!strcmp(args, "end")) {
        uint8_t key[QD_FAIR_KEY_BYTES];
        uint32_t last = qd_fair_count();
        if (!qd_fair_end(key)) { printf("fair mode is off\n"); return; }
        if (fair_checking) {
            fair_checking = false;
            printf("done checking; back to live rolls\n");
            return;
        }
        printf("fair mode off after %lu action%s. The key (16 words):\n", (unsigned long)last,
               last == 1 ? "" : "s");
        print_key(key);
        qd_fair_words(fair_print_at_start, QD_FAIR_PRINT_BYTES, words, sizeof(words));
        printf("Its fingerprint, shown at the start: %s\n", words);
        printf("To check: 'fair check' and the 16 words, then each action with its\n");
        printf("number, e.g. #3 4d6k3   (the PC build of this console does the same)\n");
        memset(key, 0, sizeof(key));
        return;
    }
    if (!strncmp(args, "check", 5) && (args[5] == ' ' || !args[5])) {
        uint8_t key[QD_FAIR_KEY_BYTES];
        int n = qd_fair_read_words(args + 5, key, QD_FAIR_KEY_BYTES);
        if (n < 0) { printf("word %d isn't one of the fair-mode words\n", -n); return; }
        if (n == 0) { printf("usage: fair check <the 16 key words>\n"); return; }
        if (n < QD_FAIR_KEY_BYTES) {
            /* the key is shown on two lines, so it often arrives on two */
            memcpy(key_part, key, (size_t)n);
            key_part_n = n;
            printf("%d words so far: type the other %d on the next line\n", n,
                   QD_FAIR_KEY_BYTES - n);
            return;
        }
        fair_check_key(key);
        return;
    }
    printf("usage: fair | fair end | fair check <16 words>\n");
}

/* Start checking with a whole key: the second half of 'fair check'. */
static void fair_check_key(const uint8_t key[QD_FAIR_KEY_BYTES])
{
    char words[QD_FAIR_TEXT];
    if (qd_fair_is_on() && !fair_checking) {
        printf("a live session is running: 'fair end' it first\n");
        return;
    }
    qd_fair_start_key(key);
    fair_checking = true;
    qd_fair_fingerprint(fair_print_at_start);
    qd_fair_fingerprint_words(words, sizeof(words));
    printf("checking with this key. Its fingerprint:\n  %s\n", words);
    printf("If that matches the one written down at the start, the key is the\n");
    printf("session's. Now type each action with its number, in any order:\n");
    printf("  #3 4d6k3   #4 reroll 2   #7 table forest   (rerolls and deals need\n");
    printf("  the actions before them typed first). 'fair end' when done.\n");
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
    { "roll",    "roll 4d6k3 (or type 4d6k3 on its own)",             cmd_roll    },
    { "parse",   "parse <roll>: how it is read, without rolling", cmd_parse  },
    { "last",    "the last roll, dice numbered for reroll",     cmd_last    },
    { "odds",    "odds <roll>: chance of each result and outcome", cmd_odds  },
    { "tables",  "the built-in random tables",                  cmd_tables  },
    { "table",   "table <name or number>: roll on a table",     cmd_table   },
    { "stats",   "stats | stats 20 | stats clear: every die so far", cmd_stats },
    { "reroll",  "reroll | reroll 2 5 | reroll !=6 | reroll best/worst", cmd_reroll },
    { "notation","what you can type in a roll",                 cmd_notation },
    { "pct",     "pct [b2|p1] [n]: d100, CoC bonus/penalty dice", cmd_pct   },
    { "custom",  "custom <rolls> 1 2 6*3 0/+ ...: your own die", cmd_custom  },
    { "deck",    "deck poker [jokers] | tarot | sw: new deck",  cmd_deck    },
    { "deal",    "deal [n]: one deal of n cards",               cmd_deal    },
    { "shuffle", "put every card back",                         cmd_shuffle },
    { "policy",  "policy every/empty/joker/none: reshuffle rules", cmd_policy },
    { "fair",    "fair | fair end | fair check <words>: checkable rolls", cmd_fair },
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
static void run_command(char *line);

static void run_line(char *line)
{
    while (*line == ' ') line++;
    if (key_part_n) {
        /* The rest of a key that 'fair check' started on the line before?
         * Only if this line is exactly the missing words: "table forest"
         * is two fair-mode words too, and should still roll on a table. */
        uint8_t more[QD_FAIR_KEY_BYTES];
        int want = QD_FAIR_KEY_BYTES - key_part_n;
        int n = qd_fair_read_words(line, more, QD_FAIR_KEY_BYTES);
        int had = key_part_n;
        key_part_n = 0;
        if (n == want) {
            memcpy(key_part + had, more, (size_t)n);
            fair_check_key(key_part);
            return;
        }
        if (n > 0)
            printf("(%d words: not the other %d of the key, so the key is dropped)\n", n, want);
    }
    if (*line == '#' && isdigit((unsigned char)line[1])) {
        /* "#17 4d6k3": checking a fair session, action 17 */
        char *end;
        unsigned long n = strtoul(line + 1, &end, 10);
        if (!qd_fair_is_on() || !qd_fair_begin_at(qd_fair_count())) {
            printf("#number is for checking a fair session: 'fair check <key words>' first\n");
            return;
        }
        if (n == 0 || n > 0xFFFFFFFFul) { printf("actions are numbered from #1\n"); return; }
        fair_at = (uint32_t)n;
        line = end;
        while (*line == ' ') line++;
        if (!*line) {
            printf("#%lu what? Type the action after its number: #%lu 4d6k3\n", n, n);
            fair_at = 0;
            return;
        }
    }
    run_command(line);
    fair_at = 0;                            /* used, or the command didn't roll */
}

static void run_command(char *line)
{
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
    /* Anything else is tried as a roll: "4d6k3", "(Bite) 1d20+5", "6 x 4d6k3". */
    if (*args) line[strlen(line)] = ' ';    /* put the line back together */
    static const char *const system_words[] = { "atk", "chk", "check", "save", "sav", "fate", "coc",
                                                 "vtm", "dcc" };
    bool looks_like_roll = !isalpha((unsigned char)*line) || tolower((unsigned char)*line) == 'd' ||
                           strchr(line, ':') != NULL ||
                           (tolower((unsigned char)line[0]) == 's' && tolower((unsigned char)line[1]) == 'w' &&
                            isdigit((unsigned char)line[2]));
    for (size_t k = 0; k < sizeof(system_words) / sizeof(system_words[0]); k++) {
        size_t n = strlen(system_words[k]);
        /* a whole word, or one running straight into "v" / "vs" ("atkv20") */
        char after = (char)tolower((unsigned char)line[n]);
        if (!strncasecmp(line, system_words[k], n) &&
            (!isalpha((unsigned char)after) ||
             (after == 'v' && !isalpha((unsigned char)line[n + 1 + (tolower((unsigned char)line[n + 1]) == 's')]))))
            looks_like_roll = true;
    }
    if (looks_like_roll) {
        cmd_roll(line);
        return;
    }
    char *space = strchr(line, ' ');
    if (space) *space = '\0';
    printf("unknown command '%s' - try help\n", line);
}

int main(void)
{
    stdio_init_all();
    for (int i = 0; i < 30 && !stdio_usb_connected(); i++)   /* up to 3 s for the PC */
        sleep_ms(100);
    qd_rng_init_hw();                                         /* hardware seed at boot */
    qd_parse_set_lookup(con_lookup, NULL);
    qd_stats_watch(&con_stats);                               /* count every die from now on */
    tbl_open(&con_tables, sample_tables, sizeof(sample_tables));
    printf("\nQD4P %s console - type help\n> ", qd_version());

    char line[128];
    size_t n = 0;
    for (;;) {
        int c = getchar_timeout_us(10000);
        if (c == PICO_ERROR_TIMEOUT) continue;
        if (c == '\r' || c == '\n') {
            if (ECHO_LINES) printf("%.*s", (int)n, line);
            if (ECHO_KEYS || ECHO_LINES) printf("\n");   /* a PC terminal echoed it already */
            line[n] = '\0';
            run_line(line);
            n = 0;
            printf("> ");
        } else if ((c == 8 || c == 127) && n > 0) {
            n--;
            if (ECHO_KEYS) printf("\b \b");
        } else if (c >= 0x20 && c < 0x7F && n < sizeof(line) - 1) {
            line[n++] = (char)c;
            if (ECHO_KEYS) putchar(c);
        }
    }
}
