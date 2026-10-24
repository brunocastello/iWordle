/*
 * iWordle - System 7 / 68k front end.
 *
 * All game rules live in the portable core (src/core/wordle_engine.*).
 * This file only knows about classic QuickDraw, the Window/Menu/Dialog
 * Managers, and the WaitNextEvent loop -- no Carbon, no Appearance Manager.
 * Built by Retro68's m68k toolchain against Apple's Universal Interfaces
 * (third_party/InterfacesAndLibraries) for a Mac SE/30 class machine: a
 * 512x342, 1-bit screen on a 16 MHz 68030. Because of that screen, tiles and
 * keys are drawn with a monochrome idiom (black / gray / dimmed / 2px frame)
 * unless the main display is really running at 4 bits or more, in which
 * case the CLAUDE.md color palette is used (see UseColor()).
 */

#include <Quickdraw.h>
#include <QuickdrawText.h>
#include <Windows.h>
#include <Menus.h>
#include <Fonts.h>
#include <Dialogs.h>
#include <Events.h>
#include <ToolUtils.h>
#include <TextUtils.h>
#include <OSUtils.h>
#include <Sound.h>
#include <Icons.h>
#include <AppleEvents.h>
#include <Files.h>
#include <Folders.h>
#include <Gestalt.h>
#include <Devices.h>
#include <Resources.h>
#include <TextEdit.h>
#include <Memory.h>
#include <Processes.h>

#include <string.h>
#include <ctype.h>

#include "wordle_engine.h"
#include "wordle_stats.h"

/* Not declared by the headers; 3 is Geneva's well-known classic font ID
 * (systemFont=0 is Chicago, applFont=1, newYork=2, geneva=3, ...). */
#define kFontGeneva 3
#define kFontChicago 0

/* ---------------------------------------------------------------------- */
/* Menu IDs (must match iWordle.r)                                        */
/* ---------------------------------------------------------------------- */

enum {
    kMenuApple = 1,
    kMenuFile = 128,
    kMenuEdit = 129
};

/* Resource IDs (must match iWordle.r) */
enum {
    kWindMain = 128,
    kAlertNeedSystem7 = 129,
    kAlertMessage = 200,
    kWindAbout = 201,
    kDialogName = 202,
    kDialogRanking = 203
};

/* ---------------------------------------------------------------------- */
/* Board / keyboard layout                                                */
/* ---------------------------------------------------------------------- */

/* Window content is 292 x 286 (WIND 128 in iWordle.r). The widest row (the
 * 10-key QWERTY row) sets the content width and everything else is centered
 * within it. Recompute WIND's bounds in iWordle.r if any of this moves. */
#define MARGIN 8

#define TILE_SIZE      28
#define TILE_GAP       4
#define BOARD_TOP      MARGIN
#define BOARD_LEFT     68              /* (292 - 156) / 2 */

#define KEY_W              24
#define KEY_H              22
#define KEY_GAP            4
#define KEY_ROW_GAP        4
#define KEYBOARD_TOP       204         /* 8 + 188 + 8 */
#define KB_ROW0_LEFT       MARGIN
#define KB_ROW1_LEFT       22
#define KB_ROW2_LEFT       22
#define KB_BACKSPACE_WIDTH 52
#define KB_ROW_COUNT       3

static const char * const kKBRows[KB_ROW_COUNT] = {
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM"
};

/* ---------------------------------------------------------------------- */
/* Color-mode palette (see CLAUDE.md); unused on a 1-bit screen            */
/* ---------------------------------------------------------------------- */

#define C16(x) ((unsigned short)(((x) << 8) | (x)))

static const RGBColor kColorCorrect  = { C16(0x6A), C16(0xAA), C16(0x64) };
static const RGBColor kColorPresent  = { C16(0xC9), C16(0xB4), C16(0x58) };
static const RGBColor kColorAbsent   = { C16(0x78), C16(0x7C), C16(0x7E) };
static const RGBColor kColorWhite    = { 0xFFFF, 0xFFFF, 0xFFFF };
static const RGBColor kColorBorder   = { C16(0x33), C16(0x33), C16(0x33) };
static const RGBColor kColorKeyFace  = { C16(0xEE), C16(0xEE), C16(0xEE) };

/* ---------------------------------------------------------------------- */
/* Globals                                                                 */
/* ---------------------------------------------------------------------- */

static WindowPtr gWindow;
static WordleGame gGame;
static Boolean gDone = false;
static WordleStatsBook gStats;
static Str255 gLastPlayerName = { 0 };

static Boolean gHasColorQD = false;   /* Color QuickDraw present (not the same as color screen) */
static Boolean gUseColor = false;     /* cached UseColor(), refreshed on each full redraw */
static Boolean gHasAppleEvents = false;

/* UPPs are created once; on 68k they are plain procedure pointers. */
static UserItemUPP gRingUPP;
static UserItemUPP gTableUPP;

/* ---------------------------------------------------------------------- */
/* Forward declarations                                                   */
/* ---------------------------------------------------------------------- */

static pascal void DefaultRingProc(DialogRef dlg, DialogItemIndex itemNo);
static pascal void StatsTableProc(DialogRef dlg, DialogItemIndex itemNo);

static void DrawAboutWindowContent(WindowPtr w);

static void PStrToCStr(char *dst, ConstStr255Param src, size_t dstSize);
static Boolean GetStatsFileSpec(FSSpec *spec);
static void LoadStats(void);
static void SaveStats(void);
static Boolean PromptForPlayerName(Str255 outName);
static void RecordGameResult(Boolean won);
static void OnStatistics(void);

static Boolean UseColor(void);
static void GetTileRect(short row, short col, Rect *outRect);
static void GetLetterKeyRect(short row, short idx, Rect *outRect);
static void GetBackspaceKeyRect(Rect *outRect);

static void DrawCenteredLabel(const Rect *r, ConstStr255Param s, short font, short size, short face);
static void DrawCell(const Rect *r, WordleLetterState state, ConstStr255Param label,
                     short font, short size, short face, short ovalInset,
                     Boolean thickFrame, Boolean isKey);
static void DrawTile(short row, short col);
static void DrawKey(const Rect *r, char letter);
static void DrawBackspaceKey(void);
static void DrawRow(short row);
static void DrawBoard(void);
static void DrawKeyboard(void);
static void RedrawAll(void);

static void CStrToPStr(Str255 dst, const char *src);
static void PStrCopy(Str255 dst, ConstStr255Param src);
static void PStrAppend(Str255 dst, ConstStr255Param src);
static void BuildWinMessage(Str255 out);
static void BuildLoseMessage(Str255 out);
static void ShowMessage(ConstStr255Param msg);

static void OnLetterKey(char c);
static void OnBackspace(void);
static void OnSubmit(void);
static void OnNewGame(void);
static void OnGiveUp(void);
static void OnAbout(void);

static void HandleContentClick(Point local);
static void HandleMenuCommand(long menuResult);
static void UpdateFileMenuState(void);
static void HandleMouseDown(EventRecord *event);
static void HandleKeyDown(EventRecord *event);
static void HandleUpdate(EventRecord *event);
static void UpdateMainWindowNow(void);
static void GetDragLimit(Rect *outLimit);
static void HandleEvent(EventRecord *event);
static void InstallAppleEventHandlers(void);
static void RunEventLoop(void);

/* ---------------------------------------------------------------------- */
/* Layout helpers                                                          */
/* ---------------------------------------------------------------------- */

static void GetTileRect(short row, short col, Rect *outRect)
{
    short top = BOARD_TOP + row * (TILE_SIZE + TILE_GAP);
    short left = BOARD_LEFT + col * (TILE_SIZE + TILE_GAP);
    SetRect(outRect, left, top, left + TILE_SIZE, top + TILE_SIZE);
}

static short KeyboardRowLeft(short row)
{
    switch (row) {
        case 0: return KB_ROW0_LEFT;
        case 1: return KB_ROW1_LEFT;
        default: return KB_ROW2_LEFT;
    }
}

static void GetLetterKeyRect(short row, short idx, Rect *outRect)
{
    short top = KEYBOARD_TOP + row * (KEY_H + KEY_ROW_GAP);
    short left = KeyboardRowLeft(row) + idx * (KEY_W + KEY_GAP);
    SetRect(outRect, left, top, left + KEY_W, top + KEY_H);
}

static void GetBackspaceKeyRect(Rect *outRect)
{
    short top = KEYBOARD_TOP + 2 * (KEY_H + KEY_ROW_GAP);
    short left = KB_ROW2_LEFT + 7 * (KEY_W + KEY_GAP);
    SetRect(outRect, left, top, left + KB_BACKSPACE_WIDTH, top + KEY_H);
}

/* ---------------------------------------------------------------------- */
/* Drawing                                                                 */
/* ---------------------------------------------------------------------- */

/* The SE/30's ROM has Color QuickDraw, but its built-in screen is 1-bit, so
 * "Color QD present" alone says nothing about whether color is usable --
 * ask the main device for its real depth instead. Re-evaluated at every
 * full redraw (the Monitors control panel can change depth at any time). */
static Boolean UseColor(void)
{
    GDHandle dev;

    if (!gHasColorQD) return false;
    dev = GetMainDevice();
    if (dev == NULL) return false;
    return (**(**dev).gdPMap).pixelSize >= 4;
}

/* Draws s centered in r with the given font/size/face; vertical placement
 * comes from the font's real ascent/descent rather than magic offsets.
 * Leaves the port's text settings at whatever it last set -- callers
 * restore them via DrawCell(). Uses the current pen/text color. */
static void DrawCenteredLabel(const Rect *r, ConstStr255Param s, short font, short size, short face)
{
    FontInfo fi;
    short w;

    if (s[0] == 0) return;

    TextFont(font);
    TextSize(size);
    TextFace(face);
    GetFontInfo(&fi);

    w = StringWidth(s);
    MoveTo(r->left + ((r->right - r->left) - w) / 2,
           r->top + ((r->bottom - r->top) - (fi.ascent + fi.descent)) / 2 + fi.ascent);
    DrawString(s);
}

/* Draws one tile or key face. Erases r first, so it can be called again
 * for an already-drawn cell to redraw it in place.
 *
 * Monochrome (1-bit) look, per state:
 *   Empty   white, 1px black frame (2px when thickFrame: a typed letter)
 *   Correct solid black, white letter (srcBic)
 *   Present 50% gray, white oval badge (ovalInset), black letter on it
 *   Absent  white, black letter, whole face dimmed with a gray patBic pass
 * Color look: the CLAUDE.md palette, white letters on colored states. */
static void DrawCell(const Rect *r, WordleLetterState state, ConstStr255Param label,
                     short font, short size, short face, short ovalInset,
                     Boolean thickFrame, Boolean isKey)
{
    Rect inner;

    EraseRect(r);

    if (gUseColor) {
        const RGBColor *fill;
        const RGBColor *textColor;

        switch (state) {
            case kLetterCorrect: fill = &kColorCorrect; textColor = &kColorWhite;  break;
            case kLetterPresent: fill = &kColorPresent; textColor = &kColorWhite;  break;
            case kLetterAbsent:  fill = &kColorAbsent;  textColor = &kColorWhite;  break;
            default:
                fill = isKey ? &kColorKeyFace : &kColorWhite;
                textColor = &kColorBorder;
                break;
        }

        RGBForeColor(fill);
        PaintRect(r);
        RGBForeColor(&kColorBorder);
        FrameRect(r);
        RGBForeColor(textColor);
        DrawCenteredLabel(r, label, font, size, face);
        ForeColor(blackColor);
        BackColor(whiteColor);
    } else {
        switch (state) {
            case kLetterCorrect:
                PaintRect(r);
                TextMode(srcBic);
                DrawCenteredLabel(r, label, font, size, face);
                TextMode(srcOr);
                break;

            case kLetterPresent:
                FillRect(r, &qd.gray);
                inner = *r;
                InsetRect(&inner, ovalInset, ovalInset);
                EraseOval(&inner);
                FrameRect(r);
                DrawCenteredLabel(r, label, font, size, face);
                break;

            case kLetterAbsent:
                DrawCenteredLabel(r, label, font, size, face);
                inner = *r;
                InsetRect(&inner, 1, 1);
                PenPat(&qd.gray);
                PenMode(patBic);
                PaintRect(&inner);
                PenNormal();
                FrameRect(r);
                break;

            default:
                if (thickFrame) PenSize(2, 2);
                FrameRect(r);
                PenNormal();
                DrawCenteredLabel(r, label, font, size, face);
                break;
        }
    }

    /* Classic System 7 button feel: 1px drop shadow on the right and bottom
     * edges, drawn in the gap between keys. */
    if (isKey) {
        MoveTo(r->left + 1, r->bottom);
        LineTo(r->right, r->bottom);
        LineTo(r->right, r->top + 1);
    }

    /* Leave the port exactly as found. */
    PenNormal();
    TextMode(srcOr);
    TextFont(kFontChicago);
    TextSize(0);
    TextFace(normal);
    ForeColor(blackColor);
    BackColor(whiteColor);
}

static void DrawTile(short row, short col)
{
    Rect r;
    WordleLetterState state = gGame.rows[row].states[col];
    char letter = gGame.rows[row].letters[col];
    Str255 label;
    Boolean thick;

    GetTileRect(row, col, &r);

    label[0] = 0;
    if (letter) {
        label[0] = 1;
        label[1] = (unsigned char)letter;
    }

    /* Mono only: tiles in the row being typed that hold a letter get a
     * heavier frame so "typed but not submitted" is visible without color. */
    thick = (!gUseColor && state == kLetterEmpty && letter != 0 &&
             row == gGame.currentRow);

    DrawCell(&r, state, label, kFontGeneva, 18, bold, 3, thick, false);
}

static void DrawKey(const Rect *r, char letter)
{
    WordleLetterState state = gGame.keyStates[letter - 'A'];
    Str255 label;

    label[0] = 1;
    label[1] = (unsigned char)letter;

    DrawCell(r, state, label, kFontChicago, 12, normal, 2, false, true);
}

static void DrawBackspaceKey(void)
{
    Rect r;
    Str255 label;

    GetBackspaceKeyRect(&r);

    /* "Delete" in Chicago 12 if it fits inside the key, else "Del". */
    TextFont(kFontChicago);
    TextSize(12);
    TextFace(normal);
    if (StringWidth("\pDelete") <= (r.right - r.left) - 6) {
        PStrCopy(label, "\pDelete");
    } else {
        PStrCopy(label, "\pDel");
    }

    DrawCell(&r, kLetterEmpty, label, kFontChicago, 12, normal, 2, false, true);
}

/* Redraws just one row of the board (used per keystroke: the typed row's
 * frame weight depends on the whole row, so all 5 tiles are redrawn). */
static void DrawRow(short row)
{
    GrafPtr savePort;
    short col;

    if (row < 0 || row >= WORDLE_MAX_GUESSES) return;

    GetPort(&savePort);
    SetPort(gWindow);
    for (col = 0; col < WORDLE_WORD_LENGTH; col++) {
        DrawTile(row, col);
    }
    SetPort(savePort);
}

static void DrawBoard(void)
{
    short row, col;
    for (row = 0; row < WORDLE_MAX_GUESSES; row++) {
        for (col = 0; col < WORDLE_WORD_LENGTH; col++) {
            DrawTile(row, col);
        }
    }
}

static void DrawKeyboard(void)
{
    short row, idx;
    Rect r;

    for (row = 0; row < KB_ROW_COUNT; row++) {
        short count = (short)strlen(kKBRows[row]);
        for (idx = 0; idx < count; idx++) {
            GetLetterKeyRect(row, idx, &r);
            DrawKey(&r, kKBRows[row][idx]);
        }
    }

    DrawBackspaceKey();
}

/* Full redraw: submit, new game, give up. Cells erase themselves, and the
 * gaps between them never change, so the window isn't erased wholesale here
 * (HandleUpdate does that, because an update region can hold stale pixels). */
static void RedrawAll(void)
{
    GrafPtr savePort;

    gUseColor = UseColor();

    GetPort(&savePort);
    SetPort(gWindow);
    DrawBoard();
    DrawKeyboard();
    SetPort(savePort);
}

/* ---------------------------------------------------------------------- */
/* Pascal string helpers                                                   */
/* ---------------------------------------------------------------------- */

static void CStrToPStr(Str255 dst, const char *src)
{
    unsigned char n = (unsigned char)strlen(src);
    dst[0] = n;
    memcpy(dst + 1, src, n);
}

static void PStrToCStr(char *dst, ConstStr255Param src, size_t dstSize)
{
    unsigned char n = src[0];
    if (n > dstSize - 1) n = (unsigned char)(dstSize - 1);
    memcpy(dst, src + 1, n);
    dst[n] = '\0';
}

static void PStrCopy(Str255 dst, ConstStr255Param src)
{
    memcpy(dst, src, (size_t)src[0] + 1);
}

static void PStrAppend(Str255 dst, ConstStr255Param src)
{
    unsigned char room = (unsigned char)(255 - dst[0]);
    unsigned char n = (src[0] < room) ? src[0] : room;
    memcpy(dst + 1 + dst[0], src + 1, n);
    dst[0] = (unsigned char)(dst[0] + n);
}

static void BuildWinMessage(Str255 out)
{
    Str255 num;
    NumToString((long)(gGame.currentRow + 1), num);
    PStrCopy(out, "\pYou got it in ");
    PStrAppend(out, num);
    PStrAppend(out, "\p guess(es)!");
}

static void BuildLoseMessage(Str255 out)
{
    Str255 word;
    CStrToPStr(word, gGame.target);
    PStrCopy(out, "\pOut of guesses! The word was ");
    PStrAppend(out, word);
    PStrAppend(out, "\p.");
}

/* ---------------------------------------------------------------------- */
/* Statistics persistence                                                  */
/*                                                                        */
/* Stored as a flat dump of WordleStatsBook in the Preferences folder      */
/* ("iWordle Stats", type STAT, creator WRDL). System 7.1 has no           */
/* Application Support folder, so unlike the Mac OS 9 build there is no    */
/* per-app subfolder -- the file sits directly in System Folder:Preferences.*/
/* ---------------------------------------------------------------------- */

static Boolean GetStatsFileSpec(FSSpec *spec)
{
    short vRefNum;
    long dirID;
    Str255 name;
    OSErr err;

    if (FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder,
                   &vRefNum, &dirID) != noErr) {
        return false;
    }

    CStrToPStr(name, "iWordle Stats");
    /* fnfErr just means the file doesn't exist yet -- spec is still
     * correctly filled in and usable for FSpCreate in that case. */
    err = FSMakeFSSpec(vRefNum, dirID, name, spec);
    return err == noErr || err == fnfErr;
}

static void LoadStats(void)
{
    FSSpec spec;
    short refNum;
    long count;
    OSErr err;

    WordleStatsInit(&gStats);

    if (!GetStatsFileSpec(&spec)) return;
    if (FSpOpenDF(&spec, fsRdPerm, &refNum) != noErr) return;

    count = sizeof(gStats);
    err = FSRead(refNum, &count, &gStats);
    FSClose(refNum);

    /* A short or garbled file must not leave half a book in memory. */
    if (err != noErr || count != (long)sizeof(gStats) ||
        gStats.playerCount > WORDLE_STATS_MAX_PLAYERS) {
        WordleStatsInit(&gStats);
    } else {
        unsigned short i;
        for (i = 0; i < gStats.playerCount; i++) {
            gStats.players[i].name[WORDLE_STATS_NAME_LEN] = '\0';
        }
    }
}

static void SaveStats(void)
{
    FSSpec spec;
    short refNum;
    long count;

    if (!GetStatsFileSpec(&spec)) return;

    FSpCreate(&spec, 'WRDL', 'STAT', smSystemScript);
    if (FSpOpenDF(&spec, fsWrPerm, &refNum) != noErr) return;

    count = sizeof(gStats);
    FSWrite(refNum, &count, &gStats);
    SetEOF(refNum, count);
    FSClose(refNum);
    /* FSClose doesn't flush the catalog; without this a freshly created
     * stats file can vanish if the machine (or emulator) dies afterwards. */
    FlushVol(NULL, spec.vRefNum);
}

/* ---------------------------------------------------------------------- */
/* Dialog helpers                                                          */
/*                                                                        */
/* On System 7 the Dialog Manager is the native, correct tool, so the     */
/* message box is a plain Alert and the name prompt / ranking are DLOG +   */
/* DITL driven by ModalDialog. Return/Enter always hit item 1 (the OK      */
/* button) by Dialog Manager convention; the heavy rounded ring marking    */
/* it as the default button is a UserItem (the last item in each DITL).    */
/* ---------------------------------------------------------------------- */

/* Rings dialog item 1 (OK) in every dialog that uses it. */
static pascal void DefaultRingProc(DialogRef dlg, DialogItemIndex itemNo)
{
    DialogItemType type;
    Handle itemH;
    Rect box;

    (void)itemNo;

    GetDialogItem(dlg, 1, &type, &itemH, &box);
    PenNormal();
    InsetRect(&box, -4, -4);
    PenSize(3, 3);
    FrameRoundRect(&box, 16, 16);
    PenNormal();
}

/* ---------------------------------------------------------------------- */
/* Message alert (Not in word list / Win / Lose)                           */
/* ---------------------------------------------------------------------- */

/* ALRT 200: item 1 OK button, item 2 StaticText "^0". The Alert Manager
 * draws the default-button outline and handles Return/Enter itself. */
static void ShowMessage(ConstStr255Param msg)
{
    ParamText(msg, "\p", "\p", "\p");
    NoteAlert(kAlertMessage, NULL);
    UpdateMainWindowNow();
}

/* ---------------------------------------------------------------------- */
/* About window: icon, app name/version, author, credits                  */
/* ---------------------------------------------------------------------- */

static void DrawAboutWindowContent(WindowPtr w)
{
    Rect box, iconRect, lineRect;
    Str255 s;
    short midX;
    GrafPtr savePort;

    GetPort(&savePort);
    SetPort(w);
    box = w->portRect;
    EraseRect(&box);

    midX = box.left + (box.right - box.left) / 2;

    PenNormal();

    SetRect(&iconRect, midX - 16, box.top + 14, midX + 16, box.top + 46);
    PlotIconID(&iconRect, atNone, ttNone, 128);

    /* Chicago 12 for the three emphasis lines, Geneva 10 for everything
     * else. Each line is a full-width rect with a baseline at line.top + 12
     * so DrawCenteredLabel's font-metric centering is reused. */
    lineRect.left = box.left;
    lineRect.right = box.right;

    CStrToPStr(s, "iWordle 1.0");
    lineRect.top = box.top + 54;  lineRect.bottom = lineRect.top + 14;
    DrawCenteredLabel(&lineRect, s, kFontChicago, 12, normal);

    CStrToPStr(s, "A native Wordle clone for System 7");
    lineRect.top = box.top + 74;  lineRect.bottom = lineRect.top + 12;
    DrawCenteredLabel(&lineRect, s, kFontGeneva, 10, normal);

    CStrToPStr(s, "Bruno Castello");
    lineRect.top = box.top + 100; lineRect.bottom = lineRect.top + 14;
    DrawCenteredLabel(&lineRect, s, kFontChicago, 12, normal);

    CStrToPStr(s, "bfcastello@hotmail.com");
    lineRect.top = box.top + 120; lineRect.bottom = lineRect.top + 12;
    DrawCenteredLabel(&lineRect, s, kFontGeneva, 10, normal);

    CStrToPStr(s, "Engineer: Claude Sonnet 5");
    lineRect.top = box.top + 148; lineRect.bottom = lineRect.top + 14;
    DrawCenteredLabel(&lineRect, s, kFontChicago, 12, normal);

    CStrToPStr(s, "\xA9 Castello Designs, 2026");
    lineRect.top = box.top + 176; lineRect.bottom = lineRect.top + 12;
    DrawCenteredLabel(&lineRect, s, kFontGeneva, 10, normal);

    CStrToPStr(s, "Built with Retro68");
    lineRect.top = box.top + 192; lineRect.bottom = lineRect.top + 12;
    DrawCenteredLabel(&lineRect, s, kFontGeneva, 10, normal);

    TextFont(kFontChicago);
    TextSize(0);
    TextFace(normal);
    SetPort(savePort);
}

/* ---------------------------------------------------------------------- */
/* Game actions                                                            */
/* ---------------------------------------------------------------------- */

/* Letter / backspace only touch the row being typed, so only that row is
 * redrawn; a 16 MHz 68030 with no double-buffering would visibly flicker
 * if every keystroke repainted the whole window. */
static void OnLetterKey(char c)
{
    if (gGame.status != kGameInProgress) return;
    WordleTypeLetter(&gGame, c);
    DrawRow(gGame.currentRow);
}

static void OnBackspace(void)
{
    if (gGame.status != kGameInProgress) return;
    WordleBackspace(&gGame);
    DrawRow(gGame.currentRow);
}

static void OnSubmit(void)
{
    WordleSubmitResult result;

    if (gGame.status != kGameInProgress) return;

    result = WordleSubmitGuess(&gGame);
    switch (result) {
        case kSubmitTooShort:
            SysBeep(10);
            break;

        case kSubmitNotInDictionary:
            SysBeep(10);
            ShowMessage("\pNot in word list.");
            break;

        case kSubmitOk:
            RedrawAll();
            if (gGame.status == kGameWon) {
                Str255 msg;
                BuildWinMessage(msg);
                ShowMessage(msg);
                RecordGameResult(true);
            } else if (gGame.status == kGameLost) {
                Str255 msg;
                BuildLoseMessage(msg);
                ShowMessage(msg);
                RecordGameResult(false);
            }
            UpdateFileMenuState();
            break;

        default:
            break;
    }
}

static void OnNewGame(void)
{
    WordleNewGame(&gGame);
    RedrawAll();
    UpdateFileMenuState();
}

static void OnGiveUp(void)
{
    Str255 msg;

    if (gGame.status != kGameInProgress) return;

    gGame.status = kGameLost;
    BuildLoseMessage(msg);
    RedrawAll();
    ShowMessage(msg);
    RecordGameResult(false);
    UpdateFileMenuState();
}

/* A plain window with its own tiny event loop, not a ModalDialog -- the
 * standard About Box convention: a real title bar with a close box, no OK
 * button, dismissed by the close box (or Return/Enter/Escape/Cmd-W/Cmd-.).
 * Same loop shape as the Mac OS 9 build, minus Carbon. Clicks anywhere
 * outside it just beep, like a modal window. */
static void OnAbout(void)
{
    WindowPtr w;
    Boolean done = false;
    EventRecord event;
    Rect sb = qd.screenBits.bounds;
    short winW, winH, left, top;

    w = gHasColorQD ? GetNewCWindow(kWindAbout, NULL, (WindowPtr)-1)
                    : GetNewWindow(kWindAbout, NULL, (WindowPtr)-1);
    if (w == NULL) return;

    /* Center on the main screen, below the menu bar and the title bar. */
    winW = w->portRect.right - w->portRect.left;
    winH = w->portRect.bottom - w->portRect.top;
    left = sb.left + ((sb.right - sb.left) - winW) / 2;
    top = sb.top + 20 + 19 + ((sb.bottom - sb.top) - 20 - 19 - winH) / 2;
    MoveWindow(w, left, top, false);

    ShowWindow(w);
    SelectWindow(w);

    do {
        WaitNextEvent(everyEvent, &event, 15, NULL);

        switch (event.what) {
            case updateEvt:
                if ((WindowPtr)event.message == w) {
                    BeginUpdate(w);
                    DrawAboutWindowContent(w);
                    EndUpdate(w);
                } else if ((WindowPtr)event.message == gWindow) {
                    HandleUpdate(&event);
                }
                break;

            case keyDown:
            case autoKey: {
                char c = (char)(event.message & charCodeMask);
                if (c == '\r' || c == 3 || c == 27 ||
                    ((event.modifiers & cmdKey) && (c == 'w' || c == 'W' || c == '.'))) {
                    done = true;
                }
                break;
            }

            case mouseDown: {
                WindowPtr whichWindow;
                short part = FindWindow(event.where, &whichWindow);

                if (whichWindow != w) {
                    SysBeep(10);
                    break;
                }

                if (part == inGoAway) {
                    if (TrackGoAway(w, event.where)) {
                        done = true;
                    }
                } else if (part == inDrag) {
                    Rect limit;
                    GetDragLimit(&limit);
                    DragWindow(w, event.where, &limit);
                }
                break;
            }

            default:
                break;
        }
    } while (!done);

    DisposeWindow(w);
    UpdateMainWindowNow();
}

/* ---------------------------------------------------------------------- */
/* Player name entry (shown after every win/loss) and the ranking          */
/* ---------------------------------------------------------------------- */

/* DLOG 202: item 1 OK, 2 StaticText "Who's playing?", 3 EditText, 4 the
 * default-button ring UserItem. Blocks until OK is hit; outName is empty if
 * the field was left blank (RecordGameResult treats that as "don't record
 * this result"). */
static Boolean PromptForPlayerName(Str255 outName)
{
    DialogPtr dlg;
    short item;
    DialogItemType type;
    Handle itemH;
    Rect box;

    outName[0] = 0;

    dlg = GetNewDialog(kDialogName, NULL, (WindowPtr)-1);
    if (dlg == NULL) return false;

    GetDialogItem(dlg, 4, &type, &itemH, &box);
    SetDialogItem(dlg, 4, type, (Handle)gRingUPP, &box);

    GetDialogItem(dlg, 3, &type, &itemH, &box);
    if (gLastPlayerName[0] > 0) {
        SetDialogItemText(itemH, gLastPlayerName);
    }
    SelectDialogItemText(dlg, 3, 0, 32767);

    ShowWindow(dlg);

    do {
        ModalDialog(NULL, &item);
    } while (item != 1);

    GetDialogItem(dlg, 3, &type, &itemH, &box);
    GetDialogItemText(itemH, outName);
    if (outName[0] > WORDLE_STATS_NAME_LEN) outName[0] = WORDLE_STATS_NAME_LEN;

    DisposeDialog(dlg);
    InitCursor();   /* the EditText may have left an I-beam */
    UpdateMainWindowNow();

    return outName[0] > 0;
}

/* Prompts for a name and records won/lost against it. Skips recording
 * entirely if the name field was left blank -- opening Ranking
 * afterward is conditioned on actually reaching SaveStats() for the
 * same reason: nothing was saved, so there's nothing new to show. */
static void RecordGameResult(Boolean won)
{
    Str255 name;
    char cname[WORDLE_STATS_NAME_LEN + 1];

    if (!PromptForPlayerName(name)) return;

    PStrToCStr(cname, name, sizeof(cname));
    WordleStatsRecordResult(&gStats, cname, won);
    PStrCopy(gLastPlayerName, name);
    SaveStats();

    OnStatistics();
}

/* Draws the ranking table into DITL item 3's box: Geneva 10, bold header
 * (Name / Played / Win % / Cur / Max), 14px rows, up to 10 players. */
static pascal void StatsTableProc(DialogRef dlg, DialogItemIndex itemNo)
{
    DialogItemType type;
    Handle itemH;
    Rect box;
    unsigned short i;
    short rowY;
    Str255 s;

    (void)itemNo;

    GetDialogItem(dlg, 3, &type, &itemH, &box);
    PenNormal();

    TextFont(kFontGeneva);
    TextFace(bold);
    TextSize(10);
    MoveTo(box.left + 12, box.top + 16);
    DrawString("\pName");
    MoveTo(box.left + 190, box.top + 16);
    DrawString("\pPlayed");
    MoveTo(box.left + 250, box.top + 16);
    DrawString("\pWin %");
    MoveTo(box.left + 310, box.top + 16);
    DrawString("\pCur");
    MoveTo(box.left + 355, box.top + 16);
    DrawString("\pMax");

    MoveTo(box.left + 8, box.top + 22);
    LineTo(box.right - 8, box.top + 22);

    TextFace(normal);

    if (gStats.playerCount == 0) {
        MoveTo(box.left + 12, box.top + 42);
        DrawString("\pNo players yet -- win or lose a game to get started.");
    } else {
        rowY = box.top + 38;
        for (i = 0; i < gStats.playerCount && i < WORDLE_STATS_MAX_PLAYERS; i++) {
            WordlePlayerStats *p = &gStats.players[i];
            short winPct = p->gamesPlayed
                ? (short)(((long)p->gamesWon * 100L) / p->gamesPlayed)
                : 0;

            CStrToPStr(s, p->name);
            MoveTo(box.left + 12, rowY);
            DrawString(s);

            NumToString((long)p->gamesPlayed, s);
            MoveTo(box.left + 190, rowY);
            DrawString(s);

            NumToString((long)winPct, s);
            MoveTo(box.left + 250, rowY);
            DrawString(s);

            NumToString((long)p->currentStreak, s);
            MoveTo(box.left + 310, rowY);
            DrawString(s);

            NumToString((long)p->maxStreak, s);
            MoveTo(box.left + 355, rowY);
            DrawString(s);

            rowY += 14;
        }
    }

    TextFont(kFontChicago);
    TextSize(0);
    TextFace(normal);
}

/* DLOG 203: item 1 OK, 2 Clear, 3 table UserItem, 4 default-button ring. */
static void OnStatistics(void)
{
    DialogPtr dlg;
    short item;
    DialogItemType type;
    Handle itemH;
    Rect box;

    dlg = GetNewDialog(kDialogRanking, NULL, (WindowPtr)-1);
    if (dlg == NULL) return;

    GetDialogItem(dlg, 3, &type, &itemH, &box);
    SetDialogItem(dlg, 3, type, (Handle)gTableUPP, &box);

    GetDialogItem(dlg, 4, &type, &itemH, &box);
    SetDialogItem(dlg, 4, type, (Handle)gRingUPP, &box);

    ShowWindow(dlg);

    do {
        ModalDialog(NULL, &item);
        if (item == 2) {
            GrafPtr savePort;

            WordleStatsClear(&gStats);
            SaveStats();

            /* Blank the table's box and invalidate it, so the next update
             * event makes the Dialog Manager call StatsTableProc again. */
            GetDialogItem(dlg, 3, &type, &itemH, &box);
            GetPort(&savePort);
            SetPort(dlg);
            EraseRect(&box);
            InvalRect(&box);
            SetPort(savePort);
        }
    } while (item != 1);

    DisposeDialog(dlg);
    UpdateMainWindowNow();
}

/* ---------------------------------------------------------------------- */
/* Event handling                                                          */
/* ---------------------------------------------------------------------- */

static void HandleContentClick(Point local)
{
    short row, idx;
    Rect r;

    for (row = 0; row < KB_ROW_COUNT; row++) {
        short count = (short)strlen(kKBRows[row]);
        for (idx = 0; idx < count; idx++) {
            GetLetterKeyRect(row, idx, &r);
            if (PtInRect(local, &r)) {
                OnLetterKey(kKBRows[row][idx]);
                return;
            }
        }
    }

    GetBackspaceKeyRect(&r);
    if (PtInRect(local, &r)) {
        OnBackspace();
    }
}

/* "End Game" (File item 2) only makes sense while a game is actually in
 * progress. Called after every game state change rather than lazily before
 * each menu interaction, so MenuKey() (Cmd-W) sees the correct enabled
 * state too, not just clicks through the menu bar. */
static void UpdateFileMenuState(void)
{
    MenuHandle fileMenu = GetMenuHandle(kMenuFile);
    if (fileMenu == NULL) return;

    if (gGame.status == kGameInProgress) {
        EnableItem(fileMenu, 2);
    } else {
        DisableItem(fileMenu, 2);
    }
}

static void HandleMenuCommand(long menuResult)
{
    short menuID = (short)(menuResult >> 16);
    short menuItem = (short)(menuResult & 0xFFFF);

    if (menuID == 0) return;

    switch (menuID) {
        case kMenuApple:
            if (menuItem == 1) {
                OnAbout();
            } else if (menuItem >= 3) {
                /* Items 1-2 are "About" and the separator; everything
                 * after is a desk accessory added by AppendResMenu. */
                Str255 daName;
                GetMenuItemText(GetMenuHandle(kMenuApple), menuItem, daName);
                OpenDeskAcc(daName);
            }
            break;

        case kMenuFile: {
            /* Cmd-W with a desk accessory in front closes the DA, as
             * every System 7 app does, rather than ending the game. */
            WindowPeek front = (WindowPeek)FrontWindow();
            if (menuItem == 2 && front != NULL && front->windowKind < 0) {
                CloseDeskAcc(front->windowKind);
                break;
            }
            if (menuItem == 1) OnNewGame();
            else if (menuItem == 2) OnGiveUp();
            else if (menuItem == 3) OnStatistics();
            else if (menuItem == 5) gDone = true;
            break;
        }

        case kMenuEdit:
            /* Our window has nothing to edit; the Edit menu exists for
             * desk accessories. SystemEdit hands the command to the
             * frontmost DA and returns false if it isn't one. */
            SystemEdit(menuItem - 1);
            break;

        default:
            break;
    }

    HiliteMenu(0);
}

/* Keep title bars reachable: inset the screen and stay below the menu bar. */
static void GetDragLimit(Rect *outLimit)
{
    *outLimit = qd.screenBits.bounds;
    InsetRect(outLimit, 4, 4);
    outLimit->top += 20;
}

static void HandleMouseDown(EventRecord *event)
{
    WindowPtr whichWindow;
    short part = FindWindow(event->where, &whichWindow);

    switch (part) {
        case inMenuBar:
            HandleMenuCommand(MenuSelect(event->where));
            break;

        case inSysWindow:
            SystemClick(event, whichWindow);
            break;

        case inDrag: {
            Rect limit;
            GetDragLimit(&limit);
            DragWindow(whichWindow, event->where, &limit);
            break;
        }

        case inGoAway:
            if (TrackGoAway(whichWindow, event->where)) gDone = true;
            break;

        case inContent:
            if (whichWindow != FrontWindow()) {
                SelectWindow(whichWindow);
            } else if (whichWindow == gWindow) {
                GrafPtr savePort;
                Point local = event->where;

                GetPort(&savePort);
                SetPort(whichWindow);
                GlobalToLocal(&local);
                HandleContentClick(local);
                SetPort(savePort);
            }
            break;

        default:
            break;
    }
}

static void HandleKeyDown(EventRecord *event)
{
    char c = (char)(event->message & charCodeMask);

    if (event->modifiers & cmdKey) {
        HandleMenuCommand(MenuKey(c));
        return;
    }

    /* Typing belongs to the game only while its window is in front. */
    if (FrontWindow() != gWindow) return;

    if (c == 0x08 || c == 0x7F) {
        OnBackspace();
    } else if (c == '\r' || c == 3) {
        /* Holding Return must not keep re-submitting (and beeping). */
        if (event->what == keyDown) OnSubmit();
    } else {
        char upper = (char)toupper((unsigned char)c);
        if (upper >= 'A' && upper <= 'Z') OnLetterKey(upper);
    }
}

static void HandleUpdate(EventRecord *event)
{
    WindowPtr w = (WindowPtr)event->message;
    GrafPtr savePort;

    if (w != gWindow) {
        /* Not ours to draw; just clear the update so it isn't resent. */
        BeginUpdate(w);
        EndUpdate(w);
        return;
    }

    GetPort(&savePort);
    SetPort(w);
    BeginUpdate(w);
    gUseColor = UseColor();
    EraseRect(&w->portRect);
    DrawBoard();
    DrawKeyboard();
    EndUpdate(w);
    SetPort(savePort);
}

/* Repaints whatever an alert/dialog/About window just uncovered right away,
 * instead of leaving a hole until the next pass through the main event
 * loop (a win shows three modals back to back). */
static void UpdateMainWindowNow(void)
{
    EventRecord e;
    e.message = (long)gWindow;
    HandleUpdate(&e);
}

static void HandleEvent(EventRecord *event)
{
    switch (event->what) {
        case mouseDown:       HandleMouseDown(event);        break;
        case keyDown:
        case autoKey:         HandleKeyDown(event);          break;
        case updateEvt:       HandleUpdate(event);           break;
        case activateEvt:                                    break;
        case osEvt:
            /* doesActivateOnFGSwitch: suspend/resume double as our
             * deactivate/activate, so highlight the window ourselves. */
            if (((event->message >> 24) & 0xFF) == suspendResumeMessage) {
                Boolean resume = (event->message & resumeFlag) != 0;
                if (FrontWindow() == gWindow) HiliteWindow(gWindow, resume);
                if (resume) InitCursor();
            }
            break;
        case kHighLevelEvent:
            if (gHasAppleEvents) AEProcessAppleEvent(event);
            break;
        default: break;
    }
}

/* ---------------------------------------------------------------------- */
/* Apple Events                                                            */
/*                                                                        */
/* External quit requests (the Finder's Shut Down / Restart, AppleScript)  */
/* arrive as a kAEQuitApplication Apple Event, not a menu click -- without */
/* a handler for it, they would be silently dropped and the app never      */
/* quit. The other three core events are required of every well-behaved    */
/* application even though this game has nothing to do for them. Only      */
/* installed when the Apple Event Manager exists (System 7 always has it,  */
/* but the check is cheap).                                                */
/* ---------------------------------------------------------------------- */

static pascal OSErr HandleQuitAE(const AppleEvent *event, AppleEvent *reply, long refcon)
{
    (void)event;
    (void)reply;
    (void)refcon;
    gDone = true;
    return noErr;
}

static pascal OSErr HandleNoOpAE(const AppleEvent *event, AppleEvent *reply, long refcon)
{
    (void)event;
    (void)reply;
    (void)refcon;
    return noErr;
}

static void InstallAppleEventHandlers(void)
{
    static AEEventHandlerUPP noOpUPP;
    static AEEventHandlerUPP quitUPP;

    noOpUPP = NewAEEventHandlerUPP(&HandleNoOpAE);
    quitUPP = NewAEEventHandlerUPP(&HandleQuitAE);

    AEInstallEventHandler(kCoreEventClass, kAEOpenApplication, noOpUPP, 0, false);
    AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments, noOpUPP, 0, false);
    AEInstallEventHandler(kCoreEventClass, kAEPrintDocuments, noOpUPP, 0, false);
    AEInstallEventHandler(kCoreEventClass, kAEQuitApplication, quitUPP, 0, false);
}

static void RunEventLoop(void)
{
    EventRecord event;
    while (!gDone) {
        if (WaitNextEvent(everyEvent, &event, 30, NULL)) {
            HandleEvent(&event);
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Entry point                                                             */
/* ---------------------------------------------------------------------- */

int main(void)
{
    long response;

    /* The Toolbox has to be initialized by hand on a 68k Mac. */
    MaxApplZone();
    MoreMasters();
    MoreMasters();
    MoreMasters();
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();
    FlushEvents(everyEvent, 0);

    if (Gestalt(gestaltSystemVersion, &response) != noErr || response < 0x0700) {
        StopAlert(kAlertNeedSystem7, NULL);
        ExitToShell();
    }

    gHasColorQD = (Gestalt(gestaltQuickdrawVersion, &response) == noErr &&
                   response >= gestalt8BitQD);
    gHasAppleEvents = (Gestalt(gestaltAppleEventsAttr, &response) == noErr &&
                       (response & (1L << gestaltAppleEventsPresent)) != 0);

    gRingUPP = NewUserItemUPP(&DefaultRingProc);
    gTableUPP = NewUserItemUPP(&StatsTableProc);

    if (gHasAppleEvents) InstallAppleEventHandlers();

    {
        Handle mbar = GetNewMBar(128);
        if (mbar != NULL) {
            SetMenuBar(mbar);
            DisposeHandle(mbar);
        }
    }
    AppendResMenu(GetMenuHandle(kMenuApple), 'DRVR');
    DrawMenuBar();

    gWindow = gHasColorQD ? GetNewCWindow(kWindMain, NULL, (WindowPtr)-1)
                          : GetNewWindow(kWindMain, NULL, (WindowPtr)-1);
    if (gWindow == NULL) ExitToShell();
    ShowWindow(gWindow);
    SetPort(gWindow);

    LoadStats();

    WordleSeedRandom((unsigned long)TickCount());
    WordleNewGame(&gGame);
    UpdateFileMenuState();

    RedrawAll();
    RunEventLoop();

    return 0;
}
