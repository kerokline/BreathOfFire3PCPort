// bof3::os events on Linux: Win32's event semantics over a std::mutex and a
// std::condition_variable, from a static pool. See os/event.h.
//
// A set is handed to the waits blocked at the set itself, as the Windows
// kernel hands it: each blocked wait has a node in the event's list, and a
// set marks the first one released (auto-reset) or all of them (manual-reset)
// under the lock. Only with no wait blocked does an auto-reset set raise the
// flag for the next wait. A flag alone loses a release: two sets, the second
// made before the first woken wait has run, would find the flag still up and
// add nothing, and the second wait would sleep on (a reset straight after a
// manual-reset set would do the same to every wait).

#include "os/event.h"
#include "os/os.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace bof3::os {

namespace {

// One blocked wait, on its own stack, in its event's list while it waits.
struct Wait {
    Wait* next = nullptr;
    bool released = false;   // under the event's m
};

struct Object {
    std::mutex m;
    std::condition_variable cv;
    bool live = false;     // under g_pool_lock
    bool manual = false;
    bool set = false;      // under m
    Wait* first = nullptr; // under m: the blocked waits not yet released, oldest first
    Wait* last = nullptr;
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
        if (o->manual) {
            // Manual-reset: every blocked wait is released, and the event stays set.
            for (Wait* w = o->first; w; w = w->next) w->released = true;
            o->first = o->last = nullptr;
            o->set = true;
        } else if (Wait* w = o->first) {
            // Auto-reset with a wait blocked: that wait takes the set, the event stays clear.
            o->first = w->next;
            if (!o->first) o->last = nullptr;
            w->released = true;
        } else {
            o->set = true;   // nobody waits: kept for the next wait
        }
    }
    // Every blocked wait looks at its own node; there are one or two of them.
    o->cv.notify_all();
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
    if (o->set) {
        if (!o->manual) o->set = false;
        return WaitResult::Signalled;
    }
    if (ms == 0) return WaitResult::Timeout;
    Wait self;
    if (o->last)
        o->last->next = &self;
    else
        o->first = &self;
    o->last = &self;
    if (ms == kWaitForever) {
        o->cv.wait(l, [&self] { return self.released; });
    } else if (!o->cv.wait_for(l, std::chrono::milliseconds(ms), [&self] { return self.released; })) {
        // Given up, and no set took this wait (the predicate was read under
        // the lock): out of the list, so that no later set is spent on it.
        Wait* before = nullptr;
        Wait* w = o->first;
        for (; w && w != &self; w = w->next) before = w;
        if (w) {
            (before ? before->next : o->first) = self.next;
            if (o->last == &self) o->last = before;
        }
        return WaitResult::Timeout;
    }
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
