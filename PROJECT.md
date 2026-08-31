# Native Wordle Clone for Classic Mac OS
**Software Architecture & Technical Specification (as built)**

---

## 1. Executive Summary & Design Paradigm

This specification describes the as-built architecture for a **native Wordle clone** shipping on both **Mac OS 9** and **Mac OS X 10.0–10.5**, for PowerPC hardware. Both builds implement the classic 6×5 letter grid, color-coded feedback logic, and virtual on-screen keyboard, sharing one portable game-rules engine and diverging only in their platform front end.

Mac OS 9 uses **QuickDraw**, the classic **Event Manager**, and Platinum-styled chrome throughout. Mac OS X uses the same Carbon Toolbox surface but against a real Apple SDK, with genuine Aqua chrome and every UI element built from real native controls — no hand-drawn substitutes anywhere in that build (see `CLAUDE.md` for why that's a hard rule, not a preference).

---

## 2. System Architecture

```
┌─────────────────────────────────────────────────────────┐
│ 🍎  iWordle  File                          Mon 1:00 AM  │
├─────────────────────────────────────────────────────────┤
│                     [ W ] [ O ] [ R ] [ D ] [ E ]        │
│                     [ . ] [ . ] [ . ] [ . ] [ . ]        │
│                     [ . ] [ . ] [ . ] [ . ] [ . ]        │
│                     [ . ] [ . ] [ . ] [ . ] [ . ]        │
│                     [ . ] [ . ] [ . ] [ . ] [ . ]        │
│                     [ . ] [ . ] [ . ] [ . ] [ . ]        │
│                                                           │
│   Q W E R T Y U I O P     (Green: Correct Spot)          │
│    A S D F G H J K L      (Yellow: Wrong Spot)           │
│     Z X C V B N M  [<-]   (Gray: Not in Word)            │
└─────────────────────────────────────────────────────────┘
```

`src/core/` holds the entire game engine — board state, guess evaluation, dictionary, RNG, per-player statistics — as portable C99 with zero Mac Toolbox/Carbon dependencies. `src/platform/macos9/` and `src/platform/macosx/` each implement the SAME UI (window/menu setup, event loop, tile/keyboard rendering, About/message/Ranking/name-prompt windows) against their own toolchain and SDK, calling into `src/core/` for every rule of the game itself. Neither front end embeds any game logic of its own.

> **Core Subsystems**
> * **QuickDraw Grid Renderer:** draws the 6×5 matrix with `PaintRect`/`FrameRect`, filling tile backgrounds with the project's `RGBColor` palette for correct (green), present (yellow), and absent (gray) letters.
> * **Event Manager loop:** `WaitNextEvent`-driven, handling both physical keyboard input (`keyDownEvt`) and clicks on the virtual on-screen keyboard, plus every window's own update/activate/mouse events.
> * **Dictionary Engine:** the valid-guess and answer word lists are compiled directly into `src/core/` as C arrays — no resource-fork or disk-based dictionary lookups, so validation is a simple in-memory search.
> * **Native windows throughout:** on Mac OS X specifically, every dialog (About, message box, name prompt, Ranking) is a plain `WIND` built entirely from real Carbon controls at runtime, not a `DLOG`/`DITL` resource — see `CLAUDE.md`'s "no hand-drawn UI" rule.

---

## 3. Core Features

### A. The 6×5 Letter Matrix
Each cell is a fixed-size square with a solid border, filled on submission with the standard Wordle color feedback: Forest Green (`#6AAA64`) correct, Golden Yellow (`#C9B458`) present, Dark Slate Gray (`#787C7E`) absent.

### B. Dual Input Handler
Physical keyboard input (letters, Return, Delete) and clicks on the on-screen QWERTY keyboard both drive the same guess-entry state machine; the on-screen keys recolor live as letters are verified.

### C. Word Validation
Guesses are checked against an in-memory word list compiled into `src/core/`; no disk or resource-fork lookup is needed at guess time.

### D. Per-player statistics
Every result (name, win/loss, streak) is recorded to a small on-disk stats file and shown in the Ranking window (File > Ranking, ⌘R), which also opens automatically right after a result is saved. Statistics persist across launches and can be cleared from the Ranking window.

### E. About window
Both platforms ship a standard About window matching their era's convention (a real title bar with a close box, no OK button — SimpleText's About Box on Mac OS 9, the equivalent native pattern on Mac OS X) showing the app icon, name/version, author, and credits.

---

## 4. Technical Stack & API Mapping

| Component | Mac OS 9 | Mac OS X |
| :--- | :--- | :--- |
| **Toolchain** | Retro68 (Carbon PowerPC target) | `powerpc-apple-darwin8-gcc` + Apple's real `MacOSX10.4u.sdk` |
| **Executable format** | PEF/CFM (`APPL`) | Mach-O `.app` bundle |
| **Graphics & Rendering** | QuickDraw (`RGBForeColor`, `PaintRect`, `DrawString`) | Same QuickDraw calls for game-board tiles; every dialog/window built from real Carbon controls instead |
| **Event Handling** | Mac OS Event Manager (`WaitNextEvent`, `keyDownEvt`) | Same |
| **UI Controls & Dialogs** | Dialog Manager (`DLOG`/`DITL`) for message box/stats; plain `WIND` + Control Manager for the name prompt | Plain `WIND` + Control Manager throughout — no `DLOG`/`DITL` anywhere in this build |
| **Storage** | `FSSpec`-based File Manager I/O for the stats file | Same File Manager API, same on-disk format |
| **Distribution** | `.sit` (StuffIt) and `.dsk` (raw HFS disk image) | `.dmg` and a zipped `.app` bundle, built with `libdmg-hfsplus` (no real Mac needed) |
| **CI** | `.github/workflows/build.yml`, `ghcr.io/autc04/retro68` Docker image | `.github/workflows/build-macosx.yml`, Retro68 borrowed only for its `Rez` tool |

---

## 5. Status

Both builds are feature-complete and released — see the repo's [Releases](../../releases) page for downloads. Ongoing/future work, if any, should be tracked as issues rather than in this document.
