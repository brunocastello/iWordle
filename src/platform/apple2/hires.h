/*
 * Apple II hi-res primitives: 280x192, 7 pixels per byte, 40 byte columns.
 * Everything is byte-aligned (x and widths count bytes, y counts lines), so
 * drawing is plain byte stores. Mono only: white is 0x7F, black 0x00, and
 * bit 7 (the palette bit) is never set. Pixel 0 of a byte is its bit 0.
 */
#ifndef IWORDLE_HIRES_H
#define IWORDLE_HIRES_H

/* DrawGlyph flags */
#define GL_INVERT 1   /* black on white (the whole 7-pixel byte is flipped) */
#define GL_DIM    2   /* glyph ANDed with the gray dither */
#define GL_DOUBLE 4   /* every glyph line twice: 14 lines tall */

/* DrawBox fill */
#define FILL_BLACK 0
#define FILL_WHITE 1
#define FILL_GRAY  2

/* The loop counter WaitKey bumps while it waits; the title screen seeds the
 * RNG from it. */
extern unsigned long gTicks;

/* Builds the row table, clears page 1 and turns hi-res graphics on. */
void HiresInit(void);
void HiresClear(void);
void HiresOff(void);

void FillRect(unsigned char x, unsigned char y, unsigned char w,
              unsigned char h, unsigned char fill);

/* A w-byte-wide, h-line-tall box: frame of `thick` pixels (0-2) in white,
 * interior `fill`. `inset` leaves a 1-pixel black margin at each side of the
 * outer bytes, so adjacent boxes show a gap (keys). */
void DrawBox(unsigned char x, unsigned char y, unsigned char w,
             unsigned char h, unsigned char thick, unsigned char fill,
             unsigned char inset);

void DrawGlyph(unsigned char x, unsigned char y, unsigned char c,
               unsigned char flags);
/* Text on the 40-column grid; y is in lines (8 per text row). */
void DrawText(unsigned char col, unsigned char y, const char *s,
              unsigned char flags);
void DrawTextCentered(unsigned char y, const char *s, unsigned char flags);

/* Blocks for a key press and returns its 7-bit ASCII (case as typed). */
unsigned char WaitKey(void);
unsigned char ToUpper(unsigned char c);

void Beep(void);

#endif /* IWORDLE_HIRES_H */
