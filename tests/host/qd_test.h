/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_test.h
 * @brief   A tiny test harness for the PC tests: CHECK() and a summary.
 *
 *     #include "qd_test.h"
 *     int main(void) {
 *         CHECK(1 + 1 == 2, "maths still works");
 *         return qd_test_done("my test");
 *     }
 */
#ifndef QD_TEST_H
#define QD_TEST_H

#include <stdio.h>

static int qd_test_checks, qd_test_failures;

/** Count a check; print it if it fails. The message takes printf arguments. */
#define CHECK(cond, ...) do {                                           \
        qd_test_checks++;                                               \
        if (!(cond)) {                                                  \
            qd_test_failures++;                                         \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);               \
            printf(__VA_ARGS__);                                        \
            printf("\n");                                               \
        }                                                               \
    } while (0)

/** Print the summary and return the process exit code (0 = all passed). */
static inline int qd_test_done(const char *name)
{
    printf("%s: %d checks, %d failures\n", name, qd_test_checks, qd_test_failures);
    return qd_test_failures ? 1 : 0;
}

#endif /* QD_TEST_H */
