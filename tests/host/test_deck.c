/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    test_deck.c
 * @brief   D2: decks - built-in poker and tarot, dealing, reshuffle rules,
 *          names, and fairness of the shuffle.
 */
#include <string.h>

#include "qd4p.h"
#include "qd_test.h"

static void test_golden(void)
{
    static const char *const expect[8] = { "JD", "JkR", "2C", "5H", "JH", "8H", "KH", "2H" };
    qd_deck_t deck;
    qd_deck_poker(&deck, 2);
    qd_rng_seed_fixed(42);
    qd_card_draw_t hand[8];
    int n = qd_deck_deal(&deck, hand, 8);
    CHECK(n == 8, "dealt 8");
    for (int i = 0; i < n; i++)
        CHECK(!strcmp(hand[i].card->label, expect[i]), "card %d: %s, reference %s",
              i, hand[i].card->label, expect[i]);
}

static void test_builtins(void)
{
    qd_deck_t deck;
    CHECK(!qd_deck_poker(&deck, 5) && !qd_deck_poker(&deck, -1), "0..4 jokers only");
    for (int j = 0; j <= 4; j++) {
        qd_deck_poker(&deck, j);
        int triggers = 0;
        for (int i = 0; i < deck.count; i++) triggers += !!(deck.cards[i].flags & QD_CARD_TRIGGER);
        CHECK(deck.count == 52 + j && triggers == j, "poker with %d jokers: %d cards", j, deck.count);
    }
    qd_deck_tarot(&deck);
    CHECK(deck.count == 78, "tarot has 78 cards");
    int labels_unique = 1;
    for (int a = 0; a < 78; a++)
        for (int b = a + 1; b < 78; b++)
            labels_unique &= strcmp(deck.cards[a].label, deck.cards[b].label) != 0;
    CHECK(labels_unique, "tarot labels are all different");

    char name[32];
    CHECK(!strcmp(qd_card_name(&deck.cards[0], name, sizeof(name)), "The Fool"), "M0 is The Fool");
    CHECK(!strcmp(qd_card_name(&deck.cards[21], name, sizeof(name)), "The World"), "M21");
    CHECK(!strcmp(qd_card_name(&deck.cards[22 + 14 + 11], name, sizeof(name)), "Knight of Cups"),
          "Knight of Cups: %s", name);
    qd_deck_poker(&deck, 2);
    CHECK(!strcmp(qd_card_name(&deck.cards[51], name, sizeof(name)), "Ace of Spades"), "AS");
    CHECK(!strcmp(qd_card_name(&deck.cards[52], name, sizeof(name)), "Red Joker"), "JkR");
    CHECK(!strcmp(qd_card_name(&deck.cards[0], name, sizeof(name)), "Two of Clubs"), "2C");

    /* Savage Worlds order: joker > A spades > A hearts > ... > 2 clubs */
    CHECK(qd_card_order(&deck.cards[52]) > qd_card_order(&deck.cards[51]), "joker beats the ace of spades");
    CHECK(qd_card_order(&deck.cards[51]) > qd_card_order(&deck.cards[38]), "ace of spades beats ace of hearts");
    CHECK(qd_card_order(&deck.cards[12]) > qd_card_order(&deck.cards[13]) &&
          qd_card_order(&deck.cards[12]) > qd_card_order(&deck.cards[11]),
          "ace of clubs beats the two of diamonds (rank before suit) and the king of clubs");
}

static void test_dealing(void)
{
    qd_deck_t deck;
    qd_card_draw_t cards[60];
    qd_deck_poker(&deck, 0);
    deck.policy = 0;                                       /* on demand only */
    int n = qd_deck_deal(&deck, cards, 60);
    CHECK(n == 52, "a 52-card deck deals 52 then stops: %d", n);
    int seen[52] = { 0 }, dup = 0;
    for (int i = 0; i < n; i++) dup |= seen[cards[i].index]++;
    CHECK(!dup, "no card dealt twice");
    CHECK(qd_deck_remaining(&deck) == 0 && !qd_deck_draw(&deck, cards), "empty deck refuses to draw");
    qd_deck_return(&deck, 7);
    CHECK(qd_deck_draw(&deck, cards) && cards[0].index == 7, "a returned card is the only one left");
    qd_deck_shuffle(&deck);
    CHECK(qd_deck_remaining(&deck) == 52, "shuffle puts every card back");

    deck.policy = QD_RESHUFFLE_WHEN_EMPTY;
    n = qd_deck_deal(&deck, cards, 60);
    CHECK(n == 60 && qd_deck_out_count(&deck) == 8, "WHEN_EMPTY reshuffles mid-deal: 60 dealt, 8 out");

    deck.policy = QD_RESHUFFLE_EVERY_DRAW;
    qd_deck_shuffle(&deck);
    for (int i = 0; i < 10; i++) qd_deck_draw(&deck, cards);
    CHECK(qd_deck_out_count(&deck) == 1, "EVERY_DRAW: only the last card is out");
}

static void test_trigger(void)
{
    /* Savage Worlds: a joker comes up during a round's deal; the deal
     * finishes, and the next round's deal starts with a reshuffle. */
    qd_deck_t deck;
    qd_deck_poker(&deck, 2);
    deck.policy = QD_RESHUFFLE_AFTER_TRIGGER | QD_RESHUFFLE_WHEN_EMPTY;
    qd_card_draw_t hand[54];
    int rounds = 0, checked = 0, ok_mid = 1, ok_next = 1;
    qd_rng_init_hw();
    for (int r = 0; r < 2000; r++) {
        int before_out = qd_deck_out_count(&deck);
        bool armed = deck.triggered;
        int n = qd_deck_deal(&deck, hand, 5);
        int joker_at = -1;
        for (int i = 0; i < n; i++)
            if (hand[i].card->flags & QD_CARD_TRIGGER) joker_at = i;
        if (armed) {                                  /* previous deal had a joker */
            ok_next &= qd_deck_out_count(&deck) == 5; /* reshuffled, then 5 out */
            checked++;
        } else if (before_out + 5 <= 54) {
            ok_mid &= qd_deck_out_count(&deck) == before_out + 5;   /* no reshuffle mid-deal */
        }
        (void)joker_at;
        rounds++;
    }
    CHECK(checked > 100, "jokers came up in %d of %d rounds", checked, rounds);
    CHECK(ok_next, "the deal after a joker starts from a full deck");
    CHECK(ok_mid, "without a joker, cards stay out between deals");

    qd_deck_poker(&deck, 2);
    deck.policy = QD_RESHUFFLE_WHEN_EMPTY;                 /* trigger rule off */
    while (!deck.triggered) qd_deck_draw(&deck, hand);
    int out = qd_deck_out_count(&deck);
    qd_deck_draw(&deck, hand);
    CHECK(qd_deck_out_count(&deck) == out + 1, "without AFTER_TRIGGER a joker changes nothing");
}

static void test_fair_shuffle(void)
{
    /* The first card of a fresh deck is equally likely to be any of 54;
     * the 30th card too (it's a uniform draw from what's left). */
    static uint32_t first[54], thirtieth[54];
    memset(first, 0, sizeof(first));
    memset(thirtieth, 0, sizeof(thirtieth));
    qd_deck_t deck;
    qd_deck_poker(&deck, 2);
    qd_card_draw_t hand[30];
    const int trials = 270000;
    for (int t = 0; t < trials; t++) {
        qd_deck_shuffle(&deck);
        qd_deck_deal(&deck, hand, 30);
        first[hand[0].index]++;
        thirtieth[hand[29].index]++;
    }
    double chi1 = 0, chi30 = 0, e = trials / 54.0;
    for (int i = 0; i < 54; i++) {
        chi1 += (first[i] - e) * (first[i] - e) / e;
        chi30 += (thirtieth[i] - e) * (thirtieth[i] - e) / e;
    }
    CHECK(chi1 < 88.0, "first card uniform over 54: chi-square %.1f", chi1);
    CHECK(chi30 < 88.0, "30th card uniform over 54: chi-square %.1f", chi30);
    printf("  shuffle: first card chi-square %.1f, 30th card %.1f (limit 88.0)\n", chi1, chi30);
}

static void test_custom(void)
{
    static const qd_card_t omens[3] = {
        { 1, 0, 0, "Sun" }, { 2, 0, 0, "Moon" }, { 3, 0, QD_CARD_TRIGGER, "Doom" },
    };
    qd_deck_t deck;
    CHECK(!qd_deck_custom(&deck, omens, 0), "an empty custom deck is refused");
    CHECK(qd_deck_custom(&deck, omens, 3) && deck.count == 3, "custom deck of 3");
    qd_card_draw_t c[3];
    CHECK(qd_deck_deal(&deck, c, 3) == 3, "all three dealt");
    char name[16];
    CHECK(!strcmp(qd_card_name(&deck.cards[2], name, sizeof(name)), "Doom"),
          "custom cards are named by label");

    /* Bitmap compatible with the settings store's DECK record. */
    qd_deck_t big;
    static qd_card_t many[255];
    for (int i = 0; i < 255; i++) many[i].value = (int8_t)(i % 100);
    qd_deck_custom(&big, many, 255);
    qd_card_draw_t one;
    for (int i = 0; i < 254; i++) qd_deck_draw(&big, &one);
    CHECK(qd_deck_remaining(&big) == 1 && sizeof(big.out) == 32, "255-card deck, 32-byte bitmap");
}

int main(void)
{
    test_golden();
    qd_rng_init_hw();
    test_builtins();
    test_dealing();
    test_trigger();
    test_fair_shuffle();
    test_custom();
    return qd_test_done("decks");
}
