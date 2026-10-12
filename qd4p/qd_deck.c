/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_deck.c
 * @brief   Decks: drawing, dealing, reshuffle rules, built-in poker and tarot.
 *
 * ORDER OF RANDOM DRAWS (copied by tests/host/rng_reference.py): each card
 * is range(cards remaining), counting through the cards still in the deck
 * in deck order. EVERY_DRAW decks put all cards back first, so that's
 * range(count).
 */
#include <stdio.h>
#include <string.h>

#include "qd_deck.h"
#include "qd_rng.h"

/* ========================================================================
 * Built-in decks (const, so they live in flash, not RAM)
 * ======================================================================== */
#define CARD(v, s, l) { (int8_t)(v), (uint8_t)(s), 0, l }
#define JOKER(l)      { 15, QD_SUIT_JOKER, QD_CARD_TRIGGER, l }

/* Clubs, diamonds, hearts, spades; then up to four jokers. */
static const qd_card_t poker_cards[56] = {
#define SUIT(s, c) \
    CARD(2, s, "2" c), CARD(3, s, "3" c), CARD(4, s, "4" c), CARD(5, s, "5" c), \
    CARD(6, s, "6" c), CARD(7, s, "7" c), CARD(8, s, "8" c), CARD(9, s, "9" c), \
    CARD(10, s, "10" c), CARD(11, s, "J" c), CARD(12, s, "Q" c), CARD(13, s, "K" c), \
    CARD(14, s, "A" c)
    SUIT(QD_SUIT_CLUBS, "C"), SUIT(QD_SUIT_DIAMONDS, "D"),
    SUIT(QD_SUIT_HEARTS, "H"), SUIT(QD_SUIT_SPADES, "S"),
#undef SUIT
    JOKER("JkR"), JOKER("JkB"), JOKER("Jk3"), JOKER("Jk4"),
};

/* Major arcana 0..21, then wands, cups, swords, pentacles: A,2..10,P,N,Q,K. */
static const qd_card_t tarot_cards[78] = {
    CARD(0, 0, "M0"),   CARD(1, 0, "M1"),   CARD(2, 0, "M2"),   CARD(3, 0, "M3"),
    CARD(4, 0, "M4"),   CARD(5, 0, "M5"),   CARD(6, 0, "M6"),   CARD(7, 0, "M7"),
    CARD(8, 0, "M8"),   CARD(9, 0, "M9"),   CARD(10, 0, "M10"), CARD(11, 0, "M11"),
    CARD(12, 0, "M12"), CARD(13, 0, "M13"), CARD(14, 0, "M14"), CARD(15, 0, "M15"),
    CARD(16, 0, "M16"), CARD(17, 0, "M17"), CARD(18, 0, "M18"), CARD(19, 0, "M19"),
    CARD(20, 0, "M20"), CARD(21, 0, "M21"),
#define SUIT(s, c) \
    CARD(1, s, "A" c), CARD(2, s, "2" c), CARD(3, s, "3" c), CARD(4, s, "4" c), \
    CARD(5, s, "5" c), CARD(6, s, "6" c), CARD(7, s, "7" c), CARD(8, s, "8" c), \
    CARD(9, s, "9" c), CARD(10, s, "10" c), CARD(11, s, "P" c), CARD(12, s, "N" c), \
    CARD(13, s, "Q" c), CARD(14, s, "K" c)
    SUIT(QD_SUIT_WANDS, "W"), SUIT(QD_SUIT_CUPS, "C"),
    SUIT(QD_SUIT_SWORDS, "S"), SUIT(QD_SUIT_PENTACLES, "P"),
#undef SUIT
};

static void setup(qd_deck_t *deck, const qd_card_t *cards, uint8_t count)
{
    memset(deck, 0, sizeof(*deck));
    deck->cards = cards;
    deck->count = count;
    deck->policy = QD_RESHUFFLE_WHEN_EMPTY;
}

bool qd_deck_poker(qd_deck_t *deck, int jokers)
{
    if (jokers < 0 || jokers > 4)
        return false;
    setup(deck, poker_cards, (uint8_t)(52 + jokers));
    return true;
}

void qd_deck_tarot(qd_deck_t *deck)
{
    setup(deck, tarot_cards, 78);
}

bool qd_deck_custom(qd_deck_t *deck, const qd_card_t *cards, uint16_t count)
{
    if (!cards || !count || count > QD_MAX_CARDS)
        return false;
    setup(deck, cards, (uint8_t)count);
    return true;
}

/* ========================================================================
 * Drawing
 * ======================================================================== */
bool qd_deck_is_out(const qd_deck_t *deck, uint8_t index)
{
    return (deck->out[index / 8] >> (index % 8)) & 1u;
}

int qd_deck_out_count(const qd_deck_t *deck)
{
    int n = 0;
    for (int i = 0; i < deck->count; i++)
        n += qd_deck_is_out(deck, (uint8_t)i);
    return n;
}

int qd_deck_remaining(const qd_deck_t *deck)
{
    return deck->count - qd_deck_out_count(deck);
}

void qd_deck_shuffle(qd_deck_t *deck)
{
    memset(deck->out, 0, sizeof(deck->out));
    deck->triggered = false;
}

void qd_deck_return(qd_deck_t *deck, uint8_t index)
{
    if (index < deck->count)
        deck->out[index / 8] &= (uint8_t)~(1u << (index % 8));
}

/* Take one card, honouring EVERY_DRAW and WHEN_EMPTY (not the trigger,
 * which only acts between deals). */
static bool take(qd_deck_t *deck, qd_card_draw_t *out)
{
    if (deck->policy & QD_RESHUFFLE_EVERY_DRAW)
        memset(deck->out, 0, sizeof(deck->out));
    int left = qd_deck_remaining(deck);
    if (!left) {
        if (!(deck->policy & QD_RESHUFFLE_WHEN_EMPTY))
            return false;
        qd_deck_shuffle(deck);
        left = deck->count;
    }
    /* Pick the k-th card still in the deck. */
    uint32_t k = qd_rng_range((uint32_t)left);
    for (int i = 0; i < deck->count; i++) {
        if (qd_deck_is_out(deck, (uint8_t)i))
            continue;
        if (k-- == 0) {
            deck->out[i / 8] |= (uint8_t)(1u << (i % 8));
            out->index = (uint8_t)i;
            out->card = &deck->cards[i];
            if (deck->cards[i].flags & QD_CARD_TRIGGER)
                deck->triggered = true;
            return true;
        }
    }
    return false;                           /* not reachable */
}

int qd_deck_deal(qd_deck_t *deck, qd_card_draw_t *out, int n)
{
    if ((deck->policy & QD_RESHUFFLE_AFTER_TRIGGER) && deck->triggered)
        qd_deck_shuffle(deck);
    int dealt = 0;
    while (dealt < n && take(deck, &out[dealt]))
        dealt++;
    return dealt;
}

bool qd_deck_draw(qd_deck_t *deck, qd_card_draw_t *out)
{
    return qd_deck_deal(deck, out, 1) == 1;
}

/* ========================================================================
 * Ordering and names
 * ======================================================================== */
int qd_card_order(const qd_card_t *card)
{
    if (card->flags & QD_CARD_TRIGGER)
        return 1000;
    return card->value * 8 + card->suit;
}

char *qd_card_name(const qd_card_t *card, char *buf, int size)
{
    static const char *const ranks[] = { "", "Ace", "Two", "Three", "Four", "Five", "Six",
        "Seven", "Eight", "Nine", "Ten", "Jack", "Queen", "King", "Ace" };
    static const char *const poker_suits[] = { "", "Clubs", "Diamonds", "Hearts", "Spades" };
    static const char *const tarot_ranks[] = { "", "Ace", "Two", "Three", "Four", "Five", "Six",
        "Seven", "Eight", "Nine", "Ten", "Page", "Knight", "Queen", "King" };
    static const char *const tarot_suits[] = { "", "Wands", "Cups", "Swords", "Pentacles" };
    static const char *const major[] = { "The Fool", "The Magician", "The High Priestess",
        "The Empress", "The Emperor", "The Hierophant", "The Lovers", "The Chariot",
        "Strength", "The Hermit", "Wheel of Fortune", "Justice", "The Hanged Man", "Death",
        "Temperance", "The Devil", "The Tower", "The Star", "The Moon", "The Sun",
        "Judgement", "The World" };

    bool tarot = card >= tarot_cards && card < tarot_cards + 78;
    bool poker = card >= poker_cards && card < poker_cards + 56;
    if (!tarot && !poker) {
        snprintf(buf, (size_t)size, "%s", card->label);    /* custom deck */
    } else if (tarot) {
        if (card->suit == QD_SUIT_MAJOR && card->value >= 0 && card->value <= 21)
            snprintf(buf, (size_t)size, "%s", major[card->value]);
        else if (card->suit >= 1 && card->suit <= 4 && card->value >= 1 && card->value <= 14)
            snprintf(buf, (size_t)size, "%s of %s", tarot_ranks[card->value], tarot_suits[card->suit]);
        else
            snprintf(buf, (size_t)size, "%s", card->label);
    } else if (card->suit == QD_SUIT_JOKER && (card->flags & QD_CARD_TRIGGER)) {
        snprintf(buf, (size_t)size, "%s",
                 !strcmp(card->label, "JkR") ? "Red Joker" :
                 !strcmp(card->label, "JkB") ? "Black Joker" : "Joker");
    } else if (card->suit >= 1 && card->suit <= 4 && card->value >= 2 && card->value <= 14) {
        snprintf(buf, (size_t)size, "%s of %s", ranks[card->value], poker_suits[card->suit]);
    } else {
        snprintf(buf, (size_t)size, "%s", card->label);
    }
    return buf;
}
