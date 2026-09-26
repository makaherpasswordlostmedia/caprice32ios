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
#ifndef CAPRICE_NO_WGUI
#include "CapriceGui.h"
#include "CapriceDevToolsView.h"
#else
class CapriceGui;
class CapriceDevToolsView;
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
    std::unique_ptr<CapriceGui> capriceGui;
    std::unique_ptr<CapriceDevToolsView> devToolsView;
    bool active = false;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_Surface* surface = nullptr;
};

#endif
