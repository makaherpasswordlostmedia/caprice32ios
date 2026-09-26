#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
/*
 * This SDK mirror's UIApplication.h omits UIApplicationOpenURLOptionsKey
 * (real iOS 9.0+ API). #define'ing a macro would break real usages
 * elsewhere, so instead alias the missing type name only if the SDK
 * itself hasn't already declared it as a macro.
 */
#ifndef UIApplicationOpenURLOptionsKey
typedef NSString * UIApplicationOpenURLOptionsKey;
#endif

/*
 * setNeedsUpdateOfHomeIndicatorAutoHidden and
 * setNeedsUpdateOfScreenEdgesDeferringSystemGestures are real
 * iOS 11+ UIViewController methods (home indicator / edge
 * gesture APIs postdate iOS 9.3 entirely - not a mirror gap).
 * SDL_uikitviewcontroller.m calls them on itself under its own
 * runtime OS-version guards, but the compiler still needs the
 * selector declared to accept the call. Declare them as a
 * category so the call type-checks; on iOS 9.3 devices SDL's
 * own guard means this code path never actually executes.
 */
@interface UIViewController (SDLCompat93)
- (void)setNeedsUpdateOfHomeIndicatorAutoHidden;
- (void)setNeedsUpdateOfScreenEdgesDeferringSystemGestures;
@end
