# Growing the answer list — plan

User request (2026-10-25): grow the pool of possible answers to match the NYT game's
scale (about 2,300 answers) while keeping more words than it has. Target: **about
3,000 answers** (currently 2,137). Words stay **our own curation**; never copy the NYT
answer list. Nothing is removed from the current answer list.

## Selection rules (NYT style)
A new answer must be:
- an everyday English word an average adult player would know (no archaic,
  dialect, technical, scientific-Latin or proper-noun words, and no abbreviations);
- a **base form**: no plurals (`TREES`), no third-person `-S` verbs (`WALKS`), and no
  `-ED` past tenses (`WALKED`). Irregular base words are fine (`OFTEN`, `WOVEN`).
  Common `-ER`/`-LY`/`-Y` words are fine (`EARLY`, `TOWER`, `CRISP`);
- not offensive, slur-adjacent or crude (the NYT removes these; so do we);
- already present in `kWordleValidGuesses`, so every answer is a known-valid guess.
  Answers are **moved** from that list into `kWordleAnswers` (not duplicated).

## Mechanics
- The candidates are guesses that aren't already answers, after an automatic filter
  that drops `-S` plurals/verbs and `-ED` forms: 10,800 words, one per line, in the
  scratchpad `words/candidates.txt`. The curator picks about 870 of them and writes
  them, one per line, to `words/picked.txt`.
- The lead regenerates `src/core/word_list.h` with a script: answers = old answers +
  picked, sorted alphabetically, 8 per line, same formatting; guesses = old guesses −
  picked. The script also fixes the header comment's counts (it says "~2,100" and,
  stale, "~500"). `WordleIsValidWord` already checks both arrays, so the set of
  accepted words stays exactly the same.
- Memory: the total number of words is unchanged (words move between arrays), so
  there's no size change for System 7, Windows or the floppy images.
