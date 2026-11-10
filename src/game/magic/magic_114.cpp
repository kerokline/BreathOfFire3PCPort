// Moved here by tools/move_funcs.py from magic_s26.cpp (docs/source-layout-round.md).
#include "game/magic/magic_114.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// SharedScratch's first dword (0x903850; a word at +0 and +2 in the beams)
// and Prim_VertexScratch's four SVECTORs (0x9037A0..0x9037BF): read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

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

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Rotates the dx / dz pair +0xC / +0x10 of the task it is given by its
// direction byte +8 - Capcom's, unnamed, in no group (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

void Turn() { MH_AT(void (__cdecl*)(unsigned char*), kTurnOffset.v)(Sc()); }

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
    SetVW(k, Pos9(AddL(rv::As<records::SpriteHead>(s)->x, rv::As<records::SpriteHead>(s)->step_x)));
    SetVW(k + 2, Pos9(AddL(rv::As<records::SpriteHead>(s)->z, rv::As<records::SpriteHead>(s)->step_z)));
    SetVW(k + 4, NegHalf(S16(s + 0x3E)));
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

}  // namespace

#define S26_EXPORT extern "C" __attribute__((disable_tail_calls))

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
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
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
    Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
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
        rv::Store(rv::As<records::BattleTask>(s)->step_x, MulL(Magic114_RingOffsets[row], s[0xA]));
        rv::Store(rv::As<records::BattleTask>(s)->step_z, MulL(Magic114_RingOffsets[row + 1], s[0xA]));
        Turn();
        s = Sc();
        rv::Store(rv::As<records::BattleTask>(s)->x, rv::As<records::BattleTask>(s)->step_x);
        rv::Store(rv::As<records::BattleTask>(s)->z, rv::As<records::BattleTask>(s)->step_z);
        rv::Store(rv::As<records::BattleTask>(s)->step_x, MulL(Magic114_RingOffsets[row + 2], s[0xA]));
        rv::Store(rv::As<records::BattleTask>(s)->step_z, MulL(Magic114_RingOffsets[row + 3], s[0xA]));
        Turn();
        Sc()[0xB] = static_cast<unsigned char>(i);
        MH_AT(void (__cdecl*)(int*), addr::Magic114_DrawTriangle)(keys);
    }
    const unsigned char* const owner = Owner();
    MH_CALL(MagicFx_LinkByDepth)(static_cast<unsigned>(rv::As<records::SpriteHead>(owner)->x), static_cast<unsigned>(rv::As<records::SpriteHead>(owner)->z), keys,
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
    SetVW(0, static_cast<unsigned>(rv::As<records::BattleTask>(s)->x >> 9));
    SetVW(2, static_cast<unsigned>(rv::As<records::BattleTask>(s)->z >> 9));
    SetVW(4, 0);
    SetVW(8, static_cast<unsigned>(rv::As<records::BattleTask>(s)->step_x >> 9));
    SetVW(0xA, static_cast<unsigned>(rv::As<records::BattleTask>(s)->step_z >> 9));
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
    v[0] = static_cast<short>(Pos9(rv::As<records::SpriteHead>(owner)->x));
    v[1] = static_cast<short>(Pos9(rv::As<records::SpriteHead>(owner)->z));
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
    rv::Store(rv::As<records::BattleTask>(s)->step_y, S16(s + 0x3E));
    rv::Store(rv::As<records::BattleTask>(s)->step_x, Shl(Magic114_RingOffsets[4], 5));
    rv::Store(rv::As<records::BattleTask>(s)->step_z, Shl(Magic114_RingOffsets[5], 5));
    Turn();
    unsigned char* owner = Owner();
    s = Sc();
    rv::Store(rv::As<records::BattleTask>(s)->x, AddL(rv::As<records::SpriteHead>(owner)->x, rv::As<records::BattleTask>(s)->step_x));
    owner = Owner();
    s = Sc();
    rv::Store(rv::As<records::BattleTask>(s)->z, AddL(rv::As<records::SpriteHead>(owner)->z, rv::As<records::BattleTask>(s)->step_z));
    owner = Owner();
    s = Sc();
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->height_whole, (rv::As<records::SpriteHead>(owner)->height_whole + 0x200) & 0xFFFF);
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
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        unsigned char* const owner = Owner();
        SetL(child + 0x80, Key(owner));
        child[1] = 2;
        child[8] = owner[8];
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        tchild->x = AddL(ts->step_x, kx);
        tchild->z = AddL(ts->step_z, Field_Kind2Z);
        rv::Store<unsigned>(tchild->height_whole, static_cast<std::uint16_t>(ts->step_y));
    }
    unsigned char* s = Sc();
    if (s[9] == 0) {
        const unsigned slot = NewTask(7);
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        unsigned char* const owner = Owner();
        child[1] = 1;
        SetL(child + 0x80, Key(owner));
        s = Sc();
        child[8] = s[8];
        tchild->step_x = rv::As<records::BattleTask>(s)->step_x;
        tchild->step_z = rv::As<records::BattleTask>(s)->step_z;
        rv::Store<unsigned>(tchild->height_whole, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_y));
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
    const std::int32_t x = AddL(rv::As<records::BattleTask>(s)->step_x, Field_Kind2X);
    const std::int32_t z = AddL(rv::As<records::BattleTask>(s)->step_z, Field_Kind2Z);
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
    SetVW(0x18, Pos9(AddL(rv::As<records::BattleTask>(s)->step_x, Field_Kind2X)));
    SetVW(0x1A, Pos9(AddL(rv::As<records::BattleTask>(s)->step_z, Field_Kind2Z)));
    SetVW(0x1C, NegHalf(rv::As<records::BattleTask>(s)->step_y));
    SetVW(0x10, Pos9(x));
    SetVW(0x12, Pos9(z));
    SetVW(0x14, NegHalf(rv::As<records::BattleTask>(s)->step_y));
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
    rv::Store(rv::As<records::BattleTask>(s)->x, AddL(rv::As<records::BattleTask>(s)->step_x, Field_Kind2X));
    s = Sc();
    rv::Store(rv::As<records::BattleTask>(s)->z, AddL(rv::As<records::BattleTask>(s)->step_z, Field_Kind2Z));
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0);
    rv::Store(rv::Current<records::BattleTask>()->work_20, 0);
    Inc(Sc()[2]);
}

// original 0x4D8600: the scroll +0x20 down 5; at length 0x30 the target's flag
// 0x10; the length +0x14 up 2, and at 0xC0 +0xB 1 (additive) and +2 on.
S26_EXPORT void __cdecl Magic114_BeamGrow(void) {
    unsigned char* s = Sc();
    rv::Store(rv::As<records::BattleTask>(s)->work_20, AddL(rv::As<records::BattleTask>(s)->work_20, -5));
    s = Sc();
    if (rv::As<records::BattleTask>(s)->step_y == 0x30) {
        MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
        s = Sc();
    }
    rv::Store(rv::As<records::BattleTask>(s)->step_y, AddL(rv::As<records::BattleTask>(s)->step_y, 2));
    s = Sc();
    if (rv::As<records::BattleTask>(s)->step_y == 0xC0) {
        s[0xB] = 1;
        Inc(Sc()[2]);
    }
}

// original 0x4D8660: the scroll down 5, the length up 2, +9 down; at 0 the
// task freed (a tail jmp).
S26_EXPORT void __cdecl Magic114_BeamFade(void) {
    unsigned char* s = Sc();
    rv::Store(rv::As<records::BattleTask>(s)->work_20, AddL(rv::As<records::BattleTask>(s)->work_20, -5));
    s = Sc();
    rv::Store(rv::As<records::BattleTask>(s)->step_y, AddL(rv::As<records::BattleTask>(s)->step_y, 2));
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
    if (rv::As<records::BattleTask>(s)->step_y <= 1) return;
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
            const std::int32_t d = SubL(rv::As<records::BattleTask>(s)->step_y, i);
            rv::Store(rv::As<records::BattleTask>(s)->step_x, d < 0x20 ? Shl(d, 0xE) : 0x80000);
        }
        rv::Current<records::BattleTask>()->step_z = y;
        Turn();
        s = Sc();
        VertexOffset(0x10, s);
        {
            const std::int32_t length = rv::As<records::BattleTask>(s)->step_y;
            SetL(s + 0xC, SubL(length, i) < 8 ? static_cast<std::uint32_t>(Shl(MulL(SubL(i, length), 3), 0xD)) : 0xFFFD0000u);
        }
        rv::Current<records::BattleTask>()->step_z = y;
        Turn();
        s = Sc();
        VertexOffset(0x18, s);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100) & 0xFFFF);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E2) & 0xFFFF);
        s = Sc();
        SetWord(Mem(::bof3::Img(kS.v)), (static_cast<std::uint32_t>(AddL(rv::As<records::BattleTask>(s)->work_20, i * 4)) % 0xBC + 0x40) & 0xFFFF);
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
            SetWord(Mem(::bof3::Img(kS.v) + 2), static_cast<unsigned>(grey) & 0xFFFF);
            p[4] = static_cast<unsigned char>(grey);
        } else {
            const unsigned grey = s[9] * 10u;
            SetWord(Mem(::bof3::Img(kS.v) + 2), grey & 0xFFFF);
            p[4] = static_cast<unsigned char>(grey);
        }
        p[5] = SB(2);
        p[6] = SB(2);
        MH_CALL(Gfx_CommitPrim)(3, 0x48);
        ++i;
        if (!(i < rv::Current<records::BattleTask>()->step_y)) return;
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
    if (rv::As<records::BattleTask>(s)->step_y <= 1) return;
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
            const std::int32_t d = SubL(rv::As<records::BattleTask>(s)->step_y, i);
            rv::Store(rv::As<records::BattleTask>(s)->step_x, d < 0x20 ? Shl(d, 0xE) : 0x80000);
        }
        rv::Current<records::BattleTask>()->step_z = y;
        Turn();
        s = Sc();
        VertexOffset(0x10, s);
        {
            const std::int32_t length = rv::As<records::BattleTask>(s)->step_y;
            SetL(s + 0xC, SubL(length, i) < 8 ? static_cast<std::uint32_t>(Shl(MulL(SubL(i, length), 3), 0xD)) : 0xFFFD0000u);
        }
        rv::Current<records::BattleTask>()->step_z = y;
        Turn();
        s = Sc();
        VertexOffset(0x18, s);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(1, abr, 0x340, 0x100) & 0xFFFF);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E2) & 0xFFFF);
        SetWord(Mem(::bof3::Img(kS.v)), (static_cast<unsigned>(i) & 0xF) * 12 + 0x40);
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
        SetWord(Mem(::bof3::Img(kS.v) + 2), grey & 0xFFFF);
        p[4] = static_cast<unsigned char>(grey);
        p[5] = SB(2);
        p[6] = SB(2);
        MH_CALL(Gfx_CommitPrim)(5, 0x48);
        ++i;
        if (!(i < rv::Current<records::BattleTask>()->step_y)) return;
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
    SetVW(8, Pos9(AddL(rv::As<records::BattleTask>(s)->x, rv::As<records::BattleTask>(s)->step_x)));
    SetVW(0xA, Pos9(AddL(rv::As<records::BattleTask>(s)->z, rv::As<records::BattleTask>(s)->step_z)));
    SetL(s + 0xC, 0xFFFB0000u);
    SetL(Sc() + 0x10, 0);
    Turn();
    s = Sc();
    SetVW(0x18, Pos9(AddL(rv::As<records::BattleTask>(s)->x, rv::As<records::BattleTask>(s)->step_x)));
    SetVW(0x1A, Pos9(AddL(rv::As<records::BattleTask>(s)->z, rv::As<records::BattleTask>(s)->step_z)));
    {
        const unsigned h = NegHalf(S16(s + 0x3E));
        SetVW(0x1C, h);
        SetVW(0x14, h);
        SetVW(0xC, h);
        SetVW(4, h);
    }
    if (rv::As<records::BattleTask>(s)->step_y <= 1) return;
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
            const std::int32_t length = rv::As<records::BattleTask>(s)->step_y;
            SetL(s + 0xC, SubL(length, i) < 8 ? static_cast<std::uint32_t>(Shl(MulL(SubL(i, length), 3), 0xD)) : 0xFFFD0000u);
        }
        rv::Current<records::BattleTask>()->step_z = y;
        Turn();
        s = Sc();
        SetVW(8, Pos9(AddL(rv::As<records::BattleTask>(s)->x, rv::As<records::BattleTask>(s)->step_x)));
        SetVW(0xA, Pos9(AddL(rv::As<records::BattleTask>(s)->z, rv::As<records::BattleTask>(s)->step_z)));
        {
            const std::int32_t length = rv::As<records::BattleTask>(s)->step_y;
            SetL(s + 0xC, SubL(length, i) < 8 ? static_cast<std::uint32_t>(Shl(MulL(SubL(i, length), 5), 0xD)) : 0xFFFB0000u);
        }
        rv::Current<records::BattleTask>()->step_z = y;
        Turn();
        s = Sc();
        SetVW(0x1A, Pos9(AddL(rv::As<records::BattleTask>(s)->z, rv::As<records::BattleTask>(s)->step_z)));
        SetVW(0x18, Pos9(AddL(rv::As<records::BattleTask>(s)->x, rv::As<records::BattleTask>(s)->step_x)));
        Average4(p);
        const unsigned grey = Sc()[9] * 4u;
        SetWord(Mem(::bof3::Img(kS.v) + 2), grey & 0xFFFF);
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
        if (!(i < rv::Current<records::BattleTask>()->step_y)) return;
    }
}

// original 0x4D90D0: the sprite child, with the effects' frame-offset table:
// a call through Magic114_SpritePhases (two entries) by +2, unchecked.
S26_EXPORT void __cdecl Magic114_SpriteRun(void) {
    static constexpr std::uint32_t kPhases[2] = {addr::Magic114_SpriteStart, addr::Magic114_SpriteTick};
    SetL(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8E3580));
    const unsigned phase = Sc()[2];
    if (phase >= 2) PastTable("Magic114_SpriteRun", phase, 2);
    magic_harness::Phase(kPhases[phase])();
    SetL(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8B3580));
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
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
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
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    const unsigned radius = static_cast<unsigned char>(~s[9]) & 0x1Fu;
    const int dy = s[8] > 1 ? 0xFE : 0;   // a byte: the callee reads it signed
    const unsigned long x = static_cast<unsigned long>(static_cast<std::uint32_t>(ts->x));
    const unsigned long z = static_cast<unsigned long>(static_cast<std::uint32_t>(ts->z));
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
    rv::Store(rv::As<records::BattleTask>(s)->x, AddL(rv::As<records::SpriteHead>(owner)->x, rv::As<records::BattleTask>(s)->step_x));
    owner = Owner();
    s = Sc();
    rv::Store(rv::As<records::BattleTask>(s)->z, AddL(rv::As<records::SpriteHead>(owner)->z, rv::As<records::BattleTask>(s)->step_z));
    owner = Owner();
    s = Sc();
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->height_whole, (rv::As<records::SpriteHead>(owner)->height_whole + 0x200) & 0xFFFF);
    s = Sc();
    short v[4];
    v[0] = static_cast<short>(Pos9(rv::As<records::BattleTask>(s)->x));
    v[1] = static_cast<short>(Pos9(rv::As<records::BattleTask>(s)->z));
    v[2] = static_cast<short>(NegHalf(S16(s + 0x3E)));
    v[3] = 0;
    PushMatrixAt(rot, v);
}

void Magic114_Inject() {
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
}
