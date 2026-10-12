/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_dice.c
 * @brief   Standard, custom and percentile dice.
 *
 * ORDER OF RANDOM DRAWS (tests/host/rng_reference.py copies these, so the
 * PC reference and the device produce the same rolls from the same seed):
 *   standard die     range(sides)
 *   custom die       range(total_weight), then walk the cumulative weights
 *                    (a die with all weights 1 uses range(sides) directly)
 *   percentile       units d10 first, then each tens d10 in order
 */
#include <string.h>

#include "qd_dice.h"
#include "qd_rng.h"

bool qd_die_standard(qd_die_t *die, uint16_t sides)
{
    if (!sides || sides > 32767)
        return false;
    memset(die, 0, sizeof(*die));
    die->sides = sides;
    return true;
}

bool qd_die_custom(qd_die_t *die, const qd_face_t *faces, uint16_t count)
{
    if (!count || count > QD_MAX_FACES)
        return false;
    memset(die, 0, sizeof(*die));
    die->sides = count;
    die->custom = true;
    for (uint16_t i = 0; i < count; i++) {
        die->faces[i] = faces[i];
        if (die->faces[i].weight == 0)
            die->faces[i].weight = 1;
        die->faces[i].label[QD_MAX_LABEL] = '\0';
        die->total_weight += die->faces[i].weight;
        if (die->faces[i].weight != 1)
            die->weighted = true;
    }
    return true;
}

void qd_die_fudge(qd_die_t *die)
{
    static const qd_face_t fudge[6] = {
        { -1, 1, "-" }, { -1, 1, "-" }, { 0, 1, " " }, { 0, 1, " " }, { 1, 1, "+" }, { 1, 1, "+" },
    };
    qd_die_custom(die, fudge, 6);
}

qd_face_roll_t qd_die_roll(const qd_die_t *die)
{
    qd_face_roll_t r = { 0, 0 };
    if (!die->weighted) {
        r.face = (uint16_t)qd_rng_range(die->sides);
    } else {
        /* Each face owns `weight` tickets in a row; draw a ticket and find
         * whose it is. At most 30 faces, so a simple walk is plenty fast. */
        uint32_t ticket = qd_rng_range(die->total_weight);
        while (ticket >= die->faces[r.face].weight) {
            ticket -= die->faces[r.face].weight;
            r.face++;
        }
    }
    r.value = qd_die_face_value(die, r.face);
    return r;
}

int16_t qd_die_face_value(const qd_die_t *die, uint16_t face)
{
    if (face >= die->sides)
        return 0;
    return die->custom ? die->faces[face].value : (int16_t)(face + 1);
}

const char *qd_die_face_label(const qd_die_t *die, uint16_t face)
{
    return (die->custom && face < die->sides) ? die->faces[face].label : "";
}

uint32_t qd_die_face_ppm(const qd_die_t *die, uint16_t face)
{
    if (face >= die->sides)
        return 0;
    if (!die->custom)
        return (uint32_t)(1000000u / die->sides);
    return (uint32_t)((uint64_t)die->faces[face].weight * 1000000u / die->total_weight);
}

/* --- Composite dice ------------------------------------------------------- */
/* What a tens die and the units die read as together: 1..100. */
static uint8_t percentile_value(uint8_t tens, uint8_t units)
{
    uint8_t v = (uint8_t)(tens * 10 + units);
    return v ? v : 100;                 /* 00 + 0 is 100, not 0 */
}

uint8_t qd_roll_percentile(int bonus, qd_percentile_t *out)
{
    qd_percentile_t p;
    if (bonus > QD_MAX_BONUS_DICE)  bonus = QD_MAX_BONUS_DICE;
    if (bonus < -QD_MAX_BONUS_DICE) bonus = -QD_MAX_BONUS_DICE;
    p.bonus = (int8_t)bonus;
    p.units = (uint8_t)qd_rng_range(10);
    p.tens_count = (uint8_t)(1 + (bonus < 0 ? -bonus : bonus));
    p.kept = 0;
    for (uint8_t i = 0; i < p.tens_count; i++) {
        p.tens[i] = (uint8_t)qd_rng_range(10);
        /* Bonus keeps the lowest (best) result, penalty the highest. Ties
         * keep the first die rolled. */
        uint8_t now = percentile_value(p.tens[i], p.units);
        uint8_t best = percentile_value(p.tens[p.kept], p.units);
        if ((bonus > 0 && now < best) || (bonus < 0 && now > best))
            p.kept = i;
    }
    p.result = percentile_value(p.tens[p.kept], p.units);
    if (out)
        *out = p;
    return p.result;
}
