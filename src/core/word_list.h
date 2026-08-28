/*
 * Portable dictionary of common 5-letter English words, all uppercase.
 *
 * This is an original, hand-picked word list (not the NYT Wordle answer
 * list) used both as the pool of possible secret words and as the set of
 * guesses accepted as valid. Kept deliberately small so it stays cheap to
 * embed in a PowerPC binary; expand freely later.
 */
#ifndef WORD_LIST_H
#define WORD_LIST_H

#include "wordle_engine.h"

static const char * const kWordleDictionary[] = {
    "ABOUT", "ABOVE", "ACTOR", "ADMIT", "ADOPT", "AFTER", "AGAIN", "AGENT",
    "AGREE", "AHEAD", "ALARM", "ALBUM", "ALERT", "ALIKE", "ALIVE", "ALLOW",
    "ALONE", "ALONG", "ALTER", "AMONG", "ANGER", "ANGLE", "ANGRY", "APPLE",
    "APPLY", "ARENA", "ARGUE", "ARISE", "ARRAY", "ASIDE", "ASSET", "AVOID",
    "AWAKE", "AWARD", "AWARE", "BADLY", "BASIC", "BEACH", "BEGAN", "BEGIN",
    "BEING", "BELOW", "BENCH", "BIRTH", "BLACK", "BLADE", "BLAME", "BLANK",
    "BLIND", "BLOCK", "BLOOD", "BOARD", "BOAST", "BONUS", "BOOST", "BOUND",
    "BRAIN", "BRAND", "BRAVE", "BREAD", "BREAK", "BREED", "BRIEF", "BRING",
    "BROAD", "BROWN", "BUILD", "BUYER", "CABIN", "CABLE", "CANDY", "CARGO",
    "CARRY", "CATCH", "CAUSE", "CHAIN", "CHAIR", "CHALK", "CHAOS", "CHARM",
    "CHART", "CHASE", "CHEAP", "CHECK", "CHESS", "CHEST", "CHIEF", "CHILD",
    "CHOSE", "CIVIL", "CLAIM", "CLASS", "CLEAN", "CLEAR", "CLICK", "CLIFF",
    "CLIMB", "CLOCK", "CLOSE", "CLOUD", "COACH", "COAST", "COLOR", "COUCH",
    "COULD", "COUNT", "COURT", "COVER", "CRAFT", "CRASH", "CRAZY", "CREAM",
    "CRIME", "CROSS", "CROWD", "CROWN", "CURVE", "CYCLE", "DAILY", "DANCE",
    "DEATH", "DELAY", "DEPTH", "DOING", "DOUBT", "DOZEN", "DRAFT", "DRAMA",
    "DRANK", "DRAWN", "DREAM", "DRESS", "DRILL", "DRINK", "DRIVE", "DROVE",
    "DUSTY", "EAGER", "EARLY", "EARTH", "EIGHT", "EMPTY", "ENEMY", "ENJOY",
    "ENTER", "ENTRY", "EQUAL", "ERROR", "EVENT", "EVERY", "EXACT", "EXIST",
    "EXTRA", "FAITH", "FALSE", "FAULT", "FENCE", "FIELD", "FIFTH", "FIGHT",
    "FINAL", "FIRST", "FIXED", "FLAME", "FLASH", "FLEET", "FLOOR", "FLUID",
    "FOCUS", "FORCE", "FORTH", "FORUM", "FOUND", "FRAME", "FRANK", "FRESH",
    "FRONT", "FROST", "FRUIT", "FUNNY", "GHOST", "GIANT", "GIVEN", "GLASS",
    "GLOBE", "GLORY", "GRACE", "GRADE", "GRAND", "GRANT", "GRASS", "GREAT",
    "GREEN", "GRIEF", "GROUP", "GROWN", "GUARD", "GUESS", "GUEST", "GUIDE",
    "HAPPY", "HARSH", "HEART", "HEAVY", "HELLO", "HONOR", "HORSE", "HOTEL",
    "HOUSE", "HUMAN", "HUMOR", "IDEAL", "IMAGE", "IMPLY", "INDEX", "INNER",
    "INPUT", "ISSUE", "JOINT", "JUDGE", "JUICE", "KNEEL", "KNIFE", "KNOWN",
    "LABEL", "LARGE", "LASER", "LATER", "LAUGH", "LAYER", "LEARN", "LEAST",
    "LEAVE", "LEGAL", "LEMON", "LEVEL", "LIGHT", "LIMIT", "LOGIC", "LOOSE",
    "LOWER", "LOYAL", "LUCKY", "LUNCH", "MAGIC", "MAJOR", "MAKER", "MARCH",
    "MATCH", "MAYBE", "MAYOR", "MEDAL", "MEDIA", "MERIT", "METAL", "MIGHT",
    "MINOR", "MINUS", "MODEL", "MONEY", "MONTH", "MORAL", "MOTOR", "MOUNT",
    "MOUSE", "MOUTH", "MOVIE", "MUSIC", "NAKED", "NIGHT", "NOISE", "NORTH",
    "NOVEL", "NURSE", "OCEAN", "OFFER", "OFTEN", "ORDER", "OTHER", "OUGHT",
    "OUTER", "PANEL", "PANIC", "PAPER", "PARTY", "PATCH", "PAUSE", "PEACE",
    "PHASE", "PHONE", "PHOTO", "PIANO", "PIECE", "PILOT", "PITCH", "PLACE",
    "PLAIN", "PLANE", "PLANT", "PLATE", "POINT", "POUND", "POWER", "PRESS",
    "PRICE", "PRIDE", "PRIME", "PRINT", "PRIOR", "PRIZE", "PROOF", "PROUD",
    "PROVE", "QUEEN", "QUERY", "QUICK", "QUIET", "QUITE", "RADIO", "RAISE",
    "RANGE", "RAPID", "RATIO", "REACH", "READY", "REALM", "REBEL", "REFER",
    "RELAX", "REPLY", "RIDGE", "RIGHT", "RIVER", "ROBOT", "ROUGH", "ROUND",
    "ROUTE", "ROYAL", "RURAL", "SALAD", "SAUCE", "SCALE", "SCENE", "SCOPE",
    "SCORE", "SENSE", "SHAPE", "SHARE", "SHARP", "SHEEP", "SHEET", "SHELF",
    "SHELL", "SHIFT", "SHINE", "SHIRT", "SHOCK", "SHOOT", "SHORT", "SHOWN",
    "SIGHT", "SILLY", "SINCE", "SIXTH", "SIXTY", "SKILL", "SLEEP", "SLIDE",
    "SMALL", "SMART", "SMELL", "SMILE", "SMOKE", "SOLID", "SOLVE", "SORRY",
    "SOUND", "SOUTH", "SPACE", "SPARE", "SPARK", "SPEAK", "SPEED", "SPELL",
    "SPEND", "SPLIT", "SPOKE", "SPORT", "STAFF", "STAGE", "STAIR", "STAKE",
    "STAND", "START", "STATE", "STEAM", "STEEL", "STICK", "STIFF", "STILL",
    "STOCK", "STONE", "STOOD", "STORE", "STORM", "STORY", "STRIP", "STUCK",
    "STUDY", "STUFF", "STYLE", "SUGAR", "SUPER", "SWEET", "TABLE", "TAKEN",
    "TASTE", "TEACH", "THANK", "THEME", "THERE", "THESE", "THICK", "THING",
    "THINK", "THIRD", "THOSE", "THREE", "THREW", "THROW", "TIGHT", "TIMER",
    "TITLE", "TODAY", "TOPIC", "TOTAL", "TOUCH", "TOUGH", "TOWER", "TRACK",
    "TRADE", "TRAIL", "TRAIN", "TREAT", "TREND", "TRIAL", "TRIBE", "TRICK",
    "TRUCK", "TRULY", "TRUST", "TRUTH", "TWICE", "UNDER", "UNION", "UNTIL",
    "UPPER", "UPSET", "URBAN", "USAGE", "USUAL", "VALID", "VALUE", "VIDEO",
    "VIRUS", "VISIT", "VITAL", "VOICE", "WASTE", "WATCH", "WATER", "WHEEL",
    "WHERE", "WHICH", "WHILE", "WHITE", "WHOLE", "WHOSE", "WOMAN", "WORLD",
    "WORRY", "WORSE", "WORST", "WORTH", "WOULD", "WOUND", "WRITE", "WRONG",
    "YIELD", "YOUNG", "YOUTH"
};

#define WORDLE_DICTIONARY_COUNT \
    ((int)(sizeof(kWordleDictionary) / sizeof(kWordleDictionary[0])))

#endif /* WORD_LIST_H */
