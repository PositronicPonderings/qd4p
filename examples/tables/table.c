/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    table.c
 * @brief   Example random tables, read in place from the asset tables.py
 *          builds, rolled with QD4P's qd_roll_text and qd_expand.
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

#include "table.h"

/* ========================================================================
 * Reading the asset
 * ======================================================================== */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Just past the 0 that ends the string at `at`, or 0 if it runs off the end. */
static uint32_t skip_str(const tbl_set_t *t, uint32_t at)
{
    while (at < t->size && t->data[at]) at++;
    return at < t->size ? at + 1 : 0;
}

static uint32_t table_at(const tbl_set_t *t, int index)
{
    return rd32(t->data + 8 + 4 * (uint32_t)index);
}

bool tbl_open(tbl_set_t *t, const void *data, uint32_t size)
{
    memset(t, 0, sizeof(*t));
    const uint8_t *d = (const uint8_t *)data;
    if (!d || size < 8 || memcmp(d, "QDT1", 4))
        return false;
    tbl_set_t c = { d, size, rd16(d + 4) };
    if (8 + 4u * c.count > size)
        return false;
    /* Walk everything once now, so rolling never needs to check. */
    for (int k = 0; k < c.count; k++) {
        uint32_t at = table_at(&c, k);
        if (at >= size || !(at = skip_str(&c, at)) || !(at = skip_str(&c, at)) || at + 2 > size)
            return false;
        uint16_t n = rd16(d + at);
        at += 2;
        for (int e = 0; e < n; e++)
            if (at + 10 > size || !(at = skip_str(&c, at + 10)))
                return false;
    }
    *t = c;
    return true;
}

const char *tbl_name(const tbl_set_t *t, int index)
{
    return index >= 0 && index < t->count ? (const char *)t->data + table_at(t, index) : "";
}

const char *tbl_dice(const tbl_set_t *t, int index)
{
    if (index < 0 || index >= t->count) return "";
    return (const char *)t->data + skip_str(t, table_at(t, index));
}

static uint32_t entries_at(const tbl_set_t *t, int index, int *n)
{
    uint32_t at = skip_str(t, skip_str(t, table_at(t, index)));
    *n = rd16(t->data + at);
    return at + 2;
}

int tbl_entries(const tbl_set_t *t, int index)
{
    int n = 0;
    if (index >= 0 && index < t->count) entries_at(t, index, &n);
    return n;
}

static int find_n(const tbl_set_t *t, const char *name, int len)
{
    for (int k = 0; k < t->count; k++) {
        const char *a = tbl_name(t, k);
        int i = 0;
        while (i < len && a[i] && tolower((unsigned char)a[i]) == tolower((unsigned char)name[i])) i++;
        if (i == len && !a[i])
            return k;
    }
    return -1;
}

int tbl_find(const tbl_set_t *t, const char *name)
{
    return find_n(t, name, (int)strlen(name));
}

/* ========================================================================
 * Rolling
 * ======================================================================== */
typedef struct {
    const tbl_set_t *t;
    tbl_result_t    *res;
} ctx_t;

typedef struct {
    ctx_t *c;
    int    depth;
} link_t;

static void step(ctx_t *c, uint8_t kind, int depth, int table, int32_t value, const char *what)
{
    tbl_result_t *r = c->res;
    if (r->steps >= TBL_STEPS) {
        r->flags |= TBL_STEPS_FULL;
        return;
    }
    tbl_step_t *s = &r->step[r->steps++];
    s->kind = kind;
    s->depth = (uint8_t)depth;
    s->table = (int16_t)table;
    s->value = value;
    snprintf(s->what, sizeof(s->what), "%s", what);
}

/* Copy a short message into out (room characters at most). */
static int say(char *out, int room, const char *msg)
{
    int n = (int)strlen(msg);
    if (n > room) n = room;
    memcpy(out, msg, (size_t)n);
    return n;
}

static int roll_into(ctx_t *c, int index, int depth, char *out, int room);

/* qd_expand calls this for each [Name]: roll on that table, into its text. */
static int link_hook(const char *name, int n, char *out, int room, void *vctx)
{
    link_t *l = (link_t *)vctx;
    int k = find_n(l->c->t, name, n);
    if (k < 0) {
        l->c->res->flags |= TBL_BAD;
        return say(out, room, "[?]");
    }
    return roll_into(l->c, k, l->depth + 1, out, room);
}

/* An entry's text, with {rolls} and [links] worked out. */
static int expand_into(ctx_t *c, const char *text, int depth, char *out, int room)
{
    static qd_expand_info_t info[TBL_DEPTH];        /* one per depth: kept off the stack */
    link_t l = { c, depth };
    int n = qd_expand_ex(text, out, room + 1, &info[depth], link_hook, &l);
    if (info[depth].flags & QD_EXPAND_CUT) c->res->flags |= TBL_CUT;
    if (info[depth].flags & QD_EXPAND_BAD) c->res->flags |= TBL_BAD;
    for (int i = 0; i < info[depth].rolls; i++)
        step(c, TBL_STEP_INLINE, depth, -1, info[depth].roll[i].value, info[depth].roll[i].what);
    return n;
}

/* Roll the table's dice and find the entry: its address, or 0 for none. */
static uint32_t pick(ctx_t *c, int index, int depth, uint8_t kind, int32_t *v)
{
    const char *dice = tbl_dice(c->t, index);
    if (!qd_roll_text(dice, v)) {
        c->res->flags |= TBL_BAD;
        return 0;
    }
    step(c, kind, depth, index, *v, dice);
    int n;
    uint32_t at = entries_at(c->t, index, &n);
    for (int e = 0; e < n; e++) {
        int32_t lo = (int32_t)rd32(c->t->data + at), hi = (int32_t)rd32(c->t->data + at + 4);
        if (*v >= lo && *v <= hi)
            return at;
        at = skip_str(c->t, at + 10);
    }
    return 0;
}

/* Out of line (GCC, Clang) so its message buffer isn't carried in
 * roll_into's frame, which repeats once per level of [links]. */
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static int no_entry(ctx_t *c, int32_t v, char *out, int room)
{
    char msg[32];
    snprintf(msg, sizeof(msg), "(no entry for %ld)", (long)v);
    c->res->flags |= TBL_NO_ROW;
    return say(out, room, msg);
}

static int roll_into(ctx_t *c, int index, int depth, char *out, int room)
{
    if (depth >= TBL_DEPTH) {
        c->res->flags |= TBL_DEEP;
        return say(out, room, "...");
    }
    const uint8_t *d = c->t->data;
    int32_t v;
    uint32_t e = pick(c, index, depth, TBL_STEP_TABLE, &v);
    if (!e)
        return no_entry(c, v, out, room);
    if (d[e + 8] != 1)                                       /* an ordinary entry */
        return expand_into(c, (const char *)d + e + 10, depth, out, room);

    /* "Roll twice": that many more rolls on this table, skipping any
     * roll-again entry that comes up (up to 20 tries each). */
    int len = 0, times = d[e + 9];
    for (int k = 0; k < times; k++) {
        uint32_t f = 0;
        for (int tries = 0; tries < 20; tries++) {
            f = pick(c, index, depth, TBL_STEP_AGAIN, &v);
            if (!f || d[f + 8] != 1) break;
        }
        if (k) len += say(out + len, room - len, "; ");
        if (!f || d[f + 8] == 1)
            len += no_entry(c, v, out + len, room - len);
        else
            len += expand_into(c, (const char *)d + f + 10, depth, out + len, room - len);
    }
    return len;
}

bool tbl_roll(const tbl_set_t *t, int index, tbl_result_t *out)
{
    memset(out, 0, sizeof(*out));
    if (index < 0 || index >= t->count)
        return false;
    ctx_t c = { t, out };
    int n = roll_into(&c, index, 0, out->text, TBL_TEXT);
    out->text[n] = '\0';
    if (n >= TBL_TEXT) out->flags |= TBL_CUT;
    return true;
}
