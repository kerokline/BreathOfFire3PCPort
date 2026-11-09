// Moved here by tools/move_funcs.py from magic_s19.cpp (docs/source-layout-round.md).
#include "game/magic/barrier_ring.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s19.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

#define MS19_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The GTE's input vectors (four SVECTORs) and the effects' scratch words.
// kV0: now Prim_VertexScratch (symbols.toml).
// kV1: now Prim_VertexScratch[1] (symbols.toml).
// kV2: now Prim_VertexScratch[2] (symbols.toml).
// kV3: now Prim_VertexScratch[3] (symbols.toml).
constexpr ::bof3::image::Va kRadius{SharedScratch_addr}; // s16 (dword for MAGIC086's draws)

constexpr ::bof3::image::Va kAngle{0x903854};    // s16 (dword: MAGIC086's inner radius)

using Handler = void (__cdecl*)();

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

// A .data handler table's cell, read in place.
Handler Cell(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

void Put16(std::uint32_t a, std::uint32_t v) { SetWord(Mem(a), v & 0xFFFF); }

// imul then sar, as the original's 32-bit registers wrap.
std::int32_t MulSar(std::int32_t a, std::int32_t b, int n) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}

std::int32_t ShlSar(std::int32_t a, int l, int r) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << l) >> r; }

std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

const short* Vec(std::uint32_t a) { return reinterpret_cast<const short*>(Mem(a)); }

float* At(unsigned char* prim, unsigned off) { return reinterpret_cast<float*>(prim + off); }

unsigned char* Prim() { return Gfx_PacketNext; }

void DrawModeAndCommit(unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Prim(), 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// The sin / cos helpers at radius r: (f(angle) * r) >> 12, as a word.
std::int32_t Sin(std::int32_t angle) { return MH_CALL(Math_Sin)(angle); }

std::int32_t Cos(std::int32_t angle) { return MH_CALL(Math_Cos)(angle); }

}  // namespace

// original 0x4C2CE0: BarrierPart_Types[1], the ring. Its phase +2 through
// BarrierRing_Phases; while live, the ring under the actor's matrix.
MS19_EXPORT void __cdecl BarrierRing_Task(void) {
    Cell(::bof3::Img(BarrierRing_Phases_addr), Sprite_Current[2])();
    if (Sprite_Current[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(BarrierRing_Draw)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C2D10: BarrierRing_Phases[0] (a body 23 files' tables share). +9
// up; the phase on at 0x10.
MS19_EXPORT void __cdecl BarrierRing_Grow(void) {
    Bump(Sprite_Current[9]);
    unsigned char* const cur = Sprite_Current;
    if (cur[9] == 0x10) Bump(cur[2]);
}

// original 0x4C2D30: BarrierRing_Phases[1]. +0xA (its lift) and +9 up; at +9
// 0x18 the owner's +0xB 1 (the tint may start) and the phase on.
MS19_EXPORT void __cdecl BarrierRing_Rise(void) {
    Bump(Sprite_Current[0xA]);
    Bump(Sprite_Current[9]);
    if (Sprite_Current[9] != 0x18) return;
    Owner()[0xB] = 1;
    Bump(Sprite_Current[2]);
}

// original 0x4C2D70: BarrierRing_Phases[2]. +0xA up; the phase on at 0x16.
MS19_EXPORT void __cdecl BarrierRing_Lift(void) {
    Bump(Sprite_Current[0xA]);
    unsigned char* const cur = Sprite_Current;
    if (cur[0xA] == 0x16) Bump(cur[2]);
}

// original 0x4C2D90: BarrierRing_Phases[3]. +0xB (0x30 at the start) down; the
// phase on at 0.
MS19_EXPORT void __cdecl BarrierRing_Hold(void) {
    Drop(Sprite_Current[0xB]);
    unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 0) Bump(cur[2]);
}

// original 0x4C2DB0: BarrierRing_Phases[4]. +9 and +0xA down; at +9 0x10 the
// owner's +0xB 2 (the fade may start) and the phase on.
MS19_EXPORT void __cdecl BarrierRing_Settle(void) {
    Drop(Sprite_Current[9]);
    Drop(Sprite_Current[0xA]);
    if (Sprite_Current[9] != 0x10) return;
    Owner()[0xB] = 2;
    Bump(Sprite_Current[2]);
}

// original 0x4C2DF0: BarrierRing_Phases[5]. +9 down to 0 (held there); +0xA
// down, and at 0 the owner's +0xB 0xFF (the effect may end) and the ring
// freed (a tail jump).
MS19_EXPORT void __cdecl BarrierRing_End(void) {
    unsigned char* cur = Sprite_Current;
    if (cur[9] != 0) {
        Drop(cur[9]);
        cur = Sprite_Current;
    }
    Drop(cur[0xA]);
    if (Sprite_Current[0xA] != 0) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C2E30: the ring - 64 POLY_G4s round the origin, the angle by
// 0x40. The dword 0x903850 the outer radius (0xC0 once past the first phase,
// else +9 * 12), 0x903854 the inner (+9 * 14), 0x90385C the lift (+0xA << 4).
// Two of every eight segments ((Frame_Counter & 7) and the next, read once)
// and every segment of the first phase are flat on the ground, from the inner
// radius to the outer, shaded by +9; the other six stand up as a wall of the
// outer radius, their tops at -(lift + (Math_Sin(((Frame_Counter + i) & 0x1F)
// << 7) << 5 >> 12)) - a wave - coloured (1, 1, 1) to (0x20, 0xC0, 0x20) and
// linked at the map cell of their top's first point (x, z << 9 plus the
// ring's), where the flat ones go to OT slot 5.
MS19_EXPORT void __cdecl BarrierRing_Draw(void) {
    DrawModeAndCommit(0x35);
    const unsigned char* const cur = Sprite_Current;
    SetLong(Mem(::bof3::Img(kRadius.v)), cur[2] != 0 ? 0xC0 : static_cast<std::int32_t>(cur[9] * 12u));
    SetLong(Mem(::bof3::Img(kAngle.v)), static_cast<std::int32_t>(cur[9] * 14u));
    SetLong(Mem(::bof3::Img(Scratch_Swap_addr)), static_cast<std::int32_t>(static_cast<std::uint32_t>(cur[0xA]) << 4));
    const std::uint32_t frame = Frame_Counter;
    const std::uint32_t gap0 = frame & 7, gap1 = (frame + 1) & 7;
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18, static_cast<std::uint32_t>(MulSar(Cos(0), Long(Mem(::bof3::Img(kRadius.v))), 12)));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, static_cast<std::uint32_t>(MulSar(Sin(0), Long(Mem(::bof3::Img(kRadius.v))), 12)));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, 0);
    // The wall's link point: the original keeps it in registers across
    // segments, and before the first wall segment they hold an uninitialised
    // stack cell - reached only if Sprite_Current[2] changed between the two
    // tests of one segment, which nothing it calls does. Ours aborts there.
    bool placed = false;
    std::int32_t x = 0, z = 0;
    std::uint32_t i = 1;
    for (std::int32_t a = 0x40; a < 0x1040; a += 0x40, ++i) {
        const std::uint32_t seg = i & 7;
        if (seg != gap0 && seg != gap1 && Sprite_Current[2] != 0) {
            const std::int32_t b = a - 0x40;
            const std::uint32_t wave = ((Frame_Counter + i) & 0x1F) << 7;
            Put16(::bof3::Img(Prim_VertexScratch_addr), static_cast<std::uint32_t>(MulSar(Cos(b), Long(Mem(::bof3::Img(kRadius.v))), 12)));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, static_cast<std::uint32_t>(MulSar(Sin(b), Long(Mem(::bof3::Img(kRadius.v))), 12)));
            const std::int32_t w0 = Sin(static_cast<std::int32_t>(wave));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, static_cast<std::uint32_t>(-Add(ShlSar(w0, 5, 12), Long(Mem(::bof3::Img(Scratch_Swap_addr))))));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, static_cast<std::uint32_t>(MulSar(Cos(a), Long(Mem(::bof3::Img(kRadius.v))), 12)));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, static_cast<std::uint32_t>(MulSar(Sin(a), Long(Mem(::bof3::Img(kRadius.v))), 12)));
            const std::int32_t w1 = Sin(static_cast<std::int32_t>(wave));
            const std::uint16_t old_y = Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, old_y);
            const std::uint16_t y1 = Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, static_cast<std::uint32_t>(-Add(ShlSar(w1, 5, 12), Long(Mem(::bof3::Img(Scratch_Swap_addr))))));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x18)));
            const std::uint16_t x1 = Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x8));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18, x1);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, y1);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, 0);
            const unsigned char* const now = Sprite_Current;
            const records::BattleTask* const tnow = rv::AsConst<records::BattleTask>(now);
            x = Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(x1))) << 9),
                    tnow->x);
            z = Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(y1))) << 9),
                    tnow->z);
            placed = true;
            MH_CALL(Gpu_SetDrawMode)(Prim(), 0, 1, 0x35, 0);
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 0, 0xC);
        } else {
            const std::int32_t b = a - 0x40;
            Put16(::bof3::Img(Prim_VertexScratch_addr), static_cast<std::uint32_t>(MulSar(Cos(b), Long(Mem(::bof3::Img(kAngle.v))), 12)));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, static_cast<std::uint32_t>(MulSar(Sin(b), Long(Mem(::bof3::Img(kAngle.v))), 12)));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, static_cast<std::uint32_t>(MulSar(Cos(a), Long(Mem(::bof3::Img(kAngle.v))), 12)));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, static_cast<std::uint32_t>(MulSar(Sin(a), Long(Mem(::bof3::Img(kAngle.v))), 12)));
            const std::uint16_t old_y = Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x18)));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, old_y);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18, static_cast<std::uint32_t>(MulSar(Cos(a), Long(Mem(::bof3::Img(kRadius.v))), 12)));
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, 0);
            Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, static_cast<std::uint32_t>(MulSar(Sin(a), Long(Mem(::bof3::Img(kRadius.v))), 12)));
        }
        unsigned char* const prim = Prim();
        MH_CALL(Gpu_SetPolyG4)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        long depth;
        MH_CALL(Gte_RotTransPers4)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x18), At(prim, 8), At(prim, 0x18), At(prim, 0x28),
                                   At(prim, 0x38), &depth);
        MH_CALL(Gte_PrimDepths4_10B)(prim);
        if (seg != gap0 && seg != gap1 && Sprite_Current[2] != 0) {
            if (!placed) bof3::Fatal("BarrierRing_Draw: a wall segment linked before any was placed");
            prim[4] = 1;
            prim[5] = 1;
            prim[6] = 1;
            prim[0x14] = 1;
            prim[0x15] = 1;
            prim[0x16] = 1;
            prim[0x24] = 0x20;
            prim[0x25] = 0xC0;
            prim[0x26] = 0x20;
            prim[0x34] = 0x20;
            prim[0x35] = 0xC0;
            prim[0x36] = 0x20;
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 0, 0x44);
        } else {
            prim[4] = 1;
            prim[5] = 1;
            prim[6] = 1;
            prim[0x14] = 1;
            prim[0x15] = 1;
            prim[0x16] = 1;
            prim[0x24] = static_cast<unsigned char>(Sprite_Current[9] << 1);
            prim[0x25] = static_cast<unsigned char>(Sprite_Current[9] << 1);
            prim[0x26] = Sprite_Current[9];
            prim[0x34] = static_cast<unsigned char>(Sprite_Current[9] << 1);
            prim[0x35] = static_cast<unsigned char>(Sprite_Current[9] << 1);
            prim[0x36] = Sprite_Current[9];
            MH_CALL(Gfx_CommitPrim)(5, 0x44);
        }
    }
    DrawModeAndCommit(0x15);
}

void BarrierRing_Inject() {
    BOF3_INJECT(BarrierRing_Task);
    BOF3_INJECT(BarrierRing_Grow);
    BOF3_INJECT(BarrierRing_Rise);
    BOF3_INJECT(BarrierRing_Lift);
    BOF3_INJECT(BarrierRing_Hold);
    BOF3_INJECT(BarrierRing_Settle);
    BOF3_INJECT(BarrierRing_End);
    BOF3_INJECT(BarrierRing_Draw);
}
