/*
 * Portable Wordle game engine.
 *
 * Pure C99, no platform headers. This file (and wordle_engine.c /
 * word_list.h) must stay free of Mac Toolbox / Carbon / Cocoa dependencies
 * so the same engine can back both the Mac OS 9 front end in
 * src/platform/macos9 and a future Mac OS X front end without changes.
 */
#ifndef WORDLE_ENGINE_H
#define WORDLE_ENGINE_H

#define WORDLE_WORD_LENGTH 5
#define WORDLE_MAX_GUESSES 6
#define WORDLE_ALPHABET_SIZE 26

typedef enum {
    kLetterEmpty = 0,
    kLetterAbsent,
    kLetterPresent,
    kLetterCorrect
} WordleLetterState;

typedef enum {
    kGameInProgress = 0,
    kGameWon,
    kGameLost
} WordleGameStatus;

typedef enum {
    kSubmitOk = 0,
    kSubmitTooShort,
    kSubmitNotInDictionary,
    kSubmitGameOver
} WordleSubmitResult;

typedef struct {
    char letters[WORDLE_WORD_LENGTH];
    WordleLetterState states[WORDLE_WORD_LENGTH];
} WordleRow;

typedef struct {
    char target[WORDLE_WORD_LENGTH + 1];
    WordleRow rows[WORDLE_MAX_GUESSES];
    int currentRow;
    int currentCol;
    WordleLetterState keyStates[WORDLE_ALPHABET_SIZE];
    WordleGameStatus status;
} WordleGame;

/* Platform seeds this once at startup with whatever entropy it has
 * (TickCount() on Mac OS 9, time(NULL) on a future OS X port, etc). */
void WordleSeedRandom(unsigned long seed);

/* Resets game state and picks a new secret word from the dictionary. */
void WordleNewGame(WordleGame *game);

/* Appends one uppercase letter to the current row, if there is room. */
void WordleTypeLetter(WordleGame *game, char letter);

/* Removes the last typed letter from the current row, if any. */
void WordleBackspace(WordleGame *game);

/* Evaluates the current row against the target word if it is full and a
 * valid dictionary word. Updates keyStates and game status on success. */
WordleSubmitResult WordleSubmitGuess(WordleGame *game);

/* 1 if word5 (WORDLE_WORD_LENGTH uppercase letters, no terminator required
 * beyond that) is a recognized dictionary word. */
int WordleIsValidWord(const char *word5);

#endif /* WORDLE_ENGINE_H */
