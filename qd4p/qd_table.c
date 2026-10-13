/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_table.c
 * @brief   Random tables, read in place from the asset tools/tables.py
 *          builds.
 *
 * THE ASSET (little-endian; strings end in a 0 byte so they can be used
 * where they lie):
 *     "QDT1"  u16 table count  u16 0  u32 offset of each table
 *     table:  name\0  dice\0  u16 entry count  entries
 *     entry:  i32 low  i32 high  u8 kind (0 text, 1 roll again)  u8 times  text\0
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "qd_parse.h"
#include "qd_table.h"

/* ========================================================================
 * Reading the asset
 * ======================================================================== */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* The end of a string starting at `at` (just past its 0), or 0 if it runs
 * off the end of the asset. */
static uint32_t skip_str(const qd_tables_t *t, uint32_t at)
{
    while (at < t->size && t->data[at]) at++;
    return at < t->size ? at + 1 : 0;
}

static uint32_t table_at(const qd_tables_t *t, int index)
{
    return rd32(t->data + 8 + 4 * (uint32_t)index);
}

bool qd_tables_open(qd_tables_t *t, const void *data, uint32_t size)
{
    memset(t, 0, sizeof(*t));
    const uint8_t *d = (const uint8_t *)data;
    if (!d || size < 8 || memcmp(d, "QDT1", 4))
        return false;
    qd_tables_t c = { d, size, rd16(d + 4) };
    if (8 + 4u * c.count > size)
        return false;
    /* Walk everything once now, so rolling never needs to check. */
    for (int k = 0; k < c.count; k++) {
        uint32_t at = table_at(&c, k);
        if (at >= size || !(at = skip_str(&c, at)) || !(at = skip_str(&c, at)) || at + 2 > size)
            return false;
        uint16_t n = rd16(d + at);
        at += 2;
        for (int e = 0; e < n; e++) {
            if (at + 10 > size || !(at = skip_str(&c, at + 10)))
                return false;
        }
    }
    *t = c;
    return true;
}

const char *qd_table_name(const qd_tables_t *t, int index)
{
    return index >= 0 && index < t->count ? (const char *)t->data + table_at(t, index) : "";
}

const char *qd_table_dice(const qd_tables_t *t, int index)
{
    if (index < 0 || index >= t->count) return "";
    return (const char *)t->data + skip_str(t, table_at(t, index));
}

/* Where a table's entries start, and how many. */
static uint32_t entries_at(const qd_tables_t *t, int index, int *n)
{
    uint32_t at = skip_str(t, skip_str(t, table_at(t, index)));
    *n = rd16(t->data + at);
    return at + 2;
}

int qd_table_entries(const qd_tables_t *t, int index)
{
    int n = 0;
    if (index >= 0 && index < t->count) entries_at(t, index, &n);
    return n;
}

int qd_tables_find(const qd_tables_t *t, const char *name)
{
    for (int k = 0; k < t->count; k++) {
        const char *a = qd_table_name(t, k), *b = name;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
        if (!*a && !*b)
            return k;
    }
    return -1;
}

/* ========================================================================
 * Rolling
 * ======================================================================== */
typedef struct {
    const qd_tables_t *t;
    qd_table_result_t *out;
    int len;
} ctx_t;

static void put(ctx_t *c, const char *s, int n)
{
    for (int i = 0; i < n; i++) {
        if (c->len >= QD_TABLE_TEXT) {
            c->out->flags |= QD_TABLE_CUT;
            return;
        }
        c->out->text[c->len++] = s[i];
    }
    c->out->text[c->len] = '\0';
}

static void puts_(ctx_t *c, const char *s)
{
    put(c, s, (int)strlen(s));
}

static void step(ctx_t *c, uint8_t kind, int depth, int table, int32_t value, const char *what, int n)
{
    qd_table_result_t *o = c->out;
    if (o->steps >= QD_TABLE_STEPS) {
        o->flags |= QD_TABLE_STEPS_FULL;
        return;
    }
    qd_table_step_t *s = &o->step[o->steps++];
    s->kind = kind;
    s->depth = (uint8_t)depth;
    s->table = (int16_t)table;
    s->value = value;
    if (n > (int)sizeof(s->what) - 1) n = (int)sizeof(s->what) - 1;
    memcpy(s->what, what, (size_t)n);
    s->what[n] = '\0';
}

/* Roll dice written as text (n characters): the total, or the successes. */
static bool roll_text(const char *text, int n, int32_t *value)
{
    static qd_parsed_t p;
    static qd_result_t res;
    char buf[64];
    qd_parse_error_t err;
    if (n <= 0 || n >= (int)sizeof(buf))
        return false;
    memcpy(buf, text, (size_t)n);
    buf[n] = '\0';
    if (!qd_parse(buf, &p, &err) || qd_roll_run(&p.roll, &res) != QD_OK)
        return false;
    *value = (res.flags & QD_RES_COUNTING) ? res.successes : res.total;
    return true;
}

static void roll_table(ctx_t *c, int index, int depth);

/* An entry's text with its {rolls} and [links] worked out. */
static void expand(ctx_t *c, const char *s, int depth)
{
    while (*s) {
        if (*s == '\\' && s[1]) {
            put(c, s + 1, 1);
            s += 2;
        } else if (*s == '{') {
            const char *e = strchr(s, '}');
            int32_t v;
            if (!e || !roll_text(s + 1, (int)(e - s - 1), &v)) {
                c->out->flags |= QD_TABLE_BAD;
                puts_(c, "{?}");
                if (!e) return;
            } else {
                char num[16];
                snprintf(num, sizeof(num), "%ld", (long)v);
                puts_(c, num);
                step(c, QD_STEP_INLINE, depth, -1, v, s + 1, (int)(e - s - 1));
            }
            s = e + 1;
        } else if (*s == '[') {
            const char *e = strchr(s, ']');
            char name[64];
            int n = e ? (int)(e - s - 1) : 0;
            int idx = -1;
            if (e && n > 0 && n < (int)sizeof(name)) {
                memcpy(name, s + 1, (size_t)n);
                name[n] = '\0';
                idx = qd_tables_find(c->t, name);
            }
            if (idx < 0) {
                c->out->flags |= QD_TABLE_BAD;
                puts_(c, "[?]");
                if (!e) return;
            } else {
                roll_table(c, idx, depth + 1);
            }
            s = e + 1;
        } else {
            put(c, s, 1);
            s++;
        }
    }
}

/* The entry a value lands on: its address, or 0 for none. */
static uint32_t find_entry(const qd_tables_t *t, int index, int32_t v)
{
    int n;
    uint32_t at = entries_at(t, index, &n);
    for (int e = 0; e < n; e++) {
        int32_t lo = (int32_t)rd32(t->data + at), hi = (int32_t)rd32(t->data + at + 4);
        if (v >= lo && v <= hi)
            return at;
        at = skip_str(t, at + 10);
    }
    return 0;
}

/* Roll the table's dice once, note it, and give back the entry. */
static uint32_t pick(ctx_t *c, int index, int depth, uint8_t kind, int32_t *v)
{
    const char *dice = qd_table_dice(c->t, index);
    if (!roll_text(dice, (int)strlen(dice), v)) {
        c->out->flags |= QD_TABLE_BAD;
        return 0;
    }
    step(c, kind, depth, index, *v, dice, (int)strlen(dice));
    return find_entry(c->t, index, *v);
}

static void roll_table(ctx_t *c, int index, int depth)
{
    if (depth >= QD_TABLE_DEPTH) {
        c->out->flags |= QD_TABLE_DEEP;
        puts_(c, "...");
        return;
    }
    int32_t v;
    uint32_t e = pick(c, index, depth, QD_STEP_TABLE, &v);
    if (!e) {
        char msg[32];
        snprintf(msg, sizeof(msg), "(no entry for %ld)", (long)v);
        c->out->flags |= QD_TABLE_NO_ROW;
        puts_(c, msg);
        return;
    }
    const uint8_t *d = c->t->data;
    if (d[e + 8] != 1) {                                     /* an ordinary entry */
        expand(c, (const char *)d + e + 10, depth);
        return;
    }
    /* "Roll twice": that many more rolls on this table, skipping any
     * roll-again entry that comes up (up to 20 tries each). */
    int times = d[e + 9];
    for (int k = 0; k < times; k++) {
        uint32_t f = 0;
        for (int tries = 0; tries < 20; tries++) {
            f = pick(c, index, depth, QD_STEP_AGAIN, &v);
            if (!f || d[f + 8] != 1) break;
        }
        if (k) puts_(c, "; ");
        if (!f || d[f + 8] == 1) {
            char msg[32];
            snprintf(msg, sizeof(msg), "(no entry for %ld)", (long)v);
            c->out->flags |= QD_TABLE_NO_ROW;
            puts_(c, msg);
        } else {
            expand(c, (const char *)d + f + 10, depth);
        }
    }
}

qd_err_t qd_table_roll(const qd_tables_t *t, int index, qd_table_result_t *out)
{
    memset(out, 0, sizeof(*out));
    if (index < 0 || index >= t->count)
        return QD_ERR_NO_DICE;
    ctx_t c = { t, out, 0 };
    roll_table(&c, index, 0);
    return QD_OK;
}
