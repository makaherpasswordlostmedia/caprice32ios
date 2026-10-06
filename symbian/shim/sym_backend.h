// Platform backend interface used by the SDL shim.
// Implemented by backend_ws.cc (Symbian Window Server) and backend_null.cc (host tests).
#pragma once
#include "SDL.h"
#ifdef __cplusplus
extern "C" {
#endif
// Opens the output window; returns 0 on success. Reports the real window size.
int  symbian_backend_open(int* w, int* h);
void symbian_backend_close(void);
// Presents an RGB565 frame (w*h, tightly packed rows of w pixels).
void symbian_backend_present(const Uint16* rgb565, int w, int h);
// Fills ev with the next pending input event (key/pointer/quit/focus). Returns 1 if one was produced.
int  symbian_backend_poll(SDL_Event* ev);
Uint32 symbian_backend_ticks(void);
void symbian_backend_delay(Uint32 ms);
void symbian_backend_set_title(const char* title);
#ifdef __cplusplus
}
#endif
