/*
 * iWordle - Mac OS 9 / Carbon front end.
 *
 * All game rules live in the portable core (src/core/wordle_engine.*).
 * This file only knows about QuickDraw, the Window/Menu/Dialog Managers,
 * and the classic WaitNextEvent loop, so a future Mac OS X front end can
 * reuse the core untouched.
 */

#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Fonts.h>
#include <Dialogs.h>
#include <Events.h>
#include <ToolUtils.h>
#include <TextUtils.h>
#include <OSUtils.h>
#include <Icons.h>
#include <AppleEvents.h>
#include <Files.h>
#include <Folders.h>

#include <string.h>
#include <ctype.h>

#include "wordle_engine.h"
#include "wordle_stats.h"

#ifndef TARGET_API_MAC_CARBON
#define NewUserItemUPP NewUserItemProc
#endif

/* Not declared by Retro68's headers; 3 is Geneva's well-known classic
 * system font ID (systemFont=0, applFont=1, newYork=2, geneva=3, ...). */
#define kFontGeneva 3

/* Not declared by Retro68's headers (Controls.h/Appearance.h don't exist
 * as files here), but these are documented Appearance Manager Control
 * Manager constants -- the Edit Text control's CDEF procID, its one
 * meaningful "part" (used with SetKeyboardFocus/Get/SetControlData), and
 * the tag identifying its text content. The Appearance Manager CDEF that
 * renders it (sunken bezel, blue focus ring) lives in the running OS, not
 * something we need a header to link against -- NewControl just needs
 * the raw procID to ask for it. */
#define kEditTextCDEF    190
#define kEditTextPart    24
#define kEditTextTextTag 'text'

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

/* ---------------------------------------------------------------------- */
/* Forward declarations                                                   */
/* ---------------------------------------------------------------------- */

pascal void MessageContentDrawProc(DialogRef dlg, DialogItemIndex itemNo);
pascal void AboutContentDrawProc(DialogRef dlg, DialogItemIndex itemNo);
pascal void NameEntryContentDrawProc(DialogRef dlg, DialogItemIndex itemNo);
pascal void StatsContentDrawProc(DialogRef dlg, DialogItemIndex itemNo);
pascal Boolean DismissOnEnterFilterProc(DialogPtr dlg, EventRecord *event, short *itemHit);
pascal Boolean DismissOnEnterFilterProc3(DialogPtr dlg, EventRecord *event, short *itemHit);
pascal Boolean NameEntryFilterProc(DialogPtr dlg, EventRecord *event, short *itemHit);
pascal void ButtonFrameProc(DialogRef dlg, DialogItemIndex itemNo);
static void PaintFullDialogBackground(DialogRef dlg);
static void PaintDialogBackgroundExcluding(DialogRef dlg, const Rect *holeRect);

static void PStrToCStr(char *dst, ConstStr255Param src, size_t dstSize);
static void GetStatsFileSpec(FSSpec *spec);
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
/* Stored as a flat dump of WordleStatsBook in System Folder:Application  */
/* Support:iWordle: -- the same place (and same per-app subfolder shape)  */
/* a future Mac OS X port would use under ~/Library/Application Support,  */
/* so the on-disk convention doesn't have to change when this is ported.  */
/* FindFolder locates (and creates, if missing) Application Support       */
/* itself; our own "iWordle" subfolder inside it is located or created    */
/* the same way classic apps always have, since Folders.h has no call     */
/* that does both in one step for an app-owned subfolder.                */
/* ---------------------------------------------------------------------- */

static OSErr GetStatsFolder(short *outVRefNum, long *outDirID)
{
    short vRefNum;
    long appSupportDirID;
    long dirID;
    Boolean isDir;
    FSSpec folderSpec;
    Str255 name;
    OSErr err;

    err = FindFolder(kOnSystemDisk, kApplicationSupportFolderType, kCreateFolder,
                      &vRefNum, &appSupportDirID);
    if (err != noErr) return err;

    CStrToPStr(name, "iWordle");
    err = FSMakeFSSpec(vRefNum, appSupportDirID, name, &folderSpec);
    if (err == noErr) {
        err = FSpGetDirectoryID(&folderSpec, &dirID, &isDir);
        if (err != noErr) return err;
        if (!isDir) return dirNFErr;
    } else {
        long newDirID;
        err = DirCreate(vRefNum, appSupportDirID, name, &newDirID);
        if (err != noErr) return err;
        dirID = newDirID;
    }

    *outVRefNum = vRefNum;
    *outDirID = dirID;
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
/* Retro68's headers don't expose the Appearance Manager (no              */
/* Appearance.h at all), so there's no SetThemeWindowBackground to ask     */
/* for a Platinum background -- item 1 in both dialogs below is a         */
/* UserItem that paints the WHOLE dialog window gray (not just its own    */
/* item rect) and draws that dialog's text content by hand. Item 2 is a   */
/* real native Button, "OK". Item 3 draws the classic bold rounded frame  */
/* around it that marks it as the dialog's default button -- the same     */
/* technique that distinguishes an OK button from a plain Cancel button   */
/* in a native alert. SetDialogDefaultItem() was tried as a way to get     */
/* that emphasis for free from the Dialog Manager instead of hand-drawing */
/* it, but rendered far worse (the button barely appeared at all) than    */
/* this hand-drawn frame does, so this is the version we're keeping.      */
/* There's a known remaining cosmetic issue where the button's own        */
/* corner pixels can show white against our gray background; the frame    */
/* itself, which is what's actually being asked for, is unaffected by     */
/* that and renders correctly. Because Return/Enter is hardwired to item  */
/* 1 by Dialog Manager convention regardless of item type, this filter     */
/* proc remaps it to item 2 instead.                                      */
/* ---------------------------------------------------------------------- */

pascal Boolean DismissOnEnterFilterProc(DialogPtr dlg, EventRecord *event, short *itemHit)
{
    (void)dlg;

    if (event->what == keyDown || event->what == autoKey) {
        char c = (char)(event->message & charCodeMask);
        if (c == '\r' || c == 3) {
            *itemHit = 2;
            return true;
        }
    }
    return false;
}

/* Same remap as above, for the statistics dialog, whose default button
 * sits at item 3 instead of item 2 (the Clear button comes first). */
pascal Boolean DismissOnEnterFilterProc3(DialogPtr dlg, EventRecord *event, short *itemHit)
{
    (void)dlg;

    if (event->what == keyDown || event->what == autoKey) {
        char c = (char)(event->message & charCodeMask);
        if (c == '\r' || c == 3) {
            *itemHit = 3;
            return true;
        }
    }
    return false;
}

/* Modal filter for the name-entry dialog, whose "field" is a real
 * Control Manager Edit Text control (gNameFieldControl) rather than a
 * DITL editText item -- ModalDialog only knows how to route keystrokes
 * and clicks to DITL items, so this proc forwards both to the control
 * by hand: HandleControlKey feeds it every non-Return keystroke, and
 * FindControl/TrackControl handle click-to-position and drag-select the
 * same way the control's own CDEF would if ModalDialog knew about it. */
pascal Boolean NameEntryFilterProc(DialogPtr dlg, EventRecord *event, short *itemHit)
{
    if (event->what == keyDown || event->what == autoKey) {
        char c = (char)(event->message & charCodeMask);
        if (c == '\r' || c == 3) {
            *itemHit = 3;
            return true;
        }
        if (gNameFieldControl != NULL) {
            short keyCode = (short)((event->message & keyCodeMask) >> 8);
            HandleControlKey(gNameFieldControl, keyCode, c, event->modifiers);
        }
        *itemHit = 0;
        return true;
    }

    if (event->what == mouseDown && gNameFieldControl != NULL) {
        WindowPtr w = (WindowPtr)dlg;
        Point local = event->where;
        ControlHandle hitControl = NULL;

        SetPortWindowPort(w);
        GlobalToLocal(&local);
        FindControl(local, w, &hitControl);

        if (hitControl == gNameFieldControl) {
            SetKeyboardFocus(w, gNameFieldControl, kEditTextPart);
            TrackControl(gNameFieldControl, local, NULL);
            *itemHit = 0;
            return true;
        }
    }

    return false;
}

/* Every dialog here keeps its default button immediately followed by the
 * UserItem that rings it, so the ring's own item number minus one always
 * lands on the button -- this makes the same proc reusable regardless of
 * how many other items (EditText, other buttons) come before it. */
pascal void ButtonFrameProc(DialogRef dlg, DialogItemIndex itemNo)
{
    DialogItemType type;
    Handle itemH;
    Rect box;

    GetDialogItem(dlg, itemNo - 1, &type, &itemH, &box);

    SetForeColor(kColorBlack);
    InsetRect(&box, -4, -4);
    PenSize(3, 3);
    FrameRoundRect(&box, 16, 16);
    PenNormal();
}

static void PaintFullDialogBackground(DialogRef dlg)
{
    Rect windowRect;

    GetPortBounds(GetWindowPort((WindowPtr)dlg), &windowRect);
    /* BackColor matters here, not just ForeColor: native controls (the
     * OK button) erase their own rounded corners using the port's
     * BackColor, so leaving it at the default white is what caused
     * white corner pixels around the button before. */
    SetBackColor(kColorWindowBG);
    SetForeColor(kColorWindowBG);
    PaintRect(&windowRect);
}

/* Same gray fill as PaintFullDialogBackground, but leaves holeRect
 * completely untouched -- used to keep a native EditText item's own
 * default white background intact instead of painting over it and then
 * patching a white rect back in, which is still hand-drawing the field
 * even if it looks the same. DrawDialog() never erases an EditText
 * item's interior itself, so whatever we leave under it here is what
 * the field ends up looking like. */
static void PaintDialogBackgroundExcluding(DialogRef dlg, const Rect *holeRect)
{
    Rect windowRect;
    RgnHandle bgRgn;
    RgnHandle holeRgn;

    GetPortBounds(GetWindowPort((WindowPtr)dlg), &windowRect);
    SetBackColor(kColorWindowBG);
    SetForeColor(kColorWindowBG);

    bgRgn = NewRgn();
    holeRgn = NewRgn();
    RectRgn(bgRgn, &windowRect);
    RectRgn(holeRgn, holeRect);
    DiffRgn(bgRgn, holeRgn, bgRgn);
    PaintRgn(bgRgn);
    DisposeRgn(holeRgn);
    DisposeRgn(bgRgn);
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

    PaintFullDialogBackground(dlg);

    GetDialogItem(dlg, 1, &type, &itemH, &box);
    SetForeColor(kColorBlack);
    PenNormal();

    /* Charcoal 12pt -- same font and size as the menu bar. */
    {
        Str255 fontName;
        short charcoalID;
        CStrToPStr(fontName, "Charcoal");
        GetFNum(fontName, &charcoalID);
        TextFont(charcoalID);
    }
    TextFace(normal);
    TextSize(12);
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

    GetDialogItem(dlg, 3, &type, &itemH, &box);
    SetDialogItem(dlg, 3, type, (Handle)NewUserItemUPP(&ButtonFrameProc), &box);

    DrawDialog(dlg);
    DrawControls(dlg);

    do {
        ModalDialog(NewModalFilterUPP(&DismissOnEnterFilterProc), &item);
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

    PaintFullDialogBackground(dlg);

    GetDialogItem(dlg, 1, &type, &itemH, &box);
    midX = box.left + (box.right - box.left) / 2;

    SetForeColor(kColorBlack);
    PenNormal();

    SetRect(&iconRect, midX - 16, box.top + 14, midX + 16, box.top + 46);
    PlotIconID(&iconRect, atNone, ttNone, 128);

    /* Charcoal (Plain) for the three emphasis lines, Geneva (Plain) for
     * everything else -- two distinct typefaces, not just two sizes of
     * one. Charcoal isn't a fixed classic font ID (it's a later
     * TrueType addition), so it has to be looked up by name; GetFNum()
     * falls back to systemFont (0) if it isn't installed. */
    {
        Str255 fontName;
        short charcoalID;
        CStrToPStr(fontName, "Charcoal");
        GetFNum(fontName, &charcoalID);

        TextFace(normal);

        TextFont(charcoalID);
        TextSize(12);
        CStrToPStr(s, "iWordle 1.0");
        DrawCenteredStringAt(midX, box.top + 64, s);

        TextFont(kFontGeneva);
        TextSize(10);
        CStrToPStr(s, "A native Wordle clone for Mac OS 9");
        DrawCenteredStringAt(midX, box.top + 84, s);

        TextFont(charcoalID);
        TextSize(12);
        CStrToPStr(s, "Bruno Castello");
        DrawCenteredStringAt(midX, box.top + 112, s);

        TextFont(kFontGeneva);
        TextSize(10);
        CStrToPStr(s, "bfcastello@hotmail.com");
        DrawCenteredStringAt(midX, box.top + 132, s);

        TextFont(charcoalID);
        TextSize(12);
        CStrToPStr(s, "Engineer: Claude Sonnet 5");
        DrawCenteredStringAt(midX, box.top + 160, s);

        TextFont(kFontGeneva);
        TextSize(10);
        CStrToPStr(s, "\xA9 Castello Designs, 2026");
        DrawCenteredStringAt(midX, box.top + 188, s);

        CStrToPStr(s, "Built with Retro68");
        DrawCenteredStringAt(midX, box.top + 208, s);
    }
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
            break;

        default:
            break;
    }
}

static void OnNewGame(void)
{
    WordleNewGame(&gGame);
    RedrawAll();
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

    GetDialogItem(dlg, 3, &type, &itemH, &box);
    SetDialogItem(dlg, 3, type, (Handle)NewUserItemUPP(&ButtonFrameProc), &box);

    DrawDialog(dlg);
    DrawControls(dlg);

    do {
        ModalDialog(NewModalFilterUPP(&DismissOnEnterFilterProc), &item);
    } while (item != 2);

    DisposeDialog(dlg);
    RedrawAll();
}

/* ---------------------------------------------------------------------- */
/* Player name entry (shown after every win/loss) and the statistics       */
/* scoreboard (File > Statistics...)                                       */
/* ---------------------------------------------------------------------- */

pascal void NameEntryContentDrawProc(DialogRef dlg, DialogItemIndex itemNo)
{
    DialogItemType type;
    Handle itemH;
    Rect box, fieldBox;

    (void)itemNo;

    /* Leave item 2's rect out of the gray fill entirely so the native
     * EditText field keeps its own default white background -- see
     * PaintDialogBackgroundExcluding. */
    GetDialogItem(dlg, 2, &type, &itemH, &fieldBox);
    PaintDialogBackgroundExcluding(dlg, &fieldBox);

    GetDialogItem(dlg, 1, &type, &itemH, &box);
    SetForeColor(kColorBlack);
    PenNormal();

    {
        Str255 fontName;
        short charcoalID;
        CStrToPStr(fontName, "Charcoal");
        GetFNum(fontName, &charcoalID);
        TextFont(charcoalID);
    }
    TextFace(normal);
    TextSize(12);
    DrawCenteredStringAt(box.left + (box.right - box.left) / 2,
                          box.top + (box.bottom - box.top) / 2 + 4,
                          "\pWho's playing?");
}

/* Blocks until OK is hit; outName is empty if the field was left blank
 * (RecordGameResult treats that as "don't record this result"). */
static Boolean PromptForPlayerName(Str255 outName)
{
    DialogPtr dlg;
    short item;
    DialogItemType type;
    Handle itemH;
    Rect box, fieldBox;

    dlg = GetNewDialog(202, NULL, (WindowPtr)-1);
    if (dlg == NULL) { outName[0] = 0; return false; }

    GetDialogItem(dlg, 1, &type, &itemH, &box);
    SetDialogItem(dlg, 1, type, (Handle)NewUserItemUPP(&NameEntryContentDrawProc), &box);

    /* Item 2 is a plain UserItem placeholder reserving layout space --
     * the real field is a Control Manager Edit Text control created
     * here, since that's the only way to get the Appearance Manager's
     * themed look (sunken box, blue focus ring) instead of a classic
     * DITL editText item's flat frame. */
    GetDialogItem(dlg, 2, &type, &itemH, &fieldBox);
    gNameFieldControl = NewControl((WindowPtr)dlg, &fieldBox, "\p", true,
                                    0, 0, 0, kEditTextCDEF, 0L);
    if (gNameFieldControl != NULL) {
        SetControlData(gNameFieldControl, kEditTextPart, kEditTextTextTag,
                        gLastPlayerName[0], (Ptr)(gLastPlayerName + 1));
        SetKeyboardFocus((WindowPtr)dlg, gNameFieldControl, kEditTextPart);
    }

    GetDialogItem(dlg, 4, &type, &itemH, &box);
    SetDialogItem(dlg, 4, type, (Handle)NewUserItemUPP(&ButtonFrameProc), &box);

    DrawDialog(dlg);
    DrawControls(dlg);

    item = 0;
    do {
        ModalDialog(NewModalFilterUPP(&NameEntryFilterProc), &item);
    } while (item != 3);

    outName[0] = 0;
    if (gNameFieldControl != NULL) {
        long actualSize = 0;
        char buf[256];
        if (GetControlData(gNameFieldControl, kEditTextPart, kEditTextTextTag,
                            sizeof(buf), buf, &actualSize) == noErr) {
            if (actualSize > 255) actualSize = 255;
            outName[0] = (unsigned char)actualSize;
            memcpy(outName + 1, buf, (size_t)actualSize);
        }
    }
    gNameFieldControl = NULL;

    DisposeDialog(dlg);
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

    PaintFullDialogBackground(dlg);

    GetDialogItem(dlg, 1, &type, &itemH, &box);
    SetForeColor(kColorBlack);
    PenNormal();

    TextFont(kFontGeneva);
    TextFace(bold);
    TextSize(12);
    MoveTo(box.left + 16, box.top + 20);
    DrawString("\pName");
    MoveTo(box.left + 220, box.top + 20);
    DrawString("\pPlayed");
    MoveTo(box.left + 280, box.top + 20);
    DrawString("\pWin %");
    MoveTo(box.left + 335, box.top + 20);
    DrawString("\pCur");
    MoveTo(box.left + 380, box.top + 20);
    DrawString("\pMax");

    MoveTo(box.left + 10, box.top + 26);
    LineTo(box.right - 10, box.top + 26);

    TextFace(normal);
    TextSize(11);

    if (gStats.playerCount == 0) {
        MoveTo(box.left + 16, box.top + 50);
        DrawString("\pNo players yet -- win or lose a game to get started.");
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

    GetDialogItem(dlg, 4, &type, &itemH, &box);
    SetDialogItem(dlg, 4, type, (Handle)NewUserItemUPP(&ButtonFrameProc), &box);

    DrawDialog(dlg);
    DrawControls(dlg);

    do {
        ModalDialog(NewModalFilterUPP(&DismissOnEnterFilterProc3), &item);
        if (item == 2) {
            /* Force the redraw ourselves rather than invalidating and
             * waiting for the next update event -- DrawDialog() repaints
             * item 1's background but never a native control's content,
             * so an update-driven repaint here would blank the buttons
             * until clicked, same bug the initial reveal had. */
            WordleStatsClear(&gStats);
            SaveStats();
            DrawDialog(dlg);
            DrawControls(dlg);
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

    RedrawAll();
    RunEventLoop();

    return 0;
}
