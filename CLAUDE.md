# CLAUDE.md - iWordle (Retro68 / Carbon, Mac OS 9 + Mac OS X)

## Project Overview
This repository contains a native Classic Mac OS / Carbon Wordle clone built for PowerPC hardware, shipped as **two separate builds**: Mac OS 9 (via Retro68) and Mac OS X 10.0–10.5 (via a real Apple SDK cross-compiler). They cannot be combined into one "fat" binary — Mac OS 9 uses PEF/CFM, Mac OS X uses Mach-O, different executable container formats regardless of both targeting PowerPC.

- `src/core/` — the portable C99 game engine, shared by both platforms, with no Mac Toolbox/Carbon headers at all.
- `src/platform/macos9/` — Mac OS 9 front end, built via `.github/workflows/build.yml` inside the `ghcr.io/autc04/retro68` Docker image, styled with the authentic **Mac OS 9.2 Platinum Theme**.
- `src/platform/macosx/` — Mac OS X front end, built via `.github/workflows/build-macosx.yml` with a `powerpc-apple-darwin8-gcc` cross-compiler against Apple's real `MacOSX10.4u.sdk`, styled with genuine Aqua chrome. Retro68 is only used there for its `Rez` tool (resource compilation), not the compiler/runtime.

## Build & Test Commands
Nothing is built locally — see the two GitHub Actions workflows above. `mkdir build && cd build && cmake .. && make` (Mac OS 9, inside the Retro68 Docker image) is what CI runs; the Mac OS X build needs Apple's non-redistributable SDK, so it only runs in CI.

## Technical Guidelines & Constraints (both platforms)
1. **Target Environment:** Classic Mac OS / Carbon targeting PowerPC (G3/G4) architectures — Mac OS 9.x via Retro68, Mac OS X 10.0–10.5 via a real Apple SDK.
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
