// src/input/key_codes.h's values against the Windows SDK's names: the one
// place the engine's own key numbers meet <dinput.h> and <windows.h>. Compiled
// with the launcher (Windows by design); no code, the build is the check.
#ifdef _WIN32

#include "input/key_codes.h"

#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0700
#include <windows.h>
#include <dinput.h>

namespace bof3x::input {

static_assert(kDikEscape == DIK_ESCAPE);
static_assert(kDikQ == DIK_Q);
static_assert(kDikW == DIK_W);
static_assert(kDikReturn == DIK_RETURN);
static_assert(kDikA == DIK_A);
static_assert(kDikS == DIK_S);
static_assert(kDikZ == DIK_Z);
static_assert(kDikX == DIK_X);
static_assert(kDikC == DIK_C);
static_assert(kDikV == DIK_V);
static_assert(kDikRShift == DIK_RSHIFT);
static_assert(kDikSpace == DIK_SPACE);
static_assert(kDikNumpad7 == DIK_NUMPAD7);
static_assert(kDikNumpad8 == DIK_NUMPAD8);
static_assert(kDikNumpad9 == DIK_NUMPAD9);
static_assert(kDikNumpad4 == DIK_NUMPAD4);
static_assert(kDikNumpad6 == DIK_NUMPAD6);
static_assert(kDikNumpad1 == DIK_NUMPAD1);
static_assert(kDikNumpad2 == DIK_NUMPAD2);
static_assert(kDikNumpad3 == DIK_NUMPAD3);
static_assert(kDikF12 == DIK_F12);
static_assert(kDikUp == DIK_UP);
static_assert(kDikLeft == DIK_LEFT);
static_assert(kDikRight == DIK_RIGHT);
static_assert(kDikDown == DIK_DOWN);

static_assert(kVkReturn == VK_RETURN);
static_assert(kVkMenu == VK_MENU);
static_assert(kVkEscape == VK_ESCAPE);
static_assert(kVkSpace == VK_SPACE);
static_assert(kVkF1 == VK_F1);
static_assert(kVkF2 == VK_F2);
static_assert(kVkF3 == VK_F3);
static_assert(kVkF4 == VK_F4);
static_assert(kVkF5 == VK_F5);
static_assert(kVkF6 == VK_F6);
static_assert(kVkF7 == VK_F7);
static_assert(kVkF8 == VK_F8);
static_assert(kVkF9 == VK_F9);
static_assert(kVkF10 == VK_F10);
static_assert(kVkF11 == VK_F11);
static_assert(kVkF12 == VK_F12);

}  // namespace bof3x::input

#endif  // _WIN32
