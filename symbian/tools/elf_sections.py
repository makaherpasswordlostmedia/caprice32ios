#!/usr/bin/env python3
"""Print ELF section headers (32/64-bit LE) and flag writable allocated sections. No dependencies."""
import struct, sys
d = open(sys.argv[1], 'rb').read()
is64 = d[4] == 2
if is64:
    shoff, = struct.unpack_from('<Q', d, 0x28); shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x3A)
else:
    shoff, = struct.unpack_from('<I', d, 0x20); shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2E)
secs = []
for i in range(shnum):
    o = shoff + i * shentsize
    if is64:
        n, t, f, a, off, sz = struct.unpack_from('<IIQQQQ', d, o)
    else:
        n, t, f, a, off, sz = struct.unpack_from('<IIIIII', d, o)
    secs.append([n, t, f, a, off, sz])
stroff = secs[shstrndx][4]
def name(n):
    e = d.index(b'\0', stroff + n); return d[stroff + n:e].decode()
TYPES = {0:'NULL',1:'PROGBITS',2:'SYMTAB',3:'STRTAB',4:'RELA',5:'HASH',6:'DYNAMIC',7:'NOTE',8:'NOBITS',9:'REL',11:'DYNSYM',14:'INIT_ARRAY',15:'FINI_ARRAY',0x70000001:'ARM_EXIDX',0x70000003:'ARM_ATTR'}
print(f"{'name':24}{'type':12}{'flags':6}{'addr':>10}{'size':>10}")
for n, t, f, a, off, sz in secs:
    fl = ('W' if f & 1 else '') + ('A' if f & 2 else '') + ('X' if f & 4 else '') + ('T' if f & 0x400 else '')
    mark = ('   <== writable' if (f & 1 and f & 2) else '') + ('   <== WRITABLE NON-ALLOC' if (f & 1 and not f & 2) else '')
    if f & 2 or mark or '--all' in sys.argv:
        print(f"{name(n):24}{TYPES.get(t, hex(t)):12}{fl:6}{a:>#10x}{sz:>#10x}{mark}")
