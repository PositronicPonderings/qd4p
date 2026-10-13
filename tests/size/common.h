/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* common.h - what every size-check program shares: start USB serial and
 * wait a moment for the PC. The baseline program (base.c) is this and one
 * printf, so "increase over base" is the cost of QD4P alone. */
#ifndef SIZE_COMMON_H
#define SIZE_COMMON_H
#include <stdio.h>
#include "pico/stdlib.h"
static inline void start(void) { stdio_init_all(); sleep_ms(2000); }
#endif
