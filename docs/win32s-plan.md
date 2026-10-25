# Windows 3.1 via Win32s — plan

User decision (2026-10-25): Windows 3.1 / 3.11 runs the **existing Win32 EXE** through
Microsoft's Win32s 1.30c add-on. There is no separate Win16 build. The same
`IWORDLE.EXE` must still run on NT 3.51 through XP and on 95/98/ME. CI-only builds;
don't commit.

## What Win32s needs that the EXE doesn't have yet

1. **No COMCTL32 import.** Win32s has no `COMCTL32.DLL`, so a static import makes the
   EXE fail to load on 3.1. `InitCommonControls()` must be called **dynamically**.
   In `WinMain`, use
   `HMODULE cc = LoadLibrary("COMCTL32.DLL"); if (cc) { FARPROC p = GetProcAddress(cc, "InitCommonControls"); if (p) ((void (WINAPI *)(void))p)(); }`.
   Never `FreeLibrary` it (XP's comctl32 v6 must stay loaded for theming). On
   Win32s, `LoadLibrary` returns NULL (or, in Win32s's 16-bit-error style, a value
   < 32, so treat **`(UINT)cc < 32` as failure**) and the call is skipped.
   Remove `#include <commctrl.h>` if nothing else needs it. Remove
   `library comctl32.lib` from the link line.
2. **Relocations kept.** Win32s loads EXEs into a shared address space at whatever
   base is free, so an EXE without a `.reloc` section won't load. Make sure the link
   step doesn't strip them. Watcom keeps them unless `option norelocs` is given; don't
   give it. Add a CI check that `objdump -h IWORDLE.EXE` lists a `.reloc` section, and
   fail if it doesn't.
3. **Import allowlist:** remove `COMCTL32.DLL!InitCommonControls` from
   `src/platform/win32/allowed-imports.txt`. `LoadLibraryA` and `GetProcAddress` are
   already imported (by Watcom's CRT). In the audit's `ALLOWED_DLLS`, drop
   `COMCTL32.DLL`, so a future static COMCTL32 import fails the build.
4. **Nothing else in the UI changes.** Windows 3.1 has Arial (TrueType) and MS Sans
   Serif. `SPI_GETWORKAREA` fails there and the code already falls back to the
   screen size. `DS_3DLOOK` is ignored. Message boxes and dialogs are native 3.1 ones.

## Packaging and docs
- The floppy image and ISO already contain `IWORDLE.EXE`. Windows 3.1 VMs usually use
  the floppy.
- `README.md` "Testing on Windows": add a Windows 3.1 / 3.11 paragraph. Install
  **Win32s 1.30c** first (the redistributable `PW1118.EXE` / `W32S130.ZIP`; Win32s
  includes the FreeCell test game, which is a good way to check that it works). Then
  copy `IWORDLE.EXE` to the hard disk and run it from File Manager or Program Manager.
  Stats are saved next to the EXE.
- `CLAUDE.md` Windows specifics: note the Win32s target. That means no static imports
  beyond KERNEL32/USER32/GDI32, COMCTL32 loaded dynamically, relocations required, and
  `LoadLibrary` failure checked as `< 32`.
- README overview: Windows 3.1/3.11 (with Win32s) joins the Windows support line.

## Files
`src/platform/win32/main.c`, `src/platform/win32/allowed-imports.txt`,
`.github/workflows/build-win32.yml`, `README.md`, `CLAUDE.md`.
