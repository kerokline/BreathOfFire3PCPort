// bof3::os on Windows: the calling thread's stack (os/stack.h), another thread
// held and read (os/sampler.h), the local time of day (os/local_time.h).
#include "os/local_time.h"
#include "os/sampler.h"
#include "os/stack.h"

#include <new>

#include <windows.h>

namespace bof3::os {

StackRange CurrentStack() noexcept {
    // The TEB's first member is the TIB (fs:0x18 on i686, gs:0x30 on x86-64).
    const NT_TIB* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    StackRange r;
    r.limit = tib->StackLimit;
    r.base = tib->StackBase;
    return r;
}

struct SampledThread {
    HANDLE thread;
};

bool CanSampleThreads() noexcept { return true; }

SampledThread* SampleThisThread() noexcept {
    HANDLE h = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &h,
                         THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0))
        return nullptr;
    auto* t = new (std::nothrow) SampledThread{h};
    if (!t) {
        CloseHandle(h);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    }
    return t;
}

bool SampleThread(SampledThread* t, void (*read)(void* ctx, std::uintptr_t ip, std::uintptr_t sp), void* ctx) noexcept {
    if (!t) {
        SetLastError(ERROR_INVALID_HANDLE);
        return false;
    }
    if (SuspendThread(t->thread) == static_cast<DWORD>(-1)) return false;
    CONTEXT c = {};
    c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    const bool ok = GetThreadContext(t->thread, &c) != 0;
    const DWORD error = ok ? 0 : GetLastError();
    if (ok) {
#if defined(__x86_64__)
        read(ctx, c.Rip, c.Rsp);
#else
        read(ctx, c.Eip, c.Esp);
#endif
    }
    ResumeThread(t->thread);
    if (!ok) SetLastError(error);
    return ok;
}

LocalTime LocalNow() noexcept {
    SYSTEMTIME t;
    GetLocalTime(&t);
    LocalTime r;
    r.hour = t.wHour;
    r.minute = t.wMinute;
    r.second = t.wSecond;
    r.millisecond = t.wMilliseconds;
    return r;
}

}  // namespace bof3::os
