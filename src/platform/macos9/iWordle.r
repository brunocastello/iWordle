/*
 * Resource definitions for iWordle: menu bar, main document window, a
 * message dialog (New Game confirm, Give Up, Win / Lose), and an About
 * dialog.
 *
 * Both are real DLOG/DITL dialogs with a native Button item (for the
 * authentic Platinum push-button look, via GetNewDialog/ModalDialog),
 * plus one UserItem for hand-drawn text content (message text, or the
 * About box's icon/name/credits). The Button item is item 1, so
 * Return/Enter triggers it via the Dialog Manager's normal default-
 * item handling with no custom filter needed. The UserItem's rect
 * never overlaps the button's, so DITL hit-testing (which matches
 * items in index order) can't resolve a button click to the wrong
 * item. The Platinum gray dialog background comes from
 * SetThemeWindowBackground() (Appearance Manager) rather than manual
 * painting, since manually painting it was what broke native
 * StaticText rendering in an earlier iteration.
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
    invisible,
    noGoAway,
    0,
    200,
    "",
    centerMainScreen
};

resource 'DITL' (200) {
    {
        { 98, 135, 122, 205 },
        Button { enabled, "OK" };

        { 0, 0, 76, 340 },
        UserItem { enabled };
    }
};

resource 'DLOG' (201, "About iWordle") {
    { 0, 0, 236, 280 },
    dBoxProc,
    invisible,
    noGoAway,
    0,
    201,
    "",
    centerMainScreen
};

resource 'DITL' (201) {
    {
        { 204, 105, 228, 175 },
        Button { enabled, "OK" };

        { 0, 0, 200, 280 },
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
