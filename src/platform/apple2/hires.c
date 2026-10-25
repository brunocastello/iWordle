/* Apple II hi-res primitives. C89, cc65. */
#include <string.h>

#include "hires.h"
#include "font.h"

#define SW(a) (*(volatile unsigned char *)(a))

unsigned long gTicks;

static unsigned char *gRow[192];

/* ---- Setup ------------------------------------------------------------- */

void HiresClear(void)
{
    memset((void *)0x2000, 0, 0x2000);
}

void HiresInit(void)
{
    unsigned int y;

    for (y = 0; y < 192; y++) {
        gRow[y] = (unsigned char *)(0x2000u + (y & 7) * 0x400u
                + ((y >> 3) & 7) * 0x80u + (y >> 6) * 0x28u);
    }
    HiresClear();
    (void)SW(0xC050);   /* graphics */
    (void)SW(0xC052);   /* full screen */
    (void)SW(0xC054);   /* page 1 */
    (void)SW(0xC057);   /* hi-res */
}

void HiresOff(void)
{
    (void)SW(0xC051);   /* back to text */
}

/* ---- Rectangles -------------------------------------------------------- */

void FillRect(unsigned char x, unsigned char y, unsigned char w,
              unsigned char h, unsigned char fill)
{
    unsigned char i, v;

    v = (fill == FILL_WHITE) ? 0x7F : 0x00;
    for (i = 0; i < h; i++) {
        memset(gRow[y + i] + x, v, w);
    }
}

void DrawBox(unsigned char x, unsigned char y, unsigned char w,
             unsigned char h, unsigned char thick, unsigned char fill,
             unsigned char inset)
{
    unsigned char lm, rm, lf, rf, i, k, f;
    unsigned char *p;

    lm = inset ? 0x7E : 0x7F;
    rm = inset ? 0x3F : 0x7F;
    lf = 0;
    rf = 0;
    if (thick) {
        lf = inset ? 0x02 : 0x01;
        rf = inset ? 0x20 : 0x40;
        if (thick > 1) {
            lf |= (unsigned char)(lf << 1);
            rf |= (unsigned char)(rf >> 1);
        }
    }
    f = (fill == FILL_WHITE) ? 0x7F : 0x00;
    for (i = 0; i < h; i++) {
        p = gRow[y + i] + x;
        if (thick && (i < thick || i >= h - thick)) {
            p[0] = lm;
            memset(p + 1, 0x7F, w - 2);
            p[w - 1] = rm;
            continue;
        }
        if (fill == FILL_GRAY) {
            for (k = 0; k < w; k++) {
                p[k] = ((unsigned char)(y + i + x + k) & 1) ? 0x2A : 0x55;
            }
            p[0] = (unsigned char)((p[0] & lm) | lf);
            p[w - 1] = (unsigned char)((p[w - 1] & rm) | rf);
        } else {
            p[0] = (unsigned char)((f & lm) | lf);
            memset(p + 1, f, w - 2);
            p[w - 1] = (unsigned char)((f & rm) | rf);
        }
    }
}

/* ---- Text -------------------------------------------------------------- */

void DrawGlyph(unsigned char x, unsigned char y, unsigned char c,
               unsigned char flags)
{
    const unsigned char *g;
    unsigned char n, i, b, yy, sh;

    c = (unsigned char)(c - 32);
    if (c > 94) c = 0;
    g = gFont[c];
    sh = (flags & GL_DOUBLE) ? 1 : 0;
    n = sh ? 14 : 8;
    for (i = 0; i < n; i++) {
        b = g[i >> sh];
        yy = (unsigned char)(y + i);
        if (flags & GL_INVERT) b = (unsigned char)(~b & 0x7F);
        if (flags & GL_DIM) b &= ((unsigned char)(yy + x) & 1) ? 0x2A : 0x55;
        gRow[yy][x] = b;
    }
}

void DrawText(unsigned char col, unsigned char y, const char *s,
              unsigned char flags)
{
    while (*s) {
        DrawGlyph(col++, y, (unsigned char)*s++, flags);
    }
}

void DrawTextCentered(unsigned char y, const char *s, unsigned char flags)
{
    DrawText((unsigned char)((40 - strlen(s)) >> 1), y, s, flags);
}

/* ---- Input and sound --------------------------------------------------- */

unsigned char WaitKey(void)
{
    unsigned char k;

    for (;;) {
        k = SW(0xC000);
        if (k & 0x80) {
            (void)SW(0xC010);
            return (unsigned char)(k & 0x7F);
        }
        gTicks++;
    }
}

unsigned char ToUpper(unsigned char c)
{
    if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 32);
    return c;
}

void Beep(void)
{
    unsigned char i, d;

    for (i = 0; i < 120; i++) {
        (void)SW(0xC030);
        for (d = 0; d < 20; d++) { }
    }
}
