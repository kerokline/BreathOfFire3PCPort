// Round thirteen group E2B (docs/effect_2b.md): analysis/round13_cut.tsv's 51
// rows for E2B and the one start inside them that no list has (0x473F10, kind
// 0x38's tile, the tail of 0x473EC0 that 0x473E30 and 0x473E90 jump to), each
// read to its last instruction with capstone (2026-09-29) and fuzzed through
// the scenario harness's effect mode (scenario_harness.h,
// docs/scenario_harness.md section 8).
//
//   EffectKind2F_Run .. _Spin          0x4731A0 .. 0x473240  Effect_KindHandlers[0x2F], EffectKind2F_States 0, 1
//   EffectKind2F_StepTrail .. _InitTrail  0x4732D0 .. 0x473460  a trail's head moved, drawn, set up (E8)
//   EffectKind33_Run .. _Grow          0x4734F0 .. 0x473550  Effect_KindHandlers[0x33], EffectKind33_States 0, 1
//   EffectKind33_DrawDisc, _DrawQuarter  0x4735B0, 0x473600  64 flat triangles round a point (E8)
//   EffectKind35_Run .. _FreeModel     0x4737A0 .. 0x473800  Effect_KindHandlers[0x35], EffectKind35_States 0..2
//   EffectKind35_ShardsInit .. _TurnPiece  0x473810 .. 0x473CE0  kind 0x1E's shards and pieces, sixteen and 55
//   EffectKind38_Run .. _DrawTint      0x473DD0 .. 0x473F10  Effect_KindHandlers[0x38], EffectKind38_States 0..3, the tile
//   EffectKind39_Run .. _DrawDisc      0x473FA0 .. 0x474070  Effect_KindHandlers[0x39], EffectKind39_States 0..2, a fan
//   EffectKind3B_Run .. _DrawGlow      0x4741E0 .. 0x4747D0  Effect_KindHandlers[0x3B], EffectKind3B_States 0..2, spirals
//   EffectKind3D_Run .. _DrawRing      0x474940 .. 0x474D20  Effect_KindHandlers[0x3D], EffectKind3D_States 0..3, rings
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again after a call, and
// kept where it keeps it in a register. No divergence: each is a faithful
// replacement. Where the original would jump through a state table past its
// code or divide by zero, ours aborts with a message (docs/effect_2b.md
// section 7).
#include "game/effect_2b.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2b::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;
using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
short SW(const unsigned char* p) { return static_cast<short>(Word(p)); }
U Key(const void* p) { return Low32(p); }
U AddressOf(const void* p) { return Key(p); }
void StoreFloat(unsigned char* at, double v) {
    const auto f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

// imul of two dwords (the product's low 32 bits), then sar: how every point
// here is placed round its centre (>> 12 for the fixed-point trig, >> 4 for
// kind 0x3B's spirals).
std::int32_t MulSar(int t, U v, unsigned shift) { return static_cast<std::int32_t>(static_cast<U>(t) * v) >> shift; }

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32.
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");

// jmp / call [table + 4 * index]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original goes through the dword after - the next kind's table or data.
Handler Entry(U table, unsigned index, unsigned entries, const char* who) {
    if (index >= entries)
        bof3::Fatal("%s: index %u past the %u entries of the table at 0x%X (the original jumps through 0x%X)", who,
                    index, entries, (unsigned)table, (unsigned)(table + 4u * index));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4u * index))));
}

// Gpu_GetTPage(0, abr, x, y) and a draw mode of it at Gfx_PacketNext (dfe 0,
// dtd `dtd`); the fifth zero the original pushed for Gpu_GetTPage and left on
// the stack is the mode's texture window.
void Mode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

// Kinds 0x3B's and 0x3D's glow: the map camera loaded, `point` projected,
// `size` scaled at its depth (EffectGte_ProjectSize: in = v, out = v + 2 - the
// original's second in word is its stack's leftover, ours 0; out's second word
// is not read); the radius that plus (Frame_Counter & 1) << `parity`, 16 bits;
// 32 semi-transparent G3 triangles fanned round the screen point at steps of
// 0x80 (the first angle whole, the second & 0xFFF), the centre `centre`'s
// shade and the two rim points `rim`'s, the depth at every vertex; each
// committed at slot 1 (0x34 bytes).
void GlowFan(const long* point, U size, U centre, U rim, unsigned parity) {
    SH_CALL(EffectGte_LoadMapCamera)();
    float p[3];
    SH_CALL(EffectGte_ProjectPoint)(point, p);
    short v[4] = {static_cast<short>(size), 0, 0, 0};
    SH_CALL(EffectGte_ProjectSize)(point, v, v + 2);
    const auto radius = static_cast<int>(static_cast<short>((Frame_Counter & 1u) << parity) + v[2]);
    const U r = static_cast<U>(static_cast<int>(static_cast<short>(radius)));
    const auto rim_shade = static_cast<unsigned char>(rim);
    U a1 = 0, next = 0;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, &p[0], 4);
        next += 0x80;
        const U a2 = next & 0xFFFu;
        std::memcpy(prim + 0xC, &p[1], 4);
        StoreFloat(prim + 0x18, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a1)), r, 12)) + static_cast<double>(p[0]));
        StoreFloat(prim + 0x1C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a1)), r, 12)) + static_cast<double>(p[1]));
        StoreFloat(prim + 0x28, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a2)), r, 12)) + static_cast<double>(p[0]));
        StoreFloat(prim + 0x2C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a2)), r, 12)) + static_cast<double>(p[1]));
        std::memcpy(prim + 0x30, &p[2], 4);
        std::memcpy(prim + 0x20, &p[2], 4);
        std::memcpy(prim + 0x10, &p[2], 4);
        prim[4] = prim[5] = prim[6] = static_cast<unsigned char>(centre);
        prim[0x14] = prim[0x15] = prim[0x16] = rim_shade;
        prim[0x24] = prim[0x25] = prim[0x26] = rim_shade;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
        a1 += 0x80;
    } while ((next & 0xFFFFu) < 0x1000u);
}

}  // namespace

// ===========================================================================
// Kind 0x2F: Effect_KindHandlers[0x2F], EffectKind2F_States by +1
// ===========================================================================

// ===========================================================================
// Kind 0x33: Effect_KindHandlers[0x33], EffectKind33_States by +1
// ===========================================================================

// ===========================================================================
// Kind 0x35: Effect_KindHandlers[0x35], EffectKind35_States by +1 - kind
// 0x1E's shards and pieces (effect_1c.cpp) on Sprite_ObjectsExtra[1]'s model
// ===========================================================================

// ===========================================================================
// Kind 0x38: Effect_KindHandlers[0x38], EffectKind38_States by +1
// ===========================================================================

// ===========================================================================
// Kind 0x39: Effect_KindHandlers[0x39], EffectKind39_States by +1
// ===========================================================================

// ===========================================================================
// Kind 0x3B: Effect_KindHandlers[0x3B], EffectKind3B_States by +1
// ===========================================================================

// ===========================================================================
// Kind 0x3D: Effect_KindHandlers[0x3D], EffectKind3D_States by +1
// ===========================================================================

void Effect2B_Inject() {
    if (bof3::WantsShadow("effect_2b")) effect_2b::SelfTest();
}
