# iWordle

A native Wordle clone for classic Mac OS, built for PowerPC hardware and
styled after the Mac OS 9.2 Platinum theme (Mac OS 9) / genuine Aqua chrome
(Mac OS X). Two independent builds ship from this one repo:

- **Mac OS 9** (`src/platform/macos9/`) — built with the
  [Retro68](https://github.com/autc04/Retro68) cross-toolchain, targeting
  Carbon on Mac OS 9.x PowerPC hardware.
- **Mac OS X 10.0–10.5** (`src/platform/macosx/`) — built with a
  `powerpc-apple-darwin8` GCC cross-compiler against Apple's real
  `MacOSX10.4u.sdk`, producing a genuine Mach-O `.app` bundle.

Prebuilt binaries for both platforms are published on the
[Releases](../../releases) page. See [`PROJECT.md`](PROJECT.md) for the
design spec and [`CLAUDE.md`](CLAUDE.md) for the technical constraints this
codebase follows.

Nothing is compiled locally: every push is built by GitHub Actions, and the
resulting binaries are uploaded as workflow artifacts.

## Architecture

The game is split into two layers so the same rules engine backs both
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

Both front ends target Carbon rather than a 68K/PPC-only or Cocoa-only API,
which is what made a from-scratch OS X port straightforward: the same
Toolbox surface (Window/Menu/Control/Dialog Managers, `WaitNextEvent`)
works natively on both, just linked against a different SDK/toolchain per
platform.

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
- `.github/workflows/build-macosx.yml` — Mac OS X, using a
  `powerpc-apple-darwin8` GCC cross-compiler + Apple's real
  `MacOSX10.4u.sdk`, with Retro68's `Rez` borrowed only for resource
  compilation (not the toolchain itself). The `.dmg` is assembled with
  `libdmg-hfsplus`, so no real Mac is needed to build one.

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

**Mac OS X** workflow runs upload an `iWordle-mac-os-x-ppc` artifact
containing:

- `iWordle-mac-os-x-ppc.zip` - the `iWordle.app` bundle, zipped.
- `iWordle.dmg` - a disk image containing the app, ready to mount and drag
  to Applications.

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

## Controls

- Type letters with the physical keyboard, or click the on-screen keyboard.
- **Return** submits a guess; **Delete** removes the last letter.
- **File > New Game** starts over; **File > Ranking** (⌘R) shows
  per-player statistics, and opens automatically after a result is
  recorded; **File > End Game** (⌘W) gives up and reveals the word.
