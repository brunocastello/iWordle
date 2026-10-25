/*
 * Apple II dictionary: loads WORDS.DAT (see tools/apple2_pack_words.py) and
 * implements the WordleDict* hooks the engine calls when it is built with
 * WORDLE_EXTERNAL_DICTIONARY.
 */
#ifndef IWORDLE_DICT_H
#define IWORDLE_DICT_H

/* Reads WORDS.DAT from the current ProDOS prefix into static arrays.
 * Returns 1 on success, 0 if the file is missing, short or has a bad
 * magic/count. */
unsigned char DictLoad(void);

#endif /* IWORDLE_DICT_H */
