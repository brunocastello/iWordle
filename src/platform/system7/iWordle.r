/*
 * Resource definitions for the System 7 / 68k build of iWordle: menu bar,
 * main document window, About window, a message alert (200), the
 * "needs System 7" alert (129), a player-name prompt (DLOG 202) and a
 * statistics scoreboard (DLOG 203).
 *
 * Unlike the Mac OS X build (which must avoid DLOG/DITL because its
 * dialogs have to be real Carbon controls) and the Mac OS 9 build (which
 * hand-draws its Platinum look), the classic Dialog Manager is the native,
 * correct tool on System 7, so the dialogs here are plain Alert / DLOG +
 * DITL resources driven by NoteAlert and ModalDialog. The only hand-drawn
 * DITL items are UserItems: the bold rounded ring that marks the default
 * button (the last item of each DLOG) and the ranking table.
 *
 * Every DITL keeps its default button as item 1 -- Return and Enter always
 * map to item 1 in the Dialog Manager, with no filter proc needed.
 *
 * Window sizes assume the 512x342 screen of a Macintosh SE/30 with a 20px
 * menu bar.
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
    { 1, 128, 129 };
};

/* Item 1 is About; item 2 is a separator. main.c appends the installed
 * desk accessories after it with AppendResMenu(menu, 'DRVR'), and treats
 * every item from 3 on as one. */
resource 'MENU' (1, preload) {
    1,
    textMenuProc,
    allEnabled,
    enabled,
    apple,
    {
        "About iWordle...", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
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
        "End Game", noIcon, "W", noMark, plain;
        "Ranking...", noIcon, "R", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Quit", noIcon, "Q", noMark, plain;
    }
};

/* Standard Edit menu, here only for desk accessories: every item is
 * routed to SystemEdit(item - 1) in main.c and otherwise ignored. */
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

/* ---------------------------------------------------------------------- */
/* Main document window                                                   */
/* ---------------------------------------------------------------------- */

/* Explicit bounds, content 304 wide x 293 tall (278 of layout + the 15px grow-box band): fits 512x342 with the menu
 * bar (20) and title bar (~19). This is the 100% size: DESIGN_W / DESIGN_H in
 * main.c depend on it. The window is resizable (zoomDocProc: grow box and
 * zoom box) and the board scales with it. */
resource 'WIND' (128, "iWordle") {
    { 40, 104, 333, 408 },
    zoomDocProc,
    visible,
    goAway,
    0,
    "iWordle",
    noAutoCenter
};

/* About window: a plain WIND with a close box, no OK button -- standard
 * About Box convention. Starts invisible; OnAbout() centers it using
 * qd.screenBits.bounds and shows it. */
resource 'WIND' (201, "About iWordle") {
    { 0, 0, 220, 260 },
    noGrowDocProc,
    invisible,
    goAway,
    0,
    "About iWordle",
    noAutoCenter
};

/* ---------------------------------------------------------------------- */
/* Message alert (Not in word list / Win / Lose) -- ALRT/DITL 200          */
/* ---------------------------------------------------------------------- */

resource 'ALRT' (200, "Message") {
    { 0, 0, 110, 300 },
    200,
    silentStages,
    alertPositionMainScreen
};

/* Item 1 OK, item 2 the message (main.c passes it via ParamText "^0"). The
 * text starts at 60 to leave room for the icon the Alert Manager draws at
 * the alert's top-left. */
resource 'DITL' (200) {
    {
        { 78, 226, 98, 286 },
        Button { enabled, "OK" };

        { 12, 60, 68, 288 },
        StaticText { disabled, "^0" };
    }
};

/* ---------------------------------------------------------------------- */
/* "Requires System 7" alert -- ALRT/DITL 129                              */
/* ---------------------------------------------------------------------- */

resource 'ALRT' (129, "Needs System 7") {
    { 0, 0, 100, 300 },
    129,
    silentStages,
    alertPositionMainScreen
};

resource 'DITL' (129) {
    {
        { 68, 226, 88, 286 },
        Button { enabled, "OK" };

        { 12, 60, 60, 288 },
        StaticText { disabled, "iWordle requires System 7 or later." };
    }
};

/* ---------------------------------------------------------------------- */
/* Player name entry -- DLOG/DITL 202                                      */
/* ---------------------------------------------------------------------- */

/* Shown after every win/loss to attribute the result to a player. Starts
 * invisible so main.c can prefill the field before showing it. */
resource 'DLOG' (202, "Who's Playing?") {
    { 0, 0, 120, 280 },
    dBoxProc,
    invisible,
    noGoAway,
    0,
    202,
    "",
    alertPositionMainScreen
};

/* Item 1 OK, 2 prompt, 3 name field, 4 the default-button ring UserItem
 * (its box is item 1's box grown by 4 on every side). */
resource 'DITL' (202) {
    {
        { 86, 204, 106, 264 },
        Button { enabled, "OK" };

        { 14, 14, 32, 266 },
        StaticText { disabled, "Who's playing?" };

        { 38, 14, 58, 266 },
        EditText { enabled, "" };

        { 82, 200, 110, 268 },
        UserItem { disabled };
    }
};

/* ---------------------------------------------------------------------- */
/* Ranking -- DLOG/DITL 203                                                */
/* ---------------------------------------------------------------------- */

resource 'DLOG' (203, "Ranking") {
    { 0, 0, 250, 400 },
    dBoxProc,
    invisible,
    noGoAway,
    0,
    203,
    "",
    alertPositionMainScreen
};

/* Item 1 OK (default), 2 Clear, 3 the table UserItem (hand-drawn rows),
 * 4 the default-button ring UserItem. */
resource 'DITL' (203) {
    {
        { 216, 326, 236, 386 },
        Button { enabled, "OK" };

        { 216, 14, 236, 84 },
        Button { enabled, "Clear" };

        { 0, 0, 206, 400 },
        UserItem { disabled };

        { 212, 322, 240, 390 },
        UserItem { disabled };
    }
};

/* ---------------------------------------------------------------------- */
/* Version / process size                                                  */
/* ---------------------------------------------------------------------- */

resource 'vers' (1) {
    0x01, 0x00, release, 0x00,
    0,
    "1.0",
    "1.0, \0xA9 Castello Designs, 2026"
};

/* 768 K preferred / 512 K minimum: the word lists are ~150 KB of static
 * data on 68k. */
resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    doesActivateOnFGSwitch,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreAppDiedEvents,
    is32BitCompatible,
    isHighLevelEventAware,
    onlyLocalHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    notDisplayManagerAware,
    reserved,
    reserved,
    768 * 1024,
    512 * 1024
};
