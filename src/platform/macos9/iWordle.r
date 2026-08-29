/*
 * Resource definitions for iWordle: menu bar, main document window, a
 * message dialog (New Game confirm, Give Up, Win / Lose), and an About
 * dialog.
 *
 * Both are DLOG/DITL dialogs with two UserItems each, no native
 * controls: item 1 paints the Platinum gray background (Retro68's
 * headers don't expose the Appearance Manager, so there's no
 * SetThemeWindowBackground to ask for it) and draws the dialog's text
 * content by hand; item 2 is where the OK button's rect lives, drawn
 * by item 1's proc via DrawPlatinumButton() since the native Button
 * DITL item's CDEF in this toolchain doesn't render correctly against
 * a non-white background (missing frame, white corner pixels). Item 2
 * itself is left with no draw proc assigned, so the Dialog Manager
 * never draws anything for it and can't paint over the button -- it
 * only exists so ModalDialog's hit-testing has a clickable rect there.
 * A modal filter proc remaps Return/Enter to item 2, since the Dialog
 * Manager's default-item handling is hardwired to item 1 regardless of
 * what's actually in it.
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

resource 'DLOG' (200, "Message") {
    { 0, 0, 140, 340 },
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
        { 0, 0, 76, 340 },
        UserItem { enabled };

        { 98, 135, 122, 205 },
        UserItem { enabled };
    }
};

resource 'DLOG' (201, "About iWordle") {
    { 0, 0, 256, 280 },
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
        { 0, 0, 214, 280 },
        UserItem { enabled };

        { 224, 105, 248, 175 },
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
