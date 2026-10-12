/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_version.c
 * @brief   qd_version(): which QD4P this is.
 */
#include "qd4p.h"

/* Two macro steps turn a number into text: STR(1) -> "1". */
#define QD_STR2(x) #x
#define QD_STR(x)  QD_STR2(x)

const char *qd_version(void)
{
    return QD_STR(QD_VERSION_MAJOR) "." QD_STR(QD_VERSION_MINOR) "." QD_STR(QD_VERSION_PATCH);
}
