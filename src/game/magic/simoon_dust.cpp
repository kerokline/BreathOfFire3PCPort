// Moved here by tools/move_funcs.py from magic_s23.cpp (docs/source-layout-round.md).
#include "game/magic/simoon_dust.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s23.h"
#include "game/magic_s23_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_s23::at;

}  // namespace

namespace {

namespace hat = magic_harness::at;

using magic_harness::Handler;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

void Bump(unsigned char& b, int by = 1) { b = static_cast<unsigned char>(b + by); }

short Get(std::uint32_t address) { return static_cast<short>(Word(Mem(address))); }

void Put(std::uint32_t address, int v) { SetWord(Mem(address), static_cast<unsigned>(v)); }

std::uint8_t Byte(std::uint32_t address) { return Mem(address)[0]; }

// 32-bit imul and shl, wrapping as the original's do.
std::int32_t Imul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

std::int32_t Shl(std::int32_t a, int n) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << n); }

void SetPointerAt(unsigned char* cell, const void* p) {
    SetLong(cell, static_cast<std::int32_t>(Low32(p)));
}

unsigned char* Slot(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + index * hat::kTaskStride); }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

// sin / cos of the angle times the s16 at `radius` (read after the call), >> 12.
int SinOf(int angle, std::uint32_t radius) { const int s = Sin(angle); return Imul(s, Get(radius)) >> 12; }

int CosOf(int angle, std::uint32_t radius) { const int c = Cos(angle); return Imul(c, Get(radius)) >> 12; }

const short* Vertex(std::uint32_t address) { return reinterpret_cast<const short*>(Mem(address)); }

float* At(unsigned char* prim, unsigned offset) { return reinterpret_cast<float*>(prim + offset); }

// The four vertices at Prim_VertexScratch projected into the primitive's
// screen points; the depth answer and the flag word are the caller's locals.
void Project4(unsigned char* prim, unsigned a, unsigned b, unsigned c, unsigned d) {
    long depth = 0;
    MH_CALL(Gte_RotTransPers4)(Vertex(::bof3::Img(Prim_VertexScratch_addr)), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18), At(prim, a), At(prim, b),
                               At(prim, c), At(prim, d), &depth);
}

void Project3(unsigned char* prim) {
    long depth = 0;
    MH_CALL(Gte_RotTransPers3)(Vertex(::bof3::Img(Prim_VertexScratch_addr)), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10), At(prim, 8), At(prim, 0x18), At(prim, 0x28),
                               &depth);
}

// Three bytes of a vertex's colour, from the low byte of a scratch word.
void Shade(unsigned char* prim, unsigned offset, std::uint32_t word) {
    prim[offset] = Byte(word);
    prim[offset + 1] = Byte(word);
    prim[offset + 2] = Byte(word);
}

void Shade(unsigned char* prim, unsigned offset, unsigned char v) {
    prim[offset] = v;
    prim[offset + 1] = v;
    prim[offset + 2] = v;
}

// A phase through a stack table the original builds, by +1: aborts past it.
void Dispatch(const char* who, const std::uint32_t* table, unsigned n) {
    const unsigned phase = Sc()[1];
    if (phase >= n) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, n);
    magic_harness::Phase(table[phase])();
}

// A .data table of handlers, read in place, the index unchecked.
Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Four task slots of kind 1 made for Sprite_Current: the slot's owner +0x80,
// then the fields the caller sets (the original checks no 0xFF).
unsigned char* Child(unsigned parameter, unsigned char*& sc) {
    const unsigned slot = MH_CALL(BattleTask_Create)(1, parameter);
    sc = Sc();
    unsigned char* const t = Slot(slot);
    SetPointerAt(t + 0x80, sc);
    return t;
}

}  // namespace

#define S23_EXPORT extern "C" __attribute__((disable_tail_calls))

// The dust's arc: the screen y +0x30 less (+9 - 8) / 2.
void Arc() {
    unsigned char* const sc = Sc();
    rv::Store<unsigned>(rv::As<records::BattleTask>(sc)->screen_y, static_cast<unsigned>(static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_y) - (static_cast<int>(sc[9]) - 8) / 2));
}

// One corner of the dust's quad: sin / cos of the angle by the size, plus
// the screen point, as floats.
void DustCorner(unsigned char* prim, unsigned offset, unsigned angle_of_life) {
    const int x = SinOf(static_cast<int>(angle_of_life), ::bof3::Img(at::kR)) + Get(::bof3::Img(Scratch_Swap_addr));
    const float fx = static_cast<float>(x);
    std::memcpy(prim + offset, &fx, sizeof fx);
}

// original 0x4CF5F0: the kind-2 task (Magic_Rows row 57). Its phase +1
// through a three-entry stack table - Simoon_Start, Simoon_Wait, 0x43FE80
// (group E's: the done flag and free) - unchecked in the original, ours
// aborts past it.
S23_EXPORT void __cdecl Simoon_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Simoon_Start, bof3::addr::Simoon_Wait,
                                                 bof3::addr::MagicFx_DoneAndFree};
    Dispatch("Simoon_Task", kPhases, 3);
}

// original 0x4CF620: four effects of kind 1, 0x2D, owned by the task and
// counted in its +0xB: a fan (+1 = 2), two dust sprites (+1 = 1, +0xB their
// index, +9 its delay i + 1) and a dome (+1 = 0); then the CLUT strip's two
// rows at 0x812980 made semi-transparent - cells 1..15 of each copied from
// 0x4000 below with bit 15, cell 0 cleared - and marked dirty; sound 0x100;
// on.
S23_EXPORT void __cdecl Simoon_Start(void) {
    {
        unsigned char* sc;
        unsigned char* const t = Child(0x2D, sc);
        t[1] = 2;
        sc[0xB] = 1;
    }
    for (unsigned i = 0; i < 2; ++i) {
        unsigned char* sc;
        unsigned char* const t = Child(0x2D, sc);
        t[1] = 1;
        t[0xB] = static_cast<unsigned char>(i);
        t[9] = static_cast<unsigned char>(i + 1);
        Bump(sc[0xB]);
    }
    {
        unsigned char* sc;
        unsigned char* const t = Child(0x2D, sc);
        t[1] = 0;
        Bump(sc[0xB]);
    }
    for (std::uint32_t a = ::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + 2; a < (::bof3::Img(Gfx_ClutStrip_addr) + 0x3400) + 0x20; a += 2) {
        SetWord(Mem(a), Word(Mem(a - 0x4000)) | 0x8000u);
        SetWord(Mem(a + 0x20), Word(Mem(a + 0x20 - 0x4000)) | 0x8000u);
    }
    SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400), 0);
    SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + 0x20), 0);
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    Bump(Sc()[1]);
}

// original 0x4CF740: once the four are gone (+0xB 0), the target's flag
// 0x40, on.
S23_EXPORT void __cdecl Simoon_Wait(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(hat::kTarget))[0]);
    Bump(Sc()[1]);
}

// original 0x4CF770: the effect (kind 1, 0x2D): jmp [SimoonFx_Types + 4 *
// +1], read in place, unchecked.
S23_EXPORT void __cdecl SimoonFx_Dispatch(void) { Entry(::bof3::Img(SimoonFx_Types_addr), Sc()[1])(); }

// original 0x4CF790: SimoonFx_Types[0]. Its phase +2 through
// SimoonDome_Phases (read in place, unchecked); alive and past phase 0, the
// dome under the actor's matrix.
S23_EXPORT void __cdecl SimoonDome_Task(void) {
    Entry(::bof3::Img(SimoonDome_Phases_addr), Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(SimoonDome_Draw)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4CF7D0: SimoonDome_Phases[0]. The source's x / z and height;
// +9 and +0xA 0; on.
S23_EXPORT void __cdecl SimoonDome_Start(void) {
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::As<records::SpriteHead>(src)->height_whole);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Bump(Sc()[2]);
}

// original 0x4CF820: SimoonDome_Phases[1]. At +9 = 4 the target's flags 0x10;
// the size +9 * 28; +9 up, at 8 on.
S23_EXPORT void __cdecl SimoonDome_Grow(void) {
    unsigned char* sc = Sc();
    if (sc[9] == 4) {
        MH_CALL(Battle_SetTargetFlags)(Mem(::bof3::Img(hat::kTarget))[0], 0x10);
        sc = Sc();
    }
    Put(::bof3::Img(at::kR), sc[9] * 28);
    Bump(sc[9]);
    if (Sc()[9] == 8) Bump(Sc()[2]);
}

// original 0x4CF870: SimoonDome_Phases[2]. The size 0xC0 + +9 * 4; +0xA and
// +9 up; at +9 = 0x28 the owner's count +0xB down and the task freed.
S23_EXPORT void __cdecl SimoonDome_Expand(void) {
    unsigned char* const sc = Sc();
    Put(::bof3::Img(at::kR), sc[9] * 4 + 0xC0);
    Bump(sc[0xA]);
    Bump(Sc()[9]);
    if (Sc()[9] != 0x28) return;
    Bump(Owner()[0xB], -1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4CF8C0: the dome. Eight rings of 32 GT4 quads from latitude
// 0xC00 up in steps of 0x80 at the size 0x903850: each ring's two colours
// 0x81, or once +9 is past 0x10 plus the ring 0x81 - 8 d (1 for d of 0x11 or
// more); each quad a draw-mode primitive (0x35) and the quad, both linked at
// its outer corner's map position, textured from the page (0x340, 0x100)
// and the CLUT (0, 0x1FA), the ring's v rows 2 (0x14 + 0x18 - ring) + +0xA,
// times 4.
S23_EXPORT void __cdecl SimoonDome_Draw(void) {
    for (int ring = 1; ring <= 8; ++ring) {
        const int top = 0xC00 - 0x80 * (ring - 1);
        const int band = 0x18 - (ring - 1);
        {
            const unsigned char* const sc = Sc();
            Put(::bof3::Img(at::kW6), 0x81);
            Put(::bof3::Img(at::kW8), 0x81);
            if (ring - 1 < static_cast<int>(sc[9]) - 0x10) {
                const short d = static_cast<short>(sc[9] - ring - 0xF);
                Put(::bof3::Img(at::kW6), d < 0x11 ? 0x81 - (d << 3) : 1);
            }
            if (ring < static_cast<int>(sc[9]) - 0x10) {
                const short d = static_cast<short>(sc[9] - ring - 0x10);
                Put(::bof3::Img(at::kW8), d);
                Put(::bof3::Img(at::kW8), d < 0x11 ? 0x81 - (Get(::bof3::Img(at::kW8)) << 3) : 1);
            }
        }
        const int lower = top - 0x80;
        Put(::bof3::Img(at::kR2), CosOf(lower, ::bof3::Img(at::kR)));
        Put(::bof3::Img(at::kW4), CosOf(top, ::bof3::Img(at::kR)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, SinOf(0, ::bof3::Img(at::kR2)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, CosOf(0, ::bof3::Img(at::kR2)));
        {
            const int z = SinOf(lower, ::bof3::Img(at::kR));
            Put(::bof3::Img(Prim_VertexScratch_addr) + 4, z);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, z);
        }
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18, SinOf(0, ::bof3::Img(at::kW4)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, CosOf(0, ::bof3::Img(at::kW4)));
        {
            const int z = SinOf(top, ::bof3::Img(at::kR));
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, z);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, z);
        }
        const unsigned char v_low = static_cast<unsigned char>((band + 0x14) << 1);
        const unsigned char v_high = static_cast<unsigned char>((band + 0x15) << 1);
        for (int j = 1, a = 0x80; a < 0x1080; ++j, a += 0x80) {
            Put(::bof3::Img(Prim_VertexScratch_addr), Get(::bof3::Img(Prim_VertexScratch_addr) + 0x8));
            Put(::bof3::Img(Prim_VertexScratch_addr) + 2, Get(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2));
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, SinOf(a, ::bof3::Img(at::kR2)));
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, CosOf(a, ::bof3::Img(at::kR2)));
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Get(::bof3::Img(Prim_VertexScratch_addr) + 0x18));
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, Get(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2));
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18, SinOf(a, ::bof3::Img(at::kW4)));
            const int y = CosOf(a, ::bof3::Img(at::kW4));
            const unsigned char* const sc = Sc();
            const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, y);
            const std::int32_t x = Shl(Get(::bof3::Img(Prim_VertexScratch_addr) + 0x18), 9) + tsc->x;
            const std::int32_t z = Shl(static_cast<short>(y), 9) + tsc->z;
            MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 2, 0xC);
            unsigned char* const prim = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyGT4)(prim);
            MH_CALL(Gpu_SetSemiTrans)(prim, 1);
            Project4(prim, 8, 0x1C, 0x30, 0x44);
            MH_CALL(Gte_PrimDepths4_14)(prim);
            rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(prim)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
            rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(prim)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
            const unsigned char u0 = static_cast<unsigned char>(j << 3);
            const unsigned char u1 = static_cast<unsigned char>((j + 1) << 3);
            prim[0x14] = u0;
            prim[0x15] = static_cast<unsigned char>((Sc()[0xA] + v_low) << 2);
            prim[0x28] = u1;
            prim[0x29] = static_cast<unsigned char>((Sc()[0xA] + v_low) << 2);
            prim[0x3C] = u0;
            prim[0x50] = u1;
            prim[0x3D] = static_cast<unsigned char>((Sc()[0xA] + v_high) << 2);
            prim[0x51] = static_cast<unsigned char>((Sc()[0xA] + v_high) << 2);
            Shade(prim, 4, ::bof3::Img(at::kW8));
            Shade(prim, 0x18, ::bof3::Img(at::kW8));
            Shade(prim, 0x2C, ::bof3::Img(at::kW6));
            Shade(prim, 0x40, ::bof3::Img(at::kW6));
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 2, 0x54);
        }
    }
}

// original 0x4CFD20: SimoonFx_Types[1]. Its phase +2 through
// SimoonDust_Phases (read in place, unchecked); alive and past phase 0, the
// dust drawn (a tail jmp in the original).
S23_EXPORT void __cdecl SimoonDust_Task(void) {
    Entry(::bof3::Img(SimoonDust_Phases_addr), Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(SimoonDust_Draw)();
}

// original 0x4CFD50: SimoonDust_Phases[0]. +9 down; at 0 the source's
// position plus SimoonDust_Offsets[+0xB] (three dwords, +0xB unbounded);
// +9 and +0xA 0; on.
S23_EXPORT void __cdecl SimoonDust_Wait(void) {
    Bump(Sc()[9], -1);
    if (Sc()[9] != 0) return;
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    for (unsigned k = 0; k < 3; ++k) {
        unsigned char* const sc = Sc();
        const std::int32_t base = Long(src + 0x34 + 4 * k);
        SetLong(sc + 0x34 + 4 * k, Long(Mem(::bof3::Img(SimoonDust_Offsets_addr) + 4 * k + 12u * sc[0xB])) + base);
    }
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Bump(Sc()[2]);
}

// original 0x4CFDE0: SimoonDust_Phases[1]. The screen point; +0xA up 2, +9
// up, at 8 on.
S23_EXPORT void __cdecl SimoonDust_Rise(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    Bump(Sc()[0xA], 2);
    Bump(Sc()[9]);
    if (Sc()[9] == 8) Bump(Sc()[2]);
}

// original 0x4CFE10: SimoonDust_Phases[2]. The screen point; the arc; +9 up,
// at 0x1C on.
S23_EXPORT void __cdecl SimoonDust_Arc(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    Arc();
    Bump(Sc()[9]);
    if (Sc()[9] == 0x1C) Bump(Sc()[2]);
}

// original 0x4CFE50: SimoonDust_Phases[3]. The screen point; the arc; +9 up
// to 0x2C; +0xA down, at 0 the owner's count +0xB down and the task freed.
S23_EXPORT void __cdecl SimoonDust_Fall(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    Arc();
    unsigned char* sc = Sc();
    if (sc[9] != 0x2C) {
        Bump(sc[9]);
        sc = Sc();
    }
    Bump(sc[0xA], -1);
    if (Sc()[0xA] != 0) return;
    Bump(Owner()[0xB], -1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4CFEB0: the dust. A draw-mode primitive (0x35) linked at the
// position (row offset 0); a GT4 in screen space: its size +9 * 2 after
// phase 1, (+9 - 8) / 4 + 0x10 before; its colour (+0xA * 8, * 7, * 4); its
// corners at the screen point +0x2E / +0x30 turned by +9 (the four angles
// (+9 - 0x80), (+9 + 0x40), (+9 - 0x40) as bytes, and +9, times 16); the page
// (0x340, 0x100), the CLUT (0x10, 0x1FA), u / v 1..0x1F; linked with 0x54
// bytes.
S23_EXPORT void __cdecl SimoonDust_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    {
        const unsigned char* const sc = Sc();
        const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
        MH_CALL(MapView_LinkPrimAt)(tsc->x, tsc->z, 0, 0xC);
    }
    unsigned char* const prim = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyGT4)(prim);
    MH_CALL(Gpu_SetSemiTrans)(prim, 1);
    {
        const unsigned char* const sc = Sc();
        Put(::bof3::Img(at::kR), sc[2] > 1 ? sc[9] * 2 : (static_cast<int>(sc[9]) - 8) / 4 + 0x10);
        Put(::bof3::Img(at::kW6), sc[0xA] << 3);
        Put(::bof3::Img(at::kW8), sc[0xA] * 7);
        Put(::bof3::Img(at::kWA), sc[0xA] << 2);
        Put(::bof3::Img(Scratch_Swap_addr), static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_x));
        Put(::bof3::Img(at::kWE), static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_y));
    }
    static constexpr unsigned kX[4] = {8, 0x1C, 0x30, 0x44};
    static constexpr int kTurn[4] = {-0x80, 0x40, -0x40, 0};
    for (unsigned k = 0; k < 4; ++k) {
        const unsigned angle = k < 3 ? ((Sc()[9] + kTurn[k]) & 0xFF) << 4 : static_cast<unsigned>(Sc()[9]) << 4;
        DustCorner(prim, kX[k], angle);
        const unsigned again = k < 3 ? ((Sc()[9] + kTurn[k]) & 0xFF) << 4 : static_cast<unsigned>(Sc()[9]) << 4;
        const int y = CosOf(static_cast<int>(again), ::bof3::Img(at::kR)) + Get(::bof3::Img(at::kWE));
        const float fy = static_cast<float>(y);
        std::memcpy(prim + kX[k] + 4, &fy, sizeof fy);
    }
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(prim)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(prim)->clut, MH_CALL(Gpu_GetClut)(0x10, 0x1FA));
    prim[0x14] = 1;
    prim[0x15] = 1;
    prim[0x28] = 0x1F;
    prim[0x29] = 1;
    prim[0x3C] = 1;
    prim[0x3D] = 0x1F;
    prim[0x50] = 0x1F;
    prim[0x51] = 0x1F;
    for (unsigned k = 0; k < 4; ++k) {
        prim[4 + 0x14 * k] = Byte(::bof3::Img(at::kW6));
        prim[5 + 0x14 * k] = Byte(::bof3::Img(at::kW8));
        prim[6 + 0x14 * k] = Byte(::bof3::Img(at::kWA));
    }
    const unsigned char* const sc = Sc();
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(tsc->x, tsc->z, 0, 0x54);
}

// original 0x4D0200: SimoonFx_Types[2]. Its phase +2 through
// SimoonFan_Phases (read in place, unchecked; entries 2 and 3 are MAGIC056's
// 0x4AE0D0 / 0x4AE0F0); alive and past phase 0, the fan under the actor's
// matrix.
S23_EXPORT void __cdecl SimoonFan_Task(void) {
    Entry(::bof3::Img(SimoonFan_Phases_addr), Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(SimoonFan_Draw)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D0240: SimoonFan_Phases[0]. The source's position; +9 and
// +0xA 0; on.
S23_EXPORT void __cdecl SimoonFan_Start(void) {
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Bump(Sc()[2]);
}

// original 0x4D0290: SimoonFan_Phases[1]. +0xA up 2, +9 up, at 8 on.
S23_EXPORT void __cdecl SimoonFan_Grow(void) {
    Bump(Sc()[0xA], 2);
    Bump(Sc()[9]);
    if (Sc()[9] == 8) Bump(Sc()[2]);
}

// original 0x4D02C0: the fan. A draw-mode primitive (0x55) committed to slot
// 5; the radius cos(0x800) * (+9 + 0x58) / 1024 after phase 1, cos(0x800) *
// +9 * 3 / 256 before; colours +0xA * 6 (centre red) and +0xA * 8; sixteen G3
// triangles from the centre, 0x100 apart, the outer corners black; committed
// with 0x34 bytes.
S23_EXPORT void __cdecl SimoonFan_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    const unsigned char* sc = Sc();
    if (sc[2] > 1) {
        const int c = Cos(0x800);
        sc = Sc();
        Put(::bof3::Img(at::kR), Shl(Imul(c, sc[9] + 0x58), 2) >> 12);
    } else {
        const int c = Cos(0x800);
        sc = Sc();
        Put(::bof3::Img(at::kR), Shl(Imul(Imul(c, sc[9]), 3), 4) >> 12);
    }
    Put(::bof3::Img(at::kW6), sc[0xA] << 3);
    Put(::bof3::Img(at::kW8), sc[0xA] * 6);
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, SinOf(0, ::bof3::Img(at::kR)));
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, CosOf(0, ::bof3::Img(at::kR)));
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
    Put(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
    for (int angle = 0x100; angle < 0x1100; angle += 0x100) {
        unsigned char* const prim = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        const short x = Get(::bof3::Img(Prim_VertexScratch_addr) + 0x10), y = Get(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2);
        Put(::bof3::Img(Prim_VertexScratch_addr), 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 2, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, x);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, y);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, SinOf(angle, ::bof3::Img(at::kR)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, CosOf(angle, ::bof3::Img(at::kR)));
        prim[4] = Byte(::bof3::Img(at::kW8));
        prim[5] = Byte(::bof3::Img(at::kW6));
        prim[6] = Byte(::bof3::Img(at::kW6));
        Shade(prim, 0x14, static_cast<unsigned char>(0));
        Shade(prim, 0x24, static_cast<unsigned char>(0));
        Project3(prim);
        MH_CALL(Gte_PrimDepths3_10B)(prim);
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
}

void SimoonDust_Inject() {
    BOF3_INJECT(Simoon_Task);
    BOF3_INJECT(Simoon_Start);
    BOF3_INJECT(Simoon_Wait);
    BOF3_INJECT(SimoonFx_Dispatch);
    BOF3_INJECT(SimoonDome_Task);
    BOF3_INJECT(SimoonDome_Start);
    BOF3_INJECT(SimoonDome_Grow);
    BOF3_INJECT(SimoonDome_Expand);
    BOF3_INJECT(SimoonDome_Draw);
    BOF3_INJECT(SimoonDust_Task);
    BOF3_INJECT(SimoonDust_Wait);
    BOF3_INJECT(SimoonDust_Rise);
    BOF3_INJECT(SimoonDust_Arc);
    BOF3_INJECT(SimoonDust_Fall);
    BOF3_INJECT(SimoonDust_Draw);
    BOF3_INJECT(SimoonFan_Task);
    BOF3_INJECT(SimoonFan_Start);
    BOF3_INJECT(SimoonFan_Grow);
    BOF3_INJECT(SimoonFan_Draw);
}
