/*
 * Resource definitions for iWordle: menu bar, main document window, a
 * message dialog (New Game confirm, Give Up, Win / Lose, 200), and a
 * statistics scoreboard (203).
 *
 * Those are still DLOG/DITL dialogs (real native Button items, tracked
 * via ModalDialog -- no reason to replace a mechanism that already
 * worked), but item 1's UserItem no longer draws anything: it's kept
 * only so main.c can read its rect via GetDialogItem() to lay out real
 * content against, since neither the dialog's background nor its text
 * is hand-drawn anymore. main.c calls SetThemeWindowBackground() on the
 * dialog's window for the native Aqua panel background, and builds the
 * actual message text / stats table out of real Static Text controls
 * (and, for the stats table's divider, a real CreateSeparatorControl())
 * added directly to that window at runtime -- the same techniques used
 * for the About window (201) and the player-name prompt (202). See
 * ShowMessage()/BuildStatsContent() in main.c.
 *
 * The player-name prompt (202) and the About window (201) are NOT
 * DLOG/DITL at all -- they're plain WINDs, built entirely from real
 * native Control Manager controls at runtime, and dismissed via their
 * own event loop rather than ModalDialog. See the comments on WIND (202)
 * below, on WIND (201) below, and on PromptForPlayerName/OnAbout in
 * main.c for why.
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
        "End Game", noIcon, "W", noMark, plain;
        "Ranking...", noIcon, "R", noMark, plain;
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

        /* Standard Aqua push button height (20px); the OS 9 build's
         * hand-drawn default-button ring doesn't exist here -- real
         * Carbon on OS X draws the native pulsing-blue default-button
         * glow itself once SetDialogDefaultItem() marks this item. */
        { 100, 135, 120, 205 },
        Button { enabled, "OK" };
    }
};

/* Plain WIND, not DLOG/DITL -- a real documentProc/goAway window gets
 * the standard Aqua traffic-light title bar for free, matching every
 * other native About window on OS X, and is dismissed by its own close
 * box rather than an OK button. Same reasoning and architecture as WIND
 * 202 (see the comment above it and on OnAbout() in main.c): a plain
 * WaitNextEvent loop with real native controls, no DITL at all. Starts
 * invisible; OnAbout() shows it only once its controls are set up. */
resource 'WIND' (201, "About iWordle") {
    { 0, 0, 205, 280 },
    documentProc,
    invisible,
    goAway,
    0,
    "About iWordle",
    centerMainScreen
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
   Item 1 is an inert UserItem, kept only to define the content rect
   BuildStatsContent() (main.c) lays real Static Text controls and a
   real CreateSeparatorControl() divider against -- see the top-of-file
   comment. Item 2 is a plain "Clear" button (not the default action).
   Item 3 is "OK" (dismiss) -- SetDialogDefaultItem() marks it as the
   native default button, no hand-drawn ring item needed. */
resource 'DLOG' (203, "Ranking") {
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

        { 264, 30, 284, 110 },
        Button { enabled, "Clear" };

        { 264, 320, 284, 390 },
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
