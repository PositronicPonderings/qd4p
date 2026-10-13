/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* tables_example: the same two tables in the example table system
 * (examples/tables), as an asset with {dice} in its text. */
#include "common.h"
#include "qd4p.h"
#include "table.h"
#include "size_tables.h"
int main(void)
{
    start();
    qd_rng_init_hw();
    static tbl_set_t t;
    static tbl_result_t r;
    tbl_open(&t, size_tables, sizeof(size_tables));
    for (;;) {
        tbl_roll(&t, 0, &r); printf("%s\n", r.text);
        tbl_roll(&t, 1, &r); printf("%s\n", r.text);
        sleep_ms(1000);
    }
}
