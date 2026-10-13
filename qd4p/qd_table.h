/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_table.h
 * @brief   Random tables: roll on a table, follow its links and inline
 *          rolls, and keep the working.
 *
 *     qd_tables_t tables;
 *     qd_table_result_t r;
 *     qd_tables_open(&tables, asset_bytes, asset_size);   // checks it once
 *     qd_table_roll(&tables, qd_tables_find(&tables, "Forest encounters"), &r);
 *     // r.text  "Bandits, led by Grey Meg, one-eyed and patient"
 *     // r.step[] the working: Forest encounters 2d6: 10, Bandit leaders 1d4: 1
 *
 * Tables are written as text (docs/tables.md) and turned into an asset on
 * the PC by tools/tables.py. The device reads the asset where it is - in
 * flash, in an asset pack - without copying it. In an entry:
 *
 *     {1d4+1}       any dice notation, rolled each time ("3")
 *     [Weather]     rolls on another table and puts its result here
 *     Roll twice    (the whole entry) rolls twice more on the same table,
 *                   ignoring roll-again entries; "Roll 3 times" too
 *     \{ \[ \\      a literal { [ or \
 */
#ifndef QD_TABLE_H
#define QD_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "qd_config.h"
#include "qd_roll.h"

#ifdef __cplusplus
extern "C" {
#endif

/** An open table asset. Points at the bytes; copies nothing. */
typedef struct {
    const uint8_t *data;
    uint32_t       size;
    uint16_t       count;               /**< tables in it */
} qd_tables_t;

/** Check an asset (every offset and string inside it) and get it ready.
 *  Returns false if it isn't a valid QD4P table asset. */
bool qd_tables_open(qd_tables_t *t, const void *data, uint32_t size);

/** A table's index by name (any case), or -1. */
int qd_tables_find(const qd_tables_t *t, const char *name);

/** A table's name, its dice ("2d6") and how many entries it has. */
const char *qd_table_name(const qd_tables_t *t, int index);
const char *qd_table_dice(const qd_tables_t *t, int index);
int qd_table_entries(const qd_tables_t *t, int index);

/** One step of the working. */
enum { QD_STEP_TABLE = 1, QD_STEP_INLINE, QD_STEP_AGAIN };

typedef struct {
    uint8_t  kind;                      /**< QD_STEP_* */
    uint8_t  depth;                     /**< 0 for the table you rolled on, 1 inside it ... */
    int16_t  table;                     /**< which table */
    int32_t  value;                     /**< what was rolled */
    char     what[24];                  /**< the dice ("2d6", "1d4+1") */
} qd_table_step_t;

/** Result flags. */
enum {
    QD_TABLE_CUT     = 1 << 0,          /**< the text was longer than QD_TABLE_TEXT */
    QD_TABLE_DEEP    = 1 << 1,          /**< links went deeper than QD_TABLE_DEPTH */
    QD_TABLE_NO_ROW  = 1 << 2,          /**< a roll had no entry (shown as "(no entry for 13)") */
    QD_TABLE_BAD     = 1 << 3,          /**< a link or roll in an entry couldn't be read */
    QD_TABLE_STEPS_FULL = 1 << 4,       /**< more working than QD_TABLE_STEPS; the rest not kept */
};

typedef struct {
    char            text[QD_TABLE_TEXT + 1];
    uint16_t        flags;              /**< QD_TABLE_* */
    uint8_t         steps;
    qd_table_step_t step[QD_TABLE_STEPS];
} qd_table_result_t;

/** Roll on table `index`. Returns QD_OK, or QD_ERR_NO_DICE for an index
 *  that isn't a table. Problems inside (a missing link) don't stop it: they
 *  show in the text and the flags. */
qd_err_t qd_table_roll(const qd_tables_t *t, int index, qd_table_result_t *out);

#ifdef __cplusplus
}
#endif

#endif /* QD_TABLE_H */
