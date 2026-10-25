# Native Wordle Clone for Classic Mac OS
**Software Architecture & Technical Specification (as built)**

---

## 1. Executive Summary & Design Paradigm

This specification describes the as-built architecture for a **native Wordle clone** shipping on **Mac OS 9**, **Mac OS X 10.0–10.5** (both PowerPC) and **System 7.x** (68k). All three builds implement the classic 6×5 letter grid, color-coded feedback logic, and virtual on-screen keyboard, sharing one portable game-rules engine and diverging only in their platform front end.

Mac OS 9 uses **QuickDraw**, the classic **Event Manager**, and Platinum-styled chrome throughout. Mac OS X uses the same Carbon Toolbox surface but against a real Apple SDK, with genuine Aqua chrome and every UI element built from real native controls — no hand-drawn substitutes anywhere in that build (see `CLAUDE.md` for why that's a hard rule, not a preference). System 7 uses the original, pre-Carbon Toolbox on 68k Macs (tested on a System 7.1 Macintosh SE/30 with a 1-bit 512×342 screen), with the classic Dialog Manager for its dialogs and a monochrome rendering path alongside the color one.

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

`src/core/` holds the entire game engine — board state, guess evaluation, dictionary, RNG, per-player statistics — as portable C99 with zero Mac Toolbox/Carbon dependencies. `src/platform/macos9/`, `src/platform/macosx/` and `src/platform/system7/` each implement the SAME UI (window/menu setup, event loop, tile/keyboard rendering, About/message/Ranking/name-prompt windows) against their own toolchain and SDK, calling into `src/core/` for every rule of the game itself. Neither front end embeds any game logic of its own.

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
All platforms ship a standard About window matching their era's convention (a real title bar with a close box, no OK button — SimpleText's About Box on Mac OS 9, TeachText-era About windows on System 7, the equivalent native pattern on Mac OS X) showing the app icon, name/version, author, and credits.

---

## 4. Technical Stack & API Mapping

| Component | Mac OS 9 | Mac OS X | System 7 |
| :--- | :--- | :--- | :--- |
| **Toolchain** | Retro68 (Carbon PowerPC target) | `powerpc-apple-darwin8-gcc` + Apple's real `MacOSX10.4u.sdk` | Retro68 (classic m68k target) |
| **Executable format** | PEF/CFM (`APPL`) | Mach-O `.app` bundle | 68k `CODE` resources (`APPL`) |
| **Graphics & Rendering** | QuickDraw (`RGBForeColor`, `PaintRect`, `DrawString`) | Same QuickDraw calls for game-board tiles; every dialog/window built from real Carbon controls instead | Classic QuickDraw; color palette on color displays, 1-bit black/white/gray patterns on monochrome ones (`RGBForeColor` vs. `PaintRect`/`FillRect` with `qd.gray`) |
| **Event Handling** | Mac OS Event Manager (`WaitNextEvent`, `keyDownEvt`) | Same | Same |
| **UI Controls & Dialogs** | Dialog Manager (`DLOG`/`DITL`) for message box/stats; plain `WIND` + Control Manager for the name prompt | Plain `WIND` + Control Manager throughout — no `DLOG`/`DITL` anywhere in this build | Dialog Manager (`DLOG`/`DITL`/`ALRT`, `ModalDialog`) throughout, with a plain `WIND` for About |
| **Storage** | `FSSpec`-based File Manager I/O for the stats file | Same File Manager API, same on-disk format | Same format, stored in the Preferences folder (`FindFolder`) |
| **Distribution** | `.sit` (StuffIt) and `.dsk` (raw HFS disk image) | `.dmg` and a zipped `.app` bundle, built with `libdmg-hfsplus` (no real Mac needed) | 1.44 MB HFS floppy image (built with `hfsutils`), plus `.dsk`, `.bin` and `.APPL` |
| **CI** | `.github/workflows/build.yml`, `ghcr.io/autc04/retro68` Docker image | `.github/workflows/build-macosx.yml`, Retro68 borrowed only for its `Rez` tool | `.github/workflows/build-system7.yml`, `ghcr.io/autc04/retro68` Docker image (m68k toolchain) |

---

## 5. Status

The Mac OS 9 and Mac OS X builds are feature-complete and released — see the repo's [Releases](../../releases) page for downloads. The System 7 build is in progress (CI-built, tested on a MAME SE/30). Ongoing/future work, if any, should be tracked as issues rather than in this document.
