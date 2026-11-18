// bof3::os::LowArena and MemReserveBelow4G: portable code over the seam's
// memory calls (os/memory.h). See os/heap.h.
//
// The layout: the arena is a run of blocks from its base up to end_, then
// reserved room (the "wilderness") up to the reservation's end. Each block
// starts with a 16-byte header {size, prev, requested, tag}: its whole size
// with the header (a multiple of 16, 32 at least), the size of the block just
// below it (0 for the first), the bytes asked for, and a tag that says used
// or free. A free block holds two offsets, next and prev, of the free list in
// its payload. Invariants (Verify): no two free blocks are neighbours, and no
// free block ends at end_ - a freed block that would is given back to the
// wilderness instead, so freeing everything leaves end_ at 0.
#include "os/heap.h"

#include "os/memory.h"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace bof3::os {

namespace {

constexpr std::uint64_t k4G = 0x100000000ull;
constexpr std::uint64_t kGrain = 0x10000;  // reservations and commits: 64 KB, Windows' granularity
constexpr std::uint32_t kUsed = 0xA11CB0F3u;
constexpr std::uint32_t kFree = 0xF4EEB0F3u;
constexpr std::uint32_t kNone = 0xFFFFFFFFu;
constexpr std::size_t kMinBlock = 32;  // a header and the two free-list offsets, rounded to 16

constexpr std::uint64_t RoundUp(std::uint64_t v, std::uint64_t to) { return (v + to - 1) / to * to; }

struct Links {
    std::uint32_t next, prev;
};

}  // namespace

struct LowArena::Head {
    std::uint32_t size;       // the whole block, header included
    std::uint32_t prev;       // the size of the block just below, 0 for the first
    std::uint32_t requested;  // the bytes asked for (live blocks)
    std::uint32_t tag;        // kUsed or kFree
};

namespace {
Links* LinksOf(void* h) { return reinterpret_cast<Links*>(static_cast<unsigned char*>(h) + LowArena::kHeader); }
}  // namespace

void* MemReserveBelow4G(std::size_t size, std::uintptr_t floor) {
    if (size == 0) return nullptr;
    const std::uint64_t bytes = RoundUp(size, kGrain);
    if (bytes >= k4G) return nullptr;
    // The free runs in [floor, 4 GB), walked upwards by MemQuery.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
    std::uint64_t a = std::max<std::uint64_t>(floor, kGrain);
    while (a < k4G) {
        MemInfo m;
        if (!MemQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a)), &m)) break;
        const std::uint64_t lo = m.base;
        std::uint64_t hi = lo + static_cast<std::uint64_t>(m.size);
        if (hi < lo || hi > k4G) hi = k4G;   // a free run to the end of the address space
        if (hi <= a) break;                  // no progress: the query's answer does not cover a
        if (m.state == MemState::Free) runs.emplace_back(std::max(lo, a), hi);
        a = hi;
    }
    // The highest run first: the bottom-up allocations of the system, the
    // image and the fixed reservations of ours lie low.
    for (auto r = runs.rbegin(); r != runs.rend(); ++r) {
        const std::uint64_t lo = RoundUp(r->first, kGrain), hi = r->second & ~(kGrain - 1);
        if (hi < lo || hi - lo < bytes) continue;
        void* const at = reinterpret_cast<void*>(static_cast<std::uintptr_t>(hi - bytes));
        void* const got = MemReserve(at, static_cast<std::size_t>(bytes));
        if (got == at) return got;
        if (got) MemRelease(got, static_cast<std::size_t>(bytes));
    }
    return nullptr;
}

bool LowArena::Open(std::size_t bytes) {
    const std::uint64_t want = RoundUp(bytes, kGrain);
    if (want == 0 || want >= k4G) return false;
    const auto n = static_cast<std::size_t>(want);
    void* p = MemReserveBelow4G(n, 0x80000000u);
    if (!p) {
        void* const q = MemReserve(nullptr, n);
        if (q && reinterpret_cast<std::uintptr_t>(q) + static_cast<std::uint64_t>(n) <= k4G)
            p = q;
        else if (q)
            MemRelease(q, n);
    }
    if (!p) p = MemReserveBelow4G(n, static_cast<std::uintptr_t>(kGrain));
    if (!p) return false;
    if (!OpenAt(p, n)) {
        MemRelease(p, n);
        return false;
    }
    return true;
}

bool LowArena::OpenAt(void* base, std::size_t bytes) {
    std::lock_guard<std::mutex> hold(lock_);
    const auto b = reinterpret_cast<std::uintptr_t>(base);
    if (base_ != 0 || b == 0 || b % 0x1000 != 0 || bytes < kMinBlock || bytes % 0x1000 != 0 ||
        static_cast<std::uint64_t>(b) + bytes > k4G)
        return false;
    base_ = b;
    reserved_ = bytes;
    committed_ = 0;
    end_ = 0;
    tail_ = 0;
    free_ = kNone;
    in_use_ = 0;
    high_ = 0;
    return true;
}

LowArena::Head* LowArena::At(std::uint32_t off) const {
    static_assert(sizeof(Head) == kHeader, "the block header");
    return reinterpret_cast<Head*>(base_ + off);
}
std::uint32_t LowArena::OffOf(const Head* h) const {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(h) - base_);
}

void LowArena::Link(Head* h) {
    Links* const l = LinksOf(h);
    l->prev = kNone;
    l->next = free_;
    if (free_ != kNone) LinksOf(At(free_))->prev = OffOf(h);
    free_ = OffOf(h);
}

void LowArena::Unlink(Head* h) {
    Links* const l = LinksOf(h);
    if (l->prev != kNone)
        LinksOf(At(l->prev))->next = l->next;
    else
        free_ = l->next;
    if (l->next != kNone) LinksOf(At(l->next))->prev = l->prev;
}

bool LowArena::CommitTo(std::size_t end) {
    if (end <= committed_) return true;
    const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(RoundUp(end, kGrain), reserved_));
    void* const at = reinterpret_cast<void*>(base_ + committed_);
    if (MemCommit(at, want - committed_, Prot::ReadWrite) != at) return false;
    committed_ = want;
    return true;
}

LowArena::Head* LowArena::Live(const void* p) const {
    if (!Owns(p)) return nullptr;
    const std::uintptr_t rel = reinterpret_cast<std::uintptr_t>(p) - base_;
    if (rel < kHeader || rel % kAlign != 0) return nullptr;
    const auto off = static_cast<std::uint32_t>(rel - kHeader);
    if (off >= end_) return nullptr;
    Head* const h = At(off);
    if (h->tag != kUsed || h->size < kMinBlock || static_cast<std::uint64_t>(off) + h->size > end_) return nullptr;
    return h;
}

// A free block (unlinked or new) as a block of `need` bytes: the rest, when
// it can stand as a block, made one and released (it merges with a free
// block above it or goes back to the wilderness).
void LowArena::Split(Head* h, std::size_t need) {
    if (h->size - need < kMinBlock) return;
    Head* const rest = At(OffOf(h) + static_cast<std::uint32_t>(need));
    rest->size = h->size - static_cast<std::uint32_t>(need);
    rest->prev = static_cast<std::uint32_t>(need);
    rest->requested = 0;
    rest->tag = kUsed;
    h->size = static_cast<std::uint32_t>(need);
    const std::uint32_t above = OffOf(rest) + rest->size;
    if (above < end_)
        At(above)->prev = rest->size;
    else
        tail_ = rest->size;
    Release(rest);
}

// A used block freed: merged with a free neighbour on either side, then
// linked, or given back to the wilderness when it ends at end_.
void LowArena::Release(Head* h) {
    h->tag = kFree;
    std::uint32_t off = OffOf(h);
    const std::uint32_t above = off + h->size;
    if (above < end_) {
        Head* const n = At(above);
        if (n->tag == kFree) {
            Unlink(n);
            n->tag = 0;
            h->size += n->size;
        }
    }
    if (h->prev != 0) {
        Head* const p = At(off - h->prev);
        if (p->tag == kFree) {
            Unlink(p);
            h->tag = 0;
            p->size += h->size;
            h = p;
            off = OffOf(p);
        }
    }
    const std::uint32_t next = off + h->size;
    if (next >= end_) {
        end_ = off;
        tail_ = h->prev;
        h->tag = 0;
        return;
    }
    At(next)->prev = h->size;
    Link(h);
}

void* LowArena::Take(std::size_t need, std::size_t n) {
    for (std::uint32_t off = free_; off != kNone; off = LinksOf(At(off))->next) {
        Head* const h = At(off);
        if (h->size < need) continue;
        Unlink(h);
        h->tag = kUsed;
        Split(h, need);
        h->requested = static_cast<std::uint32_t>(n);
        in_use_ += n;
        return reinterpret_cast<unsigned char*>(h) + kHeader;
    }
    if (static_cast<std::uint64_t>(end_) + need > reserved_ || !CommitTo(end_ + need)) return nullptr;
    Head* const h = At(end_);
    h->size = static_cast<std::uint32_t>(need);
    h->prev = tail_;
    h->requested = static_cast<std::uint32_t>(n);
    h->tag = kUsed;
    end_ += static_cast<std::uint32_t>(need);
    tail_ = h->size;
    high_ = std::max<std::size_t>(high_, end_);
    in_use_ += n;
    return reinterpret_cast<unsigned char*>(h) + kHeader;
}

namespace {
std::size_t Need(std::size_t n) { return std::max<std::size_t>(RoundUp(n + LowArena::kHeader, LowArena::kAlign), kMinBlock); }
}  // namespace

void* LowArena::AllocLocked(std::size_t n) {
    if (base_ == 0 || n > kMaxRequest) return nullptr;
    const std::size_t need = Need(n);
    if (need > reserved_) return nullptr;
    return Take(need, n);
}

void* LowArena::Alloc(std::size_t n) {
    std::lock_guard<std::mutex> hold(lock_);
    return AllocLocked(n);
}

void* LowArena::Calloc(std::size_t count, std::size_t size) {
    if (size != 0 && count > kMaxRequest / size) return nullptr;
    const std::size_t n = count * size;
    std::lock_guard<std::mutex> hold(lock_);
    void* const p = AllocLocked(n);
    if (p) std::memset(p, 0, n);
    return p;
}

void* LowArena::Realloc(void* p, std::size_t n) {
    if (!p) return Alloc(n);
    std::lock_guard<std::mutex> hold(lock_);
    Head* const h = Live(p);
    if (!h) return nullptr;
    if (n == 0) {
        in_use_ -= h->requested;
        Release(h);
        return nullptr;
    }
    if (n > kMaxRequest) return nullptr;
    const std::size_t need = Need(n);
    const std::uint32_t off = OffOf(h);
    if (need <= h->size) {   // shrink (or the same): in place
        in_use_ = in_use_ - h->requested + n;
        h->requested = static_cast<std::uint32_t>(n);
        Split(h, need);
        return p;
    }
    const std::uint32_t above = off + h->size;
    if (above == end_) {   // the top block: up into the wilderness
        if (static_cast<std::uint64_t>(off) + need <= reserved_ && CommitTo(off + need)) {
            end_ = off + static_cast<std::uint32_t>(need);
            h->size = static_cast<std::uint32_t>(need);
            tail_ = h->size;
            high_ = std::max<std::size_t>(high_, end_);
            in_use_ = in_use_ - h->requested + n;
            h->requested = static_cast<std::uint32_t>(n);
            return p;
        }
    } else {
        Head* const nb = At(above);
        if (nb->tag == kFree && static_cast<std::size_t>(h->size) + nb->size >= need) {   // into the free block above
            Unlink(nb);
            nb->tag = 0;
            h->size += nb->size;
            At(off + h->size)->prev = h->size;   // a free block never ends at end_
            Split(h, need);
            in_use_ = in_use_ - h->requested + n;
            h->requested = static_cast<std::uint32_t>(n);
            return p;
        }
    }
    const std::size_t old = h->requested;
    void* const q = AllocLocked(n);
    if (!q) return nullptr;
    std::memcpy(q, p, std::min(old, n));
    in_use_ -= old;
    Release(h);
    return q;
}

bool LowArena::Free(void* p) {
    std::lock_guard<std::mutex> hold(lock_);
    Head* const h = Live(p);
    if (!h) return false;
    in_use_ -= h->requested;
    Release(h);
    return true;
}

bool LowArena::SizeOf(const void* p, std::size_t* bytes) const {
    std::lock_guard<std::mutex> hold(lock_);
    const Head* const h = Live(p);
    if (!h) return false;
    *bytes = h->requested;
    return true;
}

std::size_t LowArena::Committed() const {
    std::lock_guard<std::mutex> hold(lock_);
    return committed_;
}
std::size_t LowArena::InUse() const {
    std::lock_guard<std::mutex> hold(lock_);
    return in_use_;
}
std::size_t LowArena::End() const {
    std::lock_guard<std::mutex> hold(lock_);
    return end_;
}
std::size_t LowArena::HighWater() const {
    std::lock_guard<std::mutex> hold(lock_);
    return high_;
}

bool LowArena::Verify(const char** why) const {
    std::lock_guard<std::mutex> hold(lock_);
    auto fail = [why](const char* w) {
        if (why) *why = w;
        return false;
    };
    if (end_ > committed_ || committed_ > reserved_) return fail("end, committed and reserved out of order");
    std::uint32_t off = 0, last = 0, frees = 0;
    std::size_t used = 0;
    bool last_free = false;
    while (off < end_) {
        const Head* const h = At(off);
        if (h->size < kMinBlock || h->size % kAlign != 0) return fail("a block's size");
        if (static_cast<std::uint64_t>(off) + h->size > end_) return fail("a block past the used end");
        if (h->prev != last) return fail("a block's prev size");
        if (h->tag == kFree) {
            if (last_free) return fail("two free neighbours");
            ++frees;
            last_free = true;
        } else if (h->tag == kUsed) {
            if (Need(h->requested) > h->size) return fail("a block smaller than its request");
            used += h->requested;
            last_free = false;
        } else {
            return fail("a block's tag");
        }
        last = h->size;
        off += h->size;
    }
    if (off != end_) return fail("the blocks do not end at the used end");
    if (last != tail_) return fail("the top block's size");
    if (last_free) return fail("a free block at the used end");
    if (used != in_use_) return fail("the bytes in use");
    std::uint32_t listed = 0, prev = kNone;
    for (std::uint32_t f = free_; f != kNone; f = LinksOf(At(f))->next) {
        if (f >= end_ || At(f)->tag != kFree) return fail("a listed block that is not free");
        if (LinksOf(At(f))->prev != prev) return fail("the free list's back link");
        if (++listed > frees) return fail("the free list longer than the free blocks");
        prev = f;
    }
    if (listed != frees) return fail("a free block not listed");
    return true;
}

}  // namespace bof3::os
