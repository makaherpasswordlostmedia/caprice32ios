#!/usr/bin/env python3
"""Move .init_array/.fini_array from the read-only code segment into .data in the SDK's image.ld.

The SDK example has no static constructors, so these (writable) sections are empty there. Caprice32 has
global constructors; symbian-native convert-exe rejects them in the code segment with
"DATA_LOSS: Writable section outside data/BSS layout". Usage: fix_ld.py symbian/glue/image.ld
"""
import re, sys
p = sys.argv[1]
s = open(p).read()
pat = re.compile(r'^[ \t]*\.(init|fini)_array\s*:\s*\{.*?\}\s*:code[ \t]*\n', re.M)
if len(pat.findall(s)) != 2:
    sys.exit("fix_ld: expected .init_array and .fini_array in code segment; not patching\n" + s)
s = pat.sub('', s)
# .got (writable, non-empty in this program) must live in the data segment too; .got.plt/.dynamic stay (import machinery)
s, ngot = re.subn(r'^[ \t]*\.got\s*:\s*\{\s*\*\(\.got\)\s*\}\s*:code[ \t]*\n', '', s, flags=re.M)
extra = (" . = ALIGN(4); __init_array_start = .; KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*))) KEEP(*(.init_array)) __init_array_end = ."
         " __fini_array_start = .; KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*))) KEEP(*(.fini_array)) __fini_array_end = .;" + (" . = ALIGN(4); *(.got)" if ngot else ""))
s2, n = re.subn(r'(\.data\s*:\s*\{[^}]*?)(\. = ALIGN\(4\);)?(\s*\}\s*:data)',
                lambda m: m.group(1) + extra.replace('__init_array_end = .', '__init_array_end = .;') + " . = ALIGN(4);" + m.group(3), s, count=1)
if n != 1:
    sys.exit("fix_ld: .data block not found\n" + s)
open(p, 'w').write(s2)
print(s2)
