// bof3::os threads on Linux: pthread_create, a flag set on return, _exit. See os/thread.h.

#include "os/thread.h"
#include "os/os.h"

#include <atomic>
#include <pthread.h>
#include <unistd.h>

namespace bof3::os {

namespace {

struct Start {
    void (*fn)(void*);
    void* arg;
    std::atomic<bool> ended{false};
};

void* Run(void* p) {
    Start* s = static_cast<Start*>(p);
    s->fn(s->arg);
    s->ended.store(true, std::memory_order_release);
    return nullptr;
}

}  // namespace

Thread StartThread(void (*fn)(void*), void* arg) {
    Start* s = new Start;
    s->fn = fn;
    s->arg = arg;
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        delete s;
        return nullptr;
    }
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t t;
    const int r = pthread_create(&t, &attr, Run, s);
    pthread_attr_destroy(&attr);
    if (r != 0) {
        delete s;
        return nullptr;
    }
    return s;   // kept for the process's life: ThreadEnded reads it after the thread is gone
}

bool ThreadEnded(Thread t) { return t && static_cast<Start*>(t)->ended.load(std::memory_order_acquire); }

void EndProcess(std::uint32_t code) { _exit(static_cast<int>(code)); }

}  // namespace bof3::os
