# System 7.x port — implementation plan

Target test rig: **System 7.1 on a MAME-emulated Macintosh SE/30** (68030, built-in
512×342 **1-bit** screen). All builds run on GitHub Actions — nothing is built or
tested locally, ever.

## 0. Ground rules

- `src/core/` is **not touched**. The engine is already portable C99.
- `src/platform/macos9/` and `src/platform/macosx/` are **not touched**.
- New front end lives in `src/platform/system7/` (`main.c`, `iWordle.r`).
- Icons: reuse `src/platform/macos9/icons.r` as-is (System 7 uses ICN#/icl4/icl8/
  ics#/ics4/ics8 + BNDL/FREF from it; the extra 32-bit/icns resources are inert).
- Code style: match `src/platform/macos9/main.c` (section banners, comment density,
  naming like `OnNewGame`/`DrawTile`, `static` helpers, forward declarations).

## 1. Platform facts the code must respect

| Fact | Consequence |
|---|---|
| 68k (68030), not PowerPC | Retro68 **m68k** toolchain: `/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake`. CMake sees `CMAKE_SYSTEM_NAME` = `Retro68`. |
| No Carbon, no Appearance Manager | Classic Toolbox only. No `GetPortBounds`, `SetPortWindowPort`, `GetQDGlobalsScreenBits`, `GetWindowPort`, `ChangeWindowAttributes`, `EnableMenuItem`, `NewControl(... kControlEditTextProc ...)`, `SetKeyboardFocus`, `HandleControlKey`. Use `SetPort(w)`, `w->portRect`, `qd.screenBits.bounds`, `EnableItem`/`DisableItem`, Dialog Manager EditText. |
| Must init the Toolbox by hand | `MaxApplZone(); MoreMasters(); InitGraf(&qd.thePort); InitFonts(); InitWindows(); InitMenus(); TEInit(); InitDialogs(NULL); InitCursor(); FlushEvents(everyEvent, 0);` |
| Screen is 512×342, menu bar 20px, title bar ~19px | Main window content must be ≤ ~300px tall. Layout in §3. |
| SE/30 ROM *has* Color QuickDraw but the screen is 1-bit | **Never** decide color vs. mono from `gestaltQuickdrawVersion` alone. Use color only if Color QD present **and** `(**(**GetMainDevice()).gdPMap).pixelSize >= 4`. Re-evaluate at each full redraw (Monitors CP can change depth). |
| Fonts: Chicago (systemFont, 0), Geneva (3), no Charcoal | Keys/labels Chicago 12 plain; tile letters Geneva 18 bold; small text Geneva 9/10. Never `GetFNum("Charcoal")`. |
| No Application Support folder on 7.1 | Stats file goes in the **Preferences** folder: `FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder, …)`, file name `iWordle Stats` (type `STAT`, creator `WRDL`), no subfolder. Same flat `WordleStatsBook` dump as OS 9. |
| Slow 16 MHz CPU, no double-buffering | Don't erase+redraw the whole window per keystroke. Letter/backspace redraw only the affected tile; submit/new game do a full redraw. |
| Desk accessories are a System 7 citizen requirement | Apple menu = "About iWordle…", separator, `AppendResMenu(appleMenu, 'DRVR')`; items ≥ 3 → `GetMenuItemText` + `OpenDeskAcc`. Add a standard Edit menu (Undo, -, Cut, Copy, Paste, Clear), all routed to `SystemEdit(item - 1)` and otherwise ignored. |

### Interfaces
The workflow runs `interfaces-and-libraries.sh` with the repo's
`third_party/InterfacesAndLibraries` (Apple Universal Interfaces 3.4), exactly like
`build.yml`, so `Folders.h`/`FindFolder`, `Gestalt.h`, `Icons.h` (`PlotIconID`),
`Dialogs.h` etc. are Apple's real 68k declarations. Include headers by their
Universal Interfaces names: `<Quickdraw.h> <Windows.h> <Menus.h> <Fonts.h>
<Dialogs.h> <Events.h> <ToolUtils.h> <TextUtils.h> <OSUtils.h> <Sound.h>
<Icons.h> <AppleEvents.h> <Files.h> <Folders.h> <Gestalt.h> <Devices.h>
<Resources.h> <TextEdit.h> <Memory.h>`. UPPs: use `NewUserItemUPP` / `NewModalFilterUPP`
/ `NewAEEventHandlerUPP` (UI 3.4 defines these for 68k too); create each UPP **once**
(static), not per call.

## 2. Build plumbing (Agent A)

### `CMakeLists.txt`
Branch on `CMAKE_SYSTEM_NAME`:
- `Retro68` → sources `wordle_engine.c wordle_stats.c src/platform/system7/main.c
  src/platform/system7/iWordle.r src/platform/macos9/icons.r`.
- anything else → today's macos9 source list, unchanged.
Keep `TYPE "APPL" CREATOR "WRDL"`, include dir, and the gc-sections flags for both.

### `.github/workflows/build-system7.yml` (new)
Name: `Build (System 7 / 68k)`. Triggers on push/PR to `src/core/**`,
`src/platform/system7/**`, `src/platform/macos9/icons.r`, `CMakeLists.txt`,
`third_party/InterfacesAndLibraries/**`, the workflow file; plus `workflow_dispatch`.

Steps:
1. Checkout.
2. Docker `ghcr.io/autc04/retro68`: run `interfaces-and-libraries.sh` (same line as
   build.yml), then `mkdir -p build-68k && cd build-68k && cmake .. -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake && cmake --build . --verbose && ls -la`.
3. Host step: `sudo apt-get update && sudo apt-get install -y hfsutils`, then build a
   **1.44 MB raw HFS floppy** for MAME's SE/30 SuperDrive:
   ```
   mkdir -p package
   dd if=/dev/zero of=package/iWordle-floppy.img bs=512 count=2880
   hformat -l iWordle package/iWordle-floppy.img
   hcopy -m build-68k/iWordle.bin :iWordle
   hls -l
   humount
   ```
   (`build-68k/` is root-owned from Docker; only write into `package/`.)
4. Upload artifact `iWordle-system-7-68k`: `package/iWordle-floppy.img`,
   `build-68k/iWordle.bin`, `build-68k/iWordle.dsk`, `build-68k/iWordle.APPL`,
   `if-no-files-found: error`.

`build.yml` (OS 9) also triggers on `CMakeLists.txt`, so it re-verifies the OS 9 path.

### Docs
- `README.md`: add System 7 as a third build (68k, Retro68 m68k), its artifact list,
  and a "Testing in MAME (SE/30)" section: `mame se30 -hard1 <your System 7.1 disk>.chd -flop1 iWordle-floppy.img`,
  drag iWordle to the hard disk. Keep the existing tone.
- `CLAUDE.md`: Project Overview gets a third bullet for `src/platform/system7/`; add a
  "System 7 specifics" section with the §1 table's rules in prose (68k, classic
  Toolbox, 1-bit 512×342, the SE/30 Color-QD-but-1-bit gotcha, Preferences folder,
  DLOG/DITL + ModalDialog is the *right* tool here unlike OS X, mono palette in §4).
- `PROJECT.md`: add a System 7 column to the §4 table and mention it in §1/§2.

## 3. Main window layout (Agent B)

`WIND 128`: `noGrowDocProc`, `goAway`, title "iWordle", **explicit bounds**
`{ 40, 110, 326, 402 }` (content 292 wide × 286 tall — fits 512×342 with title bar
and menu bar), `visible`, not `centerMainScreen`. White background (System 7 window
default — use `EraseRect(&w->portRect)`).

```
#define MARGIN        8
#define TILE_SIZE     28
#define TILE_GAP      4
#define BOARD_TOP     MARGIN            /* board 156 x 188 */
#define BOARD_LEFT    68                /* (292 - 156) / 2 */
#define KEY_W         24
#define KEY_H         22
#define KEY_GAP       4
#define KEY_ROW_GAP   4
#define KEYBOARD_TOP  204               /* 8 + 188 + 8 */
#define KB_ROW0_LEFT  MARGIN            /* 10 keys = 276 */
#define KB_ROW1_LEFT  22                /* 9 keys  = 248 */
#define KB_ROW2_LEFT  22                /* 7 keys + gap + BACK = 248 */
#define KB_BACKSPACE_WIDTH 52
```
Bottom of keyboard = 204 + 3·22 + 2·4 = 278, + MARGIN = 286. Backspace key sits after
the 7th letter key of row 2 (same as OS 9), label "Delete" if it fits in Chicago 12
inside 52px, else "Del".

## 4. Rendering

Two palettes behind one function `static Boolean UseColor(void)` (rule in §1).

**Color path** (depth ≥ 4): the CLAUDE.md palette exactly (`#6AAA64`, `#C9B458`,
`#787C7E`, white tiles, `#333333` borders) via `RGBForeColor`, same as OS 9.

**Monochrome path** (SE/30) — classic 1-bit idioms only:

| State | Tile / key |
|---|---|
| Empty | White fill, 1px black frame. Tiles in the current row that hold a typed letter get a **2px** frame (`PenSize(2,2)`). Black letter. |
| Correct | Solid black fill (`PaintRect`), letter drawn white with `TextMode(srcBic)`. |
| Present | `FillRect(&r, &qd.gray)`, white oval badge (`InsetRect` 4px tile / 2px key, `EraseOval`), black letter centered on it, 1px black frame. |
| Absent | White fill, black letter, then **dimmed** the System 7 way: `PenPat(&qd.gray); PenMode(patBic); PaintRect(&inner); PenNormal();`, 1px black frame left undimmed. |

Keys: same states; default key face is white with a 1px black frame plus a 1px
drop shadow on the right and bottom edges (classic System 7 button feel). Always
restore `TextMode(srcOr)`, `PenNormal()`, `ForeColor(blackColor)`/`BackColor(whiteColor)`
after drawing.

Text centering: `StringWidth` for x; for y use `GetFontInfo` (`ascent`) to center
vertically instead of magic offsets.

Incremental drawing: `OnLetterKey` / `OnBackspace` redraw only the one tile that
changed (plus its row-mates if frame weight depends on the row — just redraw the
current row's 5 tiles). Full `RedrawAll()` after submit, new game, and on update
events. Update handling: `BeginUpdate` / draw / `EndUpdate` with `SetPort` saved/
restored via `GetPort`.

## 5. Dialogs — classic Dialog Manager is correct here

The OS X "no DLOG/DITL" rule is about Carbon on OS X; on System 7 the Dialog
Manager is the native, correct tool. Use it.

- **Message** (`ALRT 200` + `DITL 200`): item 1 OK button, item 2 StaticText `^0`.
  `ParamText(msg, "\p", "\p", "\p"); NoteAlert(200, NULL);` — the Alert Manager
  draws the default-button outline and handles Return/Enter itself. Used for
  "Not in word list.", win, lose. Size it ~ `{0,0,110,300}` with stage 4-entry `ALRT`
  format (`OK, visible, silent` ×4), position `alertPositionMainScreen`.
- **Name prompt** (`DLOG 202` + `DITL 202`, `dBoxProc`, `{0,0,120,280}`,
  `alertPositionMainScreen`): 1 OK button, 2 StaticText "Who's playing?",
  3 EditText (prefilled with the last name via `SetDialogItemText` +
  `SelectDialogItemText(dlg, 3, 0, 32767)`), 4 UserItem drawing the default-button
  ring (`PenSize(3,3)`, `InsetRect(-4,-4)`, `FrameRoundRect(16,16)` around item 1).
  `ModalDialog(NULL, &item)` until item 1 — Return/Enter map to item 1 automatically.
  Read with `GetDialogItemText`, truncate to `WORDLE_STATS_NAME_LEN`. Blank → don't
  record (same semantics as OS 9).
- **Ranking** (`DLOG 203` + `DITL 203`, `dBoxProc`, ~`{0,0,250,400}`, fits 342
  tall): 1 OK, 2 Clear, 3 UserItem table, 4 UserItem default ring around item 1.
  Table: Geneva 10, header bold (Name / Played / Win % / Cur / Max), 14px rows, up to
  10 players, empty-state line. Clear: `WordleStatsClear`, `SaveStats`,
  `EraseRect` + `InvalRect` the table item's box so the next update redraws it.
- **About** (`WIND 201`, `noGrowDocProc`, `goAway`, invisible, ~`{0,0,220,260}`
  centered by code using `qd.screenBits.bounds`): same content and same small
  `WaitNextEvent` loop as OS 9's `OnAbout()` — dismiss on close box, Return, Enter,
  Escape, or ⌘W/⌘. — `PlotIconID(&r, atNone, ttNone, 128)` for the icon; Chicago 12
  for the emphasis lines and Geneva 10 for the rest; text identical to OS 9 except
  the subtitle reads "A native Wordle clone for System 7". Ignore clicks outside it
  (beep) while open.

## 6. Menus & events

- `MBAR 128` = { 1 Apple, 128 File, 129 Edit }. File menu identical to OS 9
  (New Game ⌘N, End Game ⌘W, Ranking… ⌘R, -, Quit ⌘Q). `UpdateFileMenuState()` uses
  `EnableItem`/`DisableItem`.
- Event loop: `WaitNextEvent(everyEvent, &e, 30, NULL)`; handle `mouseDown`
  (`inMenuBar`, `inSysWindow` → `SystemClick`, `inDrag` with `qd.screenBits.bounds`,
  `inGoAway` → quit, `inContent`), `keyDown`/`autoKey` (⌘ → `MenuKey`), `updateEvt`,
  `activateEvt` (no-op is fine), `osEvt` suspend/resume (no-op is fine but don't
  crash), `kHighLevelEvent` → `AEProcessAppleEvent`.
- Apple Events: same four handlers as OS 9 (quit sets `gDone`). Only install them
  if `Gestalt(gestaltAppleEventsAttr, &r) == noErr && (r & (1L << gestaltAppleEventsPresent))`.
- Startup guard: if `Gestalt(gestaltSystemVersion, &v) != noErr || v < 0x0700`,
  `StopAlert` an `ALRT 129` ("iWordle requires System 7 or later.") and `ExitToShell()`.

## 7. Resources (`src/platform/system7/iWordle.r`)

`#include "Types.r" "Windows.r" "Menus.r" "Dialogs.r" "Processes.r"` (+ `"SysTypes.r"`
for `vers`). Contains: MBAR/MENUs, WIND 128, WIND 201, ALRT/DITL 200, ALRT/DITL 129,
DLOG/DITL 202, DLOG/DITL 203, `vers (1)` "1.0", `SIZE (-1)` with
`isHighLevelEventAware`, `is32BitCompatible`, **768 K preferred / 512 K minimum**
(the word lists are ~150 KB of static data on 68k). A header comment, like the OS 9
`.r`, explaining why classic DLOG/DITL is used here.

## 8. Out of scope (note, don't do)
- Moving word lists to `char[][6]` to save ~84 KB of pointers on 68k (touches core).
- System 6 support.

## 9. Review checklist
1. No Carbon-only or Appearance-only calls in `system7/main.c` (list in §1).
2. Every `SetPort` change is restored; every PenMode/TextMode/PenPat restored.
3. Mono path legible: Correct/Present/Absent/Empty distinguishable on tiles **and** keys.
4. Window and all dialogs fit 512×342 with menu bar.
5. Stats path uses Preferences folder; handles FindFolder failure gracefully.
6. DA support: Apple menu DRVRs, `SystemClick`, `SystemEdit`.
7. Workflow: correct toolchain path, artifacts exist, `hcopy -m` from `.bin`.
8. OS 9 build path in CMakeLists unchanged in behavior.
