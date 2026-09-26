// main_ios.mm
// iOS launch shim for Caprice32.
//
// SDL2's iOS backend (SDL_uikitappdelegate.m) provides its own `main`
// replacement via SDL_main.h: it calls UIApplicationMain, spins up a
// UIKit app delegate, and once the app has finished launching it calls
// into *this* translation unit's `main(argc, argv)` on a dedicated
// thread. That call is expected to block for the lifetime of the app,
// which is exactly what cap32_main()'s `while (true)` loop already does
// on desktop platforms - so nothing about the emulator's own control
// flow needs to change.
//
// What iOS *does* require that Linux/macOS didn't:
//   1. No real argv - we synthesize one pointing at sandboxed paths.
//   2. No writable directory next to the binary - the app bundle
//      (Bundle.main) is read-only at runtime, so cap32.cfg and any
//      first-run ROM/media assets must be seeded into the app's
//      Documents directory (which cap32.cpp's HOME-based search
//      already knows how to find, once HOME is set correctly - see
//      getConfigurationFilename() in cap32.cpp).
//   3. HOME must be set explicitly. iOS does not populate it.
//
// This file intentionally does not touch cap32.cpp's own path search
// logic (getenv("HOME"), getenv("XDG_CONFIG_HOME"), etc.) - it just
// makes sure those environment variables point somewhere sane before
// cap32_main() runs, so the existing desktop logic "just works".

#import <Foundation/Foundation.h>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>

extern "C" int cap32_main(int argc, char **argv);

// ---------------------------------------------------------------------
// First-run seeding: copy bundled defaults (cap32.cfg, roms/) into the
// writable Documents directory the first time the app launches. After
// this, cap32's own config/rom search (which walks $HOME) finds them
// as if they'd always been there.
// ---------------------------------------------------------------------
static void SeedWritableSupportFilesIfNeeded(NSString *documentsPath)
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSBundle *bundle = [NSBundle mainBundle];

    // cap32.cfg
    NSString *destCfg = [documentsPath stringByAppendingPathComponent:@"cap32.cfg"];
    if (![fm fileExistsAtPath:destCfg]) {
        NSString *srcCfg = [bundle pathForResource:@"cap32" ofType:@"cfg"];
        if (srcCfg) {
            NSError *err = nil;
            [fm copyItemAtPath:srcCfg toPath:destCfg error:&err];
            if (err) {
                NSLog(@"Caprice32: failed to seed cap32.cfg: %@", err);
            }
        }
    }

    // roms/ directory (OS ROMs required to boot a CPC at all)
    NSString *destRoms = [documentsPath stringByAppendingPathComponent:@"roms"];
    BOOL isDir = NO;
    if (![fm fileExistsAtPath:destRoms isDirectory:&isDir] || !isDir) {
        NSString *srcRoms = [[bundle resourcePath] stringByAppendingPathComponent:@"roms"];
        if ([fm fileExistsAtPath:srcRoms]) {
            NSError *err = nil;
            [fm copyItemAtPath:srcRoms toPath:destRoms error:&err];
            if (err) {
                NSLog(@"Caprice32: failed to seed roms/: %@", err);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    @autoreleasepool {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                              NSUserDomainMask, YES);
        NSString *documentsPath = paths.firstObject;

        SeedWritableSupportFilesIfNeeded(documentsPath);

        // cap32.cpp's getConfigurationFilename() looks at $HOME/.config
        // and $HOME/.cap32.cfg, in that order, before giving up. Point
        // HOME at Documents so both the sandbox-seeded cap32.cfg above
        // and any user-imported DSK/CDT/SNA files live in one place
        // that's visible from Files.app (if the app enables document
        // sharing) and survives app updates.
        setenv("HOME", documentsPath.UTF8String, 1);

        // Caprice32 also derives a config search entry from argv[0]'s
        // parent directory (see `binPath` in cap32.cpp) - point that at
        // the bundle's Resources directory, where any bundled
        // cap32.cfg/roms fallback copy lives read-only.
        NSString *resourcePath = [[NSBundle mainBundle] resourcePath];

        // Build a synthetic argv. iOS gives us no meaningful CLI args;
        // UIKit-launched apps are opened via Springboard, not a shell.
        // argv[0] just needs to be a path whose parent_path() resolves
        // to somewhere sensible for the binPath fallback in cap32.cpp.
        std::string fakeArgv0 = std::string(resourcePath.UTF8String) + "/Caprice32ARMv7";

        std::vector<std::string> args = { fakeArgv0 };

        // If a file was handed to us via document-open / URL scheme
        // (see AppDelegate's application:openURL: handling), forward it
        // as a positional slot argument exactly like the desktop build
        // accepts `cap32 game.dsk` on the command line.
        NSString *pendingFile = [[NSUserDefaults standardUserDefaults]
                                    stringForKey:@"CaprisePendingOpenFile"];
        if (pendingFile.length > 0) {
            args.push_back(std::string(pendingFile.UTF8String));
            [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"CaprisePendingOpenFile"];
        }

        std::vector<char *> cargv;
        cargv.reserve(args.size());
        for (auto &a : args) {
            cargv.push_back(const_cast<char *>(a.c_str()));
        }

        // Never returns under normal operation - cap32_main() runs its
        // own event loop until the emulator is quit.
        return cap32_main(static_cast<int>(cargv.size()), cargv.data());
    }
}
