// SDL2 subset implementation for Symbian. No exceptions/RTTI, no iostream.
#include "SDL.h"
#include "sym_backend.h"
#include "vkbd.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

struct SDL_Window { int w, h; char title[64]; Uint32 flags; };
struct SDL_Renderer { SDL_Window* win; };
struct SDL_Texture { int w, h; Uint16* px; };

static char g_err[128] = "";
static SDL_Window g_win;
static SDL_Renderer g_ren;
static bool g_open = false;
static int g_full_h = 0;
static SDL_Event g_q[32];
static int g_qh = 0, g_qt = 0;

extern "C" {
const char* SDL_GetError(void) { return g_err; }
int SDL_SetError(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt); vsnprintf(g_err, sizeof g_err, fmt, ap); va_end(ap); return -1;
}
int SDL_Init(Uint32) { return 0; }
int SDL_InitSubSystem(Uint32) { return 0; }
void SDL_QuitSubSystem(Uint32) {}
void SDL_Quit(void) { if (g_open) { symbian_backend_close(); g_open = false; } }
SDL_bool SDL_SetHint(const char*, const char*) { return SDL_TRUE; }
Uint32 SDL_GetTicks(void) { return symbian_backend_ticks(); }
void SDL_Delay(Uint32 ms) { symbian_backend_delay(ms); }

SDL_Surface* SDL_CreateRGBSurface(Uint32, int w, int h, int depth, Uint32 R, Uint32 G, Uint32 B, Uint32 A) {
  if (w <= 0 || h <= 0 || (depth != 8 && depth != 16 && depth != 32)) { SDL_SetError("bad surface"); return nullptr; }
  SDL_Surface* s = (SDL_Surface*)calloc(1, sizeof *s);
  SDL_PixelFormat* f = (SDL_PixelFormat*)calloc(1, sizeof *f);
  if (!s || !f) { free(s); free(f); SDL_SetError("oom"); return nullptr; }
  if (!R && !G && !B) {
    if (depth == 16) { R = 0xF800; G = 0x07E0; B = 0x001F; }
    else if (depth == 32) { R = 0x00FF0000; G = 0x0000FF00; B = 0x000000FF; }
  }
  f->BitsPerPixel = (Uint8)depth; f->BytesPerPixel = (Uint8)(depth / 8);
  f->Rmask = R; f->Gmask = G; f->Bmask = B; f->Amask = A;
  if (depth == 8) {
    f->palette = (SDL_Palette*)calloc(1, sizeof(SDL_Palette));
    f->palette->ncolors = 256; f->palette->colors = (SDL_Color*)calloc(256, sizeof(SDL_Color));
  }
  s->format = f; s->w = w; s->h = h; s->pitch = (w * f->BytesPerPixel + 3) & ~3;
  s->pixels = calloc((size_t)s->pitch * h, 1); s->owns_pixels = 1;
  if (!s->pixels) { SDL_SetError("oom"); free(f); free(s); return nullptr; }
  return s;
}
void SDL_FreeSurface(SDL_Surface* s) {
  if (!s) return;
  if (s->owns_pixels) free(s->pixels);
  if (s->format) { if (s->format->palette) { free(s->format->palette->colors); free(s->format->palette); } free(s->format); }
  free(s);
}
int SDL_LockSurface(SDL_Surface*) { return 0; }
void SDL_UnlockSurface(SDL_Surface*) {}

static Uint32 shift_of(Uint32 m) { int n = 0; if (!m) return 0; while (!(m & 1)) { m >>= 1; n++; } return n; }
static Uint32 scale_of(Uint32 m) { if (!m) return 0; while (!(m & 1)) m >>= 1; return m; }
Uint32 SDL_MapRGB(const SDL_PixelFormat* f, Uint8 r, Uint8 g, Uint8 b) {
  if (f->BitsPerPixel == 8) return 0;
  Uint32 rm = scale_of(f->Rmask), gm = scale_of(f->Gmask), bm = scale_of(f->Bmask);
  return (((Uint32)r * rm / 255) << shift_of(f->Rmask)) | (((Uint32)g * gm / 255) << shift_of(f->Gmask)) |
         (((Uint32)b * bm / 255) << shift_of(f->Bmask)) | f->Amask;
}
static void clip(SDL_Surface* s, const SDL_Rect* r, SDL_Rect* o) {
  SDL_Rect q = r ? *r : SDL_Rect{0, 0, s->w, s->h};
  if (q.x < 0) { q.w += q.x; q.x = 0; } if (q.y < 0) { q.h += q.y; q.y = 0; }
  if (q.x + q.w > s->w) q.w = s->w - q.x; if (q.y + q.h > s->h) q.h = s->h - q.y;
  if (q.w < 0) q.w = 0; if (q.h < 0) q.h = 0; *o = q;
}
int SDL_FillRect(SDL_Surface* d, const SDL_Rect* rect, Uint32 c) {
  SDL_Rect q; clip(d, rect, &q); int bpp = d->format->BytesPerPixel;
  for (int y = 0; y < q.h; y++) {
    Uint8* p = (Uint8*)d->pixels + (size_t)(q.y + y) * d->pitch + q.x * bpp;
    if (bpp == 1) memset(p, (int)c, q.w);
    else if (bpp == 2) for (int x = 0; x < q.w; x++) ((Uint16*)p)[x] = (Uint16)c;
    else for (int x = 0; x < q.w; x++) ((Uint32*)p)[x] = c;
  }
  return 0;
}
int SDL_SetPaletteColors(SDL_Palette* p, const SDL_Color* c, int first, int n) {
  if (!p) return -1; if (first < 0 || first + n > p->ncolors) return -1;
  memcpy(p->colors + first, c, sizeof(SDL_Color) * n); return 0;
}
int SDL_UpperBlit(SDL_Surface* s, const SDL_Rect* sr, SDL_Surface* d, SDL_Rect* dr) {
  if (s->format->BitsPerPixel != d->format->BitsPerPixel) return SDL_SetError("blit: format mismatch");
  SDL_Rect a; clip(s, sr, &a); int dx = dr ? dr->x : 0, dy = dr ? dr->y : 0;
  if (dx < 0) { a.x -= dx; a.w += dx; dx = 0; } if (dy < 0) { a.y -= dy; a.h += dy; dy = 0; }
  if (dx + a.w > d->w) a.w = d->w - dx; if (dy + a.h > d->h) a.h = d->h - dy;
  if (a.w <= 0 || a.h <= 0) return 0; int bpp = s->format->BytesPerPixel;
  for (int y = 0; y < a.h; y++)
    memcpy((Uint8*)d->pixels + (size_t)(dy + y) * d->pitch + dx * bpp,
           (Uint8*)s->pixels + (size_t)(a.y + y) * s->pitch + a.x * bpp, (size_t)a.w * bpp);
  return 0;
}

int SDL_CreateWindowAndRenderer(int w, int h, Uint32 flags, SDL_Window** win, SDL_Renderer** ren) {
  *win = nullptr; *ren = nullptr; int bw = w, bh = h;
  if (symbian_backend_open(&bw, &bh) != 0) return SDL_SetError("no window");
  g_open = true; g_full_h = bh; vkbd_init(bw, bh); g_win.w = bw; g_win.h = vkbd_emu_height(); g_win.flags = flags; g_ren.win = &g_win;
  *win = &g_win; *ren = &g_ren; return 0;
}
SDL_Window* SDL_CreateWindow(const char*, int, int, int w, int h, Uint32 f) { SDL_Window* a; SDL_Renderer* b; return SDL_CreateWindowAndRenderer(w, h, f, &a, &b) ? nullptr : a; }
SDL_Renderer* SDL_CreateRenderer(SDL_Window* w, int, Uint32) { g_ren.win = w; return &g_ren; }
void SDL_DestroyRenderer(SDL_Renderer*) {}
void SDL_DestroyWindow(SDL_Window*) { if (g_open) { symbian_backend_close(); g_open = false; } }
void SDL_SetWindowTitle(SDL_Window*, const char* t) { symbian_backend_set_title(t); }
void SDL_GetWindowSize(SDL_Window* w, int* pw, int* ph) { *pw = w->w; *ph = w->h; }
Uint32 SDL_GetWindowFlags(SDL_Window* w) { return w->flags; }
int SDL_GetRendererInfo(SDL_Renderer*, SDL_RendererInfo* i) {
  memset(i, 0, sizeof *i); i->name = "symbian-ws"; i->num_texture_formats = 1; i->texture_formats[0] = 16; return 0;  // low byte = bpp (RGB565)
}
int SDL_GetCurrentDisplayMode(int, SDL_DisplayMode* m) { m->w = g_win.w; m->h = g_win.h; m->refresh_rate = 60; return 0; }

SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer*, SDL_Surface* s) {
  SDL_Texture* t = (SDL_Texture*)calloc(1, sizeof *t); if (!t) return nullptr;
  t->w = s->w; t->h = s->h; t->px = (Uint16*)calloc((size_t)s->w * s->h, 2);
  if (!t->px) { free(t); return nullptr; } return t;
}
void SDL_DestroyTexture(SDL_Texture* t) { if (t) { free(t->px); free(t); } }
int SDL_UpdateTexture(SDL_Texture* t, const SDL_Rect*, const void* px, int pitch) {
  for (int y = 0; y < t->h; y++) memcpy(t->px + (size_t)y * t->w, (const Uint8*)px + (size_t)y * pitch, (size_t)t->w * 2);
  return 0;
}
// Scaled present: nearest-neighbour from texture to a window-sized RGB565 frame.
static Uint16* g_frame = nullptr; static int g_fw = 0, g_fh = 0;
int SDL_RenderClear(SDL_Renderer* r) {
  int full_h = g_full_h ? g_full_h : r->win->h;
  if (!g_frame || g_fw != r->win->w || g_fh != full_h) {
    free(g_frame); g_fw = r->win->w; g_fh = full_h; g_frame = (Uint16*)calloc((size_t)g_fw * g_fh, 2);
  }
  if (g_frame) memset(g_frame, 0, (size_t)g_fw * g_fh * 2);
  return 0;
}
int SDL_RenderCopy(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect*, const SDL_Rect* dst) {
  if (!g_frame) SDL_RenderClear(r); if (!g_frame) return -1;
  SDL_Rect d = dst ? *dst : SDL_Rect{0, 0, g_fw, g_fh};
  for (int y = 0; y < d.h; y++) {
    int oy = d.y + y; if (oy < 0 || oy >= g_fh) continue;
    const Uint16* srow = t->px + (size_t)((y * t->h) / d.h) * t->w;
    Uint16* orow = g_frame + (size_t)oy * g_fw;
    for (int x = 0; x < d.w; x++) { int ox = d.x + x; if (ox >= 0 && ox < g_fw) orow[ox] = srow[(x * t->w) / d.w]; }
  }
  return 0;
}
void SDL_RenderPresent(SDL_Renderer*) { if (!g_frame) return; vkbd_draw(g_frame, g_fw, g_fh); symbian_backend_present(g_frame, g_fw, g_fh); }

int SDL_PushEvent(SDL_Event* e) {
  int n = (g_qt + 1) & 31; if (n == g_qh) return -1; g_q[g_qt] = *e; g_qt = n; return 1;
}
int SDL_PollEvent(SDL_Event* e) {
  if (g_qh != g_qt) { *e = g_q[g_qh]; g_qh = (g_qh + 1) & 31; return 1; }
  SDL_Event ev;
  while (symbian_backend_poll(&ev)) {
    if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP || ev.type == SDL_MOUSEMOTION) {
      SDL_Event k[3]; int n = vkbd_pointer((int)ev.type, ev.motion.x, ev.motion.y, k);
      if (n >= 0) { for (int i = 0; i < n; i++) SDL_PushEvent(&k[i]); if (g_qh != g_qt) { *e = g_q[g_qh]; g_qh = (g_qh + 1) & 31; return 1; } continue; }
    }
    *e = ev; return 1;
  }
  return 0;
}
int SDL_ShowCursor(int) { return 0; }
int SDL_SetRelativeMouseMode(SDL_bool) { return 0; }
void SDL_StartTextInput(void) {}
void SDL_StopTextInput(void) {}
const char* SDL_GetKeyName(SDL_Keycode) { return ""; }
char* SDL_GetClipboardText(void) { return (char*)calloc(1, 1); }

// Audio: no media API in the Symbian SDK yet -> fail open, cap32 then disables sound itself.
int SDL_GetNumAudioDevices(int) { return 0; }
const char* SDL_GetAudioDeviceName(int, int) { return ""; }
SDL_AudioDeviceID SDL_OpenAudioDevice(const char*, int, const SDL_AudioSpec*, SDL_AudioSpec*, int) { SDL_SetError("audio unsupported on Symbian SDK"); return 0; }
void SDL_PauseAudioDevice(SDL_AudioDeviceID, int) {}
void SDL_CloseAudioDevice(SDL_AudioDeviceID) {}
void SDL_PauseAudio(int) {}

int SDL_JoystickEventState(int s) { return s; }
int SDL_NumJoysticks(void) { return 0; }
SDL_Joystick* SDL_JoystickOpen(int) { return nullptr; }
void SDL_JoystickClose(SDL_Joystick*) {}
}
