#!/usr/bin/env python3
"""Applies the minimal source patches needed for the Symbian build. Idempotent. Usage: apply.py <repo-root>"""
import sys, pathlib
root = pathlib.Path(sys.argv[1])
def sub(path, old, new, count=1):
    p = root / path; s = p.read_text()
    if new in s: return
    assert old in s, f"{path}: pattern not found: {old!r}"
    p.write_text(s.replace(old, new, count))
# 1. Symbian EXE data+bss limit is 1 MiB -> move the 2 MiB IPF decode buffer to the heap.
sub("src/ipf.cpp", "static byte abDecoded[0x200000];  // 2MB",
    "static byte* abDecoded = nullptr;  // 2MB, heap-allocated lazily (Symbian 1 MiB data/bss limit)")
sub("src/ipf.cpp", "  uPos = uDecoded = 0;",
    "  if (!abDecoded) abDecoded = new byte[0x200000];\n  uPos = uDecoded = 0;")
# 2. No libpng on this target: real zlib-only PNG writer replaces savepng.cpp.
import shutil
shutil.copyfile(pathlib.Path(__file__).with_name("savepng.cpp"), root / "src/savepng.cpp")
# 3. Net4CPC needs BSD sockets (not in the SDK libc): replace with an inert stub (no network card).
(root / "src/net4cpc.cpp").write_text('''// Symbian: Net4CPC (W5100S) emulation disabled.
#include "net4cpc.h"
void net4cpc_reset() {}
byte net4cpc_in(byte) { return 0xFF; }
void net4cpc_out(byte, byte) {}
''')
print("patched")
