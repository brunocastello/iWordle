/*
 * iWordle for the Apple II (any 64K machine running ProDOS 8, plain 6502).
 *
 * Every screen is drawn in hi-res with our own font (hires.c / font.c), so
 * lowercase works even on a II+, and there is no text mode or conio. Mono
 * only, like the System 7 build on a 1-bit screen. The portable engine lives
 * in src/core; the word lists come from WORDS.DAT (dict.c).
 */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wordle_engine.h"
#include "wordle_stats.h"
#include "dict.h"
#include "hires.h"

/* hires.c, font.c and dict.c fill the low RAM below the hi-res page; the
 * screen flow goes up to the main code area. */
#pragma code-name (push, "CODE")

/* ---------------------------------------------------------------------- */
/* Layout (byte columns and lines)                                         */
/* ---------------------------------------------------------------------- */

#define TILE_X0     10
#define TILE_Y0     10
#define TILE_W      3
#define TILE_H      20
#define TILE_PITCH_X 4
#define TILE_PITCH_Y 22

#define KEY_H       14
#define KEY_Y1      144
#define KEY_PITCH_Y 16

#define ROW(r)      ((r) * 8)

/* Cell states: the engine's WordleLetterState values plus "typed". */
#define CS_EMPTY    0
#define CS_ABSENT   1
#define CS_PRESENT  2
#define CS_CORRECT  3
#define CS_TYPED    4

static const char kKeys[] = "QWERTYUIOPASDFGHJKLZXCVBNM";

static WordleGame gGame;
static WordleStatsBook gStats;
static char gName[WORDLE_STATS_NAME_LEN + 1];
static char gLastName[WORDLE_STATS_NAME_LEN + 1];
static char gMsg[41];
static unsigned char gShown[26];   /* key states currently on screen */
static unsigned char gPending;     /* a finished game still to be recorded */

/* ---------------------------------------------------------------------- */
/* Small helpers                                                           */
/* ---------------------------------------------------------------------- */

static void UtoA(unsigned int v, char *out)
{
    char t[6];
    unsigned char n, i;

    n = 0;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    for (i = 0; n; i++) out[i] = t[--n];
    out[i] = '\0';
}

static void PutNum(unsigned char col, unsigned char y, unsigned int v)
{
    char buf[6];

    if (v > 999) v = 999;
    UtoA(v, buf);
    DrawText(col, y, buf, 0);
}

static void TitleBar(const char *s)
{
    FillRect(0, 0, 40, 8, FILL_WHITE);
    DrawTextCentered(0, s, GL_INVERT);
}

/* ---------------------------------------------------------------------- */
/* Stats file                                                              */
/* ---------------------------------------------------------------------- */

static void LoadStats(void)
{
    int fd;
    unsigned char i, ok;

    ok = 0;
    fd = open("STATS", O_RDONLY);
    if (fd >= 0) {
        ok = (unsigned char)(read(fd, &gStats, sizeof(gStats))
                == (int)sizeof(gStats)
            && gStats.playerCount <= WORDLE_STATS_MAX_PLAYERS);
        close(fd);
    }
    if (!ok) WordleStatsInit(&gStats);
    for (i = 0; i < WORDLE_STATS_MAX_PLAYERS; i++) {
        gStats.players[i].name[WORDLE_STATS_NAME_LEN] = '\0';
    }
}

/* A write-protected disk just means nothing is saved. */
static void SaveStats(void)
{
    int fd;

    fd = open("STATS", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0) {
        write(fd, &gStats, sizeof(gStats));
        close(fd);
    }
}

/* ---------------------------------------------------------------------- */
/* Board drawing                                                           */
/* ---------------------------------------------------------------------- */

/* One tile or key. See the state table in docs/apple2-port-plan.md. */
static void DrawCell(unsigned char x, unsigned char y, unsigned char w,
                     unsigned char h, unsigned char st, unsigned char ch,
                     unsigned char key)
{
    static const unsigned char kThick[5] = { 1, 0, 1, 0, 2 };
    static const unsigned char kFill[5] = {
        FILL_BLACK, FILL_BLACK, FILL_GRAY, FILL_WHITE, FILL_BLACK
    };
    unsigned char flags;

    DrawBox(x, y, w, h, kThick[st], kFill[st], key);
    if (ch == 0) return;
    flags = (st == CS_CORRECT) ? GL_INVERT : (st == CS_ABSENT) ? GL_DIM : 0;
    if (!key) flags |= GL_DOUBLE;
    if (st == CS_PRESENT) FillRect(x + 1, y + 2, 1, key ? 10 : 16, FILL_BLACK);
    DrawGlyph(x + 1, y + 3, ch, flags);
}

static void DrawTile(unsigned char r, unsigned char c)
{
    unsigned char st, ch;

    ch = (unsigned char)gGame.rows[r].letters[c];
    st = (unsigned char)gGame.rows[r].states[c];
    if (st == CS_EMPTY && ch) st = CS_TYPED;
    DrawCell(TILE_X0 + c * TILE_PITCH_X, TILE_Y0 + r * TILE_PITCH_Y,
             TILE_W, TILE_H, st, ch, 0);
}

static void DrawRow(unsigned char r)
{
    unsigned char c;

    for (c = 0; c < WORDLE_WORD_LENGTH; c++) DrawTile(r, c);
}

static void DrawKey(unsigned char pos)
{
    unsigned char x, y, ch, st;

    if (pos < 10) {
        x = 5 + pos * 3;
        y = KEY_Y1;
    } else if (pos < 19) {
        x = 6 + (pos - 10) * 3;
        y = KEY_Y1 + KEY_PITCH_Y;
    } else {
        x = 9 + (pos - 19) * 3;
        y = KEY_Y1 + 2 * KEY_PITCH_Y;
    }
    ch = (unsigned char)kKeys[pos];
    st = (unsigned char)gGame.keyStates[ch - 'A'];
    gShown[pos] = st;
    DrawCell(x, y, 3, KEY_H, st, ch, 1);
}

/* Only the keys whose state changed since they were last drawn. */
static void DrawKeys(void)
{
    unsigned char pos;

    for (pos = 0; pos < 26; pos++) {
        if (gShown[pos] != (unsigned char)gGame.keyStates[kKeys[pos] - 'A']) {
            DrawKey(pos);
        }
    }
}

static void DrawMessageLine(void)
{
    FillRect(0, 0, 40, 8, FILL_BLACK);
    if (gMsg[0]) DrawTextCentered(0, gMsg, 0);
    else DrawText(31, 0, "ESC: MENU", 0);
}

static void SetMsg(const char *s)
{
    strcpy(gMsg, s);
    DrawMessageLine();
}

static void RedrawGame(void)
{
    unsigned char r;

    HiresClear();
    DrawMessageLine();
    for (r = 0; r < WORDLE_MAX_GUESSES; r++) DrawRow(r);
    memset(gShown, 0xFF, sizeof(gShown));
    DrawKeys();
    DrawBox(2, KEY_Y1 + 2 * KEY_PITCH_Y, 7, KEY_H, 1, FILL_BLACK, 1);
    DrawText(3, KEY_Y1 + 2 * KEY_PITCH_Y + 3, "ENTER", 0);
    DrawBox(30, KEY_Y1 + 2 * KEY_PITCH_Y, 7, KEY_H, 1, FILL_BLACK, 1);
    DrawText(32, KEY_Y1 + 2 * KEY_PITCH_Y + 3, "DEL", 0);
}

/* ---------------------------------------------------------------------- */
/* Screens                                                                 */
/* ---------------------------------------------------------------------- */

static void ShowAbout(void)
{
    HiresClear();
    DrawTextCentered(32, "iWordle 1.2", GL_DOUBLE);
    DrawTextCentered(64, "A native Wordle clone for the Apple II", 0);
    DrawTextCentered(88, "Bruno Castello", 0);
    DrawTextCentered(104, "bfcastello@hotmail.com", 0);
    DrawTextCentered(128, "Engineer: Claude Sonnet 5", 0);
    DrawTextCentered(144, "(c) Castello Designs, 2026", 0);
    DrawTextCentered(160, "Built with cc65", 0);
    DrawTextCentered(184, "PRESS ANY KEY", 0);
    WaitKey();
}

static void ShowRanking(void)
{
    unsigned char i, k, y;
    WordlePlayerStats *p;
    char buf[17];

    for (;;) {
        HiresClear();
        TitleBar("Ranking");
        DrawText(1, 24, "Name", 0);
        DrawText(19, 24, "Played", 0);
        DrawText(26, 24, "Win%", 0);
        DrawText(32, 24, "Cur", 0);
        DrawText(36, 24, "Max", 0);
        FillRect(1, 33, 38, 1, FILL_WHITE);
        if (gStats.playerCount == 0) {
            DrawTextCentered(80, "No players yet.", 0);
        }
        for (i = 0; i < gStats.playerCount; i++) {
            p = &gStats.players[i];
            y = 40 + i * 10;
            strncpy(buf, p->name, 16);
            buf[16] = '\0';
            DrawText(1, y, buf, 0);
            PutNum(19, y, p->gamesPlayed);
            PutNum(26, y, p->gamesPlayed == 0 ? 0
                : p->gamesPlayed > 655
                    ? p->gamesWon / (p->gamesPlayed / 100)
                    : (p->gamesWon * 100) / p->gamesPlayed);
            PutNum(32, y, p->currentStreak);
            PutNum(36, y, p->maxStreak);
        }
        DrawTextCentered(176, "C: CLEAR   ESC: BACK", 0);

        for (;;) {
            k = ToUpper(WaitKey());
            if (k == 0x1B || k == 0x0D) return;
            if (k == 'C' && gStats.playerCount) {
                FillRect(0, 176, 40, 8, FILL_BLACK);
                DrawTextCentered(176, "CLEAR ALL? Y/N", 0);
                k = ToUpper(WaitKey());
                if (k == 'Y') {
                    WordleStatsClear(&gStats);
                    SaveStats();
                }
                break;
            }
        }
    }
}

/* Asks for the player's name (the last one prefilled). Returns 1 if a name
 * was entered, 0 if it was skipped. */
static unsigned char PromptName(void)
{
    unsigned char len, k;

    HiresClear();
    TitleBar("Game over");
    DrawTextCentered(56, "Your name for the ranking:", 0);
    DrawTextCentered(168, "RETURN: OK   ESC: SKIP", 0);
    strcpy(gName, gLastName);
    len = (unsigned char)strlen(gName);
    DrawText(4, 80, gName, 0);
    DrawGlyph(4 + len, 80, ' ', GL_INVERT);
    for (;;) {
        k = WaitKey();
        if (k == 0x0D) break;
        if (k == 0x1B) {
            len = 0;
            break;
        }
        if (k == 0x08 || k == 0x7F) {
            if (len) {
                DrawGlyph(4 + len, 80, ' ', 0);
                len--;
                gName[len] = '\0';
                DrawGlyph(4 + len, 80, ' ', GL_INVERT);
            }
        } else if (len < WORDLE_STATS_NAME_LEN
                && ((k >= 'A' && k <= 'Z') || (k >= 'a' && k <= 'z')
                    || (k >= '0' && k <= '9') || k == ' ' || k == '.'
                    || k == '\'' || k == '-')) {
            gName[len] = (char)k;
            gName[len + 1] = '\0';
            DrawGlyph(4 + len, 80, k, 0);
            len++;
            DrawGlyph(4 + len, 80, ' ', GL_INVERT);
        }
    }
    gName[len] = '\0';
    return (unsigned char)(len > 0);
}

/* ---------------------------------------------------------------------- */
/* Game actions                                                            */
/* ---------------------------------------------------------------------- */

static void OnNewGame(void)
{
    WordleNewGame(&gGame);
    gMsg[0] = '\0';
    gPending = 0;
}

static void BuildEndMessage(void)
{
    char num[6];

    if (gGame.status == kGameWon) {
        UtoA((unsigned int)(gGame.currentRow + 1), num);
        strcpy(gMsg, "YOU GOT IT IN ");
        strcat(gMsg, num);
        strcat(gMsg, "!");
    } else {
        strcpy(gMsg, "THE WORD WAS ");
        strcat(gMsg, gGame.target);
    }
    DrawMessageLine();
    gPending = 1;
}

static void OnSubmit(void)
{
    switch (WordleSubmitGuess(&gGame)) {
        case kSubmitTooShort:
            Beep();
            break;
        case kSubmitNotInDictionary:
            Beep();
            SetMsg("NOT IN WORD LIST");
            break;
        case kSubmitOk:
            DrawRow((unsigned char)gGame.currentRow
                - (gGame.status == kGameInProgress));
            DrawKeys();
            if (gGame.status != kGameInProgress) BuildEndMessage();
            break;
        default:
            break;
    }
}

static void OnGiveUp(void)
{
    gGame.status = kGameLost;
    BuildEndMessage();
}

/* Name prompt, record the result, show the ranking. */
static void RecordGameResult(void)
{
    gPending = 0;
    if (PromptName()) {
        WordleStatsRecordResult(&gStats, gName, gGame.status == kGameWon);
        strcpy(gLastName, gName);
        SaveStats();
        ShowRanking();
    }
    strcpy(gMsg, "PRESS RETURN FOR A NEW GAME");
}

/* Esc menu. Returns when the player goes back to the board. */
static void Menu(void)
{
    unsigned char k;

    for (;;) {
        HiresClear();
        TitleBar("iWordle");
        DrawText(12, 40, "N    New game", 0);
        if (gGame.status == kGameInProgress) DrawText(12, 56, "G    Give up", 0);
        DrawText(12, 72, "R    Ranking", 0);
        DrawText(12, 88, "A    About", 0);
        DrawText(12, 104, "Q    Quit", 0);
        DrawText(12, 128, "Esc  Back to the game", 0);
        k = ToUpper(WaitKey());
        if (k == 0x1B) return;
        if (k == 'N') {
            OnNewGame();
            return;
        }
        if (k == 'G' && gGame.status == kGameInProgress) {
            OnGiveUp();
            return;
        }
        if (k == 'R') ShowRanking();
        if (k == 'A') ShowAbout();
        if (k == 'Q') {
            HiresOff();
            exit(0);
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Startup                                                                 */
/* ---------------------------------------------------------------------- */

static void Fatal(const char *s)
{
    DrawTextCentered(104, s, 0);
    DrawTextCentered(120, "PRESS ANY KEY", 0);
    WaitKey();
    HiresOff();
    exit(1);
}

int main(void)
{
    unsigned char k, u;
    unsigned char r, c;

    HiresInit();
    DrawTextCentered(88, "LOADING WORDS...", 0);
    if (!DictLoad()) Fatal("WORDS.DAT IS MISSING OR DAMAGED.");
    LoadStats();

    HiresClear();
    DrawTextCentered(56, "iWordle", GL_DOUBLE);
    DrawTextCentered(80, "1.2", 0);
    DrawTextCentered(120, "PRESS ANY KEY", 0);
    WaitKey();
    WordleSeedRandom(gTicks + 1);

    OnNewGame();
    RedrawGame();
    for (;;) {
        k = WaitKey();
        if (gPending) {
            RecordGameResult();
            RedrawGame();
            continue;
        }
        if (k == 0x1B) {
            Menu();
            RedrawGame();
            continue;
        }
        if (gGame.status != kGameInProgress) {
            if (k == 0x0D || ToUpper(k) == 'N') {
                OnNewGame();
                RedrawGame();
            }
            continue;
        }
        if (gMsg[0]) {
            gMsg[0] = '\0';
            DrawMessageLine();
        }
        r = (unsigned char)gGame.currentRow;
        c = (unsigned char)gGame.currentCol;
        if (k == 0x08 || k == 0x7F) {
            if (c) {
                WordleBackspace(&gGame);
                DrawTile(r, c - 1);
            }
        } else if (k == 0x0D) {
            OnSubmit();
        } else {
            u = ToUpper(k);
            if (u >= 'A' && u <= 'Z' && c < WORDLE_WORD_LENGTH) {
                WordleTypeLetter(&gGame, (char)u);
                DrawTile(r, c);
            }
        }
    }
    return 0;
}
