/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_tables.c
 * @brief   D8: the example table system (examples/tables), rolled from an
 *          asset built by its tables.py (run_tests.sh builds tables_test.txt
 *          into build/tables_test.h).
 * also: ../../examples/tables/table.c
 */
#include <string.h>

#include "qd4p.h"
#include "qd_test.h"
#include "table.h"
#include "tables_test.h"

static tbl_set_t T;
static tbl_result_t r;

static int find(const char *name) { return tbl_find(&T, name); }

static void test_open(void)
{
    tbl_set_t t;
    CHECK(tbl_open(&T, tables_test, sizeof(tables_test)) && T.count == 9, "the asset opens: %d tables", T.count);
    CHECK(!tbl_open(&t, tables_test, sizeof(tables_test) - 1), "a cut-short asset is refused");
    static uint8_t bad[sizeof(tables_test)];
    memcpy(bad, tables_test, sizeof(bad));
    bad[0] = 'X';
    CHECK(!tbl_open(&t, bad, sizeof(bad)), "a wrong header is refused");
    memcpy(bad, tables_test, sizeof(bad));
    bad[8] = 0xFF; bad[9] = 0xFF;                           /* first table's offset off the end */
    CHECK(!tbl_open(&t, bad, sizeof(bad)), "an offset past the end is refused");
    CHECK(!tbl_open(&t, "QDT1", 4), "too short to hold a header");

    CHECK(find("two") == 0 && find("TWO") == 0 && find("Bandits") == -1, "names match in any case");
    CHECK(!strcmp(tbl_dice(&T, find("Escapes")), "1d1") && tbl_entries(&T, find("Two")) == 3,
          "a table without a roll line gets 1dN from its ranges");
    CHECK(!tbl_roll(&T, 99, &r), "a missing table index is refused");
}

static void test_odds(void)
{
    int low = 0, seven = 0, high = 0, ok = 1;
    const int N = 36000;
    for (int i = 0; i < N; i++) {
        tbl_roll(&T, find("Two"), &r);
        low += !strcmp(r.text, "Low");
        seven += !strcmp(r.text, "Seven");
        high += !strcmp(r.text, "High");
        ok &= r.steps == 1 && r.step[0].kind == TBL_STEP_TABLE && r.step[0].value >= 2 && r.step[0].value <= 12 &&
              !strcmp(r.step[0].what, "2d6") &&
              (r.step[0].value == 7) == !strcmp(r.text, "Seven");
    }
    double e7 = N / 6.0, e = N * 15 / 36.0;
    double chi = (seven - e7) * (seven - e7) / e7 + (low - e) * (low - e) / e + (high - e) * (high - e) / e;
    CHECK(ok && low + seven + high == N, "every roll lands on its entry, with its working");
    CHECK(chi < 13.82, "2d6 table: Low %d, Seven %d, High %d (chi-square %.1f)", low, seven, high, chi);

    int lo = 0, mid = 0, hi = 0;
    for (int i = 0; i < 3600; i++) {
        tbl_roll(&T, find("Open"), &r);
        lo += !strcmp(r.text, "Low");
        mid += !strcmp(r.text, "Middle");
        hi += !strcmp(r.text, "High");
    }
    CHECK(lo + mid + hi == 3600 && lo > 0 && hi > 0, "open-ended ranges: 5- and 15+ (%d, %d, %d)", lo, mid, hi);
}

static void test_links(void)
{
    int seen[5] = { 0 };
    for (int i = 0; i < 4000; i++) {
        tbl_roll(&T, find("Links"), &r);
        int v = r.step[0].value;
        seen[v]++;
        if (v == 1) {
            bool ok = (!strcmp(r.text, "A Low") || !strcmp(r.text, "A Seven") || !strcmp(r.text, "A High")) &&
                      r.steps == 2 && r.step[1].depth == 1 && r.step[1].table == find("Two");
            CHECK(ok, "a link rolls on the other table: '%s'", r.text);
        } else if (v == 2) {
            int n = 0;
            bool ok = sscanf(r.text, "%d", &n) == 1 && n >= 101 && n <= 106 && r.steps == 2 &&
                      r.step[1].kind == TBL_STEP_INLINE && r.step[1].value == n && !strcmp(r.step[1].what, "1d6+100");
            CHECK(ok, "an inline roll: '%s'", r.text);
        } else if (v == 3) {
            CHECK((r.flags & TBL_DEEP) && strstr(r.text, "down down") && strstr(r.text, "..."),
                  "a table that links to itself stops at the depth limit: '%s'", r.text);
        } else {
            bool ok = strstr(r.text, "; ") && !strstr(r.text, "Roll twice") && r.step[1].kind == TBL_STEP_AGAIN;
            CHECK(ok, "roll twice: two results, never itself: '%s'", r.text);
        }
        if (qd_test_failures) break;
    }
    CHECK(seen[1] && seen[2] && seen[3] && seen[4], "all four entries came up");

    int parts_ok = 1;
    for (int i = 0; i < 2000; i++) {
        tbl_roll(&T, find("Again"), &r);
        if (r.step[0].value == 6) {
            int parts = 1;
            for (const char *p = r.text; (p = strstr(p, "; ")); p++) parts++;
            parts_ok &= parts == 3 && !strcmp(r.text, "x1; x1; x1");
        } else {
            parts_ok &= !strcmp(r.text, "x1");
        }
    }
    CHECK(parts_ok, "roll 3 times: three results, rerolling any roll-again that comes up");
}

static void test_edges(void)
{
    int gaps = 0;
    for (int i = 0; i < 1000; i++) {
        tbl_roll(&T, find("Gappy"), &r);
        if (r.flags & TBL_NO_ROW) {
            gaps++;
            CHECK(strstr(r.text, "(no entry for ") && r.step[0].value > 5, "a gap says so: '%s'", r.text);
            if (qd_test_failures) break;
        }
    }
    CHECK(gaps > 350 && gaps < 650, "a 1d10 table covering 1-5 has no entry about half the time (%d)", gaps);

    tbl_roll(&T, find("Escapes"), &r);
    CHECK(!strcmp(r.text, "Braces {not a roll} and [not a link]") && r.flags == 0, "escapes: '%s'", r.text);

    tbl_roll(&T, find("Long"), &r);
    CHECK((r.flags & TBL_CUT) && strlen(r.text) == TBL_TEXT, "long results are cut at %d", TBL_TEXT);

    tbl_result_t a, b;
    qd_rng_seed_fixed(5);
    tbl_roll(&T, find("Links"), &a);
    qd_rng_seed_fixed(5);
    tbl_roll(&T, find("Links"), &b);
    CHECK(!strcmp(a.text, b.text), "replay mode gives the same result");
}

int main(void)
{
    qd_rng_init_hw();
    test_open();
    test_odds();
    test_links();
    test_edges();
    return qd_test_done("random tables");
}
