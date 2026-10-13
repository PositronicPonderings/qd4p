/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_odds.c
 * @brief   Odds: exact by convolution, exact by trying every roll, or
 *          simulated. See qd_odds.h for when each is used.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "qd_odds.h"
#include "qd_rng.h"
#include "qd_sys.h"

/* ========================================================================
 * Gathering results (trying every roll, or simulating)
 * ======================================================================== */
typedef struct {
    double  weight, m1, m2;
    int32_t lo, hi;
    bool    any;
    double  pass, level[9];
    bool    judged;
    int32_t first;
    double  hist[QD_ODDS_BINS];
    double  below, above;
} acc_t;

static acc_t acc;

static void acc_add(const qd_roll_t *r, const qd_result_t *res, double w, bool *counting)
{
    *counting = (res->flags & QD_RES_COUNTING) != 0;
    int32_t v = *counting ? res->successes : res->total;
    acc.weight += w;
    acc.m1 += w * v;
    acc.m2 += w * (double)v * v;
    if (!acc.any || v < acc.lo) acc.lo = v;
    if (!acc.any || v > acc.hi) acc.hi = v;
    acc.any = true;
    int64_t i = (int64_t)v - acc.first;
    if (i < 0) acc.below += w;
    else if (i >= QD_ODDS_BINS) acc.above += w;
    else acc.hist[i] += w;
    qd_outcome_t o;
    if (qd_sys_outcome(r, res, &o)) {
        acc.judged = true;
        if (o.pass) acc.pass += w;
        int l = o.level < -4 ? -4 : o.level > 4 ? 4 : o.level;
        acc.level[l + 4] += w;
    }
}

static void acc_finish(qd_odds_t *out)
{
    double W = acc.weight > 0 ? acc.weight : 1;
    out->mean = acc.m1 / W;
    double var = acc.m2 / W - out->mean * out->mean;
    out->sd = var > 0 ? sqrt(var) : 0;
    out->lo = acc.lo;
    out->hi = acc.hi;
    out->judged = acc.judged;
    out->pass = acc.pass / W;
    for (int l = 0; l < 9; l++) out->level[l] = acc.level[l] / W;
    out->first = acc.first;
    out->bins = QD_ODDS_BINS;
    for (int i = 0; i < QD_ODDS_BINS; i++) out->dist[i] = (float)(acc.hist[i] / W);
    out->below = acc.below / W;
    out->above = acc.above / W;
    /* trim empty ends of the window */
    int a = 0, b = QD_ODDS_BINS;
    while (a < b && out->dist[a] == 0) a++;
    while (b > a && out->dist[b - 1] == 0) b--;
    if (a < b) {
        memmove(out->dist, out->dist + a, (size_t)(b - a) * sizeof(float));
        out->first += a;
        out->bins = (uint16_t)(b - a);
    } else {
        out->bins = 0;
    }
}

/* Where to put the window of values: a quick look at a few rolls first. */
static void choose_window(const qd_roll_t *r)
{
    static qd_result_t res;
    int32_t lo = 0, hi = 0;
    for (int i = 0; i < 200; i++) {
        qd_roll_run(r, &res);
        int32_t v = (res.flags & QD_RES_COUNTING) ? res.successes : res.total;
        if (!i || v < lo) lo = v;
        if (!i || v > hi) hi = v;
    }
    int64_t mid = ((int64_t)lo + hi) / 2;
    acc.first = (int32_t)(mid - QD_ODDS_BINS / 2);
    if (hi - lo + 1 < QD_ODDS_BINS / 2 && lo - acc.first > 8)
        acc.first = lo - 8 < acc.first ? acc.first : lo - 8;
}

/* ========================================================================
 * 2. Trying every roll: the generator is scripted to give each possible
 *    sequence of draws once, like an odometer. Each sequence's chance is
 *    the product of 1/n for every draw of n.
 * ======================================================================== */
#define MAX_DRAWS 256

/* A rough upper bound on how many rolls there are to try (log10), or a
 * huge number when dice can explode or reroll without end. */
static double log_rolls(const qd_roll_t *r)
{
    double total = 0;
    for (int k = 0; k < r->pool_count; k++) {
        const qd_pool_t *p = &r->pools[k];
        double per;                                     /* log10 of draws' choices per die */
        if (p->percentile) per = 2 + abs(p->pct_bonus);        /* a units d10, 1+ tens d10s */
        else if (p->digits) {
            per = 0;
            for (int d = 0; d < p->digits; d++) per += log10(p->digit_sides[d]);
        } else if (p->die) per = log10(p->die->weighted ? p->die->total_weight : p->die->sides);
        else per = log10(p->sides);
        int extra = 0;
        for (int s = 0; s < p->step_count; s++) {
            if (p->steps[s].forever) return 99;
            extra++;                                    /* each step adds at most one draw per die */
        }
        int dice = p->count + abs(p->advantage);
        total += dice * per * (1 + extra);
    }
    return total;
}

static bool try_every_roll(const qd_roll_t *r, qd_odds_t *out)
{
    static uint32_t pick[MAX_DRAWS], size[MAX_DRAWS];
    static qd_result_t res;
    qd_rng_script_t sc = { pick, size, 0, MAX_DRAWS, 0, false };
    qd_rng_script(&sc);
    uint32_t runs = 0;
    bool ok = true, counting = false;
    for (;;) {
        sc.pos = 0;
        qd_roll_run(r, &res);
        if (sc.over || ++runs > QD_ODDS_EXACT_RUNS) {
            ok = false;
            break;
        }
        double w = 1;
        for (int i = 0; i < sc.pos; i++) w /= size[i];
        acc_add(r, &res, w, &counting);
        int i = sc.pos - 1;                             /* next sequence, odometer style */
        while (i >= 0 && ++pick[i] >= size[i]) i--;
        if (i < 0) break;
        sc.len = (uint16_t)(i + 1);
    }
    qd_rng_script(NULL);
    out->runs = runs;
    out->counting = counting;
    return ok;
}

/* ========================================================================
 * 3. Simulation.
 * ======================================================================== */
static void simulate(const qd_roll_t *r, qd_odds_t *out)
{
    static qd_result_t res;
    uint32_t runs = 0;
    long dice = 0;
    bool counting = false;
    while (runs < QD_ODDS_SAMPLES && dice < QD_ODDS_DICE_BUDGET) {
        qd_roll_run(r, &res);
        acc_add(r, &res, 1, &counting);
        dice += res.rolled;
        runs++;
    }
    out->runs = runs;
    out->counting = counting;
}

/* ========================================================================
 * 1. Convolution, for sums (or success counts) of plain dice.
 * ======================================================================== */
static int32_t contrib(const qd_pool_t *p, int16_t v, const char *label, int16_t lo, int16_t hi);

/* Single-precision sums drift a little over many dice; putting the total
 * back to exactly 1 after each step keeps averages right to ~1 part in 10^6. */
static void normalise(float *a, int n)
{
    double s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    if (s > 0)
        for (int i = 0; i < n; i++) a[i] = (float)(a[i] / s);
}

static float buf_g[QD_ODDS_SPAN], buf_t[QD_ODDS_SPAN], buf_p[QD_ODDS_SPAN], buf_d[QD_ODDS_SPAN];

/* out = a * (b scaled by c): every way a value from a and c times a value
 * from b add up. Returns false if the result is wider than QD_ODDS_SPAN. */
static bool convolve(const float *a, int32_t alo, int an, const float *b, int32_t blo, int bn,
                     int32_t c, float *out, int32_t *olo, int *on)
{
    int64_t lo_b = (int64_t)c * blo, hi_b = (int64_t)c * (blo + bn - 1);
    if (lo_b > hi_b) { int64_t t = lo_b; lo_b = hi_b; hi_b = t; }
    int64_t lo = alo + lo_b, n = (int64_t)an - 1 + (hi_b - lo_b) + 1;
    if (n > QD_ODDS_SPAN || lo < INT32_MIN || lo + n > INT32_MAX)
        return false;
    memset(out, 0, (size_t)n * sizeof(float));
    for (int i = 0; i < an; i++) {
        if (a[i] == 0) continue;
        for (int j = 0; j < bn; j++)
            out[(alo + i) + (int64_t)c * (blo + j) - lo] += a[i] * b[j];
    }
    *olo = (int32_t)lo;
    *on = (int)n;
    return true;
}

/* One die of a pool as a distribution of what it adds: its value, or
 * (counting) the successes it scores minus the failures. */
static bool die_dist(const qd_pool_t *p, bool counting, float *d, int32_t *lo, int *n)
{
    int16_t vlo, vhi;
    qd_pool_range(p, &vlo, &vhi);
    if (counting) {
        *lo = -1;
        *n = 4;
    } else {
        *lo = vlo;
        *n = vhi - vlo + 1;
    }
    if (*n > QD_ODDS_SPAN)
        return false;
    memset(d, 0, (size_t)*n * sizeof(float));
#define ADD(v, prob) do {                                                          \
        int16_t vv = (int16_t)(v);                                                 \
        int32_t at = counting ? contrib(p, vv, "", vlo, vhi) : vv;                 \
        d[at - *lo] += (float)(prob);                                              \
    } while (0)
    if (p->digits) {
        int combos = 1;
        for (int k = 0; k < p->digits; k++) combos *= p->digit_sides[k];
        for (int c = 0; c < combos; c++) {
            int v = 0, x = c;
            for (int k = p->digits - 1, mul = 1; k >= 0; k--, mul *= 10) {
                v += (x % p->digit_sides[k] + 1) * mul;
                x /= p->digit_sides[k];
            }
            ADD(v, 1.0 / combos);
        }
    } else if (p->die) {
        for (uint16_t f = 0; f < p->die->sides; f++) {
            int32_t at = counting ? contrib(p, qd_die_face_value(p->die, f), qd_die_face_label(p->die, f), vlo, vhi)
                                  : qd_die_face_value(p->die, f);
            d[at - *lo] += (float)((double)p->die->faces[f].weight / p->die->total_weight);
        }
    } else {
        int s = p->percentile ? 100 : p->sides;
        for (int v = 1; v <= s; v++) ADD(v, 1.0 / s);
    }
#undef ADD
    /* Trim impossible ends (a success roll without doubles never adds 2),
     * so the ends of every sum really can happen. */
    int a = 0, b = *n;
    while (a < b - 1 && d[a] == 0) a++;
    while (b > a + 1 && d[b - 1] == 0) b--;
    if (a) memmove(d, d + a, (size_t)(b - a) * sizeof(float));
    *lo += a;
    *n = b - a;
    return true;
}

static bool counts_successes(const qd_pool_t *p)
{
    return p->success.op != QD_TEST_NONE || p->failure.op != QD_TEST_NONE;
}

/* What one kept die adds to the successes. */
static int32_t contrib(const qd_pool_t *p, int16_t v, const char *label, int16_t lo, int16_t hi)
{
    int32_t s = 0;
    if (qd_test_face(&p->success, v, label, lo, hi)) {
        s++;
        if (qd_test_face(&p->twice, v, label, lo, hi)) s++;
    }
    if (qd_test_face(&p->failure, v, label, lo, hi)) s--;
    return s;
}

/* The sum as coefficient x pool total + constant, if it is that simple. */
static bool linear(const qd_roll_t *r, int32_t coef[QD_MAX_POOLS], int64_t *constant)
{
    struct { int64_t c[QD_MAX_POOLS]; int64_t k; } st[QD_MAX_ROLL_OPS];
    int top = 0;
    for (int i = 0; i < r->op_count; i++) {
        const qd_op_t *o = &r->ops[i];
        switch (o->kind) {
        case QD_OP_POOL:
            memset(&st[top], 0, sizeof(st[top]));
            st[top++].c[o->arg] = 1;
            break;
        case QD_OP_NUMBER:
            memset(&st[top], 0, sizeof(st[top]));
            st[top++].k = o->arg;
            break;
        case QD_OP_NEG:
            for (int k = 0; k < QD_MAX_POOLS; k++) st[top - 1].c[k] = -st[top - 1].c[k];
            st[top - 1].k = -st[top - 1].k;
            break;
        case QD_OP_ADD: case QD_OP_SUB: {
            int sg = o->kind == QD_OP_ADD ? 1 : -1;
            top--;
            for (int k = 0; k < QD_MAX_POOLS; k++) st[top - 1].c[k] += sg * st[top].c[k];
            st[top - 1].k += sg * st[top].k;
            break;
        }
        case QD_OP_MUL: {
            top--;
            bool a_const = true, b_const = true;
            for (int k = 0; k < QD_MAX_POOLS; k++) {
                a_const &= st[top - 1].c[k] == 0;
                b_const &= st[top].c[k] == 0;
            }
            if (!a_const && !b_const) return false;
            /* (c . pools + k) * m, with m the side that is a plain number */
            int64_t m = a_const ? st[top - 1].k : st[top].k;
            if (a_const) {
                memcpy(st[top - 1].c, st[top].c, sizeof(st[top].c));
                st[top - 1].k = st[top].k;
            }
            for (int k = 0; k < QD_MAX_POOLS; k++) st[top - 1].c[k] *= m;
            st[top - 1].k *= m;
            break;
        }
        default:
            return false;                               /* division, max: not linear */
        }
        if (top > 0) {
            for (int k = 0; k < QD_MAX_POOLS; k++)
                if (st[top - 1].c[k] > 10000 || st[top - 1].c[k] < -10000) return false;
        }
    }
    if (top != 1) return false;
    for (int k = 0; k < QD_MAX_POOLS; k++) coef[k] = (int32_t)st[0].c[k];
    *constant = st[0].k;
    return true;
}

static bool convolution(const qd_roll_t *r, qd_odds_t *out)
{
    if (r->sys.id)
        return false;
    bool counting = false;
    for (int k = 0; k < r->pool_count; k++) {
        const qd_pool_t *p = &r->pools[k];
        if (p->advantage || p->step_count || p->keep != QD_KEEP_ALL || (p->percentile && p->pct_bonus))
            return false;
        counting |= counts_successes(p);
    }
    int32_t coef[QD_MAX_POOLS];
    int64_t constant = 0;
    if (counting) {
        for (int k = 0; k < QD_MAX_POOLS; k++) coef[k] = 1;
    } else if (!linear(r, coef, &constant)) {
        return false;
    }

    /* G starts as "certainly 0" and takes in each pool. */
    int32_t glo = 0, plo, dlo, tlo;
    int gn = 1, pn, dn, tn;
    buf_g[0] = 1;
    for (int k = 0; k < r->pool_count; k++) {
        const qd_pool_t *p = &r->pools[k];
        if (coef[k] == 0 || (counting && !counts_successes(p)))
            continue;
        if (!die_dist(p, counting, buf_d, &dlo, &dn))
            return false;
        /* P = the pool: one die convolved with itself count times */
        buf_p[0] = 1; plo = 0; pn = 1;
        for (int i = 0; i < p->count; i++) {
            if (!convolve(buf_p, plo, pn, buf_d, dlo, dn, 1, buf_t, &tlo, &tn))
                return false;
            memcpy(buf_p, buf_t, (size_t)tn * sizeof(float));
            plo = tlo; pn = tn;
            normalise(buf_p, pn);
        }
        if (!convolve(buf_g, glo, gn, buf_p, plo, pn, coef[k], buf_t, &tlo, &tn))
            return false;
        memcpy(buf_g, buf_t, (size_t)tn * sizeof(float));
        glo = tlo; gn = tn;
        normalise(buf_g, gn);
    }
    glo += (int32_t)constant;

    /* Read it out. */
    double m1 = 0, m2 = 0, pass = 0;
    bool any = false;
    out->counting = counting;
    for (int i = 0; i < gn; i++) {
        double pr = buf_g[i], v = (double)glo + i;
        if (pr <= 0) continue;
        m1 += pr * v;
        m2 += pr * v * v;
        if (!any) out->lo = glo + i;
        out->hi = glo + i;
        any = true;
        if (r->sys.has_target && glo + i >= r->sys.target) pass += pr;
    }
    out->mean = m1;
    out->sd = m2 - m1 * m1 > 0 ? sqrt(m2 - m1 * m1) : 0;
    /* The ends of the range are always possible (all lowest, all highest
     * faces) even when their chance is too small for a float: 6^-100. */
    out->lo = glo;
    out->hi = glo + gn - 1;
    if (r->sys.has_target) {
        out->judged = true;
        out->pass = pass;
        out->level[QD_LVL_PASS + 4] = pass;
        out->level[QD_LVL_FAIL + 4] = 1 - pass;
    }
    /* the window: everything if it fits, else centred on the average */
    int from = 0;
    if (gn > QD_ODDS_BINS) {
        from = (int)(out->mean - glo) - QD_ODDS_BINS / 2;
        if (from < 0) from = 0;
        if (from > gn - QD_ODDS_BINS) from = gn - QD_ODDS_BINS;
    }
    out->first = glo + from;
    out->bins = (uint16_t)(gn - from < QD_ODDS_BINS ? gn - from : QD_ODDS_BINS);
    for (int i = 0; i < gn; i++) {
        if (i < from) out->below += buf_g[i];
        else if (i >= from + out->bins) out->above += buf_g[i];
        else out->dist[i - from] = buf_g[i];
    }
    out->method = QD_ODDS_EXACT_SUM;
    out->exact = true;
    return true;
}

/* ========================================================================
 * Putting it together
 * ======================================================================== */
qd_err_t qd_odds(const qd_roll_t *r, qd_odds_t *out)
{
    memset(out, 0, sizeof(*out));
    qd_err_t e = qd_roll_check(r);
    if (e)
        return e;
    if (convolution(r, out))
        return QD_OK;

    /* The others roll the dice, so: keep the generator and stats as they are. */
    uint32_t saved[4];
    qd_rng_save(saved);
    void *ctx;
    qd_observer_t watcher = qd_roll_observe(NULL, NULL, &ctx);

    memset(&acc, 0, sizeof(acc));
    choose_window(r);
    int32_t first = acc.first;
    bool exact = false;
    if (log_rolls(r) <= log10((double)QD_ODDS_EXACT_RUNS) + 0.01) {
        exact = try_every_roll(r, out);
        if (!exact) {                                   /* too many after all: start again */
            memset(&acc, 0, sizeof(acc));
            acc.first = first;
        }
    }
    if (!exact)
        simulate(r, out);
    acc_finish(out);
    out->exact = exact;
    out->method = exact ? QD_ODDS_EXACT_ALL : QD_ODDS_SIMULATED;

    qd_roll_observe(watcher, ctx, NULL);
    qd_rng_restore(saved);
    return QD_OK;
}

double qd_odds_at_least(const qd_odds_t *o, int32_t v)
{
    double p = o->above;
    for (int i = 0; i < o->bins; i++)
        if (o->first + i >= v) p += o->dist[i];
    if (v <= o->first) p += o->below;
    return p;
}

double qd_odds_exactly(const qd_odds_t *o, int32_t v)
{
    int64_t i = (int64_t)v - o->first;
    return i >= 0 && i < o->bins ? o->dist[i] : 0;
}
