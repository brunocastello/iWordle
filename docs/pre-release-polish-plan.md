# Pre-release polish — plan

User requests (2026-10-25), on branch `win32-port`. CI-only builds; don't commit.

## A. Windows About box, done the Gateway way (`src/platform/win32/`)
Reference: `../Gateway/src/win32/main_win32.c`, the "About" section (`about_font`,
`kAbout[]`, `AboutProc`, `about_show`). Read it, then reproduce the same design.
It's the Mac About box redrawn in Windows' own typeface:
- Replace the `IDD_ABOUT` DIALOG with a small **registered window class**
  (`"iWordleAboutClass"`, `WS_CAPTION | WS_SYSMENU`, title "About iWordle"). The
  client area is **280 × 272** (the Mac's 230 of content plus the OK button), sized
  with `AdjustWindowRect` and centered in the work area (with the screen fallback
  when `SPI_GETWORKAREA` fails on NT 3.51/Win32s). Keep a single instance: if it's
  already open, `SetForegroundWindow` it.
- `WM_PAINT`: `DrawIcon` (**not** `DrawIconEx`; it must exist on 3.1/Win32s) the
  app icon centered at y = 14. Then `SetTextAlign(TA_CENTER | TA_BASELINE)` and draw
  these lines centered on x = width/2 at these **baselines**, which are the OS 9
  About's exact offsets:

  | y | font | text |
  |---|---|---|
  | 64 | 12 px **bold** | `iWordle 1.0` |
  | 84 | 10 px normal | `A native Wordle clone for Windows` |
  | 112 | 10 px **bold** | `Bruno Castello` |
  | 132 | 10 px normal | `bfcastello@hotmail.com` |
  | 160 | 10 px **bold** | `Engineer: Claude Sonnet 5` |
  | 188 | 10 px normal | `\xA9 Castello Designs, 2026` |
  | 208 | 10 px normal | `Built with Open Watcom` |

  The fonts come from Gateway's `about_font(dpi, px, weight)`:
  `CreateFont(-MulDiv(px, dpi, 96), …, "MS Sans Serif")`, with `dpi` from
  `GetDeviceCaps(dc, LOGPIXELSY)`. Create and delete them in WM_PAINT.
  `SetBkMode(TRANSPARENT)`. The class background is `COLOR_BTNFACE + 1`.
- An OK `BUTTON` (`BS_DEFPUSHBUTTON`, id `IDOK`) at `((280 - 76) / 2, 230)`, 76 × 26.
  `WM_COMMAND IDOK` and `WM_CLOSE` → `DestroyWindow`. `WM_DESTROY` clears the handle.
  Return or Esc should also close it, so either run it from the main message loop with
  `IsDialogMessage(gAbout, &msg)`, or handle `WM_KEYDOWN` for VK_RETURN/VK_ESCAPE.
  IsDialogMessage is in user32 on every target, so it's the simplest.
- Delete `IDD_ABOUT`, `AboutDlgProc`, `gAboutBoldFont`, and the
  `IDC_ABOUT_TITLE`/`IDC_ABOUT_AUTHOR` ids. `OnAbout()` calls `AboutShow()`.
- API floor unchanged (NT 3.51 / Win32s). New imports are `DrawIcon`,
  `SetTextAlign`, `TextOutA`, `GetDeviceCaps` and `IsDialogMessageA`, all present
  since Windows 3.x. Add any new ones to `allowed-imports.txt`, alphabetically sorted.
  C89 only.

## B. Modern macOS app (`src/platform/cocoa/main.m`)
1. **About window matches the OS X build.** Use a custom window, not
   `orderFrontStandardAboutPanel`: an `NSPanel`/`NSWindow` titled "About iWordle",
   closable, not resizable, with a **280 × 230** content area, centered, and only one
   instance. It has a 32 × 32 app icon (`NSApp.applicationIconImage` in an
   `NSImageView`) centered with its top at 14. Below it are centered, non-editable
   `NSTextField` labels whose **tops** match `src/platform/macosx/main.c` `OnAbout()`:

   | top | height | font | text |
   |---|---|---|---|
   | 54 | 20 | `[NSFont boldSystemFontOfSize:13]` (= kThemeAlertHeaderFont) | `iWordle 1.0` |
   | 74 | 18 | `[NSFont systemFontOfSize:NSFont.smallSystemFontSize]` (= kThemeSmallSystemFont) | `A native Wordle clone for macOS` |
   | 100 | 20 | bold 13 | `Bruno Castello` |
   | 120 | 18 | small system | `bfcastello@hotmail.com` |
   | 146 | 20 | bold 13 | `Engineer: Claude Sonnet 5` |
   | 172 | 18 | small system | `© Castello Designs, 2026` |

   Use a flipped container view, or convert `top` into Cocoa coordinates. Label colors
   are `labelColor` / `secondaryLabelColor`, so dark mode works. No OK button: it
   closes from its close box, ⌘W, Esc or Return, like the OS X build. The app menu's
   "About iWordle" opens it.
2. **Follow the system appearance.** Remove the forced
   `NSAppearanceNameAqua`. The palette's state colors (correct `#6AAA64`, present
   `#C9B458`, absent `#787C7E`) stay as they are. The **empty tile** colors become
   dynamic (`+[NSColor colorWithName:dynamicProvider:]`, macOS 10.15+). In light mode,
   fill `#FFFFFF`, border `#333333`, letter `#333333`. In dark mode, fill `#121213`,
   border `#565758`, letter `#FFFFFF` (Wordle's dark scheme). The window background is
   `windowBackgroundColor`.
3. **Real macOS buttons for the on-screen keys.** Replace the hand-drawn keys with
   **`NSButton` subviews** of the board view, one per key (26 letters + ENTER +
   BACK), so each OS draws its own native button. That means Aqua on 11–15, and
   Liquid Glass on 26+ when built against the macOS 26 SDK.
   - `bezelStyle = NSBezelStyleFlexiblePush` (named `NSBezelStyleRegularSquare` in
     older SDKs; use whichever compiles, with an `#if` on
     `__MAC_OS_X_VERSION_MAX_ALLOWED` if needed). This is the push style that
     allows any height. `refusesFirstResponder = YES`, so typing stays with the
     board view. Each button's target/action calls the same handlers as the
     keyboard does.
   - Frames come from the existing scaled layout helpers. Reposition them all in
     the board view's `layout` (or `resizeSubviewsWithOldSize:`) whenever the view
     resizes, and update each key's title font to
     `[NSFont systemFontOfSize:16*scale weight:NSFontWeightSemibold]` (ENTER/BACK
     use 14*scale).
   - State colors: once a letter is correct/present/absent, set the key's
     `bezelColor` to the palette color and give it an `attributedTitle` in white.
     Unknown letters get `bezelColor = nil` and the default title color. Refresh
     after every submit or new game.
   - Hit-testing for keys now comes from the buttons. Remove the key part of
     `mouseDown:` and the key drawing code. Tiles are still drawn by the board view.
4. **Toolchain:** the Liquid Glass look needs the newest SDK. In
   `build-macos.yml`, before compiling, select the newest installed Xcode
   (`sudo xcode-select -s "$(ls -d /Applications/Xcode*.app | sort -V | tail -1)"`)
   and print `xcrun --show-sdk-version`. Keep `-mmacosx-version-min=11.0`. Any API
   newer than 11.0 must be guarded with `@available`.
5. CLAUDE.md "Modern macOS specifics": replace the forced-Aqua note with the dynamic
   empty-tile colors and the NSButton keys. Note the About box mirrors the OS X build.

## C. Workflow names (lead)
Each workflow is named after the OSes it supports and the CPU architecture(s).
