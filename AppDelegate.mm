// AppDelegate.mm
//
// SDL2 (SDL_uikitappdelegate.m) installs its own UIApplicationMain
// delegate (SDLUIKitDelegate) that owns the run loop and eventually
// calls main() in main_ios.mm. We subclass it rather than replacing it,
// so SDL's own lifecycle handling (backgrounding, audio session
// interruptions, etc.) still runs. We only add handling for
// "Open in Caprice32" / Files.app document opens, which SDL's base
// delegate doesn't do anything with.
//
// Flow: a .dsk/.sna/.cdt/.cpr file is opened -> we stash its local
// path in NSUserDefaults -> main_ios.mm reads it back on the next
// launch and appends it to argv, exactly like passing a filename on
// the desktop command line (`cap32 game.dsk`). If the app is already
// running when a file is opened, we currently just record it for the
// *next* cold launch, since cap32_main() has no public "load this disk
// now" entry point exposed outside of its own SDL event loop / GUI.
// Wiring live hot-swap is a follow-up, not required for a working port.

#import <UIKit/UIKit.h>
#import "SDL_uikitappdelegate.h"

@interface Caprice32AppDelegate : SDLUIKitDelegate
@end

@implementation Caprice32AppDelegate

- (BOOL)application:(UIApplication *)app
            openURL:(NSURL *)url
            options:(NSDictionary<UIApplicationOpenURLOptionsKey, id> *)options
{
    if (!url.isFileURL) {
        return NO;
    }

    BOOL didStartAccessing = [url startAccessingSecurityScopedResource];

    NSFileManager *fm = [NSFileManager defaultManager];
    NSArray *docPaths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                             NSUserDomainMask, YES);
    NSString *documentsPath = docPaths.firstObject;
    NSString *destPath = [documentsPath stringByAppendingPathComponent:url.lastPathComponent];

    NSError *err = nil;
    [fm removeItemAtPath:destPath error:nil]; // ignore "didn't exist" errors
    BOOL copied = [fm copyItemAtURL:url
                               toURL:[NSURL fileURLWithPath:destPath]
                               error:&err];

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
    // relaunch. A future improvement could expose a thread-safe
    // "insert disk" hook into cap32.cpp's slot-loading code
    // (see fillSlots()/loadSlots() in cap32.cpp) for hot loading.
    UIAlertController *alert =
        [UIAlertController alertControllerWithTitle:@"File Imported"
                                             message:@"Restart Caprice32 to load this file."
                                      preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                               style:UIAlertActionStyleDefault
                                             handler:nil]];
    [self.window.rootViewController presentViewController:alert
                                                   animated:YES
                                                 completion:nil];

    return YES;
}

@end
