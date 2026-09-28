// CPCVirtualKeyboardBridge.h
// Plain-C entry points so pure C++ (cap32.cpp) can drive the ObjC++
// UIKit overlay without importing any Objective-C headers.
#ifndef CPC_VIRTUAL_KEYBOARD_BRIDGE_H
#define CPC_VIRTUAL_KEYBOARD_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Create the overlay window (idempotent, callable from any thread).
void CPCVKbd_Install(void);
// Show/hide the keyboard panel (thread-safe).
void CPCVKbd_Toggle(void);

#ifdef __cplusplus
}
#endif

#endif
