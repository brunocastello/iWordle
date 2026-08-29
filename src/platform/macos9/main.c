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
#include <Events.h>
#include <ToolUtils.h>
#include <TextUtils.h>
#include <OSUtils.h>
#include <Icons.h>
#include <AppleEvents.h>

#include <string.h>
#include <ctype.h>

#include "wordle_engine.h"

/* Not declared by Retro68's headers; 3 is Geneva's well-known classic
 * system font ID (systemFont=0, applFont=1, newYork=2, geneva=3, ...). */
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

/* ---------------------------------------------------------------------- */
/* Forward declarations                                                   */
/* ---------------------------------------------------------------------- */

typedef void (*ModalDrawProc)(WindowPtr w);
typedef Boolean (*ModalOKHitProc)(Point local);
static void RunSimpleModalWindow(WindowPtr w, ModalDrawProc drawFn, ModalOKHitProc okHitFn);

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
/* Simple modal windows                                                    */
/*                                                                        */
/* The message box and About box are plain WIND resources, not Dialog     */
/* Manager DLOG/DITL dialogs: we draw their entire content (background,   */
/* text, and the OK button) ourselves and run our own small event loop,   */
/* exactly like the main game window already does. This matches how a    */
/* real classic app builds this kind of window (confirmed by inspecting  */
/* OS9Map's About window: a plain WIND, no DLOG/DITL/CNTL at all), and    */
/* it sidesteps a string of Dialog/Appearance Manager quirks we hit       */
/* trying to theme DLOG-based dialogs instead (backgrounds not            */
/* repainting, overlapping DITL items swallowing clicks, StaticText not   */
/* rendering once a background UserItem was added, native controls        */
/* erasing their corners against the wrong background color).             */
/* ---------------------------------------------------------------------- */

static void RunSimpleModalWindow(WindowPtr w, ModalDrawProc drawFn, ModalOKHitProc okHitFn)
{
    EventRecord event;
    Boolean done = false;

    SetPortWindowPort(w);
    drawFn(w);
    ShowWindow(w);
    SelectWindow(w);

    while (!done) {
        if (WaitNextEvent(everyEvent, &event, 15, NULL)) {
            switch (event.what) {
                case updateEvt:
                    if ((WindowPtr)event.message == w) {
                        BeginUpdate(w);
                        SetPortWindowPort(w);
                        drawFn(w);
                        EndUpdate(w);
                    } else if ((WindowPtr)event.message == gWindow) {
                        HandleUpdate(&event);
                    }
                    break;

                case mouseDown: {
                    WindowPtr which;
                    short part = FindWindow(event.where, &which);
                    if (which == w && part == inContent) {
                        Point local = event.where;
                        SetPortWindowPort(w);
                        GlobalToLocal(&local);
                        if (okHitFn(local)) done = true;
                    }
                    /* Clicks outside this window are swallowed (modal). */
                    break;
                }

                case keyDown:
                case autoKey: {
                    char c = (char)(event.message & charCodeMask);
                    if (c == '\r' || c == 3 || c == 0x1B) done = true;
                    break;
                }

                default:
                    break;
            }
        }
    }

    DisposeWindow(w);
}

/* ---------------------------------------------------------------------- */
/* Message window (New Game / Give Up / Win / Lose)                       */
/* ---------------------------------------------------------------------- */

/* Set by ShowMessage() just before the modal loop; read by
 * DrawMessageContent(). */
static Str255 gMessageText;

static void GetMessageOKButtonRect(Rect *r)
{
    SetRect(r, 135, 98, 205, 122);
}

static void DrawMessageContent(WindowPtr w)
{
    Rect full, okRect;
    Str255 ok;

    GetPortBounds(GetWindowPort(w), &full);

    SetForeColor(kColorWindowBG);
    PaintRect(&full);
    SetForeColor(kColorBlack);
    PenNormal();

    TextFont(kFontGeneva);
    TextFace(normal);
    TextSize(11);
    DrawCenteredStringAt((full.left + full.right) / 2, 55, gMessageText);

    GetMessageOKButtonRect(&okRect);
    DrawBevelRect(&okRect, kColorKeyFace);
    SetForeColor(kColorBorder);
    TextSize(12);
    TextFace(bold);
    CStrToPStr(ok, "OK");
    DrawCenteredStringAt((okRect.left + okRect.right) / 2,
                          okRect.top + (okRect.bottom - okRect.top) / 2 + 4, ok);
    SetForeColor(kColorBlack);
}

static Boolean MessageOKHit(Point local)
{
    Rect r;
    GetMessageOKButtonRect(&r);
    return PtInRect(local, &r);
}

static void ShowMessage(ConstStr255Param msg)
{
    WindowPtr w;

    PStrCopy(gMessageText, msg);

    w = GetNewCWindow(200, NULL, (WindowPtr)-1);
    if (w == NULL) return;

    RunSimpleModalWindow(w, DrawMessageContent, MessageOKHit);
    RedrawAll();
}

/* ---------------------------------------------------------------------- */
/* About window: icon, app name/version, author, credits, OK button       */
/* ---------------------------------------------------------------------- */

static void GetAboutOKButtonRect(Rect *r)
{
    SetRect(r, 105, 204, 175, 228);
}

static void DrawAboutContent(WindowPtr w)
{
    Rect full, iconRect, okRect;
    Str255 s;
    short midX;

    GetPortBounds(GetWindowPort(w), &full);
    midX = (full.left + full.right) / 2;

    SetForeColor(kColorWindowBG);
    PaintRect(&full);
    SetForeColor(kColorBlack);
    PenNormal();

    SetRect(&iconRect, midX - 16, full.top + 6, midX + 16, full.top + 38);
    PlotIconID(&iconRect, atNone, ttNone, 128);

    /* Classic About-box convention: the app name is set in the bold
     * System font (the OS's own UI face), while body/credit lines use
     * Geneva -- two distinct typefaces, not just two sizes of one. */
    TextFont(systemFont);
    TextFace(bold);
    TextSize(18);
    CStrToPStr(s, "iWordle 1.0");
    DrawCenteredStringAt(midX, full.top + 56, s);

    /* Everything below the title is Geneva at one consistent size;
     * only bold/plain varies, matching the reference layout. */
    TextFont(kFontGeneva);
    TextSize(10);

    TextFace(normal);
    CStrToPStr(s, "A native Wordle clone for Mac OS 9");
    DrawCenteredStringAt(midX, full.top + 74, s);

    TextFace(bold);
    CStrToPStr(s, "Bruno Castello");
    DrawCenteredStringAt(midX, full.top + 100, s);

    TextFace(normal);
    CStrToPStr(s, "bfcastello@hotmail.com");
    DrawCenteredStringAt(midX, full.top + 118, s);

    TextFace(bold);
    CStrToPStr(s, "Engineer: Claude Sonnet 5");
    DrawCenteredStringAt(midX, full.top + 144, s);

    TextFace(normal);
    CStrToPStr(s, "\xA9 Castello Designs, 2026");
    DrawCenteredStringAt(midX, full.top + 170, s);

    CStrToPStr(s, "Built with Retro68");
    DrawCenteredStringAt(midX, full.top + 188, s);

    GetAboutOKButtonRect(&okRect);
    DrawBevelRect(&okRect, kColorKeyFace);
    SetForeColor(kColorBorder);
    TextSize(12);
    TextFace(bold);
    CStrToPStr(s, "OK");
    DrawCenteredStringAt((okRect.left + okRect.right) / 2,
                          okRect.top + (okRect.bottom - okRect.top) / 2 + 4, s);
    SetForeColor(kColorBlack);
}

static Boolean AboutOKHit(Point local)
{
    Rect r;
    GetAboutOKButtonRect(&r);
    return PtInRect(local, &r);
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
            } else if (gGame.status == kGameLost) {
                Str255 msg;
                BuildLoseMessage(msg);
                ShowMessage(msg);
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
}

static void OnAbout(void)
{
    WindowPtr w = GetNewCWindow(201, NULL, (WindowPtr)-1);
    if (w == NULL) return;

    RunSimpleModalWindow(w, DrawAboutContent, AboutOKHit);
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
            else if (menuItem == 4) gDone = true;
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

    WordleSeedRandom((unsigned long)TickCount());
    WordleNewGame(&gGame);

    RedrawAll();
    RunEventLoop();

    return 0;
}
