// On-screen CPC keyboard: drawn into the RGB565 frame, touch -> SDL key events. No SDK dependencies.
#include "vkbd.h"

struct Key { const char* label; int sc; int w; };
#define L(c) SDL_SCANCODE_##c
static const Key kRow1[] = {{"ESC",L(ESCAPE),1},{"1",L(1),1},{"2",L(2),1},{"3",L(3),1},{"4",L(4),1},{"5",L(5),1},{"6",L(6),1},{"7",L(7),1},{"8",L(8),1},{"9",L(9),1},{"0",L(0),1}};
static const Key kRow2[] = {{"Q",L(Q),1},{"W",L(W),1},{"E",L(E),1},{"R",L(R),1},{"T",L(T),1},{"Y",L(Y),1},{"U",L(U),1},{"I",L(I),1},{"O",L(O),1},{"P",L(P),1},{"DEL",L(BACKSPACE),1}};
static const Key kRow3[] = {{"A",L(A),1},{"S",L(S),1},{"D",L(D),1},{"F",L(F),1},{"G",L(G),1},{"H",L(H),1},{"J",L(J),1},{"K",L(K),1},{"L",L(L),1},{"RET",L(RETURN),2}};
static const Key kRow4[] = {{"SHF",L(LSHIFT),2},{"Z",L(Z),1},{"X",L(X),1},{"C",L(C),1},{"V",L(V),1},{"B",L(B),1},{"N",L(N),1},{"M",L(M),1},{",",L(COMMA),1},{".",L(PERIOD),1}};
static const Key kRow5[] = {{"CTL",L(LCTRL),1},{"TAB",L(TAB),1},{"SPC",L(SPACE),4},{"<",L(LEFT),1},{"v",L(DOWN),1},{"^",L(UP),1},{">",L(RIGHT),1},{"F1",L(F1),1}};
struct Row { const Key* k; int n; };
#define R(a) {a, (int)(sizeof(a)/sizeof(a[0]))}
static const Row kRows[5] = {R(kRow1), R(kRow2), R(kRow3), R(kRow4), R(kRow5)};

// 3x5 font, 5 rows of 3 bits.
static const char kGlyphChars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ,.<>^v-";
static const unsigned char kGlyph[][5] = {
 {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7},
 {2,5,7,5,5},{6,5,6,5,6},{3,4,4,4,3},{6,5,5,5,6},{7,4,6,4,7},{7,4,6,4,4},{3,4,5,5,3},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,2},
 {5,5,6,5,5},{4,4,4,4,7},{5,7,7,5,5},{6,5,5,5,5},{2,5,5,5,2},{6,5,6,4,4},{2,5,5,7,3},{6,5,6,5,5},{3,4,2,1,6},{7,2,2,2,2},
 {5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},{7,1,2,4,7},
 {0,0,0,2,4},{0,0,0,0,2},{1,2,4,2,1},{4,2,1,2,4},{2,5,0,0,0},{0,0,0,5,2},{0,0,7,0,0}};

static int g_w, g_h, g_emu_h, g_kb_top, g_visible = 1, g_portrait;
static int g_pressed_row = -1, g_pressed_key = -1;
static bool g_shift_latched = false, g_ctrl_latched = false;

void sym_make_key_event(SDL_Event* ev, int down, int sc) {
  memset(ev, 0, sizeof *ev);
  ev->type = down ? SDL_KEYDOWN : SDL_KEYUP; ev->key.state = down ? SDL_PRESSED : SDL_RELEASED;
  ev->key.keysym.scancode = (SDL_Scancode)sc;
  SDL_Keycode sym;
  if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) sym = 'a' + (sc - SDL_SCANCODE_A);
  else if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) sym = '1' + (sc - SDL_SCANCODE_1);
  else if (sc == SDL_SCANCODE_0) sym = '0';
  else if (sc == SDL_SCANCODE_RETURN) sym = '\r';
  else if (sc == SDL_SCANCODE_SPACE) sym = ' ';
  else if (sc == SDL_SCANCODE_ESCAPE) sym = 27;
  else if (sc == SDL_SCANCODE_BACKSPACE) sym = 8;
  else if (sc == SDL_SCANCODE_TAB) sym = 9;
  else if (sc == SDL_SCANCODE_COMMA) sym = ',';
  else if (sc == SDL_SCANCODE_PERIOD) sym = '.';
  else sym = sc | SDLK_SCANCODE_MASK;
  ev->key.keysym.sym = sym;
}

void vkbd_init(int w, int h) {
  g_w = w; g_h = h; g_portrait = h * 10 >= w * 13;
  g_emu_h = g_portrait ? h * 52 / 100 : h;
  g_kb_top = g_portrait ? g_emu_h : h * 50 / 100;
  g_visible = g_portrait;           // portrait: shown by default; landscape: overlay on demand
}
int vkbd_emu_height(void) { return g_emu_h; }
void vkbd_toggle(void) { g_visible = !g_visible; }

static bool key_rect(int r, int idx, int* x, int* y, int* w, int* h) {
  const Row& row = kRows[r]; int total = 0, before = 0;
  for (int i = 0; i < row.n; i++) { if (i == idx) before = total; total += row.k[i].w; }
  int kh = (g_h - g_kb_top) / 5;
  *x = g_w * before / total; *w = g_w * (before + row.k[idx].w) / total - *x;
  *y = g_kb_top + r * kh; *h = kh; return true;
}
static void fill(Uint16* f, int x0, int y0, int w, int h, Uint16 c) {
  for (int y = y0; y < y0 + h && y < g_h; y++) { if (y < 0) continue;
    for (int x = x0; x < x0 + w && x < g_w; x++) if (x >= 0) f[(size_t)y * g_w + x] = c; }
}
static void text(Uint16* f, const char* s, int cx, int cy, int sc, Uint16 c) {
  int n = 0; while (s[n]) n++;
  int tw = n * 4 * sc - sc, x = cx - tw / 2, y = cy - 5 * sc / 2;
  for (int i = 0; i < n; i++, x += 4 * sc) {
    int g = -1; for (int k = 0; kGlyphChars[k]; k++) if (kGlyphChars[k] == s[i]) { g = k; break; }
    if (g < 0) continue;
    for (int r = 0; r < 5; r++) for (int b = 0; b < 3; b++)
      if (kGlyph[g][r] & (4 >> b)) fill(f, x + b * sc, y + r * sc, sc, sc, c);
  }
}
void vkbd_draw(Uint16* f, int w, int h) {
  if (!g_visible || w != g_w || h != g_h) return;
  fill(f, 0, g_kb_top, g_w, g_h - g_kb_top, 0x2104);
  for (int r = 0; r < 5; r++) for (int i = 0; i < kRows[r].n; i++) {
    int x, y, kw, kh; key_rect(r, i, &x, &y, &kw, &kh);
    const Key& k = kRows[r].k[i];
    bool on = (r == g_pressed_row && i == g_pressed_key) || (k.sc == SDL_SCANCODE_LSHIFT && g_shift_latched) || (k.sc == SDL_SCANCODE_LCTRL && g_ctrl_latched);
    fill(f, x + 1, y + 1, kw - 2, kh - 2, on ? 0xFD20 : 0x4A69);
    int n = 0; while (k.label[n]) n++;
    int sc = kh / 12, fit = (kw - 4) / (n * 4); if (sc > fit) sc = fit; if (sc < 1) sc = 1;
    text(f, k.label, x + kw / 2, y + kh / 2, sc, 0xFFFF);
  }
}
static bool in_toggle_corner(int x, int y) { int s = g_w / 8; return x >= g_w - s && y < s; }
static bool hit(int x, int y, int* rr, int* ii) {
  if (y < g_kb_top) return false;
  int kh = (g_h - g_kb_top) / 5, r = (y - g_kb_top) / kh; if (r > 4) r = 4;
  for (int i = 0; i < kRows[r].n; i++) { int kx, ky, kw, kq; key_rect(r, i, &kx, &ky, &kw, &kq); if (x >= kx && x < kx + kw) { *rr = r; *ii = i; return true; } }
  return false;
}
int vkbd_pointer(int type, int x, int y, SDL_Event* out) {
  int n = 0;
  if (type == SDL_MOUSEBUTTONDOWN && in_toggle_corner(x, y)) { vkbd_toggle(); return 0; }
  if (!g_visible) return -1;
  if (type == SDL_MOUSEBUTTONDOWN) {
    int r, i; if (!hit(x, y, &r, &i)) return -1;
    g_pressed_row = r; g_pressed_key = i; int sc = kRows[r].k[i].sc;
    if (sc == SDL_SCANCODE_LSHIFT || sc == SDL_SCANCODE_LCTRL) {          // sticky modifiers
      bool& l = (sc == SDL_SCANCODE_LSHIFT) ? g_shift_latched : g_ctrl_latched;
      l = !l; sym_make_key_event(&out[n++], l, sc); g_pressed_row = -1; return n;
    }
    sym_make_key_event(&out[n++], 1, sc); return n;
  }
  if (type == SDL_MOUSEBUTTONUP) {
    if (g_pressed_row < 0) return (y >= g_kb_top) ? 0 : -1;
    sym_make_key_event(&out[n++], 0, kRows[g_pressed_row].k[g_pressed_key].sc);
    g_pressed_row = -1;
    if (g_shift_latched) { g_shift_latched = false; sym_make_key_event(&out[n++], 0, SDL_SCANCODE_LSHIFT); }
    if (g_ctrl_latched)  { g_ctrl_latched = false;  sym_make_key_event(&out[n++], 0, SDL_SCANCODE_LCTRL); }
    return n;
  }
  if (type == SDL_MOUSEMOTION) return (y >= g_kb_top && g_visible) ? 0 : -1;
  return -1;
}
