#!/usr/bin/env bash
# Builds core+shim with the null backend (no exceptions/RTTI, like the target) and runs 50 frames.
set -e
S=symbian/shim; O=$(mktemp -d)
INC="-I$S -Isrc -Isrc/capsimg/LibIPF -Isrc/capsimg/Device -Isrc/capsimg/CAPSImg -Isrc/capsimg/Codec -Isrc/capsimg/Core -I."
printf '#include "cap32.h"\nint main(int c,char**v){return cap32_main(c,v);}\n' > $O/m.cc
cp symbian/app/backend_null.cc $O/ 2>/dev/null || cp $S/backend_null.cc $O/
(ls src/*.cpp src/capsimg/*/*.cpp | grep -v glfuncs; echo $O/m.cc; echo $S/sdl_shim.cc; echo $O/backend_null.cc; echo symbian/app/vkbd_symbian.cc; echo $S/vkbd.cc) | \
  xargs -P4 -I{} sh -c 'g++ -std=c++17 -O1 -w -fno-exceptions -fno-rtti -DCAPRICE_NO_WGUI '"$INC"' -c {} -o '$O'/$(echo {}|tr / _).o'
g++ $O/*.o -lz -lpthread -o $O/cap32sym
HOME=$O timeout 30 $O/cap32sym 2>&1 | tail -3 | grep -q cleanExit && echo "SMOKE OK"
