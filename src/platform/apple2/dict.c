/* WORDS.DAT loader and lookup. C89, cc65. */
#include <fcntl.h>
#include <unistd.h>

#include "wordle_engine.h"
#include "dict.h"

#define DICT_MAX_WORDS   10000
#define DICT_BUCKETS     676
#define DICT_HEADER_SIZE 2038

static unsigned short gTotal;
static unsigned short gAnswers;
static unsigned short gStart[DICT_BUCKETS + 1];
static unsigned char  gAnswerCount[DICT_BUCKETS];
static unsigned short gEntries[DICT_MAX_WORDS];

/* ---- Loading ----------------------------------------------------------- */

static unsigned char ReadFull(int fd, void *buf, unsigned int len)
{
    return (unsigned char)((unsigned int)read(fd, buf, len) == len);
}

unsigned char DictLoad(void)
{
    static unsigned char magic[4];
    static unsigned short counts[2];
    int fd;
    unsigned char ok;

    fd = open("WORDS.DAT", O_RDONLY);
    if (fd < 0) return 0;

    ok = ReadFull(fd, magic, 4)
        && magic[0] == 'I' && magic[1] == 'W' && magic[2] == 'D' && magic[3] == '1'
        && ReadFull(fd, counts, 4)
        && counts[0] <= DICT_MAX_WORDS && counts[1] <= counts[0]
        && ReadFull(fd, gStart, sizeof(gStart))
        && ReadFull(fd, gAnswerCount, sizeof(gAnswerCount))
        && gStart[DICT_BUCKETS] == counts[0]
        && ReadFull(fd, gEntries, counts[0] * 2u);
    close(fd);
    if (ok) {
        gTotal = counts[0];
        gAnswers = counts[1];
    }
    return ok;
}

/* ---- Engine hooks ------------------------------------------------------ */

unsigned short WordleDictAnswerCount(void)
{
    return gAnswers;
}

void WordleDictGetAnswer(unsigned short index, char *out5)
{
    unsigned short b, i, end;

    for (b = 0; b < DICT_BUCKETS; b++) {
        if (index < gAnswerCount[b]) break;
        index -= gAnswerCount[b];
    }
    if (b >= DICT_BUCKETS) b = 0; /* index out of range: cannot happen */
    end = gStart[b + 1];
    for (i = gStart[b]; i < end; i++) {
        if (gEntries[i] & 0x8000u) {
            if (index == 0) break;
            --index;
        }
    }
    if (i >= end) i = gStart[b];
    out5[0] = (char)('A' + b / 26);
    out5[1] = (char)('A' + b % 26);
    out5[2] = (char)('A' + ((gEntries[i] >> 10) & 31));
    out5[3] = (char)('A' + ((gEntries[i] >> 5) & 31));
    out5[4] = (char)('A' + (gEntries[i] & 31));
}

int WordleDictContains(const char *word5)
{
    unsigned char c0, c1;
    unsigned short bk, key, lo, hi, mid, v;

    c0 = (unsigned char)(word5[0] - 'A');
    c1 = (unsigned char)(word5[1] - 'A');
    if (c0 > 25 || c1 > 25) return 0;
    key = (unsigned short)(((unsigned char)(word5[2] - 'A') & 31) << 10)
        | (unsigned short)(((unsigned char)(word5[3] - 'A') & 31) << 5)
        | (unsigned short)((unsigned char)(word5[4] - 'A') & 31);
    bk = (unsigned short)(c0 * 26u + c1);
    lo = gStart[bk];
    hi = gStart[bk + 1];
    while (lo < hi) {
        mid = (unsigned short)((lo + hi) >> 1);
        v = (unsigned short)(gEntries[mid] & 0x7FFFu);
        if (v == key) return 1;
        if (v < key) lo = (unsigned short)(mid + 1);
        else hi = mid;
    }
    return 0;
}
