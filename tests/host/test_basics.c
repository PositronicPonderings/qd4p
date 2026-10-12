/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_basics.c
 * @brief   D0: the library builds and links on a PC, and its version and
 *          limits are what the headers say.
 */
#include <stdio.h>
#include <string.h>

#include "qd4p.h"
#include "qd_test.h"

int main(void)
{
    char expect[32];
    snprintf(expect, sizeof(expect), "%d.%d.%d",
             QD_VERSION_MAJOR, QD_VERSION_MINOR, QD_VERSION_PATCH);
    CHECK(!strcmp(qd_version(), expect), "qd_version() is \"%s\", headers say \"%s\"",
          qd_version(), expect);

    /* Limits must make sense together. */
    CHECK(QD_MAX_KEPT_DICE <= QD_MAX_ROLL_DICE, "kept dice can't exceed dice per roll");
    CHECK(QD_MAX_CARDS <= 255, "deck size must fit the settings store's u8");
    CHECK(QD_MAX_FACES >= 2, "a die needs at least two faces");

    return qd_test_done("basics");
}
