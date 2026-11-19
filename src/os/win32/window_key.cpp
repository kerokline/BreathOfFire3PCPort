// bof3::os, a key posted to a window on Windows: WM_KEYDOWN or WM_KEYUP. See
// os/window_key.h.
#include "os/window_key.h"

#include <windows.h>

namespace bof3::os {

bool PostWindowKey(void* window, unsigned vk, bool down) noexcept {
    return down ? PostMessageA(static_cast<HWND>(window), WM_KEYDOWN, vk, 1) != 0
                : PostMessageA(static_cast<HWND>(window), WM_KEYUP, vk, static_cast<LPARAM>(0xC0000001u)) != 0;
}

}  // namespace bof3::os
