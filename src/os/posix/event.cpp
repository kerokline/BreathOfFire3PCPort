// bof3::os events on Linux: Win32's event semantics over a std::mutex and a
// std::condition_variable, from a static pool. See os/event.h.

#include "os/event.h"
#include "os/os.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace bof3::os {

namespace {

struct Object {
    std::mutex m;
    std::condition_variable cv;
    bool live = false;     // under g_pool_lock
    bool manual = false;
    bool set = false;      // under m
};

Object g_pool[kEventPool];
std::mutex g_pool_lock;

Object* Of(Event e) {
    const auto at = reinterpret_cast<std::uintptr_t>(e), lo = reinterpret_cast<std::uintptr_t>(g_pool);
    if (!e || at < lo || at >= lo + sizeof g_pool || (at - lo) % sizeof(Object) != 0) return nullptr;
    Object* o = static_cast<Object*>(e);
    std::lock_guard<std::mutex> g(g_pool_lock);
    return o->live ? o : nullptr;
}

}  // namespace

Event EventCreate(bool manual_reset, bool initially_set) {
    std::lock_guard<std::mutex> g(g_pool_lock);
    for (Object& o : g_pool) {
        if (o.live) continue;
        std::lock_guard<std::mutex> l(o.m);
        o.live = true;
        o.manual = manual_reset;
        o.set = initially_set;
        return &o;
    }
    return nullptr;
}

bool EventSet(Event e) {
    Object* o = Of(e);
    if (!o) return false;
    {
        std::lock_guard<std::mutex> l(o->m);
        o->set = true;
    }
    // Auto-reset releases one wait, manual-reset every wait.
    if (o->manual)
        o->cv.notify_all();
    else
        o->cv.notify_one();
    return true;
}

bool EventReset(Event e) {
    Object* o = Of(e);
    if (!o) return false;
    std::lock_guard<std::mutex> l(o->m);
    o->set = false;
    return true;
}

WaitResult EventWait(Event e, std::uint32_t ms) {
    Object* o = Of(e);
    if (!o) return WaitResult::Failed;
    std::unique_lock<std::mutex> l(o->m);
    if (ms == kWaitForever) {
        o->cv.wait(l, [o] { return o->set; });
    } else if (!o->cv.wait_for(l, std::chrono::milliseconds(ms), [o] { return o->set; })) {
        return WaitResult::Timeout;
    }
    if (!o->manual) o->set = false;
    return WaitResult::Signalled;
}

void EventClose(Event e) {
    Object* o = Of(e);
    if (!o) return;
    std::lock_guard<std::mutex> g(g_pool_lock);
    std::lock_guard<std::mutex> l(o->m);
    o->live = false;
    o->set = false;
}

}  // namespace bof3::os
