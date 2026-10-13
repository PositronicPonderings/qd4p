/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    table.h
 * @brief   An example random-table system built on QD4P's building blocks
 *          (qd_text.h). Not part of the library: copy it into an app and
 *          change it to suit - columns, weights, no-repeat draws ...
 *
 *     tbl_set_t tables;
 *     tbl_result_t r;
 *     tbl_open(&tables, asset_bytes, asset_size);         // checks it once
 *     tbl_roll(&tables, tbl_find(&tables, "Forest encounters"), &r);
 *     // r.text "Bandits, led by Grey Meg, one-eyed and patient"
 *     // r.step[] the working
 *
 * Tables are written as text (README.md here) and turned into an asset by
 * tables.py on a PC; the asset is read where it lies, in flash. In an
 * entry: {1d4+1} rolls dice (qd_expand), [Weather] rolls on another table
 * (a qd_expand hook), and an entry that is only "Roll twice" / "Roll N
 * times" rolls again on the same table.
 */
#ifndef TABLE_H
#define TABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "qd4p.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Limits. Each can be set from the build (-DTBL_DEPTH=6) instead.
 *
 * TBL_DEPTH is how deep [links] may go: a table linking to a table linking
 * to a table is 3. Each level costs 216 bytes of stack and 452 of RAM (its
 * working, kept in a static array). At 4, the deepest roll needs about
 * 1.6 KB of stack, inside the 2 KB the Pico SDK gives main(); 2.1 KB only
 * if a table's dice also nest brackets eight deep. Real tables rarely link
 * past 3 (tests/size/stack.py works these figures out). */
#ifndef TBL_DEPTH
#define TBL_DEPTH   4                   /**< tables inside tables */
#endif
#ifndef TBL_TEXT
#define TBL_TEXT    320                 /**< characters in a result */
#endif
#ifndef TBL_STEPS
#define TBL_STEPS   24                  /**< rolls kept as the working */
#endif

/** An open table asset. Points at the bytes; copies nothing. */
typedef struct {
    const uint8_t *data;
    uint32_t       size;
    uint16_t       count;               /**< tables in it */
} tbl_set_t;

/** Check an asset (every offset and string in it) and get it ready.
 *  Returns false if it isn't a valid table asset. */
bool tbl_open(tbl_set_t *t, const void *data, uint32_t size);

/** A table's index by name (any case), or -1. */
int tbl_find(const tbl_set_t *t, const char *name);

/** A table's name, its dice ("2d6") and how many entries it has. */
const char *tbl_name(const tbl_set_t *t, int index);
const char *tbl_dice(const tbl_set_t *t, int index);
int tbl_entries(const tbl_set_t *t, int index);

enum { TBL_STEP_TABLE = 1, TBL_STEP_INLINE, TBL_STEP_AGAIN };

typedef struct {
    uint8_t  kind;                      /**< TBL_STEP_* */
    uint8_t  depth;                     /**< 0 for the table rolled on, 1 inside it ... */
    int16_t  table;                     /**< which table (-1 for an inline roll) */
    int32_t  value;                     /**< what was rolled */
    char     what[24];                  /**< the dice ("2d6", "1d4+1") */
} tbl_step_t;

enum {
    TBL_CUT        = 1 << 0,            /**< the text was longer than TBL_TEXT */
    TBL_DEEP       = 1 << 1,            /**< links went deeper than TBL_DEPTH */
    TBL_NO_ROW     = 1 << 2,            /**< a roll had no entry ("(no entry for 13)") */
    TBL_BAD        = 1 << 3,            /**< a link or roll in an entry couldn't be read */
    TBL_STEPS_FULL = 1 << 4,            /**< more working than TBL_STEPS */
};

typedef struct {
    char       text[TBL_TEXT + 1];
    uint16_t   flags;                   /**< TBL_* */
    uint8_t    steps;
    tbl_step_t step[TBL_STEPS];
} tbl_result_t;

/** Roll on table `index`. Returns false for an index that isn't a table.
 *  Problems inside (a missing link) show in the text and the flags. */
bool tbl_roll(const tbl_set_t *t, int index, tbl_result_t *out);

#ifdef __cplusplus
}
#endif

#endif /* TABLE_H */
