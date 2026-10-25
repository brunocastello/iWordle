# CLAUDE.md - iWordle (Retro68 / Carbon, Mac OS 9 + Mac OS X + System 7)

## Project Overview
This repository contains a native Classic Mac OS / Carbon Wordle clone built for PowerPC and 68k hardware, shipped as **three separate builds**: Mac OS 9 (via Retro68), Mac OS X 10.0–10.5 (via a real Apple SDK cross-compiler), and System 7.x on 68k (via Retro68's m68k toolchain). They cannot be combined into one "fat" binary — Mac OS 9 uses PEF/CFM, Mac OS X uses Mach-O, and System 7 is a 68k application, different executable container formats (and, for System 7, a different CPU).

- `src/core/` — the portable C99 game engine, shared by all platforms, with no Mac Toolbox/Carbon headers at all.
- `src/platform/macos9/` — Mac OS 9 front end, built via `.github/workflows/build.yml` inside the `ghcr.io/autc04/retro68` Docker image, styled with the authentic **Mac OS 9.2 Platinum Theme**.
- `src/platform/system7/` — System 7 front end for 68k Macs, built via `.github/workflows/build-system7.yml` inside the same `ghcr.io/autc04/retro68` Docker image but with the m68k toolchain (`CMakeLists.txt` branches on `CMAKE_SYSTEM_NAME`: `Retro68` selects this front end, anything else the Mac OS 9 one). Reuses `src/platform/macos9/icons.r`. Test rig: System 7.1 on a MAME-emulated Macintosh SE/30.
- `src/platform/macosx/` — Mac OS X front end, built via `.github/workflows/build-macosx.yml` with a `powerpc-apple-darwin8-gcc` cross-compiler against Apple's real `MacOSX10.4u.sdk`, styled with genuine Aqua chrome. Retro68 is only used there for its `Rez` tool (resource compilation), not the compiler/runtime.

## Build & Test Commands
Nothing is built locally — see the three GitHub Actions workflows above. `mkdir build && cd build && cmake .. && make` (Mac OS 9, inside the Retro68 Docker image) is what CI runs (System 7 does the same with the m68k toolchain file); the Mac OS X build needs Apple's non-redistributable SDK, so it only runs in CI.

## Technical Guidelines & Constraints (all platforms)
1. **Target Environment:** Classic Mac OS / Carbon targeting PowerPC (G3/G4) architectures — Mac OS 9.x via Retro68, Mac OS X 10.0–10.5 via a real Apple SDK. (System 7 is the exception: 68k, classic Toolbox — see below.)
2. **Color Palette:**
   * Active Tile Background: Crisp white (`#FFFFFF`) with dark gray inset borders (`#333333`).
   * Correct Letter: Forest Green (`#6AAA64`).
   * Misplaced Letter: Golden Yellow (`#C9B458`).
   * Absent Letter: Dark Slate Gray (`#787C7E`).
3. **Event Loop:** Handle window dragging, updating (`updateEvt`), and keyboard/mouse clicks using low-level Mac OS Event Manager routines (`WaitNextEvent`, `keyDownEvt`).
4. **Code Style:** Standard C99 compatible with Mac Toolbox headers (`<Quickdraw.h>`, `<Windows.h>`, `<Controls.h>`, `<Events.h>`). Avoid modern standard libraries that require heavy runtime environments.

## Mac OS 9 specifics
Follow Mac OS 9 Platinum guidelines strictly. Use crisp monochrome bevels (`FrameRect`, `PaintRect`), Geneva typography (`TextFont(geneva)`), and native window borders (`documentProc`/`noGrowDocProc`).

## Mac OS X specifics — no hand-drawn UI, ever
**Standing rule, learned the hard way:** every window and dialog must be built from real native Carbon controls — `CreateStaticTextControl`, `CreateEditUnicodeTextControl`, native buttons, `SetThemeWindowBackground` for backgrounds — never `DrawString`, `DrawThemeTextBox`, raw ATSUI, or any hand-painted background. This isn't a style preference: an extended debugging arc (see git log around the About/Ranking/message-box UI work) tried every hand-drawn approach and each one rendered subtly wrong (wrong font, missing antialiasing, wrong weight) in ways specific to this cross-compiled toolchain — only genuine native controls rendered correctly, since the OS does the rendering, not this code. If you're tempted to hand-draw text or a background here, use a native control instead.

A few non-obvious things this codebase learned about that toolchain, in case they come up again:
- `GetFNum()`-by-name font resolution and `DrawString()` don't reliably render right — go through `CreateStaticTextControl`/`UseThemeFont()` instead.
- `GetNewDialog()`/`ModalDialog()`'s window doesn't respect `SetThemeWindowBackground()` — use a plain `WIND` (`GetNewCWindow` + a small `WaitNextEvent` loop) for anything that needs a correct native background, not a DLOG/DITL dialog.
- `DisposeControl()` doesn't reliably remove a control from its window — when rebuilding dynamic content (e.g. a table after a "Clear" action), tear down and recreate the whole window rather than trying to dispose/recreate individual controls in place.
- `ChangeWindowAttributes(win, 0, kWindowResizableAttribute)` removes functional resizing but a `documentProc` window's frame still renders grow-box chrome regardless — use `noGrowDocProc` at the resource level for any window that should never be resizable.
- Classic `icns` `it32`/`ih32`/`il32`/`is32` icon RGB data must be PackBits-compressed, not raw, or real Icon Services renders it as garbage.

## System 7 specifics
**Target:** 68k (68030), not PowerPC, built with Retro68's m68k toolchain (`/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake`). Guidelines 1 and 4's PowerPC/Carbon wording does not apply here — this is the classic Toolbox only, with no Carbon and no Appearance Manager. Do not use `GetPortBounds`, `SetPortWindowPort`, `GetQDGlobalsScreenBits`, `GetWindowPort`, `ChangeWindowAttributes`, `EnableMenuItem`, `kControlEditTextProc` controls, `SetKeyboardFocus` or `HandleControlKey`; use `SetPort(w)`, `w->portRect`, `qd.screenBits.bounds`, `EnableItem`/`DisableItem` and Dialog Manager EditText instead. The Toolbox must be initialized by hand (`MaxApplZone`, `MoreMasters`, `InitGraf`, `InitFonts`, `InitWindows`, `InitMenus`, `TEInit`, `InitDialogs`, `InitCursor`).

- **Screen:** the test machine's built-in display is 512×342 and **1-bit**, with a 20px menu bar, so every window and dialog must fit that. The main window is about 304×286 (the bottom keyboard row, Enter + 7 letters + Delete, sets the width).
- **Color vs. mono gotcha:** the SE/30 ROM *has* Color QuickDraw but its screen is 1-bit, so never choose color from `gestaltQuickdrawVersion` alone. Use the color palette only if Color QD is present **and** the main device's pixel depth is at least 4, and re-check at each full redraw. The monochrome path uses classic 1-bit idioms: black fill for correct, gray pattern plus a white badge for present, and a gray-dimmed tile for absent.
- **Fonts:** Chicago (system font) and Geneva only — no Charcoal.
- **Stats file:** lives in the **Preferences** folder via `FindFolder` (there is no Application Support folder on System 7.1), flat, named `iWordle Stats`, type `STAT`, creator `WRDL`, same `WordleStatsBook` dump as the other builds.
- **Performance:** the CPU is slow (16 MHz) with no double buffering, so redraw only the affected tiles on a keystroke and do full redraws only on submit, new game and update events.
- **Dialogs:** unlike Mac OS X, the classic Dialog Manager (`DLOG`/`DITL` + `ModalDialog`, `ALRT` + `NoteAlert`) is the *right*, native tool on System 7 — the OS X "no DLOG/DITL" rule does not apply here.
- **Desk accessories:** the Apple menu lists the system's `DRVR` resources (`AppendResMenu` + `OpenDeskAcc`), clicks in system windows go to `SystemClick`, and a standard Edit menu is routed to `SystemEdit`.
- **Palette:** the CLAUDE.md color palette applies on color displays only; monochrome displays use the 1-bit scheme above.
