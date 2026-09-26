# Caprice32 → iOS 9.3 (armv7, 32-bit) port

## What's in this folder

```
ios/Caprice32iOS/
├── Caprice32iOS/
│   ├── main_ios.mm            # iOS entry point (replaces main.cpp)
│   ├── AppDelegate.mm         # SDLUIKitDelegate subclass, handles "Open in..."
│   ├── Info.plist             # iOS 9.3 / armv7 target config
│   └── LaunchScreen.storyboard
```

## Why this was a good candidate (vs. the "1984" emulator)

Audited the whole `src/` tree before writing anything:

- Uses **SDL2**, not SDL3. SDL2's iOS backend supported iOS 8/9 and armv7
  for years; SDL3 dropped that low a target already.
- All timing/audio/joystick/threading goes through SDL2's own API
  (`SDL_GetTicks`, `SDL_Delay`, `SDL_OpenAudioDevice`, `SDL_JoystickOpen`) —
  nothing raw-pthread or platform-specific in the emulation core.
- Rendering is `SDL_Surface`/`SDL_Renderer` software blitting
  (`Direct`, `Scale2x`, `TV 2x`, etc.). The one OpenGL plugin is gated by
  `HAVE_GL`, and `src/glfuncs.h` **already `#undef`s `HAVE_GL` for every
  `__APPLE__` target** (added when macOS dropped desktop GL) — since iOS
  also defines `__APPLE__`, this is already disabled for free. No patch
  needed.
- `dlopen`/`dlsym`/`dlfcn.h` appear only as an unused stray include in
  `ipf.cpp` — grep confirms zero actual calls. `capsimg`/IPF sources
  compile straight into the main binary already (picked up by the
  Makefile's `find src -name *.cpp` glob), so there's no dynamic-library
  loading to strip out for App Store / sandboxing compliance.
- `getopt.h`/`getopt_long` — part of BSD libc, present on iOS unchanged,
  no shim needed (I was wrong to flag this earlier).
- Config/path resolution (`getConfigurationFilename()` in `cap32.cpp`)
  already walks `$HOME`-relative paths via plain `getenv("HOME")`. iOS
  just needs `HOME` set to the app sandbox at launch — handled in
  `main_ios.mm`, zero changes to `cap32.cpp` itself.
- `cap32_main(argc, argv)` calls `SDL_Init` and runs its own blocking
  `while (true)` loop, same shape SDL2 expects from a UIKit `main()` —
  no event-loop restructuring needed.

Net result: **no patches to the emulator core were needed.** The whole
port is additive — new iOS-side files only.

## What `main_ios.mm` does that `main.cpp` didn't need to

1. Sets `HOME` to the app's Documents directory (sandbox-writable).
2. On first launch, seeds `cap32.cfg` and `roms/` from the app bundle
   into Documents, since the bundle itself is read-only at runtime.
3. Builds a synthetic `argv` (iOS gives you none) with `argv[0]`
   pointing at the bundle's Resources path, so `binPath` in `cap32.cpp`
   resolves sensibly.
4. Checks for a pending "opened file" path (set by `AppDelegate.mm` when
   the user does Files.app → Share → Open in Caprice32) and appends it
   as a positional slot arg, matching desktop `cap32 game.dsk` behavior.

## What you still need to provide (source assets, not code)

- **CPC OS ROMs** (`cpc464.rom`/`cpc6128.rom`/etc.) — not included in
  this repo or my output; Amstrad's OS ROMs are copyrighted and aren't
  bundled by Caprice32 upstream either. Drop your own legally-obtained
  ROM files into `Caprice32iOS/roms/` before building, matching whatever
  `cap32.cfg` expects.
- `cap32.cfg` — copy the repo's `cap32.cfg.tmpl`/`cap32.cfg`, add it to
  the Xcode target as a bundled resource.

## Xcode project setup (mechanical steps — old Xcode, not covered by my sandbox)

I did not hand-author a `.pbxproj` — Xcode's project file format is
extremely sensitive to being hand-edited (UUID cross-references,
navigator group state) and a guessed one is more likely to fail to open
in Xcode 7/8 than to save you time. These are the exact settings to
punch in once you create the project in real Xcode:

1. **Xcode 7.3.1** (last version with full 32-bit iOS SDK + armv7
   codegen) on macOS 10.11/10.12, or Xcode 8.x if you confirm it still
   emits armv7 — Xcode 9 was the last with *any* 32-bit device support,
   Xcode 10 removed it outright.
2. New project → iOS → Single View Application, Objective-C, disable
   Swift/Storyboards for the main view (SDL2 replaces it).
3. Build Settings:
   - `IPHONEOS_DEPLOYMENT_TARGET = 9.3`
   - `ARCHS = armv7` (do **not** add `arm64` — no arm64 device runs 9.3
     anyway, and mixing archs complicates the SDL2 static lib build)
   - `VALID_ARCHS = armv7`
   - `ENABLE_BITCODE = NO`
   - C++ Language Dialect: `GNU++17` (needed for `<filesystem>` in
     `cap32.cpp` — this is a *compiler* feature, unaffected by the OS
     deployment target)
   - C++ Standard Library: `libc++`
4. Add SDL2 (build `SDL2.xcodeproj` from SDL2's own `Xcode-iOS/` folder
   against the same old Xcode/SDK, or a prebuilt armv7 `libSDL2.a` from
   that era) as a static library dependency. Do the same for
   `libpng`, `zlib` (zlib ships with the iOS SDK already), and
   `freetype2` (build as a static lib; iOS doesn't ship it).
5. Add all of `src/*.cpp` and `src/capsimg/**/*.cpp` (skip `src/gui/`
   unless you're porting the wGui debugger UI too — not required for a
   playable emulator) plus `main_ios.mm` and `AppDelegate.mm` to the
   target. **Do not add `main.cpp`** (desktop entry point) — it
   conflicts with `main_ios.mm`.
6. Header search paths: `src/`, `src/capsimg/LibIPF`,
   `src/capsimg/Device`, `src/capsimg/CAPSImg`, `src/capsimg/Codec`,
   `src/capsimg/Core` (mirrors `CAPS_INCLUDES` in the Makefile).
7. Link `libSDL2.a`, `libpng.a`, `libz.tbd`, `libfreetype.a`, plus
   iOS frameworks SDL2 itself needs: `AudioToolbox`, `AVFoundation`,
   `CoreAudio`, `CoreGraphics`, `CoreMotion`, `Foundation`, `GameController`,
   `OpenGLES` (SDL2 links it internally for context creation even though
   Caprice32's own GL plugin is disabled), `QuartzCore`, `UIKit`.
8. Set `Info.plist` (provided) as the target's Info.plist, and add
   `LaunchScreen.storyboard` (provided) as the launch screen.

## Known follow-up items (not blockers, just not done)

- No on-screen touch controls yet (virtual joystick/keyboard overlay).
  SDL2 will happily run without them, but you'll have nothing to
  press — an MFi controller works out of the box via SDL2's joystick
  API, but touch input needs a small UIKit overlay calling
  `SDL_PushEvent` with synthetic key events, same shape as the
  existing `virtualKeyboardEvents` mechanism `cap32.cpp` already uses
  for `--autocmd`.
- `src/gui/` (the in-emulator wGui debugger/config UI) wasn't ported;
  it's built on the same SDL_Surface primitives so it's not
  fundamentally blocked, just extra scope I didn't include here.
- Hot-swapping a disk while the app is already running (vs. cold
  launch) isn't wired up — noted in `AppDelegate.mm`.
