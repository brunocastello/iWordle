/*
 * Placeholder front end (Agent A): loads the dictionary, starts one game and
 * reports with the speaker: one beep = OK, three = failure. No conio. To be
 * replaced by the real main.c.
 */
#include <stdlib.h>
#include <string.h>

#include "wordle_engine.h"
#include "dict.h"

#define SW(a) (*(volatile unsigned char *)(a))

static WordleGame gGame;

static void Beep(void)
{
    unsigned char i;
    unsigned char d;

    for (i = 0; i < 100; i++) {
        (void)SW(0xC030);
        for (d = 0; d < 20; d++) { }
    }
}

static void Pause(void)
{
    unsigned int n;

    for (n = 0; n < 20000u; n++) { }
}

static void Report(unsigned char count)
{
    while (count--) {
        Beep();
        Pause();
    }
}

int main(void)
{
    unsigned char ok;

    /* Hi-res page 1, full screen, cleared first. */
    memset((void *)0x2000, 0, 0x2000);
    (void)SW(0xC050);
    (void)SW(0xC052);
    (void)SW(0xC054);
    (void)SW(0xC057);

    ok = DictLoad();
    if (ok) {
        WordleSeedRandom(12345UL);
        WordleNewGame(&gGame);
        ok = (unsigned char)(WordleDictAnswerCount() == 2850u
            && WordleIsValidWord(gGame.target)
            && !WordleIsValidWord("QQQQQ"));
    }
    Report(ok ? 1 : 3);

    while (!(SW(0xC000) & 0x80)) { }
    (void)SW(0xC010);
    exit(0);
    return 0;
}
