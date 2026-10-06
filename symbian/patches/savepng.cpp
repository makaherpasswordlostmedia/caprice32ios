// Real PNG writer without libpng: uses zlib only. Supports 8-bit palette, 16-bit 565 and 32-bit surfaces.
#include "savepng.h"
#include "SDL.h"
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
static void put32(unsigned char* p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static bool chunk(FILE* f, const char* type, const unsigned char* d, uint32_t n) {
  unsigned char hdr[8]; put32(hdr, n); memcpy(hdr + 4, type, 4);
  uLong c = crc32(0, hdr + 4, 4); if (n) c = crc32(c, d, n);
  unsigned char tail[4]; put32(tail, (uint32_t)c);
  return fwrite(hdr, 1, 8, f) == 8 && (!n || fwrite(d, 1, n, f) == n) && fwrite(tail, 1, 4, f) == 4;
}
static unsigned expand(Uint32 v, Uint32 mask) { if (!mask) return 0; int sh = 0; Uint32 m = mask; while (!(m & 1)) { m >>= 1; sh++; } return (((v & mask) >> sh) * 255 + m / 2) / m; }
int SDL_SavePNG(SDL_Surface* s, const std::string& file) {
  if (!s || !s->pixels) return SDL_SetError("no surface");
  size_t stride = (size_t)s->w * 3 + 1, raw = stride * s->h;
  unsigned char* buf = (unsigned char*)malloc(raw);
  if (!buf) return SDL_SetError("oom");
  const SDL_PixelFormat* pf = s->format;
  for (int y = 0; y < s->h; y++) {
    unsigned char* o = buf + (size_t)y * stride; *o++ = 0;                                  // filter: none
    const unsigned char* row = (const unsigned char*)s->pixels + (size_t)y * s->pitch;
    for (int x = 0; x < s->w; x++) {
      if (pf->BitsPerPixel == 8) { SDL_Color c = pf->palette->colors[row[x]]; *o++ = c.r; *o++ = c.g; *o++ = c.b; }
      else {
        Uint32 v = pf->BitsPerPixel == 16 ? ((const Uint16*)row)[x] : ((const Uint32*)row)[x];
        *o++ = expand(v, pf->Rmask); *o++ = expand(v, pf->Gmask); *o++ = expand(v, pf->Bmask);
      }
    }
  }
  uLongf zl = compressBound(raw); unsigned char* z = (unsigned char*)malloc(zl);
  if (!z || compress2(z, &zl, buf, raw, 6) != Z_OK) { free(buf); free(z); return SDL_SetError("deflate failed"); }
  free(buf);
  FILE* f = fopen(file.c_str(), "wb"); if (!f) { free(z); return SDL_SetError("cannot open %s", file.c_str()); }
  static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  unsigned char ihdr[13]; put32(ihdr, s->w); put32(ihdr + 4, s->h); ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
  bool ok = fwrite(sig, 1, 8, f) == 8 && chunk(f, "IHDR", ihdr, 13) && chunk(f, "IDAT", z, (uint32_t)zl) && chunk(f, "IEND", nullptr, 0);
  fclose(f); free(z);
  return ok ? 0 : SDL_SetError("write failed");
}
