Caprice32 → iOS 7.1.2 (armv7) port
Derived from the iOS 9.3 port in this repo. Same emulator core, same Theos/CI pipeline; the deployment target is lowered to iOS 7.0 so the binary loads on 7.1.2.

Read this first: iPad 1
Apple shipped iPad 1 (A4, 256 MB RAM) with iOS up to 5.1.1; it never officially ran 7.x. If your iPad 1 is really on 7.1.2 it is a custom-firmware / jailbroken setup, and you should expect:

Install path: fake-signed (ldid) build => needs a jailbreak (.deb via Cydia/dpkg, or the .ipa via AppSync/Installous-style installer). A normal sideload will not work on 7.x.
Memory: 256 MB total, roughly 100-130 MB usable by an app before jetsam kills it. The emulator core itself is small (a few MB for 128 KB CPC RAM) but SDL2 + GLES textures + ROMs add up; keep ram_size=128 and do not enable 6128+/cartridge modes unless needed.
Speed: A4 @ 1 GHz, single core, no NEON use in this code. Video is forced to 384x270 (CAPRICE_FAST_VIDEO) and audio to 22.05 kHz (CAPRICE_IOS7). CPC 4 MHz real-time speed should be reachable for most software, but heavy demos may drop frames.
What changed vs. the 9.3 port
Area	iOS 9.3 build	iOS 7.1.2 build
Deployment target	9.3	7.0 (SDK headers still 9.3)
AppDelegate.mm	UIAlertController, openURL:options:	UIAlertView, openURL:sourceApplication:annotation: (+ 9.x path kept)
Security-scoped URLs	called directly	respondsToSelector: guarded
SDL2 2.0.9 UIKit	as-is	nativeScale/nativeBounds -> scale/bounds; message-box backend stubbed (its UIAlertController ref is a strong import => dyld: Symbol not found on 7.x)
Overlay keyboard	relies on landscape UIScreen.bounds	iOS 7 UIScreen.bounds is always portrait and a 2nd UIWindow is not auto-rotated => rotated by hand from statusBarOrientation
Toggle buttons	emoji ⌨ 🎮	text KBD / PAD (U+2328 does not exist before iOS 9.1)
Info.plist	no UIDeviceFamily	UIDeviceFamily=[2] (else iPhone-compat 2x mode on iPad); launch image size {1024,768}; iOS 11-only key removed
Frameworks	GameController linked	-weak_framework GameController (exists on 7.0+, weak for safety)
Audio	44.1 kHz	forced 22.05 kHz under -DCAPRICE_IOS7
Emulator core (src/*.cpp) is untouched except one #ifdef CAPRICE_IOS7 block in loadConfiguration() (audio rate).

Build
Same as before: push to the touchhle branch or run the "Build iOS .ipa (armv7, iOS 7.1.2 / iPad 1)" workflow manually. Artifacts: Caprice32-*-armv7-iOS7.1.2.ipa and .deb.

Two new CI steps help diagnose problems without a device:

Verify deployment target prints LC_VERSION_MIN_IPHONEOS; it must say 7.0. If it says 9.3, dyld on 7.1.2 will refuse to launch it.
Check binary for symbols missing on iOS 7.1.2 lists strong imports of iOS 8+ classes and fails the build on the fatal ones.
Not verified
I could not compile or run this here (no iOS toolchain/network in my sandbox), so treat it as "should work, untested on hardware":

SDL2 sed patches assume the 2.0.9 source layout; the CI log prints what remains referencing iOS 8+ APIs (possible iOS 8+ API references) so any leftover is visible at once.
The manual overlay rotation is written from iOS 7 UIKit behaviour, not observed on a device. If the on-screen keyboard appears sideways or upside-down, flip LandscapeTransform() in CPCVirtualKeyboard.mm (swap the M_PI_2 / -M_PI_2 cases).
GLES2 on the A4 (SGX535) works but is slow; if the picture is black, add SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles") (GLES1) in cap32.cpp next to the existing hint.
You still need your own CPC ROMs in rom/ (copyrighted, see README_PORT.md).
