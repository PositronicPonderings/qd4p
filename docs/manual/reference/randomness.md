# Randomness

[between](#qd_rng_between) · [range](#qd_rng_range) · [next32](#qd_rng_next32) · [init_hw](#qd_rng_init_hw) · [seed_fixed](#qd_rng_seed_fixed) · [is_replay](#qd_rng_is_replay) · [stir](#qd_rng_stir) · [stir32](#qd_rng_stir32) · [stir_count](#qd_rng_stir_count) · [save / restore](#qd_rng_save) · [entropy sources](#qd_entropy_source_t) · [stir_source](#qd_rng_stir_source) · [TRNG](#qd_entropy_trng) · [ADC noise](#qd_entropy_adc) · [events](#qd_entropy_event) · [platform hooks](#qd_platform_seed) · [hold, script](#qd_rng_hold)

Header: `qd_rng.h` (included by `qd4p.h`); the Pico's sources are in `qd_entropy_pico.h`.

**How randomness works here, in four lines:**
1. There is **one generator** for the whole program. Every die, card and table roll comes from it, so everything that makes it better (seeding, stirring) makes every roll better.
2. At boot, [`qd_rng_init_hw`](#qd_rng_init_hw) fills it from the RP2350's hardware random number generator, so every power-up is different.
3. Anything unpredictable can be **stirred** in later: a button press's exact microsecond, sensor noise, a word typed by the DM. Stirring can only add randomness, never take it away.
4. **Replay mode** ([`qd_rng_seed_fixed`](#qd_rng_seed_fixed)) does the opposite: the same seed gives the same rolls every time, on any machine. For tests, bug reports and teaching.

The examples on this page print the same numbers every time you run them in the manual's checks, because the checks use a stand-in for the hardware that is the same every run. On a Pico, your numbers will differ: that's the point.

---

## qd_rng_between

A whole number from `lo` to `hi`, both included, every value equally likely. `qd_rng_between(1, 6)` is a d6.

```c
int32_t qd_rng_between(int32_t lo, int32_t hi);
```

```c example=rng_between
qd_rng_init_hw();
printf("d20: %ld\n", (long)qd_rng_between(1, 20));
printf("d6:  %ld\n", (long)qd_rng_between(1, 6));
printf("a temperature: %ld C\n", (long)qd_rng_between(-10, 35));
```

```text output
d20: 19
d6:  4
a temperature: 33 C
```

| Parameter | Meaning |
|---|---|
| `lo` | the smallest value it can return |
| `hi` | the largest. If `lo` > `hi` they are swapped |

**Returns** the number. Any range of `int32_t` works, including the whole of it.

For dice with modifiers (exploding, keep the best 3, successes), build a roll instead: [the roll engine](rolls.md). For one die of an odd shape, [single dice](dice.md).

---

## qd_rng_range

A whole number from 0 to `n` - 1, every value equally likely. The building block under everything else.

```c
uint32_t qd_rng_range(uint32_t n);
```

```c example=rng_range
static const char *const weather[] = { "sun", "rain", "fog", "snow" };
qd_rng_init_hw();
for (int day = 1; day <= 3; day++)
    printf("day %d: %s\n", day, weather[qd_rng_range(4)]);
```

```text output
day 1: snow
day 2: fog
day 3: snow
```

| Parameter | Meaning |
|---|---|
| `n` | how many values. `n` = 0 returns 0 |

**Returns** 0 to `n` - 1.

**Why not `rand() % n`?** Because `%` is very slightly unfair: 2<sup>32</sup> isn't a multiple of 6, so with `% 6` the low faces turn up a hair more often. `qd_rng_range` rejects the few leftover values instead (Lemire's method), so every value is exactly as likely as every other. [How it works](../10-how-it-works.md#unbiased-ranges) has the details.

---

## qd_rng_next32

The next 32 random bits, as they come out of the generator.

```c
uint32_t qd_rng_next32(void);
```

```c example=rng_next32
qd_rng_seed_fixed(1);
printf("%08lx %08lx\n", (unsigned long)qd_rng_next32(), (unsigned long)qd_rng_next32());
```

```text output
54d30301 650941ba
```

**Returns** a number from 0 to 4,294,967,295.

For dice, use [`qd_rng_between`](#qd_rng_between) or [`qd_rng_range`](#qd_rng_range): turning 32 bits into a fair d6 has a trap in it, and those two step around it.

---

## qd_rng_init_hw

Seed the generator from the hardware random source, and leave replay mode. Call it once at boot.

```c
void qd_rng_init_hw(void);
```

```c example=rng_init_hw
qd_rng_seed_fixed(7);                      /* replay mode, for a test */
printf("replay: %s\n", qd_rng_is_replay() ? "on" : "off");
qd_rng_init_hw();                          /* back to real randomness */
printf("replay: %s\n", qd_rng_is_replay() ? "on" : "off");
```

```text output
replay: on
replay: off
```

- **On a Pico 2** it reads the RP2350's true random number generator, a circuit that measures electrical noise. Every power-up starts somewhere new and unguessable.
- **On a PC** (tests, the PC console) it uses the operating system's random source, `/dev/urandom`, when there is one, and the clock otherwise.
- **Calling it again later** stirs in fresh hardware randomness on top of what's there. It can't hurt.
- **In replay mode** it leaves replay and starts afresh. **In fair mode** it is ignored.

---

## qd_rng_seed_fixed

Replay mode: from now on, the same rolls as every other time this seed was used, on any machine.

```c
void qd_rng_seed_fixed(uint64_t seed);
```

```c example=rng_seed_fixed
qd_rng_seed_fixed(42);
long a = (long)qd_rng_between(1, 20), b = (long)qd_rng_between(1, 20);
qd_rng_seed_fixed(42);
long c = (long)qd_rng_between(1, 20), d = (long)qd_rng_between(1, 20);
printf("%ld %ld, then %ld %ld\n", a, b, c, d);
```

```text output
9 20, then 9 20
```

| Parameter | Meaning |
|---|---|
| `seed` | any number. Each gives its own sequence |

While replay mode is on, stirring is ignored (it would break the replay). [`qd_rng_init_hw`](#qd_rng_init_hw) ends it. In fair mode this function is ignored: end the session first.

The console's `seed 10` is this. Show the user when it's on: a dice roller stuck in replay mode rolls the same "random" numbers every time it's switched on, which players notice faster than you'd think.

---

## qd_rng_is_replay

Is replay mode on?

```c
bool qd_rng_is_replay(void);
```

```c example=rng_is_replay
qd_rng_seed_fixed(3);
printf("%s\n", qd_rng_is_replay() ? "REPLAY: same rolls every time" : "live");
```

```text output
REPLAY: same rolls every time
```

**Returns** `true` between [`qd_rng_seed_fixed`](#qd_rng_seed_fixed) and the next [`qd_rng_init_hw`](#qd_rng_init_hw).

---

## qd_rng_stir

Mix bytes into the generator. Any bytes: a word the DM types, sensor readings, timings.

```c
bool qd_rng_stir(const void *bytes, size_t len);
```

```c example=rng_stir
const char *word = "owlbear";
qd_rng_init_hw();
printf("%s\n", qd_rng_stir(word, strlen(word)) ? "stirred in" : "ignored");
qd_rng_seed_fixed(5);
printf("%s\n", qd_rng_stir(word, strlen(word)) ? "stirred in" : "ignored: replay mode");
```

```text output
stirred in
ignored: replay mode
```

| Parameter | Meaning |
|---|---|
| `bytes`, `len` | what to mix in, and how many bytes |

**Returns** `false` (and changes nothing) in replay mode or fair mode, or if `len` is 0; `true` otherwise.

**Stirring can't make things worse.** The bytes are mixed with what's already there, so even something completely predictable ("aaaa") leaves the generator exactly as unpredictable as before, and anything unpredictable makes it more so. That's why it's safe to stir in a source you're not sure about.

---

## qd_rng_stir32

Mix one 32-bit number in: the same as `qd_rng_stir(&v, 4)`.

```c
bool qd_rng_stir32(uint32_t v);
```

```c example=rng_stir32 compile-only
#include "pico/stdlib.h"
#include "qd4p.h"

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();
    qd_rng_stir32(time_us_32());     /* when the program got this far */
    return 0;
}
```

**Returns** the same as [`qd_rng_stir`](#qd_rng_stir).

---

## qd_rng_stir_count

How many times stirring has changed the generator since it was last seeded. For a "randomness added: 37" line in a settings screen.

```c
uint32_t qd_rng_stir_count(void);
```

```c example=rng_stir_count
qd_rng_seed_fixed(1);                  /* replay: stirring ignored ... */
qd_rng_stir32(123);
printf("%lu\n", (unsigned long)qd_rng_stir_count());
qd_rng_init_hw();                      /* ... live: counted */
qd_rng_stir32(123);
qd_rng_stir32(456);
printf("%lu\n", (unsigned long)qd_rng_stir_count());
```

```text output
0
2
```

---

## qd_rng_save

Copy the generator's 128-bit state out, and put one back with `qd_rng_restore`: to replay a stretch of rolls exactly, or to save a game with its dice.

```c
void qd_rng_save(uint32_t state[4]);
void qd_rng_restore(const uint32_t state[4]);
```

```c example=rng_save
uint32_t saved[4];
qd_rng_init_hw();
qd_rng_save(saved);
printf("%ld %ld %ld\n", (long)qd_rng_between(1, 6), (long)qd_rng_between(1, 6), (long)qd_rng_between(1, 6));
qd_rng_restore(saved);
printf("%ld %ld %ld\n", (long)qd_rng_between(1, 6), (long)qd_rng_between(1, 6), (long)qd_rng_between(1, 6));
```

```text output
6 4 6
6 4 6
```

`qd_rng_restore` leaves replay mode as it was. An all-zero state, which would get the generator stuck at zero forever, is quietly made non-zero. [`qd_odds`](odds.md#qd_odds) uses this pair to borrow the generator and give it back untouched.

---

## qd_entropy_source_t

A standard way to describe anything that produces unpredictable readings, so a new source of randomness is one small struct.

```c
typedef struct {
    const char *name;               /* shown in menus: "Light sensor" */
    bool (*begin)(void *ctx);       /* start the source; false = unavailable (may be NULL) */
    uint32_t (*sample)(void *ctx);  /* one raw reading */
    void (*end)(void *ctx);         /* shut it down (may be NULL) */
    void *ctx;                      /* passed to the three */
} qd_entropy_source_t;
```

```c example=entropy_source
static uint32_t counter;
static uint32_t read_counter(void *ctx) { (void)ctx; return counter++; }   /* a poor source: fine */

int main(void)
{
    const qd_entropy_source_t mine = { "Counter", NULL, read_counter, NULL, NULL };
    qd_rng_init_hw();
    printf("%s: %lu readings stirred in\n", mine.name,
           (unsigned long)qd_rng_stir_source(&mine, 16));
    return 0;
}
```

```text output
Counter: 16 readings stirred in
```

A reading doesn't need to be good: it is stirred in with the time it took to take, and stirring can't make things worse. Real sources on the Pico: [the TRNG](#qd_entropy_trng), [ADC noise](#qd_entropy_adc), [button timing](#qd_entropy_event).

---

## qd_rng_stir_source

Take readings from a source and stir each one in, with how long it took.

```c
uint32_t qd_rng_stir_source(const qd_entropy_source_t *src, uint32_t samples);
```

| Parameter | Meaning |
|---|---|
| `src` | the source |
| `samples` | how many readings |

**Returns** how many readings were stirred in: 0 if the source's `begin` said no, or in replay or fair mode.

Example: [`qd_entropy_source_t`](#qd_entropy_source_t), above.

---

## qd_entropy_trng

The RP2350's hardware random number generator, as a source. Pico only (`qd_entropy_pico.h`).

```c
extern const qd_entropy_source_t qd_entropy_trng;
```

```c example=entropy_trng compile-only
#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();                               /* reads the TRNG already */
    qd_rng_stir_source(&qd_entropy_trng, 16);       /* and 16 readings more */
    return 0;
}
```

[`qd_rng_init_hw`](#qd_rng_init_hw) already seeds from it. Stirring in more later is for the cautious: it does no harm.

---

## qd_entropy_adc

Electrical noise from an ADC pin (GPIO 26 to 29), whatever is wired to it: a light sensor, a knob, nothing at all. Pico only.

```c
typedef struct { qd_entropy_source_t source; unsigned gpio; } qd_entropy_adc_t;
const qd_entropy_source_t *qd_entropy_adc(qd_entropy_adc_t *a, unsigned gpio);
```

```c example=entropy_adc compile-only
#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();
    static qd_entropy_adc_t light;                          /* must outlive its use */
    qd_rng_stir_source(qd_entropy_adc(&light, 28), 64);     /* GPIO 28: 64 readings */
    return 0;
}
```

| Parameter | Meaning |
|---|---|
| `a` | somewhere to keep the source's details; it must stay alive while the source is used |
| `gpio` | 26, 27, 28 or 29. Any other pin: the source says it's unavailable |

**Returns** the source, ready for [`qd_rng_stir_source`](#qd_rng_stir_source).

The top bits of an ADC reading are what the sensor sees; the bottom ones wobble with noise nobody can predict. Mixing keeps the wobble and ignores the rest.

---

## qd_entropy_event

Stir in the exact microsecond of something a person did: a button press, an encoder click. Pico only.

```c
void qd_entropy_event(void);
```

```c example=entropy_event compile-only
#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();
    for (;;) {
        int c = getchar_timeout_us(100000);
        if (c == PICO_ERROR_TIMEOUT) continue;
        qd_entropy_event();                         /* when the key came, to the microsecond */
        printf("d20: %ld\n", (long)qd_rng_between(1, 20));
    }
}
```

Nobody can time a click to the microsecond, so every press of a button adds a little real randomness, for free. Call it from your button handler and forget about it.

---

## qd_platform_seed

The two hooks QD4P needs from the machine it runs on. `qd_entropy_pico.c` supplies them on a Pico, `qd_entropy_host.c` on a PC; you only write them to run QD4P somewhere else.

```c
void qd_platform_seed(uint32_t *out, size_t words);   /* fill with the best randomness there is */
uint32_t qd_platform_micros(void);                    /* a free-running microsecond counter */
```

---

## qd_rng_hold

Low level, for the library's own use; listed so nothing is a secret.

```c
void qd_rng_hold(const uint32_t state[4]);   /* fair mode: pin the state, ignore stirring */
void qd_rng_release(void);
bool qd_rng_is_held(void);
void qd_rng_script(qd_rng_script_t *script); /* odds: return chosen values, not random ones */
```

- **Hold** is how [fair mode](fair.md) fixes each action's dice: while held, only `qd_rng_hold` changes the state; stirring, reseeding and replay are ignored. Use [`qd_fair_*`](fair.md), not these.
- **Script** is how [`qd_odds`](odds.md#qd_odds) tries every possible roll exactly once: while a script is set, [`qd_rng_range`](#qd_rng_range) returns the script's values instead of random ones. Not for ordinary use.
