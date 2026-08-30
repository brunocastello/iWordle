/*
 * iWordle - Mac OS X (PowerPC, 10.0-10.5) / Carbon front end.
 *
 * All game rules live in the portable core (src/core/wordle_engine.*).
 * This is the same Carbon API surface as src/platform/macos9/main.c
 * (WaitNextEvent, the classic Window/Menu/Control/Dialog Managers,
 * FSSpec-based file I/O) since Carbon on OS X supports all of it
 * natively through Tiger/Leopard for 32-bit PowerPC apps -- the only
 * real difference from the Mac OS 9 build is the single umbrella
 * include below (real Apple headers, not Retro68's) and how the
 * resulting Mach-O binary gets packaged into a .app bundle instead of
 * a PEF/CFM resource fork.
 */

#include <Carbon/Carbon.h>

#include <string.h>
#include <ctype.h>

#include "wordle_engine.h"
#include "wordle_stats.h"

#ifndef TARGET_API_MAC_CARBON
#define NewUserItemUPP NewUserItemProc
#endif

/* Real Apple headers already declare this classic font ID as the
 * lowercase enum constant `geneva`; kFontGeneva just keeps this file's
 * naming identical to the Mac OS 9 build's copy (systemFont=0,
 * applFont=1, newYork=2, geneva=3, ...). */
#define kFontGeneva 3

/* ---------------------------------------------------------------------- */
/* Menu IDs (must match iWordle.r)                                        */
/* ---------------------------------------------------------------------- */

enum {
    kMenuApple = 1,
    kMenuFile = 128
};

/* ---------------------------------------------------------------------- */
/* Board / keyboard layout                                                */
/* ---------------------------------------------------------------------- */

/* Window content is sized to fit this layout with a MARGIN-wide border
 * on every side and nothing more; the widest row (10-key QWERTY row)
 * sets the overall content width, and everything else is centered
 * within it. Recompute WIND's bounds in iWordle.r if any of this moves. */
#define MARGIN 14

#define TILE_SIZE      54
#define TILE_GAP       8
#define BOARD_TOP      MARGIN
#define BOARD_LEFT     100

#define KEY_SIZE          42
#define KEY_GAP           6
#define KEY_ROW_GAP       10
#define KEYBOARD_TOP      398
#define KB_ROW0_LEFT      MARGIN
#define KB_ROW1_LEFT      38
#define KB_ROW2_LEFT      33
#define KB_BACKSPACE_WIDTH 100
#define KB_ROW_COUNT      3

static const char * const kKBRows[KB_ROW_COUNT] = {
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM"
};

/* ---------------------------------------------------------------------- */
/* Platinum theme colors (see CLAUDE.md)                                  */
/* ---------------------------------------------------------------------- */

#define C16(x) ((unsigned short)(((x) << 8) | (x)))

static const RGBColor kColorCorrect  = { C16(0x6A), C16(0xAA), C16(0x64) };
static const RGBColor kColorPresent  = { C16(0xC9), C16(0xB4), C16(0x58) };
static const RGBColor kColorAbsent   = { C16(0x78), C16(0x7C), C16(0x7E) };
static const RGBColor kColorWhite    = { 0xFFFF, 0xFFFF, 0xFFFF };
static const RGBColor kColorBlack    = { 0x0000, 0x0000, 0x0000 };
static const RGBColor kColorBorder   = { C16(0x33), C16(0x33), C16(0x33) };
static const RGBColor kColorWindowBG = { C16(0xDD), C16(0xDD), C16(0xDD) };
static const RGBColor kColorKeyFace  = { C16(0xEE), C16(0xEE), C16(0xEE) };
static const RGBColor kColorBevelLo  = { C16(0x99), C16(0x99), C16(0x99) };

/* ---------------------------------------------------------------------- */
/* Globals                                                                 */
/* ---------------------------------------------------------------------- */

static WindowPtr gWindow;
static WordleGame gGame;
static Boolean gDone = false;
static WordleStatsBook gStats;
static Str255 gLastPlayerName = { 0 };
static ControlHandle gNameFieldControl = NULL;
static ControlHandle gNameOKControl = NULL;

/* ---------------------------------------------------------------------- */
/* Forward declarations                                                   */
/* ---------------------------------------------------------------------- */

pascal void MessageContentDrawProc(DialogRef dlg, DialogItemIndex itemNo);
pascal void AboutContentDrawProc(DialogRef dlg, DialogItemIndex itemNo);
pascal void StatsContentDrawProc(DialogRef dlg, DialogItemIndex itemNo);
static void ShowDialogWithDefaultButton(DialogPtr dlg, short defaultItem);

static void DrawNameWindowContent(WindowPtr w);

static void PStrToCStr(char *dst, ConstStr255Param src, size_t dstSize);
static Boolean GetStatsFileSpec(FSSpec *spec);
static void LoadStats(void);
static void SaveStats(void);
static Boolean PromptForPlayerName(Str255 outName);
static void RecordGameResult(Boolean won);
static void OnStatistics(void);

static void GetTileRect(short row, short col, Rect *outRect);
static void GetLetterKeyRect(short row, short idx, Rect *outRect);
static void GetBackspaceKeyRect(Rect *outRect);

static void DrawBevelRect(const Rect *r, RGBColor fill);
static void DrawCenteredLetter(const Rect *r, char letter, short fontSize, const RGBColor *color);
static void DrawCenteredStringAt(short centerX, short baselineY, ConstStr255Param s);
static void DrawTile(short row, short col);
static void DrawKey(const Rect *r, char letter);
static void DrawBoard(void);
static void DrawKeyboard(void);
static void EraseBackground(WindowPtr w);
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
    short top = KEYBOARD_TOP + row * (KEY_SIZE + KEY_ROW_GAP);
    short left = KeyboardRowLeft(row) + idx * (KEY_SIZE + KEY_GAP);
    SetRect(outRect, left, top, left + KEY_SIZE, top + KEY_SIZE);
}

static void GetBackspaceKeyRect(Rect *outRect)
{
    short top = KEYBOARD_TOP + 2 * (KEY_SIZE + KEY_ROW_GAP);
    short left = KB_ROW2_LEFT + 7 * (KEY_SIZE + KEY_GAP);
    SetRect(outRect, left, top, left + KB_BACKSPACE_WIDTH, top + KEY_SIZE);
}

/* ---------------------------------------------------------------------- */
/* Drawing                                                                 */
/* ---------------------------------------------------------------------- */

/* RGBForeColor()/RGBBackColor() take a non-const RGBColor*; our palette
 * entries are const, so route through a local copy instead of casting
 * away const. */
static void SetForeColor(RGBColor color)
{
    RGBForeColor(&color);
}

static void SetBackColor(RGBColor color)
{
    RGBBackColor(&color);
}

static void DrawBevelRect(const Rect *r, RGBColor fill)
{
    Rect inner = *r;

    SetForeColor(fill);
    PaintRect(r);

    InsetRect(&inner, 1, 1);

    SetForeColor(kColorWhite);
    MoveTo(inner.left, inner.bottom - 1);
    LineTo(inner.left, inner.top);
    LineTo(inner.right - 1, inner.top);

    SetForeColor(kColorBevelLo);
    LineTo(inner.right - 1, inner.bottom - 1);
    LineTo(inner.left, inner.bottom - 1);

    SetForeColor(kColorBorder);
    FrameRect(r);
}

static void DrawCenteredLetter(const Rect *r, char letter, short fontSize, const RGBColor *color)
{
    Str255 s;
    short w;

    if (!letter) return;

    s[0] = 1;
    s[1] = (unsigned char)letter;

    TextFont(kFontGeneva);
    TextSize(fontSize);
    TextFace(bold);
    SetForeColor(*color);

    w = StringWidth(s);
    MoveTo(r->left + ((r->right - r->left) - w) / 2,
           r->top + (r->bottom - r->top) / 2 + fontSize / 3);
    DrawString(s);
}

/* Draws s horizontally centered on centerX with its baseline at
 * baselineY; caller sets font/size/face/color beforehand. Used for the
 * message and About windows' hand-drawn text and OK buttons. */
static void DrawCenteredStringAt(short centerX, short baselineY, ConstStr255Param s)
{
    short w = StringWidth(s);
    MoveTo(centerX - w / 2, baselineY);
    DrawString(s);
}

static void DrawTile(short row, short col)
{
    Rect r;
    WordleLetterState state = gGame.rows[row].states[col];
    char letter = gGame.rows[row].letters[col];
    RGBColor fill;
    RGBColor textColor;

    GetTileRect(row, col, &r);

    switch (state) {
        case kLetterCorrect: fill = kColorCorrect; textColor = kColorWhite;  break;
        case kLetterPresent: fill = kColorPresent; textColor = kColorWhite;  break;
        case kLetterAbsent:  fill = kColorAbsent;  textColor = kColorWhite;  break;
        default:             fill = kColorWhite;   textColor = kColorBorder; break;
    }

    RGBForeColor(&fill);
    PaintRect(&r);
    SetForeColor(kColorBorder);
    FrameRect(&r);

    DrawCenteredLetter(&r, letter, 28, &textColor);
    SetForeColor(kColorBlack);
}

static void DrawKey(const Rect *r, char letter)
{
    WordleLetterState state = gGame.keyStates[letter - 'A'];
    RGBColor fill;
    RGBColor textColor;

    switch (state) {
        case kLetterCorrect: fill = kColorCorrect; textColor = kColorWhite;  break;
        case kLetterPresent: fill = kColorPresent; textColor = kColorWhite;  break;
        case kLetterAbsent:  fill = kColorAbsent;  textColor = kColorWhite;  break;
        default:             fill = kColorKeyFace; textColor = kColorBorder; break;
    }

    DrawBevelRect(r, fill);
    DrawCenteredLetter(r, letter, 16, &textColor);
    SetForeColor(kColorBlack);
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

    GetBackspaceKeyRect(&r);
    DrawBevelRect(&r, kColorKeyFace);
    {
        Str255 s;
        short w;
        s[0] = 4;
        s[1] = 'B';
        s[2] = 'A';
        s[3] = 'C';
        s[4] = 'K';
        TextFont(kFontGeneva);
        TextSize(14);
        TextFace(bold);
        SetForeColor(kColorBorder);
        w = StringWidth(s);
        MoveTo(r.left + ((r.right - r.left) - w) / 2, r.top + (r.bottom - r.top) / 2 + 5);
        DrawString(s);
    }
    SetForeColor(kColorBlack);
}

static void EraseBackground(WindowPtr w)
{
    Rect r;
    GetPortBounds(GetWindowPort(w), &r);
    SetForeColor(kColorWindowBG);
    PaintRect(&r);
}

static void RedrawAll(void)
{
    SetPortWindowPort(gWindow);
    EraseBackground(gWindow);
    DrawBoard();
    DrawKeyboard();
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
    PStrCopy(out, "\016You got it in ");
    PStrAppend(out, num);
    PStrAppend(out, "\013 guess(es)!");
}

static void BuildLoseMessage(Str255 out)
{
    Str255 word;
    CStrToPStr(word, gGame.target);
    PStrCopy(out, "\035Out of guesses! The word was ");
    PStrAppend(out, word);
    PStrAppend(out, "\001.");
}

/* ---------------------------------------------------------------------- */
/* Statistics persistence                                                  */
/*                                                                        */
/* Stored as a flat dump of WordleStatsBook in System Folder:Application  */
/* Support:iWordle: -- the same place (and same per-app subfolder shape)  */
/* a future Mac OS X port would use under ~/Library/Application Support,  */
/* so the on-disk convention doesn't have to change when this is ported.  */
/*                                                                        */
/* This needs the real Folder Manager (FindFolder): the open-source       */
/* Multiversal Interfaces this toolchain used by default doesn't have one */
/* at all, and its classic pre-Folder-Manager fallback (SysEnvirons +     */
/* GetWDInfo) compiles but won't link under Carbon, matching how real     */
/* Apple Carbon actually dropped those calls. Both problems go away now   */
/* that this project links against Apple's real Universal Interfaces      */
/* (see third_party/InterfacesAndLibraries), where FindFolder is a real,  */
/* Carbon-linkable call. */
/* ---------------------------------------------------------------------- */

static OSErr ResolveOrCreateSubfolder(short vRefNum, long parentDirID,
                                       ConstStr255Param name, long *outDirID)
{
    CInfoPBRec pb;
    FSSpec spec;
    OSErr err;

    memset(&pb, 0, sizeof(pb));
    pb.dirInfo.ioNamePtr = (StringPtr)name;
    pb.dirInfo.ioVRefNum = vRefNum;
    pb.dirInfo.ioDrDirID = parentDirID;
    pb.dirInfo.ioFDirIndex = 0;
    if (PBGetCatInfoSync(&pb) == noErr) {
        *outDirID = pb.dirInfo.ioDrDirID;
        return noErr;
    }

    err = FSMakeFSSpec(vRefNum, parentDirID, name, &spec);
    if (err != noErr && err != fnfErr) return err;

    return FSpDirCreate(&spec, smSystemScript, outDirID);
}

static OSErr GetStatsFolder(short *outVRefNum, long *outDirID)
{
    short vRefNum;
    long appSupportDirID;
    Str255 name;
    OSErr err;

    err = FindFolder(kOnSystemDisk, kApplicationSupportFolderType, kCreateFolder,
                      &vRefNum, &appSupportDirID);
    if (err != noErr) return err;

    CStrToPStr(name, "iWordle");
    err = ResolveOrCreateSubfolder(vRefNum, appSupportDirID, name, outDirID);
    if (err != noErr) return err;

    *outVRefNum = vRefNum;
    return noErr;
}

static Boolean GetStatsFileSpec(FSSpec *spec)
{
    short vRefNum;
    long dirID;
    Str255 name;
    OSErr err;

    if (GetStatsFolder(&vRefNum, &dirID) != noErr) return false;

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

    WordleStatsInit(&gStats);

    if (!GetStatsFileSpec(&spec)) return;
    if (FSpOpenDF(&spec, fsRdPerm, &refNum) != noErr) return;

    count = sizeof(gStats);
    FSRead(refNum, &count, &gStats);
    FSClose(refNum);
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
}

/* ---------------------------------------------------------------------- */
/* Shared modal helpers                                                    */
/*                                                                        */
/* Unlike the Mac OS 9 build (Retro68's headers have no Appearance        */
/* Manager at all), real Mac OS X Carbon draws a genuinely native Aqua    */
/* dialog background and default-button glow for free -- so this front   */
/* end doesn't hand-paint a background or hand-draw a default-button      */
/* ring the way OS 9 has to. Item 1's UserItem draw procs below only      */
/* draw their own text/icon/table content, leaving the dialog's real      */
/* background untouched. SetDialogDefaultItem() marks the given item as   */
/* both the pulsing-blue-glow default button AND the Return/Enter target, */
/* which is the standard, HIG-correct replacement for both the hand-drawn */
/* ring and the custom Return-key filter procs OS 9 needs.                */
/* ---------------------------------------------------------------------- */

/* GetNewDialog() on real Carbon doesn't reliably paint its native
 * controls on first show -- without an explicit extra draw here, the OK/
 * Clear buttons stay invisible until the user's first click forces a
 * redraw. Draw1Control-ing the default button directly closes that gap
 * regardless of the exact cause. */
static void ShowDialogWithDefaultButton(DialogPtr dlg, short defaultItem)
{
    ControlRef defaultControl;

    SetDialogDefaultItem(dlg, defaultItem);
    DrawDialog(dlg);
    DrawControls((WindowPtr)dlg);
    if (GetDialogItemAsControl(dlg, defaultItem, &defaultControl) == noErr) {
        Draw1Control(defaultControl);
    }
}

/* ---------------------------------------------------------------------- */
/* Message dialog (New Game / Give Up / Win / Lose)                       */
/* ---------------------------------------------------------------------- */

/* Set by ShowMessage() just before the modal loop; read by
 * MessageContentDrawProc(). */
static Str255 gMessageText;

pascal void MessageContentDrawProc(DialogRef dlg, DialogItemIndex itemNo)
{
    DialogItemType type;
    Handle itemH;
    Rect box;

    (void)itemNo;

    GetDialogItem(dlg, 1, &type, &itemH, &box);
    SetForeColor(kColorBlack);
    PenNormal();

    /* Same font as the menu bar. UseThemeFont(kThemeMenuTitleFont, ...)
     * resolves to whatever font+size this build's menu bar is actually
     * using instead of hardcoding a name -- Lucida Grande 14pt Regular on
     * stock Aqua, but not assumed here in case a given 10.0-10.5 install
     * differs. */
    UseThemeFont(kThemeMenuTitleFont, smSystemScript);
    DrawCenteredStringAt(box.left + (box.right - box.left) / 2,
                          box.top + (box.bottom - box.top) / 2 + 4, gMessageText);
}

static void ShowMessage(ConstStr255Param msg)
{
    DialogPtr dlg;
    short item;
    DialogItemType type;
    Handle itemH;
    Rect box;

    PStrCopy(gMessageText, msg);

    dlg = GetNewDialog(200, NULL, (WindowPtr)-1);
    if (dlg == NULL) return;

    GetDialogItem(dlg, 1, &type, &itemH, &box);
    SetDialogItem(dlg, 1, type, (Handle)NewUserItemUPP(&MessageContentDrawProc), &box);

    ShowDialogWithDefaultButton(dlg, 2);

    do {
        ModalDialog(NULL, &item);
    } while (item != 2);

    DisposeDialog(dlg);
    RedrawAll();
}

/* ---------------------------------------------------------------------- */
/* About dialog: icon, app name/version, author, credits, native button   */
/* ---------------------------------------------------------------------- */

pascal void AboutContentDrawProc(DialogRef dlg, DialogItemIndex itemNo)
{
    DialogItemType type;
    Handle itemH;
    Rect box, iconRect;
    Str255 s;
    short midX;

    (void)itemNo;

    GetDialogItem(dlg, 1, &type, &itemH, &box);
    midX = box.left + (box.right - box.left) / 2;

    SetForeColor(kColorBlack);
    PenNormal();

    SetRect(&iconRect, midX - 16, box.top + 14, midX + 16, box.top + 46);
    PlotIconID(&iconRect, atNone, ttNone, 128);

    /* Same font as the menu bar, for every line -- see the comment in
     * MessageContentDrawProc() above on why this is resolved through the
     * Appearance Manager rather than a hardcoded name/size. */
    UseThemeFont(kThemeMenuTitleFont, smSystemScript);

    CStrToPStr(s, "iWordle 1.0");
    DrawCenteredStringAt(midX, box.top + 64, s);

    CStrToPStr(s, "A native Wordle clone for Mac OS X");
    DrawCenteredStringAt(midX, box.top + 84, s);

    CStrToPStr(s, "Bruno Castello");
    DrawCenteredStringAt(midX, box.top + 112, s);

    CStrToPStr(s, "bfcastello@hotmail.com");
    DrawCenteredStringAt(midX, box.top + 132, s);

    CStrToPStr(s, "Engineer: Claude Sonnet 5");
    DrawCenteredStringAt(midX, box.top + 160, s);

    CStrToPStr(s, "\xA9 Castello Designs, 2026");
    DrawCenteredStringAt(midX, box.top + 188, s);
}

/* ---------------------------------------------------------------------- */
/* Game actions                                                            */
/* ---------------------------------------------------------------------- */

static void OnLetterKey(char c)
{
    if (gGame.status != kGameInProgress) return;
    WordleTypeLetter(&gGame, c);
    RedrawAll();
}

static void OnBackspace(void)
{
    if (gGame.status != kGameInProgress) return;
    WordleBackspace(&gGame);
    RedrawAll();
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
            ShowMessage("\021Not in word list.");
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

static void OnAbout(void)
{
    DialogPtr dlg;
    short item;
    DialogItemType type;
    Handle itemH;
    Rect box;

    dlg = GetNewDialog(201, NULL, (WindowPtr)-1);
    if (dlg == NULL) return;

    GetDialogItem(dlg, 1, &type, &itemH, &box);
    SetDialogItem(dlg, 1, type, (Handle)NewUserItemUPP(&AboutContentDrawProc), &box);

    ShowDialogWithDefaultButton(dlg, 2);

    do {
        ModalDialog(NULL, &item);
    } while (item != 2);

    DisposeDialog(dlg);
    RedrawAll();
}

/* ---------------------------------------------------------------------- */
/* Player name entry (shown after every win/loss) and the statistics       */
/* scoreboard (File > Statistics...)                                       */
/* ---------------------------------------------------------------------- */

/* This prompt is a plain window with its own tiny event loop, not a
 * ModalDialog -- every bug hit getting a real native Edit Text control
 * to accept keystrokes traced back to some ModalDialog behavior we don't
 * control (auto update handling that skips DrawControls, no guaranteed
 * port before handing us a keyDown, etc.). The main game window already
 * runs a plain WaitNextEvent loop with real native controls with none of
 * that friction, so this reuses that same architecture instead of
 * continuing to fight ModalDialog for a case it wasn't built for. */
/* No hand-painted background or hand-drawn default-button ring here --
 * this is a real Carbon dBoxProc window on OS X, which already draws
 * its own native Aqua dialog background; PromptForPlayerName() marks
 * gNameOKControl as the window's default button via
 * SetWindowDefaultButton(), which draws the native pulsing-blue glow
 * instead. */
static void DrawNameWindowContent(WindowPtr w)
{
    Rect windowRect;

    SetPortWindowPort(w);
    GetPortBounds(GetWindowPort(w), &windowRect);

    SetForeColor(kColorBlack);
    PenNormal();
    UseThemeFont(kThemeMenuTitleFont, smSystemScript);
    DrawCenteredStringAt((windowRect.right - windowRect.left) / 2, 34, "\016Who's playing?");
}

/* Blocks until OK is hit; outName is empty if the field was left blank
 * (RecordGameResult treats that as "don't record this result"). */
static Boolean PromptForPlayerName(Str255 outName)
{
    WindowPtr w;
    Rect fieldRect, okRect;
    ControlRef rootControl;
    Boolean done = false;
    EventRecord event;

    w = GetNewCWindow(202, NULL, (WindowPtr)-1);
    if (w == NULL) { outName[0] = 0; return false; }

    if (GetRootControl(w, &rootControl) != noErr) {
        CreateRootControl(w, &rootControl);
    }

    SetRect(&fieldRect, 60, 67, 260, 85);
    gNameFieldControl = NewControl(w, &fieldRect, "\000", true, 0, 0, 0, kControlEditTextProc, 0L);

    /* Standard Aqua push button height (20px). */
    SetRect(&okRect, 125, 120, 195, 140);
    gNameOKControl = NewControl(w, &okRect, "\002OK", true, 0, 0, 0, kControlPushButtonProc, 0L);
    SetWindowDefaultButton(w, gNameOKControl);

    if (gNameFieldControl != NULL && gLastPlayerName[0] > 0) {
        SetControlData(gNameFieldControl, kControlEditTextPart, kControlEditTextTextTag,
                        gLastPlayerName[0], (Ptr)(gLastPlayerName + 1));
    }

    ShowWindow(w);
    SelectWindow(w);

    if (gNameFieldControl != NULL) {
        SetKeyboardFocus(w, gNameFieldControl, kControlFocusNextPart);
    }

    do {
        WaitNextEvent(everyEvent, &event, 15, NULL);

        switch (event.what) {
            case updateEvt:
                if ((WindowPtr)event.message == w) {
                    BeginUpdate(w);
                    DrawNameWindowContent(w);
                    DrawControls(w);
                    if (gNameOKControl != NULL) Draw1Control(gNameOKControl);
                    EndUpdate(w);
                }
                break;

            case keyDown:
            case autoKey: {
                char c = (char)(event.message & charCodeMask);
                if (c == '\r' || c == 3) {
                    done = true;
                } else if (gNameFieldControl != NULL) {
                    short keyCode = (short)((event.message & keyCodeMask) >> 8);
                    SetPortWindowPort(w);
                    HandleControlKey(gNameFieldControl, keyCode, c, event.modifiers);
                }
                break;
            }

            case mouseDown: {
                WindowPtr whichWindow;
                short part = FindWindow(event.where, &whichWindow);

                if (whichWindow != w) {
                    if (part == inMenuBar) {
                        /* Keep this window on top of the game's menu the
                         * same way a modal dialog would -- ignore menu
                         * clicks while it's up. */
                        HiliteMenu(0);
                    } else {
                        SelectWindow(w);
                    }
                    break;
                }

                if (part == inContent) {
                    Point local = event.where;
                    ControlHandle hitControl = NULL;

                    SetPortWindowPort(w);
                    GlobalToLocal(&local);
                    FindControl(local, w, &hitControl);

                    if (hitControl == gNameFieldControl) {
                        SetKeyboardFocus(w, gNameFieldControl, kControlFocusNextPart);
                        TrackControl(gNameFieldControl, local, NULL);
                    } else if (hitControl == gNameOKControl) {
                        if (TrackControl(gNameOKControl, local, NULL) != 0) {
                            done = true;
                        }
                    }
                }
                break;
            }

            default:
                break;
        }
    } while (!done);

    outName[0] = 0;
    if (gNameFieldControl != NULL) {
        long actualSize = 0;
        char buf[256];
        if (GetControlData(gNameFieldControl, kControlEditTextPart, kControlEditTextTextTag,
                            sizeof(buf), buf, &actualSize) == noErr) {
            if (actualSize > 255) actualSize = 255;
            outName[0] = (unsigned char)actualSize;
            memcpy(outName + 1, buf, (size_t)actualSize);
        }
    }
    gNameFieldControl = NULL;
    gNameOKControl = NULL;

    DisposeWindow(w);
    RedrawAll();

    return outName[0] > 0;
}

/* Prompts for a name and records won/lost against it. Skips recording
 * entirely if the name field was left blank. */
static void RecordGameResult(Boolean won)
{
    Str255 name;
    char cname[WORDLE_STATS_NAME_LEN + 1];

    if (!PromptForPlayerName(name)) return;

    PStrToCStr(cname, name, sizeof(cname));
    WordleStatsRecordResult(&gStats, cname, won);
    PStrCopy(gLastPlayerName, name);
    SaveStats();
}

pascal void StatsContentDrawProc(DialogRef dlg, DialogItemIndex itemNo)
{
    DialogItemType type;
    Handle itemH;
    Rect box;
    unsigned short i;
    short rowY;
    Str255 s;

    (void)itemNo;

    GetDialogItem(dlg, 1, &type, &itemH, &box);
    SetForeColor(kColorBlack);
    PenNormal();

    UseThemeFont(kThemeMenuTitleFont, smSystemScript);
    MoveTo(box.left + 16, box.top + 20);
    DrawString("\004Name");
    MoveTo(box.left + 220, box.top + 20);
    DrawString("\006Played");
    MoveTo(box.left + 280, box.top + 20);
    DrawString("\005Win %");
    MoveTo(box.left + 335, box.top + 20);
    DrawString("\003Cur");
    MoveTo(box.left + 380, box.top + 20);
    DrawString("\003Max");

    MoveTo(box.left + 10, box.top + 26);
    LineTo(box.right - 10, box.top + 26);

    if (gStats.playerCount == 0) {
        MoveTo(box.left + 16, box.top + 50);
        DrawString("\064No players yet -- win or lose a game to get started.");
        return;
    }

    rowY = box.top + 44;
    for (i = 0; i < gStats.playerCount; i++) {
        WordlePlayerStats *p = &gStats.players[i];
        short winPct = p->gamesPlayed
            ? (short)(((long)p->gamesWon * 100L) / p->gamesPlayed)
            : 0;

        CStrToPStr(s, p->name);
        MoveTo(box.left + 16, rowY);
        DrawString(s);

        NumToString((long)p->gamesPlayed, s);
        MoveTo(box.left + 220, rowY);
        DrawString(s);

        NumToString((long)winPct, s);
        MoveTo(box.left + 280, rowY);
        DrawString(s);

        NumToString((long)p->currentStreak, s);
        MoveTo(box.left + 335, rowY);
        DrawString(s);

        NumToString((long)p->maxStreak, s);
        MoveTo(box.left + 380, rowY);
        DrawString(s);

        rowY += 18;
    }
}

static void OnStatistics(void)
{
    DialogPtr dlg;
    short item;
    DialogItemType type;
    Handle itemH;
    Rect box;

    dlg = GetNewDialog(203, NULL, (WindowPtr)-1);
    if (dlg == NULL) return;

    GetDialogItem(dlg, 1, &type, &itemH, &box);
    SetDialogItem(dlg, 1, type, (Handle)NewUserItemUPP(&StatsContentDrawProc), &box);

    ShowDialogWithDefaultButton(dlg, 3);

    do {
        ModalDialog(NULL, &item);
        if (item == 2) {
            /* Force the redraw ourselves rather than invalidating and
             * waiting for the next update event. */
            WordleStatsClear(&gStats);
            SaveStats();
            ShowDialogWithDefaultButton(dlg, 3);
        }
    } while (item != 3);

    DisposeDialog(dlg);
    RedrawAll();
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
 * progress. DisableItem/EnableItem -- the classic pair -- aren't
 * available under Carbon at all (confirmed in the real Menus.h: wrapped
 * in #if CALL_NOT_IN_CARBON, explicitly "CarbonLib: not available"),
 * so this uses EnableMenuItem/DisableMenuItem instead, the Carbon-era
 * replacement (available since Mac OS 8.5). Called after every game
 * state change rather than lazily before each menu interaction, so
 * MenuKey() (Cmd-W) sees the correct enabled state too, not just clicks
 * through the menu bar. */
static void UpdateFileMenuState(void)
{
    MenuHandle fileMenu = GetMenuHandle(kMenuFile);
    if (fileMenu == NULL) return;

    if (gGame.status == kGameInProgress) {
        EnableMenuItem(fileMenu, 2);
    } else {
        DisableMenuItem(fileMenu, 2);
    }
}

static void HandleMenuCommand(long menuResult)
{
    short menuID = (short)(menuResult >> 16);
    short menuItem = (short)(menuResult & 0xFFFF);

    if (menuID == 0) return;

    switch (menuID) {
        case kMenuApple:
            if (menuItem == 1) OnAbout();
            break;

        case kMenuFile:
            if (menuItem == 1) OnNewGame();
            else if (menuItem == 2) OnGiveUp();
            else if (menuItem == 3) OnStatistics();
            else if (menuItem == 5) gDone = true;
            break;

        default:
            break;
    }

    HiliteMenu(0);
}

static void HandleMouseDown(EventRecord *event)
{
    WindowPtr whichWindow;
    short part = FindWindow(event->where, &whichWindow);

    switch (part) {
        case inMenuBar:
            HandleMenuCommand(MenuSelect(event->where));
            break;

        case inDrag: {
            BitMap screenBits;
            GetQDGlobalsScreenBits(&screenBits);
            DragWindow(whichWindow, event->where, &screenBits.bounds);
            break;
        }

        case inGoAway:
            if (TrackGoAway(whichWindow, event->where)) gDone = true;
            break;

        case inContent:
            if (whichWindow != FrontWindow()) {
                SelectWindow(whichWindow);
            } else {
                Point local = event->where;
                SetPortWindowPort(whichWindow);
                GlobalToLocal(&local);
                HandleContentClick(local);
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

    if (c == 0x08 || c == 0x7F) {
        OnBackspace();
    } else if (c == '\r' || c == 3) {
        OnSubmit();
    } else {
        char upper = (char)toupper((unsigned char)c);
        if (upper >= 'A' && upper <= 'Z') OnLetterKey(upper);
    }
}

static void HandleUpdate(EventRecord *event)
{
    WindowPtr w = (WindowPtr)event->message;
    BeginUpdate(w);
    SetPortWindowPort(w);
    EraseBackground(w);
    DrawBoard();
    DrawKeyboard();
    EndUpdate(w);
}

static void HandleEvent(EventRecord *event)
{
    switch (event->what) {
        case mouseDown:       HandleMouseDown(event);        break;
        case keyDown:
        case autoKey:         HandleKeyDown(event);          break;
        case updateEvt:       HandleUpdate(event);           break;
        case kHighLevelEvent: AEProcessAppleEvent(event);    break;
        default: break;
    }
}

/* ---------------------------------------------------------------------- */
/* Apple Events                                                            */
/*                                                                        */
/* External quit requests (a Dock-replacement utility's "Quit", the       */
/* Finder, AppleScript, system shutdown) arrive as a kAEQuitApplication   */
/* Apple Event, not a menu click -- without a handler for it, they were   */
/* silently dropped and the app never quit. The other three core events   */
/* are required of every well-behaved application even though this game  */
/* has nothing to do for them.                                            */
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
    AEInstallEventHandler(kCoreEventClass, kAEOpenApplication,
                           NewAEEventHandlerUPP(&HandleNoOpAE), 0, false);
    AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments,
                           NewAEEventHandlerUPP(&HandleNoOpAE), 0, false);
    AEInstallEventHandler(kCoreEventClass, kAEPrintDocuments,
                           NewAEEventHandlerUPP(&HandleNoOpAE), 0, false);
    AEInstallEventHandler(kCoreEventClass, kAEQuitApplication,
                           NewAEEventHandlerUPP(&HandleQuitAE), 0, false);
}

static void RunEventLoop(void)
{
    EventRecord event;
    while (!gDone) {
        if (WaitNextEvent(everyEvent, &event, 15, NULL)) {
            HandleEvent(&event);
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Entry point                                                             */
/* ---------------------------------------------------------------------- */

int main(void)
{
    /* Unlike the classic Mac OS 9 build, a bundled Mach-O Carbon app on
     * OS X doesn't get its Contents/Resources/iWordle.rsrc mapped into
     * the Resource Manager for free -- without this, GetNewMBar/
     * GetNewCWindow/GetNewDialog below all silently return NULL and the
     * app launches with no window and no menu at all. Locate the file
     * explicitly by name via CFBundleCopyResourceURL and open it
     * directly with FSOpenResourceFile, rather than relying on any
     * automatic bundle/executable-name-matching convention. It's
     * opened by the reserved data-fork name (not the default resource
     * fork) since the packaging pipeline writes the compiled resource
     * map into the file's plain data fork -- there's no real HFS+
     * resource fork support on the Linux build image that produces it. */
    {
        CFBundleRef mainBundle = CFBundleGetMainBundle();
        CFURLRef rsrcURL = (mainBundle != NULL)
            ? CFBundleCopyResourceURL(mainBundle, CFSTR("iWordle"), CFSTR("rsrc"), NULL)
            : NULL;

        if (rsrcURL != NULL) {
            FSRef rsrcRef;
            if (CFURLGetFSRef(rsrcURL, &rsrcRef)) {
                HFSUniStr255 dataForkName;
                SInt16 refNum;

                FSGetDataForkName(&dataForkName);
                FSOpenResourceFile(&rsrcRef, dataForkName.length, dataForkName.unicode,
                                    fsRdPerm, &refNum);
            }
            CFRelease(rsrcURL);
        }
    }

#if !TARGET_API_MAC_CARBON
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
#endif
    InitCursor();
    FlushEvents(everyEvent, 0);
    InstallAppleEventHandlers();

    {
        Handle mbar = GetNewMBar(128);
        if (mbar != NULL) {
            SetMenuBar(mbar);
            DisposeHandle(mbar);
        }
    }
    DrawMenuBar();

    gWindow = GetNewCWindow(128, NULL, (WindowPtr)-1);
    ShowWindow(gWindow);

    LoadStats();

    WordleSeedRandom((unsigned long)TickCount());
    WordleNewGame(&gGame);
    UpdateFileMenuState();

    RedrawAll();
    RunEventLoop();

    return 0;
}
