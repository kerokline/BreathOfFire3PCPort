// bof3::os events on Windows: the kernel's event object. See os/event.h.

#include "os/event.h"
#include "os/os.h"

#include <windows.h>

namespace bof3::os {

Event EventCreate(bool manual_reset, bool initially_set) {
    return CreateEventA(nullptr, manual_reset ? TRUE : FALSE, initially_set ? TRUE : FALSE, nullptr);
}

bool EventSet(Event e) { return SetEvent(static_cast<HANDLE>(e)) != 0; }

bool EventReset(Event e) { return ResetEvent(static_cast<HANDLE>(e)) != 0; }

WaitResult EventWait(Event e, std::uint32_t ms) {
    switch (WaitForSingleObject(static_cast<HANDLE>(e), ms)) {
        case WAIT_OBJECT_0: return WaitResult::Signalled;
        case WAIT_TIMEOUT: return WaitResult::Timeout;
        default: return WaitResult::Failed;
    }
}

void EventClose(Event e) {
    if (e) CloseHandle(static_cast<HANDLE>(e));
}

}  // namespace bof3::os
