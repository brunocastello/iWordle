#include <string.h>

#include "wordle_stats.h"

void WordleStatsInit(WordleStatsBook *book)
{
    memset(book, 0, sizeof(*book));
}

void WordleStatsClear(WordleStatsBook *book)
{
    WordleStatsInit(book);
}

static WordlePlayerStats *FindPlayer(WordleStatsBook *book, const char *name)
{
    unsigned short i;
    for (i = 0; i < book->playerCount; i++) {
        if (strcmp(book->players[i].name, name) == 0) return &book->players[i];
    }
    return NULL;
}

void WordleStatsRecordResult(WordleStatsBook *book, const char *name, int won)
{
    WordlePlayerStats *p = FindPlayer(book, name);

    if (p == NULL) {
        if (book->playerCount >= WORDLE_STATS_MAX_PLAYERS) return;
        p = &book->players[book->playerCount++];
        memset(p, 0, sizeof(*p));
        strncpy(p->name, name, WORDLE_STATS_NAME_LEN);
        p->name[WORDLE_STATS_NAME_LEN] = '\0';
    }

    p->gamesPlayed++;
    if (won) {
        p->gamesWon++;
        p->currentStreak++;
        if (p->currentStreak > p->maxStreak) p->maxStreak = p->currentStreak;
    } else {
        p->currentStreak = 0;
    }
}
