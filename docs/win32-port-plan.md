# Windows (Win32) port — implementation plan

One **32-bit x86 PE executable** that runs unchanged on **Windows NT 3.51, 95, 98,
ME, NT 4.0, 2000 and XP**. User's test VMs: 95/98/ME, NT4/2000, XP (NT 3.51 is a
supported target but is not on the test rig, so everything NT 3.51-specific must be
right by construction). All builds run on GitHub Actions; nothing is built or tested
locally, ever.

Decisions already made with the user (do not revisit):
- **Toolchain:** Open Watcom **1.9**, run on `ubuntu-latest` via
  `open-watcom/setup-watcom@v0`. Its static CRT doesn't depend on any `msvcrt.dll`.
- **UI:** native Win32 that adapts to each OS: standard menus, dialogs, message
  boxes and system colors. It should look 3.1-style on NT 3.51, 95-style on
  95/NT4/2000, and themed on XP (via a manifest).
- **Stats file:** always next to the EXE.
- **Layout:** same as the other ports: bottom keyboard row
  `[ENTER] Z X C V B N M [BACK]`. Clicking ENTER submits exactly like the Return key.

## 0. Ground rules

- `src/core/` is **not touched**. It is C89-clean already (declarations at block
  top, no `//`, no `stdint`/`stdbool`), which Watcom 1.9 (a C89 compiler) needs.
- No Mac platform directory is touched. `CMakeLists.txt` is not touched either:
  the Windows build invokes Watcom directly from its workflow.
- New front end: `src/platform/win32/` — `main.c`, `iWordle.rc`, `resource.h`,
  `iWordle.manifest`.
- New tool: `tools/mac_icon_to_ico.py` (runs in CI and builds the `.ico` from the
  existing Mac icon family in `src/platform/macos9/icons.r`).
- New workflow: `.github/workflows/build-win32.yml`.
- **C89 only** in `main.c`: declarations at the top of each block, `/* */` comments,
  no `//`, no mixed declarations, no `for (int i…)`, no `inline`, no `long long`.
- Code style: match `src/platform/system7/main.c`. That means the same section
  banners, comment density and naming (`OnNewGame`, `OnSubmit`, `DrawTile`,
  `GetEnterKeyRect`, `RecordGameResult`, `OnStatistics`, `OnAbout`…), `static`
  helpers and a forward-declaration block.
- **ANSI build only.** Never define `UNICODE`/`_UNICODE`; call the `…A` APIs through
  the plain macros. Windows 95 has no working wide-character APIs.

## 1. API compatibility rules (NT 3.51 is the floor)

The EXE may import only **KERNEL32, USER32, GDI32, COMCTL32** (CI enforces this,
§4). Within those DLLs:

**Allowed** (present on NT 3.51 and 95): `RegisterClass`/`WNDCLASS`, `CreateWindow`,
`AdjustWindowRect`, `GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)`, `LoadIcon`,
`LoadCursor`, `LoadMenu`/resource menus, `LoadAccelerators`/`TranslateAccelerator`,
`EnableMenuItem`, `DialogBoxParam`, `EndDialog`, `MessageBox`, `MessageBeep`,
`SendDlgItemMessage`, `SetDlgItemText`/`GetDlgItemText`, `EM_LIMITTEXT`, `EM_SETSEL`,
`LB_ADDSTRING`/`LB_RESETCONTENT`/`LB_SETTABSTOPS`, `wsprintf`, `lstrcpyn`, `lstrlen`,
`GetModuleFileName`, `CreateFile`/`ReadFile`/`WriteFile`/`CloseHandle`,
`GetTickCount`. GDI: `CreateFont`/`CreateFontIndirect`, `CreateSolidBrush`,
`CreatePen`, `FillRect`, `Rectangle`, `MoveToEx`/`LineTo`, `SetTextColor`,
`SetBkMode`, `DrawText`, `CreateCompatibleDC`/`CreateCompatibleBitmap`/`BitBlt`,
`SelectObject`/`DeleteObject`/`DeleteDC`, `GetSysColor`/`GetSysColorBrush`… **except**
note that `GetSysColorBrush` is 95/NT4+, so **don't use it**. Create brushes from
`GetSysColor` instead. `InitCommonControls` (COMCTL32 exists on NT 3.51).

**Forbidden** (95/NT4+ or later, or missing on NT 3.51): `LoadImage`, `DrawEdge`,
`DrawFrameControl`, `DrawState`, `RegisterClassEx`/`WNDCLASSEX`,
`GetSysColorBrush`, `GetMenuItemInfo`/`SetMenuItemInfo`, `CheckMenuRadioItem`,
`TrackMouseEvent`, `SHGetSpecialFolderPath`/any SHELL32, `GetFileAttributesEx`,
`GradientFill`/`AlphaBlend`/`TransparentBlt`, `SetWindowLongPtr`,
`InitCommonControlsEx`. Also forbidden: system colors beyond
the Win 3.x set. Use only `COLOR_BTNFACE`, `COLOR_BTNSHADOW`, `COLOR_BTNHIGHLIGHT`,
`COLOR_BTNTEXT`, `COLOR_WINDOW`, `COLOR_WINDOWTEXT`, `COLOR_WINDOWFRAME`. No
`COLOR_3DLIGHT`/`COLOR_3DDKSHADOW`.

`SystemParametersInfo(SPI_GETWORKAREA, …)` **may** be called but returns FALSE on NT
3.51. Fall back to `SM_CXSCREEN`/`SM_CYSCREEN`.

## 2. Front end: `src/platform/win32/main.c`

### Layout (client area 356 × 398 — must fit a 640×480 Win95 desktop with taskbar)

```
#define MARGIN            10
#define TILE_SIZE         38
#define TILE_GAP          4
#define BOARD_TOP         MARGIN
#define BOARD_LEFT        75          /* (356 - 206) / 2 */
#define KEY_W             30
#define KEY_H             36
#define KEY_GAP           4
#define KEY_ROW_GAP       6
#define KEYBOARD_TOP      268         /* 10 + 248 + 10 */
#define KB_ROW0_LEFT      MARGIN      /* 10 keys = 336 */
#define KB_ROW1_LEFT      27          /* 9 keys = 302, (356 - 302) / 2 */
#define KB_ROW2_LEFT      61          /* letters, after the ENTER key */
#define KB_WIDE_KEY_WIDTH 47          /* ENTER + gap + 7 keys + gap + BACK = 336 */
#define CLIENT_W          356
#define CLIENT_H          398         /* 268 + 3*36 + 2*6 + 10 */
```
Row 2: ENTER at x 10–57, letters Z…M from 61, BACK at 299–346. Same helpers as the
Mac ports: `GetTileRect`, `KeyboardRowLeft`, `GetLetterKeyRect`,
`GetBackspaceKeyRect`, `GetEnterKeyRect`, using the Win32 `RECT` type (exclusive
right/bottom, built with `SetRect`).

### Window
- Class `"iWordleMain"`, `WNDCLASS` with `CS_HREDRAW | CS_VREDRAW` **not** set (fixed
  size) and no `CS_DBLCLKS`, so a fast second click arrives as another
  `WM_LBUTTONDOWN`. `hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1)`. Icon is
  `LoadIcon(hInst, MAKEINTRESOURCE(IDI_APP))`.
- Style `WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX`, which makes it
  non-resizable. Title is `"iWordle"`, and the menu comes from the class/`LoadMenu`
  (`IDM_MAIN`).
- Size: `AdjustWindowRect(&r, style, TRUE)` on `{0,0,CLIENT_W,CLIENT_H}`. Center it in
  the work area (or the full screen on NT 3.51, see §1) and clamp top/left to ≥ 0.

### Painting
- `WM_PAINT`: `BeginPaint`. Render the **whole client** into a memory DC
  (`CreateCompatibleDC` + `CreateCompatibleBitmap(hdc, CLIENT_W, CLIENT_H)`). Fill it
  with a brush made from `GetSysColor(COLOR_BTNFACE)`, draw the board and keyboard,
  `BitBlt` to the window, then free everything and `EndPaint`. This means no flicker
  on any OS. `WM_ERASEBKGND` returns 1.
- Keystroke updates: `InvalidateRect(hwnd, &rowRect, FALSE)` for the typed row only.
  Submit/new game/give up: `InvalidateRect(hwnd, NULL, FALSE)`, then
  **`UpdateWindow(hwnd)` before any `MessageBox`**, so the final colors are on screen
  behind the message.
- Fonts are created once at startup and deleted at exit. All are `"Arial"`, `FW_BOLD`,
  `ANSI_CHARSET`, `VARIABLE_PITCH | FF_SWISS` (Arial is TrueType on every target):
  tile font height `-22`, key font `-14`, wide-key font `-11`.
- **Tile:** fill by state with the CLAUDE.md palette: empty `#FFFFFF`, correct
  `#6AAA64`, present `#C9B458`, absent `#787C7E`. 1px `#333333` frame
  (`Rectangle` with a `#333333` pen and the fill brush). Letter centered with
  `DrawText(DT_CENTER|DT_VCENTER|DT_SINGLELINE)`, `SetBkMode(TRANSPARENT)`. Text is
  white on colored tiles and `#333333` on empty ones.
- **Key:** a native-looking raised bevel built only from Win 3.x system colors, so it
  matches the OS. Draw a 1px `COLOR_WINDOWFRAME` outer frame, then a 1px
  `COLOR_BTNHIGHLIGHT` top/left inner line and a 1px `COLOR_BTNSHADOW` bottom/right
  inner line. The face is `COLOR_BTNFACE` for unused letters, or the palette color for
  correct/present/absent. Label text is `COLOR_BTNTEXT` on the plain face and white on
  palette faces. Letter keys use the key font. ENTER/BACK use the wide-key font with
  labels `"ENTER"` and `"BACK"` (same as the OS 9/OS X ports) and are always the plain
  face. One helper does this: `DrawKeyCell(hdc, &r, face, textColor, label, font)`.
- Brushes and pens are created and deleted per paint (simple and leak-free), or cached
  as globals created at startup. Either is fine, but every `SelectObject` must be
  restored before deleting.

### Input
- `WM_CHAR`: `a`–`z`/`A`–`Z` → `OnLetterKey(toupper)`. `'\b'` → `OnBackspace()`.
  `'\r'` → `OnSubmit()` **only if `(lParam & 0x40000000) == 0`**, so holding Return
  doesn't keep re-submitting. Everything else is ignored.
- `WM_LBUTTONDOWN`: get the point from `LOWORD/HIWORD(lParam)` (cast to `short`) and
  hit-test like `HandleContentClick` in the System 7 port: letter keys, then BACK, then
  ENTER. Act on mouse-down, same as the Mac ports.
- Game actions (`OnLetterKey`, `OnBackspace`, `OnSubmit`, `OnNewGame`, `OnGiveUp`)
  mirror `src/platform/system7/main.c` one-for-one, including messages:
  - too short → `MessageBeep(MB_OK)`;
  - not in dictionary → `MessageBox(hwnd, "Not in word list.", "iWordle", MB_OK | MB_ICONEXCLAMATION)`;
  - win → `"You got it in N guess(es)!"`, `MB_ICONINFORMATION`, then `RecordGameResult(TRUE)`;
  - lose / End Game → `"Out of guesses! The word was XXXXX."`, `MB_ICONINFORMATION`,
    then `RecordGameResult(FALSE)`;
  - call `UpdateGameMenuState()` after every state change.

### Menus and accelerators (Windows games convention, like Minesweeper/Solitaire)
```
&Game
    &New Game        F2      (IDM_NEWGAME; Ctrl+N also maps here)
    &End Game                (IDM_ENDGAME; grayed unless a game is in progress)
    &Ranking...      Ctrl+R  (IDM_RANKING)
    ---
    E&xit                    (IDM_EXIT → DestroyWindow)
&Help
    &About iWordle...        (IDM_ABOUT)
```
Accelerator table `IDA_MAIN`: `VK_F2`, `"^N"`, `"^R"`. The message loop is
`GetMessage` → `TranslateAccelerator(gWindow, gAccel, &msg)` → else
`TranslateMessage` + `DispatchMessage`. Gray/enable End Game with
`EnableMenuItem(GetMenu(hwnd), IDM_ENDGAME, MF_BYCOMMAND | (inProgress ? MF_ENABLED : MF_GRAYED))`.

### Dialogs (all real Dialog Manager resources in `iWordle.rc`)
Every `DIALOG` uses `STYLE DS_MODALFRAME | DS_3DLOOK | WS_POPUP | WS_CAPTION | WS_SYSMENU`
and `FONT 8, "MS Sans Serif"`. `DS_3DLOOK` gives the 3D look on 95/NT4 even though the
EXE is marked subsystem 3.10, and NT 3.51 ignores it. If Watcom 1.9's headers lack
`DS_3DLOOK`, `#define DS_3DLOOK 0x0004L` in `resource.h` under `#ifndef`. Each dialog
centers itself over the main window in `WM_INITDIALOG` (`GetWindowRect` math plus
`SetWindowPos`, no `DS_CENTER`, which is 95+).

- **IDD_NAME "Who's Playing?"**: static `"Who's playing?"`, `EDITTEXT IDC_NAME`
  (`ES_AUTOHSCROLL`), `DEFPUSHBUTTON "OK" IDOK`, `PUSHBUTTON "Cancel" IDCANCEL`. In
  `WM_INITDIALOG`: `EM_LIMITTEXT` to `WORDLE_STATS_NAME_LEN`, prefill with
  `gLastPlayerName`, `EM_SETSEL 0,-1`, `SetFocus` on the edit, return FALSE. OK copies
  the text out. Cancel or a blank name means "don't record", the same as a blank name
  on the Mac ports.
- **IDD_RANKING "Ranking"**: header statics `Name`, `Played`, `Win %`, `Cur`, `Max`
  aligned over a `LISTBOX IDC_LIST` (`LBS_USETABSTOPS | LBS_NOSEL | LBS_NOINTEGRALHEIGHT
  | WS_VSCROLL | WS_BORDER`, about 240×110 DLU). Tab stops are set with
  `LB_SETTABSTOPS` in dialog units, matching the header positions (for example Name 0,
  Played 110, Win % 145, Cur 180, Max 210). Rows are built with `wsprintf("%s\t%u\t%u\t%u\t%u")`,
  where Win% is the integer `won*100/played` (0 if played is 0). With no players there
  is one row: `"No players yet -- win or lose a game to get started."`. Buttons:
  `PUSHBUTTON "&Clear" IDC_CLEAR` bottom-left and `DEFPUSHBUTTON "OK" IDOK`
  bottom-right. Clear runs `WordleStatsClear`, then `SaveStats`, then refills the list.
- **IDD_ABOUT "About iWordle"**: `ICON IDI_APP` centered at the top, then centered
  statics. Two of them are bold: in `WM_INITDIALOG`, take the dialog font (`WM_GETFONT`
  → `GetObject` → `lfWeight = FW_BOLD` → `CreateFontIndirect`), `WM_SETFONT` it onto
  those statics, and delete it on `WM_DESTROY`. Lines:
  **iWordle 1.0** / A native Wordle clone for Windows / **Bruno Castello** /
  bfcastello@hotmail.com / Engineer: Claude Sonnet 5 / © Castello Designs, 2026
  (`\xA9`, cp1252) / Built with Open Watcom. One `DEFPUSHBUTTON "OK" IDOK`, which is
  the Windows About-box convention.

Dialog procs: `BOOL CALLBACK NameDlgProc(HWND, UINT, WPARAM, LPARAM)` etc., invoked
with `DialogBoxParam(gInst, MAKEINTRESOURCE(IDD_…), hwnd, Proc, 0)`. `IDCANCEL`
(Esc / close box) ends every dialog.

### Stats persistence
- Path: `GetModuleFileName(NULL, path, MAX_PATH)`. Cut after the last `'\\'` and
  append `"IWORDLE.DAT"` (8.3 name, safe on FAT without long names).
- Load: `CreateFile(GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING)` and
  `ReadFile(sizeof(WordleStatsBook))`. If it's missing, short, or
  `playerCount > WORDLE_STATS_MAX_PLAYERS`, use `WordleStatsInit`. Always force each
  `name[WORDLE_STATS_NAME_LEN] = '\0'`.
- Save: `CreateFile(GENERIC_WRITE, 0, CREATE_ALWAYS)` and write the flat dump. On
  failure (read-only CD/floppy, limited XP account in Program Files), fail silently,
  like the Mac ports. The file is Windows-only and not shared with the Mac builds.
- `RecordGameResult(won)`: prompt → if non-blank, `WordleStatsRecordResult`, remember
  `gLastPlayerName`, `SaveStats`, `OnStatistics()`. Same as the System 7 port.

### Startup / shutdown
`WinMain`: `InitCommonControls()` (needed so the XP manifest's comctl32 v6 is
activated; harmless elsewhere), `WordleSeedRandom(GetTickCount())`, `LoadStats`,
create fonts, register the class, `LoadAccelerators`, `CreateWindow`, `WordleNewGame`,
`UpdateGameMenuState`, `ShowWindow(nCmdShow)`, `UpdateWindow`, then the message loop.
`WM_DESTROY` → `PostQuitMessage(0)`. Delete fonts after the loop.

Include `<windows.h>` and `<commctrl.h>` (`InitCommonControls`), plus `<string.h>`
and `<ctype.h>`.

## 3. Resources: `iWordle.rc`, `resource.h`, `iWordle.manifest`

- `resource.h`: all `IDI_/IDM_/IDA_/IDD_/IDC_` ids. Included by both `main.c` and the
  `.rc`.
- `iWordle.rc`: `IDI_APP ICON "iWordle.ico"` (generated into `build-win32/` by CI,
  §4), the menu, the accelerator table, the three dialogs, a `VERSIONINFO` (FileVersion
  1,0,0,0; CompanyName "Castello Designs"; FileDescription "iWordle"; ProductName
  "iWordle"; LegalCopyright "\251 Castello Designs, 2026"; OriginalFilename
  "IWORDLE.EXE"), and the manifest: `1 24 "iWordle.manifest"` (RT_MANIFEST = 24, id
  1).
- `iWordle.manifest`: the standard XP manifest with `assemblyIdentity` (`version="1.0.0.0"`,
  `processorArchitecture="X86"`, `name="CastelloDesigns.iWordle"`, `type="win32"`), a
  description, and a dependency on `Microsoft.Windows.Common-Controls` version
  `6.0.0.0`, `processorArchitecture="X86"`, `publicKeyToken="6595b64144ccf1df"`,
  `language="*"`. No `trustInfo` or `compatibility` sections (XP's loader is strict
  about schema it doesn't know). Older OSes ignore it.

## 4. Build plumbing

### `tools/mac_icon_to_ico.py`
Usage: `python3 -I tools/mac_icon_to_ico.py src/platform/macos9/icons.r out.ico`.
Standard library only.
- Parse `data 'TYPE' (128) { $"…" … };` blocks. Concatenate every `$"…"` hex string
  in the block, strip spaces and convert to bytes. Needs `ICN#`, `icl4`, `icl8`,
  `ics#`, `ics4`, `ics8`.
- Emit an ICO with **four** images: 32×32 4bpp (`icl4` + `ICN#` mask), 32×32 8bpp
  (`icl8` + `ICN#` mask), 16×16 4bpp (`ics4` + `ics#` mask), 16×16 8bpp (`ics8` +
  `ics#` mask). **No 32bpp entries**, because 95/NT4 can pick them at true-color
  depths and render them wrong.
- The mask is the second half of `ICN#` (bytes 128–255) and `ics#` (bytes 32–63). Mac
  mask bit 1 = opaque; ICO AND-mask bit 1 = transparent, so **invert**. Where a pixel
  is transparent, write palette index 0 into the XOR bitmap and make palette entry 0
  black (the ICO convention). That means remapping, so build each palette with black
  at index 0 and remap the Mac indices accordingly (simplest: palette = black + the
  distinct Mac colors used).
- Mac palettes:
  - 4-bit (index → RGB): 0 FFFFFF, 1 FCF305, 2 FF6402, 3 DD0806, 4 F20884, 5 4600A5,
    6 0000D4, 7 02ABEA, 8 1FB714, 9 006411, 10 562C05, 11 90713A, 12 C0C0C0,
    13 808080, 14 404040, 15 000000. Pixels are high nibble first.
  - 8-bit: indices 0–214 are the 6×6×6 cube with levels `[FF,CC,99,66,33,00]`, so
    `i = r*36 + g*6 + b`. 215–224 are reds `EE DD BB AA 88 77 55 44 22 11` (g=b=0).
    225–234 are the same ramp for green, 235–244 for blue, 245–254 for gray (r=g=b),
    and 255 is 000000.
- ICO layout: `ICONDIR` (0, 1, count), then 16-byte entries (w, h, colorCount: 16 or
  0 for 256, 0, planes 1, bitCount, bytesInRes, offset), then each image as a
  `BITMAPINFOHEADER` (biHeight = 2×h, biPlanes 1, biBitCount 4/8, biSizeImage = XOR
  + AND bytes, the rest 0), a palette of 16/256 `RGBQUAD`s (BGR0), the XOR rows
  **bottom-up** padded to 4 bytes, then the AND rows bottom-up, 1bpp, padded to 4
  bytes.
- Fail loudly (non-zero exit) if any block is missing or the wrong length (ICN# 256,
  icl4 512, icl8 1024, ics# 64, ics4 128, ics8 256).

### `.github/workflows/build-win32.yml`
Name: `Build (Windows x86 / Win32)`. Triggers on push/PR to `src/core/**`,
`src/platform/win32/**`, `src/platform/macos9/icons.r`, `tools/mac_icon_to_ico.py`,
and the workflow file itself, plus `workflow_dispatch`. Job on `ubuntu-latest`:

1. `actions/checkout@v4`.
2. `open-watcom/setup-watcom@v0` with `version: "1.9"`. If 1.9 turns out not to
   install on the Linux runner, `"2.0"` is the fallback (decided by the lead, not the
   implementer). The import audit in step 6 then matters even more.
3. `sudo apt-get update && sudo apt-get install -y mtools genisoimage binutils-mingw-w64-i686 zip`.
4. Icon + resources:
   ```
   mkdir -p build-win32
   python3 -I tools/mac_icon_to_ico.py src/platform/macos9/icons.r build-win32/iWordle.ico
   cp src/platform/win32/iWordle.manifest build-win32/
   cd build-win32
   wrc -q -r -bt=nt -i=../src/platform/win32 -i="$WATCOM/h" -i="$WATCOM/h/nt" \
       -fo=iWordle.res ../src/platform/win32/iWordle.rc
   ```
   (Run from `build-win32/`, so the `.ico`/`.manifest` file references resolve.)
5. Compile + link (from `build-win32/`):
   ```
   for f in ../src/core/wordle_engine.c ../src/core/wordle_stats.c ../src/platform/win32/main.c; do
     wcc386 -q -bt=nt -3r -ox -w3 -ei -i=../src/core -i=../src/platform/win32 \
            -i="$WATCOM/h" -i="$WATCOM/h/nt" -fo="$(basename "${f%.c}").obj" "$f"
   done
   wlink option quiet system nt_win name IWORDLE.EXE \
         file wordle_engine.obj,wordle_stats.obj,main.obj \
         library comctl32.lib \
         option resource=iWordle.res \
         runtime windows=3.10 \
         option map=IWORDLE.MAP
   ```
   `runtime windows=3.10` is **essential**. NT 3.51 refuses to load images whose
   subsystem version is 4.0.
6. Import audit, which fails the build on violations:
   ```
   i686-w64-mingw32-objdump -p IWORDLE.EXE > imports.txt
   ```
   Print the "DLL Name" lines and every imported function name. Fail if any DLL other
   than KERNEL32/USER32/GDI32/COMCTL32 (case-insensitive) is imported. Fail if any
   name from §1's forbidden list is imported. If
   `src/platform/win32/allowed-imports.txt` exists (`DLL!Function` per line, created by
   the lead after the first green run), fail on any import not in it. Also print the
   subsystem version line (`MajorSubsystemVersion`/`MinorSubsystemVersion`) and fail
   unless it is 3.10.
7. Package (back in the repo root, writing only into `package/`):
   ```
   mkdir -p package
   cp build-win32/IWORDLE.EXE package/
   (cd package && zip -9 iWordle-win32.zip IWORDLE.EXE)
   dd if=/dev/zero of=package/iWordle-floppy.img bs=512 count=2880
   mformat -i package/iWordle-floppy.img -f 1440 -v IWORDLE ::
   mcopy -i package/iWordle-floppy.img package/IWORDLE.EXE ::
   mdir -i package/iWordle-floppy.img ::
   mkdir -p iso && cp package/IWORDLE.EXE iso/
   genisoimage -quiet -J -r -V IWORDLE -o package/iWordle.iso iso
   ```
8. `actions/upload-artifact@v4`, name `iWordle-windows-x86`: `package/IWORDLE.EXE`,
   `package/iWordle-win32.zip`, `package/iWordle-floppy.img`, `package/iWordle.iso`,
   `build-win32/IWORDLE.MAP`, `if-no-files-found: error`.

## 5. Docs

- `README.md`: add Windows as the fourth build (x86, Open Watcom 1.9, one EXE for NT
  3.51–XP and 95/98/ME). Add its artifact list, a short "Testing on Windows"
  section, and a Windows line under Controls (F2 New Game, Ctrl+R Ranking). The
  testing section should cover:
  - mount the ISO (or the floppy) in VirtualBox/86Box/VMware;
  - **copy `IWORDLE.EXE` to the hard disk** before playing, because stats are saved
    next to the EXE and a CD/floppy is read-only;
  - on XP under a limited account, keep it somewhere writable.

  Keep the existing tone and structure.
- `CLAUDE.md`: title/overview say **four** builds. Add a bullet for
  `src/platform/win32/` and a **"Windows specifics"** section summarizing §1 (NT 3.51
  floor, allowed DLLs, forbidden APIs, ANSI only, subsystem 3.10, C89 for Watcom 1.9),
  the 640×480 client-size budget, the stats file next to the EXE, and that dialogs
  are native `DIALOG` resources.

## 6. Work split

- **Agent A** — `src/platform/win32/main.c`, `iWordle.rc`, `resource.h`,
  `iWordle.manifest` (§2, §3).
- **Agent B** — `tools/mac_icon_to_ico.py`, `.github/workflows/build-win32.yml`,
  `README.md`, `CLAUDE.md` (§4, §5).

They don't share files. Neither agent builds anything locally or probes for local
toolchains. Verification is CI only.

## 7. Test checklist (user, in VMs)

95/98/ME, NT4/2000, XP:
1. The EXE launches, the window fits the screen (640×480 on 9x), the icon shows in
   the title bar, taskbar and Explorer.
2. Typing and clicking both work. ENTER submits, BACK deletes, holding Return submits
   only once.
3. Not-in-list, win and lose messages appear over a correctly repainted board.
4. The name prompt → Ranking flow works. Stats survive a restart (EXE on the hard
   disk). Clear empties the table.
5. Game menu: End Game is grayed after a result, and F2 / Ctrl+N / Ctrl+R work. The
   About box looks right.
6. XP: themed buttons in the dialogs (the manifest is active).
