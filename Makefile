export THEOS_DEVICE_IP =
ARCHS = armv7
TARGET = iphone:clang:9.3:9.3
INSTALL_TARGET_PROCESSES = Caprice32ARMv7

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = Caprice32ARMv7

# --- Caprice32 core sources -------------------------------------------
# Mirrors the desktop makefile's `find src -name *.cpp` glob, minus
# main.cpp (replaced by main_ios.mm) and src/gui (wGui debugger UI,
# not ported - not required for a playable emulator).
Caprice32ARMv7_FILES = $(filter-out src/gui/%,$(wildcard src/*.cpp src/capsimg/*/*.cpp))
Caprice32ARMv7_FILES += main_ios.mm AppDelegate.mm

# SDK_ROOT is passed in from the workflow (see
# .github/workflows/ios-build.yml, "Stage libs into Theos SDK lib dir").
# It points at the iPhoneOS9.3.sdk directory where our cross-compiled
# SDL2/freetype2/libpng/zlib were staged under usr/lib and usr/include.
# Deliberately NOT using THEOS_STAGING_DIR here - that's a reserved
# Theos system variable for its own package staging output, unrelated
# to the SDK path, and overwriting it breaks Theos's SDK auto-detection.
SDK_ROOT ?= $(THEOS)/sdks/iPhoneOS9.3.sdk

Caprice32ARMv7_CFLAGS = \
	-Isrc \
	-Isrc/capsimg/LibIPF -Isrc/capsimg/Device -Isrc/capsimg/CAPSImg \
	-Isrc/capsimg/Codec -Isrc/capsimg/Core \
	-I$(SDK_ROOT)/usr/include \
	-I$(SDK_ROOT)/usr/include/SDL2 \
	-I$(SDK_ROOT)/usr/include/freetype2 \
	-DNDEBUG

Caprice32ARMv7_CXXFLAGS = $(Caprice32ARMv7_CFLAGS) -std=gnu++17

# Static libs staged into $THEOS/vendor/lib/armv7 by the CI steps
# (two cross-compiled from source: zlib, libpng; two prebuilt: SDL2,
# freetype2 - see .github/workflows/ios-build.yml for how each lands
# in $(SDK_ROOT)/usr/lib).
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
Caprice32ARMv7_RESOURCE_FILES = Resources/LaunchScreen.storyboard Resources/cap32.cfg $(wildcard roms/*)

include $(THEOS_MAKE_PATH)/application.mk

after-install::
	install.exec "killall -9 Caprice32ARMv7 || true"
