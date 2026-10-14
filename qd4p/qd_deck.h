/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_deck.h
 * @brief   Card decks: poker (with jokers), tarot, or your own cards, with
 *          rules for when the deck is reshuffled.
 *
 * QUICK START: Savage Worlds initiative
 *
 *     qd_deck_t deck;
 *     qd_deck_poker(&deck, 2);                       // 52 cards + 2 jokers
 *     deck.policy = QD_RESHUFFLE_AFTER_TRIGGER | QD_RESHUFFLE_WHEN_EMPTY;
 *
 *     qd_card_draw_t hand[5];
 *     int n = qd_deck_deal(&deck, hand, 5);        // one round's deal
 *     printf("%s\n", hand[0].card->label);          // e.g. "QH"
 *     // if a joker came up, the NEXT qd_deck_deal starts with a reshuffle
 *
 * HOW A DECK WORKS
 *   A deck is a list of card definitions plus a record of which cards are
 *   out (dealt, in a hand or discarded). Drawing picks one of the cards
 *   still in the deck at random, which is exactly what drawing the top
 *   card of a well-shuffled deck does, so there is no "order" to store.
 *   The "out" record is a bitmap: 1 bit per card, which is also how the
 *   Dice Roller's settings store saves a deck between sessions.
 *
 * RESHUFFLE POLICY (combine with |)
 *   QD_RESHUFFLE_EVERY_DRAW     every card goes back before the next draw,
 *                               so draws are independent (a card-shaped die)
 *   QD_RESHUFFLE_WHEN_EMPTY     an empty deck reshuffles instead of failing
 *   QD_RESHUFFLE_AFTER_TRIGGER  once a trigger card (a joker in the built-in
 *                               poker deck) is dealt, the next deal starts
 *                               with a reshuffle. A deal in progress
 *                               finishes first.
 *   0 (no flags)                only qd_deck_shuffle() puts cards back
 *
 * MANUAL: docs/manual/reference/decks.md, every function with an example.
 */
#ifndef QD_DECK_H
#define QD_DECK_H

#include <stdbool.h>
#include <stdint.h>

#include "qd_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One card definition. */
typedef struct {
    int8_t  value;                      /**< rank: 2..14 in poker (ace high) */
    uint8_t suit;                       /**< suit or group (see QD_SUIT_*) */
    uint8_t flags;                      /**< QD_CARD_TRIGGER, ... */
    char    label[QD_MAX_LABEL + 1];    /**< short name, e.g. "10S", "Jk1", "M0" */
} qd_card_t;

#define QD_CARD_TRIGGER 0x01            /**< dealing it arms AFTER_TRIGGER */

/* Suits in the built-in decks. Poker suits are numbered in Savage Worlds
 * initiative order (spades beat hearts beat diamonds beat clubs). */
#define QD_SUIT_JOKER     0
#define QD_SUIT_CLUBS     1
#define QD_SUIT_DIAMONDS  2
#define QD_SUIT_HEARTS    3
#define QD_SUIT_SPADES    4
#define QD_SUIT_MAJOR     0             /* tarot major arcana */
#define QD_SUIT_WANDS     1
#define QD_SUIT_CUPS      2
#define QD_SUIT_SWORDS    3
#define QD_SUIT_PENTACLES 4

#define QD_RESHUFFLE_EVERY_DRAW    0x01
#define QD_RESHUFFLE_WHEN_EMPTY    0x02
#define QD_RESHUFFLE_AFTER_TRIGGER 0x04

/** A deck: which cards it has, which are out, and its reshuffle rules. */
typedef struct {
    const qd_card_t *cards;             /**< card definitions (not copied) */
    uint8_t  count;                     /**< cards in the full deck */
    uint8_t  policy;                    /**< QD_RESHUFFLE_* flags */
    bool     triggered;                 /**< a trigger card is out: reshuffle next deal */
    uint8_t  out[(QD_MAX_CARDS + 7) / 8];/**< 1 bit per card: dealt/discarded */
} qd_deck_t;

/** A card that was drawn. */
typedef struct {
    uint8_t index;                      /**< which card in the deck, 0 .. count-1 */
    const qd_card_t *card;              /**< its definition */
} qd_card_draw_t;

/* --- Making decks ----------------------------------------------------------- */

/** A poker deck: 52 cards plus 0..4 jokers (Savage Worlds uses 2). Jokers
 *  are trigger cards. Policy starts as QD_RESHUFFLE_WHEN_EMPTY. */
bool qd_deck_poker(qd_deck_t *deck, int jokers);

/** A 78-card tarot deck: 22 major arcana (suit QD_SUIT_MAJOR, value 0..21)
 *  then four suits of 14 (value 1..14: ace .. ten, page, knight, queen,
 *  king). Policy starts as QD_RESHUFFLE_WHEN_EMPTY. */
void qd_deck_tarot(qd_deck_t *deck);

/** A deck of your own cards (not copied: they must stay in memory, e.g. a
 *  const array). Up to QD_MAX_CARDS. Policy starts as WHEN_EMPTY. */
bool qd_deck_custom(qd_deck_t *deck, const qd_card_t *cards, uint16_t count);

/* --- Using decks -------------------------------------------------------------- */

/** One deal of up to `n` cards into `out`. Reshuffle rules are applied: a
 *  pending trigger reshuffle happens before the first card; an empty deck
 *  reshuffles mid-deal only with WHEN_EMPTY. Returns the number of cards
 *  dealt (fewer than n only if the deck ran out). */
int qd_deck_deal(qd_deck_t *deck, qd_card_draw_t *out, int n);

/** One card (a deal of one). Returns false if the deck is empty. */
bool qd_deck_draw(qd_deck_t *deck, qd_card_draw_t *out);

/** Put every card back. Clears a pending trigger. */
void qd_deck_shuffle(qd_deck_t *deck);

/** Put one card back into the deck (e.g. returned from a hand). */
void qd_deck_return(qd_deck_t *deck, uint8_t index);

/** Cards still in the deck, and cards out of it. */
int qd_deck_remaining(const qd_deck_t *deck);
int qd_deck_out_count(const qd_deck_t *deck);

/** Is this card out of the deck? */
bool qd_deck_is_out(const qd_deck_t *deck, uint8_t index);

/** A sort key for initiative: higher deals first. Jokers beat everything,
 *  then rank, then suit (spades high). */
int qd_card_order(const qd_card_t *card);

/** The card's full name, e.g. "Queen of Hearts", "Red Joker", "The Fool",
 *  "Knight of Cups" for cards of the built-in decks; a custom card's name
 *  is its label. Writes into buf (at most size bytes) and returns it. */
char *qd_card_name(const qd_card_t *card, char *buf, int size);

#ifdef __cplusplus
}
#endif

#endif /* QD_DECK_H */
