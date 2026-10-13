/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_roll.c
 * @brief   The roll engine: build a roll, check it, run it.
 *
 * ORDER OF RANDOM DRAWS (tests/host/rng_reference.py copies this, so the
 * PC reference and the device give the same rolls from the same seed):
 *
 *   Pools are rolled one after another, in the order they were added.
 *   Within a pool:
 *     1. count + |advantage| dice, first to last. A die is one draw as in
 *        qd_dice.c; a digit die is one draw per digit, left to right.
 *     2. For each step in order, the dice in play when the step starts are
 *        visited first to last. A matching die rolls its new die (or its
 *        whole chain, for ie / ir) straight away, before the next die is
 *        visited. New dice go on the end of the list, so a step never
 *        visits the dice it made itself.
 *   Keep/drop, totals and counting make no draws.
 */
#include <stdio.h>
#include <string.h>

#include "qd_roll.h"
#include "qd_rng.h"

/* ========================================================================
 * Tests
 * ======================================================================== */
static qd_test_t make_test(uint8_t op, int a, int b)
{
    qd_test_t t;
    memset(&t, 0, sizeof(t));
    t.op = op;
    t.a = (int16_t)a;
    t.b = (int16_t)b;
    return t;
}

qd_test_t qd_test_ge(int a)           { return make_test(QD_TEST_GE, a, 0); }
qd_test_t qd_test_le(int a)           { return make_test(QD_TEST_LE, a, 0); }
qd_test_t qd_test_gt(int a)           { return make_test(QD_TEST_GT, a, 0); }
qd_test_t qd_test_lt(int a)           { return make_test(QD_TEST_LT, a, 0); }
qd_test_t qd_test_eq(int a)           { return make_test(QD_TEST_EQ, a, 0); }
qd_test_t qd_test_ne(int a)           { return make_test(QD_TEST_NE, a, 0); }
qd_test_t qd_test_range(int a, int b) { return make_test(QD_TEST_RANGE, a, b); }
qd_test_t qd_test_max(void)           { return make_test(QD_TEST_MAX, 0, 0); }
qd_test_t qd_test_min(void)           { return make_test(QD_TEST_MIN, 0, 0); }

qd_test_t qd_test_label(const char *label)
{
    qd_test_t t = make_test(QD_TEST_LABEL, 0, 0);
    snprintf(t.label, sizeof(t.label), "%s", label ? label : "");
    return t;
}

bool qd_test_face(const qd_test_t *t, int16_t v, const char *label, int16_t lo, int16_t hi)
{
    switch (t->op) {
    case QD_TEST_GE:    return v >= t->a;
    case QD_TEST_LE:    return v <= t->a;
    case QD_TEST_GT:    return v > t->a;
    case QD_TEST_LT:    return v < t->a;
    case QD_TEST_EQ:    return v == t->a;
    case QD_TEST_NE:    return v != t->a;
    case QD_TEST_RANGE: return v >= t->a && v <= t->b;
    case QD_TEST_MAX:   return v == hi;
    case QD_TEST_MIN:   return v == lo;
    case QD_TEST_LABEL: return label && *t->label && !strcmp(label, t->label);
    default:            return false;
    }
}

char *qd_test_text(const qd_test_t *t, char *buf, int size)
{
    static const char *const sym[] = { "", ">=", "<=", ">", "<", "=", "!=" };
    if (t->op >= QD_TEST_GE && t->op <= QD_TEST_NE)
        snprintf(buf, (size_t)size, "%s%d", sym[t->op], t->a);
    else if (t->op == QD_TEST_RANGE)
        snprintf(buf, (size_t)size, "%d-%d", t->a, t->b);
    else if (t->op == QD_TEST_MAX)
        snprintf(buf, (size_t)size, "=max");
    else if (t->op == QD_TEST_MIN)
        snprintf(buf, (size_t)size, "=min");
    else if (t->op == QD_TEST_LABEL)
        snprintf(buf, (size_t)size, "\"%s\"", t->label);
    else
        snprintf(buf, (size_t)size, "-");
    return buf;
}

/* ========================================================================
 * Building
 * ======================================================================== */
void qd_roll_init(qd_roll_t *r)
{
    memset(r, 0, sizeof(*r));
}

qd_pool_t *qd_roll_new_pool(qd_roll_t *r)
{
    if (r->pool_count >= QD_MAX_POOLS) {
        r->error = QD_ERR_FULL;
        return NULL;
    }
    qd_pool_t *p = &r->pools[r->pool_count++];
    memset(p, 0, sizeof(*p));
    return p;
}

bool qd_roll_push(qd_roll_t *r, qd_op_kind_t kind, int32_t arg)
{
    if (r->op_count >= QD_MAX_ROLL_OPS) {
        r->error = QD_ERR_FULL;
        return false;
    }
    r->ops[r->op_count].kind = (uint8_t)kind;
    r->ops[r->op_count].arg = arg;
    r->op_count++;
    return true;
}

/* Put a new pool into the running total, added or subtracted. */
static qd_pool_t *join_sum(qd_roll_t *r, qd_pool_t *p, bool minus)
{
    if (!p)
        return NULL;
    bool first = r->op_count == 0;
    qd_roll_push(r, QD_OP_POOL, (int32_t)(p - r->pools));
    if (first) {
        if (minus)
            qd_roll_push(r, QD_OP_NEG, 0);
    } else {
        qd_roll_push(r, minus ? QD_OP_SUB : QD_OP_ADD, 0);
    }
    return p;
}

static qd_pool_t *standard_pool(qd_roll_t *r, uint16_t count, uint16_t sides)
{
    qd_pool_t *p = qd_roll_new_pool(r);
    if (p) {
        p->count = count;
        p->sides = sides;
    }
    return p;
}

qd_pool_t *qd_roll_dice(qd_roll_t *r, uint16_t count, uint16_t sides)
{
    return join_sum(r, standard_pool(r, count, sides), false);
}

qd_pool_t *qd_roll_dice_minus(qd_roll_t *r, uint16_t count, uint16_t sides)
{
    return join_sum(r, standard_pool(r, count, sides), true);
}

qd_pool_t *qd_roll_custom(qd_roll_t *r, uint16_t count, const qd_die_t *die)
{
    qd_pool_t *p = qd_roll_new_pool(r);
    if (p) {
        p->count = count;
        p->die = die;
        p->sides = die ? die->sides : 0;
    }
    return join_sum(r, p, false);
}

qd_pool_t *qd_roll_percent(qd_roll_t *r, uint16_t count, int bonus)
{
    qd_pool_t *p = qd_roll_new_pool(r);
    if (p) {
        p->count = count;
        p->sides = 100;
        p->percentile = true;
        p->pct_bonus = (int8_t)(bonus > 127 ? 127 : bonus < -127 ? -127 : bonus);
    }
    return join_sum(r, p, false);
}

qd_pool_t *qd_roll_digits(qd_roll_t *r, uint16_t count, const char *digits)
{
    size_t n = digits ? strlen(digits) : 0;
    if (n < 1 || n > QD_MAX_DIGITS) {
        r->error = QD_ERR_DIGITS;
        return NULL;
    }
    for (size_t i = 0; i < n; i++)
        if (digits[i] < '1' || digits[i] > '9') {
            r->error = QD_ERR_DIGITS;
            return NULL;
        }
    qd_pool_t *p = qd_roll_new_pool(r);
    if (p) {
        p->count = count;
        p->digits = (uint8_t)n;
        for (size_t i = 0; i < n; i++)
            p->digit_sides[i] = (uint8_t)(digits[i] - '0');
    }
    return join_sum(r, p, false);
}

void qd_roll_add(qd_roll_t *r, int32_t n)
{
    qd_roll_push(r, QD_OP_NUMBER, n);
    if (r->op_count > 1)
        qd_roll_push(r, QD_OP_ADD, 0);
}

void qd_roll_mul(qd_roll_t *r, int32_t n)
{
    qd_roll_push(r, QD_OP_NUMBER, n);
    qd_roll_push(r, QD_OP_MUL, 0);
}

void qd_roll_div(qd_roll_t *r, int32_t n)
{
    qd_roll_push(r, QD_OP_NUMBER, n);
    qd_roll_push(r, QD_OP_DIV, 0);
}

bool qd_roll_bucket(qd_roll_t *r, qd_test_t t)
{
    if (r->bucket_count >= QD_MAX_BUCKETS) {
        r->error = QD_ERR_FULL;
        return false;
    }
    r->buckets[r->bucket_count++].test = t;
    return true;
}

/* Every qd_pool_* setter does nothing with a NULL pool, so a builder that
 * ran out of room (and returned NULL) can be chained without a check:
 * qd_pool_keep(qd_roll_dice(&r, 4, 6), QD_KEEP_HIGH, 3). The roll itself
 * remembers the failure, and qd_roll_run refuses it with QD_ERR_FULL. */
void qd_pool_advantage(qd_pool_t *p, int n)
{
    if (!p) return;
    if (n > 127) n = 127;
    if (n < -127) n = -127;
    p->advantage = (int8_t)n;
}

static bool add_step(qd_pool_t *p, uint8_t kind, qd_test_t when, bool forever)
{
    if (!p || p->step_count >= QD_MAX_POOL_STEPS)
        return false;
    qd_step_t *s = &p->steps[p->step_count++];
    s->kind = kind;
    s->forever = forever;
    s->test = when;
    return true;
}

bool qd_pool_explode(qd_pool_t *p, qd_test_t when, bool forever)
{
    return add_step(p, QD_STEP_EXPLODE, when, forever);
}

bool qd_pool_implode(qd_pool_t *p, qd_test_t when)
{
    return add_step(p, QD_STEP_IMPLODE, when, false);
}

bool qd_pool_reroll(qd_pool_t *p, qd_test_t when, bool forever)
{
    return add_step(p, QD_STEP_REROLL, when, forever);
}

void qd_pool_keep(qd_pool_t *p, qd_keep_t how, uint16_t n)
{
    if (!p) return;
    p->keep = (uint8_t)how;
    p->keep_n = n;
}

void qd_pool_success(qd_pool_t *p, qd_test_t t) { if (p) p->success = t; }
void qd_pool_twice(qd_pool_t *p, qd_test_t t)   { if (p) p->twice = t; }
void qd_pool_failure(qd_pool_t *p, qd_test_t t) { if (p) p->failure = t; }
void qd_pool_botch(qd_pool_t *p, qd_test_t t)   { if (p) p->botch = t; }

/* ========================================================================
 * Faces of a pool's die
 * ======================================================================== */
void qd_pool_range(const qd_pool_t *p, int16_t *lo, int16_t *hi)
{
    if (p->percentile) {
        *lo = 1;
        *hi = 100;
    } else if (p->digits) {
        int a = 0, b = 0;
        for (int i = 0; i < p->digits; i++) {
            a = a * 10 + 1;
            b = b * 10 + p->digit_sides[i];
        }
        *lo = (int16_t)a;
        *hi = (int16_t)b;
    } else if (p->die) {
        *lo = *hi = qd_die_face_value(p->die, 0);
        for (uint16_t f = 1; f < p->die->sides; f++) {
            int16_t v = qd_die_face_value(p->die, f);
            if (v < *lo) *lo = v;
            if (v > *hi) *hi = v;
        }
    } else {
        *lo = 1;
        *hi = (int16_t)p->sides;
    }
}

static const char *face_label(const qd_pool_t *p, uint16_t face)
{
    return p->die ? qd_die_face_label(p->die, face) : "";
}

/* Number of distinct faces, for "does this test match every face?". */
static uint16_t face_total(const qd_pool_t *p)
{
    return p->die ? p->die->sides : p->sides;
}

static int16_t face_value(const qd_pool_t *p, uint16_t face)
{
    return p->die ? qd_die_face_value(p->die, face) : (int16_t)(face + 1);
}

/* ========================================================================
 * Checking
 * ======================================================================== */
const char *qd_err_text(qd_err_t e)
{
    switch (e) {
    case QD_OK:                return "ok";
    case QD_ERR_FULL:          return "too many pools, steps, buckets or terms";
    case QD_ERR_NO_DICE:       return "a pool needs at least 1 die with at least 1 side";
    case QD_ERR_TOO_MANY_DICE: return "too many dice";
    case QD_ERR_DIGITS:        return "digit dice need 1-4 digits of 1-9 and can't explode or reroll";
    case QD_ERR_FOREVER:       return "explodes or rerolls on every face, so it would never stop";
    case QD_ERR_SUM:           return "the sum doesn't add up";
    case QD_ERR_DIV_ZERO:      return "divide by zero";
    case QD_ERR_PERCENT:       return "percentile dice take bonus/penalty (up to 3) but no other modifiers";
    case QD_ERR_REROLL:        return "that die can't be rerolled";
    }
    return "unknown error";
}

qd_err_t qd_roll_check(const qd_roll_t *r)
{
    if (r->error)
        return (qd_err_t)r->error;
    if (!r->pool_count)
        return QD_ERR_NO_DICE;

    uint32_t dice = 0;
    for (int i = 0; i < r->pool_count; i++) {
        const qd_pool_t *p = &r->pools[i];
        if (!p->count)
            return QD_ERR_NO_DICE;
        if (p->digits) {
            if (p->digits > QD_MAX_DIGITS || p->step_count)
                return QD_ERR_DIGITS;
            for (int d = 0; d < p->digits; d++)
                if (p->digit_sides[d] < 1 || p->digit_sides[d] > 9)
                    return QD_ERR_DIGITS;
        } else if (p->die ? !p->die->sides : (!p->sides || p->sides > 32767)) {
            return QD_ERR_NO_DICE;
        }
        if (p->percentile) {
            if (p->step_count || p->advantage || p->die || p->digits ||
                p->pct_bonus > QD_MAX_BONUS_DICE || p->pct_bonus < -QD_MAX_BONUS_DICE)
                return QD_ERR_PERCENT;
            dice += (uint32_t)p->count * (1u + (uint32_t)(p->pct_bonus < 0 ? -p->pct_bonus : p->pct_bonus));
            continue;
        }
        dice += p->count + (uint32_t)(p->advantage < 0 ? -p->advantage : p->advantage);

        /* "ie1" on a d6 would explode forever: refuse it up front. */
        int16_t lo, hi;
        qd_pool_range(p, &lo, &hi);
        for (int s = 0; s < p->step_count; s++) {
            if (!p->steps[s].forever)
                continue;
            bool all = true;
            for (uint16_t f = 0; f < face_total(p) && all; f++)
                all = qd_test_face(&p->steps[s].test, face_value(p, f), face_label(p, f), lo, hi);
            if (all)
                return QD_ERR_FOREVER;
        }
    }
    if (dice > QD_MAX_ROLL_DICE)
        return QD_ERR_TOO_MANY_DICE;

    /* Walk the sum program counting the stack: never below one operand
     * for an operator, exactly one value at the end. */
    int depth = 0;
    for (int i = 0; i < r->op_count; i++) {
        const qd_op_t *o = &r->ops[i];
        switch (o->kind) {
        case QD_OP_POOL:
            if (o->arg < 0 || o->arg >= r->pool_count)
                return QD_ERR_SUM;
            depth++;
            break;
        case QD_OP_NUMBER:
            depth++;
            break;
        case QD_OP_NEG:
            if (depth < 1)
                return QD_ERR_SUM;
            break;
        case QD_OP_DIV:
            if (i > 0 && r->ops[i - 1].kind == QD_OP_NUMBER && r->ops[i - 1].arg == 0)
                return QD_ERR_DIV_ZERO;
            /* fall through */
        case QD_OP_ADD: case QD_OP_SUB: case QD_OP_MUL: case QD_OP_MAX:
            if (depth < 2)
                return QD_ERR_SUM;
            depth--;
            break;
        default:
            return QD_ERR_SUM;
        }
    }
    if (r->op_count && depth != 1)
        return QD_ERR_SUM;
    return QD_OK;
}

/* ========================================================================
 * Running
 * ======================================================================== */

/* The working list for the pool being rolled. Every die of the pool is
 * here; the result lists as many as it has room for. */
static qd_die_result_t work[QD_MAX_ROLL_DICE];
static uint16_t order[QD_MAX_ROLL_DICE];
static uint16_t work_n;         /* dice in this pool */
static uint16_t roll_n;         /* dice in the whole roll so far */

typedef struct {
    const qd_pool_t *p;
    uint8_t index;
    int16_t lo, hi;
} pool_ctx_t;

static bool matches(const pool_ctx_t *c, const qd_test_t *t, const qd_die_result_t *d)
{
    return qd_test_face(t, d->value, face_label(c->p, d->face), c->lo, c->hi);
}

/* Roll one die of the pool onto the end of the list. Returns its index,
 * or -1 if the roll already has QD_MAX_ROLL_DICE dice. */
/* Put a die whose value is already known on the end of the list. */
static int add_record(const pool_ctx_t *c, int16_t value, uint16_t face, uint16_t parent,
                      uint16_t flags)
{
    if (roll_n >= QD_MAX_ROLL_DICE)
        return -1;
    qd_die_result_t *d = &work[work_n];
    d->value = value;
    d->face = face;
    d->was = 0;
    d->flags = flags;
    if (d->value == c->hi) d->flags |= QD_DIE_MAX;
    if (d->value == c->lo) d->flags |= QD_DIE_MIN;
    d->parent = parent;
    d->pool = c->index;
    roll_n++;
    return work_n++;
}

static qd_observer_t observer;
static void *observer_ctx;

qd_observer_t qd_roll_observe(qd_observer_t fn, void *ctx, void **old_ctx)
{
    qd_observer_t old = observer;
    if (old_ctx) *old_ctx = observer_ctx;
    observer = fn;
    observer_ctx = ctx;
    return old;
}

static void observe(uint16_t sides, const qd_die_t *die, uint16_t face, int16_t value)
{
    if (observer)
        observer(sides, die, face, value, observer_ctx);
}

/* One fresh value for a die of the pool: the only place dice are drawn. */
static void draw(const qd_pool_t *p, qd_die_result_t *d)
{
    if (p->digits) {
        int v = 0;
        for (int i = 0; i < p->digits; i++) {
            int digit = (int)qd_rng_range(p->digit_sides[i]) + 1;
            observe(p->digit_sides[i], NULL, (uint16_t)(digit - 1), (int16_t)digit);
            v = v * 10 + digit;
        }
        d->value = (int16_t)v;
        d->face = (uint16_t)v;
    } else if (p->die) {
        qd_face_roll_t f = qd_die_roll(p->die);
        d->value = f.value;
        d->face = f.face;
        observe(p->die->sides, p->die, f.face, f.value);
    } else {
        d->face = (uint16_t)qd_rng_range(p->sides);
        d->value = (int16_t)(d->face + 1);
        observe(p->sides, NULL, d->face, d->value);
    }
}

/* A percentile digit as the d10 shows it: 0 reads as 10. */
static void observe_d10(uint8_t digit)
{
    int v = digit ? digit : 10;
    observe(10, NULL, (uint16_t)(v - 1), (int16_t)v);
}

static int add_die(const pool_ctx_t *c, uint16_t parent, uint16_t flags)
{
    if (roll_n >= QD_MAX_ROLL_DICE)
        return -1;
    qd_die_result_t *d = &work[work_n];
    draw(c->p, d);
    d->was = 0;
    d->flags = flags;
    if (d->value == c->hi) d->flags |= QD_DIE_MAX;
    if (d->value == c->lo) d->flags |= QD_DIE_MIN;
    d->parent = parent;
    d->pool = c->index;
    roll_n++;
    return work_n++;
}

/* Sort the in-play dice by value, lowest first; ties stay in the order
 * they were rolled. A bottom-up merge sort: stable, and about 10,000
 * comparisons for 1000 dice where a simple insertion sort needs 250,000. */
static int sort_in_play(void)
{
    static uint16_t spare[QD_MAX_ROLL_DICE];
    int n = 0;
    for (uint16_t i = 0; i < work_n; i++)
        if (!(work[i].flags & QD_DIE_OUT))
            order[n++] = i;
    uint16_t *from = order, *to = spare;
    for (int width = 1; width < n; width *= 2) {
        for (int lo = 0; lo < n; lo += 2 * width) {
            int mid = lo + width < n ? lo + width : n;
            int hi = lo + 2 * width < n ? lo + 2 * width : n;
            int a = lo, b = mid, k = lo;
            while (a < mid && b < hi)        /* <= keeps ties in rolled order */
                to[k++] = work[from[a]].value <= work[from[b]].value ? from[a++] : from[b++];
            while (a < mid) to[k++] = from[a++];
            while (b < hi)  to[k++] = from[b++];
        }
        uint16_t *t = from; from = to; to = t;
    }
    if (from != order)
        memcpy(order, from, (size_t)n * sizeof(order[0]));
    return n;
}

/* Mark `n` dice from the bottom and `m` from the top of the sorted list. */
static void mark_ends(int live, int n, int m, uint16_t flag)
{
    if (n > live) n = live;
    if (m > live - n) m = live - n;
    for (int i = 0; i < n; i++)
        work[order[i]].flags |= flag;
    for (int i = 0; i < m; i++)
        work[order[live - 1 - i]].flags |= flag;
}

static void run_step(const pool_ctx_t *c, const qd_step_t *s, qd_result_t *res)
{
    uint16_t n0 = work_n;
    for (uint16_t i = 0; i < n0; i++) {
        if ((work[i].flags & QD_DIE_OUT) || !matches(c, &s->test, &work[i]))
            continue;
        int j;
        switch (s->kind) {
        case QD_STEP_EXPLODE: {
            uint16_t last = i;
            for (int k = 1;; k++) {
                if ((j = add_die(c, last, QD_DIE_EXPLODED)) < 0) {
                    res->flags |= QD_RES_CAPPED;
                    break;
                }
                if (!s->forever || !matches(c, &s->test, &work[j]))
                    break;
                if (k >= QD_MAX_EXPLOSIONS) {
                    work[j].flags |= QD_DIE_CAPPED;
                    res->flags |= QD_RES_CAPPED;
                    break;
                }
                last = (uint16_t)j;
            }
            break;
        }
        case QD_STEP_IMPLODE:
            if (add_die(c, i, QD_DIE_IMPLODED) < 0)
                res->flags |= QD_RES_CAPPED;
            break;
        case QD_STEP_REROLL: {
            uint16_t cur = i;
            for (int k = 1;; k++) {
                if ((j = add_die(c, cur, QD_DIE_REPLACEMENT)) < 0) {
                    res->flags |= QD_RES_CAPPED;     /* no new die: the old one stays */
                    break;
                }
                work[cur].flags |= QD_DIE_REROLLED;
                if (!s->forever || !matches(c, &s->test, &work[j]))
                    break;
                if (k >= QD_MAX_EXPLOSIONS) {
                    work[j].flags |= QD_DIE_CAPPED;
                    res->flags |= QD_RES_CAPPED;
                    break;
                }
                cur = (uint16_t)j;
            }
            break;
        }
        }
    }
}

static void finish_pool(const qd_roll_t *r, const pool_ctx_t *cp, qd_result_t *res, bool list);

static void run_pool(const qd_roll_t *r, uint8_t index, qd_result_t *res)
{
    const qd_pool_t *p = &r->pools[index];
    pool_ctx_t c = { p, index, 0, 0 };
    qd_pool_range(p, &c.lo, &c.hi);
    qd_pool_result_t *pr = &res->pools[index];
    work_n = 0;

    /* 1. Roll, then set aside for advantage / disadvantage. */
    int adv = p->advantage;
    int extra = adv < 0 ? -adv : adv;
    if (p->percentile) {
        /* Each d% is a units die and tens dice (qd_dice.c draws them). The
         * result is listed; the tens dice not used are listed after it,
         * set aside, as the results they would have given. */
        for (int i = 0; i < p->count; i++) {
            qd_percentile_t pc;
            qd_roll_percentile(p->pct_bonus, &pc);
            observe_d10(pc.units);
            for (int k = 0; k < pc.tens_count; k++) observe_d10(pc.tens[k]);
            int main = add_record(&c, pc.result, (uint16_t)(pc.result - 1), QD_NO_PARENT, 0);
            for (int k = 0; k < pc.tens_count; k++) {
                if (k == pc.kept) continue;
                int v = pc.tens[k] * 10 + pc.units;
                if (!v) v = 100;
                add_record(&c, (int16_t)v, (uint16_t)(v - 1), (uint16_t)main, QD_DIE_SET_ASIDE);
            }
        }
    } else {
        for (int i = 0; i < p->count + extra; i++)
            add_die(&c, QD_NO_PARENT, 0);
    }
    if (extra) {
        int live = sort_in_play();
        mark_ends(live, adv > 0 ? extra : 0, adv < 0 ? extra : 0, QD_DIE_SET_ASIDE);
    }

    /* 2. Explode, implode, reroll. */
    for (int s = 0; s < p->step_count; s++)
        run_step(&c, &p->steps[s], res);

    pr->rolled = work_n;
    finish_pool(r, &c, res, true);
}

/* Stages 3, 4 and 6 for the pool in `work`: keep or drop, totals,
 * counting, sets, buckets. Run once after rolling (list = true: the dice
 * are then copied into the result) and again after a reroll (list =
 * false: they go back where they came from). */
static void finish_pool(const qd_roll_t *r, const pool_ctx_t *cp, qd_result_t *res, bool list)
{
    const qd_pool_t *p = cp->p;
    pool_ctx_t c = *cp;
    qd_pool_result_t *pr = &res->pools[c.index];

    /* 3. Keep or drop (no need to sort if there's nothing to keep or drop). */
    int live = p->keep == QD_KEEP_ALL ? 0 : sort_in_play();
    int n = p->keep_n > live ? live : p->keep_n;
    switch (p->keep) {
    case QD_KEEP_HIGH:   mark_ends(live, live - n, 0, QD_DIE_DROPPED); break;
    case QD_KEEP_LOW:    mark_ends(live, 0, live - n, QD_DIE_DROPPED); break;
    case QD_KEEP_MIDDLE: mark_ends(live, (live - n) / 2, live - n - (live - n) / 2, QD_DIE_DROPPED); break;
    case QD_DROP_LOW:    mark_ends(live, n, 0, QD_DIE_DROPPED); break;
    case QD_DROP_HIGH:   mark_ends(live, 0, n, QD_DIE_DROPPED); break;
    default: break;
    }

    /* Matching sets among the kept dice: runs of equal values in sorted order. */
    if (p->sets) {
        int kept = sort_in_play();
        for (int a = 0; a < kept;) {
            int b = a;
            while (b < kept && work[order[b]].value == work[order[a]].value) b++;
            if (b - a >= 2) {
                /* insert, biggest set first, then highest value */
                int16_t v = work[order[a]].value;
                uint16_t n = (uint16_t)(b - a);
                int at = pr->set_count;
                while (at > 0 && (pr->sets[at - 1].size < n ||
                                  (pr->sets[at - 1].size == n && pr->sets[at - 1].value < v)))
                    at--;
                if (at < QD_MAX_SETS) {
                    int last = pr->set_count < QD_MAX_SETS ? pr->set_count : QD_MAX_SETS - 1;
                    for (int m = last; m > at; m--) pr->sets[m] = pr->sets[m - 1];
                    pr->sets[at].value = v;
                    pr->sets[at].size = n;
                    if (pr->set_count < QD_MAX_SETS) pr->set_count++;
                }
            }
            a = b;
        }
    }

    /* 4 and 6. Totals and counting over the kept dice. */
    pr->face_counts = !p->digits && face_total(p) <= QD_MAX_FACES;
    bool first = true;
    for (uint16_t i = 0; i < work_n; i++) {
        qd_die_result_t *d = &work[i];
        if (d->flags & QD_DIE_IMPLODED) {
            if (!(d->flags & QD_DIE_REROLLED))     /* its die was rerolled: gone */
                pr->total -= d->value;
            continue;
        }
        if (d->flags & QD_DIE_OUT)
            continue;
        pr->total += d->value;
        pr->kept++;
        if (first || d->value < pr->min) pr->min = d->value;
        if (first || d->value > pr->max) pr->max = d->value;
        first = false;
        if (pr->face_counts)
            pr->face_count[d->face]++;
        if (matches(&c, &p->success, d)) {
            d->flags |= QD_DIE_SUCCESS;
            pr->successes++;
            if (matches(&c, &p->twice, d)) {
                d->flags |= QD_DIE_TWICE;
                pr->successes++;
            }
        }
        if (matches(&c, &p->failure, d)) {
            d->flags |= QD_DIE_FAILURE;
            pr->failures++;
        }
        if (matches(&c, &p->botch, d)) {
            d->flags |= QD_DIE_BOTCH;
            pr->botches++;
        }
        bool any = false;
        for (int b = 0; b < r->bucket_count; b++)
            if (matches(&c, &r->buckets[b].test, d)) {
                res->buckets[b]++;
                any = true;
            }
        if (!any)
            res->buckets[r->bucket_count]++;
    }
    if (p->success.op != QD_TEST_NONE || p->failure.op != QD_TEST_NONE)
        res->flags |= QD_RES_COUNTING;
    res->successes += (int16_t)(pr->successes - pr->failures);
    res->failures += pr->failures;
    res->botches += pr->botches;

    if (!list) {                                   /* after a reroll: put them back */
        memcpy(&res->dice[pr->first], work, pr->listed * sizeof(work[0]));
        return;
    }

    /* List as many dice as there is room for. */
    pr->first = res->dice_count;
    uint16_t room = (uint16_t)(QD_MAX_KEPT_DICE - res->dice_count);
    pr->listed = work_n < room ? work_n : room;
    memcpy(&res->dice[res->dice_count], work, pr->listed * sizeof(work[0]));
    res->dice_count = (uint16_t)(res->dice_count + pr->listed);
    if (pr->listed < work_n)
        res->flags |= QD_RES_SUMMARY;
}

static int64_t clamp32(int64_t v, qd_result_t *res)
{
    if (v > INT32_MAX) { res->flags |= QD_RES_CLAMPED; return INT32_MAX; }
    if (v < INT32_MIN) { res->flags |= QD_RES_CLAMPED; return INT32_MIN; }
    return v;
}

/* 5. The sum. qd_roll_check has made sure the stack never runs dry. */
static int32_t run_sum(const qd_roll_t *r, qd_result_t *res)
{
    int64_t stack[QD_MAX_ROLL_OPS];
    int top = 0;
    for (int i = 0; i < r->op_count; i++) {
        const qd_op_t *o = &r->ops[i];
        int64_t a, b;
        switch (o->kind) {
        case QD_OP_POOL:   stack[top++] = res->pools[o->arg].total; continue;
        case QD_OP_NUMBER: stack[top++] = o->arg; continue;
        case QD_OP_NEG:    stack[top - 1] = clamp32(-stack[top - 1], res); continue;
        default: break;
        }
        b = stack[--top];
        a = stack[top - 1];
        switch (o->kind) {
        case QD_OP_ADD: a += b; break;
        case QD_OP_SUB: a -= b; break;
        case QD_OP_MUL: a *= b; break;
        case QD_OP_MAX: a = a > b ? a : b; break;
        case QD_OP_DIV:
            if (b == 0) {
                res->flags |= QD_RES_DIV_ZERO;
                a = 0;
            } else {
                a /= b;
            }
            break;
        }
        stack[top - 1] = clamp32(a, res);
    }
    return top ? (int32_t)stack[0] : 0;
}

uint16_t qd_roll_warnings(const qd_roll_t *r)
{
    uint16_t w = 0;
    for (int i = 0; i < r->pool_count; i++) {
        const qd_pool_t *p = &r->pools[i];
        if (p->digits) {
            for (int d = 0; d < p->digits; d++)
                if (p->digit_sides[d] == 1) w |= QD_WARN_ONE_SIDED;
        } else if (face_total(p) == 1) {
            w |= QD_WARN_ONE_SIDED;
        }
        if (p->keep == QD_KEEP_HIGH || p->keep == QD_KEEP_LOW || p->keep == QD_KEEP_MIDDLE) {
            if (p->keep_n == 0) w |= QD_WARN_KEEP_NONE;
        }
    }
    return w;
}

const char *qd_warn_text(uint16_t w)
{
    if (w & QD_WARN_ONE_SIDED) return "a die with one side always shows the same number";
    if (w & QD_WARN_KEEP_NONE) return "keeping 0 dice: the total is always 0";
    return "";
}

qd_err_t qd_roll_run(const qd_roll_t *r, qd_result_t *res)
{
    memset(res, 0, sizeof(*res));
    qd_err_t e = qd_roll_check(r);
    if (e)
        return e;
    res->warnings = qd_roll_warnings(r);
    res->pool_count = r->pool_count;
    res->bucket_count = r->bucket_count;
    roll_n = 0;
    for (uint8_t i = 0; i < r->pool_count; i++)
        run_pool(r, i, res);
    res->rolled = roll_n;
    res->total = run_sum(r, res);
    return QD_OK;
}

/* ========================================================================
 * Rerolls after the fact
 * ======================================================================== */

/* Work every pool out again from the dice in the result, then the sum. */
static void recompute(const qd_roll_t *r, qd_result_t *res)
{
    const uint16_t derived = QD_DIE_DROPPED | QD_DIE_SUCCESS | QD_DIE_TWICE | QD_DIE_FAILURE |
                             QD_DIE_BOTCH;
    res->successes = res->failures = res->botches = 0;
    memset(res->buckets, 0, sizeof(res->buckets));
    res->flags &= (uint16_t)~QD_RES_COUNTING;
    for (uint8_t k = 0; k < r->pool_count; k++) {
        qd_pool_result_t *pr = &res->pools[k];
        pool_ctx_t c = { &r->pools[k], k, 0, 0 };
        qd_pool_range(c.p, &c.lo, &c.hi);
        work_n = pr->listed;
        memcpy(work, &res->dice[pr->first], work_n * sizeof(work[0]));
        for (uint16_t i = 0; i < work_n; i++)
            work[i].flags &= (uint16_t)~derived;
        pr->total = 0;
        pr->kept = 0;
        pr->min = pr->max = 0;
        pr->successes = pr->failures = pr->botches = 0;
        pr->set_count = 0;
        memset(pr->face_count, 0, sizeof(pr->face_count));
        finish_pool(r, &c, res, false);
    }
    res->total = run_sum(r, res);
}

/* Note the result as it was, before a reroll changes it. */
static void remember(qd_result_t *res)
{
    res->was_total = res->total;
    res->was_successes = res->successes;
    res->flags |= QD_RES_REROLLED;
    if (res->rerolls < 255) res->rerolls++;
}

qd_err_t qd_reroll_all(const qd_roll_t *r, qd_result_t *res)
{
    int32_t was = res->total;
    int16_t was_s = res->successes;
    uint8_t n = res->rerolls;
    qd_err_t e = qd_roll_run(r, res);
    if (e)
        return e;
    res->was_total = was;
    res->was_successes = was_s;
    res->rerolls = (uint8_t)(n < 255 ? n + 1 : n);
    res->flags |= QD_RES_REROLLED;
    return QD_OK;
}

/* Which pool a listed die belongs to, and its index within that pool. */
static bool locate(const qd_result_t *res, uint16_t i, uint8_t *pool, uint16_t *local)
{
    if (i >= res->dice_count)
        return false;
    *pool = res->dice[i].pool;
    *local = (uint16_t)(i - res->pools[*pool].first);
    return true;
}

qd_err_t qd_reroll_dice(const qd_roll_t *r, qd_result_t *res, const uint16_t *which, int n)
{
    if (res->flags & QD_RES_SUMMARY)
        return QD_ERR_REROLL;
    /* Check them all first, so a bad index changes nothing. */
    const uint16_t gone = QD_DIE_SET_ASIDE | QD_DIE_REROLLED | QD_DIE_IMPLODED;
    for (int k = 0; k < n; k++) {
        uint8_t pool;
        uint16_t local;
        if (!locate(res, which[k], &pool, &local) || (res->dice[which[k]].flags & gone) ||
            r->pools[pool].percentile)
            return QD_ERR_REROLL;
    }
    remember(res);
    for (int k = 0; k < n; k++) {
        qd_die_result_t *d = &res->dice[which[k]];
        const qd_pool_t *p = &r->pools[d->pool];
        pool_ctx_t c = { p, d->pool, 0, 0 };
        qd_pool_range(p, &c.lo, &c.hi);
        if (!(d->flags & QD_DIE_CHANGED)) {
            d->was = d->value;
            d->flags |= QD_DIE_CHANGED;
        }
        draw(p, d);
        d->flags &= (uint16_t)~(QD_DIE_MAX | QD_DIE_MIN);
        if (d->value == c.hi) d->flags |= QD_DIE_MAX;
        if (d->value == c.lo) d->flags |= QD_DIE_MIN;

        /* Dice that came from this one (explosions, implosions, and theirs)
         * are set aside: the roll that made them is gone. */
        const qd_pool_result_t *pr = &res->pools[d->pool];
        uint16_t self = (uint16_t)(which[k] - pr->first);
        bool changed = true;
        while (changed) {
            changed = false;
            for (uint16_t i = 0; i < pr->listed; i++) {
                qd_die_result_t *e = &res->dice[pr->first + i];
                if (e->parent == QD_NO_PARENT || (e->flags & QD_DIE_REROLLED) ||
                    !(e->flags & (QD_DIE_EXPLODED | QD_DIE_IMPLODED)))
                    continue;
                const qd_die_result_t *parent = &res->dice[pr->first + e->parent];
                if (e->parent == self || (parent->flags & QD_DIE_REROLLED && parent != d)) {
                    e->flags |= QD_DIE_REROLLED;
                    changed = true;
                }
            }
        }
    }
    recompute(r, res);
    return QD_OK;
}

int qd_reroll_matching(const qd_roll_t *r, qd_result_t *res, int pool, qd_test_t t)
{
    static uint16_t which[QD_MAX_KEPT_DICE];
    int n = 0;
    for (uint16_t i = 0; i < res->dice_count; i++) {
        const qd_die_result_t *d = &res->dice[i];
        if ((pool >= 0 && d->pool != pool) || (d->flags & QD_DIE_OUT) || r->pools[d->pool].percentile)
            continue;
        pool_ctx_t c = { &r->pools[d->pool], d->pool, 0, 0 };
        qd_pool_range(c.p, &c.lo, &c.hi);
        if (matches(&c, &t, d))
            which[n++] = i;
    }
    if (!n)
        return 0;
    return qd_reroll_dice(r, res, which, n) == QD_OK ? n : -1;
}

bool qd_reroll_keep(const qd_roll_t *r, qd_result_t *res, bool higher)
{
    static qd_result_t again;
    if (qd_roll_run(r, &again) != QD_OK)
        return false;
    bool counting = (res->flags & QD_RES_COUNTING) != 0;
    int32_t old_v = counting ? res->successes : res->total;
    int32_t new_v = counting ? again.successes : again.total;
    bool take_new = higher ? new_v >= old_v : new_v <= old_v;
    uint8_t n = res->rerolls;
    if (take_new) {
        int32_t was = res->total;
        int16_t was_s = res->successes;
        *res = again;
        res->was_total = was;
        res->was_successes = was_s;
    } else {
        res->was_total = again.total;
        res->was_successes = again.successes;
        res->flags |= QD_RES_KEPT_FIRST;
    }
    res->flags |= QD_RES_REROLLED;
    res->rerolls = (uint8_t)(n < 255 ? n + 1 : n);
    return take_new;
}
