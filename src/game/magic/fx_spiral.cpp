// Moved here by tools/move_funcs.py from magic_s23.cpp (docs/source-layout-round.md).
#include "game/magic/fx_spiral.h"

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

// The acting side's facing: the actor sprite's +8, turned by 2 when the
// target's bit 0x40 and the actor's side disagree (bit 0x40 and a party
// actor, or neither and an enemy).
unsigned char Facing() {
    const bool bit = (Mem(::bof3::Img(hat::kTarget))[0] & 0x40) != 0;
    const bool party = Mem(::bof3::Img(hat::kActor))[0] < 3;
    const unsigned char f = Pointer(::bof3::Img(MagicS23_ActorSprite_addr))[8];
    return bit == party ? static_cast<unsigned char>(f ^ 2) : f;
}

}  // namespace

#define S23_EXPORT extern "C" __attribute__((disable_tail_calls))

// The spiral's first two phases' common part: +0xB up 8, the height the
// owner's + 0x180 + sin((Frame_Counter & 0x1F) << 7) / 128.
void Bob() {
    Bump(Sc()[0xB], 8);
    const int s = Sin(static_cast<int>((Frame_Counter & 0x1F) << 7));
    const unsigned char* const owner = Owner();
    unsigned char* const sc = Sc();
    rv::Store<unsigned>(rv::As<records::BattleTask>(sc)->height_whole, static_cast<unsigned>((Shl(s, 5) >> 12) + rv::As<records::SpriteHead>(owner)->height_whole + 0x180));
}

// original 0x4CD6B0: the kind-2 task (Magic_Rows row 26). Its phase +1
// through a five-entry stack table - Typhoon_Start, Typhoon_Grow, SpellFx_Countdown
// (0x4DA3B0, MAGIC118's: group S27), Typhoon_Fade, BattleFx_Finish - unchecked in the
// original, ours aborts past it; then, while +0xA and +0 are not 0, the fan
// under the actor's matrix.
S23_EXPORT void __cdecl Typhoon_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Typhoon_Start, bof3::addr::Typhoon_Grow,
                                                 bof3::addr::SpellFx_Countdown, bof3::addr::Typhoon_Fade,
                                                 bof3::addr::BattleFx_Finish};
    Dispatch("Typhoon_Task", kPhases, 5);
    const unsigned char* const sc = Sc();
    if (sc[0xA] == 0 || sc[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(Typhoon_DrawFan)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4CD710: the facing to +8; the side's centre (0x4FC0E0); +0xB 0,
// +9 0x30, +0xA 0; on; sixteen spirals (kind 1, 0x1B) owned by the task,
// each +4 its index and +9 its delay (i / 4) * 12 + 1, counted in +0xB; sound
// 0x100.
S23_EXPORT void __cdecl Typhoon_Start(void) {
    const unsigned char facing = Facing();
    Sc()[8] = facing;
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[0xB] = 0;
    Sc()[9] = 0x30;
    Sc()[0xA] = 0;
    Bump(Sc()[1]);
    for (unsigned i = 0; i < 16; ++i) {
        unsigned char* sc;
        unsigned char* const t = Child(0x1B, sc);
        t[1] = 0;
        t[4] = static_cast<unsigned char>(i);
        t[9] = static_cast<unsigned char>((i >> 2) * 12 + 1);
        Bump(sc[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4CD7E0: +0xA up; at 0x10 the target's flags 0x10, on.
S23_EXPORT void __cdecl Typhoon_Grow(void) {
    Bump(Sc()[0xA]);
    if (Sc()[0xA] != 0x10) return;
    MH_CALL(Battle_SetTargetFlags)(Mem(::bof3::Img(hat::kTarget))[0], 0x10);
    Bump(Sc()[1]);
}

// original 0x4CD820: +0xA down; at 0 on.
S23_EXPORT void __cdecl Typhoon_Fade(void) {
    Bump(Sc()[0xA], -1);
    if (Sc()[0xA] == 0) Bump(Sc()[1]);
}

// original 0x4CD840: the fan. A draw-mode primitive (0x55) committed to slot
// 5; the radius 0x80 + sin((Frame_Counter & 0x1F) << 7) / 256; colour
// +0xA * 6; eight G3 triangles from the centre, 0x200 apart, the outer
// corners colour 1, committed with 0x34 bytes; a closing draw mode (0x15).
S23_EXPORT void __cdecl Typhoon_DrawFan(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const int s = Sin(static_cast<int>((Frame_Counter & 0x1F) << 7));
        const unsigned char* const sc = Sc();
        Put(::bof3::Img(at::kR), (Shl(s, 4) >> 12) + 0x80);
        Put(::bof3::Img(at::kW8), sc[0xA] * 6);
    }
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, SinOf(0, ::bof3::Img(at::kR)));
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, CosOf(0, ::bof3::Img(at::kR)));
    for (int angle = 0x200; angle < 0x1200; angle += 0x200) {
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
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
        Project3(prim);
        MH_CALL(Gte_PrimDepths3_10B)(prim);
        Shade(prim, 4, ::bof3::Img(at::kW8));
        Shade(prim, 0x14, static_cast<unsigned char>(1));
        Shade(prim, 0x24, static_cast<unsigned char>(1));
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4CDA00: the spiral (kind 1, 0x1B): jmp [FxSpiral_Types + 4 * +1],
// read in place, unchecked.
S23_EXPORT void __cdecl FxSpiral_Dispatch(void) { Entry(::bof3::Img(FxSpiral_Types_addr), Sc()[1])(); }

// original 0x4CDA20: FxSpiral_Types[0]. Its phase +2 through FxSpiral_Phases
// (read in place, unchecked); then, alive and past phase 0, the band under
// its own matrix.
S23_EXPORT void __cdecl FxSpiral_Task(void) {
    Entry(::bof3::Img(FxSpiral_Phases_addr), Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(FxSpiral_PushMatrix)();
    MH_CALL(FxSpiral_Draw)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4CDA60: FxSpiral_Phases[0]. +9 down; at 0: the owner's actor
// byte and position, 0x180 map units above it (+0x3C + 0x1800000); the
// angle index +0xB = (+4 << 2) & 0x3F; the tilt +0xC = FxSpiral_Turns[+4]
// << 8 (+4 unbounded); the radius +0x10 = 0x60, its scale +0x14 = 0x20; +9
// = (+4 << 2) + 1; +0xA 0; on.
S23_EXPORT void __cdecl FxSpiral_Wait(void) {
    Bump(Sc()[9], -1);
    unsigned char* const sc = Sc();
    if (sc[9] != 0) return;
    sc[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height + 0x1800000);
    {
        unsigned char* const s = Sc();
        s[0xB] = static_cast<unsigned char>((s[4] << 2) & 0x3F);
    }
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_x = static_cast<std::int32_t>(static_cast<std::uint32_t>(Word(Mem(::bof3::Img(FxSpiral_Turns_addr) + 2u * s[4]))) << 8);
    }
    rv::Current<records::BattleTask>()->step_z = 0x60;
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x20);
    {
        unsigned char* const s = Sc();
        s[9] = static_cast<unsigned char>((s[4] << 2) + 1);
    }
    Sc()[0xA] = 0;
    Bump(Sc()[2]);
}

// original 0x4CDB30: FxSpiral_Phases[1]. Bob; +0xA up 2, at 0x10 on.
S23_EXPORT void __cdecl FxSpiral_Grow(void) {
    Bob();
    Bump(Sc()[0xA], 2);
    if (Sc()[0xA] == 0x10) Bump(Sc()[2]);
}

// original 0x4CDB90: FxSpiral_Phases[2]. Bob; +9 down; at 0: sound 0x101 for
// spiral 0 only; +0xB = FxSpiral_Tilts[2 * +8] (the facing, unbounded); the
// radius and scale 0x80, the spread +0x1C 0x18; +9 0x10; on.
S23_EXPORT void __cdecl FxSpiral_Hold(void) {
    Bob();
    Bump(Sc()[9], -1);
    unsigned char* sc = Sc();
    if (sc[9] != 0) return;
    if (sc[4] == 0) {
        MH_CALL(Sound_PlayById)(0x101);
        sc = Sc();
    }
    sc[0xB] = Byte(::bof3::Img(FxSpiral_Tilts_addr) + 2u * sc[8]);
    rv::Current<records::BattleTask>()->step_z = 0x80;
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x80);
    rv::Current<records::BattleTask>()->work_1c = 0x18;
    Sc()[9] = 0x10;
    Bump(Sc()[2]);
}

// original 0x4CDC50: FxSpiral_Phases[3]. +0xB up 4, the radius up by the
// spread; +9 down, at 0 on.
S23_EXPORT void __cdecl FxSpiral_Spread(void) {
    Bump(Sc()[0xB], 4);
    {
        unsigned char* const sc = Sc();
        records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
        tsc->step_z = tsc->step_z + tsc->work_1c;
    }
    Bump(Sc()[9], -1);
    if (Sc()[9] == 0) Bump(Sc()[2]);
}

// original 0x4CDC90: FxSpiral_Phases[4]. +0xB up by +0xA / 2; the spread
// down 2 and the radius up by it; +0xA down, at 0 the owner's count +0xB down
// and the task freed.
S23_EXPORT void __cdecl FxSpiral_Fade(void) {
    {
        unsigned char* const sc = Sc();
        sc[0xB] = static_cast<unsigned char>(sc[0xB] + (sc[0xA] >> 1));
    }
    {
        unsigned char* const sc = Sc();
        records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
        tsc->work_1c = tsc->work_1c - 2;
    }
    {
        unsigned char* const sc = Sc();
        records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
        tsc->step_z = tsc->step_z + tsc->work_1c;
    }
    Bump(Sc()[0xA], -1);
    if (Sc()[0xA] != 0) return;
    Bump(Owner()[0xB], -1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4CDCF0: the spiral's matrix: Gte_PushMatrix; the tilt +0xC
// about y for an owner facing with bit 0, about x otherwise; the translation
// at the task's position and half its height.
S23_EXPORT void __cdecl FxSpiral_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const bool about_y = (Owner()[8] & 1) != 0;
    const unsigned char* const sc = Sc();
    const short tilt = static_cast<short>(Word(sc + 0xC));
    const short angles[3] = {about_y ? static_cast<short>(0) : tilt, about_y ? tilt : static_cast<short>(0), 0};
    short v[3];
    Position(sc, static_cast<short>(rv::As<records::BattleTask>(sc)->height_whole), v);
    LoadMatrix(angles, v);
}

// original 0x4CDDC0: the spiral band. A draw-mode primitive (0x35) linked at
// the position (row offset 2); inner radius +0x10, outer +0x10 + sin(0) *
// +0x14 >> 12; the angle index +0xB; 31 G4 quads round the circle, the outer
// radius +0x10 + sin(0x40 i) * +0x14 >> 12, the colour +0xA * i for the
// first seven and +0xA * 8 after, the inner pair +0xA; linked with 0x44
// bytes.
S23_EXPORT void __cdecl FxSpiral_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    {
        const unsigned char* const sc = Sc();
        const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
        MH_CALL(MapView_LinkPrimAt)(tsc->x, tsc->z, 2, 0xC);
    }
    {
        const unsigned char* const r = Sc() + 0x10;
        const int s = Sin(0);
        const unsigned char* const sc = Sc();
        Put(::bof3::Img(at::kR), (Imul(s, rv::As<records::BattleTask>(sc)->step_y) >> 12) + Word(r));
        Put(::bof3::Img(at::kR2), Word(r));
        Put(::bof3::Img(at::kW4), sc[0xB]);
        const int first = (sc[0xB] & 0x3F) << 6;
        Put(::bof3::Img(at::kW6), first);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, SinOf(first, ::bof3::Img(at::kR)));
    }
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, CosOf(Get(::bof3::Img(at::kW6)), ::bof3::Img(at::kR)));
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18, SinOf(Get(::bof3::Img(at::kW6)), ::bof3::Img(at::kR2)));
    Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, CosOf(Get(::bof3::Img(at::kW6)), ::bof3::Img(at::kR2)));
    for (int i = 1, angle = 0x40; angle < 0x800; ++i, angle += 0x40) {
        Put(::bof3::Img(at::kW6), ((Byte(::bof3::Img(at::kW4)) + i) & 0x3F) << 6);
        const int s = Sin(angle);
        {
            const unsigned char* const sc = Sc();
            const int outer = (Imul(s, rv::As<records::BattleTask>(sc)->step_y) >> 12) + Word(sc + 0x10);
            Put(::bof3::Img(Prim_VertexScratch_addr), Get(::bof3::Img(Prim_VertexScratch_addr) + 0x8));
            Put(::bof3::Img(Prim_VertexScratch_addr) + 2, Get(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2));
            Put(::bof3::Img(at::kR), outer);
        }
        Put(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, SinOf(Get(::bof3::Img(at::kW6)), ::bof3::Img(at::kR)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, CosOf(Get(::bof3::Img(at::kW6)), ::bof3::Img(at::kR)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Get(::bof3::Img(Prim_VertexScratch_addr) + 0x18));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, Get(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18, SinOf(Get(::bof3::Img(at::kW6)), ::bof3::Img(at::kR2)));
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, CosOf(Get(::bof3::Img(at::kW6)), ::bof3::Img(at::kR2)));
        unsigned char* const prim = Gfx_PacketNext;
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, 0);
        MH_CALL(Gpu_SetPolyG4)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        {
            const unsigned char* const sc = Sc();
            Put(::bof3::Img(at::kW8), angle < 0x200 ? sc[0xA] * i : sc[0xA] << 3);
            Put(::bof3::Img(at::kWA), sc[0xA]);
        }
        Shade(prim, 4, ::bof3::Img(at::kW8));
        Shade(prim, 0x14, ::bof3::Img(at::kW8));
        Shade(prim, 0x24, ::bof3::Img(at::kWA));
        Shade(prim, 0x34, ::bof3::Img(at::kWA));
        Project4(prim, 8, 0x18, 0x28, 0x38);
        MH_CALL(Gte_PrimDepths4_10B)(prim);
        const unsigned char* const sc = Sc();
        const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
        MH_CALL(MapView_LinkPrimAt)(tsc->x, tsc->z, 2, 0x44);
    }
}

void FxSpiral_Inject() {
    BOF3_INJECT(Typhoon_Task);
    BOF3_INJECT(Typhoon_Start);
    BOF3_INJECT(Typhoon_Grow);
    BOF3_INJECT(Typhoon_Fade);
    BOF3_INJECT(Typhoon_DrawFan);
    BOF3_INJECT(FxSpiral_Dispatch);
    BOF3_INJECT(FxSpiral_Task);
    BOF3_INJECT(FxSpiral_Wait);
    BOF3_INJECT(FxSpiral_Grow);
    BOF3_INJECT(FxSpiral_Hold);
    BOF3_INJECT(FxSpiral_Spread);
    BOF3_INJECT(FxSpiral_Fade);
    BOF3_INJECT(FxSpiral_PushMatrix);
    BOF3_INJECT(FxSpiral_Draw);
}
