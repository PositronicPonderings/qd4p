# Decks of cards

[types](#qd_deck_t) · [poker](#qd_deck_poker) · [tarot](#qd_deck_tarot) · [your own](#qd_deck_custom) · [deal](#qd_deck_deal) · [draw](#qd_deck_draw) · [shuffle](#qd_deck_shuffle) · [return](#qd_deck_return) · [how many left](#qd_deck_remaining) · [is it out?](#qd_deck_is_out) · [reshuffle rules](#reshuffle-rules) · [card order](#qd_card_order) · [card name](#qd_card_name)

Header: `qd_deck.h` (included by `qd4p.h`).

**How a deck works, in four lines:**
1. A deck is a list of **cards** (which stays in flash) and a record of which cards are **out**: dealt, in a hand, discarded.
2. Drawing picks one of the cards still in the deck at random, which is exactly what drawing the top card of a well-shuffled deck does. So there's no order to store, and "shuffling" is putting every card back.
3. The record is 1 bit per card, so a whole deck in play is 40 bytes of RAM (`qd_deck_t`), and the Dice Roller can save it between sessions.
4. **Reshuffle rules** say when cards go back on their own: after every draw, when the deck runs out, or after a joker. Savage Worlds' action deck is a poker deck with two jokers and the last two rules.

---

## qd_deck_t

The types: a card, a deck, and a card that was drawn.

```c
typedef struct {
    int8_t  value;            /* rank: 2 .. 14 in poker (ace high), 0 .. 21 for tarot's major arcana */
    uint8_t suit;             /* QD_SUIT_* */
    uint8_t flags;            /* QD_CARD_TRIGGER: dealing it arms the after-joker reshuffle */
    char    label[5];         /* short name: "10S", "QH", "JkR", "M0" */
} qd_card_t;

typedef struct {
    const qd_card_t *cards;   /* the card definitions (not copied) */
    uint8_t  count;           /* cards in the full deck */
    uint8_t  policy;          /* QD_RESHUFFLE_* rules: change it whenever you like */
    bool     triggered;       /* a trigger card is out: the next deal reshuffles first */
    uint8_t  out[32];         /* 1 bit per card: out of the deck */
} qd_deck_t;

typedef struct {
    uint8_t index;            /* which card of the deck, 0 .. count - 1 */
    const qd_card_t *card;    /* its definition */
} qd_card_draw_t;
```

Suits in the built-in decks:

| Poker | | Tarot | |
|---|---|---|---|
| `QD_SUIT_JOKER` | 0 | `QD_SUIT_MAJOR` | 0 |
| `QD_SUIT_CLUBS` | 1 | `QD_SUIT_WANDS` | 1 |
| `QD_SUIT_DIAMONDS` | 2 | `QD_SUIT_CUPS` | 2 |
| `QD_SUIT_HEARTS` | 3 | `QD_SUIT_SWORDS` | 3 |
| `QD_SUIT_SPADES` | 4 | `QD_SUIT_PENTACLES` | 4 |

Poker suits are numbered in Savage Worlds initiative order: spades beat hearts beat diamonds beat clubs.

---

## qd_deck_poker

A poker deck: 52 cards and 0 to 4 jokers. Jokers are trigger cards.

```c
bool qd_deck_poker(qd_deck_t *deck, int jokers);
```

```c example=deck_poker
qd_deck_t deck;
qd_card_draw_t hand[5];
char name[24];
qd_rng_init_hw();
qd_deck_poker(&deck, 0);
int n = qd_deck_deal(&deck, hand, 5);
for (int i = 0; i < n; i++)
    printf("%-4s %s\n", hand[i].card->label, qd_card_name(hand[i].card, name, sizeof name));
printf("%d left\n", qd_deck_remaining(&deck));
```

```text output
QS   Queen of Spades
2H   Two of Hearts
JS   Jack of Spades
KC   King of Clubs
8H   Eight of Hearts
47 left
```

| Parameter | Meaning |
|---|---|
| `deck` | the deck to set up, all cards in |
| `jokers` | 0 to 4. Savage Worlds uses 2: a red and a black |

**Returns** `false` for more than 4 jokers. The reshuffle rule starts as `QD_RESHUFFLE_WHEN_EMPTY`.

Cards are labelled `2C` to `AS` (rank, then suit: C, D, H, S) and `JkR`, `JkB`, `Jk3`, `Jk4`. Values: 2 to 10, J 11, Q 12, K 13, A 14.

---

## qd_deck_tarot

A 78-card tarot deck: the 22 major arcana, then four suits of 14.

```c
void qd_deck_tarot(qd_deck_t *deck);
```

```c example=deck_tarot
static const char *const position[] = { "Past", "Present", "Future" };
qd_deck_t deck;
qd_card_draw_t spread[3];
char name[24];
qd_rng_init_hw();
qd_deck_tarot(&deck);
qd_deck_deal(&deck, spread, 3);
for (int i = 0; i < 3; i++)
    printf("%-8s %s\n", position[i], qd_card_name(spread[i].card, name, sizeof name));
```

```text output
Past     Page of Pentacles
Present  Five of Cups
Future   Ten of Pentacles
```

The major arcana are suit `QD_SUIT_MAJOR`, values 0 (The Fool) to 21 (The World), labelled `M0` to `M21`. The suits are values 1 to 14: ace to ten, page, knight, queen, king. The reshuffle rule starts as `QD_RESHUFFLE_WHEN_EMPTY`.

---

## qd_deck_custom

A deck of your own cards: a hand of encounter cards, a custom initiative deck, a deck of 25 identical "Fate" cards.

```c
bool qd_deck_custom(qd_deck_t *deck, const qd_card_t *cards, uint16_t count);
```

```c example=deck_custom
static const qd_card_t events[] = {
    { 1, 1, 0, "Rain" }, { 2, 1, 0, "Wolf" }, { 3, 1, 0, "Lost" },
    { 4, 2, 0, "Inn" },  { 5, 2, 0, "Gold" }, { 6, 3, QD_CARD_TRIGGER, "Doom" } };
qd_deck_t deck;
qd_card_draw_t c;
qd_rng_init_hw();
qd_deck_custom(&deck, events, 6);
deck.policy = QD_RESHUFFLE_AFTER_TRIGGER | QD_RESHUFFLE_WHEN_EMPTY;
for (int day = 1; day <= 4; day++) {
    qd_deck_draw(&deck, &c);
    printf("day %d: %s\n", day, c.card->label);
}
```

```text output
day 1: Doom
day 2: Inn
day 3: Doom
day 4: Wolf
```

| Parameter | Meaning |
|---|---|
| `cards` | the cards. **Not copied**: keep them alive for as long as the deck is used, which a `static const` array does |
| `count` | 1 to 255 (`QD_MAX_CARDS`) |

**Returns** `false` for 0 or too many cards. The reshuffle rule starts as `QD_RESHUFFLE_WHEN_EMPTY`. A custom card's name ([`qd_card_name`](#qd_card_name)) is its label, so labels of up to 4 characters are what players see; keep your own longer names in a table beside the cards if you need them.

---

## qd_deck_deal

One deal: up to `n` cards, with the reshuffle rules applied.

```c
int qd_deck_deal(qd_deck_t *deck, qd_card_draw_t *out, int n);
```

```c example=deck_deal
qd_deck_t deck;
qd_card_draw_t hand[13];
qd_rng_init_hw();
qd_deck_poker(&deck, 0);
deck.policy = 0;                                  /* no automatic reshuffles */
for (int round = 1; round <= 5; round++) {
    int got = qd_deck_deal(&deck, hand, 13);
    printf("round %d: %d cards, %d left\n", round, got, qd_deck_remaining(&deck));
}
```

```text output
round 1: 13 cards, 39 left
round 2: 13 cards, 26 left
round 3: 13 cards, 13 left
round 4: 13 cards, 0 left
round 5: 0 cards, 0 left
```

| Parameter | Meaning |
|---|---|
| `out` | where the cards go: room for `n` |
| `n` | how many to deal |

**Returns** how many were dealt: fewer than `n` only if the deck ran out (and isn't set to reshuffle when empty).

A deal is one go. If the rules say to reshuffle after a joker, a joker in this deal makes the **next** deal start with a reshuffle; this one finishes as it is. That's the Savage Worlds rule, and why the function deals a whole round at a time.

---

## qd_deck_draw

One card: a deal of one.

```c
bool qd_deck_draw(qd_deck_t *deck, qd_card_draw_t *out);
```

**Returns** `false` if the deck is empty and isn't set to reshuffle. Example: [`qd_deck_custom`](#qd_deck_custom).

---

## qd_deck_shuffle

Put every card back. Clears a pending after-joker reshuffle.

```c
void qd_deck_shuffle(qd_deck_t *deck);
```

```c example=deck_shuffle
qd_deck_t deck;
qd_card_draw_t hand[10];
qd_rng_init_hw();
qd_deck_tarot(&deck);
qd_deck_deal(&deck, hand, 10);
printf("%d in the deck\n", qd_deck_remaining(&deck));
qd_deck_shuffle(&deck);
printf("%d in the deck\n", qd_deck_remaining(&deck));
```

```text output
68 in the deck
78 in the deck
```

---

## qd_deck_return

Put one card back: returned from a hand, a discard taken back.

```c
void qd_deck_return(qd_deck_t *deck, uint8_t index);
```

```c example=deck_return
qd_deck_t deck;
qd_card_draw_t c;
qd_rng_init_hw();
qd_deck_poker(&deck, 2);
qd_deck_draw(&deck, &c);
printf("drew %s; out: %s\n", c.card->label, qd_deck_is_out(&deck, c.index) ? "yes" : "no");
qd_deck_return(&deck, c.index);
printf("returned; out: %s\n", qd_deck_is_out(&deck, c.index) ? "yes" : "no");
```

```text output
drew AS; out: yes
returned; out: no
```

| Parameter | Meaning |
|---|---|
| `index` | the card's index in the deck: `qd_card_draw_t.index` |

---

## qd_deck_remaining

How many cards are in the deck, and how many are out of it.

```c
int qd_deck_remaining(const qd_deck_t *deck);
int qd_deck_out_count(const qd_deck_t *deck);
```

Together they always add up to `deck->count`. Examples: [`qd_deck_deal`](#qd_deck_deal), [`qd_deck_shuffle`](#qd_deck_shuffle).

---

## qd_deck_is_out

Is this card out of the deck?

```c
bool qd_deck_is_out(const qd_deck_t *deck, uint8_t index);
```

Example: [`qd_deck_return`](#qd_deck_return).

---

## Reshuffle rules

Set `deck.policy` to any of these, combined with `|`, whenever you like:

| Rule | Means |
|---|---|
| `QD_RESHUFFLE_EVERY_DRAW` | every card goes back before each draw: draws are independent, like rolling a 52-sided die |
| `QD_RESHUFFLE_WHEN_EMPTY` | an empty deck reshuffles instead of running out, even in the middle of a deal |
| `QD_RESHUFFLE_AFTER_TRIGGER` | once a trigger card (a joker, in the built-in poker deck) has been dealt, the next deal starts with a reshuffle |
| `0` | only [`qd_deck_shuffle`](#qd_deck_shuffle) puts cards back |

```c example=deck_policy
qd_deck_t deck;                       /* Savage Worlds' action deck */
qd_card_draw_t hand[54];
qd_rng_init_hw();
qd_deck_poker(&deck, 2);
deck.policy = QD_RESHUFFLE_AFTER_TRIGGER | QD_RESHUFFLE_WHEN_EMPTY;
int got = qd_deck_deal(&deck, hand, 54);          /* the whole deck: both jokers are in it */
printf("dealt %d; a joker is out: %s\n", got, deck.triggered ? "yes" : "no");
qd_deck_deal(&deck, hand, 3);                     /* so this deal starts with a reshuffle */
printf("%d in the deck\n", qd_deck_remaining(&deck));
```

```text output
dealt 54; a joker is out: yes
51 in the deck
```

---

## qd_card_order

A number for sorting cards into initiative order: higher acts first. Jokers beat everything, then rank, then suit (spades high).

```c
int qd_card_order(const qd_card_t *card);
```

```c example=card_order
static const char *const who[] = { "Ana", "Bors", "Cleo", "the ogre" };
qd_deck_t deck;
qd_card_draw_t hand[4];
char name[24];
qd_rng_init_hw();
qd_deck_poker(&deck, 2);
qd_deck_deal(&deck, hand, 4);
for (int turn = 0; turn < 4; turn++) {           /* highest card first */
    int best = -1;
    for (int i = 0; i < 4; i++)
        if (hand[i].card && (best < 0 || qd_card_order(hand[i].card) > qd_card_order(hand[best].card)))
            best = i;
    printf("%-9s %s\n", who[best], qd_card_name(hand[best].card, name, sizeof name));
    hand[best].card = NULL;
}
```

```text output
Ana       Ace of Spades
the ogre  Ace of Clubs
Cleo      King of Spades
Bors      Three of Hearts
```

---

## qd_card_name

A card's full name: "Queen of Hearts", "Red Joker", "The Fool", "Knight of Cups". A custom card's name is its label.

```c
char *qd_card_name(const qd_card_t *card, char *buf, int size);
```

| Parameter | Meaning |
|---|---|
| `buf`, `size` | where to write the name; 24 bytes holds every built-in name ("Knight of Pentacles" is the longest, at 19 characters) |

**Returns** `buf`, so it can go straight into a `printf`. Examples: [`qd_deck_poker`](#qd_deck_poker), [`qd_deck_tarot`](#qd_deck_tarot).
