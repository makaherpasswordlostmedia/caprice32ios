#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <cstring>

using byte = uint8_t;
using word = uint16_t;
using dword = uint32_t;
using qword = uint64_t;

// Unaligned little-endian load/store helpers.
//
// A lot of code in this codebase walks raw byte buffers (tape images,
// snapshot/CDT blocks, zip central-directory records, PSG registers) and
// used to read/write multi-byte fields with `*reinterpret_cast<word*>(p)`
// or `*reinterpret_cast<dword*>(p)`. That compiles to a strict-alignment
// halfword/word load or store on ARM, and p is frequently NOT aligned
// (file formats don't pad to word boundaries, and struct/union layouts
// here often overlay byte arrays). The result is a SIGBUS
// (EXC_ARM_DA_ALIGN) whenever the data happens to land on an odd or
// non-4-aligned address - which depends on file contents, so it doesn't
// show up in every run. memcpy has no alignment requirement on any
// architecture and produces the same bytes (this codebase already assumes
// a little-endian host elsewhere), so use these instead of a typed
// pointer dereference when reading/writing to a raw byte pointer whose
// alignment isn't guaranteed.
static inline word load_le16(const void *p) {
   word v;
   std::memcpy(&v, p, sizeof(v));
   return v;
}
static inline dword load_le32(const void *p) {
   dword v;
   std::memcpy(&v, p, sizeof(v));
   return v;
}
static inline void store_le16(void *p, word v) {
   std::memcpy(p, &v, sizeof(v));
}
static inline void store_le32(void *p, dword v) {
   std::memcpy(p, &v, sizeof(v));
}

#endif // TYPES_H
