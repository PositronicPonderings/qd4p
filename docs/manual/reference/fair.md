# Fair mode

[start](#qd_fair_start) · [start with a key](#qd_fair_start_key) · [is it on?](#qd_fair_is_on) · [begin an action](#qd_fair_begin) · [begin at](#qd_fair_begin_at) · [count](#qd_fair_count) · [fingerprint](#qd_fair_fingerprint) · [end](#qd_fair_end) · [fingerprint of a key](#qd_fair_print_of) · [words](#qd_fair_word) · [words to bytes](#qd_fair_read_words) · [SHA-256](#qd_sha256)

Header: `qd_fair.h` (included by `qd4p.h`).

**Fair mode, in four lines:**
1. A session starts with a **secret key**. The device shows its **fingerprint**, six words, and everyone writes them down.
2. Each action (a roll, a reroll, a deal, a table roll) gets the next **number**, and its dice come only from the key and that number.
3. At the end the **key** is revealed, sixteen words. Anyone can check that it gives the fingerprint, then repeat any action from its number and see the same dice.
4. It proves nothing was changed *during* the session; it can't prove the firmware is honest. [Part 7](../07-fair-mode.md) is the guide, with what to tell players.

Fair mode costs about 5 KB of flash when it's used and nothing when it isn't ([what it costs](../../SIZES.md)).

---

## qd_fair_start

Start a session with a new secret key.

```c
bool qd_fair_start(void);
```

```c example=fair_start
char words[QD_FAIR_TEXT];
qd_rng_init_hw();
qd_fair_start();
qd_fair_fingerprint_words(words, sizeof words);
printf("fingerprint: %s\n", words);
```

```text output
fingerprint: hook brush farm mask pizza easel
```

The key comes from fresh hardware randomness and the generator's own output (which carries everything stirred in so far), hashed together. **Returns** `false` (and changes nothing) if a session is already running. From now on, stirring, [`qd_rng_init_hw`](randomness.md#qd_rng_init_hw) and replay mode are ignored.

---

## qd_fair_start_key

Start a session with a key you already have: to check a session after its key was revealed, or for tests.

```c
void qd_fair_start_key(const uint8_t key[QD_FAIR_KEY_BYTES]);
```

```c example=fair_start_key
uint8_t key[QD_FAIR_KEY_BYTES];
char words[QD_FAIR_TEXT];
qd_fair_read_words("wood cherry shell shoe tower gate coffee nest "
                   "piano butter ankle echo wizard lemon vase baker", key, QD_FAIR_KEY_BYTES);
qd_fair_start_key(key);
qd_fair_fingerprint_words(words, sizeof words);
printf("fingerprint: %s\n", words);
```

```text output
fingerprint: baker canyon dove rose cycle orange
```

Replaces any running session. A session started this way is a **checking** session: [`qd_fair_begin_at`](#qd_fair_begin_at) works in it.

---

## qd_fair_is_on

Is a session running?

```c
bool qd_fair_is_on(void);
```

---

## qd_fair_begin

Before each action: take the next number, with the generator set for it.

```c
uint32_t qd_fair_begin(void);
```

```c example=fair_begin
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_fair_start();
qd_roll_init(&roll);
qd_roll_dice(&roll, 1, 20);
for (int i = 0; i < 3; i++) {
    uint32_t n = qd_fair_begin();
    qd_roll_run(&roll, &res);
    printf("#%lu: %ld\n", (unsigned long)n, (long)res.total);
}
qd_fair_end(NULL);
```

```text output
#1: 17
#2: 13
#3: 1
```

**Returns** the number: 1, 2, 3 ... Show it with the result, so players can write it down. Returns 0 (and does nothing) when fair mode is off, so an app can call it unconditionally.

Call it once per action the players see, right before it rolls. Anything rolled without it carries on from the last action's numbers: still fixed by the key, but only checkable by repeating that whole action.

---

## qd_fair_begin_at

Checking: set the generator for action `n`.

```c
bool qd_fair_begin_at(uint32_t n);
```

```c example=fair_begin_at
static qd_parsed_t p;
static qd_result_t res;
qd_parse_error_t err;
uint8_t key[QD_FAIR_KEY_BYTES] = { 0 };            /* a revealed key */
qd_fair_start_key(key);
qd_parse("4d6k3", &p, &err);
qd_fair_begin_at(3);                               /* action #3, on its own */
qd_roll_run(&p.roll, &res);
printf("#3 4d6k3: %ld\n", (long)res.total);
qd_fair_begin_at(3);                               /* and again: always the same */
qd_roll_run(&p.roll, &res);
printf("#3 4d6k3: %ld\n", (long)res.total);
```

```text output
#3 4d6k3: 12
#3 4d6k3: 12
```

**Returns** `false`, doing nothing, on a live session: letting a live session go back would let an action be redone. Afterwards [`qd_fair_begin`](#qd_fair_begin) carries on from `n + 1`.

---

## qd_fair_count

The number of the last action begun (0 before the first, and when fair mode is off).

```c
uint32_t qd_fair_count(void);
```

---

## qd_fair_fingerprint

The fingerprint: as six bytes, or as six words.

```c
bool qd_fair_fingerprint(uint8_t out[QD_FAIR_PRINT_BYTES]);
int  qd_fair_fingerprint_words(char *buf, int size);
```

**Returns** `false` / 0 (and an empty string) when fair mode is off; the words' length otherwise. Example: [`qd_fair_start`](#qd_fair_start).

---

## qd_fair_end

End the session and reveal the key.

```c
bool qd_fair_end(uint8_t key[QD_FAIR_KEY_BYTES]);
```

```c example=fair_end
uint8_t key[QD_FAIR_KEY_BYTES];
char words[QD_FAIR_TEXT];
qd_rng_init_hw();
qd_fair_start();
/* ... the session ... */
qd_fair_end(key);
qd_fair_words(key, 8, words, sizeof words);
printf("%s\n", words);
qd_fair_words(key + 8, 8, words, sizeof words);
printf("%s\n", words);
```

```text output
door tent lilac tiger angle scout bugle wand
dove dawn easel wolf hawk planet jungle soap
```

| Parameter | Meaning |
|---|---|
| `key` | where to put the key, or `NULL` to end without seeing it (a checking session) |

**Returns** `false` if no session was running. The generator is let go and stirred with fresh hardware randomness, since its last state can be worked out from the key. Show the key in two lines of eight words: that's how the console shows it, and how [`qd_fair_read_words`](#qd_fair_read_words) best takes it back.

---

## qd_fair_print_of

The fingerprint of any key: for a checker, to compare with the one written down at the start.

```c
void qd_fair_print_of(const uint8_t key[QD_FAIR_KEY_BYTES], uint8_t out[QD_FAIR_PRINT_BYTES]);
```

---

## qd_fair_word

The word list: one word for each byte value.

```c
const char *qd_fair_word(uint8_t b);
int qd_fair_words(const uint8_t *bytes, int n, char *buf, int size);
```

```c example=fair_word
uint8_t some[] = { 0, 1, 2, 128, 254, 255 };
char words[QD_FAIR_TEXT];
qd_fair_words(some, 6, words, sizeof words);
printf("%s\n", words);
```

```text output
acorn actor agent hippo yarn zebra
```

256 words of 3 to 6 letters, a to z, in alphabetical order; no two start with the same three letters, so three letters are always enough to type one. `qd_fair_words` writes bytes as words separated by spaces, whole words only, and **returns** the length written.

---

## qd_fair_read_words

Words back to bytes.

```c
int qd_fair_read_words(const char *text, uint8_t *out, int max);
```

```c example=fair_read_words
uint8_t b[16];
int n = qd_fair_read_words("Wood, CHE-she sho", b, 16);
printf("%d words:", n);
for (int i = 0; i < n; i++) printf(" %s", qd_fair_word(b[i]));
printf("\n%d\n", qd_fair_read_words("wood cherry shoelace", b, 16));
```

```text output
4 words: wood cherry shell shoe
-3
```

Any case; spaces, commas or hyphens between words; each word may be cut to its first three or more letters. **Returns** how many bytes were read, at most `max`, or -(k + 1) if word k (counting from 0) isn't one of the words.

---

## qd_sha256

SHA-256 (FIPS 180-4): fair mode's hash. Here for tests, and anything else that needs one.

```c
void qd_sha256(const void *data, size_t len, uint8_t out[32]);
```

```c example=sha256
uint8_t h[32];
qd_sha256("abc", 3, h);
for (int i = 0; i < 32; i++) printf("%02x", h[i]);
printf("\n");
```

```text output
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
```

Done in software: about 20 microseconds a block on a Pico 2. The RP2350 has a SHA-256 accelerator too, but software is portable, and fair mode only needs two hashes per action.
