/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_parse.c
 * @brief   Dice notation parser and formatter.
 *
 * A hand-written recursive descent parser. Each function below reads one
 * piece of the grammar and pushes what it found straight into a qd_roll_t:
 * pools for dice, and the sum as a small stack program (numbers and pool
 * totals pushed, operators applied after their operands).
 *
 *   line     roll { ";" roll }
 *   roll     { repeat | label } sum [ buckets ] [ "!" comment ]
 *   repeat   NUMBER "x"                       6 x 4d6k3
 *   label    "(" letter ... ")"               (Bite)
 *   sum      product { ("+" | "-") product }
 *   product  unary { ("*" | "/") unary }
 *   unary    "-" unary | "+" unary | primary
 *   primary  NUMBER | dice | "(" sum ")"
 *   dice     [ name ":" ] [ NUMBER ] "d" ( NUMBER | "%" | "F" | "{" name "}" | "d" DIGITS )
 *            { modifier }
 *   buckets  "[" bucket { "," bucket } "]"
 *
 * Spaces are allowed between any two pieces (not inside a number or in
 * "4d6" itself). Keywords ignore case. Errors stop at the first problem
 * and report its column, so a person can see exactly where to look.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "qd_parse.h"
#include "qd_sys.h"

static qd_die_lookup_t lookup_fn;
static void *lookup_ctx;

void qd_parse_set_lookup(qd_die_lookup_t fn, void *ctx)
{
    lookup_fn = fn;
    lookup_ctx = ctx;
}

static const qd_die_t *fudge_die(void) { return qd_fudge_die(); }

/* ========================================================================
 * Reading characters
 * ======================================================================== */
typedef struct {
    const char        *s;               /* the whole line */
    int                i;               /* where we are */
    int                end;             /* end of this roll: its ';' or the line's end */
    qd_parsed_t       *out;
    qd_parse_error_t  *err;
    bool               failed;
    int                depth;           /* brackets open */
    int                sw_x;            /* where a later Savage Worlds "x" is, or -1 */
} ps_t;

/* Record the first problem; always returns false so callers can
 * `return fail(...)`. */
static bool fail(ps_t *ps, int pos, int len, const char *fmt, ...)
{
    if (!ps->failed) {
        ps->failed = true;
        ps->err->pos = (uint16_t)(pos < 0 ? 0 : pos);
        ps->err->len = (uint16_t)(len < 1 ? 1 : len);
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(ps->err->message, sizeof(ps->err->message), fmt, ap);
        va_end(ap);
    }
    return false;
}

static char at(const ps_t *ps, int k)   { return k < ps->end ? ps->s[k] : '\0'; }
static char peek(const ps_t *ps)        { return at(ps, ps->i); }
static char lower(char c)               { return (char)tolower((unsigned char)c); }
static bool is_digit(char c)            { return c >= '0' && c <= '9'; }
static bool is_alpha(char c)            { return isalpha((unsigned char)c) != 0; }

static void skip_space(ps_t *ps)
{
    while (peek(ps) == ' ' || peek(ps) == '\t')
        ps->i++;
}

/* Does the text at the current position start with `w` (any case)? */
static bool looking_at(const ps_t *ps, const char *w)
{
    for (int k = 0; w[k]; k++)
        if (lower(at(ps, ps->i + k)) != w[k])
            return false;
    return true;
}

/* A whole number: digits only. Refuses anything over a billion, which is
 * far past any limit and keeps the sums safely inside 32 bits. */
static bool read_number(ps_t *ps, long *v)
{
    int start = ps->i;
    long n = 0;
    if (!is_digit(peek(ps)))
        return false;
    while (is_digit(peek(ps))) {
        n = n * 10 + (peek(ps) - '0');
        ps->i++;
        if (n > 1000000000L)
            return fail(ps, start, ps->i - start, "number too big");
    }
    *v = n;
    return true;
}

static bool push(ps_t *ps, qd_op_kind_t kind, int32_t arg, int pos)
{
    if (!qd_roll_push(&ps->out->roll, kind, arg))
        return fail(ps, pos, 1, "too long: at most %d numbers, dice and signs", QD_MAX_ROLL_OPS);
    return true;
}

/* ========================================================================
 * Dice and their modifiers
 * ======================================================================== */
enum { M_ADV, M_DIS, M_IRG, M_IR, M_IE, M_I, M_RG, M_R, M_KL, M_KM, M_K, M_DH, M_DS,
       M_DD, M_D, M_TL, M_T, M_F, M_B, M_E, M_SETS, M_P };

/* Longest first wherever one keyword starts another ("irg" before "ir"). */
static const char *const keywords[] = {
    "adv", "dis", "irg", "ir", "ie", "i", "rg", "r", "kl", "km", "k", "dh", "ds",
    "dd", "d", "tl", "t", "f", "b", "e", "sets", "p",
};

/* "vs" (or just "v") ends the dice part: it isn't a modifier. Returns
 * the length of the word, or 0. */
static int vs_len(const ps_t *ps)
{
    if (looking_at(ps, "vs") && !is_alpha(at(ps, ps->i + 2))) return 2;
    if (looking_at(ps, "v") && !is_alpha(at(ps, ps->i + 1))) return 1;
    return 0;
}

static bool at_vs(const ps_t *ps)
{
    return vs_len(ps) > 0;
}

static bool modifiers(ps_t *ps, qd_pool_t *p)
{
    for (;;) {
        skip_space(ps);
        int start = ps->i;
        if (!is_alpha(peek(ps)) || at_vs(ps))
            return true;
        int m = -1;
        for (int k = 0; k < (int)(sizeof(keywords) / sizeof(keywords[0])); k++)
            if (looking_at(ps, keywords[k])) {
                m = k;
                ps->i += (int)strlen(keywords[k]);
                break;
            }
        if (m < 0) {
            int j = ps->i;
            while (is_alpha(at(ps, j))) j++;
            return fail(ps, start, j - start, "'%.*s' isn't a modifier (see notation)", j - start,
                        ps->s + start);
        }
        if (m == M_DD)
            return fail(ps, start, 2, "more dice need a + or -: write '+ dd66'");

        long n = 0;
        bool has = is_digit(peek(ps));
        if (has && !read_number(ps, &n))
            return false;
        int len = ps->i - start;
        if (n > 32767)
            return fail(ps, start, len, "number too big for a modifier");

        bool step = m == M_IRG || m == M_IR || m == M_IE || m == M_I || m == M_RG ||
                    m == M_R || m == M_E;
        if (step && p->digits)
            return fail(ps, start, len, "digit dice can't explode, implode or reroll");
        if ((step || m == M_ADV || m == M_DIS) && p->percentile)
            return fail(ps, start, len, "d%% takes b and p (bonus, penalty) but can't explode, reroll or take advantage");
        qd_test_t t;
        bool ok = true;
        switch (m) {
        case M_ADV: case M_DIS:
            if (!has) n = 1;
            if (n > QD_MAX_ROLL_DICE)
                return fail(ps, start, len, "too much advantage");
            /* adv2 dis1 is adv1: they cancel */
            qd_pool_advantage(p, p->advantage + (m == M_ADV ? (int)n : -(int)n));
            break;
        case M_IE: case M_E:
            t = has ? qd_test_ge((int)n) : qd_test_max();
            ok = qd_pool_explode(p, t, m == M_IE);
            break;
        case M_I:
            t = has ? qd_test_le((int)n) : qd_test_min();
            ok = qd_pool_implode(p, t);
            break;
        case M_IRG: case M_RG:
            t = has ? qd_test_ge((int)n) : qd_test_max();
            ok = qd_pool_reroll(p, t, m == M_IRG);
            break;
        case M_IR: case M_R:
            t = has ? qd_test_le((int)n) : qd_test_min();
            ok = qd_pool_reroll(p, t, m == M_IR);
            break;
        case M_K: case M_KL: case M_KM: case M_D: case M_DH: {
            static const qd_keep_t how[] = { [M_K] = QD_KEEP_HIGH, [M_KL] = QD_KEEP_LOW,
                [M_KM] = QD_KEEP_MIDDLE, [M_D] = QD_DROP_LOW, [M_DH] = QD_DROP_HIGH };
            if (p->keep != QD_KEEP_ALL)
                return fail(ps, start, len, "only one keep or drop per dice");
            qd_pool_keep(p, how[m], (uint16_t)(has ? n : 1));
            break;
        }
        case M_T: case M_TL:
            if (!has)
                return fail(ps, start, len, "%s needs a number: %s8", keywords[m], keywords[m]);
            qd_pool_success(p, m == M_T ? qd_test_ge((int)n) : qd_test_le((int)n));
            break;
        case M_DS:
            if (p->success.op == QD_TEST_NONE)
                return fail(ps, start, len, "ds goes after t or tl: t7ds10");
            if (!has)
                t = p->success;
            else
                t = p->success.op == QD_TEST_LE ? qd_test_le((int)n) : qd_test_ge((int)n);
            qd_pool_twice(p, t);
            break;
        case M_F:
            qd_pool_failure(p, has ? qd_test_le((int)n) : qd_test_min());
            break;
        case M_B: case M_P:
            if (p->percentile) {                        /* d% b2: bonus dice, d% p1: penalty */
                int b = p->pct_bonus + (m == M_B ? 1 : -1) * (int)(has ? n : 1);
                if (b > QD_MAX_BONUS_DICE || b < -QD_MAX_BONUS_DICE)
                    return fail(ps, start, len, "at most %d bonus or penalty dice", QD_MAX_BONUS_DICE);
                p->pct_bonus = (int8_t)b;
            } else if (m == M_P) {
                return fail(ps, start, len, "p (penalty dice) is for d%%: d%% p1");
            } else {
                qd_pool_botch(p, has ? qd_test_le((int)n) : qd_test_min());
            }
            break;
        case M_SETS:
            if (has)
                return fail(ps, start, len, "sets takes no number");
            p->sets = true;
            break;
        }
        if (!ok)
            return fail(ps, start, len, "at most %d explode, implode and reroll steps per dice",
                        QD_MAX_POOL_STEPS);
    }
}

/* Dice, from the 'd' on. `start` is where the dice began (the count or a
 * group name), for error messages. */
static bool dice(ps_t *ps, int start, long count, const char *name)
{
    qd_roll_t *r = &ps->out->roll;
    if (count < 1)
        return fail(ps, start, ps->i - start, "at least 1 die");
    if (count > QD_MAX_ROLL_DICE)
        return fail(ps, start, ps->i - start, "at most %d dice in a roll", QD_MAX_ROLL_DICE);
    qd_pool_t *p = qd_roll_new_pool(r);
    if (!p)
        return fail(ps, start, 1, "at most %d sets of dice in one roll", QD_MAX_POOLS);
    int pool = (int)(p - r->pools);
    p->count = (uint16_t)count;
    if (name)
        snprintf(p->name, sizeof(p->name), "%s", name);
    ps->out->die_names[pool][0] = '\0';
    ps->i++;                                            /* the 'd' */

    char c = lower(peek(ps));
    if (c == 'd') {                                     /* digit dice */
        ps->i++;
        int n = 0;
        while (is_digit(peek(ps))) {
            if (peek(ps) == '0')
                return fail(ps, ps->i, 1, "digit dice have 1 to 9 sides, not 0");
            if (n == QD_MAX_DIGITS)
                return fail(ps, ps->i, 1, "at most %d digits", QD_MAX_DIGITS);
            p->digit_sides[n++] = (uint8_t)(peek(ps) - '0');
            ps->i++;
        }
        if (!n)
            return fail(ps, ps->i, 1, "dd needs a digit for each die: dd66");
        p->digits = (uint8_t)n;
    } else if (c == '%') {
        ps->i++;
        p->sides = 100;
        p->percentile = true;                           /* a units d10 and a tens d10 */
    } else if (c == 'f') {
        ps->i++;
        p->die = fudge_die();
        p->sides = 6;
    } else if (c == '{') {
        int open = ps->i, close = open + 1;
        while (close < ps->end && ps->s[close] != '}') close++;
        if (close >= ps->end)
            return fail(ps, open, 1, "missing '}' after the die's name");
        int a = open + 1, b = close;
        while (a < b && ps->s[a] == ' ') a++;
        while (b > a && ps->s[b - 1] == ' ') b--;
        if (b - a < 1 || b - a > QD_MAX_DIE_NAME)
            return fail(ps, open, close - open + 1, "a die's name is 1 to %d characters", QD_MAX_DIE_NAME);
        char *dn = ps->out->die_names[pool];
        memcpy(dn, ps->s + a, (size_t)(b - a));
        dn[b - a] = '\0';
        const qd_die_t *die = lookup_fn ? lookup_fn(dn, lookup_ctx) : NULL;
        if (!die)
            return fail(ps, a, b - a, "no custom die called '%s'", dn);
        p->die = die;
        p->sides = die->sides;
        ps->i = close + 1;
    } else if (is_digit(c)) {
        int s = ps->i;
        long sides;
        if (!read_number(ps, &sides))
            return false;
        if (sides < 1)
            return fail(ps, s, ps->i - s, "a die needs at least 1 side");
        if (sides > 32767)
            return fail(ps, s, ps->i - s, "at most 32767 sides");
        p->sides = (uint16_t)sides;
    } else {
        return fail(ps, ps->i, 1, "what kind of die? d6, d%%, dF, dd66 or d{name}");
    }
    if (!push(ps, QD_OP_POOL, pool, start))
        return false;
    return modifiers(ps, p);
}

/* ========================================================================
 * Sums
 * ======================================================================== */
static bool sum(ps_t *ps);

static bool primary(ps_t *ps)
{
    skip_space(ps);
    int start = ps->i;
    char c = peek(ps);

    if (c == '(') {
        if (ps->depth >= QD_MAX_NESTING)
            return fail(ps, start, 1, "brackets nested too deep (most %d)", QD_MAX_NESTING);
        ps->i++;
        ps->depth++;
        if (!sum(ps))
            return false;
        skip_space(ps);
        if (peek(ps) != ')')
            return fail(ps, ps->i, 1, "missing ')' for the '(' at column %d", start + 1);
        ps->i++;
        ps->depth--;
        return true;
    }

    /* A group name: letters (and digits, _) right before a ':'. */
    char name[QD_MAX_NAME + 1];
    bool named = false;
    if (is_alpha(c)) {
        int j = ps->i;
        while (is_alpha(at(ps, j)) || is_digit(at(ps, j)) || at(ps, j) == '_') j++;
        if (at(ps, j) == ':') {
            if (j - ps->i > QD_MAX_NAME)
                return fail(ps, start, j - start, "a group name is at most %d characters", QD_MAX_NAME);
            memcpy(name, ps->s + ps->i, (size_t)(j - ps->i));
            name[j - ps->i] = '\0';
            named = true;
            ps->i = j + 1;
            skip_space(ps);
            c = peek(ps);
        }
    }

    if (is_digit(c) || lower(c) == 'd') {
        long n = 1;
        if (is_digit(c) && !read_number(ps, &n))
            return false;
        if (lower(peek(ps)) == 'd')
            return dice(ps, start, n, named ? name : NULL);
        if (named)
            return fail(ps, start, ps->i - start, "a group name goes before dice: base:4d6");
        return push(ps, QD_OP_NUMBER, (int32_t)n, start);
    }
    if (named)
        return fail(ps, ps->i, 1, "a group name goes before dice: base:4d6");
    if (ps->i == ps->sw_x)
        return fail(ps, ps->i, 1, "x (an Extra) goes after the trait die or after the maths");
    if (!c)
        return fail(ps, ps->i, 1, "the roll stops too soon: a number, dice or '(' should come next");
    return fail(ps, ps->i, 1, "expected a number, dice or '(' instead of '%c'", c);
}

static bool unary(ps_t *ps)
{
    skip_space(ps);
    int start = ps->i;
    if (peek(ps) == '-') {
        ps->i++;
        return unary(ps) && push(ps, QD_OP_NEG, 0, start);
    }
    if (peek(ps) == '+') {
        ps->i++;
        return unary(ps);
    }
    return primary(ps);
}

/* The "* x / y ..." that follows a first value already pushed. */
static bool product_rest(ps_t *ps)
{
    for (;;) {
        skip_space(ps);
        char c = peek(ps);
        if (c != '*' && c != '/')
            return true;
        int op = ps->i++;
        if (!unary(ps))
            return false;
        const qd_roll_t *r = &ps->out->roll;
        if (c == '/' && r->ops[r->op_count - 1].kind == QD_OP_NUMBER && r->ops[r->op_count - 1].arg == 0)
            return fail(ps, op, ps->i - op, "can't divide by zero");
        if (!push(ps, c == '*' ? QD_OP_MUL : QD_OP_DIV, 0, op))
            return false;
    }
}

static bool product(ps_t *ps)
{
    return unary(ps) && product_rest(ps);
}

/* The "+ x - y ..." that follows a first product already pushed. */
static bool sum_rest(ps_t *ps)
{
    for (;;) {
        skip_space(ps);
        char c = peek(ps);
        if (c != '+' && c != '-')
            return true;
        int op = ps->i++;
        if (!product(ps))
            return false;
        if (!push(ps, c == '+' ? QD_OP_ADD : QD_OP_SUB, 0, op))
            return false;
    }
}

static bool sum(ps_t *ps)
{
    return product(ps) && sum_rest(ps);
}

/* ========================================================================
 * Game-system shortcuts: atk, chk, save, sw8, coc 65, fate, vtm 7 h2, dcc d20
 * ======================================================================== */

/* Is the word `w` here, whole (not followed by a letter or ':')? */
static bool word_here(const ps_t *ps, const char *w)
{
    int n = (int)strlen(w);
    char after = at(ps, ps->i + n);
    return looking_at(ps, w) && !is_alpha(after) && after != ':' && after != '_';
}

/* An optional number straight after a letter: "b2", "h3". */
static bool small_number(ps_t *ps, long dflt, long *v)
{
    if (!is_digit(peek(ps))) { *v = dflt; return true; }
    return read_number(ps, v);
}

static bool sys_dnd(ps_t *ps, int kind, int start)
{
    int adv = 0;
    for (;;) {
        skip_space(ps);
        if (word_here(ps, "adv"))      { ps->i += 3; adv++; continue; }
        if (word_here(ps, "dis"))      { ps->i += 3; adv--; continue; }
        /* "chk d20" is allowed (the d20 is already there); another die isn't */
        int j = ps->i;
        if (at(ps, j) == '1' && lower(at(ps, j + 1)) == 'd') j++;
        if (lower(at(ps, j)) == 'd' && is_digit(at(ps, j + 1))) {
            int k = j + 1;
            while (is_digit(at(ps, k))) k++;
            if (k - j == 3 && at(ps, j + 1) == '2' && at(ps, j + 2) == '0' && !is_alpha(at(ps, k))) {
                ps->i = k;
                continue;
            }
            return fail(ps, ps->i, k - ps->i, "atk/chk/save use a d20; else write 1%.*s vs 5",
                        k - j, ps->s + j);
        }
        break;
    }
    if (!qd_sys_dnd(&ps->out->roll, kind, adv))
        return fail(ps, start, ps->i - start, "too long");
    return true;
}

static bool sys_sw(ps_t *ps, int start)
{
    long sides;
    if (!read_number(ps, &sides))
        return fail(ps, ps->i, 1, "sw needs the trait die: sw4, sw6, sw8, sw10 or sw12");
    bool extra = false;
    skip_space(ps);
    if (lower(peek(ps)) == 'x' && !is_alpha(at(ps, ps->i + 1)) && !is_digit(at(ps, ps->i + 1))) {
        ps->i++;
        extra = true;
    } else {
        /* "sw4-2 x": the x may also come after the maths, before vs / [ / ! */
        for (int j = ps->i; j < ps->end && ps->s[j] != '!' && ps->s[j] != '['; j++)
            if (lower(ps->s[j]) == 'x' && !is_alpha(at(ps, j - 1)) &&
                !is_alpha(at(ps, j + 1)) && !is_digit(at(ps, j + 1))) {
                ps->sw_x = j;
                extra = true;
                break;
            }
    }
    if (!qd_sys_sw(&ps->out->roll, (int)sides, !extra))
        return fail(ps, start, ps->i - start, "the trait die is d4, d6, d8, d10 or d12: sw4 ... sw12 (add +1 for d12+1)");
    return true;
}

static bool sys_coc(ps_t *ps, int start)
{
    long skill;
    skip_space(ps);
    if (!read_number(ps, &skill))
        return fail(ps, ps->i, 1, "coc needs the skill: coc 65");
    int bonus = 0;
    for (;;) {
        skip_space(ps);
        int at0 = ps->i;
        char c = lower(peek(ps));
        if ((c == 'b' || c == 'p') && (is_digit(at(ps, ps->i + 1)) || !is_alpha(at(ps, ps->i + 1)))) {
            ps->i++;
            long n;
            if (!small_number(ps, 1, &n))
                return false;
            bonus += c == 'b' ? (int)n : -(int)n;
            if (bonus > QD_MAX_BONUS_DICE || bonus < -QD_MAX_BONUS_DICE)
                return fail(ps, at0, ps->i - at0, "at most %d bonus or penalty dice", QD_MAX_BONUS_DICE);
        } else {
            break;
        }
    }
    if (!qd_sys_coc(&ps->out->roll, (int)skill, bonus))
        return fail(ps, start, ps->i - start, "the skill is 0 to 999");
    return true;
}

static bool sys_vtm(ps_t *ps, int start)
{
    long pool, hunger = 0;
    skip_space(ps);
    if (!read_number(ps, &pool))
        return fail(ps, ps->i, 1, "vtm needs the dice pool: vtm 7 h2");
    skip_space(ps);
    if (lower(peek(ps)) == 'h' && !is_alpha(at(ps, ps->i + 1))) {
        int h0 = ps->i++;
        if (!small_number(ps, 1, &hunger))
            return false;
        if (hunger > 5)
            return fail(ps, h0, ps->i - h0, "hunger is 0 to 5");
    }
    if (pool < 1 || pool > 50)
        return fail(ps, start, ps->i - start, "the pool is 1 to 50 dice");
    qd_sys_vtm(&ps->out->roll, (int)pool, (int)hunger);
    return true;
}

static bool sys_dcc(ps_t *ps, int start)
{
    skip_space(ps);
    long count = 1, sides;
    int d0 = ps->i;
    if (is_digit(peek(ps)) && !read_number(ps, &count))
        return false;
    if (lower(peek(ps)) != 'd' || !is_digit(at(ps, ps->i + 1)))
        return fail(ps, ps->i, 1, "dcc needs a die: dcc d20");
    ps->i++;
    if (!read_number(ps, &sides))
        return false;
    if (!qd_dcc_chain((int)sides, 0))
        return fail(ps, d0, ps->i - d0, "not on the dice chain (d3 d4 d5 d6 d7 d8 d10 d12 d14 d16 d20 d24 d30)");
    /* Chain steps: "+1d", "-2d" - a sign, a number and a d with no sides after it. */
    int steps = 0;
    for (;;) {
        skip_space(ps);
        char sign = peek(ps);
        if (sign != '+' && sign != '-') break;
        int j = ps->i + 1;
        while (at(ps, j) == ' ') j++;
        int k = j;
        while (is_digit(at(ps, k))) k++;
        char after = at(ps, k + 1);
        if (lower(at(ps, k)) != 'd' || is_digit(after) || after == '%' || after == '{' ||
            lower(after) == 'f' || lower(after) == 'd')
            break;                                      /* "+2" or "+1d4": maths, not a step */
        long n = k == j ? 1 : 0;                        /* "+d" is one step */
        for (int q = j; q < k; q++) n = n * 10 + (ps->s[q] - '0');
        steps += sign == '+' ? (int)n : -(int)n;
        ps->i = k + 1;
    }
    if (!qd_sys_dcc(&ps->out->roll, (int)count, (int)sides, steps))
        return fail(ps, start, ps->i - start, "can't build that dcc roll");
    return true;
}

/* A shortcut at the start of a roll? `*found` says whether one was read. */
static bool try_system(ps_t *ps, bool *found)
{
    int start = ps->i;
    *found = true;
    if (word_here(ps, "atk"))   { ps->i += 3; return sys_dnd(ps, QD_DND_ATTACK, start); }
    if (word_here(ps, "chk"))   { ps->i += 3; return sys_dnd(ps, QD_DND_CHECK, start); }
    if (word_here(ps, "check")) { ps->i += 5; return sys_dnd(ps, QD_DND_CHECK, start); }
    if (word_here(ps, "save"))  { ps->i += 4; return sys_dnd(ps, QD_DND_SAVE, start); }
    if (word_here(ps, "sav"))   { ps->i += 3; return sys_dnd(ps, QD_DND_SAVE, start); }
    if (word_here(ps, "fate"))  {
        ps->i += 4;
        if (!qd_sys_fate(&ps->out->roll)) return fail(ps, start, 4, "too long");
        return true;
    }
    if (word_here(ps, "coc"))   { ps->i += 3; return sys_coc(ps, start); }
    if (word_here(ps, "vtm"))   { ps->i += 3; return sys_vtm(ps, start); }
    if (word_here(ps, "dcc"))   { ps->i += 3; return sys_dcc(ps, start); }
    if (looking_at(ps, "sw") && is_digit(at(ps, ps->i + 2))) { ps->i += 2; return sys_sw(ps, start); }
    *found = false;
    return true;
}

/* "vs 15", "vs -2", or for Call of Cthulhu "vs hard". */
static bool try_vs(ps_t *ps)
{
    if (!at_vs(ps))
        return true;
    int start = ps->i;
    ps->i += vs_len(ps);
    skip_space(ps);
    qd_roll_t *r = &ps->out->roll;
    if (r->sys.id == QD_SYS_COC) {
        static const char *const levels[] = { "regular", "hard", "extreme" };
        for (int k = 0; k < 3; k++)
            if (word_here(ps, levels[k])) {
                ps->i += (int)strlen(levels[k]);
                qd_roll_vs(r, QD_COC_REGULAR + k);
                return true;
            }
        return fail(ps, start, ps->i - start + 1, "for coc, vs is regular, hard or extreme");
    }
    bool neg = peek(ps) == '-';
    if (neg) ps->i++;
    long n;
    if (!is_digit(peek(ps)))
        return fail(ps, ps->i, 1, "vs needs a number: vs 15");
    if (!read_number(ps, &n))
        return false;
    qd_roll_vs(r, (int32_t)(neg ? -n : n));
    return true;
}

/* ========================================================================
 * Buckets, labels, comments, repeats
 * ======================================================================== */
static bool read_signed(ps_t *ps, long *v)
{
    bool neg = false;
    if (peek(ps) == '-' && is_digit(at(ps, ps->i + 1))) {
        neg = true;
        ps->i++;
    }
    if (!read_number(ps, v))
        return false;
    if (*v > 32767)
        return fail(ps, ps->i - 1, 1, "number too big for a bucket");
    if (neg) *v = -*v;
    return true;
}

static bool bucket(ps_t *ps)
{
    static const struct { const char *sym; qd_test_t (*make)(int); } cmp[] = {
        { "<=", qd_test_le }, { ">=", qd_test_ge }, { "!=", qd_test_ne },
        { "<", qd_test_lt }, { ">", qd_test_gt }, { "=", qd_test_eq },
    };
    skip_space(ps);
    int start = ps->i;
    int c = -1;
    for (int k = 0; k < 6 && c < 0; k++)
        if (looking_at(ps, cmp[k].sym)) {
            c = k;
            ps->i += (int)strlen(cmp[k].sym);
        }
    skip_space(ps);
    qd_test_t t;
    long a, b;
    char ch = peek(ps);
    if (is_digit(ch) || (ch == '-' && is_digit(at(ps, ps->i + 1)))) {
        if (!read_signed(ps, &a))
            return false;
        if (c < 0 && peek(ps) == '-') {                 /* 3-5 */
            ps->i++;
            if (!read_signed(ps, &b))
                return false;
            t = qd_test_range((int)a, (int)b);
        } else {
            t = c < 0 ? qd_test_eq((int)a) : cmp[c].make((int)a);
        }
    } else if (c == 5 || c < 0) {                       /* = or nothing: max, min, a label */
        char label[QD_MAX_LABEL + 1];
        int n = 0;
        if (looking_at(ps, "max") && !is_alpha(at(ps, ps->i + 3))) {
            ps->i += 3;
            t = qd_test_max();
        } else if (looking_at(ps, "min") && !is_alpha(at(ps, ps->i + 3))) {
            ps->i += 3;
            t = qd_test_min();
        } else {
            bool quoted = peek(ps) == '"';
            if (quoted) ps->i++;
            while (peek(ps) && (quoted ? peek(ps) != '"' : (peek(ps) != ',' && peek(ps) != ']' && peek(ps) != ' '))) {
                if (n == QD_MAX_LABEL)
                    return fail(ps, start, ps->i - start + 1, "a face label is at most %d characters", QD_MAX_LABEL);
                label[n++] = peek(ps);
                ps->i++;
            }
            if (quoted) {
                if (peek(ps) != '"')
                    return fail(ps, start, ps->i - start, "missing closing '\"'");
                ps->i++;
            }
            if (!n)
                return fail(ps, start, 1, "a bucket is a test like <3, =6, 3-5 or =+");
            label[n] = '\0';
            t = qd_test_label(label);
        }
    } else {
        return fail(ps, ps->i, 1, "a number should follow '%s'", cmp[c].sym);
    }
    if (!qd_roll_bucket(&ps->out->roll, t))
        return fail(ps, start, ps->i - start, "at most %d buckets", QD_MAX_BUCKETS);
    return true;
}

static bool buckets(ps_t *ps)
{
    int open = ps->i++;
    for (;;) {
        if (!bucket(ps))
            return false;
        skip_space(ps);
        if (peek(ps) == ',') { ps->i++; continue; }
        if (peek(ps) == ']') { ps->i++; return true; }
        if (!peek(ps))
            return fail(ps, open, 1, "missing ']' for these buckets");
        return fail(ps, ps->i, 1, "expected ',' or ']' in the buckets");
    }
}

/* Copy text[a..b) into dst, spaces trimmed. Returns false if it won't fit. */
static bool copy_trimmed(char *dst, int size, const char *text, int a, int b)
{
    while (a < b && (text[a] == ' ' || text[a] == '\t')) a++;
    while (b > a && (text[b - 1] == ' ' || text[b - 1] == '\t')) b--;
    if (b - a >= size)
        return false;
    memcpy(dst, text + a, (size_t)(b - a));
    dst[b - a] = '\0';
    return true;
}

/* "6 x" in front of a roll? Looks ahead without moving unless it is. */
static bool try_repeat(ps_t *ps, bool *done)
{
    *done = false;
    if (!is_digit(peek(ps)))
        return true;
    int j = ps->i;
    while (is_digit(at(ps, j))) j++;
    int k = j;
    while (at(ps, k) == ' ' || at(ps, k) == '\t') k++;
    if (lower(at(ps, k)) != 'x')
        return true;
    char after = at(ps, k + 1);
    if (is_alpha(after) && lower(after) != 'd')
        return true;
    int start = ps->i;
    long n;
    if (!read_number(ps, &n))
        return false;
    if (n < 1 || n > QD_MAX_MASS)
        return fail(ps, start, j - start, "repeat 1 to %d times", QD_MAX_MASS);
    if (ps->out->repeat != 1)
        return fail(ps, start, k + 1 - start, "only one repeat per roll");
    ps->out->repeat = (uint16_t)n;
    ps->i = k + 1;
    *done = true;
    return true;
}

static bool try_label(ps_t *ps, bool *done)
{
    *done = false;
    if (peek(ps) != '(')
        return true;
    /* "(Bite)" is a label; "(d6+1)" and "(base:2d6)" are maths. */
    int j = ps->i + 1;
    while (at(ps, j) == ' ') j++;
    if (!is_alpha(at(ps, j)))
        return true;
    char d1 = lower(at(ps, j + 1));
    if (lower(at(ps, j)) == 'd' &&
        (is_digit(d1) || d1 == '%' || d1 == '{' || (d1 == 'f' && !is_alpha(at(ps, j + 2))) ||
         (d1 == 'd' && is_digit(at(ps, j + 2)))))
        return true;
    int k = j;
    while (is_alpha(at(ps, k)) || is_digit(at(ps, k)) || at(ps, k) == '_') k++;
    if (at(ps, k) == ':')
        return true;
    int close = j;
    while (close < ps->end && ps->s[close] != ')') close++;
    if (close >= ps->end)
        return fail(ps, ps->i, 1, "missing ')' after the label");
    if (ps->out->label[0])
        return fail(ps, ps->i, close - ps->i + 1, "only one label per roll");
    if (!copy_trimmed(ps->out->label, sizeof(ps->out->label), ps->s, ps->i + 1, close))
        return fail(ps, ps->i, close - ps->i + 1, "a label is at most %d characters", QD_MAX_ROLL_LABEL);
    ps->i = close + 1;
    *done = true;
    return true;
}

/* Everything inside a roll (or inside a set's brackets), up to ps->end. */
static bool body(ps_t *ps)
{
    qd_parsed_t *out = ps->out;
    int start = ps->i;

    for (;;) {                                          /* repeat and label, either order */
        bool did_repeat, did_label;
        skip_space(ps);
        if (!try_repeat(ps, &did_repeat))
            return false;
        skip_space(ps);
        if (!try_label(ps, &did_label))
            return false;
        if (!did_repeat && !did_label)
            break;
    }
    skip_space(ps);
    if (!peek(ps))
        return fail(ps, ps->i, 1, "nothing to roll");
    bool sys;
    if (!try_system(ps, &sys))
        return false;
    if (sys) {
        /* After a shortcut: maths, for systems where the total is what counts. */
        uint8_t id = out->roll.sys.id;
        skip_space(ps);
        char c = peek(ps);
        if ((id == QD_SYS_COC || id == QD_SYS_VTM) && c && strchr("+-*/", c))
            return fail(ps, ps->i, 1, "no maths after %s: the %s is the number that counts",
                        id == QD_SYS_COC ? "coc" : "vtm", id == QD_SYS_COC ? "skill" : "pool");
        if (!product_rest(ps) || !sum_rest(ps))
            return false;
        if (ps->sw_x >= 0) {                            /* "sw4-2 x" */
            skip_space(ps);
            if (ps->i != ps->sw_x)
                return fail(ps, ps->sw_x, 1, "x (an Extra) goes after the trait die or after the maths");
            ps->i++;
        }
    } else if (!sum(ps)) {
        return false;
    }
    skip_space(ps);
    if (!try_vs(ps))
        return false;
    skip_space(ps);
    if (peek(ps) == '[' && !buckets(ps))
        return false;
    skip_space(ps);
    if (peek(ps) == '!') {
        int a = ps->i + 1, b = ps->end;
        while (a < b && ps->s[a] == ' ') a++;
        while (b > a && ps->s[b - 1] == ' ') b--;
        if (b - a > QD_MAX_COMMENT) b = a + QD_MAX_COMMENT;     /* long comments are cut */
        copy_trimmed(out->comment, sizeof(out->comment), ps->s, a, b);
        ps->i = ps->end;
    }
    skip_space(ps);
    char c = peek(ps);
    if (c) {
        int k = ps->i - 1;
        while (k >= start && ps->s[k] == ' ') k--;
        if (lower(c) == 'd' && k >= start && is_digit(ps->s[k]) && k < ps->i - 1)
        {
            /* quote what was typed: "4 d7" -> "write 4d7" */
            int n0 = k;
            while (n0 > start && is_digit(ps->s[n0 - 1])) n0--;
            int d1 = ps->i + 1;
            while (d1 < ps->end && (is_digit(ps->s[d1]) || ps->s[d1] == '%' || lower(ps->s[d1]) == 'f')) d1++;
            return fail(ps, ps->i, 1, "no space inside dice: write %.*s%.*s", k + 1 - n0, ps->s + n0,
                        d1 - ps->i, ps->s + ps->i);
        }
        if (c == ')')
            return fail(ps, ps->i, 1, "a ')' without its '('");
        /* "400 atk vs 12": a number then a roll - the x is missing */
        int d0 = start;
        while (d0 < ps->end && ps->s[d0] == ' ') d0++;
        int d1 = d0;
        while (d1 < ps->end && is_digit(ps->s[d1])) d1++;
        if (d1 > d0 && (is_alpha(c) || c == '(')) {
            bool only_number = true;
            for (int q = d1; q < ps->i; q++) only_number &= ps->s[q] == ' ';
            if (only_number)
                return fail(ps, ps->i, 1, "to roll it %.*s times, write %.*s x ...", d1 - d0, ps->s + d0,
                            d1 - d0, ps->s + d0);
        }
        return fail(ps, ps->i, 1, "expected + - * / vs [ ! or the end, not '%c'", c);
    }
    if (!out->roll.pool_count)
        return fail(ps, start, ps->end - start, "no dice in this roll");
    qd_err_t e = qd_roll_check(&out->roll);
    if (e)
        return fail(ps, start, ps->end - start, "%s", qd_err_text(e));
    return true;
}

/* "(6 x ..." here? Then *close is its matching ')'. */
static bool set_here(ps_t *ps, int *close)
{
    if (peek(ps) != '(')
        return false;
    int j = ps->i + 1;
    while (at(ps, j) == ' ') j++;
    if (at(ps, j) == '(') {                             /* "((Fire) 3 x 8d6) d1": a label first */
        int k = j + 1;
        while (at(ps, k) == ' ') k++;
        if (!is_alpha(at(ps, k)))
            return false;
        while (k < ps->end && ps->s[k] != ')') k++;
        j = k + 1;
        while (at(ps, j) == ' ') j++;
    }
    if (!is_digit(at(ps, j)))
        return false;
    while (is_digit(at(ps, j))) j++;
    while (at(ps, j) == ' ') j++;
    if (lower(at(ps, j)) != 'x' || (is_alpha(at(ps, j + 1)) && lower(at(ps, j + 1)) != 'd'))
        return false;
    int depth = 0;
    for (int k = ps->i; k < ps->end; k++) {
        if (ps->s[k] == '(') depth++;
        if (ps->s[k] == ')' && --depth == 0) {
            *close = k;
            return true;
        }
    }
    *close = -1;
    return true;
}

static bool roll(ps_t *ps)
{
    qd_parsed_t *out = ps->out;
    memset(out, 0, sizeof(*out));
    qd_roll_init(&out->roll);
    out->repeat = 1;

    bool labelled;
    skip_space(ps);
    if (!try_label(ps, &labelled))
        return false;
    skip_space(ps);
    int open = ps->i, close;
    if (!set_here(ps, &close))
        return body(ps);

    /* A set: "(6 x 4d6) k3". Read the inside as a roll of its own. */
    if (close < 0)
        return fail(ps, open, 1, "missing ')' for this set");
    int end = ps->end;
    ps->end = close;
    ps->i = open + 1;
    if (!body(ps))
        return false;
    ps->end = end;
    ps->i = close + 1;
    out->set = true;
    if (out->repeat > QD_MAX_REPEAT)
        return fail(ps, open, close - open + 1, "a set has at most %d rolls", QD_MAX_REPEAT);

    skip_space(ps);
    static const char *const words[] = { "kl", "km", "k", "dh", "d" };
    static const qd_keep_t how[] = { QD_KEEP_LOW, QD_KEEP_MIDDLE, QD_KEEP_HIGH, QD_DROP_HIGH, QD_DROP_LOW };
    if (is_alpha(peek(ps))) {
        int at0 = ps->i, w = -1;
        for (int k = 0; k < 5 && w < 0; k++)
            if (looking_at(ps, words[k]) && !is_alpha(at(ps, ps->i + (int)strlen(words[k])))) w = k;
        if (w < 0)
            return fail(ps, at0, 1, "after a set: k, kl, km, d or dh (keep or drop whole rolls)");
        ps->i += (int)strlen(words[w]);
        long n = 1;
        if (is_digit(peek(ps)) && !read_number(ps, &n))
            return false;
        if (n > out->repeat)
            return fail(ps, at0, ps->i - at0, "the set has only %u rolls", out->repeat);
        out->set_keep = (uint8_t)how[w];
        out->set_keep_n = (uint16_t)n;
    }
    skip_space(ps);
    if (peek(ps) == '!') {
        int a = ps->i + 1, b = ps->end;
        if (b - a > QD_MAX_COMMENT + 8) b = a + QD_MAX_COMMENT + 8;
        while (a < b && ps->s[a] == ' ') a++;
        if (b - a > QD_MAX_COMMENT) b = a + QD_MAX_COMMENT;
        copy_trimmed(out->comment, sizeof(out->comment), ps->s, a, b);
        ps->i = ps->end;
    }
    skip_space(ps);
    if (peek(ps))
        return fail(ps, ps->i, 1, "after a set: k, kl, km, d, dh, a comment or the end");
    return true;
}

qd_err_t qd_set_run(const qd_parsed_t *p, qd_set_result_t *out, qd_set_each_t each, void *ctx)
{
    static qd_result_t res;
    memset(out, 0, sizeof(*out));
    int n = p->repeat < 1 ? 1 : p->repeat > QD_MAX_REPEAT ? QD_MAX_REPEAT : p->repeat;
    for (int k = 0; k < n; k++) {
        qd_err_t e = qd_roll_run(&p->roll, &res);
        if (e)
            return e;
        out->counting = (res.flags & QD_RES_COUNTING) != 0;
        out->value[k] = out->counting ? res.successes : res.total;
        out->kept[k] = true;
        if (each)
            each(k, &res, ctx);
    }
    out->count = (uint8_t)n;

    /* Keep or drop whole rolls: sort by value (stable), mark the ends. */
    if (p->set && p->set_keep != QD_KEEP_ALL) {
        int order[QD_MAX_REPEAT];
        for (int k = 0; k < n; k++) {
            int j = k;
            while (j > 0 && out->value[order[j - 1]] > out->value[k]) { order[j] = order[j - 1]; j--; }
            order[j] = k;
        }
        int m = p->set_keep_n > n ? n : p->set_keep_n, lo = 0, hi = 0;
        switch (p->set_keep) {
        case QD_KEEP_HIGH:   lo = n - m; break;
        case QD_KEEP_LOW:    hi = n - m; break;
        case QD_KEEP_MIDDLE: lo = (n - m) / 2; hi = n - m - lo; break;
        case QD_DROP_LOW:    lo = m; break;
        case QD_DROP_HIGH:   hi = m; break;
        }
        for (int k = 0; k < lo; k++) out->kept[order[k]] = false;
        for (int k = 0; k < hi; k++) out->kept[order[n - 1 - k]] = false;
    }
    for (int k = 0; k < n; k++)
        if (out->kept[k]) out->total += out->value[k];
    return QD_OK;
}

qd_err_t qd_mass_run(const qd_parsed_t *p, qd_mass_result_t *out)
{
    static qd_result_t res;
    memset(out, 0, sizeof(*out));
    int n = p->repeat < 1 ? 1 : p->repeat > QD_MAX_MASS ? QD_MAX_MASS : p->repeat;
    for (int k = 0; k < n; k++) {
        qd_err_t e = qd_roll_run(&p->roll, &res);
        if (e)
            return e;
        out->counting = (res.flags & QD_RES_COUNTING) != 0;
        int32_t v = out->counting ? res.successes : res.total;
        out->sum += v;
        if (!k || v < out->lo) out->lo = v;
        if (!k || v > out->hi) out->hi = v;
        qd_outcome_t o;
        if (qd_sys_outcome(&p->roll, &res, &o)) {
            out->judged = true;
            out->passes += o.pass;
            int l = o.level < -4 ? -4 : o.level > 4 ? 4 : o.level;
            out->level[l + 4]++;
        }
    }
    out->count = (uint16_t)n;
    return QD_OK;
}

int qd_parse_line(const char *text, qd_parsed_t *out, int max, qd_parse_error_t *err)
{
    qd_parse_error_t dummy;
    ps_t ps = { text ? text : "", 0, 0, NULL, err ? err : &dummy, false, 0, -1 };
    memset(ps.err, 0, sizeof(*ps.err));
    int len = (int)strlen(ps.s), count = 0;
    for (;;) {
        int semi = ps.i;
        while (semi < len && ps.s[semi] != ';') semi++;
        if (count == max) {
            fail(&ps, ps.i - 1, 1, max == 1 ? "one roll only here" : "at most %d rolls on one line", max);
            return -1;
        }
        ps.end = semi;
        ps.out = &out[count];
        ps.depth = 0;
        ps.sw_x = -1;
        if (!roll(&ps))
            return -1;
        count++;
        if (semi >= len)
            return count;
        ps.i = semi + 1;
    }
}

bool qd_parse(const char *text, qd_parsed_t *out, qd_parse_error_t *err)
{
    return qd_parse_line(text, out, 1, err) == 1;
}

/* ========================================================================
 * Writing notation back out
 * ======================================================================== */
typedef struct {
    char *buf;
    int size, len;
} out_t;

static void put(out_t *o, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int room = o->size - o->len;
    int n = vsnprintf(o->buf + (room > 0 ? o->len : 0), room > 0 ? (size_t)room : 0, fmt, ap);
    va_end(ap);
    if (n > 0) o->len += n;
    if (o->size > 0 && o->len >= o->size) o->buf[o->size - 1] = '\0';
}

/* A modifier's number, or nothing when it is the default (max / min). */
static void put_test(out_t *o, const char *word, const qd_test_t *t)
{
    if (t->op == QD_TEST_MAX || t->op == QD_TEST_MIN)
        put(o, " %s", word);
    else
        put(o, " %s%d", word, t->a);
}

static void put_pool(out_t *o, const qd_parsed_t *ps, int k)
{
    const qd_pool_t *p = &ps->roll.pools[k];
    if (p->name[0]) put(o, "%s:", p->name);
    put(o, "%ud", p->count);
    if (p->digits) {
        put(o, "d");
        for (int d = 0; d < p->digits; d++) put(o, "%u", p->digit_sides[d]);
    } else if (p->percentile) {
        put(o, "%%");
        if (p->pct_bonus > 0) put(o, " b%d", p->pct_bonus);
        if (p->pct_bonus < 0) put(o, " p%d", -p->pct_bonus);
    } else if (p->die == fudge_die()) {
        put(o, "F");
    } else if (p->die) {
        put(o, "{%s}", ps->die_names[k][0] ? ps->die_names[k] : "?");
    } else {
        put(o, "%u", p->sides);
    }
    if (p->advantage > 0) put(o, " adv%d", p->advantage);
    if (p->advantage < 0) put(o, " dis%d", -p->advantage);
    for (int s = 0; s < p->step_count; s++) {
        const qd_step_t *st = &p->steps[s];
        bool up = st->test.op == QD_TEST_GE || st->test.op == QD_TEST_MAX;
        if (st->kind == QD_STEP_EXPLODE)      put_test(o, st->forever ? "ie" : "e", &st->test);
        else if (st->kind == QD_STEP_IMPLODE) put_test(o, "i", &st->test);
        else if (up)                          put_test(o, st->forever ? "irg" : "rg", &st->test);
        else                                  put_test(o, st->forever ? "ir" : "r", &st->test);
    }
    static const char *const keep[] = { "", "k", "kl", "km", "d", "dh" };
    if (p->keep != QD_KEEP_ALL) put(o, " %s%u", keep[p->keep], p->keep_n);
    if (p->success.op != QD_TEST_NONE) {
        put(o, " %s%d", p->success.op == QD_TEST_LE ? "tl" : "t", p->success.a);
        if (p->twice.op != QD_TEST_NONE) {
            if (p->twice.op == p->success.op && p->twice.a == p->success.a) put(o, " ds");
            else put(o, " ds%d", p->twice.a);
        }
    }
    if (p->failure.op != QD_TEST_NONE) put_test(o, "f", &p->failure);
    if (p->botch.op != QD_TEST_NONE)   put_test(o, "b", &p->botch);
    if (p->sets) put(o, " sets");
}

/* A system shortcut, written back as it would be typed. */
static void put_system(out_t *o, const qd_roll_t *r)
{
    const qd_system_t *s = &r->sys;
    switch (s->id) {
    case QD_SYS_DND:
        put(o, "%s", s->kind == QD_DND_ATTACK ? "atk" : s->kind == QD_DND_SAVE ? "save" : "chk");
        if (s->a > 0) put(o, " adv");
        if (s->a < 0) put(o, " dis");
        break;
    case QD_SYS_SW:   put(o, "sw%d%s", s->a, s->b ? "" : " x"); break;
    case QD_SYS_COC:
        put(o, "coc %d", s->a);
        if (s->b > 0) put(o, " b%d", s->b);
        if (s->b < 0) put(o, " p%d", -s->b);
        break;
    case QD_SYS_FATE: put(o, "fate"); break;
    case QD_SYS_VTM:
        put(o, "vtm %d", s->a);
        if (s->b) put(o, " h%d", s->b);
        break;
    case QD_SYS_DCC:
        put(o, "dcc %dd%d", s->kind, s->c);
        if (s->b) put(o, " %+dd", s->b);
        break;
    default: put(o, "?");
    }
}

int qd_format_pool(const qd_parsed_t *p, int pool, char *buf, int size)
{
    out_t o = { buf, size, 0 };
    if (size > 0) buf[0] = '\0';
    if (pool >= 0 && pool < p->roll.pool_count) {
        put_pool(&o, p, pool);
    }
    return o.len;
}

static int precedence(const qd_op_t *op)
{
    switch (op->kind) {
    case QD_OP_ADD: case QD_OP_SUB: return 1;
    case QD_OP_MUL: case QD_OP_DIV: return 2;
    default: return 3;                                  /* numbers, dice, negation */
    }
}

/* The sum is a stack program; turn it back into a tree (left and right
 * operand of each operator) and print that with as few brackets as the
 * usual precedence allows. */
static void put_op(out_t *o, const qd_parsed_t *ps, const int *left, const int *right, int k)
{
    const qd_op_t *op = &ps->roll.ops[k];
    if (ps->roll.sys.id && k == ps->roll.sys.ops - 1) {    /* the shortcut's own steps */
        put_system(o, &ps->roll);
        return;
    }
    switch (op->kind) {
    case QD_OP_POOL:   put_pool(o, ps, op->arg); return;
    case QD_OP_NUMBER: put(o, "%ld", (long)op->arg); return;
    case QD_OP_NEG: {
        bool br = precedence(&ps->roll.ops[left[k]]) < 3 || ps->roll.ops[left[k]].kind == QD_OP_NEG;
        put(o, br ? "-(" : "-");
        put_op(o, ps, left, right, left[k]);
        if (br) put(o, ")");
        return;
    }
    default: break;
    }
    int p = precedence(op);
    const qd_op_t *l = &ps->roll.ops[left[k]], *r = &ps->roll.ops[right[k]];
    bool lb = precedence(l) < p;
    bool rb = precedence(r) < p ||
              (precedence(r) == p && (op->kind == QD_OP_SUB || op->kind == QD_OP_DIV));
    static const char sym[] = { [QD_OP_ADD] = '+', [QD_OP_SUB] = '-', [QD_OP_MUL] = '*', [QD_OP_DIV] = '/',
                                [QD_OP_MAX] = '?' };
    if (lb) put(o, "(");
    put_op(o, ps, left, right, left[k]);
    put(o, lb ? ") %c " : " %c ", sym[op->kind]);
    if (rb) put(o, "(");
    put_op(o, ps, left, right, right[k]);
    if (rb) put(o, ")");
}

/* The sum, its target and its buckets. */
static void put_body(out_t *o_, const qd_parsed_t *p)
{
    out_t o = *o_;
    const qd_roll_t *r = &p->roll;
    int left[QD_MAX_ROLL_OPS], right[QD_MAX_ROLL_OPS], stack[QD_MAX_ROLL_OPS], top = 0;
    bool ok = r->op_count > 0;
    for (int k = 0; k < r->op_count && ok; k++) {
        switch (r->ops[k].kind) {
        case QD_OP_POOL: case QD_OP_NUMBER:
            stack[top++] = k;
            break;
        case QD_OP_NEG:
            ok = top >= 1;
            if (ok) { left[k] = stack[top - 1]; stack[top - 1] = k; }
            break;
        default:
            ok = top >= 2;
            if (ok) { right[k] = stack[--top]; left[k] = stack[top - 1]; stack[top - 1] = k; }
        }
    }
    if (ok && top == 1)
        put_op(&o, p, left, right, stack[0]);
    else
        put(&o, "?");
    if (r->sys.has_target) {
        if (r->sys.id == QD_SYS_COC) {
            static const char *const levels[] = { "regular", "hard", "extreme" };
            int t = r->sys.target;
            put(&o, " vs %s", levels[(t < 1 ? 1 : t > 3 ? 3 : t) - 1]);
        } else {
            put(&o, " vs %ld", (long)r->sys.target);
        }
    }

    if (r->bucket_count) {
        put(&o, " [");
        for (int b = 0; b < r->bucket_count; b++) {
            char t[16];
            const qd_test_t *bt = &r->buckets[b].test;
            if (bt->op == QD_TEST_LABEL) snprintf(t, sizeof(t), "=\"%s\"", bt->label);
            else qd_test_text(bt, t, sizeof(t));
            put(&o, b ? ", %s" : "%s", t);
        }
        put(&o, "]");
    }
    *o_ = o;
}

int qd_format(const qd_parsed_t *p, char *buf, int size)
{
    out_t o = { buf, size, 0 };
    if (size > 0) buf[0] = '\0';
    if (p->set) {
        static const char *const keep[] = { "", "k", "kl", "km", "d", "dh" };
        if (p->label[0]) put(&o, "(%s) ", p->label);
        put(&o, "(%u x ", p->repeat);
        put_body(&o, p);
        put(&o, ")");
        if (p->set_keep != QD_KEEP_ALL) put(&o, " %s%u", keep[p->set_keep], p->set_keep_n);
    } else {
        if (p->repeat > 1) put(&o, "%u x ", p->repeat);
        if (p->label[0]) put(&o, "(%s) ", p->label);
        put_body(&o, p);
    }
    if (p->comment[0]) put(&o, " ! %s", p->comment);
    return o.len;
}
