# CLAUDE.md - Wordle for Mac OS 9 (Retro68)

## Project Overview
This repository contains a native Classic Mac OS / Carbon Wordle clone built for PowerPC hardware, compiled using **Retro68** via GitHub Actions, and styled with the authentic **Mac OS 9.2 Platinum Theme**.

## Build & Test Commands
* **Configure CMake:** `mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release`
* **Compile Binary:** `cd build && make`
* **Output Artifact:** Retro68 generates a Mac application binary with standard resource forks (`APPL` type, creator `WRDL`).

## Technical Guidelines & Constraints
1. **Target Environment:** Classic Mac OS / Carbon targeting PowerPC (G3/G4) architectures using the Retro68 toolchain.
2. **UI & Theme:** Follow Mac OS 9 Platinum guidelines strictly. Use crisp monochrome bevels (`FrameRect`, `PaintRect`), Geneva typography (`TextFont(geneva)`), and native window borders (`documentProc`).
3. **Color Palette:** 
   * Active Tile Background: Crisp white (`#FFFFFF`) with dark gray inset borders (`#333333`).
   * Correct Letter: Forest Green (`#6AAA64`).
   * Misplaced Letter: Golden Yellow (`#C9B458`).
   * Absent Letter: Dark Slate Gray (`#787C7E`).
4. **Event Loop:** Handle window dragging, updating (`updateEvt`), and keyboard/mouse clicks using low-level Mac OS Event Manager routines (`WaitNextEvent`, `keyDownEvt`).
5. **Code Style:** Standard C99 compatible with Mac Toolbox headers (`<Quickdraw.h>`, `<Windows.h>`, `<Controls.h>`, `<Events.h>`). Avoid modern standard libraries that require heavy runtime environments.
