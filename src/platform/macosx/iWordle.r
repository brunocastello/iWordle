/*
 * Resource definitions for iWordle: menu bar, main document window, and
 * four dialog-style windows (message box 200, About 201, player-name
 * prompt 202, statistics scoreboard 203).
 *
 * None of the four are DLOG/DITL -- they're all plain WINDs, built
 * entirely from real native Control Manager controls at runtime (Static
 * Text, Buttons, an Edit Text field, a Separator line), and dismissed
 * via their own small WaitNextEvent loop rather than ModalDialog. This
 * used to be DLOG/DITL for 200 and 203, tracked via ModalDialog, with
 * SetThemeWindowBackground() called on the dialog's underlying window
 * for its native Aqua panel background -- but ModalDialog's window
 * apparently doesn't respect that call the way a plain Window-Manager
 * window does (confirmed by screenshot: still plain white), matching
 * why the player-name prompt already had to abandon ModalDialog for its
 * Edit Text field (see the comment on WIND 202 below). Converting 200
 * and 203 to the same plain-WIND architecture fixed it. See
 * ShowMessage()/OnStatistics() in main.c.
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

/* noGrowDocProc, not documentProc -- the board/keyboard layout is a
 * fixed size (see MARGIN etc. in main.c), so this was never meant to be
 * resizable. ChangeWindowAttributes(win, 0, kWindowResizableAttribute)
 * in main.c removes the functional resizing, but documentProc's own
 * frame still rendered a grow-box-shaped chrome element in the corner
 * regardless (a white square left over where the resize handle used to
 * be, confirmed by screenshot) -- noGrowDocProc is the classic WDEF
 * variant that never draws that chrome at all, fixing it at the root
 * instead of trying to suppress it at runtime. */
resource 'WIND' (128, "iWordle") {
    { 0, 0, 558, 502 },
    noGrowDocProc,
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

/* Plain WIND, not DLOG/DITL -- a real noGrowDocProc/goAway window gets
 * the standard Aqua traffic-light title bar for free, matching every
 * other native About window on OS X, and is dismissed by its own close
 * box rather than an OK button. noGrowDocProc rather than documentProc
 * for the same reason as WIND (128) above -- no grow-box chrome for a
 * fixed-size window. Same reasoning and architecture as WIND 202 (see
 * the comment above it and on OnAbout() in main.c): a plain
 * WaitNextEvent loop with real native controls, no DITL at all. Starts
 * invisible; OnAbout() shows it only once its controls are set up. */
resource 'WIND' (201, "About iWordle") {
    { 0, 0, 205, 280 },
    noGrowDocProc,
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

/* ---- Statistics scoreboard (WIND 203) ----
   OnStatistics() (main.c) builds the header row, divider, and every
   player row out of real Static Text controls and a real
   CreateSeparatorControl() divider (BuildStatsContent()), plus real
   "Clear" and "OK" push buttons -- all added to this window at runtime,
   same architecture as the message box (200) above. */
resource 'WIND' (203, "Ranking") {
    { 0, 0, 300, 420 },
    dBoxProc,
    invisible,
    noGoAway,
    0,
    "Ranking",
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
