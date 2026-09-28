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

// SDL2's iOS entry point (src/main/ios/SDL_uikit_main.m) defines the
// real `main(argc, argv)` itself and calls into UIApplicationMain,
// which eventually invokes *our* app code on a dedicated thread via
// the name `SDL_main` (see SDL_main.h, which #defines main to
// SDL_main after inclusion). Without including SDL_main.h, this file's
// `int main(...)` below was a second, competing definition of the
// plain C `main` symbol - harmless to compile, but it meant our code
// was never the one SDL_uikit_main.m actually called, and conversely
// left SDL2's own SDL_main() reference unresolved at link time.
// Including SDL_main.h makes the macro substitution apply to the
// definition below, so it becomes SDL_main and hooks into SDL's real
// iOS launch sequence correctly.
#include <SDL2/SDL_main.h>

#import <Foundation/Foundation.h>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>

// cap32_main() is a plain C++ function (declared in src/cap32.h,
// defined in src/cap32.cpp) - it was never given C linkage, so
// declaring it extern "C" here made this translation unit look for
// the linker symbol `_cap32_main` while cap32.cpp actually emits the
// C++-mangled name. Include the real header instead of hand-declaring
// a mismatched prototype, so both sides always agree on linkage.
#include "src/cap32.h"

// ---------------------------------------------------------------------
// First-run seeding: copy bundled defaults (cap32.cfg, rom/) into the
// writable Documents directory the first time the app launches. After
// this, cap32's own config/rom search (which walks $HOME) finds them
// as if they'd always been there.
// ---------------------------------------------------------------------
static void SeedWritableSupportFilesIfNeeded(NSString *documentsPath)
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSBundle *bundle = [NSBundle mainBundle];

    // cap32.cfg
    //
    // IMPORTANT: getConfigurationFilename() in cap32.cpp never checks
    // "$HOME/cap32.cfg" directly - only "$HOME/.config/cap32.cfg" and
    // "$HOME/.cap32.cfg" (plus chAppPath/cap32.cfg, which on iOS points
    // nowhere useful - see chAppPath below). Seeding a plain
    // "cap32.cfg" here meant NONE of those candidates ever matched, so
    // the app always fell through to an empty config, which in turn
    // pointed rom_path at a directory that was never actually seeded.
    // cap32_main() then failed to open the OS ROMs and crashed before
    // any UI/log output - exactly the "flash then crash" symptom this
    // is fixing. Seed to the dotfile name cap32.cpp actually looks for.
    NSString *destCfg = [documentsPath stringByAppendingPathComponent:@".cap32.cfg"];
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

    // rom/ directory (OS ROMs required to boot a CPC at all).
    //
    // IMPORTANT: the Makefile bundles ROMs via
    // `Caprice32ARMv7_RESOURCE_FILES = ... $(wildcard rom/*)`, which
    // copies each ROM file individually into the app bundle's
    // Resources ROOT (Resources/cpc6128.rom, Resources/amsdos.rom,
    // etc.) - there is no Resources/rom/ subdirectory in the bundle.
    // The old check here looked for `resourcePath/rom` as a single
    // directory to copy wholesale; that path never existed, so the
    // copy silently no-op'd (fileExistsAtPath was just false - no
    // error to log) and Documents/rom/ was never created at all. That
    // is exactly why cap32.cpp reported "Couldn't open ROM file
    // '.../Documents/rom//cpc6128.rom'" - the directory was empty.
    // Instead, create Documents/rom/ ourselves and copy each known ROM
    // file in from the bundle root.
    NSArray<NSString *> *romFiles = @[ @"cpc464.rom", @"cpc664.rom", @"cpc6128.rom",
                                        @"amsdos.rom", @"MF2.rom" ];
    NSString *destRoms = [documentsPath stringByAppendingPathComponent:@"rom"];
    BOOL isDir = NO;
    if (![fm fileExistsAtPath:destRoms isDirectory:&isDir] || !isDir) {
        NSError *dirErr = nil;
        [fm createDirectoryAtPath:destRoms withIntermediateDirectories:YES attributes:nil error:&dirErr];
        if (dirErr) {
            NSLog(@"Caprice32: failed to create rom/ dir: %@", dirErr);
        }
    }
    for (NSString *romName in romFiles) {
        NSString *destRomPath = [destRoms stringByAppendingPathComponent:romName];
        if ([fm fileExistsAtPath:destRomPath]) continue;
        NSString *srcRomPath = [[bundle resourcePath] stringByAppendingPathComponent:romName];
        if (![fm fileExistsAtPath:srcRomPath]) {
            NSLog(@"Caprice32: expected ROM not found in bundle: %@", srcRomPath);
            continue;
        }
        NSError *err = nil;
        [fm copyItemAtPath:srcRomPath toPath:destRomPath error:&err];
        if (err) {
            NSLog(@"Caprice32: failed to seed %@: %@", romName, err);
        }
    }
}

// stdout/stderr handling.
//
// On-device (Springboard) launches have no attached console, so anything
// written to stderr/stdout is lost. Redirecting it to a file in Documents
// makes it visible - but it is EXPENSIVE: every LOG_* / fprintf(stderr)
// becomes an unbuffered write() to flash, and the file grows forever. On
// an iPad mini 1 (armv7, A5) that alone is enough to wreck the frame rate.
//
// So by default the streams are pointed at /dev/null (cheap, and keeps
// std::cerr/std::cout from ever touching the disk). To get the file log
// back for debugging, either build with -DCAPRICE_FILE_LOG, or create an
// empty file named "enable_log" in the app's Documents folder.
static void RedirectStdioToDocuments(NSString *documentsPath)
{
    NSString *logPath = [documentsPath stringByAppendingPathComponent:@"caprice32.log"];
    NSString *flagPath = [documentsPath stringByAppendingPathComponent:@"enable_log"];

    BOOL wantFileLog = NO;
#ifdef CAPRICE_FILE_LOG
    wantFileLog = YES;
#endif
    if ([[NSFileManager defaultManager] fileExistsAtPath:flagPath]) {
        wantFileLog = YES;
    }

    if (wantFileLog) {
        // Truncate ("w") so the log does not grow across runs, and leave
        // the streams fully buffered - no _IONBF.
        freopen(logPath.UTF8String, "w", stdout);
        freopen(logPath.UTF8String, "a", stderr);
        setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
        setvbuf(stderr, nullptr, _IOLBF, 1 << 12);
        NSLog(@"Caprice32: logging to %@", logPath);
    } else {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
    }
}

int main(int argc, char *argv[])
{
    @autoreleasepool {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                              NSUserDomainMask, YES);
        NSString *documentsPath = paths.firstObject;

        RedirectStdioToDocuments(documentsPath);

        SeedWritableSupportFilesIfNeeded(documentsPath);

        // cap32.cpp's getConfigurationFilename() looks at $HOME/.config
        // and $HOME/.cap32.cfg, in that order, before giving up. Point
        // HOME at Documents so both the sandbox-seeded cap32.cfg above
        // and any user-imported DSK/CDT/SNA files live in one place
        // that's visible from Files.app (if the app enables document
        // sharing) and survives app updates.
        setenv("HOME", documentsPath.UTF8String, 1);

        // cap32_main() falls back to getcwd() for chAppPath (used as
        // the default base for rom_path/dsk_path/snap_path/etc. when
        // the config doesn't override them). On iOS the process's cwd
        // is not Documents and is not guaranteed to be anything
        // sane/writable, so force it to Documents explicitly - this is
        // the same directory SeedWritableSupportFilesIfNeeded() just
        // populated with rom/, so the defaults actually resolve to
        // real, seeded files instead of a directory that was never
        // created.
        chdir(documentsPath.UTF8String);

        // Caprice32 also derives a config search entry from argv[0]'s
        // parent directory (see `binPath` in cap32.cpp) - point that at
        // the bundle's Resources directory, where any bundled
        // cap32.cfg/rom fallback copy lives read-only.
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
