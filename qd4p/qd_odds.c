/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_odds.c
 * @brief   Odds: the chance of every result of a roll, worked out four
 *          ways. qd_odds.h says which rolls use which.
 *
 * HOW THIS FILE IS LAID OUT
 *   1. Convolution (exact sums). Each die is turned into a little table of
 *      "what it adds, and how likely": a d6 adds 1..6 at 1/6 each. Two
 *      tables combine into the table of their sum by trying every pair of
 *      entries; the third die is combined with that, and so on. The tables
 *      live in four static float buffers of QD_ODDS_SPAN entries.
 *   2. Trying every roll (exact anything). The generator is SCRIPTED
 *      (qd_rng_script): instead of random numbers it returns chosen ones,
 *      and the roll is run once for every possible sequence of them, like
 *      an odometer turning over. Each run's chance is the product of 1/n
 *      for each draw of n it made. The engine does all the work, so keep,
 *      advantage, rerolls and game outcomes come for free.
 *   3. The bell curve (big plain sums). Each die's average and variance
 *      add up exactly; the chances come from the normal distribution.
 *   4. Simulation (everything else): roll it QD_ODDS_SAMPLES times and
 *      count.
 *   Gathering (acc_*) is shared by 2 and 4: a tally of values, passes and
 *   outcome levels, each weighted by its chance.
 *
 * NOTHING CHANGES OUTSIDE: the generator's state is saved first and put
 * back after, and the stats observer is switched off while working, so the
 * rolls after asking for odds are the rolls there would have been anyway.
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
    int32_t first, width;
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
    int64_t off = (int64_t)v - acc.first;
    int64_t i = off >= 0 ? off / acc.width : -1;
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
    out->width = acc.width;
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
        out->first += a * out->width;
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
    int64_t span = (int64_t)hi - lo + 1;
    if (span * 2 <= QD_ODDS_BINS) {
        /* one entry per value, with room either side for rarer results */
        int64_t mid = ((int64_t)lo + hi) / 2;
        acc.width = 1;
        acc.first = (int32_t)(mid - QD_ODDS_BINS / 2);
        if (lo - acc.first > 8)
            acc.first = lo - 8 < acc.first ? acc.first : lo - 8;
    } else {
        /* grouped, covering twice what the quick look saw */
        acc.width = (int32_t)((2 * span + QD_ODDS_BINS - 1) / QD_ODDS_BINS);
        acc.first = (int32_t)(lo - span / 2);
    }
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
    /* The sum, worked out as a little stack machine: each entry is
     * "coefficients of each pool, plus a constant". Static, not on the
     * stack: at the defaults it's 1.7 KB, most of a Pico's 2 KB stack. */
    static struct { int64_t c[QD_MAX_POOLS]; int64_t k; } st[QD_MAX_ROLL_OPS];
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

/* Is the roll a plain sum (or success count): no system, no keep,
 * advantage, steps or bonus dice, and + - * by numbers only? */
static bool plain_sum(const qd_roll_t *r, int32_t coef[QD_MAX_POOLS], int64_t *constant, bool *counting)
{
    if (r->sys.id)
        return false;
    *counting = false;
    *constant = 0;
    for (int k = 0; k < r->pool_count; k++) {
        const qd_pool_t *p = &r->pools[k];
        if (p->advantage || p->step_count || p->keep != QD_KEEP_ALL || (p->percentile && p->pct_bonus))
            return false;
        *counting |= counts_successes(p);
    }
    if (*counting) {
        for (int k = 0; k < QD_MAX_POOLS; k++) coef[k] = 1;
        return true;
    }
    return linear(r, coef, constant);
}

/* Spread a value range of chances over dist, grouping if it is wide. */
static void fill_dist(qd_odds_t *out, const float *g, int32_t glo, int gn)
{
    int32_t w = gn <= QD_ODDS_BINS ? 1 : (gn + QD_ODDS_BINS - 1) / QD_ODDS_BINS;
    out->first = glo;
    out->width = w;
    out->bins = (uint16_t)((gn + w - 1) / w);
    memset(out->dist, 0, sizeof(out->dist));
    for (int i = 0; i < gn; i++) out->dist[i / w] += g[i];
}

static bool convolution(const qd_roll_t *r, qd_odds_t *out, const int32_t *coef, int64_t constant,
                        bool counting)
{
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
    fill_dist(out, buf_g, glo, gn);
    out->method = QD_ODDS_EXACT_SUM;
    out->exact = true;
    return true;
}

/* ========================================================================
 * 3. The bell curve, for big plain sums too wide to convolve. The average
 *    and spread are exact (each die's, added up); for 20 or more dice the
 *    normal distribution gives the chances to within a fraction of a
 *    percent.
 * ======================================================================== */
static double phi(double z)
{
    return 0.5 * erfc(-z / sqrt(2.0));
}

/* One die's average, variance, lowest and highest contribution. */
static void die_moments(const qd_pool_t *p, bool counting, double *m, double *v, double *lo, double *hi)
{
    int16_t vlo, vhi;
    qd_pool_range(p, &vlo, &vhi);
    double s1 = 0, s2 = 0, n = 0;
    *lo = 1e300;
    *hi = -1e300;
#define FACE(val, label, prob) do {                                               \
        double x = counting ? contrib(p, (int16_t)(val), (label), vlo, vhi) : (val); \
        s1 += (prob) * x; s2 += (prob) * x * x; n += (prob);                      \
        if (x < *lo) *lo = x;                                                     \
        if (x > *hi) *hi = x;                                                     \
    } while (0)
    if (!counting && !p->die && !p->digits) {
        /* standard dice (and d%): no need to visit every face */
        double s = p->percentile ? 100 : p->sides;
        *m = (s + 1) / 2;
        *v = (s * s - 1) / 12;
        *lo = 1;
        *hi = s;
        return;
    }
    if (p->digits) {
        int combos = 1;
        for (int k = 0; k < p->digits; k++) combos *= p->digit_sides[k];
        for (int c = 0; c < combos; c++) {
            int val = 0, x = c;
            for (int k = p->digits - 1, mul = 1; k >= 0; k--, mul *= 10) {
                val += (x % p->digit_sides[k] + 1) * mul;
                x /= p->digit_sides[k];
            }
            FACE(val, "", 1.0 / combos);
        }
    } else if (p->die) {
        for (uint16_t f = 0; f < p->die->sides; f++)
            FACE(qd_die_face_value(p->die, f), qd_die_face_label(p->die, f),
                 (double)p->die->faces[f].weight / p->die->total_weight);
    } else {
        int s = p->percentile ? 100 : p->sides;
        for (int val = 1; val <= s; val++) FACE(val, "", 1.0 / s);
    }
#undef FACE
    *m = s1 / n;
    *v = s2 / n - *m * *m;
}

static bool normal(const qd_roll_t *r, qd_odds_t *out, const int32_t *coef, int64_t constant, bool counting)
{
    double mean = (double)constant, var = 0, lo = (double)constant, hi = (double)constant;
    long dice = 0;
    for (int k = 0; k < r->pool_count; k++) {
        const qd_pool_t *p = &r->pools[k];
        if (coef[k] == 0 || (counting && !counts_successes(p)))
            continue;
        double m, v, a, b;
        die_moments(p, counting, &m, &v, &a, &b);
        double c = coef[k], n = p->count;
        mean += c * n * m;
        var += c * c * n * v;
        lo += c >= 0 ? c * n * a : c * n * b;
        hi += c >= 0 ? c * n * b : c * n * a;
        dice += p->count;
    }
    if (dice < 20 || var <= 0)
        return false;
    double sd = sqrt(var);
    out->counting = counting;
    out->mean = mean;
    out->sd = sd;
    out->lo = (int32_t)lo;
    out->hi = (int32_t)hi;

    /* The chart: mean +-5 spreads (all but 1 in 3 million), grouped. */
    double from = mean - 5 * sd, to = mean + 5 * sd;
    if (from < lo) from = lo;
    if (to > hi) to = hi;
    int64_t first = (int64_t)floor(from), span = (int64_t)ceil(to) - first + 1;
    int64_t w = (span + QD_ODDS_BINS - 1) / QD_ODDS_BINS;
    if (w < 1) w = 1;
    out->first = (int32_t)first;
    out->width = (int32_t)w;
    out->bins = (uint16_t)((span + w - 1) / w);
    for (int i = 0; i < out->bins; i++) {
        double a = first + (int64_t)i * w - 0.5, b = first + (int64_t)(i + 1) * w - 0.5;
        out->dist[i] = (float)(phi((b - mean) / sd) - phi((a - mean) / sd));
    }
    out->below = phi((first - 0.5 - mean) / sd);
    out->above = 1 - phi((first + (int64_t)out->bins * w - 0.5 - mean) / sd);
    if (r->sys.has_target) {
        double t = r->sys.target;
        double pass = t <= lo ? 1 : t > hi ? 0 : 1 - phi((t - 0.5 - mean) / sd);
        out->judged = true;
        out->pass = pass;
        out->level[QD_LVL_PASS + 4] = pass;
        out->level[QD_LVL_FAIL + 4] = 1 - pass;
    }
    out->method = QD_ODDS_NORMAL;
    out->exact = false;
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
    int32_t coef[QD_MAX_POOLS];
    int64_t constant;
    bool counting;
    if (plain_sum(r, coef, &constant, &counting)) {
        if (convolution(r, out, coef, constant, counting))
            return QD_OK;
        memset(out, 0, sizeof(*out));
        if (normal(r, out, coef, constant, counting))
            return QD_OK;
        memset(out, 0, sizeof(*out));
    }

    /* The others roll the dice, so: keep the generator and stats as they are. */
    uint32_t saved[4];
    qd_rng_save(saved);
    void *ctx;
    qd_observer_t watcher = qd_roll_observe(NULL, NULL, &ctx);

    memset(&acc, 0, sizeof(acc));
    choose_window(r);
    int32_t first = acc.first, width = acc.width;
    bool exact = false;
    if (log_rolls(r) <= log10((double)QD_ODDS_EXACT_RUNS) + 0.01) {
        exact = try_every_roll(r, out);
        if (!exact) {                                   /* too many after all: start again */
            memset(&acc, 0, sizeof(acc));
            acc.first = first;
            acc.width = width;
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
    int32_t w = o->width > 0 ? o->width : 1;
    double p = o->above;
    if (v <= o->first) p += o->below;
    for (int i = 0; i < o->bins; i++) {
        int64_t start = (int64_t)o->first + (int64_t)i * w, end = start + w - 1;
        if (start >= v) p += o->dist[i];
        else if (end >= v) p += o->dist[i] * (double)(end - v + 1) / w;   /* part of a group */
    }
    return p;
}

double qd_odds_exactly(const qd_odds_t *o, int32_t v)
{
    int32_t w = o->width > 0 ? o->width : 1;
    int64_t off = (int64_t)v - o->first;
    if (off < 0) return 0;
    int64_t i = off / w;
    return i < o->bins ? o->dist[i] / w : 0;
}
