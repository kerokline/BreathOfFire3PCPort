// bof3::os threads on Windows: CreateThread, the handle's signal, TerminateProcess. See os/thread.h.

#include "os/thread.h"
#include "os/os.h"

#include <windows.h>

namespace bof3::os {

namespace {

struct Start {
    void (*fn)(void*);
    void* arg;
};

DWORD WINAPI Run(void* p) {
    const Start s = *static_cast<Start*>(p);
    delete static_cast<Start*>(p);
    s.fn(s.arg);
    return 0;
}

}  // namespace

Thread StartThread(void (*fn)(void*), void* arg) {
    Start* s = new Start{fn, arg};
    HANDLE h = CreateThread(nullptr, 0, Run, s, 0, nullptr);
    if (!h) delete s;
    return h;
}

bool ThreadEnded(Thread t) { return t && WaitForSingleObject(static_cast<HANDLE>(t), 0) == WAIT_OBJECT_0; }

void EndProcess(std::uint32_t code) {
    TerminateProcess(GetCurrentProcess(), code);
    for (;;) {
    }
}

}  // namespace bof3::os
