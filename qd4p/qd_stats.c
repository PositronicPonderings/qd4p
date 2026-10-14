/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_stats.c
 * @brief   Stats for nerds: tallies per kind of die and a fairness check.
 *
 * HOW IT WORKS
 *   qd_stats_watch() sets the roll engine's observer (qd_roll_observe) to
 *   a function that tallies each die it's told about into its kind: the
 *   count, the sum, the sum of squares (for the spread), the lowest and
 *   highest, and how often each face came up.
 *
 *   The report compares those with a fair die's: the expected average and
 *   spread come from the face values and their chances (qd_die_face_ppm),
 *   so a weighted custom die is compared with what its weights promise.
 *   The fairness check is Pearson's chi-square: the sum, over the faces,
 *   of (seen - expected)^2 / expected. A fair die rarely scores high; the
 *   line is drawn where it scores higher only 1 time in 1,000.
 */
#include <math.h>
#include <string.h>

#include "qd_roll.h"
#include "qd_stats.h"

void qd_stats_clear(qd_stats_t *s)
{
    memset(s, 0, sizeof(*s));
}

static void observer(uint16_t sides, const qd_die_t *die, uint16_t face, int16_t value, void *ctx)
{
    qd_stats_add((qd_stats_t *)ctx, sides, die, face, value);
}

void qd_stats_watch(qd_stats_t *s)
{
    qd_roll_observe(s ? observer : NULL, s, NULL);
}

static qd_die_stats_t *find(qd_stats_t *s, uint16_t sides, const qd_die_t *die)
{
    for (int i = 0; i < s->kinds; i++)
        if (s->kind[i].sides == sides && s->kind[i].die == die)
            return &s->kind[i];
    return NULL;
}

const qd_die_stats_t *qd_stats_find(const qd_stats_t *s, uint16_t sides, const qd_die_t *die)
{
    return find((qd_stats_t *)s, sides, die);
}

void qd_stats_add(qd_stats_t *s, uint16_t sides, const qd_die_t *die, uint16_t face, int16_t value)
{
    qd_die_stats_t *k = find(s, sides, die);
    if (!k) {
        if (s->kinds >= QD_STATS_KINDS) {
            s->untracked++;
            return;
        }
        k = &s->kind[s->kinds++];
        memset(k, 0, sizeof(*k));
        k->sides = sides;
        k->die = die;
        k->faces = sides <= QD_STATS_FACES;
        k->min = k->max = value;
    }
    k->count++;
    k->sum += value;
    k->sum_sq += (uint64_t)((int64_t)value * value);
    if (value < k->min) k->min = value;
    if (value > k->max) k->max = value;
    if (k->faces && face < QD_STATS_FACES)
        k->face[face]++;
}

double qd_chi_limit(int df)
{
    /* The table's values up to 30 (a d31 or less); past that the
     * Wilson-Hilferty approximation is within 0.3%. */
    static const float table[31] = { 0, 10.828f, 13.816f, 16.266f, 18.467f, 20.515f, 22.458f,
        24.322f, 26.124f, 27.877f, 29.588f, 31.264f, 32.909f, 34.528f, 36.123f, 37.697f, 39.252f,
        40.790f, 42.312f, 43.820f, 45.315f, 46.797f, 48.268f, 49.728f, 51.179f, 52.620f, 54.052f,
        55.476f, 56.892f, 58.301f, 59.703f };
    if (df < 1) return 0;
    if (df <= 30) return table[df];
    double h = 2.0 / (9.0 * df);
    double t = 1.0 - h + 3.090 * sqrt(h);           /* 3.090: the normal 0.999 point */
    return df * t * t * t;
}

/* A face's value and its share of rolls on a fair (or properly weighted) die. */
static double face_value(const qd_die_stats_t *k, int f)
{
    return k->die ? qd_die_face_value(k->die, (uint16_t)f) : f + 1;
}

static double face_share(const qd_die_stats_t *k, int f)
{
    return k->die ? qd_die_face_ppm(k->die, (uint16_t)f) / 1e6 : 1.0 / k->sides;
}

void qd_stats_report(const qd_die_stats_t *k, qd_stats_report_t *r)
{
    memset(r, 0, sizeof(*r));
    if (!k->count)
        return;
    double n = k->count;
    r->mean = k->sum / n;
    double var = k->sum_sq / n - r->mean * r->mean;
    r->sd = var > 0 ? sqrt(var) : 0;

    /* What a fair die gives: mean and spread from its faces. A big standard
     * die's are worked out directly rather than face by face. */
    if (!k->die) {
        double s = k->sides;
        r->expect_mean = (s + 1) / 2;
        r->expect_sd = sqrt((s * s - 1) / 12);
    } else {
        double m = 0, m2 = 0;
        for (int f = 0; f < k->sides; f++) {
            double v = face_value(k, f), p = face_share(k, f);
            m += v * p;
            m2 += v * v * p;
        }
        r->expect_mean = m;
        r->expect_sd = m2 - m * m > 0 ? sqrt(m2 - m * m) : 0;
    }

    if (!k->faces) {
        r->verdict = QD_STATS_NO_FACES;
        return;
    }
    /* The top and bottom faces by value: "natural 20s" and "natural 1s". */
    int top = 0, bottom = 0;
    for (int f = 1; f < k->sides; f++) {
        if (face_value(k, f) > face_value(k, top)) top = f;
        if (face_value(k, f) < face_value(k, bottom)) bottom = f;
    }
    for (int f = 0; f < k->sides; f++) {
        if (face_value(k, f) == face_value(k, top)) r->highest += k->face[f];
        if (face_value(k, f) == face_value(k, bottom)) r->lowest += k->face[f];
    }

    /* Chi-square over the faces, once each face expects at least 5 rolls. */
    double fewest = 1e300;
    int used = 0;
    for (int f = 0; f < k->sides; f++) {
        double e = n * face_share(k, f);
        if (e <= 0) continue;
        if (e < fewest) fewest = e;
        r->chi += (k->face[f] - e) * (k->face[f] - e) / e;
        used++;
    }
    r->df = used - 1;
    r->limit = qd_chi_limit(r->df);
    if (r->df < 1 || fewest < 5)
        r->verdict = QD_STATS_TOO_FEW;
    else
        r->verdict = r->chi > r->limit ? QD_STATS_ODD : QD_STATS_FAIR;
}
