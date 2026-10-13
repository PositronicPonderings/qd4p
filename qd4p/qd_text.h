/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_text.h
 * @brief   Dice in text, and rolls that pick from ranges: the building
 *          blocks for random tables, generators and card text.
 *
 *     char line[80];
 *     qd_expand("{1d4+1} wolves and {2d6*10} gp", line, sizeof(line), NULL);
 *     // "3 wolves and 70 gp"
 *
 *     static const qd_range_t ranges[] = { {2, 6}, {7, 7}, {8, 12} };
 *     int32_t v;
 *     int row = qd_pick_range("2d6", ranges, 3, &v);    // v = 9, row = 2
 *
 * QD4P doesn't define what a random table is - how it is stored, written
 * or shown - because every app wants something a little different
 * (columns, weights, no repeats). These are the pieces they all share. A
 * complete table system built from them, with a file format and a PC
 * tool, is in examples/tables/.
 */
#ifndef QD_TEXT_H
#define QD_TEXT_H

#include <stdbool.h>
#include <stdint.h>

#include "qd_config.h"
#include "qd_roll.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Roll notation written as text ("2d6", "coc 50", "6d10 t8"): the value
 *  is the total, or the successes for a roll that counts them. Returns
 *  false if the text isn't a roll. Up to 63 characters. */
bool qd_roll_text(const char *notation, int32_t *value);

/** The same for text that isn't 0-terminated: `n` characters. */
bool qd_roll_textn(const char *notation, int n, int32_t *value);

/* --- Ranges ----------------------------------------------------------------- */

/** A range of values, both ends included. Open ends: INT32_MIN, INT32_MAX. */
typedef struct {
    int32_t lo, hi;
} qd_range_t;

/** Which range `v` falls in (the first that holds it), or -1 for none. */
int qd_range_find(const qd_range_t *ranges, int n, int32_t v);

/** Roll `notation` and find its range. Returns the index, -1 if the roll
 *  falls in no range, or -2 if the notation can't be read. The value
 *  rolled goes in *value either way (0 if unreadable). */
int qd_pick_range(const char *notation, const qd_range_t *ranges, int n, int32_t *value);

/* --- Dice in text ------------------------------------------------------------- */

/** One inline roll made by qd_expand. */
typedef struct {
    int32_t value;
    char    what[24];                   /**< the notation, as written */
} qd_expand_roll_t;

enum {
    QD_EXPAND_CUT   = 1 << 0,           /**< the output didn't fit; it was cut */
    QD_EXPAND_BAD   = 1 << 1,           /**< a {roll} couldn't be read: shown as {?} */
    QD_EXPAND_FULL  = 1 << 2,           /**< more rolls than QD_EXPAND_ROLLS; the rest not kept */
};

/** What qd_expand did: its flags and its rolls (the working). */
typedef struct {
    uint16_t         flags;
    uint8_t          rolls;
    qd_expand_roll_t roll[QD_EXPAND_ROLLS];
} qd_expand_info_t;

/** An app's own [brackets]: called with the text between [ and ] (n
 *  characters, not 0-terminated). Write the replacement into out (room
 *  characters at most) and return how many were written, or -1 to leave
 *  the brackets as they are. A random-table app uses this for [Table]
 *  links. */
typedef int (*qd_expand_hook_t)(const char *name, int n, char *out, int room, void *ctx);

/** Copy `text` into `out` (size bytes, always 0-terminated), rolling each
 *  {notation} and putting its value in its place. "\{", "\[" and "\\"
 *  give a literal { [ or \. [brackets] are left alone unless `hook` is
 *  given (qd_expand_ex). `info` may be NULL. Returns the length written. */
int qd_expand(const char *text, char *out, int size, qd_expand_info_t *info);

int qd_expand_ex(const char *text, char *out, int size, qd_expand_info_t *info,
                 qd_expand_hook_t hook, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* QD_TEXT_H */
