// CPCVirtualKeyboard.h
//
// Native UIKit on-screen Amstrad CPC keyboard for the iOS port.
// Engine: SDL2 2.0.9 / UIKit, iOS 9.3, armv7, ARC.
//
// The upstream virtual keyboard (src/gui/CapriceVKeyboard*) is built on
// wGui, which this port excludes (-DCAPRICE_NO_WGUI), so showVKeyboard()
// is an empty stub on iOS. This overlay replaces it with real touch
// input: buttons synthesize SDL_KEYDOWN/SDL_KEYUP events and push them
// into SDL's queue with SDL_PushEvent (thread-safe), exactly the shape
// InputMapper::CPCscancodeFromKeysym() already understands. No changes
// to the emulator core are needed.

#import <UIKit/UIKit.h>

@interface CPCVirtualKeyboard : NSObject

// Installs the overlay window (idempotent). Safe to call from any
// thread; hops to the main thread internally.
+ (void)install;

// Show / hide the keyboard panel. The small toggle button stays visible.
+ (void)setKeyboardVisible:(BOOL)visible;
+ (void)toggleKeyboard;

@end
