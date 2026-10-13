/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_parse.c
 * @brief   D4: dice notation. Every golden roll is parsed from its text
 *          (as written, without spaces, and in capitals) and compared die
 *          by die with the Python reference; then precedence, labels,
 *          buckets, errors and writing notation back out.
 */
#include <ctype.h>
#include <string.h>

#include "qd4p.h"
#include "qd_test.h"
#include "roll_goldens.h"

static const qd_face_t loaded_faces[6] = {
    { 1, 1, "" }, { 2, 1, "" }, { 3, 1, "" }, { 4, 1, "" }, { 5, 1, "" }, { 6, 3, "" },
};
static qd_die_t loaded;

static const qd_die_t *lookup(const char *name, void *ctx)
{
    (void)ctx;
    return !strcmp(name, "loaded") ? &loaded : NULL;
}

static qd_parsed_t parsed, again;
static qd_result_t res;
static qd_parse_error_t err;

/* Parse `text`, roll it from `seed`, compare with golden g. */
static bool matches_golden(const char *text, const golden_t *g)
{
    const uint16_t tracked = QD_DIE_DROPPED | QD_DIE_SET_ASIDE | QD_DIE_REROLLED |
                             QD_DIE_EXPLODED | QD_DIE_IMPLODED | QD_DIE_REPLACEMENT;
    if (!qd_parse(text, &parsed, &err)) {
        printf("  '%s': column %d: %s\n", text, err.pos + 1, err.message);
        return false;
    }
    qd_rng_seed_fixed((uint64_t)g->seed);
    if (qd_roll_run(&parsed.roll, &res) != QD_OK)
        return false;
    bool ok = res.pools[0].rolled == g->n && res.total == g->total &&
              res.pools[0].successes == g->successes && res.pools[0].failures == g->failures;
    for (int i = 0; ok && i < g->n && i < res.dice_count; i++)
        ok = res.dice[i].value == g->values[i] && (res.dice[i].flags & tracked) == g->flags[i] &&
             res.dice[i].parent == g->parents[i];
    return ok;
}

static void test_goldens(void)
{
    int bad = 0;
    for (size_t k = 0; k < sizeof(goldens) / sizeof(goldens[0]); k++) {
        const golden_t *g = &goldens[k];
        char tight[64], caps[64], tidy[128];
        int n = 0;
        for (const char *c = g->text; *c && n < 63; c++)
            if (*c != ' ') tight[n++] = *c;
        tight[n] = '\0';
        for (n = 0; g->text[n] && n < 63; n++)
            caps[n] = (char)toupper((unsigned char)g->text[n]);
        caps[n] = '\0';
        if (strstr(caps, "{LOADED}")) memcpy(strstr(caps, "{LOADED}"), "{loaded}", 8);

        bool ok = matches_golden(g->text, g) && matches_golden(tight, g) && matches_golden(caps, g);
        qd_parse(g->text, &parsed, &err);
        qd_format(&parsed, tidy, sizeof(tidy));
        ok = ok && matches_golden(tidy, g);
        if (!ok && bad++ < 5)
            printf("  golden '%s' seed %d (tight '%s', tidy '%s') differs\n", g->text, g->seed, tight, tidy);
        CHECK(ok, "'%s' seed %d", g->text, g->seed);
    }
    printf("  %d golden rolls parsed four ways (as written, no spaces, capitals, re-written)\n",
           (int)(sizeof(goldens) / sizeof(goldens[0])));
}

/* --- Maths ------------------------------------------------------------------ */
typedef long (*expect_fn)(const qd_result_t *r);
#define P(k) ((long)r->pools[k].total)
static long e0(const qd_result_t *r) { return P(0) - P(1) * 2; }
static long e1(const qd_result_t *r) { return (P(0) + 2) * 3; }
static long e2(const qd_result_t *r) { return -P(0); }
static long e3(const qd_result_t *r) { return 2 * -P(0); }
static long e4(const qd_result_t *r) { return P(0) - (P(1) - P(2)); }
static long e5(const qd_result_t *r) { return 100 / P(0) / 2; }
static long e6(const qd_result_t *r) { return P(0) - P(1) - P(2); }
static long e7(const qd_result_t *r) { return P(0) + P(1) * P(2) - 7 / 2; }
static long e8(const qd_result_t *r) { return -(P(0) + 1) * -2; }
static long e9(const qd_result_t *r) { return ((P(0))); }

static void test_maths(void)
{
    static const struct { const char *text; expect_fn f; } cases[] = {
        { "1d20 - 1d4 * 2", e0 },        { "(1d6+2)*3", e1 },       { "-1d4", e2 },
        { "2*-1d6", e3 },                { "1d6-(1d4-1d8)", e4 },   { "100/1d6/2", e5 },
        { "1d6 - 1d4 - 1d8", e6 },       { "d20+d6*d4-7/2", e7 },   { "-(1d6+1)*-2", e8 },
        { "((1d10))", e9 },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        bool ok = qd_parse(cases[c].text, &parsed, &err);
        for (int i = 0; ok && i < 300; i++) {
            qd_roll_run(&parsed.roll, &res);
            ok = res.total == cases[c].f(&res);
        }
        char tidy[96];
        qd_format(&parsed, tidy, sizeof(tidy));
        bool same = qd_parse(tidy, &again, &err);
        for (int i = 0; same && i < 300; i++) {
            qd_roll_run(&again.roll, &res);
            same = res.total == cases[c].f(&res);
        }
        CHECK(ok && same, "'%s' (written back as '%s') works out right", cases[c].text, tidy);
    }
}

/* --- Everything around the dice ----------------------------------------- */
static void test_extras(void)
{
    CHECK(qd_parse("(Bite) 1d20+14 ! vs AC 15", &parsed, &err) &&
          !strcmp(parsed.label, "Bite") && !strcmp(parsed.comment, "vs AC 15") && parsed.repeat == 1,
          "label and comment");
    CHECK(qd_parse("6 x 4d6k3", &parsed, &err) && parsed.repeat == 6, "6 x 4d6k3");
    CHECK(qd_parse("6x4d6k3", &parsed, &err) && parsed.repeat == 6, "6x4d6k3");
    CHECK(qd_parse("(Stats) 6 x 4d6 k3", &parsed, &err) && parsed.repeat == 6 &&
          !strcmp(parsed.label, "Stats"), "label then repeat");
    CHECK(qd_parse("3 x (Fire) 8d6", &parsed, &err) && parsed.repeat == 3 &&
          !strcmp(parsed.label, "Fire"), "repeat then label");
    CHECK(qd_parse("(d6+1)*2", &parsed, &err) && !parsed.label[0], "(d6+1) is maths, not a label");
    CHECK(qd_parse("(dodge) 1d20", &parsed, &err) && !strcmp(parsed.label, "dodge"), "(dodge) is a label");
    CHECK(qd_parse("(dF)", &parsed, &err) && !parsed.label[0] && parsed.roll.pools[0].die,
          "(dF) is maths");

    static qd_parsed_t rolls[4];
    int n = qd_parse_line("2d6 ; (Hit) 3d8 ; 1d20+5 ! note; 4d10", rolls, 4, &err);
    CHECK(n == 4 && !strcmp(rolls[1].label, "Hit") && !strcmp(rolls[2].comment, "note") &&
          rolls[3].roll.pools[0].sides == 10, "four rolls on one line");
    CHECK(qd_parse_line("1d4;1d4;1d4;1d4;1d4", rolls, 4, &err) == -1 && err.pos == 15,
          "a fifth roll is refused at its ';' (column %d)", err.pos + 1);
    CHECK(!qd_parse("1d4; 1d6", &parsed, &err) && err.pos == 3, "qd_parse takes one roll");

    CHECK(qd_parse("base:4d6 t6 + stress:2d6 t6", &parsed, &err) &&
          !strcmp(parsed.roll.pools[0].name, "base") && !strcmp(parsed.roll.pools[1].name, "stress"),
          "group names");
    CHECK(qd_parse("2d{ loaded } + 1", &parsed, &err) && parsed.roll.pools[0].die == &loaded &&
          !strcmp(parsed.die_names[0], "loaded"), "custom die by name");

    CHECK(qd_parse("10d6 [<3, =6, 3-5, =max, !=1, >=4, <=2, >5]", &parsed, &err) &&
          parsed.roll.bucket_count == 8, "eight buckets");
    const qd_test_t *b = &parsed.roll.buckets[0].test;
    CHECK(b[0].op == QD_TEST_LT && b[0].a == 3 && b[2].op == QD_TEST_RANGE && b[2].b == 5 &&
          b[3].op == QD_TEST_MAX && b[4].op == QD_TEST_NE, "bucket tests read right");
    CHECK(qd_parse("4dF [=+, =\"-\", =-1, 0]", &parsed, &err) &&
          !strcmp(parsed.roll.buckets[0].test.label, "+") &&
          !strcmp(parsed.roll.buckets[1].test.label, "-") &&
          parsed.roll.buckets[2].test.a == -1 && parsed.roll.buckets[3].test.op == QD_TEST_EQ,
          "label buckets, a negative value, a bare number");

    const qd_pool_t *p;
    CHECK(qd_parse("1d20 adv2 dis1", &parsed, &err) && parsed.roll.pools[0].advantage == 1,
          "adv2 dis1 is adv1");
    CHECK(qd_parse("4D6 K3", &parsed, &err) && parsed.roll.pools[0].keep == QD_KEEP_HIGH,
          "capitals");
    CHECK(qd_parse("4d6d1", &parsed, &err) && parsed.roll.pools[0].keep == QD_DROP_LOW &&
          parsed.roll.pool_count == 1, "4d6d1 drops the lowest");
    CHECK(qd_parse("4d6+1d1", &parsed, &err) && parsed.roll.pool_count == 2, "+1d1 is a die");
    CHECK(qd_parse("6d10 tl3 ds", &parsed, &err) && (p = &parsed.roll.pools[0])->twice.op == QD_TEST_LE &&
          p->twice.a == 3, "ds on its own doubles every success");
    CHECK(qd_parse("d%", &parsed, &err) && parsed.roll.pools[0].sides == 100, "d%% is d100");
    CHECK(qd_parse("3dd666", &parsed, &err) && parsed.roll.pools[0].digits == 3, "dd666");
    CHECK(qd_parse("1d10 i e", &parsed, &err) && parsed.roll.pools[0].step_count == 2 &&
          parsed.roll.pools[0].steps[0].kind == QD_STEP_IMPLODE, "'i e' is implode then explode");
    CHECK(qd_parse("1d10 ie", &parsed, &err) && parsed.roll.pools[0].step_count == 1 &&
          parsed.roll.pools[0].steps[0].forever, "'ie' is explode again and again");
}

/* --- Errors ----------------------------------------------------------------- */
static void test_errors(void)
{
    static const struct { const char *text; int col; const char *says; } cases[] = {
        { "",               1,  "nothing to roll" },
        { "4d6 +",          6,  "stops too soon" },
        { "4d6 k3 x",       8,  "isn't a modifier" },
        { "4d6 kitten",     7,  "t needs a number" },   /* k, i, then t */
        { "4 d6",           3,  "no space inside dice: write 4d6" },
        { "12 d7 K4",       4,  "write 12d7" },
        { "4d",             3,  "what kind of die" },
        { "4d0",            3,  "at least 1 side" },
        { "4d40000",        3,  "at most 32767 sides" },
        { "1001d6",         1,  "at most 1000 dice" },
        { "(1d6+2",         7,  "missing ')' for the '(' at column 1" },
        { "1d6+2)",         6,  "without its '('" },
        { "1d6 / 0",        5,  "divide by zero" },
        { "3 + 4",          1,  "no dice" },
        { "4d6 k3 k2",      8,  "only one keep" },
        { "4d6 dd66",       5,  "need a +" },
        { "dd60",           4,  "not 0" },
        { "2dd66 e",        7,  "can't explode" },
        { "1d6 ie1",        1,  "never stop" },
        { "2d{nope}",       4,  "no custom die called 'nope'" },
        { "2d{loaded",      3,  "missing '}'" },
        { "1d6 [<3",        5,  "missing ']'" },
        { "1d6 [<x]",       7,  "a number should follow" },
        { "base:5",         1,  "goes before dice" },
        { "1001 x 1d6",     1,  "repeat 1 to 1000" },
        { "400 atk vs 12",  5,  "to roll it 400 times, write 400 x" },
        { "400(atk v 12)",  4,  "write 400 x" },
        { "100atkv20",      4,  "to roll it 100 times, write 100 x" },
        { "2x3",            1,  "no dice" },
        { "2d6vhard",       5,  "vs needs a number" },
        { "atkins",         1,  "" },
        { "(This label is far too long) 1d6", 1, "at most 23" },
        { "6d10 ds10",      6,  "ds goes after t" },
        { "chk d30 vs 5",   5,  "else write 1d30 vs 5" },
        { "sw4 - x 2",      7,  "x (an Extra) goes after" },
        { "1d6 e e e e e",  13, "at most 4" },
        { "1d6+1d6+1d6+1d6+1d6+1d6+1d6+1d6+1d6", 33, "at most 8 sets" },
        { "((((((((((1d6))))))))))", 9, "nested too deep" },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        bool parsed_ok = qd_parse(cases[c].text, &parsed, &err);
        CHECK(!parsed_ok && err.pos + 1 == cases[c].col && strstr(err.message, cases[c].says),
              "'%s': column %d '%s' (wanted column %d, '%s')", cases[c].text, err.pos + 1,
              err.message, cases[c].col, cases[c].says);
    }
}

/* --- Writing notation back ------------------------------------------------- */
static void test_format(void)
{
    static const struct { const char *in, *out; } cases[] = {
        { "4d6k3+2",                    "4d6 k3 + 2" },
        { "(Bite) 1d20+14 ! vs AC 15",  "(Bite) 1d20 + 14 ! vs AC 15" },
        { "6x4d6k3",                    "6 x 4d6 k3" },
        { "(1d8+2)*2",                  "(1d8 + 2) * 2" },
        { "1d20-(1d4-1)",               "1d20 - (1d4 - 1)" },
        { "-(1d6+1)",                   "-(1d6 + 1)" },
        { "4d10t8ie10f1",               "4d10 ie10 t8 f1" },
        { "10d6e6k8+4",                 "10d6 e6 k8 + 4" },
        { "1d20adv1",                   "1d20 adv1" },
        { "2d20 dis",                   "2d20 dis1" },
        { "4dF [=+]",                   "4dF [=\"+\"]" },
        { "100d6 [<3,=6,3-5]",          "100d6 [<3, =6, 3-5]" },
        { "base:4d6t6+stress:2d6t6",    "base:4d6 t6 + stress:2d6 t6" },
        { "5dd66dh1",                   "5dd66 dh1" },
        { "1d10 i e",                   "1d10 i e" },
        { "6d10 t7 ds10",               "6d10 t7 ds10" },
        { "6d10 tl3 ds",                "6d10 tl3 ds" },
        { "8d6 ir2 rg5 b",              "8d6 ir2 rg5 b" },
        { "2d{loaded}",                 "2d{loaded}" },
        { "D%",                         "1d%" },
        { "d% b2",                      "1d% b2" },
        { "atk adv+5 vs 15",            "atk adv + 5 vs 15" },
        { "chk+3",                      "chk + 3" },
        { "check dis - 1 vs 12",        "chk dis - 1 vs 12" },
        { "save+2",                     "save + 2" },
        { "sw8+1 vs 6",                 "sw8 + 1 vs 6" },
        { "sw12 x",                     "sw12 x" },
        { "coc 65 b2 p1",               "coc 65 b1" },
        { "coc 40 p2 vs hard",          "coc 40 p2 vs hard" },
        { "fate+2 vs 3",                "fate + 2 vs 3" },
        { "vtm 7 h2 vs 3",              "vtm 7 h2 vs 3" },
        { "vtm 4",                      "vtm 4" },
        { "dcc d20+1d+2",               "dcc 1d20 +1d + 2" },
        { "dcc 3d6 -2d",                "dcc 3d6 -2d" },
        { "(Shove) atk+4 vs 13 ! prone", "(Shove) atk + 4 vs 13 ! prone" },
        { "5d10 sets",                  "5d10 sets" },
        { "1d20+5 v 15",                "1d20 + 5 vs 15" },
        { "chk d20 v 15",               "chk vs 15" },
        { "atk adv 1d20 +4",            "atk adv + 4" },
        { "sav dis+1",                  "save dis + 1" },
        { "sw4-2 x",                    "sw4 x - 2" },
        { "sw4x-2 vs 4",                "sw4 x - 2 vs 4" },
        { "sw6 + 1 x v 6",              "sw6 x + 1 vs 6" },
        { "dcc d20 + d + 2 vs 15",      "dcc 1d20 +1d + 2 vs 15" },
        { "dcc d16 -d -d",              "dcc 1d16 -2d" },
        { "6d10 t8 vs 3",               "6d10 t8 vs 3" },
        { "1d20+5 vs -2",               "1d20 + 5 vs -2" },
        /* a repeat's x and a system word may touch what follows */
        { "100xatkv20",                 "100 x atk vs 20" },
        { "100 xatk v20",               "100 x atk vs 20" },
        { "100 x atkv20",               "100 x atk vs 20" },
        { "6x d20",                     "6 x 1d20" },
        { "(6xatkv12)k2",               "(6 x atk vs 12) k2" },
        { "atkv20",                     "atk vs 20" },
        { "chkvs15",                    "chk vs 15" },
        { "savev12",                    "save vs 12" },
        { "atk advv10",                 "atk adv vs 10" },
        { "coc50vhard",                 "coc 50 vs hard" },
        { "coc 50 vsextreme",           "coc 50 vs extreme" },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        char out[128], twice[128];
        bool ok = qd_parse(cases[c].in, &parsed, &err);
        qd_format(&parsed, out, sizeof(out));
        ok = ok && qd_parse(out, &again, &err);
        qd_format(&again, twice, sizeof(twice));
        CHECK(ok && !strcmp(out, cases[c].out) && !strcmp(out, twice),
              "'%s' is written '%s' (wanted '%s', then '%s')", cases[c].in, out, cases[c].out, twice);
    }
    char small[8];
    qd_parse("4d6 k3 + 1d4 + 2", &parsed, &err);
    int n = qd_format(&parsed, small, sizeof(small));
    CHECK(n > 7 && strlen(small) == 7, "a short buffer is cut safely (needed %d)", n);
}

int main(void)
{
    qd_die_custom(&loaded, loaded_faces, 6);
    qd_parse_set_lookup(lookup, NULL);
    test_goldens();
    qd_rng_init_hw();
    test_maths();
    test_extras();
    test_errors();
    test_format();
    return qd_test_done("notation");
}
