# Resizable, scaling main window — Mac ports plan

User request (2026-10-25): every build gets a resizable main window. When the window
shrinks or grows, the tiles, keys and fonts scale with it so everything always fits.
The Windows port already does this (`src/platform/win32/main.c`: `gScale`,
`ScaleLength`, `ScaleRect`, `UpdateLayout`, `OnGetMinMaxInfo`). Use it as the
reference and mirror its names and structure.

All builds are CI-only. Never build or probe toolchains locally. Don't commit. Only
touch the files listed for your port.

## Common design (all three ports)

1. **Design units.** The existing layout `#define`s stay as they are and become
   "design units". Add `DESIGN_W` / `DESIGN_H`: the current content size of the main
   window (values per port below).
2. **Globals:** `static short gScale = 1000;` (per mille, aspect preserved),
   `static short gOffsetX = 0, gOffsetY = 0;`. Use `long` math for the products.
3. **Helpers** (same names as Windows):
   - `static short ScaleLength(short v)` returns `(short)(((long)v * gScale) / 1000)`.
   - `static void ScaleRect(Rect *r)` maps each edge independently:
     `r->left = gOffsetX + ScaleLength(r->left)`, and the same for top, right and
     bottom.
   - `static void UpdateLayout(short contentW, short contentH)` computes
     `sx = contentW*1000/DESIGN_W` and `sy = contentH*1000/DESIGN_H`, takes
     `gScale = min(sx, sy)` (≥ 1), and centers the layout:
     `gOffsetX = (contentW - DESIGN_W*gScale/1000)/2`, and the same for Y. There are no
     cached fonts on the Mac, so this only sets the globals.
4. **Every layout helper scales its result.** `GetTileRect`, `GetLetterKeyRect`,
   `GetBackspaceKeyRect` and `GetEnterKeyRect` build the design-unit rect as today, then
   call `ScaleRect(outRect)` at the end. Painting, hit-testing (`HandleContentClick`)
   and partial redraws then follow automatically. **Grep each port** for any other
   direct use of layout constants (`BOARD_`, `KEYBOARD_TOP`, `KB_`, `KEY_`, `TILE_`,
   `MARGIN`) outside the helpers and route it through the helpers or `ScaleRect`.
5. **Fonts scale.** Wrap every `TextSize(n)` used to draw the **main window's** tiles and
   keys as `TextSize(ScaleLength(n))`, with a floor of 7. Leave the About / name /
   ranking / message windows alone. The "does the label fit" checks (System 7's
   `DrawWideKey` "Enter"/"Ent", "Delete"/"Del") already measure the real scaled rect,
   so they keep working. Bevel/frame line widths stay 1px (2px where they are 2px
   today); they don't scale. System 7's `ovalInset` is scaled with `ScaleLength`, with a
   minimum of 1.
6. **Minimum and maximum size.** `MIN_SCALE 600`: the content can't shrink below 60% of
   the design size. The maximum is the main screen's bounds (minus the menu bar).
7. **Resize and zoom.** The main window's `WIND` proc becomes **`zoomDocProc`** (8),
   which has a grow box and a zoom box. In `HandleMouseDown`:
   - `inGrow` → resize with the per-port call below, then call `OnWindowResized()`.
   - `inZoomIn` / `inZoomOut` → `if (TrackBox(w, event->where, part)) { SetPort…;
     EraseRect(whole port); ZoomWindow(w, part, false); OnWindowResized(); }`. Plain
     `ZoomWindow` with the WDEF's default standard state is fine.
   - `static void OnWindowResized(void)` reads the window's current content size, calls
     `UpdateLayout`, then erases and invalidates the whole content (the board moves, so
     no partial update).
8. **Initial layout.** Right after the main window is created, call
   `UpdateLayout(DESIGN_W, DESIGN_H)` (the `WIND` bounds stay the 100% size).
9. **Never draw the board under the grow box.** The design already leaves a bottom
   margin of at least 14 design units, so nothing extra is needed. Just don't remove
   it.

## Mac OS 9 — `src/platform/macos9/main.c`, `src/platform/macos9/iWordle.r`

- `DESIGN_W 502`, `DESIGN_H 568` (the current `WIND 128` content, which includes the
  extra 10px bottom margin added for the grow box).
- `WIND 128`: proc `documentProc` → `zoomDocProc`. Keep the bounds. Update the comment.
- This is a Carbon build, so resize with
  `ResizeWindow(w, event->where, &limits, NULL)`. `limits` is a `Rect` whose
  top-left is the minimum content size (`DESIGN_*` × `MIN_SCALE`/1000) and whose
  bottom-right is the screen's size (`GetQDGlobalsScreenBits(&bits)`, or
  `GetAvailableWindowPositioningBounds` if it's already used). Content size comes from
  `GetPortBounds(GetWindowPort(w), &r)`. Invalidate with
  `InvalWindowRect(w, &r)` after `EraseRect`.
- Scale the main-window `TextSize` calls: tiles 28, keys 16, wide keys 14.

## Mac OS X — `src/platform/macosx/main.c`, `src/platform/macosx/iWordle.r`, `CLAUDE.md`

- `DESIGN_W 502`, `DESIGN_H 568` (WIND 128 raised from 558 to match OS 9; see "Grow-box band" below).
- `WIND 128`: proc `noGrowDocProc` → `zoomDocProc`. Rewrite the long comment above it:
  the window is now deliberately resizable (user request, 2026-10-25), so the grow
  chrome is wanted. Delete the `ChangeWindowAttributes(gWindow, 0,
  kWindowResizableAttribute)` call and its comment in `main()`.
- Resize with `ResizeWindow` exactly as OS 9 does (Carbon). Zoom as in the common
  section.
- Scale the main-window `TextSize` calls: tiles 28, keys 16, wide keys 14.
- `CLAUDE.md`: in the Mac OS X specifics list, amend the
  `ChangeWindowAttributes`/`noGrowDocProc` bullet. Keep the lesson (use
  `noGrowDocProc` for windows that must *never* resize), and note that the main window
  is now intentionally a resizable `zoomDocProc`. Add one bullet to a new short
  "Resizable main window (all builds)" paragraph at the end of the file (one shared
  section). It should explain the design-units / `gScale` / scaled layout helpers /
  60% minimum scheme, and that layout helpers return already-scaled rects. Only the
  OS X agent edits CLAUDE.md.

## System 7 — `src/platform/system7/main.c`, `src/platform/system7/iWordle.r`

- `DESIGN_W 304`, `DESIGN_H 293` (WIND 128 raised from 286; see "Grow-box band" below).
- `WIND 128`: proc `noGrowDocProc` → `zoomDocProc`. Keep the bounds. Update the comment.
- This is the classic Toolbox (no Carbon). For `inGrow`:
  `newSize = GrowWindow(w, event->where, &sizeRect)`, where `sizeRect` is
  `{minH, minW, maxH, maxW}` in the classic GrowWindow order
  (top = min height, left = min width, bottom = max height, right = max width). The
  minimums come from `MIN_SCALE`. The maximums are `qd.screenBits.bounds` size. If
  `newSize != 0`, call `SizeWindow(w, LoWord(newSize), HiWord(newSize), true)`, then
  `OnWindowResized()`. Content size is `w->portRect`. Invalidate with `SetPort(w);
  EraseRect(&w->portRect); InvalRect(&w->portRect);`.
- **Grow icon:** a classic `documentProc` window needs the app to draw it. At the end of
  `HandleUpdate` (and after `OnWindowResized`'s redraw), draw it **clipped to just the
  15×15 box**, so the scroll-bar divider lines don't appear: save the clip region, then
  `ClipRect` to `{bottom-15, right-15, bottom, right}` of `portRect`,
  `DrawGrowIcon(w)`, and restore the clip. Do the same on activate/deactivate events
  if the port handles `activateEvt` (if it doesn't, add a minimal handler that only
  redraws the grow icon).
- Scale the main-window text sizes: tile Geneva 18, keys Chicago 12, and the wide keys'
  Chicago 12 measurement in `DrawWideKey`. That measurement calls `TextSize` before
  `StringWidth`, so measure at the scaled size.
- The "redraw only the affected row per keystroke" rule still holds. `DrawRow` uses
  `GetTileRect`, so it's scaled for free.
- CLAUDE.md is handled by the OS X agent. Don't edit it.

## Review checklist (lead)
- Hit-testing matches the painted keys at 60%, at 100% and when zoomed.
- No layout constant is used unscaled outside the helpers.
- Fonts: only the main window's sizes scale.
- The grow box is drawn and the board never overlaps it.
- The OS X `ChangeWindowAttributes` removal is really gone.

## Grow-box band (lead's fix after review)
The bottom margins alone don't keep the keyboard clear of the 15px grow box at small
scales; System 7 overlapped it even at 100%. Each port therefore defines `LAYOUT_H`
(design-unit bottom of the keyboard: 544 on OS 9/OS X, 278 on System 7) and
`GROW_BOX_SIZE 15`. `UpdateLayout` scales vertically by
`(contentH - GROW_BOX_SIZE) / LAYOUT_H` and centers the layout within
`contentH - GROW_BOX_SIZE`, so a 15px band at the bottom always stays clear. The
minimum content height is `LAYOUT_H × MIN_SCALE + GROW_BOX_SIZE`.
