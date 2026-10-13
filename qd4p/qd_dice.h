/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_dice.h
 * @brief   Dice: standard, custom (weighted, labelled) and percentile.
 *
 * QUICK START
 *
 *     qd_die_t d20;
 *     qd_die_standard(&d20, 20);
 *     qd_face_roll_t r = qd_die_roll(&d20);       // r.value 1..20, r.face 0..19
 *
 *     static const qd_face_t loaded_faces[] = {
 *         {1, 1, ""}, {2, 1, ""}, {3, 1, ""}, {4, 1, ""}, {5, 1, ""}, {6, 3, ""} };
 *     qd_die_t loaded;
 *     qd_die_custom(&loaded, loaded_faces, 6);     // a six three times as likely
 *
 *     qd_percentile_t p;
 *     qd_roll_percentile(2, &p);                   // CoC with two bonus dice
 *     // p.result 1..100, p.tens[] every tens die rolled, p.kept which one counted
 *
 * FACES, NOT JUST VALUES
 *   Every roll reports which face came up (0 = the first face) as well as
 *   its value. A die can have two faces showing 3, or faces whose value is
 *   0 but whose label is "+"; the face index keeps them apart, which is
 *   what labels on screen and fairness statistics need.
 *
 * WEIGHTS
 *   A face's weight is how many "tickets" it has in the draw: weight 3 is
 *   three times as likely as weight 1. All weights 1 is a fair die.
 */
#ifndef QD_DICE_H
#define QD_DICE_H

#include <stdbool.h>
#include <stdint.h>

#include "qd_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One face of a custom die. */
typedef struct {
    int16_t value;                      /**< what the face counts as */
    uint8_t weight;                     /**< relative chance; 0 is treated as 1 */
    char    label[QD_MAX_LABEL + 1];    /**< shown instead of the value if not "" */
} qd_face_t;

/** A die. Standard dice store only their side count; custom dice carry
 *  their faces. Fill one in with qd_die_standard/custom/fudge. */
typedef struct {
    uint16_t  id;                       /**< the app's id for it (0 = none) */
    uint16_t  sides;                    /**< number of faces */
    bool      custom;                   /**< false: faces are 1..sides */
    bool      weighted;                 /**< true if any weight isn't 1 */
    uint16_t  total_weight;             /**< sum of weights (custom dice) */
    qd_face_t faces[QD_MAX_FACES];      /**< custom dice only */
} qd_die_t;

/** The outcome of rolling one die. */
typedef struct {
    uint16_t face;                      /**< which face, 0 .. sides-1 */
    int16_t  value;                     /**< its value */
} qd_face_roll_t;

/** A standard die with faces 1..sides (sides 1 to 32767, so every value
 *  fits in an int16_t; DCC's d3, d5, d7, d14, d16, d24 and d30 are just
 *  standard dice). Returns false if sides is 0 or too big. */
bool qd_die_standard(qd_die_t *die, uint16_t sides);

/** A custom die from up to QD_MAX_FACES faces (copied in). Returns false if
 *  count is 0 or too many. */
bool qd_die_custom(qd_die_t *die, const qd_face_t *faces, uint16_t count);

/** A Fudge/FATE die: -1, 0, +1 twice each, labelled "-", " ", "+". */
void qd_die_fudge(qd_die_t *die);

/** One shared, ready-made Fudge die (what "dF" and the FATE system use). */
const qd_die_t *qd_fudge_die(void);

/** Roll one die. */
qd_face_roll_t qd_die_roll(const qd_die_t *die);

/** The value of a face (face index 0 .. sides-1). */
int16_t qd_die_face_value(const qd_die_t *die, uint16_t face);

/** The label of a face: the custom label, or "" if it has none. */
const char *qd_die_face_label(const qd_die_t *die, uint16_t face);

/** The chance of a face in parts per million, for stats and odds. */
uint32_t qd_die_face_ppm(const qd_die_t *die, uint16_t face);

/* --- Composite dice ------------------------------------------------------- */

/* Digit dice (dd66: a tens d6 and a units d6 read as 11..66) are a kind
 * of pool in the roll engine: see qd_roll_digits() in qd_roll.h. */

/** Percentile dice, with Call of Cthulhu bonus and penalty dice. */
#define QD_MAX_BONUS_DICE 3

typedef struct {
    uint8_t units;                      /**< the units die, 0..9 */
    uint8_t tens_count;                 /**< tens dice rolled, 1 + |bonus| */
    uint8_t tens[1 + QD_MAX_BONUS_DICE];/**< each tens die, 0..9 (shown as 00..90) */
    uint8_t kept;                       /**< index into tens[] of the one that counted */
    int8_t  bonus;                      /**< net bonus (+) or penalty (-) dice used */
    uint8_t result;                     /**< 1..100 (00 + 0 reads as 100) */
} qd_percentile_t;

/** Roll d100 as a units die and tens dice. `bonus` > 0 adds that many
 *  bonus dice (keep the best result), < 0 penalty dice (keep the worst);
 *  bonus and penalty dice cancel, so pass (bonus - penalty). Clamped to
 *  -3..+3. Returns the result 1..100. */
uint8_t qd_roll_percentile(int bonus, qd_percentile_t *out);

#ifdef __cplusplus
}
#endif

#endif /* QD_DICE_H */
