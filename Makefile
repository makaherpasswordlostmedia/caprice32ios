export THEOS_DEVICE_IP =
ARCHS = armv7
TARGET = iphone:clang:9.3:9.3
INSTALL_TARGET_PROCESSES = Caprice32ARMv7

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = Caprice32ARMv7

# Mirrors the desktop makefile's `find src -name *.cpp` glob, minus
# main.cpp (replaced by main_ios.mm) and src/gui (wGui debugger UI,
# not ported - not required for a playable emulator). devtools.cpp and
# the equivalent call sites in cap32.cpp still compile - they're
# stubbed out under -DCAPRICE_NO_WGUI (see those files) rather than
# excluded, so DevTools/showGui/etc. stay real, linkable no-ops instead
# of needing every call site in cap32.cpp's main loop touched.
Caprice32ARMv7_FILES = $(filter-out src/gui/%,$(wildcard src/*.cpp src/capsimg/*/*.cpp))
Caprice32ARMv7_FILES += main_ios.mm AppDelegate.mm ios93_availability_stub.m CPCVirtualKeyboard.mm

# SDK_ROOT is passed in from the workflow (see
# .github/workflows/ios-build.yml, "Stage libs into Theos SDK lib dir").
# It points at the iPhoneOS9.3.sdk directory where our cross-compiled
# SDL2/freetype2/libpng/zlib were staged under usr/lib and usr/include.
# Deliberately NOT using THEOS_STAGING_DIR here - that's a reserved
# Theos system variable for its own package staging output, unrelated
# to the SDK path, and overwriting it breaks Theos's SDK auto-detection.
SDK_ROOT ?= $(THEOS)/sdks/iPhoneOS9.3.sdk

# ADDITIONAL_CFLAGS is appended last on Theos's actual compile command
# line (after internal target flags), so this wins even if Theos's own
# darwin_head.mk implicitly adds -fmodules for this Apple-style iphone
# target ahead of our instance _CFLAGS above.
#
# Theos's iphone:clang target enables -Werror by default. SDL2's own
# SDL_uikitappdelegate.h declares `@property (nonatomic) UIWindow
# *window;` with no explicit strong/retain/assign - a long-standing
# upstream SDL2 quirk that's harmless in practice (every real SDL2 iOS
# build just emits it as a warning) but fails outright under -Werror.
# Downgrade only these two specific warning classes back to warnings
# instead of disabling -Werror wholesale.
ADDITIONAL_CFLAGS += -fno-modules -fno-cxx-modules -fno-implicit-modules -fno-implicit-module-maps \
	-Wno-error=objc-property-no-attribute -Wno-error=property-attribute-mismatch

# AppDelegate.mm needs the same compat shim the SDL2 build step
# force-includes when it compiles SDL_uikitappdelegate.m - this iOS 9.3
# SDK mirror is missing the UIApplicationOpenURLOptionsKey typedef
# (real iOS 9.0+ API) that AppDelegate.mm's own
# -application:openURL:options: override references. Theos only
# compiles this file through its own rules, so the CI-side `-include`
# used for the SDL2 build never reaches it. Theos's per-file flag
# syntax is "File.extension_CFLAGS" (see theos.dev/docs/variables,
# "Local Variables"), which applies to every file matching that
# name+extension in this project instance.
AppDelegate.mm_CFLAGS += -include $(THEOS_PROJECT_DIR)/ios93_compat_shim.h

# CPCVirtualKeyboard.mm is written for ARC (no manual retain/release, and
# it keeps its window/controller alive via file-static strong refs).
# Theos does not enable ARC by default for this target, and without it
# the overlay would be silently deallocated. Scope ARC to this file only
# so the rest of the port is left exactly as it was.
CPCVirtualKeyboard.mm_CFLAGS += -fobjc-arc

Caprice32ARMv7_CFLAGS = \
	-I. \
	-Isrc \
	-Isrc/gui/includes \
	-Isrc/capsimg/LibIPF -Isrc/capsimg/Device -Isrc/capsimg/CAPSImg \
	-Isrc/capsimg/Codec -Isrc/capsimg/Core \
	-I$(SDK_ROOT)/usr/include \
	-I$(SDK_ROOT)/usr/include/SDL2 \
	-I$(SDK_ROOT)/usr/include/freetype2 \
	-fno-modules -fno-cxx-modules -fno-implicit-modules -fno-implicit-module-maps \
	-DNDEBUG -DCAPRICE_NO_WGUI \
	-DCAPRICE_NO_LOG -DCAPRICE_FORCE_KEY_JOYSTICK -DCAPRICE_FAST_VIDEO

# Debug switches (all OFF by default for speed on iPad mini 1 / armv7):
#   -UCAPRICE_NO_LOG      re-enable LOG_ERROR/WARNING/INFO/VERBOSE
#   -DCAPRICE_CRASHLOG    per-instruction crash trail (slow!)
#   -UCAPRICE_FAST_VIDEO  back to 2x2 "Double size" rendering (slower, sharper)
#   -DCAPRICE_FILE_LOG    always write stdout/stderr to Documents/caprice32.log
#                         (or just drop an empty 'enable_log' file in Documents)

Caprice32ARMv7_CXXFLAGS = $(Caprice32ARMv7_CFLAGS) -std=gnu++17

# Static libs staged into $THEOS/vendor/lib/armv7 by the CI steps
# (two cross-compiled from source: zlib, libpng; two prebuilt: SDL2,
# freetype2 - see .github/workflows/ios-build.yml for how each lands
# in $(SDK_ROOT)/usr/lib).
# Availability builtin (___isPlatformVersionAtLeast) needed by SDL2's
# UIKit backend is provided by our own ios93_availability_stub.m
# (this bare cross-toolchain ships no compiler-rt archive at all to
# link it from) - see that file for details. No extra linker flag
# needed here; it's just another translation unit in FILES above.
Caprice32ARMv7_LDFLAGS = \
	-L$(SDK_ROOT)/usr/lib \
	-lSDL2 -lfreetype -lpng16 -lz \
	-framework UIKit \
	-framework Foundation \
	-framework QuartzCore \
	-framework CoreGraphics \
	-framework CoreAudio \
	-framework AudioToolbox \
	-framework AVFoundation \
	-framework GameController \
	-framework CoreMotion \
	-framework OpenGLES

Caprice32ARMv7_CODESIGN_FLAGS = -Sentitlements.plist

Caprice32ARMv7_PLIST = Resources/Info.plist
Caprice32ARMv7_RESOURCE_FILES = Resources/LaunchImage.png Resources/LaunchImage@2x.png Resources/cap32.cfg $(wildcard rom/*)

include $(THEOS_MAKE_PATH)/application.mk

after-install::
	install.exec "killall -9 Caprice32ARMv7 || true"
