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

Caprice32ARMv7_CFLAGS = \
	-Isrc \
	-Isrc/capsimg/LibIPF -Isrc/capsimg/Device -Isrc/capsimg/CAPSImg \
	-Isrc/capsimg/Codec -Isrc/capsimg/Core \
	-I$(THEOS_STAGING_DIR)/usr/include \
	-I$(THEOS_STAGING_DIR)/usr/include/SDL2 \
	-I$(THEOS_STAGING_DIR)/usr/include/freetype2 \
	-DNDEBUG

Caprice32ARMv7_CXXFLAGS = $(Caprice32ARMv7_CFLAGS) -std=gnu++17

# Static libs staged into $THEOS/vendor/lib/armv7 by the CI steps
# (two cross-compiled from source: zlib, libpng; two prebuilt: SDL2,
# freetype2 - see .github/workflows/ios-build.yml for how each lands
# in $(THEOS_STAGING_DIR)/usr/lib).
Caprice32ARMv7_LDFLAGS = \
	-L$(THEOS_STAGING_DIR)/usr/lib \
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
