#include "game/draw_emit.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

// The end of a 64 KB packet pool less 0x54 bytes of margin - the literal
// 0x7F1BAC in the original, for buffer 0.
constexpr std::uint32_t kPoolLimit = 0x7F1BAC;   // Gfx_PacketPools 0x7E1C00 + 0x10000 - 0x54

// A layer is 0x30 bytes: three lists, each a (first, last) pair per buffer.
// The second list, at +0x10, is the one closed here.
constexpr unsigned kLayerDwords = 12, kSecondList = 4;

// --- BOF3X_SHADOW=draw_emit: a differential fuzz, once at start-up ---------------
// Byte-copies of the two, their calls re-aimed at byte-copies of Gpu_LinkPrim,
// Gpu_SetDrawMode and each other. The packet pointer is kept inside the real
// pool - the room test compares addresses - often right at its limit; the
// ordering-table pointers are aimed at a scratch row of tails.

constexpr unsigned kLayers = 0x38;
constexpr unsigned kOwned = kLayers * kLayerDwords * 4 + 255 * 8;   // as DrawLayers_Reset's fuzz: the index is a whole byte
constexpr unsigned kWindow = 0x40;

struct State {
    unsigned char layers[kOwned];
    unsigned char* packet_next;
    unsigned long* ot[8];
    unsigned char buffer_index, slot;
    unsigned long tails[8];
    unsigned char window[kWindow];   // the pool, from packet_next as it was on the way in
};

unsigned long g_tails[8];

void Capture(State& s, const unsigned char* window_at) {
    std::memcpy(s.layers, DrawLayers, kOwned);
    s.packet_next = Gfx_PacketNext;
    std::memcpy(s.ot, Gfx_OtPointers, sizeof s.ot);
    s.buffer_index = Gfx_BufferIndex;
    s.slot = Draw_OtSlot;
    std::memcpy(s.tails, g_tails, sizeof s.tails);
    if (window_at) std::memcpy(s.window, window_at, kWindow);
}
void Apply(const State& s, unsigned char* window_at) {
    std::memcpy(DrawLayers, s.layers, kOwned);
    Gfx_PacketNext = s.packet_next;
    std::memcpy(Gfx_OtPointers, s.ot, sizeof s.ot);
    Gfx_BufferIndex = s.buffer_index;
    Draw_OtSlot = s.slot;
    std::memcpy(g_tails, s.tails, sizeof s.tails);
    if (window_at) std::memcpy(window_at, s.window, kWindow);
}

using Fn = void (__cdecl*)(unsigned, unsigned);

void SelfTest(Fn their_commit, Fn their_close) {
    constexpr unsigned kRounds = 12000;
    static State saved, input, their_out, our_out;
    Capture(saved, nullptr);
    static unsigned char saved_pool[2 * 0x10000];
    std::memcpy(saved_pool, Gfx_PacketPools, sizeof saved_pool);

    std::uint32_t rng = 0xC2B2AE35u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    unsigned bad = 0, full = 0, linked = 0;
    const Fn our_commit = reinterpret_cast<Fn>(reinterpret_cast<void*>(&Gfx_CommitPrim));
    const Fn our_close = reinterpret_cast<Fn>(reinterpret_cast<void*>(&DrawLayer_Close));
    for (unsigned round = 0; round < kRounds; ++round) {
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < sizeof input; ++i) bytes[i] = static_cast<unsigned char>(next());
        input.buffer_index = static_cast<unsigned char>(next() % 8 ? next() % 2 : next());
        input.slot = static_cast<unsigned char>(next() % 8);
        for (unsigned k = 0; k < 8; ++k) input.ot[k] = &g_tails[next() % 8];
        // The packet pointer: inside buffer 0's pool or buffer 1's, half the
        // time within 0x100 bytes of the room test's limit for buffer 0 or 1.
        const std::uint32_t pool = next() % 2 ? 0x10000u : 0u;
        std::uint32_t offset = next() % (0x10000u - kWindow - 0x200u);
        if (next() % 2 == 0) offset = 0x10000u - 0x54u - 0x100u + next() % 0x100u;
        input.packet_next = Gfx_PacketPools + pool + offset;
        // A layer's second list: empty a third of the time.
        const unsigned layer = next() % kLayers;
        const bool is_close = round % 2 == 1;
        unsigned long* const cell =
            reinterpret_cast<unsigned long*>(input.layers) + layer * kLayerDwords + kSecondList + input.buffer_index * 2u;
        if (reinterpret_cast<unsigned char*>(cell) + 8 <= input.layers + kOwned && next() % 3 == 0) cell[0] = 0;
        // Noise above the byte each reads; the slot itself one of the eight
        // there are, since the ninth is somebody else's memory.
        const unsigned a0 = is_close ? layer : ((next() & ~0xFFu) | (next() % 8)), a1 = next();

        unsigned char* const window_at = input.packet_next;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input, window_at);
            if (is_close) (pass ? our_close : their_close)(a0, a1);
            else (pass ? our_commit : their_commit)(a0, a1);
            Capture(pass ? our_out : their_out, window_at);
        }
        if (their_out.packet_next == input.packet_next) ++full;
        if (is_close && std::memcmp(their_out.tails, input.tails, sizeof input.tails) != 0) ++linked;
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 8)
            bof3::Log("shadow      draw_emit self-test MISMATCH: %s, round %u, buffer %u, packet pointer %p",
                      is_close ? "DrawLayer_Close" : "Gfx_CommitPrim", round, input.buffer_index,
                      static_cast<void*>(input.packet_next));
    }
    Apply(saved, nullptr);
    std::memcpy(Gfx_PacketPools, saved_pool, sizeof saved_pool);
    bof3::Log("shadow      draw_emit self-test: 2 functions, %u rounds (%u where the pool had no room, %u closes that "
              "linked something), %u MISMATCHES; the layers, the packet pointer and 0x%X bytes at it, the "
              "ordering-table pointers and their tails compared", kRounds, full, linked, bad, kWindow);
    if (bad) bof3::Fatal("draw_emit differs from the originals in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

// original 0x461E50: the primitive just built at Gfx_PacketNext joins ordering
// -table slot `slot`, and the packet pointer moves on by `size` - if the pool
// has room for it, and silently not at all if it has not. Both arguments are
// bytes. 640,316 calls a whole attract cycle.
//
// As the original has it: the slot is not checked against the eight there are.
extern "C" void __cdecl Gfx_CommitPrim(unsigned slot, unsigned size) {
    size &= 0xFF;
    const std::uint32_t limit = (static_cast<std::uint32_t>(Gfx_BufferIndex) << 16) + kPoolLimit;
    const std::uint32_t next = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Gfx_PacketNext));
    if (limit <= next + size) return;
    slot &= 0xFF;
    Gpu_LinkPrim(Gfx_OtPointers[slot], next);
    Gfx_OtPointers[slot] = reinterpret_cast<unsigned long*>(Gfx_PacketNext);
    Gfx_PacketNext = Gfx_PacketNext + size;
}

// original 0x56FE80: after a layer's sprites, in the draw-order pass - a
// draw-mode primitive (texture page 0x95, nothing else set) committed to slot
// 6, then the layer's second list, if it has one, appended to the slot
// Draw_OtSlot names. 403,315 calls.
extern "C" void __cdecl DrawLayer_Close(int layer) {
    Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, 0x95, 0);
    Gfx_CommitPrim(6, 0xC);
    const std::uint32_t first = static_cast<std::uint32_t>(layer) * 6u;
    unsigned long* cell = DrawLayers + kSecondList + (Gfx_BufferIndex + first) * 2u;
    if (cell[0] == 0) return;
    Gpu_LinkPrim(Gfx_OtPointers[Draw_OtSlot], cell[0]);
    // The index is read again after the call, as the original reads it.
    cell = DrawLayers + kSecondList + (Gfx_BufferIndex + first) * 2u;
    Gfx_OtPointers[Draw_OtSlot] = reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(cell[1]));
}

void DrawEmit_Inject() {
    // Called before psx_gpu's Inject: a function can only be cloned before its
    // entry is patched. Calls, capstone 2026-09-20: Gfx_CommitPrim +0x44 ->
    // Gpu_LinkPrim; DrawLayer_Close +0x12 -> Gpu_SetDrawMode, +0x1B ->
    // Gfx_CommitPrim, +0x52 -> Gpu_LinkPrim. No jump leaves either.
    if (bof3::WantsShadow("draw_emit")) {
        const void* link = bof3::CloneOriginal("Gpu_LinkPrim", bof3::addr::Gpu_LinkPrim, 0xB);
        const void* mode = bof3::CloneOriginal("Gpu_SetDrawMode", bof3::addr::Gpu_SetDrawMode, 0x41);
        const bof3::CloneCall commit_calls[] = {{0x44, link}};
        const void* commit =
            bof3::CloneOriginal("Gfx_CommitPrim", bof3::addr::Gfx_CommitPrim, 0x5D, commit_calls, 1);
        const bof3::CloneCall close_calls[] = {{0x12, mode}, {0x1B, commit}, {0x52, link}};
        const void* close = bof3::CloneOriginal("DrawLayer_Close", bof3::addr::DrawLayer_Close, 0x7C, close_calls, 3);
        SelfTest(reinterpret_cast<Fn>(const_cast<void*>(commit)), reinterpret_cast<Fn>(const_cast<void*>(close)));
    }
    BOF3_INJECT(Gfx_CommitPrim);
    BOF3_INJECT(DrawLayer_Close);
}
