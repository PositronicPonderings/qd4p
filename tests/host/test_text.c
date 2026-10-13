/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_text.c
 * @brief   D8: dice in text (qd_expand) and rolls that pick from ranges.
 */
#include <stdlib.h>
#include <string.h>

#include "qd4p.h"
#include "qd_test.h"

static char out[128];
static qd_expand_info_t info;

static void test_roll_text(void)
{
    int32_t v;
    bool ok = true;
    for (int i = 0; i < 1000; i++) ok &= qd_roll_text("2d6+1", &v) && v >= 3 && v <= 13;
    CHECK(ok, "qd_roll_text gives totals");
    CHECK(qd_roll_text("6d10 t11", &v) && v == 0, "and successes for a success roll");
    CHECK(!qd_roll_text("2d", &v) && v == 0 && !qd_roll_text("", &v) && !qd_roll_text(NULL, &v),
          "unreadable notation is refused");
    CHECK(qd_roll_textn("1d1+4 and more", 5, &v) && v == 5, "qd_roll_textn reads only n characters");
}

static void test_ranges(void)
{
    static const qd_range_t r[] = { { 2, 6 }, { 7, 7 }, { 8, 12 }, { 13, INT32_MAX } };
    CHECK(qd_range_find(r, 4, 2) == 0 && qd_range_find(r, 4, 7) == 1 && qd_range_find(r, 4, 99) == 3 &&
          qd_range_find(r, 4, 1) == -1, "qd_range_find");
    int count[3] = { 0 }, ok = 1;
    const int N = 36000;
    for (int i = 0; i < N; i++) {
        int32_t v;
        int k = qd_pick_range("2d6", r, 3, &v);
        ok &= k >= 0 && k < 3 && v >= 2 && v <= 12 && qd_range_find(r, 3, v) == k;
        count[k]++;
    }
    double e[3] = { N * 15 / 36.0, N / 6.0, N * 15 / 36.0 }, chi = 0;
    for (int k = 0; k < 3; k++) chi += (count[k] - e[k]) * (count[k] - e[k]) / e[k];
    CHECK(ok && chi < 13.82, "qd_pick_range 2d6: %d %d %d (chi-square %.1f)", count[0], count[1], count[2], chi);
    int32_t v;
    CHECK(qd_pick_range("1d4+10", r, 2, &v) == -1 && v >= 11, "a roll in no range: -1");
    CHECK(qd_pick_range("nonsense", r, 2, &v) == -2, "unreadable notation: -2");
}

static int shout(const char *name, int n, char *o, int room, void *ctx)
{
    (*(int *)ctx)++;
    if (n == 4 && !memcmp(name, "keep", 4)) return -1;      /* leave [keep] alone */
    int w = 0;
    for (int i = 0; i < n && w < room; i++) o[w++] = (char)(name[i] >= 'a' && name[i] <= 'z' ? name[i] - 32 : name[i]);
    return w;
}

static void test_expand(void)
{
    int n = qd_expand("{1d1+2} wolves and {2d1*10} gp", out, sizeof(out), &info);
    CHECK(!strcmp(out, "3 wolves and 20 gp") && n == (int)strlen(out) && info.rolls == 2 && info.flags == 0 &&
          info.roll[0].value == 3 && !strcmp(info.roll[0].what, "1d1+2") && !strcmp(info.roll[1].what, "2d1*10"),
          "rolls in text, with the working: '%s'", out);
    qd_expand("plain text, [not a link]", out, sizeof(out), &info);
    CHECK(!strcmp(out, "plain text, [not a link]") && !info.rolls, "no rolls, brackets left alone");
    qd_expand("\\{not\\} a \\\\ roll", out, sizeof(out), NULL);
    CHECK(!strcmp(out, "{not} a \\ roll"), "escapes: '%s'", out);
    qd_expand("{2d} and {", out, sizeof(out), &info);
    CHECK(!strcmp(out, "{?} and {?}") && (info.flags & QD_EXPAND_BAD), "bad rolls show {?}: '%s'", out);
    qd_expand("{6d10 t8}", out, sizeof(out), &info);
    CHECK(atoi(out) >= 0 && atoi(out) <= 6, "a success roll gives its successes");

    char small[8];
    n = qd_expand("abcdefghij", small, sizeof(small), &info);
    CHECK(n == 7 && !strcmp(small, "abcdefg") && (info.flags & QD_EXPAND_CUT), "cut to fit, flagged");

    int calls = 0;
    qd_expand_ex("[orc] meets [keep] and [elf]", out, sizeof(out), &info, shout, &calls);
    CHECK(!strcmp(out, "ORC meets [keep] and ELF") && calls == 3, "a hook for [brackets]: '%s'", out);

    char many[256] = "";
    for (int i = 0; i < QD_EXPAND_ROLLS + 2; i++) strcat(many, "{1d1}");
    qd_expand(many, out, sizeof(out), &info);
    CHECK(info.rolls == QD_EXPAND_ROLLS && (info.flags & QD_EXPAND_FULL), "the working holds %d rolls", QD_EXPAND_ROLLS);
}

int main(void)
{
    qd_rng_init_hw();
    test_roll_text();
    test_ranges();
    test_expand();
    return qd_test_done("dice in text");
}
