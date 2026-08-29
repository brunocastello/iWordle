/*
 * Resource definitions for iWordle: menu bar, main document window, a
 * message dialog (New Game confirm, Give Up, Win / Lose), and an About
 * dialog.
 *
 * Both are DLOG/DITL dialogs with two items: item 1 is a UserItem that
 * paints the Platinum gray background (Retro68's headers don't expose
 * the Appearance Manager, so there's no SetThemeWindowBackground to ask
 * for it) and draws the dialog's text content by hand; item 2 is a
 * real native Button, "OK", marked as the dialog's default item in
 * code via SetDialogDefaultItem() so the Dialog Manager draws its own
 * native default-button emphasis. An earlier version hand-drew a bold
 * frame around the button in a third item to fake that emphasis, which
 * fought the Dialog Manager's own default-item rendering (it redraws
 * itself independently, e.g. to animate) and could never be made to
 * render cleanly -- going fully native instead is both simpler and is
 * literally what "native button" means.
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
        Button { enabled, "OK" };
    }
};

resource 'DLOG' (201, "About iWordle") {
    { 0, 0, 280, 280 },
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
        { 0, 0, 222, 280 },
        UserItem { enabled };

        { 242, 105, 266, 175 },
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
