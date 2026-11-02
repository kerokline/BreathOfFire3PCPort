// A 64-bit DLL with a low fixed preferred base, for lowmem_probe.cpp (the
// 64-bit build round, group XA2): does the loader put it at 0x10000000 with
// DYNAMIC_BASE off, as it does the 32-bit bof3x.dll (CMakeLists.txt, the
// link options of bof3x)? The exported function is what a bind-table row
// jumps to in the probe.
//
//   x86_64-w64-mingw32-clang++ -O1 -shared -o lowdll_probe.dll tools/probes/lowdll_probe.cpp \
//       -Wl,--image-base,0x10000000 -Wl,--disable-dynamicbase
#include <windows.h>

extern "C" __declspec(dllexport) unsigned LowDll_Answer(unsigned x) { return 2 * x + 2; }

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
