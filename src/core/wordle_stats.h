/*
 * Portable per-player statistics tracking (games played/won, streaks).
 *
 * Pure C99, no platform headers -- same rule as wordle_engine.h. The
 * Mac OS 9 front end owns reading/writing this to disk; this file only
 * knows how to keep the in-memory book correct.
 */
#ifndef WORDLE_STATS_H
#define WORDLE_STATS_H

#define WORDLE_STATS_MAX_PLAYERS 10
#define WORDLE_STATS_NAME_LEN 31

typedef struct {
    char name[WORDLE_STATS_NAME_LEN + 1];
    unsigned short gamesPlayed;
    unsigned short gamesWon;
    unsigned short currentStreak;
    unsigned short maxStreak;
} WordlePlayerStats;

typedef struct {
    WordlePlayerStats players[WORDLE_STATS_MAX_PLAYERS];
    unsigned short playerCount;
} WordleStatsBook;

void WordleStatsInit(WordleStatsBook *book);

/* Wipes every recorded player back to zero. */
void WordleStatsClear(WordleStatsBook *book);

/* Finds name's existing record (case-sensitive) or creates one if there's
 * room, then applies one game's result to it: increments gamesPlayed,
 * and on a win, gamesWon/currentStreak (bumping maxStreak if it's a new
 * high); a loss resets currentStreak to 0. Silently does nothing if the
 * book is full and name isn't already in it. */
void WordleStatsRecordResult(WordleStatsBook *book, const char *name, int won);

#endif /* WORDLE_STATS_H */
