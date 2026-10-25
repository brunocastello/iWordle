# Modern macOS port (Cocoa, Intel + Apple Silicon) — plan

User decision (2026-10-25): a native app for current macOS on **Intel (x86_64) and
Apple Silicon (arm64)**, shipped as one Universal 2 binary. The Carbon/QuickDraw
front ends can't run there (32-bit only). CI-only builds; don't commit.

## 0. Ground rules
- New front end: `src/platform/cocoa/`, containing `main.m`, `Info.plist`. One
  Objective-C file, ARC, AppKit only, with no nib/xib/storyboard and no Swift.
  `src/core/` is not touched and is compiled as C.
- Deployment target **macOS 11.0** (the first version with Apple Silicon), built on
  GitHub's `macos-latest` runner with the Xcode command-line tools:
  `clang -arch x86_64 -arch arm64 -mmacosx-version-min=11.0 -fobjc-arc -O2 -Wall`.
- Behavior mirrors the other ports exactly: same game flow, messages, name prompt →
  Ranking, stats semantics, and the `[ENTER] Z…M [BACK]` keyboard. It also uses the
  same resizable, aspect-preserving scaled layout with a 60% minimum, so use the
  same design units as the OS X port (502 × 544 layout, see
  `src/platform/macosx/main.c`). Modern macOS windows have no grow box, so there's
  no grow band. Just keep the 14-unit margin below the keyboard.
- Palette from CLAUDE.md. Force the light appearance
  (`NSApp.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua]`) so the
  fixed palette (white tiles, #333333 borders) always reads correctly. Note this in
  CLAUDE.md.

## 1. App structure (`main.m`)
- `int main` → `NSApplication sharedApplication`, `setActivationPolicy:Regular`,
  an `AppDelegate`, a menu bar built in code, then `[NSApp run]`.
  `applicationShouldTerminateAfterLastWindowClosed:` → YES.
- **Menus:**
  - App menu: About iWordle (`orderFrontStandardAboutPanelWithOptions:`, credits
    below), separator, Hide ⌘H, Hide Others ⌥⌘H, Show All, separator, Quit ⌘Q.
  - **Game** menu: New Game ⌘N, End Game ⌘W (same key as the other Mac builds;
    disabled via `validateMenuItem:` unless a game is in progress), Ranking… ⌘R.
  - Window menu: Minimize ⌘M, Zoom. Set it with `NSApp.windowsMenu`.
- **Main window:** titled, closable, miniaturizable, **resizable**. Content size
  502×558 at 100% (layout 544 + 14 margin). `contentMinSize` is 60% of that, and
  `contentAspectRatio` isn't set (the view letterboxes, like the other ports). Title
  "iWordle". Centered on first launch, with `setFrameAutosaveName:@"iWordleMain"`.
- **`BoardView : NSView`** (flipped, `acceptsFirstResponder` YES):
  - Layout helpers take design units, as in the OS X port (`GetTileRect`,
    `GetLetterKeyRect`, `GetBackspaceKeyRect`, `GetEnterKeyRect`). They return
    scaled `NSRect`s using `scale = min(w/502, h/558)` and centering offsets
    recomputed in `drawRect:` / `layout`. Hit-testing (`mouseDown:`) uses the same
    helpers.
  - Drawing with `NSBezierPath`/`NSColor`: tiles are the palette fill plus a 1px
    #333333 frame; letters are `[NSFont boldSystemFontOfSize:28*scale]` (keys
    16, wide keys 14) centered with `NSAttributedString`. Keys are rounded rects
    (radius 4×scale) filled with `NSColor.controlColor`, or the palette color once a
    letter is known, with a 1px `NSColor.separatorColor` stroke; the label is
    `NSColor.controlTextColor`, or white on palette faces. On Retina, line widths stay
    1pt.
  - `keyDown:`: letters → type. Return/Enter (`\r`, `\x03`) → submit, ignoring
    `event.isARepeat`. Delete (`0x7F`) and Forward Delete → backspace. Everything else
    goes to `[super keyDown:]`.
  - `setNeedsDisplay:YES` after every change. Drawing is cheap, so there's no
    per-row invalidation.
- **Messages:** `NSAlert` run as a sheet on the main window
  (`beginSheetModalForWindow:completionHandler:`) for "Not in word list.", win
  ("You got it in N guess(es)!") and lose ("Out of guesses! The word was XXXXX.").
  Too short → `NSBeep()`. The completion handler for win/lose runs the name prompt.
- **Name prompt:** an `NSAlert` sheet with message "Who's playing?", an
  `NSTextField` accessory view (220×24) prefilled with the last name and selected,
  and buttons OK / Cancel. Trim whitespace. Empty or Cancel means don't record. The
  name is limited to `WORDLE_STATS_NAME_LEN` bytes of UTF-8, so truncate cleanly and
  never split a UTF-8 sequence. Then `WordleStatsRecordResult`, save, show Ranking.
- **Ranking:** an `NSPanel` (titled, closable) with an `NSTableView` in an
  `NSScrollView`. The columns are Name, Played, Win %, Cur and Max, with the numbers
  right-aligned, and the data source is `gStats`. Its buttons are Clear and OK
  (default, Return). Clear runs `WordleStatsClear`, saves and reloads the table.
  Show it as a sheet on the main window.
- **Stats file:** `~/Library/Application Support/iWordle/iWordle Stats`. Create the
  folder with `NSFileManager`. It holds the same flat `WordleStatsBook` dump. Load
  validates the size and `playerCount <= WORDLE_STATS_MAX_PLAYERS` and
  NUL-terminates every name. Save writes atomically (`NSData writeToFile:atomically:`).
  The file is not shared with the old builds.
- **About credits** (`NSAboutPanelOptionCredits`, an attributed string, centered,
  small system font): "A native Wordle clone for macOS", "Bruno Castello",
  "bfcastello@hotmail.com", "Engineer: Claude Sonnet 5". Application version 1.0.
  Copyright from Info.plist: "© Castello Designs, 2026".
- Random seed: `WordleSeedRandom((unsigned long)arc4random())`.

## 2. `Info.plist`
`CFBundleName`/`CFBundleDisplayName` iWordle, `CFBundleIdentifier`
`com.castellodesigns.iWordle`, `CFBundleExecutable` iWordle, `CFBundleIconFile`
iWordle, `CFBundlePackageType` APPL, `CFBundleSignature` WRDL,
`CFBundleShortVersionString` 1.0, `CFBundleVersion` 1, `LSMinimumSystemVersion`
11.0, `NSHighResolutionCapable` true, `NSPrincipalClass` NSApplication,
`NSHumanReadableCopyright` "© Castello Designs, 2026", `LSApplicationCategoryType`
public.app-category.word-games.

## 3. Workflow `.github/workflows/build-macos.yml`
Name `Build (macOS Universal / Cocoa)`. Runs on `macos-latest`. Triggers on
`src/core/**`, `src/platform/cocoa/**`, `src/platform/macosx/icons.r` and the workflow
file, plus `workflow_dispatch`.
1. Checkout.
2. Compile: `clang -arch x86_64 -arch arm64 -mmacosx-version-min=11.0 -fobjc-arc -O2
   -Wall -Isrc/core src/core/wordle_engine.c src/core/wordle_stats.c
   src/platform/cocoa/main.m -framework AppKit -o build/iWordle`. Then
   `lipo -info build/iWordle` must list both architectures, or the step fails.
3. Icon: extract `data 'icns' (128)` from `src/platform/macosx/icons.r` with the same
   Python as `build-macosx.yml` into `build/iWordle.icns`.
4. Bundle `package/iWordle.app` (MacOS/iWordle, Resources/iWordle.icns,
   Info.plist, PkgInfo `APPLWRDL`), then **ad-hoc sign** it with
   `codesign --force --deep -s - package/iWordle.app` (arm64 won't run unsigned
   code) and verify with `codesign --verify --verbose package/iWordle.app`.
5. Package: `ditto -c -k --keepParent package/iWordle.app package/iWordle-macos.zip`,
   and `hdiutil create -volname iWordle -srcfolder package/iWordle.app -ov -format UDZO
   package/iWordle-macos.dmg`.
6. Upload artifact `iWordle-macos-universal`: the zip and the dmg.

## 4. Docs
- README: add modern macOS (11+, Intel and Apple Silicon) as a build, with its
  artifacts. Add a first-launch note: the app is ad-hoc signed, not notarized, so
  right-click → Open the first time, or run
  `xattr -dr com.apple.quarantine iWordle.app`.
- CLAUDE.md: a project-overview bullet for `src/platform/cocoa/`, plus a short
  "Modern macOS specifics" section: AppKit/ARC, no nibs, 11.0 deployment target,
  Universal 2 + ad-hoc signing, forced Aqua appearance for the fixed palette, and
  the same design-unit scaling (no grow band).
