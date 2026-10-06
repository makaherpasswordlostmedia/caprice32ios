// Bridge called by cap32.cpp (same names as the iOS build). Real implementation: shim/vkbd.cc.
#include "vkbd.h"
extern "C" { void CPCVKbd_Install(void) {} void CPCVKbd_Toggle(void) { vkbd_toggle(); } }
