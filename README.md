# iWordle

A native Wordle clone for classic Mac OS and classic Windows, built for
PowerPC, 68k and x86 hardware and styled after the Mac OS 9.2 Platinum theme
(Mac OS 9) / genuine Aqua chrome (Mac OS X) / classic 1-bit System 7 chrome
(System 7) / the stock look of whichever Windows it runs on (Windows). Four
independent builds ship from this one repo:

- **Mac OS 9** (`src/platform/macos9/`) — built with the
  [Retro68](https://github.com/autc04/Retro68) cross-toolchain, targeting
  Carbon on Mac OS 9.x PowerPC hardware.
- **Mac OS X 10.0–10.5** (`src/platform/macosx/`) — built with a
  `powerpc-apple-darwin8` GCC cross-compiler against Apple's real
  `MacOSX10.4u.sdk`, producing a genuine Mach-O `.app` bundle.
- **Modern macOS 11+** (`src/platform/cocoa/`) — a native AppKit app (one
  Objective-C file, ARC, no nibs) built with `clang` on a `macos-latest`
  runner as a Universal 2 binary (Intel `x86_64` + Apple Silicon `arm64`).
- **System 7.x** (`src/platform/system7/`) — built with Retro68's classic
  **m68k** toolchain, targeting 68k Macs (developed against a System 7.1
  Macintosh SE/30 with its built-in 512×342 1-bit screen) using the classic
  Toolbox only, with no Carbon or Appearance Manager. It uses color on
  machines with a color display and a 1-bit black/white/gray rendering on
  monochrome ones.
- **Windows 3.1 / 3.11 (with Win32s) / NT 3.51 / 95 / 98 / ME / NT 4.0 / 2000 / XP**
  (`src/platform/win32/`) — built with [Open Watcom](https://open-watcom.github.io/)
  1.9 as a single 32-bit x86 PE executable (`IWORDLE.EXE`) that runs
  unchanged on every one of those versions. It uses plain Win32 with native
  menus, dialogs and system colors, so it looks 3.1-style on NT 3.51,
  95-style on 95/NT 4.0/2000 and themed on XP.

These are not a single "fat" binary and can't be — Mac OS 9 uses the classic
PEF/CFM executable format, Mac OS X uses Mach-O, System 7 runs a plain
68k code-resource application, and Windows uses the PE format; the container
formats differ and so does the CPU (PowerPC vs. 68k vs. x86).

Prebuilt binaries for all five platforms are published on the
[Releases](../../releases) page. See [`PROJECT.md`](PROJECT.md) for the
design spec and [`CLAUDE.md`](CLAUDE.md) for the technical constraints this
codebase follows.

Nothing is compiled locally: every push is built by GitHub Actions, and the
resulting binaries are uploaded as workflow artifacts.

## Architecture

The game is split into two layers so the same rules engine backs all
platform front ends:

- `src/core/` - portable C99 game engine (board state, guess evaluation,
  dictionary, RNG). No Mac Toolbox, no Carbon, no platform headers.
- `src/platform/macos9/` - the Mac OS 9 front end: window/menu setup,
  the classic `WaitNextEvent` loop, and QuickDraw rendering, using
  Retro68's Carbon PowerPC toolchain.
- `src/platform/macosx/` - the Mac OS X front end: the same event-loop
  architecture, but built against real Apple headers/SDK and using genuine
  native controls throughout (see "UI philosophy" below) instead of any
  hand-drawn content.
- `src/platform/system7/` - the System 7 front end: the same event-loop
  architecture and game flow, written against the classic Toolbox only (no
  Carbon), with the classic Dialog Manager (`DLOG`/`DITL`) for its dialogs
  and a 1-bit rendering path for monochrome screens. Built with Retro68's
  m68k toolchain.
- `src/platform/win32/` - the Windows front end: a plain Win32 message loop
  with native menus and `DIALOG` resources, restricted to the APIs that exist
  on Windows NT 3.51 and Win32s (static imports from KERNEL32, USER32 and
  GDI32 only; COMCTL32 is loaded dynamically). Built with
  Open Watcom 1.9; the icon is generated from the Mac icon family in CI by
  `tools/mac_icon_to_ico.py`.

The Mac OS 9 and Mac OS X front ends target Carbon rather than a
68K/PPC-only or Cocoa-only API, which is what made a from-scratch OS X port
straightforward: the same Toolbox surface (Window/Menu/Control/Dialog
Managers, `WaitNextEvent`) works natively on both, just linked against a
different SDK/toolchain per platform. The System 7 front end predates Carbon
and sticks to the original Toolbox calls.

## UI philosophy (Mac OS X port)

Every window and dialog in the Mac OS X build is built from real native
Carbon controls — `CreateStaticTextControl`, `CreateEditUnicodeTextControl`,
native buttons, native window background brushes (`SetThemeWindowBackground`)
— never hand-drawn text or hand-painted backgrounds. This was a deliberate,
hard-won standing rule after an extended debugging arc where every
hand-drawn substitute (raw `DrawString`, `DrawThemeTextBox`, manual ATSUI)
rendered subtly wrong in some way that real native controls simply don't.
See `CLAUDE.md` for the specifics if you're touching that code.

## Building

Building happens entirely in CI:

- `.github/workflows/build.yml` — Mac OS 9, using the prebuilt
  `ghcr.io/autc04/retro68` Docker image.
- `.github/workflows/build-system7.yml` — System 7, using the same
  `ghcr.io/autc04/retro68` Docker image with its m68k toolchain; the job
  also packs the app onto a 1.44 MB HFS floppy image with `hfsutils`.
- `.github/workflows/build-macosx.yml` — Mac OS X, using a
  `powerpc-apple-darwin8` GCC cross-compiler + Apple's real
  `MacOSX10.4u.sdk`, with Retro68's `Rez` borrowed only for resource
  compilation (not the toolchain itself). The `.dmg` is assembled with
  `libdmg-hfsplus`, so no real Mac is needed to build one.
- `.github/workflows/build-macos.yml` — modern macOS 11+, on GitHub's
  `macos-latest` runner with the Xcode command-line tools: one Universal 2
  `clang` build, bundled and ad-hoc signed.
- `.github/workflows/build-win32.yml` — Windows, using Open Watcom 1.9 on an
  Ubuntu runner (`open-watcom/setup-watcom`). The job audits the EXE's import
  table so it can't accidentally depend on anything newer than NT 3.51, then
  packs the EXE into a zip, a 1.44 MB FAT floppy image and an ISO.

To reproduce the Mac OS 9 build locally with Docker:

```bash
docker run --rm -v "$(pwd):/root/iWordle" -w /root/iWordle -i ghcr.io/autc04/retro68 /bin/bash <<'EOF'
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/powerpc-apple-macos/cmake/retrocarbon.toolchain.cmake
cmake --build .
EOF
```

The Mac OS X build needs Apple's `MacOSX10.4u.sdk`, which isn't
redistributable, so reproducing it outside CI requires supplying that SDK
yourself — see `.github/workflows/build-macosx.yml` for the exact compiler
flags and linked frameworks.

## Artifacts

**Mac OS 9** workflow runs upload an `iWordle-mac-os-9-ppc` artifact
containing:

- `iWordle.dsk` - a raw HFS disk image containing just the app. This is the
  file to hand to an emulator.
- `iWordle.APPL` - the application with its resource fork, as a folder your
  emulator's host-file-sharing can mount directly.
- `iWordle.bin` - a MacBinary-encoded copy of the app, for transferring
  through tools that don't preserve resource forks natively.
- `iWordle.sit` - a StuffIt archive of the app, resource fork intact.

**System 7** workflow runs upload an `iWordle-system-7-68k` artifact
containing:

- `iWordle-floppy.img` - a 1.44 MB raw HFS floppy image containing just the
  app. This is the file to hand to an emulator's floppy drive.
- `iWordle-cd.iso` - a hybrid ISO9660/HFS CD image with an Apple partition
  map, for emulators whose Mac OS won't mount a bare HFS volume (for example
  Mac OS 8 in QEMU's `q800`, which offers to initialize `iWordle.dsk`).
  Insert it in an emulated SCSI CD-ROM drive.
- `iWordle.dsk` - a raw HFS disk image containing just the app.
- `iWordle.APPL` - the application with its resource fork, as a folder your
  emulator's host-file-sharing can mount directly.
- `iWordle.bin` - a MacBinary-encoded copy of the app.

**Mac OS X** workflow runs upload an `iWordle-mac-os-x-ppc` artifact
containing:

- `iWordle-mac-os-x-ppc.zip` - the `iWordle.app` bundle, zipped.
- `iWordle.dmg` - a disk image containing the app, ready to mount and drag
  to Applications.

**Modern macOS** workflow runs upload an `iWordle-macos-universal` artifact
containing:

- `iWordle-macos.zip` - the Universal (Intel + Apple Silicon) `iWordle.app`
  bundle, zipped.
- `iWordle-macos.dmg` - a disk image containing the app.

The app is ad-hoc signed but not notarized, so the first time, right-click
`iWordle.app` and choose Open, or run
`xattr -dr com.apple.quarantine iWordle.app`.

**Windows** workflow runs upload an `iWordle-windows-x86` artifact
containing:

- `IWORDLE.EXE` - the game itself, a single 32-bit executable.
- `iWordle-win32.zip` - the EXE, zipped.
- `iWordle-floppy.img` - a 1.44 MB FAT floppy image containing just the EXE.
- `iWordle.iso` - a CD image containing just the EXE.
- `IWORDLE.MAP` - the linker map, for debugging.

## Testing in `qemu-system-ppc`

**Mac OS 9**: download and unzip the `iWordle-mac-os-9-ppc` artifact from
the Actions run, then attach `iWordle.dsk` as an extra disk alongside your
existing Mac OS 9 boot disk image:

```bash
qemu-system-ppc -M mac99 -m 256 \
  -drive file=macos9.img,format=raw,media=disk \
  -drive file=iWordle.dsk,format=raw,media=disk \
  -boot c
```

Mac OS 9 will mount `iWordle.dsk` as an ordinary HFS volume on the desktop.
Drag `iWordle` onto your hard disk (or launch it straight off the mounted
volume) and run it.

**Mac OS X**: mount `iWordle.dmg` inside a Mac OS X 10.0–10.5 PowerPC
install running under `qemu-system-ppc`, and drag `iWordle.app` to
Applications (or run it directly off the mounted volume).

## Testing in MAME (SE/30)

**System 7**: the 68k build is tested on a MAME-emulated Macintosh SE/30
running System 7.1. Download and unzip the `iWordle-system-7-68k` artifact
from the Actions run, then boot your own System 7.1 hard disk image with
`iWordle-floppy.img` (in the artifact's `package/` folder) in the floppy
drive:

```bash
mame se30 -hard1 <your System 7.1 disk>.chd -flop1 package/iWordle-floppy.img
```

The floppy mounts on the desktop as an ordinary HFS volume named `iWordle`.
Drag `iWordle` onto the hard disk and run it from there.

## Testing on Windows

Download and unzip the `iWordle-windows-x86` artifact from the Actions run,
then mount `iWordle.iso` (or `iWordle-floppy.img` as a floppy) in
VirtualBox, 86Box or VMware running Windows NT 3.51, 95, 98, ME, NT 4.0,
2000 or XP.

**Windows 3.1 / 3.11:** the same `IWORDLE.EXE` runs there through Microsoft's
Win32s add-on. Install **Win32s 1.30c** first (the redistributable
`PW1118.EXE` / `W32S130.ZIP`; Win32s includes the FreeCell test game, which is
a good way to check it works). Then copy `IWORDLE.EXE` to the hard disk and run
it from File Manager or Program Manager. Windows 3.1 VMs usually use the
floppy image.

**Copy `IWORDLE.EXE` to the virtual hard disk before playing.** The ranking
statistics are saved in `IWORDLE.DAT` next to the EXE, and a CD or floppy is
read-only, so running it straight off the disc loses your stats. On XP under a
limited account, keep the EXE somewhere writable (not in `Program Files`).

## Controls

- Type letters with the physical keyboard, or click the on-screen keyboard.
- **Return** submits a guess; **Delete** removes the last letter.
  The on-screen keyboard also has an **Enter** key (left of Z), so the
  game can be played with the mouse alone.
- **File > New Game** starts over; **File > Ranking** (⌘R) shows
  per-player statistics, and opens automatically after a result is
  recorded; **File > End Game** (⌘W) gives up and reveals the word.
- **Windows**: **F2** (or Ctrl+N) starts a new game, **Ctrl+R** shows the
  ranking, and everything lives under the **Game** menu (**Game > End Game**
  gives up and reveals the word).
