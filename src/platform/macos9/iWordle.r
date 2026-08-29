/*
 * Resource definitions for iWordle: menu bar, main document window, a
 * small custom-drawn message window (New Game confirm, Give Up, Win /
 * Lose), and a custom-drawn About window.
 *
 * The message and About windows are plain WIND resources, not Dialog
 * Manager DLOG/DITL dialogs: we draw their entire content (background,
 * text, and the OK button) ourselves and run our own small event loop
 * for them, exactly like the main game window already does. This
 * matches how classic Mac apps commonly built this kind of window --
 * confirmed by inspecting a real one (OS9Map's About window is a plain
 * WIND, procID dBoxProc, no DLOG/DITL/CNTL at all) -- and sidesteps a
 * string of Dialog Manager/Appearance Manager quirks we hit trying to
 * theme and lay out DLOG-based dialogs (backgrounds not repainting,
 * overlapping DITL items swallowing clicks, StaticText not rendering
 * once a background UserItem was added, native controls erasing their
 * corners against the wrong background color).
 */

#include "Types.r"
#include "Windows.r"
#include "Menus.r"
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

resource 'WIND' (200, "Message") {
    { 0, 0, 140, 340 },
    dBoxProc,
    invisible,
    noGoAway,
    0,
    "",
    centerMainScreen
};

resource 'WIND' (201, "About iWordle") {
    { 0, 0, 236, 280 },
    dBoxProc,
    invisible,
    noGoAway,
    0,
    "",
    centerMainScreen
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
