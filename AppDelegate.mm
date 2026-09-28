// AppDelegate.mm
//
// iOS 7.0+ compatible (built for the iOS 7.1.2 / iPad 1 target).
//
// SDL2 (SDL_uikitappdelegate.m) installs its own UIApplicationMain
// delegate (SDLUIKitDelegate) that owns the run loop and eventually
// calls main() in main_ios.mm. We subclass it rather than replacing it,
// so SDL's own lifecycle handling (backgrounding, audio session
// interruptions, etc.) still runs. We only add handling for
// "Open in Caprice32" document opens, which SDL's base delegate doesn't
// do anything with.
//
// Compatibility notes vs. the iOS 9.3 build:
//   - application:openURL:options: is iOS 9+. On iOS 7/8 UIKit calls the
//     older application:openURL:sourceApplication:annotation: instead,
//     so we implement BOTH and funnel them into one helper.
//   - UIAlertController is iOS 8+ (class does not exist on 7.x, using it
//     would crash). We use UIAlertView, which exists on 7.x and is only
//     deprecated (not removed) on newer iOS.
//   - -startAccessingSecurityScopedResource is iOS 8+; guarded with
//     respondsToSelector:.
//
// Flow: a .dsk/.sna/.cdt/.cpr file is opened -> we copy it into
// Documents and stash its path in NSUserDefaults -> main_ios.mm reads it
// back on the next launch and appends it to argv, exactly like passing a
// filename on the desktop command line (`cap32 game.dsk`).

#import <UIKit/UIKit.h>
#import "SDL_uikitappdelegate.h"

@interface Caprice32AppDelegate : SDLUIKitDelegate
@end

@implementation Caprice32AppDelegate

- (BOOL)caprice_importFileURL:(NSURL *)url
{
    if (!url.isFileURL) {
        return NO;
    }

    BOOL didStartAccessing = NO;
    if ([url respondsToSelector:@selector(startAccessingSecurityScopedResource)]) {
        didStartAccessing = [url startAccessingSecurityScopedResource];
    }

    NSFileManager *fm = [NSFileManager defaultManager];
    NSArray *docPaths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                             NSUserDomainMask, YES);
    NSString *documentsPath = docPaths.firstObject;
    NSString *destPath = [documentsPath stringByAppendingPathComponent:url.lastPathComponent];

    NSError *err = nil;
    BOOL copied = YES;
    // If the system already handed us a file inside Documents (Inbox
    // copies live in Documents/Inbox on iOS 7), don't delete it before
    // copying onto itself.
    if (![url.path isEqualToString:destPath]) {
        [fm removeItemAtPath:destPath error:nil]; // ignore "didn't exist"
        copied = [fm copyItemAtPath:url.path toPath:destPath error:&err];
    }

    if (didStartAccessing) {
        [url stopAccessingSecurityScopedResource];
    }

    if (!copied) {
        NSLog(@"Caprice32: failed to import opened file: %@", err);
        return NO;
    }

    [[NSUserDefaults standardUserDefaults] setObject:destPath
                                               forKey:@"CaprisePendingOpenFile"];
    [[NSUserDefaults standardUserDefaults] synchronize];

    // The emulator core only reads this at process start (see
    // main_ios.mm), so if it's already running the user needs to
    // relaunch.
    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"File Imported"
                                                    message:@"Restart Caprice32 to load this file."
                                                   delegate:nil
                                          cancelButtonTitle:@"OK"
                                          otherButtonTitles:nil];
    [alert show];
    return YES;
}

// iOS 4.2 - 8.x path (this is the one that fires on 7.1.2).
- (BOOL)application:(UIApplication *)application
            openURL:(NSURL *)url
  sourceApplication:(NSString *)sourceApplication
         annotation:(id)annotation
{
    return [self caprice_importFileURL:url];
}

// iOS 9+ path, kept so the same binary behaves on newer systems too.
// Typed as plain NSDictionary so no iOS 9 SDK typedef is required.
- (BOOL)application:(UIApplication *)app
            openURL:(NSURL *)url
            options:(NSDictionary *)options
{
    return [self caprice_importFileURL:url];
}

@end
