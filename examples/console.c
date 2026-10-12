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

/* Roll plain dice. Takes "24d19" (the usual notation), "d20" or the
 * older "d 19 24" form. The full notation (4d6k3, 2d6+3, ...) arrives
 * with the parser in D4. */
static void roll_plain(long count, long sides)
{
    if (sides < 1 || count < 1 || count > 100000) {
        printf("usage: 24d19, d20, or d <sides> [count]\n"); return;
    }
    long total = 0;
    for (long i = 0; i < count; i++) {
        int32_t v = qd_rng_between(1, (int32_t)sides);
        total += v;
        if (count <= 100) printf("%ld%s", (long)v, (i % 20 == 19 || i == count - 1) ? "\n" : " ");
    }
    printf("total %ld, average %.2f (expected %.2f)\n", total, (double)total / count,
           (sides + 1) / 2.0);
}

static void cmd_d(char *args)
{
    long sides = arg_num(&args, 0), count = arg_num(&args, 10);
    roll_plain(count, sides);
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

static void roll_fudge(long count)
{
    if (count < 1 || count > 100) { printf("usage: 4dF (1 to 100 Fudge dice)\n"); return; }
    qd_die_t f;
    qd_die_fudge(&f);
    int total = 0;
    for (long i = 0; i < count; i++) {
        qd_face_roll_t r = qd_die_roll(&f);
        printf("[%s]", qd_die_face_label(&f, r.face));
        total += r.value;
    }
    printf("  = %+d\n", total);
}

static void cmd_d66(char *args)
{
    long n = arg_num(&args, 1);
    for (long i = 0; i < n; i++)
        printf("%d%s", qd_roll_d66().result, (i % 10 == 9 || i == n - 1) ? "\n" : " ");
}

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
        double expect = count * qd_die_face_ppm(&die, (uint16_t)f) / 1e6;
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
    { "d",       "roll dice: type 24d19, d20 or 4dF",             cmd_d       },
    { "test",    "test <sides> <rolls>: fairness check",        cmd_test    },
    { "stir",    "stir <text>: mix your own input in",          cmd_stir    },
    { "trng",    "trng [n]: stir in n hardware TRNG readings",  cmd_trng    },
    { "adc",     "adc <gpio> [n]: stir in ADC noise",           cmd_adc     },
    { "tap",     "tap [seconds]: stir in your key timings",     cmd_tap     },
    { "d66",     "d66 [n]: tens and units d6",                  cmd_d66     },
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
    /* "24d19", "d20" or "4dF" typed as the command itself */
    char *d = strchr(line, 'd');
    if (d && (d[1] == 'F' || d[1] == 'f') && !d[2]) {
        char *end;
        long count = (d == line) ? 1 : strtol(line, &end, 10);
        if (d == line || end == d) { roll_fudge(count); return; }
    }
    if (d && d[1] >= '1' && d[1] <= '9') {
        char *end;
        long count = (d == line) ? 1 : strtol(line, &end, 10);
        if (d == line || end == d) {
            long sides = strtol(d + 1, &end, 10);
            if (!*end) { roll_plain(count, sides); return; }
        }
    }
    printf("unknown command '%s' - try help\n", line);
}

int main(void)
{
    stdio_init_all();
    for (int i = 0; i < 30 && !stdio_usb_connected(); i++)   /* up to 3 s for the PC */
        sleep_ms(100);
    qd_rng_init_hw();                                         /* hardware seed at boot */
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
