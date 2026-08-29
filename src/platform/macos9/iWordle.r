/*
 * Resource definitions for iWordle: menu bar, main document window, a
 * small default-Platinum message dialog (New Game confirm, Give Up,
 * Win / Lose), and a custom-drawn About box.
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
    { 1, 128 };
};

resource 'MENU' (1, preload) {
    1,
    textMenuProc,
    allEnabled,
    enabled,
    apple,
    {
        "About iWordle...", noIcon, noKey, noMark, plain;
    }
};

resource 'MENU' (128, "File") {
    128,
    textMenuProc,
    allEnabled,
    enabled,
    "File",
    {
        "New Game", noIcon, "N", noMark, plain;
        "Give Up", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Quit", noIcon, "Q", noMark, plain;
    }
};

/* ---------------------------------------------------------------------- */
/* Main document window                                                   */
/* ---------------------------------------------------------------------- */

resource 'WIND' (128, "iWordle") {
    { 60, 80, 618, 582 },
    documentProc,
    visible,
    goAway,
    0,
    "iWordle",
    0
};

/* ---------------------------------------------------------------------- */
/* Small message dialog (New Game confirm, Give Up, Win / Lose)           */
/*                                                                        */
/* Plain Button + StaticText items only, so Mac OS 9's Appearance         */
/* Manager renders them with the standard Platinum look, rather than      */
/* any custom-drawn chrome.                                               */
/* ---------------------------------------------------------------------- */

resource 'DLOG' (200, "Message") {
    { 100, 180, 220, 480 },
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
        { 80, 214, 104, 284 },
        Button { enabled, "OK" };

        { 12, 12, 70, 288 },
        StaticText { disabled, "^0" };
    }
};

/* ---------------------------------------------------------------------- */
/* About box: custom-drawn content in a native movable modal window       */
/* ---------------------------------------------------------------------- */

resource 'DLOG' (201, "About iWordle") {
    { 130, 170, 325, 450 },
    movableDBoxProc,
    visible,
    noGoAway,
    0,
    201,
    "About iWordle...",
    centerMainScreen
};

resource 'DITL' (201) {
    {
        { 162, 110, 186, 170 },
        Button { enabled, "OK" };

        { 0, 0, 155, 280 },
        UserItem { enabled };
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
