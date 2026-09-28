// CPCVirtualKeyboard.mm
//
// Engine: SDL2 2.0.9 (UIKit backend) | iOS 9.3 | armv7 | ARC | C++17/ObjC++
//
// Design notes
// ------------
// * Separate UIWindow above SDL's window. We never touch SDL's own
//   view/view-controller, so SDL's rendering and touch->mouse path are
//   left alone.
// * The emulator core maps *base key + modifier*, not characters:
//   '@' is SDLK_2|SHIFT, '!' is SDLK_1|SHIFT, etc. (see
//   SDLkeysymFromCPCkeys_us in src/keyboard.cpp). So every button here
//   is a physical key (sym) and Shift/Ctrl are real modifiers applied
//   via keysym.mod. Shifted legends are just labels.
// * Timing: cap32's own auto-typing holds a key for >=1 emulated frame,
//   otherwise the CPC firmware debouncer eats it. We hold a tap for
//   ~40 ms (2+ frames at 50 Hz) before sending KEYUP.
// * Shift/Ctrl are sticky (one-shot): tap to arm, next key consumes it.
//   Double-tap on Shift = Caps-style lock for the session.
// * SDL_PushEvent is thread safe, so pushing from the main (UIKit)
//   thread while the emulator loop polls on SDL's thread is fine.

#import "CPCVirtualKeyboard.h"
#import "CPCVirtualKeyboardBridge.h"
#import <QuartzCore/QuartzCore.h>
#include <SDL2/SDL.h>

// ---------------------------------------------------------------------
// Key model
// ---------------------------------------------------------------------
// Every button is an explicit (sym, needsShift) pair copied from the
// core's US map (SDLkeysymFromCPCkeys_us in src/keyboard.cpp). The
// printed legends follow the *CPC* keycaps; what is sent is what the
// core's InputMapper needs to resolve that same CPC key.
typedef NS_ENUM(NSInteger, CPCKeyKind) {
    CPCKeyKindNormal = 0,
    CPCKeyKindShift,
    CPCKeyKindCtrl,
    CPCKeyKindCaps,   // real, toggling CAPSLOCK key - not a sticky modifier
};

@interface CPCKeyDef : NSObject
@property (nonatomic, copy)   NSString *label;      // main legend
@property (nonatomic, copy)   NSString *shiftLabel; // small legend above (may be nil)
@property (nonatomic, assign) SDL_Keycode sym;
@property (nonatomic, assign) BOOL baseShift;       // chord always sent WITH shift
// YES for keys that have NO "Shift+key" entry in the core's map (ESC,
// CLR, DEL, TAB, RETURN, ENTER, COPY, SPACE, F-keys handled separately).
// Sending Shift+<these> makes CPCscancodeFromKeysym() return 0xff and
// the keypress is silently dropped, so Shift is stripped for them.
@property (nonatomic, assign) BOOL ignoresShift;
// Optional override used when the on-screen SHIFT is armed. Needed
// because the CPC's shifted legend is NOT always host Shift + same key
// (CPC: Shift+7 = '  but host US map: Shift+7 = &, and ' is SDLK_QUOTE).
@property (nonatomic, assign) BOOL hasShiftOverride;
@property (nonatomic, assign) SDL_Keycode shiftSym;
@property (nonatomic, assign) BOOL shiftSymNeedsShift;
@property (nonatomic, assign) CGFloat width;        // relative width, 1.0 = std key
@property (nonatomic, assign) CPCKeyKind kind;
@end
@implementation CPCKeyDef
@end

// Plain key. `shifted` is the CPC's shifted legend, display only; the
// user gets it by arming the on-screen SHIFT key.
static CPCKeyDef *K(NSString *label, NSString *shifted, SDL_Keycode sym, CGFloat w)
{
    CPCKeyDef *k = [CPCKeyDef new];
    k.label = label; k.shiftLabel = shifted;
    k.sym = sym; k.width = w; k.kind = CPCKeyKindNormal;
    return k;
}

// Plain key + explicit chord to send when on-screen SHIFT is armed.
// `plainSym`  : sent with no modifier.
// `shiftSym`  : sent when SHIFT is armed; `needsShift` says whether that
//               chord itself carries host Shift (true for most symbols).
static CPCKeyDef *KO(NSString *label, NSString *shifted, SDL_Keycode plainSym,
                     SDL_Keycode shiftSym, BOOL needsShift, CGFloat w)
{
    CPCKeyDef *k = K(label, shifted, plainSym, w);
    k.hasShiftOverride = YES;
    k.shiftSym = shiftSym;
    k.shiftSymNeedsShift = needsShift;
    return k;
}

// Key that must never carry host Shift (see ignoresShift).
static CPCKeyDef *KN(NSString *label, SDL_Keycode sym, CGFloat w)
{
    CPCKeyDef *k = K(label, nil, sym, w);
    k.ignoresShift = YES;
    return k;
}

// Key whose *own* CPC function is a host Shift chord (e.g. CPC '@' is
// SDLK_2|SHIFT in the core map). Shift is forced on for the tap.
static CPCKeyDef *KS(NSString *label, NSString *shifted, SDL_Keycode sym, CGFloat w)
{
    CPCKeyDef *k = K(label, shifted, sym, w);
    k.baseShift = YES;
    return k;
}

// CPC "@" key. Plain tap = '@' (host Shift+2). With on-screen SHIFT
// armed the CPC prints '|' (CPC_PIPE = SDLK_BACKSLASH|SHIFT in the core
// map) - needed for |A, |B, |CPM, |TAPE etc. to switch drives/load discs.
static CPCKeyDef *KAT(NSString *label, NSString *shifted, CGFloat w)
{
    CPCKeyDef *k = K(label, shifted, SDLK_2, w);
    k.baseShift = YES;
    k.hasShiftOverride = YES;
    k.shiftSym = SDLK_BACKSLASH;
    k.shiftSymNeedsShift = YES;
    return k;
}

static CPCKeyDef *KMod(NSString *label, SDL_Keycode sym, CGFloat w, CPCKeyKind kind)
{
    CPCKeyDef *k = K(label, nil, sym, w);
    k.kind = kind;
    return k;
}

// CPC 6128 layout. Rows (CPC keycaps):
//   ESC 1..0 - ^ CLR DEL / TAB Q..P @ [ RETURN / CAPS A..L : ; ] ENTER
//   / SHIFT Z..M , . / \ SHIFT / CTRL COPY SPACE cursors
// F0..F9 are the numeric-keypad block on the real machine; they get
// their own top row here since there is no room for a keypad.
static NSArray<NSArray<CPCKeyDef *> *> *BuildLayout(void)
{
    return @[
        // F-keys / editing
        @[ KN(@"ESC", SDLK_ESCAPE, 1.4),
           K(@"F1",  nil, SDLK_KP_1,      1.0), K(@"F2", nil, SDLK_KP_2, 1.0),
           K(@"F3",  nil, SDLK_KP_3,      1.0), K(@"F4", nil, SDLK_KP_4, 1.0),
           K(@"F5",  nil, SDLK_KP_5,      1.0), K(@"F6", nil, SDLK_KP_6, 1.0),
           K(@"F7",  nil, SDLK_KP_7,      1.0), K(@"F8", nil, SDLK_KP_8, 1.0),
           K(@"F9",  nil, SDLK_KP_9,      1.0), K(@"F0", nil, SDLK_KP_0, 1.0),
           KN(@"CLR", SDLK_DELETE, 1.3),
           KN(@"DEL", SDLK_BACKSPACE, 1.3) ],

        // Number row. CPC keycaps: 1!  2"  3#  4$  5%  6&  7'  8(  9)  0_
        // Host US map (core): ! = Shift+1  " = Shift+'  # = Shift+3
        // $ = Shift+4  % = Shift+5  & = Shift+7  ' = plain '
        // ( = Shift+9  ) = Shift+0  _ = Shift+-
        // so 2/6/7/8/9/0 need per-key overrides for their shifted glyph.
        @[ K (@"1", @"!", SDLK_1, 1.0),
           KO(@"2", @"\"", SDLK_2, SDLK_QUOTE,  YES, 1.0), // "  = Shift+'
           K (@"3", @"#", SDLK_3, 1.0),
           K (@"4", @"$", SDLK_4, 1.0),
           K (@"5", @"%", SDLK_5, 1.0),
           KO(@"6", @"&", SDLK_6, SDLK_7,        YES, 1.0), // &  = Shift+7
           KO(@"7", @"'", SDLK_7, SDLK_QUOTE,    NO,  1.0), // '  = plain '
           KO(@"8", @"(", SDLK_8, SDLK_9,        YES, 1.0), // (  = Shift+9
           KO(@"9", @")", SDLK_9, SDLK_0,        YES, 1.0), // )  = Shift+0
           KO(@"0", @"_", SDLK_0, SDLK_MINUS,    YES, 1.0), // _  = Shift+-
           KO(@"-", @"=", SDLK_MINUS, SDLK_EQUALS, NO, 1.0),  // = is plain '=' 
           KS(@"^", nil, SDLK_6, 1.0),    // CPC_POWER = SDLK_6|SHIFT (no £ on US map)
           KN(@"TAB", SDLK_TAB, 1.3) ],

        // QWERTY row
        @[ K(@"Q", nil, SDLK_q, 1.0), K(@"W", nil, SDLK_w, 1.0),
           K(@"E", nil, SDLK_e, 1.0), K(@"R", nil, SDLK_r, 1.0),
           K(@"T", nil, SDLK_t, 1.0), K(@"Y", nil, SDLK_y, 1.0),
           K(@"U", nil, SDLK_u, 1.0), K(@"I", nil, SDLK_i, 1.0),
           K(@"O", nil, SDLK_o, 1.0), K(@"P", nil, SDLK_p, 1.0),
           KAT(@"@", @"|", 1.0),         // CPC_AT = SDLK_2|SHIFT; with SHIFT armed -> CPC_PIPE = SDLK_BACKSLASH|SHIFT
           K(@"[", @"{", SDLK_LEFTBRACKET, 1.0),
           KN(@"RETURN", SDLK_RETURN, 1.9) ],

        // Home row
        @[ KMod(@"CAPS", SDLK_CAPSLOCK, 1.5, CPCKeyKindCaps),
           K(@"A", nil, SDLK_a, 1.0), K(@"S", nil, SDLK_s, 1.0),
           K(@"D", nil, SDLK_d, 1.0), K(@"F", nil, SDLK_f, 1.0),
           K(@"G", nil, SDLK_g, 1.0), K(@"H", nil, SDLK_h, 1.0),
           K(@"J", nil, SDLK_j, 1.0), K(@"K", nil, SDLK_k, 1.0),
           K(@"L", nil, SDLK_l, 1.0),
           KS(@":", @"*", SDLK_SEMICOLON, 1.0), // CPC_COLON = SDLK_SEMICOLON|SHIFT
           KO(@";", @"+", SDLK_SEMICOLON, SDLK_EQUALS, YES, 1.0), // + = Shift+=
           K(@"]", @"}", SDLK_RIGHTBRACKET, 1.0),
           KN(@"ENTER", SDLK_KP_ENTER, 1.4) ],

        // Bottom letter row
        @[ KMod(@"SHIFT", SDLK_LSHIFT, 2.0, CPCKeyKindShift),
           K(@"Z", nil, SDLK_z, 1.0), K(@"X", nil, SDLK_x, 1.0),
           K(@"C", nil, SDLK_c, 1.0), K(@"V", nil, SDLK_v, 1.0),
           K(@"B", nil, SDLK_b, 1.0), K(@"N", nil, SDLK_n, 1.0),
           K(@"M", nil, SDLK_m, 1.0),
           K(@",", @"<", SDLK_COMMA,  1.0),
           K(@".", @">", SDLK_PERIOD, 1.0),
           K(@"/", @"?", SDLK_SLASH,  1.0),
           KO(@"\\", @"`", SDLK_BACKSLASH, SDLK_BACKQUOTE, NO, 1.0),
           KMod(@"SHIFT", SDLK_RSHIFT, 2.0, CPCKeyKindShift) ],

        // Space / cursors
        @[ KMod(@"CTRL", SDLK_LCTRL, 1.6, CPCKeyKindCtrl),
           KN(@"COPY", SDLK_LALT, 1.4),
           KN(@"SPACE", SDLK_SPACE, 5.2),
           K(@"◀", nil, SDLK_LEFT,  1.1),
           K(@"▲", nil, SDLK_UP,    1.1),
           K(@"▼", nil, SDLK_DOWN,  1.1),
           K(@"▶", nil, SDLK_RIGHT, 1.1) ],
    ];
}

// ---------------------------------------------------------------------
// SDL event injection
// ---------------------------------------------------------------------
static void PushKey(SDL_Keycode sym, Uint16 mod, bool down)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.type = e.type;
    e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.key.repeat = 0;
    e.key.keysym.sym = sym;
    e.key.keysym.scancode = SDL_GetScancodeFromKey(sym);
    e.key.keysym.mod = mod;
    e.key.windowID = 0;
    SDL_PushEvent(&e);
}

// ---------------------------------------------------------------------
// Key button view
// ---------------------------------------------------------------------
@interface CPCKeyButton : UIControl
@property (nonatomic, strong) CPCKeyDef *def;
@property (nonatomic, assign) BOOL latched;   // sticky modifier armed
@property (nonatomic, assign) BOOL locked;    // shift lock (double tap)
@property (nonatomic, strong) UILabel *mainLabel;
@property (nonatomic, strong) UILabel *subLabel;
- (void)refreshAppearance;
@end

@implementation CPCKeyButton

- (instancetype)initWithDef:(CPCKeyDef *)def
{
    if ((self = [super initWithFrame:CGRectZero])) {
        _def = def;
        self.layer.cornerRadius = 5.0;
        self.layer.borderWidth = 1.0;
        self.multipleTouchEnabled = NO;
        self.exclusiveTouch = YES;

        _mainLabel = [UILabel new];
        _mainLabel.text = def.label;
        _mainLabel.textAlignment = NSTextAlignmentCenter;
        _mainLabel.textColor = [UIColor whiteColor];
        _mainLabel.adjustsFontSizeToFitWidth = YES;
        _mainLabel.minimumScaleFactor = 0.5;
        _mainLabel.userInteractionEnabled = NO;
        [self addSubview:_mainLabel];

        if (def.shiftLabel.length) {
            _subLabel = [UILabel new];
            _subLabel.text = def.shiftLabel;
            _subLabel.textAlignment = NSTextAlignmentCenter;
            _subLabel.textColor = [UIColor colorWithWhite:0.75 alpha:1.0];
            _subLabel.userInteractionEnabled = NO;
            [self addSubview:_subLabel];
        }
        [self refreshAppearance];
    }
    return self;
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    CGRect b = self.bounds;
    BOOL hasSub = (_subLabel != nil);
    CGFloat h = b.size.height;
    _mainLabel.font = [UIFont boldSystemFontOfSize:(hasSub ? h * 0.36 : h * 0.34)];
    if (hasSub) {
        _subLabel.font = [UIFont systemFontOfSize:h * 0.26];
        _subLabel.frame = CGRectMake(0, 2, b.size.width, h * 0.40);
        _mainLabel.frame = CGRectMake(0, h * 0.38, b.size.width, h * 0.58);
    } else {
        _mainLabel.frame = CGRectInset(b, 2, 0);
    }
}

- (void)refreshAppearance
{
    BOOL isMod = (_def.kind == CPCKeyKindShift || _def.kind == CPCKeyKindCtrl);
    UIColor *base = isMod ? [UIColor colorWithRed:0.20 green:0.22 blue:0.30 alpha:0.94]
                          : [UIColor colorWithWhite:0.16 alpha:0.94];
    UIColor *on   = _locked ? [UIColor colorWithRed:0.85 green:0.45 blue:0.10 alpha:0.98]
                            : [UIColor colorWithRed:0.20 green:0.50 blue:0.85 alpha:0.98];
    if (self.highlighted || _latched || _locked) {
        self.backgroundColor = (_latched || _locked) ? on : [UIColor colorWithWhite:0.38 alpha:0.98];
    } else {
        self.backgroundColor = base;
    }
    self.layer.borderColor = [UIColor colorWithWhite:0.05 alpha:1.0].CGColor;
}

- (void)setHighlighted:(BOOL)highlighted
{
    [super setHighlighted:highlighted];
    [self refreshAppearance];
}

@end

// ---------------------------------------------------------------------
// Keyboard panel
// ---------------------------------------------------------------------
@interface CPCKeyboardPanel : UIView
@property (nonatomic, strong) NSArray<NSArray<CPCKeyDef *> *> *layout;
@property (nonatomic, strong) NSMutableArray<CPCKeyButton *> *buttons;
@property (nonatomic, assign) BOOL shiftLatched;
@property (nonatomic, assign) BOOL shiftLocked;
@property (nonatomic, assign) BOOL ctrlLatched;
@property (nonatomic, assign) NSTimeInterval lastShiftTap;
@end

@implementation CPCKeyboardPanel

static const NSInteger kRowCount = 6;

- (instancetype)initWithFrame:(CGRect)frame
{
    if ((self = [super initWithFrame:frame])) {
        self.backgroundColor = [UIColor colorWithWhite:0.05 alpha:0.78];
        self.layer.cornerRadius = 8.0;
        self.multipleTouchEnabled = YES;
        _layout = BuildLayout();
        _buttons = [NSMutableArray array];
        for (NSArray<CPCKeyDef *> *row in _layout) {
            for (CPCKeyDef *def in row) {
                CPCKeyButton *b = [[CPCKeyButton alloc] initWithDef:def];
                [b addTarget:self action:@selector(keyDown:)
                    forControlEvents:UIControlEventTouchDown];
                [self addSubview:b];
                [_buttons addObject:b];
            }
        }
    }
    return self;
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    const CGFloat pad = 6.0, gap = 4.0;
    CGFloat availH = self.bounds.size.height - 2 * pad - gap * (kRowCount - 1);
    CGFloat rowH = availH / kRowCount;
    NSInteger idx = 0;
    for (NSInteger r = 0; r < (NSInteger)_layout.count; r++) {
        NSArray<CPCKeyDef *> *row = _layout[r];
        CGFloat totalUnits = 0;
        for (CPCKeyDef *d in row) totalUnits += d.width;
        CGFloat availW = self.bounds.size.width - 2 * pad - gap * (row.count - 1);
        CGFloat unit = availW / totalUnits;
        CGFloat x = pad, y = pad + r * (rowH + gap);
        for (NSUInteger c = 0; c < row.count; c++) {
            CGFloat w = unit * row[c].width;
            _buttons[idx++].frame = CGRectMake(x, y, w, rowH);
            x += w + gap;
        }
    }
}

// -- modifier state helpers -------------------------------------------
- (Uint16)currentMod
{
    Uint16 m = 0;
    if (_shiftLatched || _shiftLocked) m |= KMOD_LSHIFT;
    if (_ctrlLatched)                  m |= KMOD_LCTRL;
    return m;
}

- (void)syncModifierButtons
{
    for (CPCKeyButton *b in _buttons) {
        if (b.def.kind == CPCKeyKindShift) {
            b.latched = _shiftLatched;
            b.locked = _shiftLocked;
        } else if (b.def.kind == CPCKeyKindCtrl) {
            b.latched = _ctrlLatched;
        }
        [b refreshAppearance];
    }
}

// -- key handling -----------------------------------------------------
- (void)keyDown:(CPCKeyButton *)btn
{
    CPCKeyDef *d = btn.def;

    if (d.kind == CPCKeyKindShift) {
        NSTimeInterval now = CACurrentMediaTime();
        if (_shiftLocked) {
            _shiftLocked = NO; _shiftLatched = NO;
        } else if (_shiftLatched && (now - _lastShiftTap) < 0.4) {
            _shiftLocked = YES; // double tap => lock
        } else {
            _shiftLatched = !_shiftLatched;
        }
        _lastShiftTap = now;
        [self syncModifierButtons];
        return;
    }
    if (d.kind == CPCKeyKindCtrl) {
        _ctrlLatched = !_ctrlLatched;
        [self syncModifierButtons];
        return;
    }

    SDL_Keycode sym = d.sym;
    Uint16 mod = 0;
    BOOL shiftArmed = (_shiftLatched || _shiftLocked);

    if (shiftArmed && d.hasShiftOverride) {
        // The CPC's shifted glyph is a different host chord than
        // "Shift + this key" - send the explicit one.
        sym = d.shiftSym;
        if (d.shiftSymNeedsShift) mod |= KMOD_LSHIFT;
    } else {
        if (shiftArmed && !d.ignoresShift) mod |= KMOD_LSHIFT;
        if (d.baseShift)                   mod |= KMOD_LSHIFT;
    }
    if (_ctrlLatched) mod |= KMOD_LCTRL;

    // CAPS is a real, toggling CPC key: never combine it with modifiers.
    if (d.kind == CPCKeyKindCaps) mod = 0;

    [self sendTapForSym:sym mod:mod];

    // One-shot: consume latched modifiers (locked shift persists).
    BOOL consumed = NO;
    if (_shiftLatched && !_shiftLocked) { _shiftLatched = NO; consumed = YES; }
    if (_ctrlLatched)                    { _ctrlLatched = NO;  consumed = YES; }
    if (consumed) [self syncModifierButtons];
}

// Press, hold ~2 emulated frames, release. Without the hold, the CPC
// firmware debouncer drops the keystroke (same reason cap32 spaces its
// own auto-typed events by a frame).
- (void)sendTapForSym:(SDL_Keycode)sym mod:(Uint16)mod
{
    PushKey(sym, mod, true);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(45 * NSEC_PER_MSEC)),
                   dispatch_get_main_queue(), ^{
        PushKey(sym, mod, false);
    });
}

// Let touches on empty panel space be swallowed (don't fall through to
// the emulator view underneath).
- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    UIView *v = [super hitTest:point withEvent:event];
    return v ?: (CGRectContainsPoint(self.bounds, point) ? self : nil);
}

@end

// The app is landscape-only (Info.plist). On iOS 8+ UIScreen.bounds is
// orientation-dependent, but a window created before the first rotation
// pass may still see portrait dimensions. Normalize to landscape.
static CGRect LandscapeScreenBounds(void)
{
    CGRect b = [UIScreen mainScreen].bounds;
    if (b.size.width < b.size.height) {
        b = CGRectMake(0, 0, b.size.height, b.size.width);
    }
    return b;
}

// ---------------------------------------------------------------------
// Overlay window that only intercepts touches on its own subviews.
// ---------------------------------------------------------------------
@interface CPCOverlayWindow : UIWindow
@end
@implementation CPCOverlayWindow
- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    UIView *v = [super hitTest:point withEvent:event];
    // Pass through anything that lands on the bare root view so the
    // emulator underneath keeps receiving mouse/touch events.
    if (v == self.rootViewController.view || v == self) return nil;
    return v;
}
@end

@interface CPCOverlayVC : UIViewController
@property (nonatomic, strong) CPCKeyboardPanel *panel;
@property (nonatomic, strong) UIButton *toggle;
@property (nonatomic, assign) BOOL kbVisible;
@end

@implementation CPCOverlayVC

- (BOOL)prefersStatusBarHidden { return YES; }

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskLandscape;
}

- (void)loadView
{
    UIView *root = [[UIView alloc] initWithFrame:LandscapeScreenBounds()];
    root.backgroundColor = [UIColor clearColor];
    root.opaque = NO;
    root.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.view = root;

    _panel = [[CPCKeyboardPanel alloc] initWithFrame:CGRectZero];
    _panel.hidden = YES;
    [root addSubview:_panel];

    _toggle = [UIButton buttonWithType:UIButtonTypeCustom];
    [_toggle setTitle:@"⌨" forState:UIControlStateNormal];
    _toggle.titleLabel.font = [UIFont systemFontOfSize:26];
    _toggle.backgroundColor = [UIColor colorWithWhite:0.05 alpha:0.60];
    _toggle.layer.cornerRadius = 8.0;
    [_toggle addTarget:self action:@selector(toggleTapped)
      forControlEvents:UIControlEventTouchUpInside];
    [root addSubview:_toggle];
}

- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];
    CGRect b = self.view.bounds;

    // Panel takes the lower ~45% of the screen. On iPad mini 1 landscape
    // (1024x768) that is ~345 pt tall, which leaves the top ~55% for the
    // 4:3 CPC picture to remain fully visible and playable.
    CGFloat panelH = MIN(b.size.height * 0.46, 360.0);
    _panel.frame = CGRectMake(0, b.size.height - panelH, b.size.width, panelH);

    // Toggle sits top-right, tucked out of the CPC picture's way.
    _toggle.frame = CGRectMake(b.size.width - 54, 8, 46, 40);
}

- (void)toggleTapped
{
    [self setKeyboardVisible:!_kbVisible];
}

- (void)setKeyboardVisible:(BOOL)visible
{
    _kbVisible = visible;
    _panel.hidden = !visible;
    _toggle.backgroundColor = visible
        ? [UIColor colorWithRed:0.20 green:0.50 blue:0.85 alpha:0.80]
        : [UIColor colorWithWhite:0.05 alpha:0.60];
}

@end

// ---------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------
static CPCOverlayWindow *sWindow;
static CPCOverlayVC *sVC;

@implementation CPCVirtualKeyboard

+ (void)install
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self install]; });
        return;
    }
    if (sWindow) return;

    CPCOverlayWindow *w = [[CPCOverlayWindow alloc]
                              initWithFrame:LandscapeScreenBounds()];
    sVC = [CPCOverlayVC new];
    w.rootViewController = sVC;
    w.backgroundColor = [UIColor clearColor];
    w.opaque = NO;
    // Above SDL's window (UIWindowLevelNormal) and above alerts is not
    // needed; a level just over normal keeps it on top of the emulator.
    w.windowLevel = UIWindowLevelNormal + 10;
    // Deliberately NOT makeKeyAndVisible: that would make this window
    // key and could pull first-responder / rotation control away from
    // SDL's window. hidden=NO shows it while SDL's window stays key.
    w.hidden = NO;
    sWindow = w;
}

+ (void)setKeyboardVisible:(BOOL)visible
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [sVC setKeyboardVisible:visible];
    });
}

+ (void)toggleKeyboard
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [sVC toggleTapped];
    });
}

@end

// ---------------------------------------------------------------------
// C bridge
// ---------------------------------------------------------------------
extern "C" void CPCVKbd_Install(void) { [CPCVirtualKeyboard install]; }
extern "C" void CPCVKbd_Toggle(void)  { [CPCVirtualKeyboard toggleKeyboard]; }
