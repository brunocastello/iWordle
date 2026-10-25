# Apple II port — implementation plan

Target: **any Apple II with 64K running ProDOS 8**: Apple II and II+ with a 16K
language card, IIe (unenhanced or enhanced), IIc, IIc Plus, IIgs. One disk, plain
**6502** code (cc65 target `apple2`, not `apple2enh`). The user's machine is an
original Apple IIc on its **green monochrome monitor**, so the screen is designed
for one color first, the way the System 7 build is designed for a 1-bit screen.
All builds run on GitHub Actions; nothing is built or tested locally.

## 0. Ground rules

- New front end in `src/platform/apple2/`. Code style: match the other front ends
  (section banners, `static` helpers, `OnNewGame`/`DrawTile` naming, forward
  declarations), but **C89** like the Windows build (cc65 is a C89 compiler):
  declarations at the top of blocks, `/* */` comments only.
- cc65 habits: `unsigned char` loop counters, `static` locals and module-level
  buffers instead of big stack arrays (cc65's C stack is slow), no `float`, no
  `printf` family except one small `utoa`/`itoa` helper path, no stdio `FILE*`
  (use `open`/`read`/`write`/`close`, which are much smaller). `-Oirs`.
- **No conio, no text mode, no 80 columns, no aux memory, no 65C02 opcodes.**
  Every screen is drawn in hi-res with our own font, so lowercase works even on a
  II+ (whose keyboard and text screen are uppercase-only).
- `src/core/` gets exactly one change (§2). Other front ends are not touched
  except for the 1.2 version bump at release time (§9).

## 1. Platform facts the code must respect

| Fact | Consequence |
|---|---|
| ProDOS 8 needs 64K and owns the language card and $BF00–$BFFF | Our program lives in main RAM $0800–$BEFF. 48K machines without a language card are not supported (ProDOS won't boot there). |
| Hi-res page 1 is $2000–$3FFF | Link with cc65's **`apple2-hgr.cfg`** (program starts at $0803, $2000–$3FFF reserved for the screen, the rest from $4000). That gives ~6K below the screen plus ~32K above. Budget: dictionary 19.3K (§3), stack 2K, leaving ~17K for code, font and data. Agent A reports the real numbers from the map file. **Fallback** if it doesn't fit: keep only the 2K header in RAM and read one bucket from `WORDS.DAT` per lookup (`lseek` + `read`, ≤ 334 bytes). |
| Mono monitor (and no way to detect the monitor) | Only black and white: white bytes `0x7F`, black `0x00`, bit 7 (palette bit) always 0. Gray = checkerboard dither (`0x55`/`0x2A` alternating by row). The CLAUDE.md palette does not apply (like System 7 on a 1-bit screen). On a color monitor it still reads, with some color fringing. |
| 280×192, 7 pixels per byte | Everything is **byte-aligned**: tiles, keys and glyphs start on byte columns, so drawing is plain byte stores, no bit shifting. A text line is 40 characters × 8 rows. |
| 1 MHz | As on System 7: a keystroke redraws one tile; submit redraws that row plus the keys whose state changed; full redraw on new game and when coming back from a menu screen. |
| No clock on most machines | Seed the RNG by counting loop iterations while the title screen waits for a key. |
| No beep call | `Beep()` toggles the speaker (read `$C030`) ~100 times with a short delay loop. |
| Keyboard | Read `$C000` (bit 7 = key ready), clear with `$C010`; strip bit 7. II/II+ keyboards send uppercase only and have no Delete key. Letters either case → uppercase. Return ($0D) submits. **Left arrow ($08) and Delete ($7F)** are backspace. Esc ($1B) opens the menu. |

## 2. Core change (the only one)

`src/core/wordle_engine.c`: when `WORDLE_EXTERNAL_DICTIONARY` is defined, don't
include `word_list.h`; call three platform functions instead.

```c
#ifdef WORDLE_EXTERNAL_DICTIONARY
/* The platform owns the word lists (the Apple II loads them packed from
 * disk); see wordle_engine.h. */
#else
#include "word_list.h"
#endif
```

In `wordle_engine.h`, inside `#ifdef WORDLE_EXTERNAL_DICTIONARY`:

```c
unsigned short WordleDictAnswerCount(void);
void WordleDictGetAnswer(unsigned short index, char *out5);
int WordleDictContains(const char *word5);  /* answers and guesses */
```

`WordleIsValidWord` returns `WordleDictContains(word5)`; `WordleNewGame` picks
`NextRandom() % WordleDictAnswerCount()` and calls `WordleDictGetAnswer`. With the
macro undefined, the compiled code is exactly what it is today. Keep it C89.

## 3. Dictionary file `WORDS.DAT` (Agent A)

`tools/apple2_pack_words.py` (Python 3, stdlib only, run with `-I` in CI) parses
`src/core/word_list.h` with the regex `"([A-Z]{5})"`, split at
`kWordleValidGuesses[]`, takes the **union** of answers and guesses (today 2,850
answers, 8,649 words in all), sorts it, and writes:

```
offset 0     "IWD1"                      magic
offset 4     u16 total word count        (little-endian, all u16s)
offset 6     u16 answer count
offset 8     677 × u16 bucket start      index into the entry array; bucket b =
                                         (first-'A')*26 + (second-'A'); entry
                                         count = start[b+1] - start[b]
offset 1362  676 × u8 answers per bucket (max today 75; assert <= 255)
offset 2038  N × u16 entries             bit 15 = is an answer; bits 14-10,
                                         9-5, 4-0 = letters 3, 4, 5 (A=0);
                                         sorted within each bucket
```

Asserts: entries ≤ 10,000 words (static array size in `dict.c`), every bucket
≤ 167 entries is today's max ("CO"), so assert ≤ 200 (lookup buffer size for the
fallback). Today the file is 19,336 bytes.

At startup the front end reads the whole file into static arrays (header + a
`static unsigned short gEntries[10000]`-sized array, sized by the assert) and
checks the magic, showing "LOADING WORDS..." (a few seconds from a real floppy).
A missing/bad file → "WORDS.DAT IS MISSING OR DAMAGED." then wait a key, `exit()`.

- `WordleDictContains`: bucket from the first two letters, binary search the
  bucket's entries on the low 15 bits.
- `WordleDictGetAnswer(i)`: walk the per-bucket answer counts to the bucket,
  return the i-th entry in it with bit 15 set.

## 4. Game screen (hi-res, 280×192, byte columns 0–39)

```
y   0-7    message line: "ESC: MENU" at the right normally; messages
           ("NOT IN WORD LIST", "YOU GOT IT IN 3!", "THE WORD WAS CRANE")
           centered, replacing it until the next key
y  10-139  board: 6 rows × 20px tiles, 2px gaps. Tile = 3 bytes (21px) wide,
           1-byte gap, 5 tiles = 19 bytes, starting at byte 10
y 144-189  keyboard: 3 rows × 14px keys, 2px gaps
             row 1  QWERTYUIOP      10 keys × 3 bytes, from byte 5
             row 2  ASDFGHJKL        9 keys × 3 bytes, from byte 6
             row 3  [ENTER] ZXCVBNM [DEL]  5 + 7×3 + 5 bytes, from byte 4
```

Mockup (approved by the user; one character ≈ one byte column):

```
                                 ESC: MENU
      [C] [R] [A] [N] [E]   <- submitted
      #S# #L# #A# [ ] [ ]   <- typing
      [ ] [ ] [ ] [ ] [ ]
      ...
  [Q][W][E][R][T][Y][U][I][O][P]
   [A][S][D][F][G][H][J][K][L]
 [ENTER][Z][X][C][V][B][N][M][DEL]
```

Fonts (original glyphs, written in `font.c`; no copied fonts):
- **Small** 7×8: ASCII $20–$7E (upper and lowercase), glyph in bits 0–4 of each
  byte (5px wide, 2px spacing). Used for keys, the message line and all the text
  screens (40 columns × 24 lines).
- **Tile** 7×16: the small A–Z glyphs with every row doubled, centered in the
  tile's middle byte, top at tile y+2.

### Tile and key states (mono, mirroring the System 7 1-bit scheme)

| State | Look |
|---|---|
| Empty | 1px white frame, black inside |
| Typed, not submitted | 2px white frame, white letter |
| Correct | solid white fill, black letter (inverse) |
| Present | white frame, dithered gray fill, solid black badge (the middle byte, letter rows) with a white letter |
| Absent | no frame, black, letter dimmed (glyph rows ANDed with the dither) |

Keys use the same table (unused key = "Empty" frame with a white letter).
ENTER and DEL are plain framed keys.

## 5. Screens and flow

All screens are hi-res: clear the screen, draw text with the small font (a
`DrawText(col, row, str)` helper on the 40×24 grid, plus `DrawTextInverse` for a
title bar line), wait for keys, then fully redraw the game screen on return.

1. **Title**: "iWordle" in the tile font, "1.2", "PRESS ANY KEY". Counts loop
   iterations while waiting → `WordleSeedRandom`. Dictionary and stats load
   before it (with the loading message).
2. **Game**: as §4. Submit: too short → beep; not in dictionary → beep +
   "NOT IN WORD LIST"; ok → redraw row + keys; won → "YOU GOT IT IN n!" (n =
   row+1); lost → "THE WORD WAS XXXXX". After a win/loss, the next key goes to
   the name prompt.
3. **Name prompt**: "Your name for the ranking:" with the last name prefilled and
   a block cursor; up to 31 chars (letters, digits, space, `.'-`); left arrow or
   Delete edits, Return accepts (empty = skip), Esc skips. Then
   `WordleStatsRecordResult`, save, show **Ranking**, back to the board with
   "PRESS RETURN FOR A NEW GAME"; Return (or N) starts one.
4. **Menu** (Esc): N New game, G Give up (only while in progress; reveals the word
   and records a loss, like the Mac builds' Give Up), R Ranking, A About,
   Q Quit, Esc back.
5. **Ranking**: columns Name (first 16 chars) / Played / Win% / Cur / Max, up to
   10 players; "C: CLEAR   ESC: BACK" (Clear asks "CLEAR ALL? Y/N").
6. **About** (centered): "iWordle 1.2", "A native Wordle clone for the Apple II",
   "Bruno Castello", "bfcastello@hotmail.com" (System 7's About strings with the
   platform line changed). Any key returns.
7. **Quit**: `exit()` → ProDOS QUIT (Bitsy Bye in ProDOS 2.4).

## 6. Stats

ProDOS file `STATS` (type BIN) on the boot disk, a flat dump of
`WordleStatsBook` (402 bytes; cc65 doesn't pad structs), via `open`/`read`/
`write`. A failed write (write-protected disk) is ignored silently, like the
Windows build on read-only media. Not shared with the other builds.

## 7. Files

```
src/platform/apple2/main.c     screens, input, flow, stats
src/platform/apple2/hires.c/.h hi-res primitives: clear, byte-rect fill/frame,
                               dither fill, glyph blit, text on the 40x24 grid,
                               row address table, Beep, key reading
src/platform/apple2/font.c     the two fonts
src/platform/apple2/dict.c     WORDS.DAT loader + WordleDict* functions
tools/apple2_pack_words.py
.github/workflows/build-apple2.yml
```

Hi-res row addresses: a 192-entry table built at startup
(`0x2000 + (y&7)*0x400 + ((y>>3)&7)*0x80 + (y>>6)*0x28`). Turn graphics on with
the soft switches `$C050` (graphics), `$C052` (full screen), `$C054` (page 1),
`$C057` (hi-res), after clearing the page.

## 8. Build and disk image (Agent A) — `.github/workflows/build-apple2.yml`

Name `Build (Apple II / 6502)`. Triggers: `src/core/**`, `src/platform/apple2/**`,
`tools/apple2_pack_words.py`, the workflow; `workflow_dispatch`.

1. Checkout; `sudo apt-get install -y cc65`; print `cl65 --version`. If it is
   older than 2.19, build cc65 from the `V2.19` tag instead.
2. `python3 -I tools/apple2_pack_words.py src/core/word_list.h build-apple2/WORDS.DAT`.
3. `cl65 -t apple2 -C apple2-hgr.cfg -Oirs -DWORDLE_EXTERNAL_DICTIONARY
   -I src/core -m build-apple2/iwordle.map -o build-apple2/IWORDLE
   src/core/wordle_engine.c src/core/wordle_stats.c src/platform/apple2/*.c`
   (check cc65's docs for how `apple2-hgr.cfg` expects the program to be laid
   out; cc65 2.19 writes an AppleSingle file for apple2 targets). Print the
   segment sizes from the map; the step fails if the link fails (ld65 errors on
   overflow).
4. Disk: download **ProDOS 2.4.3** (`ProDOS_2_4_3.po`) and **AppleCommander's
   `ac` jar** (GitHub releases), each pinned by SHA-256 (the first run prints
   the hashes; then pin them, as `build-macosx.yml` does for the SDK). Then:
   ```
   java -jar ac.jar -pro140 iWordle.po IWORDLE
   dd if=ProDOS_2_4_3.po of=iWordle.po bs=512 count=2 conv=notrunc   # boot blocks
   java -jar ac.jar -g ProDOS_2_4_3.po PRODOS > PRODOS
   java -jar ac.jar -p iWordle.po PRODOS SYS 0x0000 < PRODOS
   java -jar ac.jar -p iWordle.po IWORDLE.SYSTEM SYS 0x2000 \
        < $(cc65 share dir)/target/apple2/util/loader.system
   java -jar ac.jar -as iWordle.po IWORDLE < build-apple2/IWORDLE
   java -jar ac.jar -p iWordle.po WORDS.DAT BIN 0x0000 < build-apple2/WORDS.DAT
   java -jar ac.jar -l iWordle.po
   ```
   ProDOS boots the first `*.SYSTEM` file, so the disk starts the game.
5. Upload artifact `iWordle-apple-ii`: `iWordle.po`, `IWORDLE`, `WORDS.DAT`, the
   map file. `if-no-files-found: error`.

## 9. Docs, testing, release 1.2

- `CLAUDE.md`: add the Apple II as the sixth build (overview bullet, workflow, an
  "Apple II specifics" section with §1's facts). `README.md`: one row in the
  download table and an emulator line (MAME `mame apple2c -flop1 iWordle.po`,
  `mame apple2p` for a II+; AppleWin; Virtual ][; real hardware via Floppy Emu
  or ADTPro).
- Test rig: the user's real IIc + green monitor (and emulators, including a II+).
- After the user's OK: bump every About/version string to **1.2** (all six builds;
  Windows FILEVERSION 1,2,0,0; both Info.plists), tag `v1.2`, release
  "iWordle 1.2" with all builds. New asset: `iWordle-1.2-AppleII-6502.po`.
  Release rules: see the release-conventions memory (two-part tag, short notes,
  "Which file do I need?" table, no drafts left).

## 10. Work split (subagent pattern)

- **Agent A** (porter): §2 core change, §3 packer + `dict.c`, §8 workflow, with a
  minimal `main.c` that loads the dictionary and shows the answer count plus a
  test lookup on the hi-res screen (or just beeps on success) so CI produces a
  bootable disk. Get CI green and report the map's memory numbers.
- **Agent B** (porter): `hires.c`, `font.c`, the real `main.c` per §4–§6.
- Review each piece (reviewer), then a final whole-diff review.
