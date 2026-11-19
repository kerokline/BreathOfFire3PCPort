// bof3::os named events on Windows: an auto-reset event "Local\<name>". See
// os/named_event.h.
#include "os/named_event.h"

#include <new>

#include <windows.h>

namespace bof3::os {

struct NamedEvent {
    HANDLE event;
};

NamedEvent* CreateNamedEvent(const char* name) noexcept {
    wchar_t wide[128];
    int n = 0;
    for (const char* p = "Local\\"; *p; ++p) wide[n++] = static_cast<wchar_t>(*p);
    for (const char* p = name; *p; ++p) {
        if (n + 1 >= static_cast<int>(sizeof wide / sizeof wide[0])) {
            SetLastError(ERROR_INVALID_NAME);
            return nullptr;
        }
        wide[n++] = static_cast<wchar_t>(static_cast<unsigned char>(*p));
    }
    wide[n] = 0;
    HANDLE h = CreateEventW(nullptr, FALSE, FALSE, wide);
    if (!h) return nullptr;
    auto* e = new (std::nothrow) NamedEvent{h};
    if (!e) {
        CloseHandle(h);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    }
    return e;
}

void ResetNamedEvent(NamedEvent* e) noexcept { ResetEvent(e->event); }

bool WaitNamedEvent(NamedEvent* e, std::uint32_t ms) noexcept { return WaitForSingleObject(e->event, ms) == WAIT_OBJECT_0; }

const char* NamedEventWaitWay() noexcept { return "WaitForSingleObject"; }

}  // namespace bof3::os
