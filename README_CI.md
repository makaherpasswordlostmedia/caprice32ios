# CI setup notes

## Update: SDL2 is now built from real source, not stubbed

Earlier version of this workflow had two steps that deliberately failed
with `exit 1` instead of linking a guessed/unverified prebuilt
`libSDL2.a`/`libfreetype.a`. That's fixed now:

- **SDL2**: cloned directly from `https://github.com/libsdl-org/SDL`,
  branch `SDL2` (the real, official upstream branch you pointed me at —
  not a mirror). Since this runner has no Xcode, and SDL2's own iOS
  build paths (`Xcode-iOS/SDL/SDL.xcodeproj`, or CMake's
  `CMAKE_OSX_SYSROOT` which shells out to `xcrun` internally) both
  assume one, the workflow instead compiles SDL2's iOS-relevant source
  files directly against the Theos clang cross toolchain — same
  approach already used for zlib/libpng, just with a larger file list.
  The subsystem list (`video/uikit`, `joystick/iphoneos`,
  `audio/coreaudio`, `render/opengles*`, `power/uikit`,
  `filesystem/cocoa`, etc.) mirrors the actual group structure in
  SDL2's own `Xcode-iOS/SDL/SDL.xcodeproj` (verified against that
  project file's contents, not guessed).
- **freetype2**: cross-compiled from its normal autotools source, same
  pattern as libpng/zlib — freetype's build has no Xcode dependency at
  all, so there was never a good reason to treat it differently.

## What's still worth double-checking before you trust this fully

I don't have a Linux/Theos environment in front of me to actually run
this workflow end to end, so treat it as "should work, unverified" not
"verified working":

- **File list completeness for SDL2.** I included the subsystems needed
  for a typical windowed/audio/joystick SDL2 iOS app. If Caprice32 hits
  an undefined-symbol linker error, it's most likely one of:
  - `src/video/yuv2rgb/*.c` (only needed if you enable YUV overlay
    rendering — Caprice32 doesn't use this)
  - `src/audio/SDL_audiotypecvt.c` / resampler internals — should
    already be pulled in by the plain `src/audio/*.c` glob
  - Metal-specific renderer files (`src/render/metal/*.m`) — correctly
    excluded, Metal doesn't exist on iOS 9.3 hardware anyway
  If you hit a missing symbol, `nm -u libSDL2.a | grep <symbol>`, then
  grep SDL2's source tree for where it's defined and add that directory
  to `SRC_DIRS` in the workflow.
- **`SDL_dynapi`**: SDL2 includes a "dynamic API" indirection layer
  (`src/dynapi/SDL_dynapi.c`) that's normally *disabled* for static iOS
  builds (it's meant for shared-library builds where app and lib may be
  compiled with different SDL2 header versions). Since we're building
  and linking statically, dynapi being compiled in should be harmless
  (it no-ops unless `SDL_DYNAMIC_API=1` is set at compile time, which we
  don't set), but if you see weird double-indirection linker behavior,
  the fix is `-DSDL_DYNAMIC_API=0` and/or dropping `src/dynapi/*.c` from
  the build entirely.
- **`-fobjc-arc`**: added because SDL2's UIKit backend (`.m` files) is
  written expecting ARC. If any `.m` file was written pre-ARC and
  chokes, that specific file may need `-fno-objc-arc` instead — I can't
  verify this without actually compiling it.

## Everything else, status unchanged from before

- Toolchain, SDK, and module-map fix steps: unaltered from the working
  reference pipeline this was adapted from.
- zlib and libpng cross-compiles: real, standard autotools
  cross-compilation, no exotic steps.
- The Theos `Makefile`: real target list built from grepping Caprice32's
  own source tree and its desktop Makefile's include paths.
- Still need CPC OS ROM files in `roms/` (copyrighted, not included —
  see `README_PORT.md`).
- No touch controls wired up yet — MFi controllers work out of the box
  via SDL2's joystick API; touch input is a follow-up.
