#pragma once
#include "SDL.h"
#ifdef __cplusplus
extern "C" {
#endif
void vkbd_init(int screen_w, int screen_h);        // computes layout; portrait => keyboard strip below the emulator
int  vkbd_emu_height(void);                        // height given to the emulator image
void vkbd_toggle(void);
void vkbd_draw(Uint16* rgb565, int w, int h);      // overlay onto a full-window RGB565 frame
// Feed a pointer event. Returns number of SDL key events written to out[] (max 3), or -1 if the
// pointer event was not consumed by the keyboard (pass it on to the emulator).
int  vkbd_pointer(int type, int x, int y, SDL_Event* out);
void sym_make_key_event(SDL_Event* ev, int down, int scancode);
#ifdef __cplusplus
}
#endif
