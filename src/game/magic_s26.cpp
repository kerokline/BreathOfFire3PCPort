// Three spell overlays compiled into the exe, round nine group S26
// (docs/magic_s26.md): the PSX's MAGIC114, MAGIC115 and MAGIC117.EMI,
// Magic_Rows rows 11, 115, 73 and 106. Read one id down (docs/cut-content.md
// section 2) the sibling labels them Fire Whip, Remedy, Rest / Snooze and
// Douse; the names below carry the overlay's number instead, and say what the
// code does.
//
//   - MAGIC114 0x4D7960..0x4D9437: a ring of eight triangles round the caster,
//     a volley of kind-1 children (task 7) that each draw a trail, then a beam
//     of three strips and a sprite at the far end; rays of lines at the close;
//   - MAGIC115 0x4D9440..0x4D9ADA: the source sprite tinted and CLUT row 26
//     marked, a child (task 0x30) circling the owner and dropping motes;
//   - MAGIC117 0x4D9F40..0x4DA214: two tint pulses on the source sprite (row
//     73: brighter twice; row 106: darker once), then a flash on the target.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s26.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
namespace addr = bof3::addr;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// DamageScratch's first dword (0x903850; a word at +0 and +2 in the beams)
// and Prim_VertexScratch's four SVECTORs (0x9037A0..0x9037BF): read again
// after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;
// The sprite frame-offset table pointer (sprite_pose.h): the effects' bank
// 0x8E3580 around a sprite child's phase, the engine's 0x8B3580 after.
constexpr std::uint32_t kFrameSet = 0x9039D8;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Source() { return Pointer(at::kSource); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
unsigned char* TintRecord(unsigned i) { return MoveScript_TintRecords + 12 * i; }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::int32_t AddL(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
std::int32_t SubL(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
std::int32_t MulL(std::int32_t a, unsigned b) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * b); }
std::int32_t Shl(std::int32_t a, unsigned n) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << n); }
// A position dword as a vertex word: `sar 9`, less 0x4000.
unsigned Pos9(std::int32_t v) { return static_cast<unsigned>((v >> 9) - 0x4000); }
// `cdq; sub eax, edx; sar eax, 1; neg eax`: minus half, toward zero.
unsigned NegHalf(std::int32_t v) { return static_cast<unsigned>(-(v / 2)); }
void SetL(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(v)); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// Rotates the dx / dz pair +0xC / +0x10 of the task it is given by its
// direction byte +8 - Capcom's, unnamed, in no group (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = bof3::addr::Battle_TurnVectorC;
void Turn() { MH_AT(void (__cdecl*)(unsigned char*), kTurnOffset)(Sc()); }

// The GTE and GPU callees with the arguments the originals push: the
// projections get the depth and flag pointers the originals pass (the
// symbols' prototypes stop one short; cdecl, the caller pops).
using Ra3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                              long*, long*);
using Ra4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                              unsigned char*, unsigned char*, long*, long*);
using Rtp1Fn = long (__cdecl*)(const short*, unsigned char*, long*, long*);
using RotTransFn = void (__cdecl*)(const short*, long*, long*);
#define S26_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

long Average3(unsigned char* prim) {
    long p, flag;
    return S26_AS(Ra3Fn, Gte_RotAverage3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}
void Average4(unsigned char* prim) {
    long p, flag;
    S26_AS(Ra4Fn, Gte_RotAverage4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38, &p,
                                  &flag);
}
void Rtp1(unsigned char* sxy) {
    long p, flag;
    S26_AS(Rtp1Fn, Gte_RotTransPers)(VP(0), sxy, &p, &flag);
}
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The GTE matrix push both of MAGIC114's pushes make: the rotation `rot`, the
// translation the vector v through the current matrix, times Camera_Matrix.
void PushMatrixAt(const short* rot, const short* v) {
    struct Matrix {
        short m[10];
        long t[3];
    } m;
    static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
    long flag;
    S26_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// The vertex word triple of the task's position moved by its +0xC / +0x10
// (both dwords, wrapping) and its height word +0x3E halved.
void VertexOffset(unsigned k, const unsigned char* s) {
    SetVW(k, Pos9(AddL(Long(s + 0x34), Long(s + 0xC))));
    SetVW(k + 2, Pos9(AddL(Long(s + 0x38), Long(s + 0x10))));
    SetVW(k + 4, NegHalf(S16(s + 0x3E)));
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

}  // namespace

#define S26_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC114 (row 11, Fire Whip read one id down)

// original 0x4D7960: the kind-2 task. A draw mode (tpage 0x35) committed to
// slot 3; while +0 and +1 are set, the ring (Magic114_DrawRing); a
// seven-entry stack table by +1, unchecked; the draw mode again.
S26_EXPORT void __cdecl Magic114_Task(void) {
    static constexpr std::uint32_t kPhases[7] = {addr::Magic114_Start, addr::Magic114_Grow,   addr::Magic114_Hold,
                                                 addr::Magic114_Launch, addr::Magic114_Rays,  addr::Magic114_Shrink,
                                                 addr::Magic114_End};
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned char* s = Sc();
    if (s[0] != 0 && s[1] != 0) {
        Call0(addr::Magic114_DrawRing);
        s = Sc();
    }
    const unsigned phase = s[1];
    if (phase >= 7) PastTable("Magic114_Task", phase, 7);
    magic_harness::Phase(kPhases[phase])();
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4D7A00: CLUT row 2 from its source with the STP bit, row 26 from
// its source as it is, Gfx_ClutStripDirty; sound 0x100; the owner's direction
// byte and position; +0xA 0, +1 on.
S26_EXPORT void __cdecl Magic114_Start(void) {
    for (unsigned k = 0; k < 0x100; ++k) {
        Gfx_ClutStrip[0x200 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x200 + k] | 0x8000);
        Gfx_ClutStrip[0x1A00 + k] = Gfx_ClutStripSource[0x1A00 + k];
    }
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0xA] = 0;
    Inc(Sc()[1]);
}

// original 0x4D7AA0: the ring's size +0xA up; past 0x20 +1 on and +9 0.
S26_EXPORT void __cdecl Magic114_Grow(void) {
    Inc(Sc()[0xA]);
    unsigned char* const s = Sc();
    if (s[0xA] > 0x20) {
        Inc(s[1]);
        Sc()[9] = 0;
    }
}

// original 0x4D7AD0: +9 up; past 0x20 +1 on and +9 0.
S26_EXPORT void __cdecl Magic114_Hold(void) {
    Inc(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] > 0x20) {
        Inc(s[1]);
        Sc()[9] = 0;
    }
}

// original 0x4D7B00: +9 up; from 0x20 on: +1 on, sound 0x101, and a kind-1
// child (task 7) with +1 0, the owner's direction byte and the owner as its
// owner.
S26_EXPORT void __cdecl Magic114_Launch(void) {
    Inc(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] < 0x20) return;
    Inc(s[1]);
    MH_CALL(Sound_PlayById)(0x101);
    const unsigned slot = NewTask(7);
    unsigned char* const child = TaskSlot(slot);
    unsigned char* const owner = Owner();
    child[1] = 0;
    child[8] = owner[8];
    SetL(child + 0x80, Key(owner));
}

// original 0x4D7B70: +9 down; at 0 +1 on; else the rays (a tail jmp).
S26_EXPORT void __cdecl Magic114_Rays(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] == 0) {
        Inc(s[1]);
        return;
    }
    Call0(addr::Magic114_DrawRays);
}

// original 0x4D7BA0: the ring's size +0xA down; at 0 +1 on and +9 0x40.
S26_EXPORT void __cdecl Magic114_Shrink(void) {
    Dec(Sc()[0xA]);
    unsigned char* const s = Sc();
    if (s[0xA] == 0) {
        Inc(s[1]);
        Sc()[9] = 0x40;
    }
}

// original 0x4D7BD0 (also MAGIC054's and MAGIC113's): +9 down; at 0 the
// target's flag 0x40, the done flag, and the task freed (a tail jmp).
S26_EXPORT void __cdecl Magic114_End(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D7C10: the ring - under the owner's matrix, eight triangles
// (Magic114_DrawTriangle, +0xB its index), each from two points of
// Magic114_RingOffsets (by index & 3) scaled by the size +0xA and turned by
// the direction byte; then linked by their depths at the owner's position
// (MagicFx_LinkByDepth: eight of 0x34 bytes, bias 2), the matrix popped.
S26_EXPORT void __cdecl Magic114_DrawRing(void) {
    Call0(addr::Magic114_PushMatrix);
    const std::uint32_t prims = Key(Gfx_PacketNext);
    int keys[8] = {};
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned row = (i & 3) * 2;
        unsigned char* s = Sc();
        SetLong(s + 0xC, MulL(Magic114_RingOffsets[row], s[0xA]));
        SetLong(s + 0x10, MulL(Magic114_RingOffsets[row + 1], s[0xA]));
        Turn();
        s = Sc();
        SetLong(s + 0x34, Long(s + 0xC));
        SetLong(s + 0x38, Long(s + 0x10));
        SetLong(s + 0xC, MulL(Magic114_RingOffsets[row + 2], s[0xA]));
        SetLong(s + 0x10, MulL(Magic114_RingOffsets[row + 3], s[0xA]));
        Turn();
        Sc()[0xB] = static_cast<unsigned char>(i);
        MH_AT(void (__cdecl*)(int*), addr::Magic114_DrawTriangle)(keys);
    }
    const unsigned char* const owner = Owner();
    MH_CALL(MagicFx_LinkByDepth)(static_cast<unsigned>(Long(owner + 0x34)), static_cast<unsigned>(Long(owner + 0x38)), keys,
                                 prims, 8, 0x34, 2);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D7D00: one of the ring's triangles, gouraud, semi-transparent
// by the draw mode: from the task's position (+0x34 / +0x38) to its offset
// (+0xC / +0x10) to the centre raised 3 x size (lowered for index 4 and up);
// its depth into keys[+0xB] (read again after the projection); colours by the
// index (8 and up: none set), white-blue on frames where Frame_Counter & 0x18
// is 0 and (Frame_Counter >> 1 ^ index) & 3 is 0 for index 4..7 in phase 2;
// in phases 3 and 4 indices 1, 2, 5 and 6 fade one vertex by +9; then
// Gfx_PacketNext on by 0x34.
S26_EXPORT void __cdecl Magic114_DrawTriangle(int* keys) {
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG3)(p);
    unsigned char* s = Sc();
    SetVW(0, static_cast<unsigned>(Long(s + 0x34) >> 9));
    SetVW(2, static_cast<unsigned>(Long(s + 0x38) >> 9));
    SetVW(4, 0);
    SetVW(8, static_cast<unsigned>(Long(s + 0xC) >> 9));
    SetVW(0xA, static_cast<unsigned>(Long(s + 0x10) >> 9));
    SetVW(0xC, 0);
    SetVW(0x10, 0);
    SetVW(0x12, 0);
    {
        const int lift = s[0xA] * 3;
        SetVW(0x14, static_cast<unsigned>(s[0xB] < 4 ? lift : -lift));
    }
    const long depth = Average3(p);
    s = Sc();
    keys[s[0xB]] = static_cast<int>(depth);
    const unsigned char dl = 0xC0;
    const auto rgb = [p](unsigned v, unsigned char r, unsigned char g, unsigned char b) {
        p[v] = r;
        p[v + 1] = g;
        p[v + 2] = b;
    };
    switch (s[0xB]) {
    case 0: case 2: rgb(4, 0x36, 0x36, 0xB0); rgb(0x14, 0x36, 0x36, 0xB0); rgb(0x24, 0x36, 0x36, dl); break;
    case 1: case 3: rgb(4, 0x40, 0x40, dl); rgb(0x14, 0x40, 0x40, dl); rgb(0x24, 0x40, 0x40, dl); break;
    case 4: case 6: rgb(4, 0x46, 0x46, 0xB0); rgb(0x14, 0x46, 0x46, 0xB0); rgb(0x24, 0x46, 0x46, dl); break;
    case 5: case 7: rgb(4, 0x50, 0x50, dl); rgb(0x14, 0x50, 0x50, dl); rgb(0x24, 0x50, 0x50, dl); break;
    default: break;
    }
    s = Sc();
    if (s[1] == 2) {
        const unsigned frame = Frame_Counter;
        if ((frame & 0x18) == 0) {
            const unsigned index = s[0xB];
            if (index > 3 && (((frame >> 1) ^ index) & 3) == 0) {
                rgb(4, 0x80, 0x80, dl);
                rgb(0x14, 0x80, 0x80, dl);
                rgb(0x24, 0x80, 0x80, dl);
                s = Sc();
            }
        }
    }
    if (s[1] == 3 || s[1] == 4) {
        const unsigned b9 = s[9];
        switch (s[0xB]) {
        case 1:
            p[0x14] = static_cast<unsigned char>(b9 * 5 + 0x40);
            p[0x15] = static_cast<unsigned char>((b9 + 0x10) << 2);
            p[0x16] = static_cast<unsigned char>(dl - (b9 << 1));
            break;
        case 2:
            p[4] = static_cast<unsigned char>(b9 * 5 + 0x36);
            p[5] = static_cast<unsigned char>((b9 << 2) + 0x36);
            p[6] = static_cast<unsigned char>(0xB0 - b9);
            break;
        case 5:
            p[0x14] = static_cast<unsigned char>((b9 + 0x14) << 2);
            p[0x15] = static_cast<unsigned char>(b9 * 3 + 0x50);
            p[0x16] = static_cast<unsigned char>(dl - (b9 << 1));
            break;
        case 6:
            p[4] = static_cast<unsigned char>((b9 << 2) + 0x46);
            p[5] = static_cast<unsigned char>(b9 * 3 + 0x46);
            p[6] = static_cast<unsigned char>(0xB0 - b9);
            break;
        default: break;
        }
    }
    Gfx_PacketNext = Gfx_PacketNext + 0x34;
}

// original 0x4D8000: the matrix pushed at the owner's position (height word
// +0x3E + 0x200 halved), turned about z by ((+0xA & 0x1F) - 2) << 7.
S26_EXPORT void __cdecl Magic114_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned size = Sc()[0xA];
    const unsigned char* const owner = Owner();
    const short rot[4] = {0, 0, static_cast<short>(((size & 0x1F) - 2) << 7), 0};
    short v[4];
    v[0] = static_cast<short>(Pos9(Long(owner + 0x34)));
    v[1] = static_cast<short>(Pos9(Long(owner + 0x38)));
    v[2] = static_cast<short>(NegHalf(S16(owner + 0x3E) + 0x200));
    v[3] = 0;
    PushMatrixAt(rot, v);
}

// original 0x4D80C0: the child's kind-1 task (parameter 7), a jmp through
// Magic114_ChildKinds (three entries) by +1, unchecked.
S26_EXPORT void __cdecl Magic114_ChildTask(void) {
    static constexpr std::uint32_t kKinds[3] = {addr::Magic114_TrailRun, addr::Magic114_BeamRun, addr::Magic114_SpriteRun};
    const unsigned kind = Sc()[1];
    if (kind >= 3) PastTable("Magic114_ChildTask", kind, 3);
    magic_harness::Phase(kKinds[kind])();
}

// original 0x4D80E0: the trail child, a jmp through Magic114_TrailPhases (two
// entries) by +2, unchecked.
S26_EXPORT void __cdecl Magic114_TrailRun(void) {
    static constexpr std::uint32_t kPhases[2] = {addr::Magic114_TrailAim, addr::Magic114_TrailSpawn};
    const unsigned phase = Sc()[2];
    if (phase >= 2) PastTable("Magic114_TrailRun", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D8100: the task at the centre of the target's side
// (MagicFx_CenterOnSide), its height word into +0x14; +0xC / +0x10
// Magic114_RingOffsets[4] and [5] << 5, turned; the position the owner's
// moved by them, the height the owner's + 0x200; +0xA and +9 0, +2 on.
S26_EXPORT void __cdecl Magic114_TrailAim(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    unsigned char* s = Sc();
    SetLong(s + 0x14, S16(s + 0x3E));
    SetLong(s + 0xC, Shl(Magic114_RingOffsets[4], 5));
    SetLong(s + 0x10, Shl(Magic114_RingOffsets[5], 5));
    Turn();
    unsigned char* owner = Owner();
    s = Sc();
    SetLong(s + 0x34, AddL(Long(owner + 0x34), Long(s + 0xC)));
    owner = Owner();
    s = Sc();
    SetLong(s + 0x38, AddL(Long(owner + 0x38), Long(s + 0x10)));
    owner = Owner();
    s = Sc();
    SetWord(s + 0x3E, (Word(owner + 0x3E) + 0x200) & 0xFFFF);
    Sc()[0xA] = 0;
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4D81A0: the trail drawn (Magic114_DrawTrail); a sprite child
// (task 7, +1 2) at the trail's end (+0xC / +0x10 plus Field_Kind2X / Z,
// height +0x14); on the first frame (+9 0) the beam child too (task 7, +1 1,
// with this task's direction, offsets and height, +9 0x10, +0xB 0); +9 up, and
// past 0xC the task freed.
S26_EXPORT void __cdecl Magic114_TrailSpawn(void) {
    Call0(addr::Magic114_DrawTrail);
    {
        const unsigned slot = NewTask(7);
        const std::int32_t kx = Field_Kind2X;
        unsigned char* const child = TaskSlot(slot);
        unsigned char* const owner = Owner();
        SetL(child + 0x80, Key(owner));
        child[1] = 2;
        child[8] = owner[8];
        const unsigned char* const s = Sc();
        SetLong(child + 0x34, AddL(Long(s + 0xC), kx));
        SetLong(child + 0x38, AddL(Long(s + 0x10), Field_Kind2Z));
        SetWord(child + 0x3E, Word(s + 0x14));
    }
    unsigned char* s = Sc();
    if (s[9] == 0) {
        const unsigned slot = NewTask(7);
        unsigned char* const child = TaskSlot(slot);
        unsigned char* const owner = Owner();
        child[1] = 1;
        SetL(child + 0x80, Key(owner));
        s = Sc();
        child[8] = s[8];
        SetLong(child + 0xC, Long(s + 0xC));
        SetLong(child + 0x10, Long(s + 0x10));
        SetWord(child + 0x3E, Word(s + 0x14));
        child[9] = 0x10;
        child[0xB] = 0;
    }
    Inc(s[9]);
    if (Sc()[9] > 0xC) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D82B0: the trail, one gouraud quad (tpage 0xB5, semi-
// transparent) from the task's position turned by (0, -0x1000) and (0,
// 0x1000) to the far end (0x28000, (5 - +9) << 17) and ((7 - +9) x 3 << 15)
// past Field_Kind2X / Z; dark red to orange; linked at the end point.
S26_EXPORT void __cdecl Magic114_DrawTrail(void) {
    unsigned char* s = Sc();
    SetL(s + 0x10, (5u - s[9]) << 0x11);
    SetL(Sc() + 0xC, 0x28000);
    Turn();
    s = Sc();
    const std::int32_t x = AddL(Long(s + 0xC), Field_Kind2X);
    const std::int32_t z = AddL(Long(s + 0x10), Field_Kind2Z);
    DrawMode(0xB5);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 0, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetL(Sc() + 0xC, 0);
    SetL(Sc() + 0x10, 0xFFFFF000u);
    Turn();
    s = Sc();
    VertexOffset(0, s);
    SetL(s + 0xC, 0);
    SetL(Sc() + 0x10, 0x1000);
    Turn();
    s = Sc();
    VertexOffset(8, s);
    SetL(s + 0x10, ((7u - s[9]) * 3) << 0xF);
    SetL(Sc() + 0xC, 0x28000);
    Turn();
    s = Sc();
    SetVW(0x18, Pos9(AddL(Long(s + 0xC), Field_Kind2X)));
    SetVW(0x1A, Pos9(AddL(Long(s + 0x10), Field_Kind2Z)));
    SetVW(0x1C, NegHalf(Long(s + 0x14)));
    SetVW(0x10, Pos9(x));
    SetVW(0x12, Pos9(z));
    SetVW(0x14, NegHalf(Long(s + 0x14)));
    Average4(p);
    p[4] = 0x10;
    p[5] = 1;
    p[6] = 1;
    p[0x14] = 0x10;
    p[0x15] = 1;
    p[0x16] = 1;
    p[0x24] = 0xE0;
    p[0x25] = 0x40;
    p[0x26] = 0x40;
    p[0x34] = 1;
    p[0x35] = 1;
    p[0x36] = 1;
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 0, 0x44);
}

// original 0x4D8530: the beam child, a call through Magic114_BeamPhases (three
// entries) by +2, unchecked; then while +0 and +2 are set its glow, its
// textured strip (semi-transparency mode +0xB) and its core (a tail jmp).
S26_EXPORT void __cdecl Magic114_BeamRun(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Magic114_BeamWait, addr::Magic114_BeamGrow, addr::Magic114_BeamFade};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("Magic114_BeamRun", phase, 3);
    magic_harness::Phase(kPhases[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(addr::Magic114_DrawBeamGlow);
    const unsigned abr = Sc()[0xB];
    MH_AT(void (__cdecl*)(unsigned), addr::Magic114_DrawBeamStrip)(abr);
    Call0(addr::Magic114_DrawBeamCore);
}

// original 0x4D8570: +9 down; at 0 sound 0x102, the position the offsets past
// Field_Kind2X / Z, +0xB 0, +9 0x10, the length +0x14 and the scroll +0x20 0,
// +2 on.
S26_EXPORT void __cdecl Magic114_BeamWait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sound_PlayById)(0x102);
    unsigned char* s = Sc();
    SetLong(s + 0x34, AddL(Long(s + 0xC), Field_Kind2X));
    s = Sc();
    SetLong(s + 0x38, AddL(Long(s + 0x10), Field_Kind2Z));
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    SetLong(Sc() + 0x14, 0);
    SetLong(Sc() + 0x20, 0);
    Inc(Sc()[2]);
}

// original 0x4D8600: the scroll +0x20 down 5; at length 0x30 the target's flag
// 0x10; the length +0x14 up 2, and at 0xC0 +0xB 1 (additive) and +2 on.
S26_EXPORT void __cdecl Magic114_BeamGrow(void) {
    unsigned char* s = Sc();
    SetLong(s + 0x20, AddL(Long(s + 0x20), -5));
    s = Sc();
    if (Long(s + 0x14) == 0x30) {
        MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
        s = Sc();
    }
    SetLong(s + 0x14, AddL(Long(s + 0x14), 2));
    s = Sc();
    if (Long(s + 0x14) == 0xC0) {
        s[0xB] = 1;
        Inc(Sc()[2]);
    }
}

// original 0x4D8660: the scroll down 5, the length up 2, +9 down; at 0 the
// task freed (a tail jmp).
S26_EXPORT void __cdecl Magic114_BeamFade(void) {
    unsigned char* s = Sc();
    SetLong(s + 0x20, AddL(Long(s + 0x20), -5));
    s = Sc();
    SetLong(s + 0x14, AddL(Long(s + 0x14), 2));
    Dec(Sc()[9]);
    if (Sc()[9] == 0) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D86A0: the beam's core - a draw mode (tpage 0xB5) to slot 3;
// then while the step i (from 1) is below the length +0x14 and at most 55
// steps (the depth -0x8000 x i above -0x1C0000), a textured quad (page 0x340,
// 0x100, CLUT (0, 0x1E2), semi-transparent) from the last pair of points to
// the next - each turned from (width, depth) - v scrolling with +0x20 over
// 0xBC rows, grey pulsing with Frame_Counter (while +2 is below 2) or +9 x 10;
// committed to slot 3.
S26_EXPORT void __cdecl Magic114_DrawBeamCore(void) {
    DrawMode(0xB5);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    SetL(Sc() + 0xC, 0x80000);
    SetL(Sc() + 0x10, 0);
    Turn();
    unsigned char* s = Sc();
    VertexOffset(0x10, s);
    SetL(s + 0xC, 0xFFFD0000u);
    SetL(Sc() + 0x10, 0);
    Turn();
    s = Sc();
    VertexOffset(0x18, s);
    if (Long(s + 0x14) <= 1) return;
    std::int32_t i = 1;
    for (std::int32_t y = -0x8000;; y -= 0x8000) {
        if (y <= -0x1C0000) return;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetVW(2, VW(0x12));
        SetVW(0, VW(0x10));
        SetVW(4, VW(0x14));
        SetVW(0xA, VW(0x1A));
        SetVW(8, VW(0x18));
        SetVW(0xC, VW(0x1C));
        s = Sc();
        {
            const std::int32_t d = SubL(Long(s + 0x14), i);
            SetLong(s + 0xC, d < 0x20 ? Shl(d, 0xE) : 0x80000);
        }
        SetLong(Sc() + 0x10, y);
        Turn();
        s = Sc();
        VertexOffset(0x10, s);
        {
            const std::int32_t length = Long(s + 0x14);
            SetL(s + 0xC, SubL(length, i) < 8 ? static_cast<std::uint32_t>(Shl(MulL(SubL(i, length), 3), 0xD)) : 0xFFFD0000u);
        }
        SetLong(Sc() + 0x10, y);
        Turn();
        s = Sc();
        VertexOffset(0x18, s);
        SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100) & 0xFFFF);
        SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1E2) & 0xFFFF);
        s = Sc();
        SetWord(Mem(kS), (static_cast<std::uint32_t>(AddL(Long(s + 0x20), i * 4)) % 0xBC + 0x40) & 0xFFFF);
        p[0x14] = 0x40;
        p[0x24] = 0xC0;
        p[0x15] = SB(0);
        p[0x25] = SB(0);
        p[0x34] = 0x40;
        p[0x44] = 0xC0;
        p[0x35] = static_cast<unsigned char>(SB(0) + 3);
        p[0x45] = static_cast<unsigned char>(SB(0) + 3);
        Average4(p);
        s = Sc();
        if (s[2] < 2) {
            const int sn = MH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + static_cast<unsigned>(i)) & 0x3F) << 6));
            const std::int32_t grey = (Shl(sn, 5) >> 0xC) + 0xA0;
            SetWord(Mem(kS + 2), static_cast<unsigned>(grey) & 0xFFFF);
            p[4] = static_cast<unsigned char>(grey);
        } else {
            const unsigned grey = s[9] * 10u;
            SetWord(Mem(kS + 2), grey & 0xFFFF);
            p[4] = static_cast<unsigned char>(grey);
        }
        p[5] = SB(2);
        p[6] = SB(2);
        MH_CALL(Gfx_CommitPrim)(3, 0x48);
        ++i;
        if (!(i < Long(Sc() + 0x14))) return;
    }
}

// original 0x4D8A60: the beam's strip, as the core with tpage 0x95 |
// (abr & 3) << 5 and semi-transparency mode abr (a byte) to slot 5, u from
// the step (i & 0xF) x 12 + 0x40 on a strip 0xC4 wide, v 0 .. 0xB, grey +9 x 8.
S26_EXPORT void __cdecl Magic114_DrawBeamStrip(unsigned abr_arg) {
    const unsigned abr = abr_arg & 0xFF;
    DrawMode(((abr & 3) << 5) | 0x95);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetL(Sc() + 0xC, 0x80000);
    SetL(Sc() + 0x10, 0);
    Turn();
    unsigned char* s = Sc();
    VertexOffset(0x10, s);
    SetL(s + 0xC, 0xFFFD0000u);
    SetL(Sc() + 0x10, 0);
    Turn();
    s = Sc();
    VertexOffset(0x18, s);
    if (Long(s + 0x14) <= 1) return;
    std::int32_t i = 1;
    for (std::int32_t y = -0x8000;; y -= 0x8000) {
        if (y <= -0x1C0000) return;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, abr);
        SetVW(2, VW(0x12));
        SetVW(0, VW(0x10));
        SetVW(4, VW(0x14));
        SetVW(0xA, VW(0x1A));
        SetVW(8, VW(0x18));
        SetVW(0xC, VW(0x1C));
        s = Sc();
        {
            const std::int32_t d = SubL(Long(s + 0x14), i);
            SetLong(s + 0xC, d < 0x20 ? Shl(d, 0xE) : 0x80000);
        }
        SetLong(Sc() + 0x10, y);
        Turn();
        s = Sc();
        VertexOffset(0x10, s);
        {
            const std::int32_t length = Long(s + 0x14);
            SetL(s + 0xC, SubL(length, i) < 8 ? static_cast<std::uint32_t>(Shl(MulL(SubL(i, length), 3), 0xD)) : 0xFFFD0000u);
        }
        SetLong(Sc() + 0x10, y);
        Turn();
        s = Sc();
        VertexOffset(0x18, s);
        SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(1, abr, 0x340, 0x100) & 0xFFFF);
        SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1E2) & 0xFFFF);
        SetWord(Mem(kS), (static_cast<unsigned>(i) & 0xF) * 12 + 0x40);
        p[0x14] = 0;
        p[0x24] = 0xC4;
        p[0x15] = SB(0);
        p[0x25] = SB(0);
        p[0x34] = 0;
        p[0x44] = 0xC4;
        p[0x35] = static_cast<unsigned char>(SB(0) + 0xB);
        p[0x45] = static_cast<unsigned char>(SB(0) + 0xB);
        Average4(p);
        const unsigned grey = Sc()[9] * 8u;
        SetWord(Mem(kS + 2), grey & 0xFFFF);
        p[4] = static_cast<unsigned char>(grey);
        p[5] = SB(2);
        p[6] = SB(2);
        MH_CALL(Gfx_CommitPrim)(5, 0x48);
        ++i;
        if (!(i < Long(Sc() + 0x14))) return;
    }
}

// original 0x4D8DE0: the beam's glow - a draw mode (tpage 0x55) to slot 5;
// then as the core's steps, gouraud quads (semi-transparent) between the
// points turned from (-0x30000, depth) and (-0x50000, depth) - narrowing over
// the last eight steps - grey +9 x 4 at the inner edge, black at the outer;
// committed to slot 5.
S26_EXPORT void __cdecl Magic114_DrawBeamGlow(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetL(Sc() + 0xC, 0xFFFD0000u);
    SetL(Sc() + 0x10, 0);
    Turn();
    unsigned char* s = Sc();
    SetVW(8, Pos9(AddL(Long(s + 0x34), Long(s + 0xC))));
    SetVW(0xA, Pos9(AddL(Long(s + 0x38), Long(s + 0x10))));
    SetL(s + 0xC, 0xFFFB0000u);
    SetL(Sc() + 0x10, 0);
    Turn();
    s = Sc();
    SetVW(0x18, Pos9(AddL(Long(s + 0x34), Long(s + 0xC))));
    SetVW(0x1A, Pos9(AddL(Long(s + 0x38), Long(s + 0x10))));
    {
        const unsigned h = NegHalf(S16(s + 0x3E));
        SetVW(0x1C, h);
        SetVW(0x14, h);
        SetVW(0xC, h);
        SetVW(4, h);
    }
    if (Long(s + 0x14) <= 1) return;
    std::int32_t i = 1;
    for (std::int32_t y = -0x8000;; y -= 0x8000) {
        if (y <= -0x1C0000) return;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetVW(0, VW(8));
        SetVW(0x10, VW(0x18));
        SetVW(2, VW(0xA));
        SetVW(0x12, VW(0x1A));
        s = Sc();
        {
            const std::int32_t length = Long(s + 0x14);
            SetL(s + 0xC, SubL(length, i) < 8 ? static_cast<std::uint32_t>(Shl(MulL(SubL(i, length), 3), 0xD)) : 0xFFFD0000u);
        }
        SetLong(Sc() + 0x10, y);
        Turn();
        s = Sc();
        SetVW(8, Pos9(AddL(Long(s + 0x34), Long(s + 0xC))));
        SetVW(0xA, Pos9(AddL(Long(s + 0x38), Long(s + 0x10))));
        {
            const std::int32_t length = Long(s + 0x14);
            SetL(s + 0xC, SubL(length, i) < 8 ? static_cast<std::uint32_t>(Shl(MulL(SubL(i, length), 5), 0xD)) : 0xFFFB0000u);
        }
        SetLong(Sc() + 0x10, y);
        Turn();
        s = Sc();
        SetVW(0x1A, Pos9(AddL(Long(s + 0x38), Long(s + 0x10))));
        SetVW(0x18, Pos9(AddL(Long(s + 0x34), Long(s + 0xC))));
        Average4(p);
        const unsigned grey = Sc()[9] * 4u;
        SetWord(Mem(kS + 2), grey & 0xFFFF);
        p[4] = static_cast<unsigned char>(grey);
        p[5] = SB(2);
        p[6] = SB(2);
        p[0x14] = SB(2);
        p[0x15] = SB(2);
        p[0x16] = SB(2);
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        p[0x34] = 1;
        p[0x35] = 1;
        p[0x36] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
        ++i;
        if (!(i < Long(Sc() + 0x14))) return;
    }
}

// original 0x4D90D0: the sprite child, with the effects' frame-offset table:
// a call through Magic114_SpritePhases (two entries) by +2, unchecked.
S26_EXPORT void __cdecl Magic114_SpriteRun(void) {
    static constexpr std::uint32_t kPhases[2] = {addr::Magic114_SpriteStart, addr::Magic114_SpriteTick};
    SetL(Mem(kFrameSet), 0x8E3580);
    const unsigned phase = Sc()[2];
    if (phase >= 2) PastTable("Magic114_SpriteRun", phase, 2);
    magic_harness::Phase(kPhases[phase])();
    SetL(Mem(kFrameSet), 0x8B3580);
}

// original 0x4D9100: the sprite set up (texture slot 0x1D, CLUT row 0x1A,
// flip from the direction byte), +9 0, +2 on, animation 0, and its screen
// point (a tail jmp to Sprite_UpdateScreen).
S26_EXPORT void __cdecl Magic114_SpriteStart(void) {
    unsigned char* const s = Sc();
    s[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x27] = 0x1A;
    Sc()[0x28] = 1;
    {
        unsigned char* const t = Sc();
        t[0x2A] = static_cast<unsigned char>((t[8] - 1) & 1);
    }
    Sc()[0x29] = 4;
    Sc()[0x24] = 0;
    SetWord(Sc() + 0x2C, 0);
    Sc()[0x2B] = 0;
    Sc()[9] = 0;
    Inc(Sc()[2]);
    MH_CALL(Sprite_SetAnimation)(0);
    MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4D9180: the sprite's script ticked; at its end the task freed,
// else its screen point (both tail jmps).
S26_EXPORT void __cdecl Magic114_SpriteTick(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() != 0)
        MH_CALL(BattleTask_FreeCurrent)();
    else
        MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4D91A0: with +9 above 0x18, the rays - under a matrix at the
// owner (Magic114_PushRayMatrix), 64 lines round a circle of radius
// (~+9 & 0x1F) x 16, grey 0x80 - radius x 16, semi-transparent, each linked at
// the task's position one row nearer (-2) when the direction byte is above 1.
S26_EXPORT void __cdecl Magic114_DrawRays(void) {
    if (Sc()[9] <= 0x18) return;
    Call0(addr::Magic114_PushRayMatrix);
    const unsigned char* const s = Sc();
    const unsigned radius = static_cast<unsigned char>(~s[9]) & 0x1Fu;
    const int dy = s[8] > 1 ? 0xFE : 0;   // a byte: the callee reads it signed
    const unsigned long x = static_cast<unsigned long>(static_cast<std::uint32_t>(Long(s + 0x34)));
    const unsigned long z = static_cast<unsigned long>(static_cast<std::uint32_t>(Long(s + 0x38)));
    const auto grey = static_cast<unsigned char>(0x80 - (radius << 4));
    const auto scaled = [radius](int v) { return (static_cast<std::uint32_t>(v) * radius) << 4 >> 0xC; };
    for (unsigned angle = 0; angle < 0x1000;) {
        DrawMode(0xB5);
        MH_CALL(MapView_LinkPrimAt)(x, z, dy, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetVW(0, scaled(MH_CALL(Math_Cos)(static_cast<int>(angle))));
        SetVW(2, scaled(MH_CALL(Math_Sin)(static_cast<int>(angle))));
        SetVW(4, 0);
        Rtp1(p + 8);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        angle += 0x40;
        SetVW(0, scaled(MH_CALL(Math_Cos)(static_cast<int>(angle))));
        SetVW(2, scaled(MH_CALL(Math_Sin)(static_cast<int>(angle))));
        SetVW(4, 0);
        Rtp1(p + 0x14);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x1C));
        p[4] = grey;
        p[5] = grey;
        p[6] = grey;
        MH_CALL(MapView_LinkPrimAt)(x, z, dy, 0x20);
    }
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D9320: the rays' matrix - the task's offset (0xC000, 0) turned,
// its position the owner's moved by it, the height the owner's + 0x200; the
// matrix pushed there, turned 0x400 about x (direction byte odd) or y.
S26_EXPORT void __cdecl Magic114_PushRayMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    unsigned char* s = Sc();
    const bool odd = (s[8] & 1) != 0;
    const short rot[4] = {static_cast<short>(odd ? 0x400 : 0), static_cast<short>(odd ? 0 : 0x400), 0, 0};
    SetL(s + 0xC, 0xC000);
    SetL(Sc() + 0x10, 0);
    Turn();
    unsigned char* owner = Owner();
    s = Sc();
    SetLong(s + 0x34, AddL(Long(owner + 0x34), Long(s + 0xC)));
    owner = Owner();
    s = Sc();
    SetLong(s + 0x38, AddL(Long(owner + 0x38), Long(s + 0x10)));
    owner = Owner();
    s = Sc();
    SetWord(s + 0x3E, (Word(owner + 0x3E) + 0x200) & 0xFFFF);
    s = Sc();
    short v[4];
    v[0] = static_cast<short>(Pos9(Long(s + 0x34)));
    v[1] = static_cast<short>(Pos9(Long(s + 0x38)));
    v[2] = static_cast<short>(NegHalf(S16(s + 0x3E)));
    v[3] = 0;
    PushMatrixAt(rot, v);
}

// ===========================================================================
// MAGIC115 (row 115, Remedy read one id down)

// original 0x4D9440: the kind-2 task. A four-entry stack table by +1:
// Magic115_Start, Magic115_TintUp, Magic115_TintDown, BattleFx_Finish.
S26_EXPORT void __cdecl Magic115_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::Magic115_Start, addr::Magic115_TintUp, addr::Magic115_TintDown,
                                                 addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Magic115_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D9480: at the source sprite (read once); a child (kind 1, task
// 0x30, +1 0) owned by this task; the source's tints released and a tint (0,
// 0, 0, 1) set, kept in +0xA; CLUT row 26's first sixteen from its source with
// the STP bit, its first again without; Gfx_ClutStripDirty; sound 0x100; +0xB
// 1, +9 0, +1 on.
S26_EXPORT void __cdecl Magic115_Start(void) {
    unsigned char* const source = Source();
    SetLong(Sc() + 0x34, Long(source + 0x34));
    SetLong(Sc() + 0x38, Long(source + 0x38));
    SetLong(Sc() + 0x3C, Long(source + 0x3C));
    const unsigned slot = NewTask(0x30);
    unsigned char* const child = TaskSlot(slot);
    SetL(child + 0x80, Key(Sc()));
    child[1] = 0;
    MH_CALL(Sprite_ReleaseTint)(source);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(source, 0, 0, 0, 1);
    Sc()[0xA] = tint;
    for (unsigned k = 0; k < 16; ++k)
        Gfx_ClutStrip[0x1A00 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x1A00 + k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = Gfx_ClutStripSource[0x1A00];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[0xB] = 1;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4D9560: the tint record +0xA's colour bytes +2..+4 = +9 x 2 (the
// child counts +9 up); from 6 on, +1 on.
S26_EXPORT void __cdecl Magic115_TintUp(void) {
    unsigned char* const s = Sc();
    TintRecord(s[0xA])[2] = static_cast<unsigned char>(s[9] << 1);
    TintRecord(s[0xA])[3] = static_cast<unsigned char>(s[9] << 1);
    TintRecord(s[0xA])[4] = static_cast<unsigned char>(s[9] << 1);
    if (s[9] >= 6) Inc(s[1]);
}

// original 0x4D95B0: on odd frames the tint record's colour down one; at red 0
// the record released (Tint_Release) and +1 on.
S26_EXPORT void __cdecl Magic115_TintDown(void) {
    unsigned char* const s = Sc();
    if ((Frame_Counter & 1) != 0) {
        Dec(TintRecord(s[0xA])[2]);
        Dec(TintRecord(s[0xA])[3]);
        Dec(TintRecord(s[0xA])[4]);
    }
    const unsigned char index = s[0xA];
    if (TintRecord(index)[2] != 0) return;
    MH_CALL(Tint_Release)(index);
    Inc(Sc()[1]);
}

// original 0x4D9630: the child's kind-1 task (parameter 0x30), a jmp through
// Magic115_ChildKinds (two entries) by +1, unchecked.
S26_EXPORT void __cdecl Magic115_ChildTask(void) {
    static constexpr std::uint32_t kKinds[2] = {addr::Magic115_OrbitRun, addr::Magic115_MoteRun};
    const unsigned kind = Sc()[1];
    if (kind >= 2) PastTable("Magic115_ChildTask", kind, 2);
    magic_harness::Phase(kKinds[kind])();
}

// original 0x4D9650: with the effects' frame-offset table, a call through
// Magic115_OrbitPhases (four entries, the last MAGIC058's 0x4AF490) by +2,
// unchecked; while +0 and +2 are set, the sprite's screen point.
S26_EXPORT void __cdecl Magic115_OrbitRun(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::Magic115_OrbitStart, addr::Magic115_Orbit,
                                                 addr::Magic115_OrbitFade, addr::MagicFx_UncountAndFree};
    SetL(Mem(kFrameSet), 0x8E3580);
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("Magic115_OrbitRun", phase, 4);
    magic_harness::Phase(kPhases[phase])();
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetL(Mem(kFrameSet), 0x8B3580);
}

// The sprite set-up both MAGIC115 children share (texture slot 0x1D, CLUT row
// 0xA0, scale 0x10000 both ways, colour 0), all but the animation.
static void Magic115_SpriteSetUp() {
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    SetL(Sc() + 0x40, 0x10000);
    SetL(Sc() + 0x44, 0x10000);
    Sc()[0x48] = 1;
    Sc()[0x27] = 0xA0;
    Sc()[0x28] = 0;
    Sc()[0x24] = 4;
    Sc()[0x5D] = 0;
    Sc()[0x5E] = 0;
    Sc()[0x5F] = 0;
    Sc()[0x5C] = 0;
    Sc()[0x2A] = 0;
    Sc()[0x29] = 4;
    SetWord(Sc() + 0x2C, 0);
    Sc()[0x2B] = 0;
}

// original 0x4D9690: at the owner (height + 0x2C0), the sprite set up,
// animation 0, +9 0xFF, +2 on.
S26_EXPORT void __cdecl Magic115_OrbitStart(void) {
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetWord(Sc() + 0x3E, (Word(Owner() + 0x3E) + 0x2C0) & 0xFFFF);
    Magic115_SpriteSetUp();
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 0xFF;
    Inc(Sc()[2]);
}

// The orbit step both circling phases share: +9 up, the angle (+9 & 0x3F) <<
// 6 kept in DamageScratch's dword, the position the owner's plus 3 x (sin,
// cos) << 2.
static void Magic115_Circle() {
    Inc(Sc()[9]);
    const std::uint32_t angle = (Sc()[9] & 0x3Fu) << 6;
    SetL(Mem(kS), angle);
    {
        const int sn = MH_CALL(Math_Sin)(static_cast<int>(angle));
        const unsigned char* const owner = Owner();
        SetLong(Sc() + 0x34, AddL(Shl(MulL(sn, 3), 0xE) >> 0xC, Long(owner + 0x34)));
    }
    {
        const int c = MH_CALL(Math_Cos)(Long(Mem(kS)));
        unsigned char* const s = Sc();
        const unsigned char* const owner = Owner();
        SetLong(s + 0x38, AddL(Shl(MulL(c, 3), 0xE) >> 0xC, Long(owner + 0x38)));
    }
}

// original 0x4D9790: the sprite's script ticked; the orbit step; every eighth
// step a mote (kind 1, task 0x30, +1 1, owned by the owner, its delay +9 from
// the step) at the sprite, the owner's +9 and +0xB counting it; at step 0x40
// the sprite's bit 0x20, its colour (1, 0xC0, 0xC0, 0xC0), +2 on.
S26_EXPORT void __cdecl Magic115_Orbit(void) {
    MH_CALL(Sprite_ScriptTick)();
    Magic115_Circle();
    unsigned char* s = Sc();
    if ((s[9] & 7) == 0) {
        const unsigned slot = NewTask(0x30);
        unsigned char* const owner = Owner();
        unsigned char* const child = TaskSlot(slot);
        s = Sc();
        SetL(child + 0x80, Key(owner));
        child[1] = 1;
        child[9] = static_cast<unsigned char>(((s[9] >> 2) + 2) << 2);
        SetLong(child + 0x34, Long(s + 0x34));
        SetLong(child + 0x38, Long(s + 0x38));
        SetLong(child + 0x3C, Long(s + 0x3C));
        Inc(owner[9]);
        Inc(Owner()[0xB]);
        s = Sc();
    }
    if (s[9] != 0x40) return;
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    Sc()[0x5C] = 1;
    Sc()[0x5D] = 0xC0;
    Sc()[0x5E] = 0xC0;
    Sc()[0x5F] = 0xC0;
    Inc(Sc()[2]);
}

// original 0x4D98E0: the colour down 4; the script ticked; the orbit step;
// red 0x80, +2 on.
S26_EXPORT void __cdecl Magic115_OrbitFade(void) {
    AddB(Sc()[0x5D], 0xFC);
    AddB(Sc()[0x5E], 0xFC);
    AddB(Sc()[0x5F], 0xFC);
    MH_CALL(Sprite_ScriptTick)();
    Magic115_Circle();
    Sc()[0x5D] = 0x80;
    Inc(Sc()[2]);
}

// original 0x4D9990: the mote, with the effects' frame-offset table: a call
// through Magic115_MotePhases (four entries, the last MAGIC058's 0x4AF490) by
// +2, unchecked; while +0 and +2 are set, its screen point.
S26_EXPORT void __cdecl Magic115_MoteRun(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::Magic115_MoteStart, addr::Magic115_MoteTick,
                                                 addr::Magic115_MoteFade, addr::MagicFx_UncountAndFree};
    SetL(Mem(kFrameSet), 0x8E3580);
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("Magic115_MoteRun", phase, 4);
    magic_harness::Phase(kPhases[phase])();
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetL(Mem(kFrameSet), 0x8B3580);
}

// original 0x4D99D0: the sprite set up, animation 1, +2 on.
S26_EXPORT void __cdecl Magic115_MoteStart(void) {
    Magic115_SpriteSetUp();
    MH_CALL(Sprite_SetAnimation)(1);
    Inc(Sc()[2]);
}

// original 0x4D9A80: the script ticked twice; at its end (the second) +2 on by
// two (past the fade).
S26_EXPORT void __cdecl Magic115_MoteTick(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    if (MH_CALL(Sprite_ScriptTickOnce)() != 0) AddB(Sc()[2], 2);
}

// original 0x4D9AA0: the colour down 4, the script ticked, red 0x80, +2 on.
S26_EXPORT void __cdecl Magic115_MoteFade(void) {
    AddB(Sc()[0x5D], 0xFC);
    AddB(Sc()[0x5E], 0xFC);
    AddB(Sc()[0x5F], 0xFC);
    MH_CALL(Sprite_ScriptTickOnce)();
    Sc()[0x5D] = 0x80;
    Inc(Sc()[2]);
}

// ===========================================================================
// MAGIC117 (rows 73 and 106: Rest / Snooze and Douse read one id down)

// The tint record +0xB's colour bytes +2..+4 moved by one (the index read
// again for each).
static void TintStep(int by) {
    const unsigned char* const s = Sc();
    AddB(TintRecord(s[0xB])[2], static_cast<unsigned>(by));
    AddB(TintRecord(s[0xB])[3], static_cast<unsigned>(by));
    AddB(TintRecord(s[0xB])[4], static_cast<unsigned>(by));
}

// original 0x4D9F40: row 73's kind-2 task. A four-entry stack table by +1:
// Magic117_TintSet, Magic117_Brighten, Magic117_Dim, and group E's 0x43F460
// (the target's flag 0x40, the done flag, free).
S26_EXPORT void __cdecl Magic117_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::Magic117_TintSet, addr::Magic117_Brighten, addr::Magic117_Dim,
                                                 addr::MagicFx_FlagTargetEnd};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Magic117_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D9F80: the source sprite's tints released and a tint (0, 0, 0,
// 1) set on it (the source read again), kept in +0xB; +9 0, +0xA 2 (pulses),
// +1 on.
S26_EXPORT void __cdecl Magic117_TintSet(void) {
    MH_CALL(Sprite_ReleaseTint)(Source());
    const unsigned char tint = MH_CALL(Sprite_SetTint)(Source(), 0, 0, 0, 1);
    Sc()[0xB] = tint;
    Sc()[9] = 0;
    Sc()[0xA] = 2;
    Inc(Sc()[1]);
}

// original 0x4D9FD0 (also MAGIC010's): the tint up one; +9 up, at 0x10 +1 on.
S26_EXPORT void __cdecl Magic117_Brighten(void) {
    TintStep(1);
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (Sc()[9] == 0x10) Inc(Sc()[1]);
}

// original 0x4DA040: the tint down one; +9 down; at 0 the pulses +0xA down -
// one left: +1 back to Magic117_Brighten; none: the tint released, the
// target flashed, +1 on.
S26_EXPORT void __cdecl Magic117_Dim(void) {
    TintStep(-1);
    unsigned char* s = Sc();
    Dec(s[9]);
    s = Sc();
    if (s[9] != 0) return;
    Dec(s[0xA]);
    s = Sc();
    if (s[0xA] != 0) {
        Dec(s[1]);
        return;
    }
    MH_CALL(Sprite_ReleaseTint)(Source());
    MH_CALL(BattleActor_Flash)(TargetByte());
    Inc(Sc()[1]);
}

// original 0x4DA0E0: row 106's kind-2 task. A four-entry stack table by +1:
// group C1's 0x49DF30 (MAGIC010: the tint set), Magic117_Darken,
// Magic117_Lighten, group E's 0x43F460.
S26_EXPORT void __cdecl Magic117_TaskB(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::WhiteFlag_TintSource, addr::Magic117_Darken,
                                                 addr::Magic117_Lighten, addr::MagicFx_FlagTargetEnd};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Magic117_TaskB", phase, 4);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4DA120: the tint down one; +9 up, at 0x10 +1 on.
S26_EXPORT void __cdecl Magic117_Darken(void) {
    TintStep(-1);
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (Sc()[9] == 0x10) Inc(Sc()[1]);
}

// original 0x4DA190: the tint up one; +9 down; at 0 the tint released, the
// target flashed, +1 on.
S26_EXPORT void __cdecl Magic117_Lighten(void) {
    TintStep(1);
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    MH_CALL(BattleActor_Flash)(TargetByte());
    Inc(Sc()[1]);
}

void MagicS26_Inject() {
    if (bof3::WantsShadow("magic_s26")) magic_s26::SelfTest();
    BOF3_INJECT(Magic114_Task);
    BOF3_INJECT(Magic114_Start);
    BOF3_INJECT(Magic114_Grow);
    BOF3_INJECT(Magic114_Hold);
    BOF3_INJECT(Magic114_Launch);
    BOF3_INJECT(Magic114_Rays);
    BOF3_INJECT(Magic114_Shrink);
    BOF3_INJECT(Magic114_End);
    BOF3_INJECT(Magic114_DrawRing);
    BOF3_INJECT(Magic114_DrawTriangle);
    BOF3_INJECT(Magic114_PushMatrix);
    BOF3_INJECT(Magic114_ChildTask);
    BOF3_INJECT(Magic114_TrailRun);
    BOF3_INJECT(Magic114_TrailAim);
    BOF3_INJECT(Magic114_TrailSpawn);
    BOF3_INJECT(Magic114_DrawTrail);
    BOF3_INJECT(Magic114_BeamRun);
    BOF3_INJECT(Magic114_BeamWait);
    BOF3_INJECT(Magic114_BeamGrow);
    BOF3_INJECT(Magic114_BeamFade);
    BOF3_INJECT(Magic114_DrawBeamCore);
    BOF3_INJECT(Magic114_DrawBeamStrip);
    BOF3_INJECT(Magic114_DrawBeamGlow);
    BOF3_INJECT(Magic114_SpriteRun);
    BOF3_INJECT(Magic114_SpriteStart);
    BOF3_INJECT(Magic114_SpriteTick);
    BOF3_INJECT(Magic114_DrawRays);
    BOF3_INJECT(Magic114_PushRayMatrix);
    BOF3_INJECT(Magic115_Task);
    BOF3_INJECT(Magic115_Start);
    BOF3_INJECT(Magic115_TintUp);
    BOF3_INJECT(Magic115_TintDown);
    BOF3_INJECT(Magic115_ChildTask);
    BOF3_INJECT(Magic115_OrbitRun);
    BOF3_INJECT(Magic115_OrbitStart);
    BOF3_INJECT(Magic115_Orbit);
    BOF3_INJECT(Magic115_OrbitFade);
    BOF3_INJECT(Magic115_MoteRun);
    BOF3_INJECT(Magic115_MoteStart);
    BOF3_INJECT(Magic115_MoteTick);
    BOF3_INJECT(Magic115_MoteFade);
    BOF3_INJECT(Magic117_Task);
    BOF3_INJECT(Magic117_TintSet);
    BOF3_INJECT(Magic117_Brighten);
    BOF3_INJECT(Magic117_Dim);
    BOF3_INJECT(Magic117_TaskB);
    BOF3_INJECT(Magic117_Darken);
    BOF3_INJECT(Magic117_Lighten);
}
