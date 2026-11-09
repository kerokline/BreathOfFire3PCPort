// Moved here by tools/move_funcs.py from magic_s24.cpp (docs/source-layout-round.md).
#include "game/magic/fx_105.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s24.h"
#include "game/magic_s24_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

namespace cell = magic_s24::cell;

using magic_harness::Handler;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The original's imul: 32 bits, wrapping.
inline int Mul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }

inline int Shl(int a, int n) { return static_cast<int>(static_cast<std::uint32_t>(a) << n); }

inline std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

inline std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}

inline short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

inline short G16(std::uint32_t address) { return static_cast<short>(Word(Mem(address))); }

inline void Put16(std::uint32_t address, int v) { SetWord(Mem(address), static_cast<unsigned>(v) & 0xFFFFu); }

inline std::uint32_t Addr(const void* p) { return Low32(p); }

inline void SetPtr(unsigned char* at_, const void* p) { SetLong(at_, static_cast<std::int32_t>(Addr(p))); }

inline void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

inline void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

inline void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

inline unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

inline unsigned char* TaskSlot(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (index & 0xFFu) * at::kTaskStride); }

inline unsigned char* Mote(unsigned index) { return Mem(::bof3::Img(cell::kMotePool) + (index & 0xFFu) * cell::kMoteStride); }

// A .data table of handlers, read in place as the original reads it (during
// the fuzz its entries are the harness's recorders).
Handler DataPhase(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Callees.
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

using TurnFn = void (__cdecl*)(unsigned char*);

using AllocFn = unsigned char (__cdecl*)();

void CentreOnTargets() { MH_CALL(MagicFx_CenterOnSide)(); }

void TurnByFacing(unsigned char* task) { MH_AT(TurnFn, magic_s24::kTurnByFacing)(task); }

// The scratch words (SharedScratch): radii at +0 / +2, then colours or angles.
constexpr ::bof3::image::Va kR{cell::kScratch};

constexpr ::bof3::image::Va kR2{cell::kScratch + 2};

constexpr ::bof3::image::Va kW4{cell::kScratch + 4};

constexpr ::bof3::image::Va kW6{cell::kScratch + 6};

constexpr ::bof3::image::Va kW8{cell::kScratch + 8};

// kWC: now Scratch_Swap (symbols.toml).
constexpr ::bof3::image::Va kWA{cell::kScratch + 0xA};

constexpr ::bof3::image::Va kWE{cell::kScratch + 0xE};

const short* Vec(std::uint32_t address) { return reinterpret_cast<const short*>(Mem(address)); }

float* Xy(unsigned char* prim, unsigned offset) { return reinterpret_cast<float*>(prim + offset); }

// The four-vertex projection every POLY_G4 of the group makes: a b c d into
// the primitive's four points, then its depth.
void Project4(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers4)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x18), Xy(p, 8), Xy(p, 0x18), Xy(p, 0x28), Xy(p, 0x38), &depth);
    MH_CALL(Gte_PrimDepths4_10B)(p);
}

// The matrix every draw of the group pushes: the angles rotated, times the
// camera, translated by the effect's point.
struct Matrix { short m[3][3]; short pad; std::int32_t t[3]; };

void PushMatrix(const short vector[3], const short angles[3]) {
    Matrix m;
    MH_CALL(Gte_RotTrans)(vector, reinterpret_cast<long*>(m.t));
    MH_CALL(Gte_RotMatrix)(angles, &m.m[0][0]);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, &m.m[0][0], &m.m[0][0]);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// The effect point as the GTE takes it: (x >> 9) - 0x4000, (z >> 9) - 0x4000,
// minus half the height (rounded toward 0).
void PointOf(short v[3], std::int32_t x, std::int32_t z, short height) {
    v[0] = static_cast<short>((x >> 9) - 0x4000);
    v[1] = static_cast<short>((z >> 9) - 0x4000);
    v[2] = static_cast<short>(-(height / 2));
}

}  // namespace

#define S24_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4D16E0: the kind-2 task. +1 through a two-entry stack table -
// Start, BattleFx_Finish - unchecked; then every live mote of the pool
// (0x695C38, 64 of 0x84) run and drawn with Sprite_Current the mote and the
// owner its +0x80, both put back after.
S24_EXPORT void __cdecl Fx105_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Fx105_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Fx105_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const task = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < cell::kMotes; ++i) {
        unsigned char* const m = Mote(i);
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = static_cast<std::int32_t>(tm->owner);
        Sprite_Current = m;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        magic_harness::Phase(bof3::addr::Fx105_Mote)();
        magic_harness::Phase(bof3::addr::Fx105_MoteDraw)();
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = task;
    }
}

// original 0x4D1760: the pool emptied (bytes 0..2 of each), the targets'
// centre (0x4FC0E0), the owner's facing, an offset (-8.0, 0) turned by it
// (0x446770) and added to the field's kind-2 point, +0xC the angle from
// there back to that point (Math_Ratan2), +0xB 0; seven orbs (kind 1,
// parameter 0x33, +1 0, +4 the index), +0xB counting them; CLUT strip row 26
// made semi-transparent (cells 1..15 from the buffer 0x4000 below with bit
// 15, cell 0 cleared); Sound_PlayById(0x100); on.
S24_EXPORT void __cdecl Fx105_Start(void) {
    for (unsigned i = 0; i < cell::kMotes; ++i) {
        unsigned char* const m = Mote(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    CentreOnTargets();
    Sprite_Current[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(0xFFF80000u));
    rv::Current<records::BattleTask>()->step_z = 0;
    TurnByFacing(Sprite_Current);
    unsigned char* sc = Sprite_Current;
    rv::Store(rv::As<records::BattleTask>(sc)->x, Add(rv::As<records::BattleTask>(sc)->step_x, Field_Kind2X));
    sc = Sprite_Current;
    rv::Store(rv::As<records::BattleTask>(sc)->z, Add(rv::As<records::BattleTask>(sc)->step_z, Field_Kind2Z));
    sc = Sprite_Current;
    const std::int32_t dz = Sub(Field_Kind2Z, rv::As<records::BattleTask>(sc)->z);
    const std::int32_t dx = Sub(Field_Kind2X, rv::As<records::BattleTask>(sc)->x);
    const int angle = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
    rv::Current<records::BattleTask>()->step_x = angle;
    Sprite_Current[0xB] = 0;
    for (unsigned i = 0; i < 7; ++i) {
        unsigned char* const t = TaskSlot(MH_CALL(BattleTask_Create)(1, 0x33));
        unsigned char* const s = Sprite_Current;
        SetPtr(t + 0x80, s);
        t[1] = 0;
        t[4] = static_cast<unsigned char>(i);
        Bump(s[0xB]);
    }
    for (unsigned i = 0; i < 16; ++i)
        SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + 2 * i), Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x3400 + 2 * i)) | 0x8000u);
    SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400), 0);
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    Bump(Sprite_Current[1]);
}

// original 0x4D18C0: kind-1 task parameter 0x33 (an orb). +1 through
// Fx105_ChildPhases (0x65B888, one entry: Orb), unchecked.
S24_EXPORT void __cdecl Fx105_Child(void) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= 1) bof3::Fatal("Fx105_Child: +1 %u, past Fx105_ChildPhases' one", phase);
    DataPhase(::bof3::Img(Fx105_ChildPhases_addr), phase)();
}

// original 0x4D18E0: an orb. +2 through a four-entry stack table - OrbStart,
// OrbEmit, OrbHold, MagicFx_EndWithChildren - unchecked; then while live and
// +2 is not 3, its matrix and bands drawn, the matrix popped.
S24_EXPORT void __cdecl Fx105_Orb(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Fx105_OrbStart, bof3::addr::Fx105_OrbEmit,
                                                 bof3::addr::Fx105_OrbHold, bof3::addr::MagicFx_EndWithChildren};
    const unsigned phase = Sprite_Current[2];
    if (phase >= 4) bof3::Fatal("Fx105_Orb: +2 %u, past the four-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 3) return;
    magic_harness::Phase(bof3::addr::Fx105_PushMatrix)();
    magic_harness::Phase(bof3::addr::Fx105_DrawOrb)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D1940: the owner's point; two angles, +0xC and +0x10, each
// the owner's +0xC plus a word of Fx105_OrbAngles by the orb's index +4
// (0x65B88C + 2i and 0x65B88E + 2i, unbounded); +0xB, +9, +0xA 0; +2 on.
S24_EXPORT void __cdecl Fx105_OrbStart(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    unsigned char* sc = Sprite_Current;
    rv::Store(rv::As<records::BattleTask>(sc)->step_x, Add(G16(::bof3::Img(Fx105_OrbAngles_addr) + 2u * sc[4]), rv::As<records::SpriteHead>(Owner())->step_x));
    sc = Sprite_Current;
    rv::Store(rv::As<records::BattleTask>(sc)->step_z, Add(G16(::bof3::Img(Fx105_OrbAngles_addr) + 2 + 2u * sc[4]), rv::As<records::SpriteHead>(Owner())->step_x));
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4D19E0: every fourth frame (by the orb's index), a mote from the
// pool (Fx105_MoteAlloc; 0xFF none): its owner the orb, +1 +2 0, +9 the orb's
// +9, +0x10 the orb's +0x10; the orb's +0xB counts it. On odd frames +9 up;
// at 32, +2 on.
S24_EXPORT void __cdecl Fx105_OrbEmit(void) {
    const unsigned char* sc = Sprite_Current;
    if (((sc[4] ^ static_cast<unsigned char>(Frame_Counter)) & 3) == 0) {
        const unsigned char k = MH_AT(AllocFn, bof3::addr::Fx105_MoteAlloc)();
        if (k != 0xFF) {
            unsigned char* const m = Mote(k);
            records::BattleTask* const tm = rv::As<records::BattleTask>(m);
            unsigned char* const s = Sprite_Current;
            records::BattleTask* const ts = rv::As<records::BattleTask>(s);
            SetPtr(m + 0x80, s);
            m[1] = 0;
            m[2] = 0;
            m[9] = s[9];
            tm->step_z = ts->step_z;
            Bump(s[0xB]);
        }
    }
    if ((Frame_Counter & 1) == 0) return;
    Bump(Sprite_Current[9]);
    unsigned char* const s = Sprite_Current;
    if (s[9] == 0x20) Bump(s[2]);
}

// original 0x4D1A80: +0xA up; at 64, +2 on.
S24_EXPORT void __cdecl Fx105_OrbHold(void) {
    Bump(Sprite_Current[0xA]);
    unsigned char* const sc = Sprite_Current;
    if (sc[0xA] == 0x40) Bump(sc[2]);
}

// original 0x4D1AA0 (reached from 15 overlays: one body the linker kept here):
// once the task's own children are gone (+0xB 0), the owner's +0xB down and
// the task freed.
S24_EXPORT void __cdecl MagicFx_EndWithChildren(void) {
    if (Sprite_Current[0xB] != 0) return;
    Drop(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D1AC0: Gte_PushMatrix, then the orb's matrix: angles 0, the
// point from +0x34 / +0x38 / +0x3E.
S24_EXPORT void __cdecl Fx105_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[3] = {0, 0, 0};
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    short v[3];
    PointOf(v, tsc->x, tsc->z, S16(sc + 0x3E));
    PushMatrix(v, angles);
}

// original 0x4D1B60: the orb. Six colour words from Fx105_OrbColours (by the
// index +4 * 3, unbounded), each c - (c >> 6) * +0xA; the first ring at
// radius 64 by the angles +0xC and +0x10; then +9 bands (radius 64 more each,
// height -(sin(step) * 3 >> 5), step 0x40 on), each a POLY_G4 on the
// ordering row of its first corner, draw mode 0x35.
S24_EXPORT void __cdecl Fx105_DrawOrb(void) {
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    const std::uint32_t colours = ::bof3::Img(Fx105_OrbColours_addr) + 3u * sc[4];
    for (unsigned j = 0; j < 6; ++j) {
        const unsigned char c = Mem(colours + j)[0];
        Put16(::bof3::Img(kW4.v) + 2 * j, c - (c >> 6) * sc[0xA]);
    }
    int t = Sin(tsc->step_x);
    Put16(::bof3::Img(Prim_VertexScratch_addr), Shl(t, 6) >> 12);
    t = Cos(rv::Current<records::BattleTask>()->step_x);
    const unsigned char* s = Sprite_Current;
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, Shl(t, 6) >> 12);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
    t = Sin(rv::As<records::BattleTask>(s)->step_z);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Shl(t, 6) >> 12);
    t = Cos(rv::Current<records::BattleTask>()->step_z);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, Shl(t, 6) >> 12);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
    if (Sprite_Current[9] + 1 <= 1) return;
    int step = 0x40;
    for (int band = 1; band < Sprite_Current[9] + 1; ++band, step += 0x40) {
        t = Sin(step);
        const short a0 = G16(::bof3::Img(Prim_VertexScratch_addr));
        const int dz = -(Shl(Mul(t, 3), 7) >> 12);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, a0);
        const short b0 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x8);
        Put16(::bof3::Img(kR2.v), dz);
        const short a2 = G16(::bof3::Img(Prim_VertexScratch_addr) + 2);
        Put16(::bof3::Img(kR.v), (band + 1) << 6);
        const short a4 = G16(::bof3::Img(Prim_VertexScratch_addr) + 4);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, a2);
        const short b2 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, a4);
        const short b4 = G16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18, b0);
        s = Sprite_Current;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, b2);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, b4);
        t = Sin(rv::As<records::BattleTask>(s)->step_x);
        Put16(::bof3::Img(Prim_VertexScratch_addr), Mul(t, G16(::bof3::Img(kR.v))) >> 12);
        t = Cos(rv::Current<records::BattleTask>()->step_x);
        t = Mul(t, G16(::bof3::Img(kR.v))) >> 12;
        const short h = G16(::bof3::Img(kR2.v));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, t);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, h);
        t = Sin(rv::Current<records::BattleTask>()->step_z);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Mul(t, G16(::bof3::Img(kR.v))) >> 12);
        t = Cos(rv::Current<records::BattleTask>()->step_z);
        t = Mul(t, G16(::bof3::Img(kR.v)));
        const short ax = G16(::bof3::Img(Prim_VertexScratch_addr)), az = G16(::bof3::Img(Prim_VertexScratch_addr) + 2);
        unsigned char* const mode = Gfx_PacketNext;
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, t >> 12);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, G16(::bof3::Img(kR2.v)));
        s = Sprite_Current;
        const std::int32_t x = Add(Shl(ax, 9), rv::As<records::BattleTask>(s)->x);
        const std::int32_t z = Add(Shl(az, 9), rv::As<records::BattleTask>(s)->z);
        MH_CALL(Gpu_SetDrawMode)(mode, 0, 1, 0x35, 0);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = Mem(::bof3::Img(kW4.v))[0];
        p[5] = Mem(::bof3::Img(kW6.v))[0];
        p[6] = Mem(::bof3::Img(kW8.v))[0];
        p[0x14] = Mem(::bof3::Img(kWA.v))[0];
        p[0x15] = Mem(::bof3::Img(Scratch_Swap_addr))[0];
        p[0x16] = Mem(::bof3::Img(kWE.v))[0];
        p[0x24] = Mem(::bof3::Img(kW4.v))[0];
        p[0x25] = Mem(::bof3::Img(kW6.v))[0];
        p[0x26] = Mem(::bof3::Img(kW8.v))[0];
        p[0x34] = Mem(::bof3::Img(kWA.v))[0];
        p[0x35] = Mem(::bof3::Img(Scratch_Swap_addr))[0];
        p[0x36] = Mem(::bof3::Img(kWE.v))[0];
        Project4(p);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 2, 0x44);
    }
}

// original 0x4D1F30: a mote (run with Sprite_Current the mote). +1 through
// Fx105_MotePhases (0x65B8B4, one entry: MoteRun), unchecked.
S24_EXPORT void __cdecl Fx105_Mote(void) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= 1) bof3::Fatal("Fx105_Mote: +1 %u, past Fx105_MotePhases' one", phase);
    DataPhase(::bof3::Img(Fx105_MotePhases_addr), phase)();
}

// original 0x4D1F50: with the sprite bank 0x9039D8 the effects' (0x8E3580),
// +2 through Fx105_MoteRunPhases (0x65B8B8, two, unchecked), then
// BattleActor_UpdateScreenXY; the bank back to 0x8B3580.
S24_EXPORT void __cdecl Fx105_MoteRun(void) {
    const unsigned phase = Sprite_Current[2];
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8E3580));
    if (phase >= 2) bof3::Fatal("Fx105_MoteRun: +2 %u, past Fx105_MoteRunPhases' two", phase);
    DataPhase(::bof3::Img(Fx105_MoteRunPhases_addr), phase)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8B3580));
}

// original 0x4D1F80: a mote's start. Height offset sin(+9 << 6) * 3 >> 4
// (scratch +2), radius (+9 + 1) << 6 (scratch +0); the point round the owner's
// by the angle +0x10 (sin / cos * radius >> 3), its height the owner's plus
// the offset less 0x20; the ground under it (AreaMap_Elevation) to +0x14; the
// sprite's fields (texture 0x1D, CLUT 0xA0, semi-transparent, tint 0xC0,
// ordering slot 3, ...); Sprite_SetAnimation(1 or 2 by Rand); +0xA a Rand in
// 0..63; +2 on.
S24_EXPORT void __cdecl Fx105_MoteStart(void) {
    int t = Sin(Sprite_Current[9] << 6);
    unsigned char* sc = Sprite_Current;
    Put16(::bof3::Img(kR2.v), Shl(Mul(t, 3), 7) >> 11);
    Put16(::bof3::Img(kR.v), (sc[9] + 1) << 6);
    t = Sin(rv::As<records::BattleTask>(sc)->step_z);
    t = (Mul(t, G16(::bof3::Img(kR.v))) >> 3);
    rv::Store(rv::Current<records::BattleTask>()->x, Add(t, rv::As<records::SpriteHead>(Owner())->x));
    t = Cos(rv::Current<records::BattleTask>()->step_z);
    t = (Mul(t, G16(::bof3::Img(kR.v))) >> 3);
    rv::Store(rv::Current<records::BattleTask>()->z, Add(t, rv::As<records::SpriteHead>(Owner())->z));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::As<records::SpriteHead>(Owner())->height_whole + Word(Mem(::bof3::Img(kR2.v))) - 0x20u);
    sc = Sprite_Current;
    const long ground = MH_CALL(AreaMap_Elevation)(rv::As<records::BattleTask>(sc)->x, rv::As<records::BattleTask>(sc)->z);
    rv::Store(rv::Current<records::BattleTask>()->step_y, static_cast<short>(ground));
    Sprite_Current[0x25] = 0x1D;
    Sprite_Current[0x26] = 0;
    Sprite_Current[0x27] = 0xA0;
    Sprite_Current[0x28] = 0;
    Sprite_Current[0] |= 0x20;
    Sprite_Current[0x5D] = 0xC0;
    Sprite_Current[0x5E] = 0xC0;
    Sprite_Current[0x5F] = 0xC0;
    Sprite_Current[0x5C] = 1;
    Sprite_Current[0x2A] = 0;
    Sprite_Current[0x29] = 3;
    Sprite_Current[0x24] = 4;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sprite_Current[0x2B] = 0;
    const int r = MH_CALL(Rand)();
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>((r & 1) + 1));
    const int r2 = MH_CALL(Rand)();
    Sprite_Current[0xA] = static_cast<unsigned char>(r2 & 0x3F);
    Bump(Sprite_Current[2]);
}

// original 0x4D2110: a mote drifting: +0xA up, the point moved by sin / cos
// of (+0xA & 63) << 8 (sign-extended from 29 bits), the height down 8, the
// script ticked once; when it ends or the height falls below the ground +0x14,
// the owner's +0xB down and the mote's bytes 0..2 cleared.
S24_EXPORT void __cdecl Fx105_MoteDrift(void) {
    Bump(Sprite_Current[0xA]);
    unsigned char* sc = Sprite_Current;
    unsigned char* const px = sc + 0x34;
    int t = Sin((sc[0xA] & 0x3F) << 8);
    SetLong(px, Add(Long(px), Shl(t, 3) >> 3));
    sc = Sprite_Current;
    unsigned char* const pz = sc + 0x38;
    t = Cos((sc[0xA] & 0x3F) << 8);
    SetLong(pz, Add(Long(pz), Shl(t, 3) >> 3));
    sc = Sprite_Current;
    rv::Store<unsigned>(rv::As<records::BattleTask>(sc)->height_whole, rv::As<records::BattleTask>(sc)->height_whole - 8u);
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) {
        sc = Sprite_Current;
        if (S16(sc + 0x3E) >= rv::As<records::BattleTask>(sc)->step_y) return;
    }
    Drop(Owner()[0xB]);
    Sprite_Current[0] = 0;
    Sprite_Current[1] = 0;
    Sprite_Current[2] = 0;
}

// original 0x4D21C0: the first mote not in use (bit 0 of +0), marked; 0xFF
// when all 64 are.
S24_EXPORT unsigned char __cdecl Fx105_MoteAlloc(void) {
    for (unsigned k = 0; k < cell::kMotes; ++k) {
        unsigned char* const m = Mote(k);
        if (m[0] & 1) continue;
        m[0] |= 1;
        return static_cast<unsigned char>(k);
    }
    return 0xFF;
}

// original 0x4D2220: a mote's sprite. Unless hidden (+0 bit 6) or its screen
// point (+0x2E, +0x30) is outside (-0x40..0x180, -0x40..0x130): the frame at
// [+0x54] + u16 +0x5A - a count, then that many 5-byte parts (size index,
// x, y, u, v) - each a draw mode (texture page by +0x28 and +0x5C) and a
// SPRT on ordering slot +0x29, its size from Fx105_MoteSizes, CLUT by +0x27
// (+0x28 picks the row), tint +0x5D..+0x5F + 0x80, flipped by bit 7 when
// +0x2A, semi-transparent by +0 bit 5. DIV-0041: the x bounds move out by the
// fill columns (the PSP's [-96, 416] for 32), so the margin past the wide
// view is the original's 64; the y bounds as they were. Unarmed or narrow,
// the original's.
S24_EXPORT void __cdecl Fx105_MoteDraw(void) {
    const unsigned char* sc = Sprite_Current;
    if (sc[0] & 0x40) return;
    const short sx = S16(sc + 0x2E);
    const int wide = static_cast<int>(Widescreen_Fill());
    if (sx > 0x180 + wide || sx < -0x40 - wide) return;
    const short sy = S16(sc + 0x30);
    if (sy > 0x130 || sy < -0x40) return;
    const unsigned char* part = Mem(static_cast<std::uint32_t>(static_cast<std::int32_t>(rv::As<records::BattleTask>(sc)->frames)) + rv::As<records::BattleTask>(sc)->frame_word);
    const unsigned char count = part[0];
    ++part;
    unsigned clut;
    if (sc[0x28] != 0) {
        clut = ((sc[0x27] + 0x1E0u) << 6) & 0xFFFFu;
    } else {
        clut = ((((sc[0x27] >> 4) + 0x1F0u) << 6) | (sc[0x27] & 0xFu)) & 0xFFFFu;
    }
    for (unsigned n = count; n != 0; --n, part += 5) {
        sc = Sprite_Current;
        const unsigned tpage = ((((sc[0x28] != 0 ? 4u : 0u) | (sc[0x5C] & 3u)) << 5) & 0xFFFFu) | 0x1Du;
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
        MH_CALL(Gfx_CommitPrim)(Sprite_Current[0x29], 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetSprt)(p);
        rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->w, Mem(::bof3::Img(Fx105_MoteSizes_addr))[(part[0] & 0xF) * 2]);
        rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->h, Mem(::bof3::Img(Fx105_MoteSizes_addr) + 1)[(part[0] & 0xF) * 2]);
        p[0x14] = part[3];
        const unsigned char v = part[4];
        const unsigned char row = Sprite_Current[0x26];
        rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->clut, clut);
        p[0x15] = static_cast<unsigned char>(row + v);
        p[4] = static_cast<unsigned char>(Sprite_Current[0x5D] + 0x80);
        p[5] = static_cast<unsigned char>(Sprite_Current[0x5E] + 0x80);
        p[6] = static_cast<unsigned char>(Sprite_Current[0x5F] + 0x80);
        const unsigned char* const s = Sprite_Current;
        if ((part[0] & 0x80) && s[0x2A] != 0) {
            PutFloat(p + 8, S16(s + 0x2E) - static_cast<signed char>(part[1]) - static_cast<int>(rv::As<records::GpuSprt>(p)->w));
        } else {
            PutFloat(p + 8, S16(s + 0x2E) + static_cast<signed char>(part[1]));
        }
        PutFloat(p + 0xC, S16(Sprite_Current + 0x30) + static_cast<signed char>(part[2]));
        MH_CALL(Gpu_SetSemiTrans)(p, (Sprite_Current[0] >> 5) & 1u);
        MH_CALL(Gfx_CommitPrim)(Sprite_Current[0x29], 0x1C);
    }
}

void Fx105_Inject() {
    BOF3_INJECT(Fx105_Task);
    BOF3_INJECT(Fx105_Start);
    BOF3_INJECT(Fx105_Child);
    BOF3_INJECT(Fx105_Orb);
    BOF3_INJECT(Fx105_OrbStart);
    BOF3_INJECT(Fx105_OrbEmit);
    BOF3_INJECT(Fx105_OrbHold);
    BOF3_INJECT(MagicFx_EndWithChildren);
    BOF3_INJECT(Fx105_PushMatrix);
    BOF3_INJECT(Fx105_DrawOrb);
    BOF3_INJECT(Fx105_Mote);
    BOF3_INJECT(Fx105_MoteRun);
    BOF3_INJECT(Fx105_MoteStart);
    BOF3_INJECT(Fx105_MoteDrift);
    BOF3_INJECT(Fx105_MoteAlloc);
    BOF3_INJECT(Fx105_MoteDraw);
}
