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
    { 0, 0, 558, 502 },
    documentProc,
    visible,
    goAway,
    0,
    "iWordle",
    centerMainScreen
};

/* ---------------------------------------------------------------------- */
/* Small message dialog (New Game confirm, Give Up, Win / Lose)           */
/*                                                                        */
/* Item 1 is a UserItem covering the whole dialog that paints our own     */
/* Platinum gray background on every redraw (Carbon's Appearance         */
/* Manager doesn't pick up RGBBackColor for these classic dialogs), so    */
/* it must be drawn before the button/text items that sit on top of it.   */
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
        { 0, 0, 120, 300 },
        UserItem { enabled };

        { 80, 214, 104, 284 },
        Button { enabled, "OK" };

        { 12, 12, 70, 288 },
        StaticText { disabled, "^0" };
    }
};

/* ---------------------------------------------------------------------- */
/* About box: custom-drawn content in a plain (no title bar) modal box    */
/* ---------------------------------------------------------------------- */

resource 'DLOG' (201, "About iWordle") {
    { 130, 170, 375, 450 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    201,
    "",
    centerMainScreen
};

resource 'DITL' (201) {
    {
        { 0, 0, 245, 280 },
        UserItem { enabled };

        { 210, 110, 234, 170 },
        Button { enabled, "OK" };
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
