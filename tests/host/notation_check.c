/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    notation_check.c
 * @brief   Reads rolls from stdin and says whether each one parses as the
 *          notation guide claims. Driven by notation_examples.py.
 *
 * Each input line is one of:
 *     OK<TAB>roll                    the roll must parse (and roll)
 *     ERR<TAB>words|words<TAB>roll   it must fail, with a message holding
 *                                    one of the |-separated words
 * Every line that isn't as expected is printed; the exit code is the count.
 */
#include <stdio.h>
#include <string.h>

#include "qd4p.h"

/* The custom dice the guide mentions by name. */
static qd_die_t loaded, custom;
static const qd_die_t *lookup(const char *name, void *ctx)
{
    (void)ctx;
    if (!strcmp(name, "Loaded")) return &loaded;
    if (!strcmp(name, "custom")) return &custom;
    return NULL;
}

int main(void)
{
    static const qd_face_t faces[] = { { 1, 1, "" }, { 2, 1, "" }, { 3, 1, "" },
                                       { 4, 1, "" }, { 5, 1, "" }, { 6, 3, "" } };
    static qd_parsed_t rolls[4];
    static qd_result_t res;
    qd_die_custom(&loaded, faces, 6);
    qd_die_custom(&custom, faces, 6);
    qd_parse_set_lookup(lookup, NULL);
    qd_rng_seed_fixed(1);

    char line[512];
    int bad = 0, n = 0;
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\n")] = '\0';
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = '\0';
        char *roll = tab + 1, *want = NULL;
        if (!strcmp(line, "ERR")) {
            want = roll;
            tab = strchr(roll, '\t');
            if (!tab) continue;
            *tab = '\0';
            roll = tab + 1;
        }
        n++;
        qd_parse_error_t err;
        int count = qd_parse_line(roll, rolls, 4, &err);
        if (!want) {
            bool ran = count > 0;
            for (int k = 0; ran && k < count; k++)
                if (rolls[k].repeat <= QD_MAX_REPEAT) ran = qd_roll_run(&rolls[k].roll, &res) == QD_OK;
            if (!ran) {
                printf("should parse: '%s' (%s)\n", roll, count < 0 ? err.message : "didn't roll");
                bad++;
            }
        } else {
            bool found = false;
            for (char *w = strtok(want, "|"); w && !found; w = strtok(NULL, "|"))
                found = count < 0 && strstr(err.message, w) != NULL;
            if (!found) {
                printf("should fail with '%s': '%s' (%s)\n", want, roll,
                       count < 0 ? err.message : "it parsed");
                bad++;
            }
        }
    }
    printf("%d notation examples checked\n", n);
    return bad;
}
