// bof3::os on Windows: a function run on a stack of its own (os/own_stack.h),
// by the fibers the renderer and the snapshot made for themselves before the
// seam (render_d3d11.cpp RunOnFiber, commit 340d3739; snapshot.cpp
// OnOwnStack, commit e04db5b4): the same calls, sizes and flags, and the
// snapshot's x87 control word carried as it carried it.
#include "os/own_stack.h"

#include <new>

#include <windows.h>

namespace bof3::os {

struct OwnStack::State {
    const OwnStackSpec* spec = nullptr;
    void* fiber = nullptr;    // the own stack
    void* caller = nullptr;   // the fiber this run returns to
    void (*fn)(void*) = nullptr;
    void* arg = nullptr;
    unsigned short cw = 0;    // the caller's x87 control word (callers_x87)
    bool busy = false;
};

namespace {

void CALLBACK Entry(void* p) {
    auto* s = static_cast<OwnStack::State*>(p);
    for (;;) {
        if (s->spec->callers_x87) __asm__ volatile("fldcw %0" : : "m"(s->cw));
        s->fn(s->arg);
        s->fn = nullptr;
        SwitchToFiber(s->caller);
    }
}

}  // namespace

const char* OwnStackFailure(OwnStackResult result) noexcept {
    switch (result) {
    case OwnStackResult::Done: return "no failure";
    case OwnStackResult::NoStack: return "CreateFiberEx failed";
    case OwnStackResult::NoFiber: return "ConvertThreadToFiberEx failed";
    case OwnStackResult::NotGivenBack: return "ConvertFiberToThread failed";
    case OwnStackResult::Nested: return "a run of the own stack from itself";
    }
    return "an unknown result";
}

OwnStackResult OwnStack::Run(void (*fn)(void*), void* arg) noexcept {
    if (!state_) {
        state_ = new (std::nothrow) State;
        if (!state_) {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return OwnStackResult::NoStack;
        }
        state_->spec = &spec_;
    }
    State* const s = state_;
    if (s->busy) return OwnStackResult::Nested;
    // The caller's fiber: the running one, or the thread made one for this run.
    bool converted = false;
    void* caller = nullptr;
    if (IsThreadAFiber()) {
        caller = GetCurrentFiber();
    } else {
        caller = ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
        if (!caller) return OwnStackResult::NoFiber;
        converted = true;
    }
    if (!s->fiber) {
        s->fiber = CreateFiberEx(spec_.commit, spec_.reserve, FIBER_FLAG_FLOAT_SWITCH, &Entry, s);
        if (!s->fiber) return OwnStackResult::NoStack;
    }
    if (spec_.callers_x87) __asm__ volatile("fnstcw %0" : "=m"(s->cw));
    s->caller = caller;
    s->fn = fn;
    s->arg = arg;
    s->busy = true;
    SwitchToFiber(s->fiber);
    s->busy = false;
    // Back on the caller's own fiber: a thread made a fiber here is given back
    // as it came, when the spec asks it.
    if (converted && spec_.give_thread_back && !ConvertFiberToThread()) return OwnStackResult::NotGivenBack;
    return OwnStackResult::Done;
}

}  // namespace bof3::os
