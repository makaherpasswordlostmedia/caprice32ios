// Minimal SDL2-API subset for the Caprice32 Symbian port.
// Software surfaces only; video/input come from a backend (see sym_backend.h).
// Compiled with -fno-exceptions -fno-rtti; no libc++ dependencies here.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t Uint8;  typedef int8_t Sint8;
typedef uint16_t Uint16; typedef int16_t Sint16;
typedef uint32_t Uint32; typedef int32_t Sint32;
typedef uint64_t Uint64; typedef int64_t Sint64;
typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER  SDL_LIL_ENDIAN
#define SDL_VERSION_ATLEAST(X, Y, Z) (1)
#define SDL_DISABLE 0
#define SDL_ENABLE  1
#define SDL_PRESSED 1
#define SDL_RELEASED 0
#define SDL_HINT_RENDER_DRIVER "SDL_RENDER_DRIVER"
#define SDL_INIT_TIMER 0x1u
#define SDL_INIT_AUDIO 0x10u
#define SDL_INIT_VIDEO 0x20u
#define SDL_INIT_JOYSTICK 0x200u
#define SDL_INIT_NOPARACHUTE 0x100000u
#define SDL_WINDOW_FULLSCREEN 0x1u
#define SDL_WINDOW_OPENGL 0x2u
#define SDL_WINDOW_SHOWN 0x4u
#define SDL_WINDOW_FULLSCREEN_DESKTOP (SDL_WINDOW_FULLSCREEN | 0x1000u)
#define SDL_WINDOWEVENT_MINIMIZED 6
#define SDL_WINDOWEVENT_ENTER 10
#define SDL_WINDOWEVENT_LEAVE 11
#define SDL_WINDOWEVENT_FOCUS_GAINED 12
#define SDL_WINDOWEVENT_FOCUS_LOST 13
#define SDL_WINDOWEVENT_TAKE_FOCUS 15
#define SDL_BITSPERPIXEL(f) ((f) & 0xFF)
#define SDL_PNGFormatAlpha 0

#define KMOD_NONE 0x0000
#define KMOD_LSHIFT 0x0001
#define KMOD_RSHIFT 0x0002
#define KMOD_LCTRL 0x0040
#define KMOD_RCTRL 0x0080
#define KMOD_LALT 0x0100
#define KMOD_RALT 0x0200
#define KMOD_NUM 0x1000
#define KMOD_CAPS 0x2000
#define KMOD_MODE 0x4000
#define KMOD_CTRL (KMOD_LCTRL | KMOD_RCTRL)
#define KMOD_SHIFT (KMOD_LSHIFT | KMOD_RSHIFT)
typedef int SDL_Keymod;

#include "SDL_keys_gen.h"

#define AUDIO_S8 0x8008
#define AUDIO_S16LSB 0x8010
#define SDL_MUSTLOCK(s) (0)

typedef struct SDL_Rect { int x, y, w, h; } SDL_Rect;
typedef struct SDL_Color { Uint8 r, g, b, a; } SDL_Color;
typedef struct SDL_Palette { int ncolors; SDL_Color* colors; } SDL_Palette;
typedef struct SDL_PixelFormat {
  Uint32 format; SDL_Palette* palette; Uint8 BitsPerPixel, BytesPerPixel;
  Uint32 Rmask, Gmask, Bmask, Amask;
} SDL_PixelFormat;
typedef struct SDL_Surface {
  Uint32 flags; SDL_PixelFormat* format; int w, h, pitch; void* pixels; int owns_pixels;
} SDL_Surface;

typedef struct SDL_Keysym { SDL_Scancode scancode; SDL_Keycode sym; Uint16 mod; Uint32 unused; } SDL_Keysym;
enum { SDL_QUIT = 0x100, SDL_WINDOWEVENT = 0x200, SDL_KEYDOWN = 0x300, SDL_KEYUP = 0x301,
       SDL_MOUSEMOTION = 0x400, SDL_MOUSEBUTTONDOWN = 0x401, SDL_MOUSEBUTTONUP = 0x402,
       SDL_JOYAXISMOTION = 0x600, SDL_JOYBUTTONDOWN = 0x603, SDL_JOYBUTTONUP = 0x604 };
typedef struct { Uint32 type, timestamp, windowID; Uint8 event; int data1, data2; } SDL_WindowEvent;
typedef struct { Uint32 type, timestamp, windowID; Uint8 state, repeat; SDL_Keysym keysym; } SDL_KeyboardEvent;
typedef struct { Uint32 type, timestamp, windowID, which, state; Sint32 x, y, xrel, yrel; } SDL_MouseMotionEvent;
typedef struct { Uint32 type, timestamp, windowID, which; Uint8 button, state, clicks; Sint32 x, y; } SDL_MouseButtonEvent;
typedef struct { Uint32 type, timestamp; Sint32 which; Uint8 axis; Sint16 value; } SDL_JoyAxisEvent;
typedef struct { Uint32 type, timestamp; Sint32 which; Uint8 button, state; } SDL_JoyButtonEvent;
typedef union SDL_Event {
  Uint32 type; SDL_WindowEvent window; SDL_KeyboardEvent key; SDL_MouseMotionEvent motion;
  SDL_MouseButtonEvent button; SDL_JoyAxisEvent jaxis; SDL_JoyButtonEvent jbutton;
} SDL_Event;

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_Joystick SDL_Joystick;

typedef struct SDL_DisplayMode { Uint32 format; int w, h, refresh_rate; void* driverdata; } SDL_DisplayMode;
typedef struct SDL_RendererInfo { const char* name; Uint32 flags, num_texture_formats; Uint32 texture_formats[16]; int max_texture_width, max_texture_height; } SDL_RendererInfo;
typedef Uint32 SDL_AudioDeviceID;
typedef void (*SDL_AudioCallback)(void* userdata, Uint8* stream, int len);
typedef struct SDL_AudioSpec { int freq; Uint16 format; Uint8 channels, silence; Uint16 samples, padding; Uint32 size; SDL_AudioCallback callback; void* userdata; } SDL_AudioSpec;
typedef struct SDL_RWops SDL_RWops;
typedef void* SDL_GLContext;
#define SDL_GL_DEPTH_SIZE 6
#define SDL_GL_DOUBLEBUFFER 5

int SDL_Init(Uint32 flags);
int SDL_InitSubSystem(Uint32 flags);
void SDL_QuitSubSystem(Uint32 flags);
void SDL_Quit(void);
const char* SDL_GetError(void);
int SDL_SetError(const char* fmt, ...);
SDL_bool SDL_SetHint(const char* name, const char* value);
Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 ms);

SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
void SDL_FreeSurface(SDL_Surface* s);
int SDL_LockSurface(SDL_Surface* s);
void SDL_UnlockSurface(SDL_Surface* s);
Uint32 SDL_MapRGB(const SDL_PixelFormat* fmt, Uint8 r, Uint8 g, Uint8 b);
int SDL_FillRect(SDL_Surface* dst, const SDL_Rect* rect, Uint32 color);
int SDL_SetPaletteColors(SDL_Palette* pal, const SDL_Color* colors, int first, int n);
int SDL_UpperBlit(SDL_Surface* src, const SDL_Rect* sr, SDL_Surface* dst, SDL_Rect* dr);
#define SDL_BlitSurface SDL_UpperBlit
#define SDL_LowerBlit SDL_UpperBlit

int SDL_CreateWindowAndRenderer(int w, int h, Uint32 flags, SDL_Window** win, SDL_Renderer** ren);
SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, Uint32 flags);
SDL_Renderer* SDL_CreateRenderer(SDL_Window* win, int index, Uint32 flags);
void SDL_DestroyRenderer(SDL_Renderer* r);
void SDL_DestroyWindow(SDL_Window* w);
void SDL_SetWindowTitle(SDL_Window* w, const char* title);
void SDL_GetWindowSize(SDL_Window* w, int* pw, int* ph);
Uint32 SDL_GetWindowFlags(SDL_Window* w);
int SDL_GetRendererInfo(SDL_Renderer* r, SDL_RendererInfo* info);
int SDL_GetCurrentDisplayMode(int idx, SDL_DisplayMode* m);
SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer* r, SDL_Surface* s);
void SDL_DestroyTexture(SDL_Texture* t);
int SDL_UpdateTexture(SDL_Texture* t, const SDL_Rect* rect, const void* pixels, int pitch);
int SDL_RenderClear(SDL_Renderer* r);
int SDL_RenderCopy(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* src, const SDL_Rect* dst);
void SDL_RenderPresent(SDL_Renderer* r);

int SDL_PollEvent(SDL_Event* e);
int SDL_PushEvent(SDL_Event* e);
int SDL_ShowCursor(int toggle);
int SDL_SetRelativeMouseMode(SDL_bool enabled);
void SDL_StartTextInput(void);
void SDL_StopTextInput(void);
const char* SDL_GetKeyName(SDL_Keycode key);
char* SDL_GetClipboardText(void);

int SDL_GetNumAudioDevices(int iscapture);
const char* SDL_GetAudioDeviceName(int index, int iscapture);
SDL_AudioDeviceID SDL_OpenAudioDevice(const char* dev, int iscapture, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained, int allowed);
void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on);
void SDL_CloseAudioDevice(SDL_AudioDeviceID dev);
void SDL_PauseAudio(int pause_on);

int SDL_JoystickEventState(int state);
int SDL_NumJoysticks(void);
SDL_Joystick* SDL_JoystickOpen(int idx);
void SDL_JoystickClose(SDL_Joystick* j);

#ifdef __cplusplus
}
#endif
