# iWordle

A native Wordle clone for Classic Mac OS, built for PowerPC (G3/G4) hardware
with the [Retro68](https://github.com/autc04/Retro68) cross-toolchain and
styled after the Mac OS 9.2 Platinum theme. See [`PROJECT.md`](PROJECT.md)
for the full design spec and [`CLAUDE.md`](CLAUDE.md) for the technical
constraints this codebase follows.

Nothing is compiled locally: every push is built by GitHub Actions inside
the official Retro68 Carbon/PowerPC toolchain container, and the resulting
disk image is uploaded as a workflow artifact.

## Architecture

The game is split into two layers so it can eventually be ported to Mac OS X
without touching the rules of the game itself:

- `src/core/` - portable C99 game engine (board state, guess evaluation,
  dictionary, RNG). No Mac Toolbox, no Carbon, no platform headers.
- `src/platform/macos9/` - the Mac OS 9 front end: window/menu setup,
  the classic `WaitNextEvent` loop, and all QuickDraw rendering.

The app is built against Retro68's **Carbon** PowerPC toolchain rather than
the classic 68K/PPC one. Carbon apps run on Mac OS 8.1+, Mac OS 9, and
natively on Mac OS X (through Tiger/Leopard), which makes this the toolchain
variant closest to a "write once, recompile later on OS X" target.

## Building

Building happens entirely in CI (`.github/workflows/build.yml`), using the
prebuilt `ghcr.io/autc04/retro68` Docker image:

```bash
docker run --rm -v "$(pwd):/root/iWordle" -w /root/iWordle -i ghcr.io/autc04/retro68 /bin/bash <<'EOF'
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/powerpc-apple-macos/cmake/retrocarbon.toolchain.cmake
cmake --build .
EOF
```

If you do have Docker locally and want to reproduce a build outside of
GitHub Actions, the command above is exactly what CI runs.

## Artifacts

Each workflow run uploads an `iWordle-mac-os-9-ppc` artifact containing:

- `iWordle.dsk` - a raw HFS disk image containing just the app. This is the
  file to hand to an emulator.
- `iWordle.APPL` - the application with its resource fork, as a folder your
  emulator's host-file-sharing can mount directly.
- `iWordle.bin` - a MacBinary-encoded copy of the app, for transferring
  through tools that don't preserve resource forks natively.

## Testing in `qemu-system-ppc`

Download and unzip the `iWordle-mac-os-9-ppc` artifact from the Actions run,
then attach `iWordle.dsk` as an extra disk alongside your existing Mac OS 9
boot disk image:

```bash
qemu-system-ppc -M mac99 -m 256 \
  -drive file=macos9.img,format=raw,media=disk \
  -drive file=iWordle.dsk,format=raw,media=disk \
  -boot c
```

Mac OS 9 will mount `iWordle.dsk` as an ordinary HFS volume on the desktop.
Drag `iWordle` onto your hard disk (or launch it straight off the mounted
volume) and run it.

## Controls

- Type letters with the physical keyboard, or click the on-screen keyboard.
- **Return** submits a guess; **Delete** removes the last letter.
- **File > New Game** starts over; **Game > Give Up** reveals the word.
