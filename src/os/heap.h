// bof3::os, the heap the game sees: the C runtime's exact block size, a
// reservation below 4 GB, and an arena of our own over one.
//
// The 64-bit engine keeps 32-bit words that hold heap addresses (Sound_Banks'
// data, Music_File, Gfx_ClutRows' rows, ...: docs/x64-build-design.md 5), so
// every block the game's allocator Crt_malloc hands out must lie below 4 GB.
// Windows' fixed-base executable gives that by placement; glibc's heap does
// not (docs/linux-round.md LA2: 0 of 48 blocks of 64 MB below 4 GB). The
// quicksave also asks a block's exact size (MSVC's _msize), which glibc does
// not answer (malloc_usable_size answers the larger usable size). LowArena is
// the answer to both: a reservation below 4 GB, committed as it fills, and a
// first-fit allocator over it that keeps each block's requested size.
//
// The allocator is portable code (src/os/heap_arena.cpp) over the seam's
// memory calls; HeapExactSize is each backend's. Part of the operating-system
// seam (docs/linux-round.md, LB1): no Windows or POSIX type appears here, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>

namespace bof3::os {

// The C runtime heap's exact size of a std::malloc block: MSVC's _msize on
// Windows (true). POSIX has no exact answer, so false there and *bytes is
// untouched. HeapHasExactSize says which, without a block.
bool HeapHasExactSize() noexcept;
bool HeapExactSize(const void* block, std::size_t* bytes) noexcept;

// `size` bytes of address space with no access, the whole run inside
// [floor, 4 GB), placed as high as a free run allows, 64 KB aligned; null
// when no free run below 4 GB holds it. On a 32-bit process every address is
// below 4 GB and the run is the highest free one above `floor`.
void* MemReserveBelow4G(std::size_t size, std::uintptr_t floor);

// An allocator over one reservation below 4 GB. Every block is 16-byte
// aligned (what the Windows C runtime gives on x86-64, more than its 8 on
// x86-32), carries a 16-byte header with its requested size, and is taken
// first-fit from a free list; a freed block merges with free neighbours, and
// one that ends at the arena's used end gives that room back. Pages are
// committed as the used end rises and never decommitted. Thread-safe: one
// lock around every call but Owns. Never destroyed with blocks live: the
// owner keeps it for the process's life.
class LowArena {
public:
    static constexpr std::size_t kAlign = 16;
    static constexpr std::size_t kHeader = 16;
    // The largest request (MSVC 6's _HEAP_MAXREQ, 0xFFFFFFE0); above it Alloc
    // answers null, as the C runtime does.
    static constexpr std::size_t kMaxRequest = 0xFFFFFFE0u;

    constexpr LowArena() = default;
    LowArena(const LowArena&) = delete;
    LowArena& operator=(const LowArena&) = delete;

    // Reserve `bytes` (rounded up to 64 KB) for the arena: inside
    // [0x80000000, 4 GB) if a free run there holds it (above every top byte
    // of the bind table, bind.cpp, and of MAP_32BIT's 2 GB), else where the
    // system places a reservation if that lies below 4 GB, else anywhere in
    // [64 KB, 2 GB). False if none, or if already open.
    bool Open(std::size_t bytes);
    // An arena over memory the caller already reserved (tests): `base` 64 KB
    // aligned, the whole run below 4 GB. False if not.
    bool OpenAt(void* base, std::size_t bytes);

    // A block of at least `n` bytes, or null (n above kMaxRequest, or no room:
    // no free block fits and the reservation cannot rise, or a commit failed).
    // n == 0 gives a block of its own, of size 0.
    void* Alloc(std::size_t n);
    // count * size zero-filled; null when the product overflows kMaxRequest.
    void* Calloc(std::size_t count, std::size_t size);
    // C's realloc: null p is Alloc(n); n == 0 frees p and answers null (MSVC's
    // rule); else the block grown or shrunk in place when it can be, moved
    // when not, and null with p untouched when there is no room.
    void* Realloc(void* p, std::size_t n);
    // False when p is not a live block of this arena (the caller's Fatal).
    // Null is not passed here (the caller's free(nullptr) does nothing).
    bool Free(void* p);
    // The size requested for a live block (what _msize answers for it).
    bool SizeOf(const void* p, std::size_t* bytes) const;
    // Whether p lies inside the reservation (live or not).
    bool Owns(const void* p) const noexcept {
        const auto a = reinterpret_cast<std::uintptr_t>(p);
        return base_ != 0 && a >= base_ && a < base_ + reserved_;
    }

    std::uintptr_t Base() const noexcept { return base_; }
    std::size_t Reserved() const noexcept { return reserved_; }
    std::size_t Committed() const;
    std::size_t InUse() const;      // the bytes requested by live blocks
    std::size_t End() const;        // the used end now: 0 when every block is free
    std::size_t HighWater() const;  // the highest the used end has risen, in bytes
    // Every invariant walked (sizes chain, prev links, free list, no two free
    // neighbours, no free block at the end); false and `why` set if one fails.
    bool Verify(const char** why) const;

private:
    struct Head;
    Head* HeadAt(std::uint32_t off) const;
    std::uint32_t OffOf(const Head* h) const;
    void Link(Head* h);
    void Unlink(Head* h);
    bool CommitTo(std::size_t end);
    Head* Live(const void* p) const;
    void* Take(std::size_t need, std::size_t n);
    void Split(Head* h, std::size_t need);
    void Release(Head* h);
    void* AllocLocked(std::size_t n);

    mutable std::mutex lock_;
    std::uintptr_t base_ = 0;
    std::size_t reserved_ = 0;
    std::size_t committed_ = 0;  // bytes from base_ committed
    std::uint32_t end_ = 0;      // offset past the last block
    std::uint32_t tail_ = 0;     // size of the block that ends at end_ (0: none)
    std::uint32_t free_ = 0xFFFFFFFFu;  // first free block's offset
    std::size_t in_use_ = 0;
    std::size_t high_ = 0;
};

}  // namespace bof3::os
