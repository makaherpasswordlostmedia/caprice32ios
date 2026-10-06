// Entry point. The SDK's startup bridge (startup.S/startup.cc from examples/gui_app) calls GuiMain
// after heap + CTrapCleanup setup; the signature below matches that example - verify against your SDK.
#include "cap32.h"
#include <stdlib.h>
#include <string.h>
extern "C" int GuiMain(void) {
  static char a0[] = "caprice32", a1[] = "-c", a2[] = "E:/caprice32/cap32.cfg";
  char* argv[] = { a0, a1, a2, nullptr };
  setenv("HOME", "E:/caprice32", 1);
  return cap32_main(3, argv);
}
