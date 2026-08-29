#include "wordle_engine.h"
#include "word_list.h"

#include <string.h>

/* Small self-contained LCG so behavior is identical on every platform this
 * engine ever runs on, instead of depending on whatever rand() a given libc
 * happens to ship. */
static unsigned long gRandomState = 1;

void WordleSeedRandom(unsigned long seed)
{
    gRandomState = seed ? seed : 1;
}

static unsigned long NextRandom(void)
{
    gRandomState = gRandomState * 1103515245UL + 12345UL;
    return (gRandomState >> 16) & 0x7FFFUL;
}

int WordleIsValidWord(const char *word5)
{
    int i;
    for (i = 0; i < WORDLE_ANSWER_COUNT; i++) {
        if (strncmp(word5, kWordleAnswers[i], WORDLE_WORD_LENGTH) == 0) {
            return 1;
        }
    }
    for (i = 0; i < WORDLE_VALID_GUESS_COUNT; i++) {
        if (strncmp(word5, kWordleValidGuesses[i], WORDLE_WORD_LENGTH) == 0) {
            return 1;
        }
    }
    return 0;
}

static void ResetRow(WordleRow *row)
{
    int i;
    for (i = 0; i < WORDLE_WORD_LENGTH; i++) {
        row->letters[i] = 0;
        row->states[i] = kLetterEmpty;
    }
}

void WordleNewGame(WordleGame *game)
{
    int i;
    unsigned long pick = NextRandom() % (unsigned long)WORDLE_ANSWER_COUNT;

    memcpy(game->target, kWordleAnswers[pick], WORDLE_WORD_LENGTH);
    game->target[WORDLE_WORD_LENGTH] = '\0';

    for (i = 0; i < WORDLE_MAX_GUESSES; i++) {
        ResetRow(&game->rows[i]);
    }
    for (i = 0; i < WORDLE_ALPHABET_SIZE; i++) {
        game->keyStates[i] = kLetterEmpty;
    }

    game->currentRow = 0;
    game->currentCol = 0;
    game->status = kGameInProgress;
}

void WordleTypeLetter(WordleGame *game, char letter)
{
    if (game->status != kGameInProgress) return;
    if (game->currentRow >= WORDLE_MAX_GUESSES) return;
    if (game->currentCol >= WORDLE_WORD_LENGTH) return;

    game->rows[game->currentRow].letters[game->currentCol] = letter;
    game->currentCol++;
}

void WordleBackspace(WordleGame *game)
{
    if (game->status != kGameInProgress) return;
    if (game->currentCol <= 0) return;

    game->currentCol--;
    game->rows[game->currentRow].letters[game->currentCol] = 0;
}

static void EvaluateGuess(const char *target, const char *guess,
                           WordleLetterState outStates[WORDLE_WORD_LENGTH])
{
    int used[WORDLE_WORD_LENGTH];
    int i, j;

    for (i = 0; i < WORDLE_WORD_LENGTH; i++) {
        used[i] = 0;
        if (guess[i] == target[i]) {
            outStates[i] = kLetterCorrect;
            used[i] = 1;
        } else {
            outStates[i] = kLetterAbsent;
        }
    }

    for (i = 0; i < WORDLE_WORD_LENGTH; i++) {
        if (outStates[i] == kLetterCorrect) continue;
        for (j = 0; j < WORDLE_WORD_LENGTH; j++) {
            if (!used[j] && target[j] == guess[i]) {
                outStates[i] = kLetterPresent;
                used[j] = 1;
                break;
            }
        }
    }
}

static void UpdateKeyStates(WordleGame *game, const WordleRow *row)
{
    int i;
    for (i = 0; i < WORDLE_WORD_LENGTH; i++) {
        int keyIndex = row->letters[i] - 'A';
        if (keyIndex < 0 || keyIndex >= WORDLE_ALPHABET_SIZE) continue;

        /* Never downgrade a key's remembered state (correct beats present
         * beats absent), since a letter can appear in later guesses with a
         * worse outcome than it already proved to have. */
        if (row->states[i] > game->keyStates[keyIndex]) {
            game->keyStates[keyIndex] = row->states[i];
        }
    }
}

WordleSubmitResult WordleSubmitGuess(WordleGame *game)
{
    WordleRow *row;

    if (game->status != kGameInProgress) return kSubmitGameOver;
    if (game->currentCol < WORDLE_WORD_LENGTH) return kSubmitTooShort;

    row = &game->rows[game->currentRow];
    if (!WordleIsValidWord(row->letters)) return kSubmitNotInDictionary;

    EvaluateGuess(game->target, row->letters, row->states);
    UpdateKeyStates(game, row);

    if (strncmp(row->letters, game->target, WORDLE_WORD_LENGTH) == 0) {
        game->status = kGameWon;
    } else if (game->currentRow + 1 >= WORDLE_MAX_GUESSES) {
        game->status = kGameLost;
    } else {
        game->currentRow++;
        game->currentCol = 0;
    }

    return kSubmitOk;
}
