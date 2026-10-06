#!/usr/bin/env python3
"""List sections carrying OS/processor-specific flag bits in ELF files or ar archives.
Usage: elf_flags.py FILE... (prints  file[member] section flags=hex)  - helps find e.g. SHF_GNU_RETAIN."""
import struct, sys
ODD = 0x0ff00000 | 0xf0000000   # SHF_MASKOS | SHF_MASKPROC

def sections(d):
    if d[:4] != b'\x7fELF': return
    is64 = d[4] == 2
    if is64:
        shoff, = struct.unpack_from('<Q', d, 0x28); ent, num, strndx = struct.unpack_from('<HHH', d, 0x3A)
    else:
        shoff, = struct.unpack_from('<I', d, 0x20); ent, num, strndx = struct.unpack_from('<HHH', d, 0x2E)
    raw = []
    for i in range(num):
        o = shoff + i * ent
        raw.append(struct.unpack_from('<IIQQQQ', d, o) if is64 else struct.unpack_from('<IIIIII', d, o))
    so = raw[strndx][4]
    for n, t, f, a, off, sz in raw:
        e = d.index(b'\0', so + n)
        yield d[so + n:e].decode(errors='replace'), f, sz

def members(path):
    d = open(path, 'rb').read()
    if d[:8] != b'!<arch>\n':
        yield path, d; return
    p, names = 8, b''
    while p + 60 <= len(d):
        h = d[p:p + 60]; name = h[:16].decode().strip(); size = int(h[48:58]); body = d[p + 60:p + 60 + size]
        if name == '//': names = body
        elif name not in ('/', '/SYM64/'):
            if name.startswith('/') and name[1:].isdigit():
                s = int(name[1:]); name = names[s:names.index(b'\n', s)].decode().rstrip('/')
            yield f"{path}[{name.rstrip('/')}]", body
        p += 60 + size + (size & 1)

for path in sys.argv[1:]:
    for label, body in members(path):
        try:
            for name, f, sz in sections(body):
                if f & ODD:
                    print(f"{label} {name} flags={f:#x} size={sz:#x}")
        except Exception as ex:
            print(f"{label}: parse error {ex}")
