// Host-side test backend: no window, no input. Used to compile-check and smoke-test off-device.
#include "sym_backend.h"
#include <time.h>
#include <unistd.h>
static Uint32 g_frames = 0;
extern "C" {
int symbian_backend_open(int* w, int* h) { *w = 360; *h = 640; return 0; }
void symbian_backend_close(void) {}
void symbian_backend_present(const Uint16*, int, int) { g_frames++; }
int symbian_backend_poll(SDL_Event* ev) { if (g_frames >= 50) { ev->type = SDL_QUIT; g_frames = 0; return 1; } return 0; }
Uint32 symbian_backend_ticks(void) { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (Uint32)(t.tv_sec * 1000 + t.tv_nsec / 1000000); }
void symbian_backend_delay(Uint32 ms) { usleep(ms * 1000); }
void symbian_backend_set_title(const char*) {}
}
