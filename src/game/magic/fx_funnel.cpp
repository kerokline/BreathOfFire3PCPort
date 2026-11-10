// Moved here by tools/move_funcs.py from magic_s23.cpp (docs/source-layout-round.md).
#include "game/magic/fx_funnel.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
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

using magic_harness::EnemyOf;

using magic_harness::Handler;

using magic_harness::Mem;

using magic_harness::PartyOf;

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

// The actor's matrix for an effect that carries its own: the SVECTOR
// `vector` through Gte_RotTrans into a MATRIX's translation, the rotation of
// `angles` into it, composed with the camera's, and loaded - as
// MagicFx_PushActorMatrix does after its own Gte_PushMatrix.
void LoadMatrix(const short* angles, const short* vector) {
    alignas(4) unsigned char m[0x20];
    MH_CALL(Gte_RotTrans)(vector, reinterpret_cast<long*>(m + 0x14));
    MH_CALL(Gte_RotMatrix)(angles, reinterpret_cast<short*>(m));
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<const short*>(m), reinterpret_cast<short*>(m));
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(m));
}

// The translation the three matrices share: the task's position in map units
// (+0x34 / +0x38 sar 9, less 0x4000) and minus half its height.
void Position(const unsigned char* sc, int height, short* v) {
    v[0] = static_cast<short>((rv::As<records::SpriteHead>(sc)->x >> 9) - 0x4000);
    v[1] = static_cast<short>((rv::As<records::SpriteHead>(sc)->z >> 9) - 0x4000);
    v[2] = static_cast<short>(-(height / 2));
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

// The target's record: a party member below 3, else an enemy (unchecked).
unsigned char* TargetRecord() {
    const unsigned char target = Mem(::bof3::Img(hat::kTarget))[0];
    return target < 3 ? PartyOf(target) : EnemyOf(target);
}

long Ground(const unsigned char* at_position) {
    return MH_CALL(AreaMap_Elevation)(rv::As<records::SpriteHead>(at_position)->x, rv::As<records::SpriteHead>(at_position)->z);
}

}  // namespace

#define S23_EXPORT extern "C" __attribute__((disable_tail_calls))

// The funnel's spin: +0x20 up 0x200 for an odd +0xB, down for an even, in
// 0..0xFFF.
void Spin(unsigned char* sc) {
    const std::int32_t a = rv::As<records::BattleTask>(sc)->work_20;
    rv::Store(rv::As<records::BattleTask>(sc)->work_20, ((sc[0xB] & 1) ? a + 0x200 : a - 0x200) & 0xFFF);
}

// original 0x4CC970: the kind-2 task (Magic_Rows row 13). Its phase +1
// through a three-entry stack table: Cyclone_Start, Cyclone_Wait,
// BattleFx_Finish. Unchecked in the original; ours aborts past it.
S23_EXPORT void __cdecl Cyclone_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Cyclone_Start, bof3::addr::Cyclone_Wait,
                                                 bof3::addr::BattleFx_Finish};
    Dispatch("Cyclone_Task", kPhases, 3);
}

// original 0x4CC9A0: the owner's actor byte and x / z, the source's +0x3C;
// +9 and +0xB 0; on; eight funnels (kind 1, 0x0A) owned by the task, each
// +0xB its index and +9 its delay 2i + 1, counted in the task's +0xB; sound
// 0x100.
S23_EXPORT void __cdecl Cyclone_Start(void) {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)))->height);
    Sc()[9] = 0;
    Sc()[0xB] = 0;
    Bump(Sc()[1]);
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* sc;
        unsigned char* const t = Child(0xA, sc);
        t[1] = 0;
        t[0xB] = static_cast<unsigned char>(i);
        t[9] = static_cast<unsigned char>(2 * i + 1);
        Bump(sc[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4CCA70: once a funnel has counted +9 up, the target's flags
// 0x10, on.
S23_EXPORT void __cdecl Cyclone_Wait(void) {
    if (Sc()[9] == 0) return;
    MH_CALL(Battle_SetTargetFlags)(Mem(::bof3::Img(hat::kTarget))[0], 0x10);
    Bump(Sc()[1]);
}

// original 0x4CCAA0: the funnel (kind 1, 0x0A): jmp [FxFunnel_Types + 4 * +1],
// read in place, unchecked.
S23_EXPORT void __cdecl FxFunnel_Dispatch(void) { Entry(::bof3::Img(FxFunnel_Types_addr), Sc()[1])(); }

// original 0x4CCAC0: FxFunnel_Types[0]. Its phase +2 through FxFunnel_Phases
// (read in place, unchecked); then, alive and past phase 0, the funnel under
// its own matrix and the ground ring under a second.
S23_EXPORT void __cdecl FxFunnel_Task(void) {
    Entry(::bof3::Img(FxFunnel_Phases_addr), Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(FxFunnel_PushMatrix)();
    MH_CALL(FxFunnel_Draw)();
    MH_CALL(Gte_PopMatrix)();
    MH_CALL(FxFunnel_PushGroundMatrix)();
    MH_CALL(FxFunnel_DrawGround)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4CCB10: FxFunnel_Phases[0]. +9 down; at 0: the owner's actor
// byte; the offset (0x60000, 0) turned by 0x446770 by the facing and added to
// the owner's x / z - then the owner's whole position copied over it (the
// offset is lost, as in the original); the heading from the owner by
// Math_Ratan2, less 0x800; the drift +0x18 = -4 - (Rand & 3) for an odd
// +0xB, else 4 + (Rand & 3); the rise +0x1C = 0x50 + (Rand & 0x30); +0xA 0;
// on.
S23_EXPORT void __cdecl FxFunnel_Wait(void) {
    Bump(Sc()[9], -1);
    unsigned char* const sc = Sc();
    if (sc[9] != 0) return;
    sc[8] = Owner()[8];
    rv::Current<records::BattleTask>()->step_x = 0x60000;
    rv::Current<records::BattleTask>()->step_z = 0;
    MH_AT(magic_s23::TurnFn, magic_s23::kTurnByFacing)(Sc());
    {
        const unsigned char* const owner = Owner();
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = rv::As<records::SpriteHead>(owner)->x + ts->step_x;
    }
    {
        const unsigned char* const owner = Owner();
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->z = rv::As<records::SpriteHead>(owner)->z + ts->step_z;
    }
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    {
        const unsigned char* const owner = Owner();
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        const std::int32_t dz = rv::As<records::SpriteHead>(owner)->z - ts->z;
        const std::int32_t dx = rv::As<records::SpriteHead>(owner)->x - ts->x;
        const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
        rv::Store(rv::Current<records::BattleTask>()->work_20, (heading - 0x800) & 0xFFF);
    }
    if (Sc()[0xB] & 1) {
        const int r = MH_CALL(Rand)();
        rv::Store(rv::Current<records::BattleTask>()->work_18, -4 - (r & 3));
    } else {
        const int r = MH_CALL(Rand)();
        rv::Store(rv::Current<records::BattleTask>()->work_18, (r & 3) + 4);
    }
    const int r = MH_CALL(Rand)();
    rv::Store(rv::Current<records::BattleTask>()->work_1c, (r & 0x30) + 0x50);
    Sc()[0xA] = 0;
    Bump(Sc()[2]);
}

// original 0x4CCC90: FxFunnel_Phases[1]. Spin; 0x4FBB40 moves the funnel
// round the target's record by the drift (+0x18 << 6) at radius 0xC000 and
// answers the angle, kept in +0x14; +0xA up 4, at 0x10 on.
S23_EXPORT void __cdecl FxFunnel_Approach(void) {
    unsigned char* const record = TargetRecord();
    Spin(Sc());
    const int angle = MH_CALL(MagicFx_StepAround)(record, 0xC000, Shl(rv::Current<records::BattleTask>()->work_18, 6));
    rv::Store(rv::Current<records::BattleTask>()->step_y, angle);
    Bump(Sc()[0xA], 4);
    if (Sc()[0xA] == 0x10) Bump(Sc()[2]);
}

// original 0x4CCD30: FxFunnel_Phases[2]. Spin; the last angle to +0x10, the
// orbit as above to +0x14; when 0x4FBC30 says the funnel is within 0xC000 of
// the target, or the angle moved by more than 0x400 and less than 0xC00
// (|last & 0xFFF - new & 0xFFF|, kept in +0x10): the owner's +9 up and on.
S23_EXPORT void __cdecl FxFunnel_Orbit(void) {
    unsigned char* const record = TargetRecord();
    Spin(Sc());
    {
        unsigned char* const sc = Sc();
        records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
        tsc->step_z = tsc->step_y;
    }
    const int angle = MH_CALL(MagicFx_StepAround)(record, 0xC000, Shl(rv::Current<records::BattleTask>()->work_18, 6));
    rv::Store(rv::Current<records::BattleTask>()->step_y, angle);
    if (MH_CALL(MagicFx_NearSprite)(record, 0xC000) == 0) {
        {
            unsigned char* const sc = Sc();
            records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
            tsc->step_z = (tsc->step_z & 0xFFF) - (tsc->step_y & 0xFFF);
        }
        unsigned char* sc = Sc();
        const std::int32_t d = rv::As<records::BattleTask>(sc)->step_z;
        if (d < 0) {
            rv::Store(rv::As<records::BattleTask>(sc)->step_z, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(d)));
            sc = Sc();
        }
        const std::int32_t moved = rv::As<records::BattleTask>(sc)->step_z;
        if (moved <= 0x400 || moved >= 0xC00) return;
    }
    Bump(Owner()[9]);
    Bump(Sc()[2]);
}

// original 0x4CCE40: FxFunnel_Phases[3]. The offset (0x1000, 0) turned by
// the facing (0x446770) and added to x / z; the height +0x3E up 0x40; spin;
// +0xA down, at 0 the owner's count +0xB down and the task freed.
S23_EXPORT void __cdecl FxFunnel_Rise(void) {
    rv::Current<records::BattleTask>()->step_x = 0x1000;
    rv::Current<records::BattleTask>()->step_z = 0;
    MH_AT(magic_s23::TurnFn, magic_s23::kTurnByFacing)(Sc());
    {
        unsigned char* const sc = Sc();
        records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
        tsc->x = tsc->x + tsc->step_x;
    }
    {
        unsigned char* const sc = Sc();
        records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
        tsc->z = tsc->z + tsc->step_z;
    }
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::Current<records::BattleTask>()->height_whole + 0x40u);
    Spin(Sc());
    Bump(Sc()[0xA], -1);
    if (Sc()[0xA] != 0) return;
    Bump(Owner()[0xB], -1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4CCEF0: the funnel's matrix: Gte_PushMatrix, then no rotation
// and the translation at the task's position, 0x100 below half its height.
S23_EXPORT void __cdecl FxFunnel_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[3] = {0, 0, 0};
    short v[3];
    Position(Sc(), static_cast<short>(rv::Current<records::BattleTask>()->height_whole) + 0x100, v);
    LoadMatrix(angles, v);
}

// original 0x4CCFA0: the funnel. A draw-mode primitive (0x35) linked at the
// task's position (row offset 2); the radius from +0x1C (inner) and inner +
// sin(0) * inner / 2 (outer, the same); +0xA * 12 and +0xA the two colours;
// the angle index +0x20 / 64; then 31 G4 quads round the circle, each from
// the last pair of points to the next - the outer radius swelling by
// sin(0x40 i) * inner / 2 - semi-transparent, projected, linked at the
// position with 0x44 bytes.
S23_EXPORT void __cdecl FxFunnel_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    {
        const unsigned char* const sc = Sc();
        const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
        MH_CALL(MapView_LinkPrimAt)(tsc->x, tsc->z, 2, 0xC);
    }
    {
        const unsigned char* const r = Sc() + 0x1C;
        const int s = Sin(0);
        Put(::bof3::Img(at::kR), (Imul(s, Long(r) / 2) >> 12) + Word(r));
        Put(::bof3::Img(at::kR2), Word(r));
    }
    {
        const unsigned char* const sc = Sc();
        Put(::bof3::Img(at::kW8), (rv::As<records::BattleTask>(sc)->work_20 / 64) & 0x3F);
        Put(::bof3::Img(at::kW4), sc[0xA] * 12);
        Put(::bof3::Img(at::kW6), sc[0xA]);
        const int first = (Byte(::bof3::Img(at::kW8)) & 0x3F) << 6;
        Put(::bof3::Img(at::kWA), first);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, SinOf(first, ::bof3::Img(at::kR)));
    }
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, CosOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR)));
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18, SinOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR2)));
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, CosOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR2)));
    for (int i = 1, angle = 0x40; angle < 0x800; ++i, angle += 0x40) {
        const int a = ((Byte(::bof3::Img(at::kW8)) + i) & 0x3F) << 6;
        const unsigned char* const r = Sc() + 0x1C;
        Put(::bof3::Img(at::kWA), a);
        const int s = Sin(angle);
        const int outer = (Imul(s, Long(r) / 2) >> 12) + Word(r);
        Put(::bof3::Img(Prim_VertexScratch_addr), Get(::bof3::Img(Prim_VertexScratch_addr) + 0x8));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 2, Get(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2));
        Put(::bof3::Img(at::kR), outer);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, SinOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, CosOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Get(::bof3::Img(Prim_VertexScratch_addr) + 0x18));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, Get(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18, SinOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR2)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, CosOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR2)));
        unsigned char* const prim = Gfx_PacketNext;
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, 0);
        MH_CALL(Gpu_SetPolyG4)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        Shade(prim, 4, ::bof3::Img(at::kW4));
        Shade(prim, 0x14, ::bof3::Img(at::kW4));
        Shade(prim, 0x24, ::bof3::Img(at::kW6));
        Shade(prim, 0x34, ::bof3::Img(at::kW6));
        Project4(prim, 8, 0x18, 0x28, 0x38);
        MH_CALL(Gte_PrimDepths4_10B)(prim);
        const unsigned char* const sc = Sc();
        const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
        MH_CALL(MapView_LinkPrimAt)(tsc->x, tsc->z, 2, 0x44);
    }
}

// original 0x4CD2D0: the ground ring's matrix: Gte_PushMatrix, no rotation,
// the translation at the task's x / z and half its height - from phase 3 on,
// half the ground's (AreaMap_Elevation) instead.
S23_EXPORT void __cdecl FxFunnel_PushGroundMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[3] = {0, 0, 0};
    const unsigned char* const sc = Sc();
    const int height = sc[2] < 3 ? static_cast<short>(rv::As<records::BattleTask>(sc)->height_whole) : static_cast<short>(Ground(sc));
    short v[3];
    Position(sc, height, v);
    LoadMatrix(angles, v);
}

// original 0x4CD390: the ground ring. A draw-mode primitive (0x55)
// committed to slot 5; inner radius +0x1C - 0x20, outer inner + sin(0) *
// inner / 2; colour +0xA * 2; 31 G4 quads as the funnel's, the outer radius
// swelling by sin(0x40 i) * inner / 2, the inner pair black-ish (1),
// committed with 0x44 bytes; a closing draw mode (0x15).
S23_EXPORT void __cdecl FxFunnel_DrawGround(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    Put(::bof3::Img(at::kR2), Word(Sc() + 0x1C) - 0x20);
    {
        const int s = Sin(0);
        const short inner = Get(::bof3::Img(at::kR2));
        Put(::bof3::Img(at::kR), (Imul(s, inner / 2) >> 12) + inner);
    }
    {
        const unsigned char* const sc = Sc();
        Put(::bof3::Img(at::kW8), (rv::As<records::BattleTask>(sc)->work_20 / 64) & 0x3F);
        Put(::bof3::Img(at::kW4), sc[0xA] * 2);
        const int first = (Byte(::bof3::Img(at::kW8)) & 0x3F) << 6;
        Put(::bof3::Img(at::kWA), first);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, SinOf(first, ::bof3::Img(at::kR)));
    }
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, CosOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR)));
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18, SinOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR2)));
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, CosOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR2)));
    for (int i = 1, angle = 0x40; angle < 0x800; ++i, angle += 0x40) {
        Put(::bof3::Img(at::kWA), ((Byte(::bof3::Img(at::kW8)) + i) & 0x3F) << 6);
        const int s = Sin(angle);
        const short inner = Get(::bof3::Img(at::kR2));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
        const int outer = (Imul(s, inner / 2) >> 12) + inner;
        Put(::bof3::Img(Prim_VertexScratch_addr), Get(::bof3::Img(Prim_VertexScratch_addr) + 0x8));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 2, Get(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2));
        Put(::bof3::Img(at::kR), outer);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, SinOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, CosOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Get(::bof3::Img(Prim_VertexScratch_addr) + 0x18));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, Get(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18, SinOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR2)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, CosOf(Get(::bof3::Img(at::kWA)), ::bof3::Img(at::kR2)));
        unsigned char* const prim = Gfx_PacketNext;
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, 0);
        MH_CALL(Gpu_SetPolyG4)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        Shade(prim, 4, ::bof3::Img(at::kW4));
        Shade(prim, 0x14, ::bof3::Img(at::kW4));
        Shade(prim, 0x24, static_cast<unsigned char>(1));
        Shade(prim, 0x34, static_cast<unsigned char>(1));
        Project4(prim, 8, 0x18, 0x28, 0x38);
        MH_CALL(Gte_PrimDepths4_10B)(prim);
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

void FxFunnel_Inject() {
    BOF3_INJECT(Cyclone_Task);
    BOF3_INJECT(Cyclone_Start);
    BOF3_INJECT(Cyclone_Wait);
    BOF3_INJECT(FxFunnel_Dispatch);
    BOF3_INJECT(FxFunnel_Task);
    BOF3_INJECT(FxFunnel_Wait);
    BOF3_INJECT(FxFunnel_Approach);
    BOF3_INJECT(FxFunnel_Orbit);
    BOF3_INJECT(FxFunnel_Rise);
    BOF3_INJECT(FxFunnel_PushMatrix);
    BOF3_INJECT(FxFunnel_Draw);
    BOF3_INJECT(FxFunnel_PushGroundMatrix);
    BOF3_INJECT(FxFunnel_DrawGround);
}
