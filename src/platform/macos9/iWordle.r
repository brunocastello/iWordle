/*
 * Resource definitions for iWordle: menu bar, main document window, and a
 * reusable message dialog (used for New Game / Give Up / Win / Lose text).
 */

#include "Types.r"
#include "Windows.r"
#include "Menus.r"
#include "Dialogs.r"
#include "Processes.r"

/* ---------------------------------------------------------------------- */
/* Menu bar                                                                */
/* ---------------------------------------------------------------------- */

resource 'MBAR' (128) {
    { 128, 129, 130, 131 };
};

resource 'MENU' (128, "File") {
    128,
    textMenuProc,
    allEnabled,
    enabled,
    "File",
    {
        "New Game", noIcon, "N", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Quit", noIcon, "Q", noMark, plain;
    }
};

resource 'MENU' (129, "Edit") {
    129,
    textMenuProc,
    allEnabled,
    enabled,
    "Edit",
    {
        "Undo", noIcon, "Z", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Cut", noIcon, "X", noMark, plain;
        "Copy", noIcon, "C", noMark, plain;
        "Paste", noIcon, "V", noMark, plain;
        "Clear", noIcon, noKey, noMark, plain;
    }
};

resource 'MENU' (130, "Game") {
    130,
    textMenuProc,
    allEnabled,
    enabled,
    "Game",
    {
        "Give Up", noIcon, noKey, noMark, plain;
    }
};

resource 'MENU' (131, "Help") {
    131,
    textMenuProc,
    allEnabled,
    enabled,
    "Help",
    {
        "About iWordle", noIcon, noKey, noMark, plain;
    }
};

/* ---------------------------------------------------------------------- */
/* Main document window                                                   */
/* ---------------------------------------------------------------------- */

resource 'WIND' (128, "Wordle9") {
    { 60, 80, 660, 740 },
    documentProc,
    visible,
    goAway,
    0,
    "Wordle9",
    0
};

/* ---------------------------------------------------------------------- */
/* Reusable message dialog (New Game confirm, Give Up, Win / Lose, About) */
/* ---------------------------------------------------------------------- */

pascal void ButtonFrameProc(DialogRef dlg, DialogItemIndex itemNo);

resource 'DLOG' (200, "Message") {
    { 80, 130, 260, 530 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    200,
    "",
    centerMainScreen
};

resource 'DITL' (200) {
    {
        { 140, 310, 164, 380 },
        Button { enabled, "OK" };

        { 136, 306, 168, 384 },
        UserItem { enabled };

        { 16, 16, 120, 384 },
        StaticText { disabled, "^0" };
    }
};

/* ---------------------------------------------------------------------- */
/* Process size / capability info                                         */
/* ---------------------------------------------------------------------- */

resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    doesActivateOnFGSwitch,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,
#ifdef TARGET_API_MAC_CARBON
    isHighLevelEventAware,
#else
    notHighLevelEventAware,
#endif
    onlyLocalHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    reserved,
    reserved,
    reserved,
#ifdef TARGET_API_MAC_CARBON
    500 * 1024,    // Carbon apparently needs additional memory.
    500 * 1024
#else
    200 * 1024,
    200 * 1024
#endif
};
