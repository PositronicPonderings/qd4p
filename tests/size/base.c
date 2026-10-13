/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* base: no QD4P at all. USB serial and printf, the floor every other
 * program here stands on. */
#include "common.h"
int main(void) { start(); for (;;) { printf("%lu\n", (unsigned long)time_us_32()); sleep_ms(1000); } }
