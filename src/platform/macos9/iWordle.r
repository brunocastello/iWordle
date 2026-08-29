/*
 * Resource definitions for iWordle: menu bar, main document window, a
 * message dialog (New Game confirm, Give Up, Win / Lose), an About
 * dialog, and a statistics scoreboard (203).
 *
 * Those are DLOG/DITL dialogs where item 1 is a UserItem that paints the
 * Platinum gray background (Retro68's default open-source Multiversal
 * Interfaces don't expose the Appearance Manager, so there's no
 * SetThemeWindowBackground to ask for it -- this project now links
 * against Apple's real Universal Interfaces instead, see
 * third_party/InterfacesAndLibraries, but these dialogs were built before
 * that and never revisited since the plain gray fill already works fine)
 * and draws that dialog's text/table content by hand, and the dialog's
 * default button is a real native Button immediately followed by a
 * UserItem that draws the classic bold rounded frame marking it as the
 * default -- the standard technique that distinguishes an OK button from
 * a plain Cancel/Clear button in a native alert. SetDialogDefaultItem()
 * was tried as a way to get that emphasis for free from the Dialog Manager
 * instead, but rendered far worse (the button barely appeared at all) than
 * this hand-drawn frame does, so this is the version we're keeping. The
 * frame's UserItem proc reads the button's box off "itemNo - 1" (the item
 * right before it), which is why every DLOG/DITL dialog here keeps its
 * default button immediately followed by its ring UserItem in the item
 * list.
 *
 * The player-name prompt (202) is NOT one of these -- it's a plain WIND
 * with no DITL at all, since it needed real native Control Manager
 * controls (an Edit Text field) that ModalDialog doesn't cooperate with
 * well. See the comment on WIND (202) below and on PromptForPlayerName
 * in main.c for why.
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
        "Statistics...", noIcon, noKey, noMark, plain;
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

        { 93, 130, 127, 210 },
        UserItem { disabled };
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

        { 237, 100, 271, 180 },
        UserItem { disabled };
    }
};

/* ---- Player name entry (WIND 202) ----
   Shown after every win/loss to attribute the result to a player. This
   is a plain window, not a DLOG/DITL -- every bug hit getting a real
   native Edit Text control to accept keystrokes traced back to some
   ModalDialog behavior working against a control it doesn't know about
   (auto update handling that skips DrawControls, no guaranteed port
   before a keyDown, etc.). PromptForPlayerName (main.c) creates its own
   Edit Text (kControlEditTextProc) and OK (kControlPushButtonProc)
   controls at runtime and runs its own small WaitNextEvent loop, the
   same architecture the main game window already uses successfully.
   Both control types need the real Control Manager, which this
   project's Retro68 toolchain now links against Apple's real Universal
   Interfaces for (see third_party/InterfacesAndLibraries and
   .github/workflows/build.yml) instead of the open-source Multiversal
   reimplementation, which never had any of it. Starts invisible;
   PromptForPlayerName shows it only once its controls are fully set up. */
resource 'WIND' (202, "Who's Playing?") {
    { 0, 0, 160, 320 },
    dBoxProc,
    invisible,
    noGoAway,
    0,
    "",
    centerMainScreen
};

/* ---- Statistics scoreboard (DLOG 203 + DITL 203) ----
   Item 1 is the background UserItem, which also hand-draws the table of
   every recorded player (name/played/win %/current streak/max streak),
   same technique the message/about dialogs use to draw their text. Item
   2 is a plain "Clear" button (no ring -- it's not the default action).
   Item 3 is "OK" (dismiss), item 4 its ButtonFrameProc ring. */
resource 'DLOG' (203, "Statistics") {
    { 0, 0, 300, 420 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    203,
    "",
    centerMainScreen
};

resource 'DITL' (203) {
    {
        { 0, 0, 250, 420 },
        UserItem { enabled };

        { 262, 30, 286, 110 },
        Button { enabled, "Clear" };

        { 262, 320, 286, 390 },
        Button { enabled, "OK" };

        { 258, 316, 290, 394 },
        UserItem { disabled };
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
