# iWordle

> [!CAUTION]
> ### 🤖 AI-Generated Code Ahead
> **This entire repository was crafted with the help of AI.**
>
> If you are allergic to neural networks, synthesized functions, or autocomplete on steroids, **this repository is not for you**.
> You are kindly invited to close this page and write your code line-by-line in `ed` or `vim`. Everyone else, enjoy!

A Wordle clone for old computers, and some new ones. It runs on a Macintosh SE/30 with
System 7, on Mac OS 8 and 9, on every Mac OS X from Tiger to Mojave, on current Macs, and on
Windows from 3.1 to 11. Each version is a real native app for its system, with that system's own
menus, windows and dialogs.

## Download

Get the file for your computer from [Releases](../../releases):

| Your computer | File |
|---|---|
| System 7 – Mac OS 8.1 (68k Macs) | `System7-68k-floppy.img` or `System7-68k-cd.iso` |
| Mac OS 8.6 – 9.2 (PowerPC; 8.6 needs CarbonLib) | `MacOS9-PPC.sit` or `MacOS9-PPC-floppy.img` |
| Mac OS X 10.4 – 10.14 (PowerPC or Intel) | `MacOSX-Universal.dmg` or `.zip` |
| macOS 11 or later (Intel or Apple Silicon) | `macOS-Universal.dmg` or `.zip` |
| Windows 95 – 11 | `IWORDLE.EXE`, in `Windows-x86.zip`, `.iso` or `-floppy.img` |
| Windows 3.1 / 3.11 (with Win32s 1.30c) and NT 3.51 | `IWORD31.EXE`, in the same files |

Copy the app to your hard disk before playing, so it can save your scores.

The modern macOS app isn't notarized, so right-click it and choose **Open** the first time. On
Windows 10 and 11, SmartScreen may ask you to confirm with **More info → Run anyway**.

## Playing

Guess the five-letter word in six tries. Type on your keyboard, or click the on-screen keys:
**ENTER** submits a guess and **BACK** deletes a letter, so you can play with just the mouse.
Green means the letter is in the right spot, yellow means it's in the word but somewhere else, and
gray means it's not in the word. On black-and-white screens, those are black, a gray pattern and a
dimmed tile.

After each game, iWordle asks who's playing and keeps a ranking of games, wins and streaks per
player. The window can be resized, and the board scales with it.

On the Mac, everything is in the **File** menu: New Game (⌘N), End Game (⌘W) and Ranking (⌘R). On
Windows, it's the **Game** menu: New Game (F2), End Game and Ranking (Ctrl+R).

## Testing in emulators

- **System 7 / Mac OS 8 (68k):** MAME's `se30`, or QEMU's `q800` (use the `.iso` there).
- **Mac OS 9 and Mac OS X on PowerPC:** QEMU's `qemu-system-ppc` or SheepShaver.
- **Windows 3.1:** 86Box, with Win32s installed.
- **Windows 95 and later:** 86Box, VirtualBox or VMware.

## Building

Every build runs on GitHub Actions, one workflow per system. There's no local build.

| Workflow | Toolchain |
|---|---|
| System 7 – Mac OS 8.1 / 68k | Retro68 (m68k) |
| Mac OS 8.6 – 9.2 / PowerPC | Retro68 (Carbon) |
| Mac OS X 10.4 – 10.14 / PowerPC + Intel 32-bit | GCC for PowerPC and clang for Intel, against Apple's 10.4 SDK |
| macOS 11+ / Intel 64-bit + Apple Silicon | Xcode on a macOS runner |
| Windows 3.1 – 11 / x86 | Open Watcom 1.9 |

The game logic in `src/core/` is plain C shared by every version. Each system's front end lives
in `src/platform/`.
