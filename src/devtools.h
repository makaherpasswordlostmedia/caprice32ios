#ifndef DEVTOOLS_H
#define DEVTOOLS_H

#include <string>
#include "SDL.h"

// On iOS the wGui-based debugger UI (src/gui/) isn't built - it's
// desktop-only tooling irrelevant to a touchscreen emulator, and
// pulling it in would require porting an entire mouse/keyboard-driven
// widget toolkit. DevTools stays a real, linkable class either way;
// under CAPRICE_NO_WGUI its guts (defined in devtools.cpp) are just
// no-ops, so every call site in cap32.cpp's main loop needs no changes.
//
// The capriceGui/devToolsView members are only declared under the
// non-iOS branch below. A forward-declared CapriceGui/CapriceDevToolsView
// is NOT enough to hold them as unique_ptr members even if the methods
// that touch the pointers are never called: unique_ptr's destructor
// needs sizeof(T) at the point DevTools's own destructor is
// instantiated (which happens wherever std::list<DevTools> is used,
// i.e. cap32.cpp - nowhere near CapriceGui.h). Omitting the members
// entirely under CAPRICE_NO_WGUI sidesteps that rather than requiring
// every TU that ever destroys a DevTools to see the full wGui headers.
#ifndef CAPRICE_NO_WGUI
#include "CapriceGui.h"
#include "CapriceDevToolsView.h"
#endif

class DevTools {
  public:
    bool Activate(int scale);
    void Deactivate();

    bool IsActive() const { return active; };

    void LoadSymbols(const std::string& filename);

    void PreUpdate();
    void PostUpdate();

    // Return true if the event was processed
    // (i.e destined to this window)
    bool PassEvent(SDL_Event& e);

  private:
#ifndef CAPRICE_NO_WGUI
    std::unique_ptr<CapriceGui> capriceGui;
    std::unique_ptr<CapriceDevToolsView> devToolsView;
    bool active = false;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_Surface* surface = nullptr;
#else
    // The no-op methods under CAPRICE_NO_WGUI (see devtools.cpp) never
    // touch an SDL window/renderer/texture/surface - there's nothing
    // to create or tear down without wGui - so keeping those members
    // around here just trips -Werror,-Wunused-private-field. `active`
    // is the only piece of state a no-op DevTools still needs, since
    // IsActive() is used unconditionally by cap32.cpp's main loop.
    bool active = false;
#endif
};

#endif
