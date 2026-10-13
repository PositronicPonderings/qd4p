/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_text.c
 * @brief   Dice in text, and rolls that pick from ranges.
 */
#include <stdio.h>
#include <string.h>

#include "qd_parse.h"
#include "qd_text.h"

bool qd_roll_textn(const char *notation, int n, int32_t *value)
{
    static qd_parsed_t p;
    static qd_result_t res;
    char buf[64];
    qd_parse_error_t err;
    *value = 0;
    if (!notation || n <= 0 || n >= (int)sizeof(buf))
        return false;
    memcpy(buf, notation, (size_t)n);
    buf[n] = '\0';
    if (!qd_parse(buf, &p, &err) || qd_roll_run(&p.roll, &res) != QD_OK)
        return false;
    *value = (res.flags & QD_RES_COUNTING) ? res.successes : res.total;
    return true;
}

bool qd_roll_text(const char *notation, int32_t *value)
{
    return qd_roll_textn(notation, notation ? (int)strlen(notation) : 0, value);
}

int qd_range_find(const qd_range_t *ranges, int n, int32_t v)
{
    for (int i = 0; i < n; i++)
        if (v >= ranges[i].lo && v <= ranges[i].hi)
            return i;
    return -1;
}

int qd_pick_range(const char *notation, const qd_range_t *ranges, int n, int32_t *value)
{
    if (!qd_roll_text(notation, value))
        return -2;
    return qd_range_find(ranges, n, *value);
}

/* ========================================================================
 * qd_expand
 * ======================================================================== */
typedef struct {
    char *out;
    int size, len;
    qd_expand_info_t *info;
} out_t;

static void flag(out_t *o, uint16_t f)
{
    if (o->info) o->info->flags |= f;
}

static void put(out_t *o, const char *s, int n)
{
    for (int i = 0; i < n; i++) {
        if (o->len >= o->size - 1) {
            flag(o, QD_EXPAND_CUT);
            break;
        }
        o->out[o->len++] = s[i];
    }
    if (o->size > 0) o->out[o->len] = '\0';
}

static void note(out_t *o, int32_t v, const char *what, int n)
{
    qd_expand_info_t *in = o->info;
    if (!in) return;
    if (in->rolls >= QD_EXPAND_ROLLS) {
        in->flags |= QD_EXPAND_FULL;
        return;
    }
    qd_expand_roll_t *r = &in->roll[in->rolls++];
    r->value = v;
    if (n > (int)sizeof(r->what) - 1) n = (int)sizeof(r->what) - 1;
    memcpy(r->what, what, (size_t)n);
    r->what[n] = '\0';
}

/* One {roll}: roll it and write its value. Out of line (GCC, Clang): an
 * app's [link] hook may call qd_expand_ex again, once per level, and this
 * keeps the number's buffer out of every level's stack frame. */
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static bool roll_in(out_t *o, const char *what, int n)
{
    int32_t v;
    if (!qd_roll_textn(what, n, &v))
        return false;
    char num[16];
    put(o, num, snprintf(num, sizeof(num), "%ld", (long)v));
    note(o, v, what, n);
    return true;
}

int qd_expand_ex(const char *text, char *out, int size, qd_expand_info_t *info,
                 qd_expand_hook_t hook, void *ctx)
{
    out_t o = { out, size, 0, info };
    if (info) memset(info, 0, sizeof(*info));
    if (size <= 0) return 0;
    out[0] = '\0';
    const char *s = text ? text : "";
    while (*s) {
        if (*s == '\\' && s[1]) {                       /* \{ \[ \\ : the character itself */
            put(&o, s + 1, 1);
            s += 2;
        } else if (*s == '{') {
            const char *e = strchr(s, '}');
            if (!e || !roll_in(&o, s + 1, (int)(e - s - 1))) {
                flag(&o, QD_EXPAND_BAD);
                put(&o, "{?}", 3);
                if (!e) break;
            }
            s = e + 1;
        } else if (*s == '[' && hook && strchr(s, ']')) {
            const char *e = strchr(s, ']');
            int room = o.size - 1 - o.len;
            int w = hook(s + 1, (int)(e - s - 1), o.out + o.len, room, ctx);
            if (w < 0) {
                put(&o, s, (int)(e - s + 1));            /* the app left it alone */
            } else {
                if (w > room) { w = room; flag(&o, QD_EXPAND_CUT); }
                o.len += w;
                o.out[o.len] = '\0';
            }
            s = e + 1;
        } else {
            put(&o, s, 1);
            s++;
        }
    }
    return o.len;
}

int qd_expand(const char *text, char *out, int size, qd_expand_info_t *info)
{
    return qd_expand_ex(text, out, size, info, NULL, NULL);
}
