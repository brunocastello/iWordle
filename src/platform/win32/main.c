/*
 * iWordle - Windows (Win32 / x86) front end.
 *
 * All game rules live in the portable core (src/core/wordle_engine.*).
 * This file only knows about plain ANSI Win32: one window, a menu, native
 * dialogs from iWordle.rc and a GDI-painted board. Built by Open Watcom 1.9
 * as a single 32-bit EXE that runs on Windows NT 3.51, 95/98/ME, NT 4.0,
 * 2000 and XP, so it sticks to the API set that exists on NT 3.51 (see
 * docs/win32-port-plan.md): KERNEL32, USER32, GDI32 and COMCTL32 only, no
 * UNICODE, no 3D-era system colors, no post-3.51 calls. C89 throughout.
 */

#include <windows.h>
#include <commctrl.h>

#include <string.h>
#include <ctype.h>

#include "wordle_engine.h"
#include "wordle_stats.h"
#include "resource.h"

/* ---------------------------------------------------------------------- */
/* Board / keyboard layout                                                */
/* ---------------------------------------------------------------------- */

/* The layout is designed at 356 x 398 (small enough for a 640x480 desktop
 * with a taskbar). The widest row (the bottom row: ENTER + 7 letters + BACK)
 * sets the width and everything else is centered within it. The window is
 * resizable: every rect and font is scaled from these design units by
 * gScale (per mille, aspect ratio kept) and centered in the client area. */
#define MARGIN             10

#define TILE_SIZE          38
#define TILE_GAP           4
#define BOARD_TOP          MARGIN
#define BOARD_LEFT         75              /* (356 - 206) / 2 */

#define KEY_W              30
#define KEY_H              36
#define KEY_GAP            4
#define KEY_ROW_GAP        6
#define KEYBOARD_TOP       268             /* 10 + 248 + 10 */
#define KB_ROW0_LEFT       MARGIN          /* 10 keys = 336 */
#define KB_ROW1_LEFT       27              /* 9 keys  = 302, (356 - 302) / 2 */
#define KB_ROW2_LEFT       61              /* letters, after the ENTER key */
#define KB_WIDE_KEY_WIDTH  47              /* ENTER + gap + 7 keys + gap + BACK = 336 */
#define KB_ROW_COUNT       3

#define CLIENT_W           356
#define CLIENT_H           398             /* 268 + 3*36 + 2*6 + 10 */
#define MIN_SCALE          600             /* smallest window: 60% */

static const char * const kKBRows[KB_ROW_COUNT] = {
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM"
};

/* ---------------------------------------------------------------------- */
/* Palette (see CLAUDE.md)                                                 */
/* ---------------------------------------------------------------------- */

#define COLOR_TILE_EMPTY    RGB(0xFF, 0xFF, 0xFF)
#define COLOR_TILE_CORRECT  RGB(0x6A, 0xAA, 0x64)
#define COLOR_TILE_PRESENT  RGB(0xC9, 0xB4, 0x58)
#define COLOR_TILE_ABSENT   RGB(0x78, 0x7C, 0x7E)
#define COLOR_TILE_BORDER   RGB(0x33, 0x33, 0x33)
#define COLOR_LABEL_WHITE   RGB(0xFF, 0xFF, 0xFF)

#define STATS_FILE_NAME     "IWORDLE.DAT"

/* ---------------------------------------------------------------------- */
/* Globals                                                                 */
/* ---------------------------------------------------------------------- */

static HINSTANCE gInst;
static HWND gWindow;
static HACCEL gAccel;
static WordleGame gGame;
static WordleStatsBook gStats;
static char gLastPlayerName[WORDLE_STATS_NAME_LEN + 1] = { 0 };
static char gStatsPath[MAX_PATH + 16];

/* Current scale (per mille) and where the scaled layout sits in the
 * client area; set by UpdateLayout on every WM_SIZE. */
static int gScale = 1000;
static int gOffsetX = 0;
static int gOffsetY = 0;

static HFONT gTileFont;       /* Arial bold, 22px at 100% */
static HFONT gKeyFont;        /* Arial bold, 14px at 100% */
static HFONT gWideKeyFont;    /* Arial bold, 11px at 100% */

/* Handed between PromptForPlayerName and NameDlgProc. */
static char gDialogName[WORDLE_STATS_NAME_LEN + 1];

/* Bold copy of the About box's font; created in WM_INITDIALOG, freed in
 * WM_DESTROY. */
static HFONT gAboutBoldFont;

/* ---------------------------------------------------------------------- */
/* Forward declarations                                                   */
/* ---------------------------------------------------------------------- */

static void BuildStatsPath(void);
static void LoadStats(void);
static void SaveStats(void);
static BOOL PromptForPlayerName(char *outName);
static void RecordGameResult(BOOL won);
static void OnStatistics(void);

static int ScaleLength(int v);
static void ScaleRect(RECT *r);
static void UpdateLayout(int clientW, int clientH);
static void GetTileRect(short row, short col, RECT *outRect);
static short KeyboardRowLeft(short row);
static void GetLetterKeyRect(short row, short idx, RECT *outRect);
static void GetBackspaceKeyRect(RECT *outRect);
static void GetEnterKeyRect(RECT *outRect);

static BOOL GetStateColor(WordleLetterState state, COLORREF *outColor);
static void DrawLabel(HDC hdc, const RECT *r, const char *text, HFONT font, COLORREF color);
static void DrawKeyCell(HDC hdc, const RECT *r, COLORREF face, COLORREF textColor,
                        const char *label, HFONT font);
static void DrawTile(HDC hdc, short row, short col);
static void DrawKey(HDC hdc, const RECT *r, char letter);
static void DrawWideKey(HDC hdc, const RECT *r, const char *label);
static void DrawBoard(HDC hdc);
static void DrawKeyboard(HDC hdc);
static void PaintClient(HDC hdc, int w, int h);
static void OnPaint(HWND hwnd);
static void InvalidateRow(short row);
static void RedrawAll(void);

static void ShowMessage(const char *msg, UINT icon);
static void BuildWinMessage(char *out);
static void BuildLoseMessage(char *out);

static void OnLetterKey(char c);
static void OnBackspace(void);
static void OnSubmit(void);
static void OnNewGame(void);
static void OnGiveUp(void);
static void OnAbout(void);

static void CenterDialog(HWND hDlg);
static void FillRankingList(HWND hDlg);
static BOOL CALLBACK NameDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK RankingDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

static void HandleContentClick(POINT pt);
static void HandleChar(WPARAM wParam, LPARAM lParam);
static void UpdateGameMenuState(void);
static void HandleCommand(int id);
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void OnGetMinMaxInfo(MINMAXINFO *info);
static HFONT MakeArialBold(int height);
static void CreateFonts(void);
static void DeleteFonts(void);
static BOOL CreateMainWindow(int nCmdShow);

/* ---------------------------------------------------------------------- */
/* Layout helpers                                                          */
/* ---------------------------------------------------------------------- */

static int ScaleLength(int v)
{
    return MulDiv(v, gScale, 1000);
}

/* Design units -> client pixels. Edges are scaled independently, so
 * neighboring cells keep a consistent gap at every size. */
static void ScaleRect(RECT *r)
{
    r->left = gOffsetX + ScaleLength(r->left);
    r->top = gOffsetY + ScaleLength(r->top);
    r->right = gOffsetX + ScaleLength(r->right);
    r->bottom = gOffsetY + ScaleLength(r->bottom);
}

/* Picks the largest scale at which the whole design fits the client area,
 * centers it, and rebuilds the fonts at that scale. */
static void UpdateLayout(int clientW, int clientH)
{
    int sx, sy, scale;

    if (clientW <= 0 || clientH <= 0) return;

    sx = MulDiv(clientW, 1000, CLIENT_W);
    sy = MulDiv(clientH, 1000, CLIENT_H);
    scale = (sx < sy) ? sx : sy;
    if (scale < 1) scale = 1;

    gOffsetX = (clientW - MulDiv(CLIENT_W, scale, 1000)) / 2;
    gOffsetY = (clientH - MulDiv(CLIENT_H, scale, 1000)) / 2;

    if (scale != gScale || gTileFont == NULL) {
        gScale = scale;
        DeleteFonts();
        CreateFonts();
    }
}

static void GetTileRect(short row, short col, RECT *outRect)
{
    int top = BOARD_TOP + row * (TILE_SIZE + TILE_GAP);
    int left = BOARD_LEFT + col * (TILE_SIZE + TILE_GAP);
    SetRect(outRect, left, top, left + TILE_SIZE, top + TILE_SIZE);
    ScaleRect(outRect);
}

static short KeyboardRowLeft(short row)
{
    switch (row) {
        case 0: return KB_ROW0_LEFT;
        case 1: return KB_ROW1_LEFT;
        default: return KB_ROW2_LEFT;
    }
}

static void GetLetterKeyRect(short row, short idx, RECT *outRect)
{
    int top = KEYBOARD_TOP + row * (KEY_H + KEY_ROW_GAP);
    int left = KeyboardRowLeft(row) + idx * (KEY_W + KEY_GAP);
    SetRect(outRect, left, top, left + KEY_W, top + KEY_H);
    ScaleRect(outRect);
}

static void GetBackspaceKeyRect(RECT *outRect)
{
    int top = KEYBOARD_TOP + 2 * (KEY_H + KEY_ROW_GAP);
    int left = KB_ROW2_LEFT + 7 * (KEY_W + KEY_GAP);
    SetRect(outRect, left, top, left + KB_WIDE_KEY_WIDTH, top + KEY_H);
    ScaleRect(outRect);
}

/* ENTER sits left of Z, mirroring BACK on the right (as in Wordle), so
 * mouse-only players can submit without the Return key. */
static void GetEnterKeyRect(RECT *outRect)
{
    int top = KEYBOARD_TOP + 2 * (KEY_H + KEY_ROW_GAP);
    int left = KB_ROW2_LEFT - KEY_GAP - KB_WIDE_KEY_WIDTH;
    SetRect(outRect, left, top, left + KB_WIDE_KEY_WIDTH, top + KEY_H);
    ScaleRect(outRect);
}

/* ---------------------------------------------------------------------- */
/* Drawing                                                                 */
/*                                                                        */
/* Everything is painted into a memory DC and blitted once (see OnPaint),  */
/* so there is no flicker on any Windows version. Brushes and pens are    */
/* created per use and deleted right after; every SelectObject is undone  */
/* before the object is deleted.                                           */
/* ---------------------------------------------------------------------- */

/* Palette color for a correct / present / absent cell. Returns FALSE for an
 * empty one, which uses the plain tile or system key face instead. */
static BOOL GetStateColor(WordleLetterState state, COLORREF *outColor)
{
    switch (state) {
        case kLetterCorrect: *outColor = COLOR_TILE_CORRECT; return TRUE;
        case kLetterPresent: *outColor = COLOR_TILE_PRESENT; return TRUE;
        case kLetterAbsent:  *outColor = COLOR_TILE_ABSENT;  return TRUE;
        default: return FALSE;
    }
}

/* Draws text centered in r with the given font and color, no background. */
static void DrawLabel(HDC hdc, const RECT *r, const char *text, HFONT font, COLORREF color)
{
    RECT box = *r;
    HFONT oldFont;

    if (text[0] == '\0') return;

    oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawText(hdc, text, -1, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
}

/* A raised key built only from Win 3.x system colors, so it matches whatever
 * look the OS has: 1px window-frame outline, highlight on the top/left
 * inside edge, shadow on the bottom/right. face fills the cell. */
static void DrawKeyCell(HDC hdc, const RECT *r, COLORREF face, COLORREF textColor,
                        const char *label, HFONT font)
{
    HBRUSH faceBrush = CreateSolidBrush(face);
    HBRUSH frameBrush = CreateSolidBrush(GetSysColor(COLOR_WINDOWFRAME));
    HBRUSH hiBrush = CreateSolidBrush(GetSysColor(COLOR_BTNHIGHLIGHT));
    HBRUSH shadowBrush = CreateSolidBrush(GetSysColor(COLOR_BTNSHADOW));
    RECT edge;

    FillRect(hdc, r, faceBrush);
    FrameRect(hdc, r, frameBrush);

    SetRect(&edge, r->left + 1, r->top + 1, r->right - 1, r->top + 2);
    FillRect(hdc, &edge, hiBrush);
    SetRect(&edge, r->left + 1, r->top + 1, r->left + 2, r->bottom - 1);
    FillRect(hdc, &edge, hiBrush);

    SetRect(&edge, r->left + 1, r->bottom - 2, r->right - 1, r->bottom - 1);
    FillRect(hdc, &edge, shadowBrush);
    SetRect(&edge, r->right - 2, r->top + 1, r->right - 1, r->bottom - 1);
    FillRect(hdc, &edge, shadowBrush);

    DrawLabel(hdc, r, label, font, textColor);

    DeleteObject(faceBrush);
    DeleteObject(frameBrush);
    DeleteObject(hiBrush);
    DeleteObject(shadowBrush);
}

static void DrawTile(HDC hdc, short row, short col)
{
    RECT r;
    WordleLetterState state = gGame.rows[row].states[col];
    char letter = gGame.rows[row].letters[col];
    char label[2];
    COLORREF fill;
    COLORREF textColor = COLOR_LABEL_WHITE;
    HBRUSH brush;
    HPEN pen;
    HBRUSH oldBrush;
    HPEN oldPen;

    GetTileRect(row, col, &r);

    if (!GetStateColor(state, &fill)) {
        fill = COLOR_TILE_EMPTY;
        textColor = COLOR_TILE_BORDER;
    }

    brush = CreateSolidBrush(fill);
    pen = CreatePen(PS_SOLID, 1, COLOR_TILE_BORDER);
    oldBrush = (HBRUSH)SelectObject(hdc, brush);
    oldPen = (HPEN)SelectObject(hdc, pen);
    Rectangle(hdc, r.left, r.top, r.right, r.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    label[0] = letter;
    label[1] = '\0';
    DrawLabel(hdc, &r, label, gTileFont, textColor);
}

static void DrawKey(HDC hdc, const RECT *r, char letter)
{
    WordleLetterState state = gGame.keyStates[letter - 'A'];
    char label[2];
    COLORREF face;
    COLORREF textColor = COLOR_LABEL_WHITE;

    if (!GetStateColor(state, &face)) {
        face = GetSysColor(COLOR_BTNFACE);
        textColor = GetSysColor(COLOR_BTNTEXT);
    }

    label[0] = letter;
    label[1] = '\0';
    DrawKeyCell(hdc, r, face, textColor, label, gKeyFont);
}

/* ENTER / BACK: always the plain face, in the smaller bold font. */
static void DrawWideKey(HDC hdc, const RECT *r, const char *label)
{
    DrawKeyCell(hdc, r, GetSysColor(COLOR_BTNFACE), GetSysColor(COLOR_BTNTEXT),
                label, gWideKeyFont);
}

static void DrawBoard(HDC hdc)
{
    short row, col;
    for (row = 0; row < WORDLE_MAX_GUESSES; row++) {
        for (col = 0; col < WORDLE_WORD_LENGTH; col++) {
            DrawTile(hdc, row, col);
        }
    }
}

static void DrawKeyboard(HDC hdc)
{
    short row, idx;
    RECT r;

    for (row = 0; row < KB_ROW_COUNT; row++) {
        short count = (short)strlen(kKBRows[row]);
        for (idx = 0; idx < count; idx++) {
            GetLetterKeyRect(row, idx, &r);
            DrawKey(hdc, &r, kKBRows[row][idx]);
        }
    }

    GetBackspaceKeyRect(&r);
    DrawWideKey(hdc, &r, "BACK");
    GetEnterKeyRect(&r);
    DrawWideKey(hdc, &r, "ENTER");
}

/* Paints the whole client area (w x h) into hdc. */
static void PaintClient(HDC hdc, int w, int h)
{
    RECT all;
    HBRUSH bg = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));

    SetRect(&all, 0, 0, w, h);
    FillRect(hdc, &all, bg);
    DeleteObject(bg);

    DrawBoard(hdc);
    DrawKeyboard(hdc);
}

/* Renders the full client into a memory bitmap and blits it, so the window
 * never shows a half-drawn or erased state. Falls back to drawing straight
 * onto the window DC if the bitmap can't be allocated. */
static void OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    RECT client;
    HDC hdc = BeginPaint(hwnd, &ps);
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = NULL;
    HBITMAP oldBmp = NULL;
    int w, h;

    GetClientRect(hwnd, &client);
    w = client.right;
    h = client.bottom;

    if (mem != NULL && w > 0 && h > 0) {
        bmp = CreateCompatibleBitmap(hdc, w, h);
    }

    if (mem != NULL && bmp != NULL) {
        oldBmp = (HBITMAP)SelectObject(mem, bmp);
        PaintClient(mem, w, h);
        BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBmp);
    } else {
        PaintClient(hdc, w, h);
    }

    if (bmp != NULL) DeleteObject(bmp);
    if (mem != NULL) DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

/* Per keystroke only the row being typed changes. */
static void InvalidateRow(short row)
{
    RECT first, last;

    if (row < 0 || row >= WORDLE_MAX_GUESSES) return;

    GetTileRect(row, 0, &first);
    GetTileRect(row, WORDLE_WORD_LENGTH - 1, &last);
    first.right = last.right;
    InvalidateRect(gWindow, &first, FALSE);
}

/* Full repaint, forced to the screen now so the final colors are visible
 * behind any message box that follows. */
static void RedrawAll(void)
{
    InvalidateRect(gWindow, NULL, FALSE);
    UpdateWindow(gWindow);
}

/* ---------------------------------------------------------------------- */
/* Message boxes                                                           */
/* ---------------------------------------------------------------------- */

static void ShowMessage(const char *msg, UINT icon)
{
    MessageBox(gWindow, msg, "iWordle", MB_OK | icon);
}

static void BuildWinMessage(char *out)
{
    wsprintf(out, "You got it in %d guess(es)!", gGame.currentRow + 1);
}

static void BuildLoseMessage(char *out)
{
    wsprintf(out, "Out of guesses! The word was %s.", gGame.target);
}

/* ---------------------------------------------------------------------- */
/* Statistics persistence                                                  */
/*                                                                        */
/* Stored as a flat dump of WordleStatsBook in IWORDLE.DAT next to the    */
/* EXE (8.3 name, safe on FAT without long names). Windows-only: the file  */
/* is not shared with the Mac builds. Saving fails silently when the      */
/* folder is read-only (CD, floppy, limited XP account in Program Files). */
/* ---------------------------------------------------------------------- */

static void BuildStatsPath(void)
{
    int len;

    gStatsPath[0] = '\0';
    GetModuleFileName(NULL, gStatsPath, MAX_PATH);
    len = lstrlen(gStatsPath);
    while (len > 0 && gStatsPath[len - 1] != '\\') len--;
    lstrcpy(gStatsPath + len, STATS_FILE_NAME);
}

static void LoadStats(void)
{
    HANDLE file;
    DWORD got = 0;
    BOOL ok;

    WordleStatsInit(&gStats);

    file = CreateFile(gStatsPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;

    ok = ReadFile(file, &gStats, sizeof(gStats), &got, NULL);
    CloseHandle(file);

    /* A short or garbled file must not leave half a book in memory. */
    if (!ok || got != (DWORD)sizeof(gStats) ||
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
    HANDLE file;
    DWORD written = 0;

    file = CreateFile(gStatsPath, GENERIC_WRITE, 0, NULL,
                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;

    WriteFile(file, &gStats, sizeof(gStats), &written, NULL);
    CloseHandle(file);
}

/* ---------------------------------------------------------------------- */
/* Dialogs                                                                 */
/*                                                                        */
/* All three are real DIALOG resources (iWordle.rc) run with               */
/* DialogBoxParam, so they take on the look of whichever Windows is        */
/* running. IDCANCEL (Esc / close box) ends every one of them.             */
/* ---------------------------------------------------------------------- */

/* Centers a dialog over the main window (DS_CENTER is 95+), keeping it on
 * screen. Centers on the screen instead if the main window is minimized. */
static void CenterDialog(HWND hDlg)
{
    RECT dlg, owner;
    int x, y, w, h, sw, sh;

    GetWindowRect(hDlg, &dlg);
    w = dlg.right - dlg.left;
    h = dlg.bottom - dlg.top;
    sw = GetSystemMetrics(SM_CXSCREEN);
    sh = GetSystemMetrics(SM_CYSCREEN);

    if (gWindow != NULL && !IsIconic(gWindow)) {
        GetWindowRect(gWindow, &owner);
    } else {
        SetRect(&owner, 0, 0, sw, sh);
    }

    x = owner.left + ((owner.right - owner.left) - w) / 2;
    y = owner.top + ((owner.bottom - owner.top) - h) / 2;
    if (x + w > sw) x = sw - w;
    if (y + h > sh) y = sh - h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/* IDD_NAME: OK copies the text into gDialogName; Cancel leaves it empty. */
static BOOL CALLBACK NameDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;

    switch (msg) {
        case WM_INITDIALOG:
            CenterDialog(hDlg);
            SendDlgItemMessage(hDlg, IDC_NAME, EM_LIMITTEXT, WORDLE_STATS_NAME_LEN, 0);
            SetDlgItemText(hDlg, IDC_NAME, gLastPlayerName);
            SendDlgItemMessage(hDlg, IDC_NAME, EM_SETSEL, 0, -1);
            SetFocus(GetDlgItem(hDlg, IDC_NAME));
            return FALSE;   /* we set the focus ourselves */

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    GetDlgItemText(hDlg, IDC_NAME, gDialogName, sizeof(gDialogName));
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

/* Fills the ranking list from gStats, one tab-separated row per player. */
static void FillRankingList(HWND hDlg)
{
    unsigned short i;
    char line[96];

    SendDlgItemMessage(hDlg, IDC_LIST, LB_RESETCONTENT, 0, 0);

    if (gStats.playerCount == 0) {
        SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0,
                           (LPARAM)"No players yet -- win or lose a game to get started.");
        return;
    }

    for (i = 0; i < gStats.playerCount; i++) {
        const WordlePlayerStats *p = &gStats.players[i];
        unsigned int winPct = 0;

        if (p->gamesPlayed > 0) {
            winPct = ((unsigned int)p->gamesWon * 100u) / p->gamesPlayed;
        }
        wsprintf(line, "%s\t%u\t%u\t%u\t%u", p->name,
                 (unsigned int)p->gamesPlayed, winPct,
                 (unsigned int)p->currentStreak, (unsigned int)p->maxStreak);
        SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)line);
    }
}

/* IDD_RANKING: Clear wipes the book, saves it and refills the list. */
static BOOL CALLBACK RankingDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    /* Tab stops in dialog units, matching the header statics in the .rc. */
    static int tabs[4] = { 110, 145, 180, 210 };

    (void)lParam;

    switch (msg) {
        case WM_INITDIALOG:
            CenterDialog(hDlg);
            SendDlgItemMessage(hDlg, IDC_LIST, LB_SETTABSTOPS, 4, (LPARAM)tabs);
            FillRankingList(hDlg);
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_CLEAR:
                    WordleStatsClear(&gStats);
                    SaveStats();
                    FillRankingList(hDlg);
                    return TRUE;
                case IDOK:
                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

/* IDD_ABOUT: the title and author lines are the dialog font in bold. */
static BOOL CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HFONT dlgFont;
    LOGFONT lf;

    (void)lParam;

    switch (msg) {
        case WM_INITDIALOG:
            CenterDialog(hDlg);
            dlgFont = (HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0);
            if (dlgFont != NULL &&
                GetObject(dlgFont, sizeof(lf), &lf) != 0) {
                lf.lfWeight = FW_BOLD;
                gAboutBoldFont = CreateFontIndirect(&lf);
                if (gAboutBoldFont != NULL) {
                    SendDlgItemMessage(hDlg, IDC_ABOUT_TITLE, WM_SETFONT,
                                       (WPARAM)gAboutBoldFont, 0);
                    SendDlgItemMessage(hDlg, IDC_ABOUT_AUTHOR, WM_SETFONT,
                                       (WPARAM)gAboutBoldFont, 0);
                }
            }
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wParam));
                return TRUE;
            }
            break;

        case WM_DESTROY:
            if (gAboutBoldFont != NULL) {
                DeleteObject(gAboutBoldFont);
                gAboutBoldFont = NULL;
            }
            break;
    }
    return FALSE;
}

/* Blocks until dismissed. Returns TRUE only if OK was hit with a non-blank
 * name (RecordGameResult treats anything else as "don't record this
 * result"). */
static BOOL PromptForPlayerName(char *outName)
{
    int result;
    int i;

    outName[0] = '\0';
    gDialogName[0] = '\0';

    result = DialogBoxParam(gInst, MAKEINTRESOURCE(IDD_NAME), gWindow, NameDlgProc, 0);
    UpdateWindow(gWindow);
    if (result != IDOK) return FALSE;

    for (i = 0; gDialogName[i] != '\0'; i++) {
        if (gDialogName[i] != ' ') {
            lstrcpyn(outName, gDialogName, WORDLE_STATS_NAME_LEN + 1);
            return TRUE;
        }
    }
    return FALSE;
}

/* Prompts for a name and records won/lost against it. Skips recording
 * entirely if the name was left blank or the prompt cancelled -- opening
 * Ranking afterward is conditioned on actually reaching SaveStats() for the
 * same reason: nothing was saved, so there's nothing new to show. */
static void RecordGameResult(BOOL won)
{
    char name[WORDLE_STATS_NAME_LEN + 1];

    if (!PromptForPlayerName(name)) return;

    WordleStatsRecordResult(&gStats, name, won);
    lstrcpyn(gLastPlayerName, name, sizeof(gLastPlayerName));
    SaveStats();

    OnStatistics();
}

static void OnStatistics(void)
{
    DialogBoxParam(gInst, MAKEINTRESOURCE(IDD_RANKING), gWindow, RankingDlgProc, 0);
    UpdateWindow(gWindow);
}

static void OnAbout(void)
{
    DialogBoxParam(gInst, MAKEINTRESOURCE(IDD_ABOUT), gWindow, AboutDlgProc, 0);
    UpdateWindow(gWindow);
}

/* ---------------------------------------------------------------------- */
/* Game actions                                                            */
/* ---------------------------------------------------------------------- */

/* "End Game" (Game menu) only makes sense while a game is actually in
 * progress. Called after every game state change. */
static void UpdateGameMenuState(void)
{
    HMENU menu = GetMenu(gWindow);
    if (menu == NULL) return;

    EnableMenuItem(menu, IDM_ENDGAME,
                   MF_BYCOMMAND | (gGame.status == kGameInProgress ? MF_ENABLED : MF_GRAYED));
}

/* Letter / backspace only touch the row being typed, so only that row is
 * invalidated. */
static void OnLetterKey(char c)
{
    if (gGame.status != kGameInProgress) return;
    WordleTypeLetter(&gGame, c);
    InvalidateRow(gGame.currentRow);
}

static void OnBackspace(void)
{
    if (gGame.status != kGameInProgress) return;
    WordleBackspace(&gGame);
    InvalidateRow(gGame.currentRow);
}

static void OnSubmit(void)
{
    WordleSubmitResult result;

    if (gGame.status != kGameInProgress) return;

    result = WordleSubmitGuess(&gGame);
    switch (result) {
        case kSubmitTooShort:
            MessageBeep(MB_OK);
            break;

        case kSubmitNotInDictionary:
            ShowMessage("Not in word list.", MB_ICONEXCLAMATION);
            break;

        case kSubmitOk:
            RedrawAll();
            if (gGame.status == kGameWon) {
                char msg[64];
                BuildWinMessage(msg);
                ShowMessage(msg, MB_ICONINFORMATION);
                RecordGameResult(TRUE);
            } else if (gGame.status == kGameLost) {
                char msg[64];
                BuildLoseMessage(msg);
                ShowMessage(msg, MB_ICONINFORMATION);
                RecordGameResult(FALSE);
            }
            UpdateGameMenuState();
            break;

        default:
            break;
    }
}

static void OnNewGame(void)
{
    WordleNewGame(&gGame);
    RedrawAll();
    UpdateGameMenuState();
}

static void OnGiveUp(void)
{
    char msg[64];

    if (gGame.status != kGameInProgress) return;

    gGame.status = kGameLost;
    BuildLoseMessage(msg);
    RedrawAll();
    ShowMessage(msg, MB_ICONINFORMATION);
    RecordGameResult(FALSE);
    UpdateGameMenuState();
}

/* ---------------------------------------------------------------------- */
/* Event handling                                                          */
/* ---------------------------------------------------------------------- */

/* Letter keys first, then BACK, then ENTER, on mouse-down. */
static void HandleContentClick(POINT pt)
{
    short row, idx;
    RECT r;

    for (row = 0; row < KB_ROW_COUNT; row++) {
        short count = (short)strlen(kKBRows[row]);
        for (idx = 0; idx < count; idx++) {
            GetLetterKeyRect(row, idx, &r);
            if (PtInRect(&r, pt)) {
                OnLetterKey(kKBRows[row][idx]);
                return;
            }
        }
    }

    GetBackspaceKeyRect(&r);
    if (PtInRect(&r, pt)) {
        OnBackspace();
        return;
    }

    GetEnterKeyRect(&r);
    if (PtInRect(&r, pt)) {
        OnSubmit();
    }
}

/* WM_CHAR: letters, backspace and Return. A held-down Return arrives as
 * auto-repeat (bit 30 of lParam set) and must not re-submit. */
static void HandleChar(WPARAM wParam, LPARAM lParam)
{
    if (wParam >= 'a' && wParam <= 'z') {
        OnLetterKey((char)toupper((int)wParam));
    } else if (wParam >= 'A' && wParam <= 'Z') {
        OnLetterKey((char)wParam);
    } else if (wParam == '\b') {
        OnBackspace();
    } else if (wParam == '\r') {
        if ((lParam & 0x40000000L) == 0) OnSubmit();
    }
}

static void HandleCommand(int id)
{
    switch (id) {
        case IDM_NEWGAME: OnNewGame(); break;
        case IDM_ENDGAME: OnGiveUp(); break;
        case IDM_RANKING: OnStatistics(); break;
        case IDM_EXIT:    DestroyWindow(gWindow); break;
        case IDM_ABOUT:   OnAbout(); break;
        default: break;
    }
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_PAINT:
            OnPaint(hwnd);
            return 0;

        case WM_ERASEBKGND:
            return 1;   /* OnPaint covers the whole client */

        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                UpdateLayout((int)LOWORD(lParam), (int)HIWORD(lParam));
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;

        case WM_GETMINMAXINFO:
            OnGetMinMaxInfo((MINMAXINFO *)lParam);
            return 0;

        case WM_CHAR:
            HandleChar(wParam, lParam);
            return 0;

        case WM_LBUTTONDOWN: {
            POINT pt;
            pt.x = (short)LOWORD(lParam);
            pt.y = (short)HIWORD(lParam);
            HandleContentClick(pt);
            return 0;
        }

        case WM_COMMAND:
            HandleCommand((int)LOWORD(wParam));
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/* ---------------------------------------------------------------------- */
/* Startup                                                                 */
/* ---------------------------------------------------------------------- */

/* The window can't shrink below MIN_SCALE of the design size, where the
 * key labels are still readable. */
static void OnGetMinMaxInfo(MINMAXINFO *info)
{
    RECT frame;

    SetRect(&frame, 0, 0, MulDiv(CLIENT_W, MIN_SCALE, 1000),
            MulDiv(CLIENT_H, MIN_SCALE, 1000));
    AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW, TRUE);
    info->ptMinTrackSize.x = frame.right - frame.left;
    info->ptMinTrackSize.y = frame.bottom - frame.top;
}

static HFONT MakeArialBold(int height)
{
    return CreateFont(height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                      ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, "Arial");
}

static void CreateFonts(void)
{
    gTileFont = MakeArialBold(-ScaleLength(22));
    gKeyFont = MakeArialBold(-ScaleLength(14));
    gWideKeyFont = MakeArialBold(-ScaleLength(11));
}

static void DeleteFonts(void)
{
    if (gTileFont != NULL) DeleteObject(gTileFont);
    if (gKeyFont != NULL) DeleteObject(gKeyFont);
    if (gWideKeyFont != NULL) DeleteObject(gWideKeyFont);
    gTileFont = NULL;
    gKeyFont = NULL;
    gWideKeyFont = NULL;
}

/* Registers the class and creates the resizable main window at its design
 * size (shrunk to fit if the work area is smaller), centered in the work
 * area (the whole screen on NT 3.51, where SPI_GETWORKAREA fails). */
static BOOL CreateMainWindow(int nCmdShow)
{
    WNDCLASS wc;
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT frame, work;
    int x, y, w, h, extraW, extraH, scale, sx, sy;

    /* No CS_HREDRAW/CS_VREDRAW (WM_SIZE invalidates everything itself) and
     * no CS_DBLCLKS, so a fast second click arrives as another
     * WM_LBUTTONDOWN. */
    wc.style = 0;
    wc.lpfnWndProc = MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = gInst;
    wc.hIcon = LoadIcon(gInst, MAKEINTRESOURCE(IDI_APP));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszMenuName = MAKEINTRESOURCE(IDM_MAIN);
    wc.lpszClassName = "iWordleMain";
    if (!RegisterClass(&wc)) return FALSE;

    if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0)) {
        SetRect(&work, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    }

    SetRect(&frame, 0, 0, CLIENT_W, CLIENT_H);
    AdjustWindowRect(&frame, style, TRUE);
    extraW = (frame.right - frame.left) - CLIENT_W;
    extraH = (frame.bottom - frame.top) - CLIENT_H;

    scale = 1000;
    sx = MulDiv((work.right - work.left) - extraW, 1000, CLIENT_W);
    sy = MulDiv((work.bottom - work.top) - extraH, 1000, CLIENT_H);
    if (sx < scale) scale = sx;
    if (sy < scale) scale = sy;
    if (scale < MIN_SCALE) scale = MIN_SCALE;

    w = MulDiv(CLIENT_W, scale, 1000) + extraW;
    h = MulDiv(CLIENT_H, scale, 1000) + extraH;
    x = work.left + ((work.right - work.left) - w) / 2;
    y = work.top + ((work.bottom - work.top) - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    gWindow = CreateWindow("iWordleMain", "iWordle", style, x, y, w, h,
                           NULL, NULL, gInst, NULL);
    if (gWindow == NULL) return FALSE;

    gAccel = LoadAccelerators(gInst, MAKEINTRESOURCE(IDA_MAIN));

    WordleNewGame(&gGame);
    UpdateGameMenuState();
    ShowWindow(gWindow, nCmdShow);
    UpdateWindow(gWindow);
    return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;

    (void)hPrevInstance;
    (void)lpCmdLine;

    gInst = hInstance;

    /* Needed so the XP manifest's comctl32 v6 is activated; harmless on
     * every other Windows. */
    InitCommonControls();

    WordleSeedRandom(GetTickCount());
    BuildStatsPath();
    LoadStats();
    CreateFonts();

    if (!CreateMainWindow(nCmdShow)) {
        DeleteFonts();
        return 0;
    }

    while (GetMessage(&msg, NULL, 0, 0)) {
        if (gAccel == NULL || !TranslateAccelerator(gWindow, gAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    DeleteFonts();
    return (int)msg.wParam;
}
