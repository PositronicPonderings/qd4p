/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    console.c
 * @brief   qd4p_console: try QD4P from a serial terminal.
 *
 * Flash qd4p_console.uf2, open the Pico's USB serial port (VS Code's Serial
 * Monitor, or `screen /dev/ttyACM0 115200`), press Enter, type `help`.
 *
 * Each milestone adds its commands to the table at the bottom of this file;
 * the line editing and command lookup above it stay the same.
 */
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"

#include "qd4p.h"

/* ------------------------------------------------------------------------ */
/* Commands                                                                 */
/* ------------------------------------------------------------------------ */
typedef struct {
    const char *name;
    const char *help;
    void (*run)(char *args);
} command_t;

static void cmd_help(char *args);

static void cmd_version(char *args)
{
    (void)args;
    printf("QD4P %s\n", qd_version());
}

static void cmd_limits(char *args)
{
    (void)args;
    printf("dice kept individually  %d (past this: summary mode)\n", QD_MAX_KEPT_DICE);
    printf("dice per roll           %d\n", QD_MAX_ROLL_DICE);
    printf("explosions per die      %d\n", QD_MAX_EXPLOSIONS);
    printf("buckets per roll        %d\n", QD_MAX_BUCKETS);
    printf("faces per custom die    %d\n", QD_MAX_FACES);
    printf("cards per deck          %d\n", QD_MAX_CARDS);
}

static const command_t commands[] = {
    { "help",    "this list",                         cmd_help    },
    { "version", "which QD4P this is",                cmd_version },
    { "limits",  "the compile-time limits in use",    cmd_limits  },
};
#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static void cmd_help(char *args)
{
    (void)args;
    for (size_t i = 0; i < COMMAND_COUNT; i++)
        printf("  %-10s %s\n", commands[i].name, commands[i].help);
}

/* ------------------------------------------------------------------------ */
/* Line input and dispatch                                                  */
/* ------------------------------------------------------------------------ */
static void run_line(char *line)
{
    while (*line == ' ') line++;
    if (!*line) return;
    char *args = strchr(line, ' ');
    if (args) {
        *args++ = '\0';
        while (*args == ' ') args++;
    } else {
        args = line + strlen(line);
    }
    for (size_t i = 0; i < COMMAND_COUNT; i++)
        if (!strcmp(line, commands[i].name)) {
            commands[i].run(args);
            return;
        }
    printf("unknown command '%s' - try help\n", line);
}

int main(void)
{
    stdio_init_all();
    for (int i = 0; i < 30 && !stdio_usb_connected(); i++)   /* up to 3 s for the PC */
        sleep_ms(100);
    printf("\nQD4P %s console - type help\n> ", qd_version());

    char line[128];
    size_t n = 0;
    for (;;) {
        int c = getchar_timeout_us(10000);
        if (c == PICO_ERROR_TIMEOUT) continue;
        if (c == '\r' || c == '\n') {
            printf("\n");
            line[n] = '\0';
            run_line(line);
            n = 0;
            printf("> ");
        } else if ((c == 8 || c == 127) && n > 0) {
            n--;
            printf("\b \b");
        } else if (c >= 0x20 && c < 0x7F && n < sizeof(line) - 1) {
            line[n++] = (char)c;
            putchar(c);
        }
    }
}
