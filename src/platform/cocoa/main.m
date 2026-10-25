/*
 * iWordle - modern macOS (Cocoa) front end, Intel + Apple Silicon.
 *
 * One Objective-C file (ARC, AppKit only, no nibs) over the portable engine
 * in src/core. Behavior mirrors the other ports; the layout uses the same
 * design units as src/platform/macosx/main.c, scaled by one aspect-preserving
 * factor and centered in the (resizable) window. See CLAUDE.md.
 */
#import <Cocoa/Cocoa.h>

#include <string.h>

#include "wordle_engine.h"
#include "wordle_stats.h"

/* ---------------------------------------------------------------------- */
/* Layout (design units, the layout at 100%)                               */
/* ---------------------------------------------------------------------- */

#define MARGIN 14

#define TILE_SIZE      54
#define TILE_GAP       8
#define BOARD_TOP      MARGIN
#define BOARD_LEFT     100

#define KEY_SIZE          42
#define KEY_GAP           6
#define KEY_ROW_GAP       10
#define KEYBOARD_TOP      398
#define KB_ROW0_LEFT      MARGIN
#define KB_ROW1_LEFT      38
#define KB_ROW2_LEFT      86
#define KB_WIDE_KEY_WIDTH 66
#define KB_ROW_COUNT      3

#define DESIGN_W   502
#define LAYOUT_H   544                    /* design-unit bottom of the keyboard */
#define DESIGN_H   (LAYOUT_H + MARGIN)    /* no grow band on modern macOS */
#define MIN_SCALE  0.6

static const char * const kKBRows[KB_ROW_COUNT] = {
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM"
};

/* ---------------------------------------------------------------------- */
/* Globals                                                                 */
/* ---------------------------------------------------------------------- */

static WordleGame gGame;
static WordleStatsBook gStats;
static char gLastName[WORDLE_STATS_NAME_LEN + 1];

static CGFloat gScale = 1.0;
static CGFloat gOffsetX = 0.0;
static CGFloat gOffsetY = 0.0;

static NSWindow *gWindow;
static NSView *gBoardView;

static void UpdateLayout(NSSize size)
{
    CGFloat sx = size.width / DESIGN_W;
    CGFloat sy = size.height / DESIGN_H;
    gScale = sx < sy ? sx : sy;
    if (gScale < MIN_SCALE) gScale = MIN_SCALE;
    gOffsetX = (size.width - DESIGN_W * gScale) / 2.0;
    gOffsetY = (size.height - DESIGN_H * gScale) / 2.0;
}

static NSRect ScaledRect(CGFloat x, CGFloat y, CGFloat w, CGFloat h)
{
    return NSMakeRect(gOffsetX + x * gScale, gOffsetY + y * gScale, w * gScale, h * gScale);
}

static NSRect GetTileRect(int row, int col)
{
    return ScaledRect(BOARD_LEFT + col * (TILE_SIZE + TILE_GAP),
                      BOARD_TOP + row * (TILE_SIZE + TILE_GAP), TILE_SIZE, TILE_SIZE);
}

static int KeyboardRowLeft(int row)
{
    switch (row) {
        case 0: return KB_ROW0_LEFT;
        case 1: return KB_ROW1_LEFT;
        default: return KB_ROW2_LEFT;
    }
}

static NSRect GetLetterKeyRect(int row, int idx)
{
    return ScaledRect(KeyboardRowLeft(row) + idx * (KEY_SIZE + KEY_GAP),
                      KEYBOARD_TOP + row * (KEY_SIZE + KEY_ROW_GAP), KEY_SIZE, KEY_SIZE);
}

static NSRect GetBackspaceKeyRect(void)
{
    return ScaledRect(KB_ROW2_LEFT + 7 * (KEY_SIZE + KEY_GAP),
                      KEYBOARD_TOP + 2 * (KEY_SIZE + KEY_ROW_GAP), KB_WIDE_KEY_WIDTH, KEY_SIZE);
}

static NSRect GetEnterKeyRect(void)
{
    return ScaledRect(KB_ROW2_LEFT - KEY_GAP - KB_WIDE_KEY_WIDTH,
                      KEYBOARD_TOP + 2 * (KEY_SIZE + KEY_ROW_GAP), KB_WIDE_KEY_WIDTH, KEY_SIZE);
}

/* ---------------------------------------------------------------------- */
/* Palette (see CLAUDE.md)                                                 */
/* ---------------------------------------------------------------------- */

static NSColor *PaletteColor(CGFloat r, CGFloat g, CGFloat b)
{
    return [NSColor colorWithSRGBRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:1.0];
}

static NSColor *ColorForState(WordleLetterState s)
{
    switch (s) {
        case kLetterCorrect: return PaletteColor(0x6A, 0xAA, 0x64);
        case kLetterPresent: return PaletteColor(0xC9, 0xB4, 0x58);
        case kLetterAbsent:  return PaletteColor(0x78, 0x7C, 0x7E);
        default:             return nil;
    }
}

/* ---------------------------------------------------------------------- */
/* Stats file                                                              */
/* ---------------------------------------------------------------------- */

static NSString *StatsFilePath(BOOL create)
{
    NSArray *dirs = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,
                                                        NSUserDomainMask, YES);
    if (dirs.count == 0) return nil;
    NSString *folder = [dirs[0] stringByAppendingPathComponent:@"iWordle"];
    if (create) {
        [[NSFileManager defaultManager] createDirectoryAtPath:folder
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:NULL];
    }
    return [folder stringByAppendingPathComponent:@"iWordle Stats"];
}

static void LoadStats(void)
{
    WordleStatsInit(&gStats);

    NSString *path = StatsFilePath(NO);
    if (!path) return;
    NSData *data = [NSData dataWithContentsOfFile:path];
    if (!data || data.length != sizeof(WordleStatsBook)) return;

    WordleStatsBook book;
    memcpy(&book, data.bytes, sizeof(book));
    if (book.playerCount > WORDLE_STATS_MAX_PLAYERS) return;
    for (int i = 0; i < WORDLE_STATS_MAX_PLAYERS; i++)
        book.players[i].name[WORDLE_STATS_NAME_LEN] = '\0';
    gStats = book;
}

static void SaveStats(void)
{
    NSString *path = StatsFilePath(YES);
    if (!path) return;
    NSData *data = [NSData dataWithBytes:&gStats length:sizeof(gStats)];
    [data writeToFile:path atomically:YES];
}

/* ---------------------------------------------------------------------- */
/* Ranking                                                                 */
/* ---------------------------------------------------------------------- */

@interface RankingController : NSObject <NSTableViewDataSource, NSTableViewDelegate>
@property (nonatomic, strong) NSPanel *panel;
@property (nonatomic, strong) NSTableView *table;
@end

@implementation RankingController

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tv
{
    return gStats.playerCount;
}

- (NSView *)tableView:(NSTableView *)tv viewForTableColumn:(NSTableColumn *)col row:(NSInteger)row
{
    if (row < 0 || row >= gStats.playerCount) return nil;
    const WordlePlayerStats *p = &gStats.players[row];
    NSString *ident = col.identifier;
    NSString *text;
    BOOL right = YES;

    if ([ident isEqualToString:@"name"]) {
        text = [NSString stringWithUTF8String:p->name] ?: @"";
        right = NO;
    } else if ([ident isEqualToString:@"played"]) {
        text = [NSString stringWithFormat:@"%u", p->gamesPlayed];
    } else if ([ident isEqualToString:@"win"]) {
        unsigned pct = p->gamesPlayed ? (unsigned)(((unsigned long)p->gamesWon * 100UL) / p->gamesPlayed) : 0;
        text = [NSString stringWithFormat:@"%u%%", pct];
    } else if ([ident isEqualToString:@"cur"]) {
        text = [NSString stringWithFormat:@"%u", p->currentStreak];
    } else {
        text = [NSString stringWithFormat:@"%u", p->maxStreak];
    }

    NSTextField *field = [NSTextField labelWithString:text];
    field.alignment = right ? NSTextAlignmentRight : NSTextAlignmentLeft;
    field.lineBreakMode = NSLineBreakByTruncatingTail;
    return field;
}

- (void)okPressed:(id)sender
{
    NSWindow *parent = self.panel.sheetParent;
    if (parent) [parent endSheet:self.panel];
    else [self.panel orderOut:nil];
}

- (void)clearPressed:(id)sender
{
    WordleStatsClear(&gStats);
    SaveStats();
    [self.table reloadData];
}

@end

static RankingController *gRanking;

static NSTableColumn *MakeColumn(NSString *ident, NSString *title, CGFloat width, BOOL right)
{
    NSTableColumn *c = [[NSTableColumn alloc] initWithIdentifier:ident];
    c.title = title;
    c.width = width;
    c.headerCell.alignment = right ? NSTextAlignmentRight : NSTextAlignmentLeft;
    return c;
}

static void ShowRanking(void)
{
    RankingController *rc = [[RankingController alloc] init];
    gRanking = rc;

    NSPanel *panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 420, 300)
                                                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    panel.title = @"Ranking";
    panel.releasedWhenClosed = NO;
    rc.panel = panel;

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 56, 380, 224)];
    scroll.hasVerticalScroller = YES;
    scroll.borderType = NSBezierBorder;

    NSTableView *table = [[NSTableView alloc] initWithFrame:scroll.bounds];
    [table addTableColumn:MakeColumn(@"name", @"Name", 140, NO)];
    [table addTableColumn:MakeColumn(@"played", @"Played", 60, YES)];
    [table addTableColumn:MakeColumn(@"win", @"Win %", 60, YES)];
    [table addTableColumn:MakeColumn(@"cur", @"Cur", 50, YES)];
    [table addTableColumn:MakeColumn(@"max", @"Max", 50, YES)];
    table.dataSource = rc;
    table.delegate = rc;
    table.allowsColumnReordering = NO;
    rc.table = table;
    scroll.documentView = table;
    [panel.contentView addSubview:scroll];

    NSButton *clear = [NSButton buttonWithTitle:@"Clear" target:rc action:@selector(clearPressed:)];
    clear.frame = NSMakeRect(20, 16, 80, 28);
    [panel.contentView addSubview:clear];

    NSButton *ok = [NSButton buttonWithTitle:@"OK" target:rc action:@selector(okPressed:)];
    ok.frame = NSMakeRect(320, 16, 80, 28);
    ok.keyEquivalent = @"\r";
    [panel.contentView addSubview:ok];

    [gWindow beginSheet:panel completionHandler:^(NSModalResponse r) {
        [panel orderOut:nil];
        gRanking = nil;
    }];
}

/* ---------------------------------------------------------------------- */
/* Messages and name prompt                                                */
/* ---------------------------------------------------------------------- */

static void ShowMessage(NSString *text, void (^done)(void))
{
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = text;
    [alert addButtonWithTitle:@"OK"];
    [alert beginSheetModalForWindow:gWindow completionHandler:^(NSModalResponse r) {
        if (done) dispatch_async(dispatch_get_main_queue(), done);
    }];
}

/* Copies s into out as UTF-8, at most WORDLE_STATS_NAME_LEN bytes, never
 * cutting a multi-byte sequence in half. */
static void CopyNameTruncated(NSString *s, char *out)
{
    const char *u = s.UTF8String;
    size_t n = u ? strlen(u) : 0;
    if (n > WORDLE_STATS_NAME_LEN) {
        n = WORDLE_STATS_NAME_LEN;
        while (n > 0 && ((unsigned char)u[n] & 0xC0) == 0x80) n--;
    }
    if (n) memcpy(out, u, n);
    out[n] = '\0';
}

static void PromptAndRecord(BOOL won)
{
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Who's playing?";
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

    NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 220, 24)];
    field.stringValue = [NSString stringWithUTF8String:gLastName] ?: @"";
    alert.accessoryView = field;
    [alert layout];
    alert.window.initialFirstResponder = field;

    [alert beginSheetModalForWindow:gWindow completionHandler:^(NSModalResponse r) {
        if (r != NSAlertFirstButtonReturn) return;
        NSString *name = [field.stringValue stringByTrimmingCharactersInSet:
                          [NSCharacterSet whitespaceAndNewlineCharacterSet]];
        char cname[WORDLE_STATS_NAME_LEN + 1];
        CopyNameTruncated(name, cname);
        if (cname[0] == '\0') return;

        WordleStatsRecordResult(&gStats, cname, won ? 1 : 0);
        strcpy(gLastName, cname);
        SaveStats();
        dispatch_async(dispatch_get_main_queue(), ^{ ShowRanking(); });
    }];
}

static BOOL SheetShowing(void)
{
    return gWindow.attachedSheet != nil;
}

/* ---------------------------------------------------------------------- */
/* Game actions                                                            */
/* ---------------------------------------------------------------------- */

static void Redraw(void)
{
    gBoardView.needsDisplay = YES;
}

static void FinishGame(BOOL won)
{
    NSString *msg;
    if (won)
        msg = [NSString stringWithFormat:@"You got it in %d guess(es)!", gGame.currentRow + 1];
    else
        msg = [NSString stringWithFormat:@"Out of guesses! The word was %s.", gGame.target];
    ShowMessage(msg, ^{ PromptAndRecord(won); });
}

static void OnLetterKey(char c)
{
    if (gGame.status != kGameInProgress || SheetShowing()) return;
    WordleTypeLetter(&gGame, c);
    Redraw();
}

static void OnBackspace(void)
{
    if (gGame.status != kGameInProgress || SheetShowing()) return;
    WordleBackspace(&gGame);
    Redraw();
}

static void OnSubmit(void)
{
    if (gGame.status != kGameInProgress || SheetShowing()) return;

    switch (WordleSubmitGuess(&gGame)) {
        case kSubmitTooShort:
            NSBeep();
            break;
        case kSubmitNotInDictionary:
            NSBeep();
            ShowMessage(@"Not in word list.", nil);
            break;
        case kSubmitOk:
            Redraw();
            if (gGame.status == kGameWon) FinishGame(YES);
            else if (gGame.status == kGameLost) FinishGame(NO);
            break;
        default:
            break;
    }
}

static void OnNewGame(void)
{
    WordleNewGame(&gGame);
    Redraw();
}

static void OnEndGame(void)
{
    if (gGame.status != kGameInProgress) return;
    gGame.status = kGameLost;
    Redraw();
    FinishGame(NO);
}

/* ---------------------------------------------------------------------- */
/* Board view                                                              */
/* ---------------------------------------------------------------------- */

@interface BoardView : NSView
@end

@implementation BoardView

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

static void DrawCentered(NSString *text, NSRect r, CGFloat fontSize, NSColor *color)
{
    NSDictionary *attrs = @{
        NSFontAttributeName: [NSFont boldSystemFontOfSize:fontSize],
        NSForegroundColorAttributeName: color
    };
    NSSize sz = [text sizeWithAttributes:attrs];
    NSPoint p = NSMakePoint(NSMidX(r) - sz.width / 2.0, NSMidY(r) - sz.height / 2.0);
    [text drawAtPoint:p withAttributes:attrs];
}

static void DrawTile(int row, int col)
{
    NSRect r = GetTileRect(row, col);
    const WordleRow *wr = &gGame.rows[row];
    WordleLetterState st = wr->states[col];
    NSColor *fill = ColorForState(st);
    char letter = 0;

    if (fill) {
        letter = wr->letters[col];
    } else {
        fill = NSColor.whiteColor;
        if (row == gGame.currentRow && col < gGame.currentCol) letter = wr->letters[col];
    }

    [fill setFill];
    NSRectFill(r);
    [PaletteColor(0x33, 0x33, 0x33) setStroke];
    NSBezierPath *frame = [NSBezierPath bezierPathWithRect:NSInsetRect(r, 0.5, 0.5)];
    frame.lineWidth = 1.0;
    [frame stroke];

    if (letter) {
        NSString *s = [NSString stringWithFormat:@"%c", letter];
        DrawCentered(s, r, 28 * gScale,
                     ColorForState(st) ? NSColor.whiteColor : PaletteColor(0x33, 0x33, 0x33));
    }
}

static void DrawKey(NSRect r, NSString *label, WordleLetterState st, CGFloat fontSize)
{
    NSColor *face = ColorForState(st);
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, 0.5, 0.5)
                                                         xRadius:4 * gScale
                                                         yRadius:4 * gScale];
    [(face ? face : NSColor.controlColor) setFill];
    [path fill];
    [NSColor.separatorColor setStroke];
    path.lineWidth = 1.0;
    [path stroke];
    DrawCentered(label, r, fontSize * gScale, face ? NSColor.whiteColor : NSColor.controlTextColor);
}

- (void)drawRect:(NSRect)dirty
{
    UpdateLayout(self.bounds.size);

    [NSColor.windowBackgroundColor setFill];
    NSRectFill(self.bounds);

    for (int r = 0; r < WORDLE_MAX_GUESSES; r++)
        for (int c = 0; c < WORDLE_WORD_LENGTH; c++)
            DrawTile(r, c);

    for (int r = 0; r < KB_ROW_COUNT; r++) {
        const char *row = kKBRows[r];
        for (int i = 0; row[i]; i++) {
            NSString *label = [NSString stringWithFormat:@"%c", row[i]];
            DrawKey(GetLetterKeyRect(r, i), label, gGame.keyStates[row[i] - 'A'], 16);
        }
    }
    DrawKey(GetEnterKeyRect(), @"ENTER", kLetterEmpty, 14);
    DrawKey(GetBackspaceKeyRect(), @"BACK", kLetterEmpty, 14);
}

- (void)mouseDown:(NSEvent *)event
{
    UpdateLayout(self.bounds.size);
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];

    if (NSPointInRect(p, GetEnterKeyRect())) { OnSubmit(); return; }
    if (NSPointInRect(p, GetBackspaceKeyRect())) { OnBackspace(); return; }
    for (int r = 0; r < KB_ROW_COUNT; r++) {
        for (int i = 0; kKBRows[r][i]; i++) {
            if (NSPointInRect(p, GetLetterKeyRect(r, i))) {
                OnLetterKey(kKBRows[r][i]);
                return;
            }
        }
    }
}

- (void)keyDown:(NSEvent *)event
{
    NSString *chars = event.characters;
    if (chars.length != 1 ||
        (event.modifierFlags & (NSEventModifierFlagCommand | NSEventModifierFlagControl))) {
        [super keyDown:event];
        return;
    }
    unichar ch = [chars characterAtIndex:0];

    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
        OnLetterKey((char)((ch >= 'a') ? ch - 'a' + 'A' : ch));
    } else if (ch == '\r' || ch == 0x03) {
        if (!event.isARepeat) OnSubmit();
    } else if (ch == 0x7F || ch == NSDeleteFunctionKey) {
        OnBackspace();
    } else {
        [super keyDown:event];
    }
}

@end

/* ---------------------------------------------------------------------- */
/* App delegate and menus                                                  */
/* ---------------------------------------------------------------------- */

@interface AppDelegate : NSObject <NSApplicationDelegate, NSMenuItemValidation>
@end

@implementation AppDelegate

- (NSMenuItem *)addItem:(NSString *)title action:(SEL)action key:(NSString *)key
                 target:(id)target toMenu:(NSMenu *)menu
{
    NSMenuItem *item = [menu addItemWithTitle:title action:action keyEquivalent:key];
    item.target = target;
    return item;
}

- (void)buildMenus
{
    NSMenu *bar = [[NSMenu alloc] init];
    NSMenuItem *item;

    /* App menu */
    item = [bar addItemWithTitle:@"" action:nil keyEquivalent:@""];
    NSMenu *app = [[NSMenu alloc] initWithTitle:@"iWordle"];
    [self addItem:@"About iWordle" action:@selector(showAbout:) key:@"" target:self toMenu:app];
    [app addItem:[NSMenuItem separatorItem]];
    [self addItem:@"Hide iWordle" action:@selector(hide:) key:@"h" target:NSApp toMenu:app];
    item = [self addItem:@"Hide Others" action:@selector(hideOtherApplications:) key:@"h"
                  target:NSApp toMenu:app];
    item.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
    [self addItem:@"Show All" action:@selector(unhideAllApplications:) key:@"" target:NSApp toMenu:app];
    [app addItem:[NSMenuItem separatorItem]];
    [self addItem:@"Quit iWordle" action:@selector(terminate:) key:@"q" target:NSApp toMenu:app];
    [bar itemAtIndex:0].submenu = app;

    /* Game menu */
    item = [bar addItemWithTitle:@"Game" action:nil keyEquivalent:@""];
    NSMenu *game = [[NSMenu alloc] initWithTitle:@"Game"];
    [self addItem:@"New Game" action:@selector(newGame:) key:@"n" target:self toMenu:game];
    [self addItem:@"End Game" action:@selector(endGame:) key:@"w" target:self toMenu:game];
    [game addItem:[NSMenuItem separatorItem]];
    [self addItem:@"Ranking…" action:@selector(showRanking:) key:@"r" target:self toMenu:game];
    item.submenu = game;

    /* Window menu */
    item = [bar addItemWithTitle:@"Window" action:nil keyEquivalent:@""];
    NSMenu *win = [[NSMenu alloc] initWithTitle:@"Window"];
    [win addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [win addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    item.submenu = win;

    NSApp.mainMenu = bar;
    NSApp.windowsMenu = win;
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSApp.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];

    [self buildMenus];

    LoadStats();
    WordleSeedRandom((unsigned long)arc4random());
    WordleNewGame(&gGame);

    NSRect content = NSMakeRect(0, 0, DESIGN_W, DESIGN_H);
    gWindow = [[NSWindow alloc] initWithContentRect:content
                                          styleMask:NSWindowStyleMaskTitled |
                                                    NSWindowStyleMaskClosable |
                                                    NSWindowStyleMaskMiniaturizable |
                                                    NSWindowStyleMaskResizable
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    gWindow.title = @"iWordle";
    gWindow.releasedWhenClosed = NO;
    gWindow.contentMinSize = NSMakeSize(DESIGN_W * MIN_SCALE, DESIGN_H * MIN_SCALE);

    BoardView *view = [[BoardView alloc] initWithFrame:content];
    gBoardView = view;
    gWindow.contentView = view;
    [gWindow makeFirstResponder:view];

    [gWindow center];
    [gWindow setFrameAutosaveName:@"iWordleMain"];
    [gWindow makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
    return YES;
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    SEL a = item.action;
    if (a == @selector(endGame:)) return gGame.status == kGameInProgress && !SheetShowing();
    if (a == @selector(newGame:) || a == @selector(showRanking:)) return !SheetShowing();
    return YES;
}

- (void)newGame:(id)sender { OnNewGame(); }
- (void)endGame:(id)sender { OnEndGame(); }
- (void)showRanking:(id)sender { ShowRanking(); }

- (void)showAbout:(id)sender
{
    NSMutableParagraphStyle *para = [[NSMutableParagraphStyle alloc] init];
    para.alignment = NSTextAlignmentCenter;
    NSDictionary *attrs = @{
        NSFontAttributeName: [NSFont systemFontOfSize:[NSFont smallSystemFontSize]],
        NSForegroundColorAttributeName: NSColor.textColor,
        NSParagraphStyleAttributeName: para
    };
    NSAttributedString *credits = [[NSAttributedString alloc]
        initWithString:@"A native Wordle clone for macOS\nBruno Castello\nbfcastello@hotmail.com\nEngineer: Claude Sonnet 5"
            attributes:attrs];
    [NSApp orderFrontStandardAboutPanelWithOptions:@{
        NSAboutPanelOptionCredits: credits,
        NSAboutPanelOptionApplicationVersion: @"1.0"
    }];
}

@end

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        static AppDelegate *delegate;   /* NSApp.delegate is weak: keep it alive */
        delegate = [[AppDelegate alloc] init];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
