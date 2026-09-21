#include "game/msg_pool.h"

#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

// The pool's place in the DAT arena: MessagePools + 0x4000, PSX 0x80014000.
// 44 shipped files carry a kind-0 chunk with this tag, 0x2D2B..0x3743 bytes,
// and no other chunk lands in or across 0x4000..0x8000 (census 2026-09-20).
constexpr std::uint32_t kPoolTag = 0x4000;
constexpr std::uint32_t kPoolRoom = 0x4000;       // in the arena, to the CLUT strip as loaded
// The relocated pool's room. The Western pools outgrew 0x4000 - FIRST's is
// 0x41E8 bytes on the US disc, which is why Capcom moved it to 0x8001A000.
// tools/loc_build.py refuses a pool larger than this (POOL_ROOM).
constexpr std::uint32_t kMovedRoom = 0x8000;

std::uint8_t* ArenaPool() {
    return reinterpret_cast<std::uint8_t*>(bof3::addr::MessagePools) + kPoolTag;
}

// Where Msg_SystemPtr reads. The arena until MsgPool_Relocate is called.
std::uint8_t* g_pool;

// --- BOF3X_SHADOW=msg_pool -------------------------------------------------
// Twelve instructions, no calls, no jumps: a byte-copy runs anywhere, and
// reads the arena whatever ours does. The arena window is filled with noise
// for the test and restored. Header words are kept below 0x2000; the index is
// the full 14 bits, seeded with the values either side of each of its upper
// bits (a first version drew indices below 0x800 and could not tell a 0x1FFF
// mask from 0x3FFF - the negative control said so). A read then reaches up to
// 0xA000 past the pool's start: further into the arena, which is only read,
// and which the relocated copy carries too. The argument's upper half, which
// the original does not read, is noise.
using SystemPtrFn = const unsigned char* (__cdecl*)(unsigned);

void SelfTest(SystemPtrFn theirs) {
    constexpr std::uint32_t kReach = 0xA000;  // 0x2000 + 2 * 0x3FFF, rounded up
    static std::uint8_t saved[kPoolRoom], moved[kReach];
    std::uint8_t* const arena = ArenaPool();
    std::memcpy(saved, arena, kPoolRoom);
    std::uint32_t rng = 0xC2B2AE35u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    for (std::uint32_t i = 0; i < kPoolRoom; ++i) arena[i] = static_cast<std::uint8_t>(next());
    for (int w = 0; w < 4; ++w) {
        const std::uint32_t header = next() % 0x2000;
        std::memcpy(arena + 4 * w, &header, sizeof header);
    }
    std::memcpy(moved, arena, kReach);
    static const unsigned kSeeds[] = {0, 1, 0x7FF, 0x800, 0xFFF, 0x1000, 0x1FFF, 0x2000, 0x2FFF, 0x3000, 0x3FFE, 0x3FFF};

    std::uint8_t* const before = g_pool;
    unsigned bad = 0;
    const unsigned rounds = 20000;
    for (unsigned n = 0; n < rounds; ++n) {
        const unsigned index = (n % 4 == 0) ? kSeeds[next() % (sizeof kSeeds / sizeof kSeeds[0])] : next() & 0x3FFF;
        const unsigned id = (next() & 0xFFFF0000u) | ((next() & 3) << 14) | index;
        const auto their_off = theirs(id) - arena;
        g_pool = arena;  // in place, as the original
        const auto ours_off = Msg_SystemPtr(id) - arena;
        g_pool = moved;  // and relocated: the same offset into the copy
        const auto moved_off = Msg_SystemPtr(id) - moved;
        if ((their_off != ours_off || their_off != moved_off) && ++bad <= 8)
            bof3::Log("shadow      Msg_SystemPtr self-test MISMATCH id %08X: %d / %d / %d", id,
                      (int)their_off, (int)ours_off, (int)moved_off);
    }
    g_pool = before;
    std::memcpy(arena, saved, kPoolRoom);
    bof3::Log("shadow      Msg_SystemPtr self-test: %u ids, in place and relocated, %u MISMATCHES", rounds, bad);
    if (bad) bof3::Fatal("Msg_SystemPtr differs from the original in %u of %u rounds", bad, rounds);
}

}  // namespace

// original 0x497740. The low word of the argument is the id; bits 14-15 pick
// one of four header dwords, each the offset of a u16 offset table; the rest
// indexes it. No bound on either, as in the original.
//
// DIVERGENCE DIV-0007: the pool's base is a variable. Unrelocated it is the
// original's constant and the result is the original's.
extern "C" const unsigned char* __cdecl Msg_SystemPtr(unsigned id) {
    id &= 0xFFFF;
    std::uint32_t table;
    std::memcpy(&table, g_pool + (id >> 14) * 4, sizeof table);
    const std::uint8_t* block = g_pool + table;
    std::uint16_t off;
    std::memcpy(&off, block + 2 * (id & 0x3FFF), sizeof off);
    return block + off;
}

void MsgPool_Relocate() {
    static std::uint8_t pool[kMovedRoom];
    g_pool = pool;
    bof3::Log("DIV-0007: system pool relocated out of the arena, to %p", (void*)pool);
}

bool MsgPool_TakeChunk(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size) {
    if (g_pool == ArenaPool() || tag != kPoolTag) return false;
    if (size > kMovedRoom) bof3::Fatal("system pool chunk of 0x%X bytes, room is 0x%X", (unsigned)size, (unsigned)kMovedRoom);
    std::memcpy(g_pool, payload, size);
    return true;
}

void MsgPool_Inject() {
    g_pool = ArenaPool();
    // 0x497740..0x49776B: no calls, no jumps (disasm 2026-09-20).
    if (bof3::WantsShadow("msg_pool"))
        SelfTest(reinterpret_cast<SystemPtrFn>(
            bof3::CloneOriginal("Msg_SystemPtr", bof3::addr::Msg_SystemPtr, 0x2B)));
    BOF3_INJECT(Msg_SystemPtr);
}
