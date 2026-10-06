// Symbian Window Server backend for the SDL shim.  UNTESTED against the real SDK headers:
// names follow the original w32std.h/e32keys.h/fbs.h; adjust to what the SDK exports.
// Polling model: one outstanding RWsSession::EventReady, re-armed after each event.
#include "sym_backend.h"
#include <w32std.h>
#include <e32keys.h>
#include <e32std.h>
#include <fbs.h>

static RWsSession g_ws;
static CWsScreenDevice* g_scr = nullptr;
static RWindowGroup g_grp;
static RWindow g_win;
static CWindowGc* g_gc = nullptr;
static CFbsBitmap* g_bmp = nullptr;
static TRequestStatus g_evst;
static bool g_armed = false;
static int g_w = 0, g_h = 0;
static bool g_down[256];

static void arm() { if (!g_armed) { g_ws.EventReady(&g_evst); g_armed = true; } }

static int map_key(TInt sc) {   // Symbian EStdKey* scancode -> SDL scancode
  if (sc >= 'A' && sc <= 'Z') return SDL_SCANCODE_A + (sc - 'A');
  if (sc >= '1' && sc <= '9') return SDL_SCANCODE_1 + (sc - '1');
  if (sc == '0') return SDL_SCANCODE_0;
  switch (sc) {
    case EStdKeyUpArrow: return SDL_SCANCODE_UP;       case EStdKeyDownArrow: return SDL_SCANCODE_DOWN;
    case EStdKeyLeftArrow: return SDL_SCANCODE_LEFT;   case EStdKeyRightArrow: return SDL_SCANCODE_RIGHT;
    case EStdKeyEnter: case EStdKeyDevice3: return SDL_SCANCODE_RETURN;   // D-pad centre = fire/enter
    case EStdKeySpace: return SDL_SCANCODE_SPACE;      case EStdKeyBackspace: return SDL_SCANCODE_BACKSPACE;
    case EStdKeyEscape: case EStdKeyDevice1: return SDL_SCANCODE_ESCAPE;  // right softkey = Esc
    case EStdKeyTab: return SDL_SCANCODE_TAB;
    case EStdKeyLeftShift: case EStdKeyRightShift: return SDL_SCANCODE_LSHIFT;
    case EStdKeyLeftCtrl: case EStdKeyRightCtrl: return SDL_SCANCODE_LCTRL;
    case EStdKeyComma: return SDL_SCANCODE_COMMA;      case EStdKeyFullStop: return SDL_SCANCODE_PERIOD;
    case EStdKeyDevice0: return SDL_SCANCODE_F1;       // left softkey = F1 (emulator menu)
  }
  return SDL_SCANCODE_UNKNOWN;
}
static void fill_key(SDL_Event* ev, bool down, int sc) {
  ev->type = down ? SDL_KEYDOWN : SDL_KEYUP; ev->key.state = down ? SDL_PRESSED : SDL_RELEASED;
  ev->key.keysym.scancode = (SDL_Scancode)sc;
  ev->key.keysym.sym = (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) ? ('a' + sc - SDL_SCANCODE_A)
                      : (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) ? ('1' + sc - SDL_SCANCODE_1)
                      : (sc == SDL_SCANCODE_0) ? '0' : (SDL_Keycode)(sc | SDLK_SCANCODE_MASK);
  if (sc == SDL_SCANCODE_RETURN) ev->key.keysym.sym = '\r';
  if (sc == SDL_SCANCODE_SPACE) ev->key.keysym.sym = ' ';
  if (sc == SDL_SCANCODE_ESCAPE) ev->key.keysym.sym = 27;
  if (sc == SDL_SCANCODE_BACKSPACE) ev->key.keysym.sym = 8;
  if (sc == SDL_SCANCODE_TAB) ev->key.keysym.sym = 9;
  ev->key.keysym.mod = 0;
}

extern "C" {
int symbian_backend_open(int* w, int* h) {
  if (g_ws.Connect() != KErrNone) return -1;
  g_scr = new CWsScreenDevice(g_ws);
  if (g_scr->Construct() != KErrNone) return -1;
  TSize sz = g_scr->SizeInPixels(); g_w = sz.iWidth; g_h = sz.iHeight;
  g_grp = RWindowGroup(g_ws);
  if (g_grp.Construct(1) != KErrNone) return -1;
  g_win = RWindow(g_ws);
  if (g_win.Construct(g_grp, 2) != KErrNone) return -1;
  g_win.SetExtent(TPoint(0, 0), sz); g_win.SetRequiredDisplayMode(EColor64K);
  g_win.Activate();
  if (g_scr->CreateContext(g_gc) != KErrNone) return -1;
  g_bmp = new CFbsBitmap();
  if (g_bmp->Create(sz, EColor64K) != KErrNone) return -1;
  *w = g_w; *h = g_h; arm(); return 0;
}
void symbian_backend_close(void) {
  if (g_armed) { g_ws.EventReadyCancel(); User::WaitForRequest(g_evst); g_armed = false; }
  delete g_gc; g_gc = nullptr; delete g_bmp; g_bmp = nullptr;
  g_win.Close(); g_grp.Close(); delete g_scr; g_scr = nullptr; g_ws.Close();
}
void symbian_backend_present(const Uint16* px, int w, int h) {
  if (!g_bmp || w != g_w || h != g_h) return;
  g_bmp->LockHeap();
  memcpy(g_bmp->DataAddress(), px, (size_t)w * h * 2);   // EColor64K = RGB565, tightly packed rows
  g_bmp->UnlockHeap();
  g_win.Invalidate(); g_win.BeginRedraw();
  g_gc->Activate(g_win); g_gc->BitBlt(TPoint(0, 0), g_bmp); g_gc->Deactivate();
  g_win.EndRedraw(); g_ws.Flush();
}
int symbian_backend_poll(SDL_Event* ev) {
  if (!g_armed || g_evst == KRequestPending) return 0;
  g_armed = false; TWsEvent e; g_ws.GetEvent(e); arm();
  switch (e.Type()) {
    case EEventKeyDown: case EEventKeyUp: {
      bool down = e.Type() == EEventKeyDown; int sc = map_key(e.Key()->iScanCode);
      if (sc == SDL_SCANCODE_UNKNOWN) return 0;
      fill_key(ev, down, sc); return 1;
    }
    case EEventPointer: {
      TPoint p = e.Pointer()->iPosition; int t = e.Pointer()->iType;
      ev->motion.x = p.iX; ev->motion.y = p.iY; ev->motion.xrel = ev->motion.yrel = 0;
      if (t == TPointerEvent::EButton1Down) { ev->type = SDL_MOUSEBUTTONDOWN; ev->button.button = 1; ev->button.state = SDL_PRESSED; return 1; }
      if (t == TPointerEvent::EButton1Up)   { ev->type = SDL_MOUSEBUTTONUP;   ev->button.button = 1; ev->button.state = SDL_RELEASED; return 1; }
      if (t == TPointerEvent::EDrag)        { ev->type = SDL_MOUSEMOTION; return 1; }
      return 0;
    }
    case EEventSwitchOn: case EEventFocusGained:
      ev->type = SDL_WINDOWEVENT; ev->window.event = SDL_WINDOWEVENT_FOCUS_GAINED; return 1;
    case EEventFocusLost:
      ev->type = SDL_WINDOWEVENT; ev->window.event = SDL_WINDOWEVENT_FOCUS_LOST; return 1;
    default: return 0;
  }
}
Uint32 symbian_backend_ticks(void) { return (Uint32)(User::NTickCount()); }
void symbian_backend_delay(Uint32 ms) { User::After((TTimeIntervalMicroSeconds32)(ms * 1000)); }
void symbian_backend_set_title(const char*) {}
}
