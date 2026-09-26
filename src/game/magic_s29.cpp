// Spell group S29 of round nine (docs/magic_s29.md): two BMAGIC overlays of
// Magic_Rows compiled into the exe, 49 functions.
//
//   MAGIC125, row 125 (read one id down: DivineBreath) 0x4E0910..0x4E1996: a
//     kind-2 task with two kind-1 children (parameter 0x5C) - a beam that
//     drops from above the side's centre and widens (a disc of POLY_G3 and a
//     wall of POLY_G4), and a burst that closes in, sets the targets' 0x10
//     flag and sheds 64 motes into a pool of its own (0x69FC30, task-shaped
//     records) - each mote flying out and up, drawn as a screen-space star of
//     POLY_G3 and a ring of POLY_G4.
//   MAGIC126, row 127 (read one id down: ShadowBreath) 0x4E19A0..0x4E3254: a
//     kind-2 task with eight kind-1 seekers (parameter 0x4E) launched from the
//     acting actor, homing on the side's centre; the first to arrive makes an
//     orb (three rings of POLY_G3 / POLY_G4), a glow (two POLY_GT4 sprites),
//     sets the targets' 0x10 flag, and sheds 64 motes into a pool of 0x20-byte
//     records (0x6A1D30) that spread, lift and fade as POLY_FT4 sprites.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a direct call to another function of the group goes by its address, which
// in the game is the jmp Inject put there to ours.
//
// No divergence: each function is a faithful replacement, except that a
// dispatch past its table - a stack table's or a .data table's - aborts where
// the original would call through whatever follows (docs/magic_fx_reached.md
// section 3, the precedent). Where the original reads memory again after a
// call (Sprite_Current, the owner, the pool's current record, the scratch
// words), ours reads it again; where it holds a pointer across a call, ours
// holds it.
#include "game/magic_s29.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s29_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
namespace cell = magic_s29::cell;
namespace tbl = magic_s29::tbl;
using magic_harness::Handler;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// --- small helpers ---------------------------------------------------------

// The original's imul: 32 bits, wrapping; `sar 0xC` after it is arithmetic.
inline int Mul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
inline int Mul12(int a, int b) { return Mul(a, b) >> 12; }
inline std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
inline std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
inline short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
inline std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
inline void SetPtr(unsigned char* at_, const void* p) { SetLong(at_, static_cast<std::int32_t>(Addr(p))); }
inline void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
inline void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
inline void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// `fild dword` then `fstp dword`: an integer as a float.
inline void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
// `fld dword` then the CRT's _ftol 0x5B9550: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 - so 0 in the low word the caller keeps. As
// battle_items.cpp's.
std::uint16_t Ftol16(std::uint32_t bits) {
    float v;
    std::memcpy(&v, &bits, sizeof v);
    if (!(v > -9.2233720368547758e18f && v < 9.2233720368547758e18f)) return 0;   // NaN included
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

inline unsigned char* Sc() { return Sprite_Current; }
inline unsigned char* Owner() { return Pointer(at::kOwner); }
inline unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
inline unsigned char* TaskSlot(unsigned index) { return Mem(at::kTasks + (index & 0xFFu) * at::kTaskStride); }
inline unsigned char* Mote(unsigned index) { return Mem(cell::kMotePool + (index & 0xFFu) * cell::kMoteStride); }
inline unsigned char* Shade(unsigned index) { return Mem(cell::kShadePool + (index & 0xFFu) * cell::kShadeStride); }
inline unsigned char* Cur() { return Pointer(cell::kShadeCurrent); }

// The scratch words and vertices, read and written in place.
inline std::uint16_t SW(std::uint32_t a) { return Word(Mem(a)); }
inline short SS(std::uint32_t a) { return static_cast<short>(Word(Mem(a))); }
inline unsigned char SB(std::uint32_t a) { return Mem(a)[0]; }
inline void Put(std::uint32_t a, int v) { SetWord(Mem(a), static_cast<unsigned>(v) & 0xFFFFu); }
inline const short* Vec(std::uint32_t a) { return reinterpret_cast<const short*>(Mem(a)); }
inline float* Xy(unsigned char* prim, unsigned offset) { return reinterpret_cast<float*>(prim + offset); }

// A .data table of handlers, read in place as the original reads it (during
// the fuzz its entries are the harness's recorders).
Handler DataPhase(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}
[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// Callees.
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }
int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }
std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }
void Commit(unsigned slot, unsigned size) { MH_CALL(Gfx_CommitPrim)(slot, size); }
void Link(std::int32_t x, std::int32_t z, int dy, unsigned size) {
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), dy, size);
}
void LinkAtSc(unsigned size) {
    const unsigned char* const s = Sc();
    Link(Long(s + 0x34), Long(s + 0x38), 2, size);
}
void LinkAtCur(unsigned size) {
    const unsigned char* const r = Cur();
    Link(Long(r + 0x10), Long(r + 0x14), 2, size);
}
void Project3(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers3)(Vec(cell::kV0), Vec(cell::kV1), Vec(cell::kV2), Xy(p, 8), Xy(p, 0x18), Xy(p, 0x28), &depth);
    MH_CALL(Gte_PrimDepths3_10B)(p);
}
void Project4(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers4)(Vec(cell::kV0), Vec(cell::kV1), Vec(cell::kV2), Vec(cell::kV3), Xy(p, 8), Xy(p, 0x18),
                               Xy(p, 0x28), Xy(p, 0x38), &depth);
    MH_CALL(Gte_PrimDepths4_10B)(p);
}
// Three colour bytes from the scratch words' low bytes (red, green, blue).
void Shade3(unsigned char* p) {
    p[0] = SB(cell::kW6);
    p[1] = SB(cell::kW8);
    p[2] = SB(cell::kWA);
}
void Fill3(unsigned char* p, unsigned char v) { p[0] = p[1] = p[2] = v; }

using TaskFn = void (__cdecl*)(unsigned char*);
using AllocFn = unsigned char (__cdecl*)();
void CallOwn(std::uint32_t address) { MH_AT(Handler, address)(); }

}  // namespace

#define S29_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC125 (row 125, DivineBreath read one id down)

// original 0x4E0910: the kind-2 task. A two-entry stack table by +1
// (DivineBreath_Start, BattleFx_Finish), unchecked; then each live record of
// DivineMote_Pool run as Sprite_Current, its +0x80 the owner, through
// DivineMote_Task - Sprite_Current and the owner as they were after the
// phase put back after each.
S29_EXPORT void __cdecl DivineBreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::DivineBreath_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("DivineBreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const saved = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < cell::kMotes; ++i) {
        unsigned char* const m = Mote(i);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = Long(m + 0x80);
        Sprite_Current = m;
        SetLong(Mem(at::kOwner), its);
        CallOwn(bof3::addr::DivineMote_Task);
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = saved;
    }
}

// original 0x4E0990: the pool emptied (bytes 0..2 of each record); the task
// at the side's centre (MagicFx_CenterOnSide); +0xB 0, +1 on; two kind-1
// children 0x5C, each its +0x80 this task: the beam (+1 0, +9 1) and the
// burst (+1 1, +9 9), +0xB counting them (the slot index unchecked); sound
// 0x100.
S29_EXPORT void __cdecl DivineBreath_Start(void) {
    for (unsigned i = 0; i < cell::kMotes; ++i) {
        unsigned char* const m = Mote(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    static constexpr unsigned char kKind[2] = {0, 1};
    static constexpr unsigned char kDelay[2] = {1, 9};
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned slot = NewTask(0x5C);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetPtr(child + 0x80, self);
        child[1] = kKind[k];
        child[9] = kDelay[k];
        Inc(self[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4E0A60: the children's kind-1 task, a jmp through
// DivineBreathFx_Kinds (two entries: DivineBeam_Run, DivineBurst_Run) by +1,
// unchecked.
S29_EXPORT void __cdecl DivineBreathFx_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("DivineBreathFx_Task", phase, 2);
    DataPhase(tbl::kDivineKinds, phase)();
}

// original 0x4E0A80: DivineBeam_Steps (three) by +2, unchecked; then while
// +0 and +2 are set: on even frames three new colour bytes +0x5D..+0x5F
// ((Rand & 3) + 4 each), the actor's matrix pushed, the disc and the wall
// drawn, the matrix popped.
S29_EXPORT void __cdecl DivineBeam_Run(void) {
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("DivineBeam_Run", phase, 3);
    DataPhase(tbl::kBeamSteps, phase)();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    if ((Frame_Counter & 1) == 0) {
        for (unsigned k = 0x5D; k <= 0x5F; ++k) {
            const std::uint32_t r = RandCall();
            Sc()[k] = static_cast<unsigned char>((r & 3) + 4);
        }
    }
    MH_CALL(MagicFx_PushActorMatrix)();
    CallOwn(bof3::addr::DivineBeam_DrawDisc);
    CallOwn(bof3::addr::DivineBeam_DrawWall);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E0B00: +9 down; at 0 the beam starts at the owner's point, its
// height 0x4000000 above; three colour bytes (Rand & 3) + 4; radius +0x14
// 0x100, +9 0x10, +0xA 8, +2 on.
S29_EXPORT void __cdecl DivineBeam_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Add(Long(Owner() + 0x3C), 0x4000000));
    for (unsigned k = 0x5D; k <= 0x5F; ++k) {
        const std::uint32_t r = RandCall();
        Sc()[k] = static_cast<unsigned char>((r & 3) + 4);
    }
    SetLong(Sc() + 0x14, 0x100);
    Sc()[9] = 0x10;
    Sc()[0xA] = 8;
    Inc(Sc()[2]);
}

// original 0x4E0BB0: the height down 0x800000; +0xA down, at 0 +2 on.
S29_EXPORT void __cdecl DivineBeam_Descend(void) {
    SetLong(Sc() + 0x3C, Add(Long(Sc() + 0x3C), static_cast<std::int32_t>(0xFF800000u)));
    Dec(Sc()[0xA]);
    if (Sc()[0xA] == 0) Inc(Sc()[2]);
}

// original 0x4E0BE0: the radius +0x14 up 0x20; +9 down; at 0 the owner's
// +0xB down and the task freed.
S29_EXPORT void __cdecl DivineBeam_Widen(void) {
    SetLong(Sc() + 0x14, Add(Long(Sc() + 0x14), 0x20));
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

namespace {

// The colours both beam draws take: word 0x903856 / 58 / 5A = the signed
// colour byte +0x5D / +0x5E / +0x5F times the brightness +9.
void BeamColours() {
    const unsigned char* const s = Sc();
    Put(cell::kW6, Mul(static_cast<signed char>(s[0x5D]), s[9]));
    Put(cell::kW8, Mul(static_cast<signed char>(s[0x5E]), s[9]));
    Put(cell::kWA, Mul(static_cast<signed char>(s[0x5F]), s[9]));
}

}  // namespace

// original 0x4E0C20: the beam's disc. Gpu_SetDrawMode(.., 0x35) and
// Gfx_CommitPrim(5, 0xC); the radius the word +0x14; sixteen semi-transparent
// POLY_G3 of the centre and two rim points 0x100 apart (sin / cos x radius
// >> 12), the centre in the colours, the rim 0x10 grey, each
// Gfx_CommitPrim(5, 0x34); then the draw mode 0x15.
S29_EXPORT void __cdecl DivineBeam_DrawDisc(void) {
    DrawMode(0x35);
    Commit(5, 0xC);
    Put(cell::kR, Word(Sc() + 0x14));
    {
        const int s = Sin(0);
        Put(cell::kV2, Mul12(s, SS(cell::kR)));
    }
    {
        const int c = Cos(0);
        Put(cell::kV2 + 2, Mul12(c, SS(cell::kR)));
    }
    BeamColours();
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        const std::uint16_t y = SW(cell::kV2 + 2);
        const std::uint16_t x = SW(cell::kV2);
        Put(cell::kV0, 0);
        Put(cell::kV0 + 2, 0);
        Put(cell::kV1, x);
        Put(cell::kV1 + 2, y);
        {
            const int s = Sin(a);
            Put(cell::kV2, Mul12(s, SS(cell::kR)));
        }
        const int c = Cos(a);
        const int ny = Mul12(c, SS(cell::kR));
        unsigned char* const p = Gfx_PacketNext;
        Put(cell::kV2 + 4, 0);
        Put(cell::kV2 + 2, ny);
        Put(cell::kV1 + 4, 0);
        Put(cell::kV0 + 4, 0);
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Project3(p);
        Shade3(p + 4);
        Fill3(p + 0x14, 0x10);
        Fill3(p + 0x24, 0x10);
        Commit(5, 0x34);
    }
    DrawMode(0x15);
    Commit(5, 0xC);
}

// original 0x4E0E00: the beam's wall. The radius the word +0x14; 32
// semi-transparent POLY_G4, each between two rim points 0x80 apart at height
// -0x300 and the same two at 0, the top dark (1), the foot in the colours:
// each after Gpu_SetDrawMode(.., 0x35) linked by MapView_LinkPrimAt at the
// task's point plus the new rim point << 9 (0, 0xC), and again (0, 0x44).
S29_EXPORT void __cdecl DivineBeam_DrawWall(void) {
    Put(cell::kR, Word(Sc() + 0x14));
    {
        const int s = Sin(0);
        Put(cell::kV1, Mul12(s, SS(cell::kR)));
    }
    {
        const int c = Cos(0);
        Put(cell::kV1 + 2, Mul12(c, SS(cell::kR)));
    }
    BeamColours();
    for (int a = 0x80; a < 0x1080; a += 0x80) {
        const std::uint16_t y = SW(cell::kV1 + 2);
        const std::uint16_t x = SW(cell::kV1);
        Put(cell::kV0, x);
        Put(cell::kV0 + 2, y);
        Put(cell::kV0 + 4, -0x300);
        Put(cell::kV2, x);
        Put(cell::kV2 + 2, y);
        Put(cell::kV2 + 4, 0);
        {
            const int s = Sin(a);
            Put(cell::kV1, Mul12(s, SS(cell::kR)));
        }
        const int c = Cos(a);
        const int ny = Mul12(c, SS(cell::kR));
        const std::uint16_t nx = SW(cell::kV1);
        Put(cell::kV1 + 4, -0x300);
        Put(cell::kV3, nx);
        Put(cell::kV3 + 4, 0);
        const unsigned char* const s = Sc();
        Put(cell::kV1 + 2, ny);
        Put(cell::kV3 + 2, ny);
        const std::int32_t lx = Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<short>(nx)) << 9),
                                    Long(s + 0x34));
        const std::int32_t lz = Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<short>(ny)) << 9),
                                    Long(s + 0x38));
        DrawMode(0x35);
        Link(lx, lz, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Fill3(p + 4, 1);
        Fill3(p + 0x14, 1);
        Shade3(p + 0x24);
        Shade3(p + 0x34);
        Project4(p);
        Link(lx, lz, 0, 0x44);
    }
}

// original 0x4E1030: DivineBurst_Steps (three) by +2, unchecked; then while
// +0 and +2 are set the wall drawn under the actor's matrix.
S29_EXPORT void __cdecl DivineBurst_Run(void) {
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("DivineBurst_Run", phase, 3);
    DataPhase(tbl::kBurstSteps, phase)();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    CallOwn(bof3::addr::DivineBeam_DrawWall);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E1070: +9 down; at 0 the burst at the owner's point, colour
// bytes 0xF, radius 0x100, its step +0x20 0x21, +9 0x10, +0xA 0x1E, +2 on.
S29_EXPORT void __cdecl DivineBurst_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0x5D] = 0xF;
    Sc()[0x5E] = 0xF;
    Sc()[0x5F] = 0xF;
    SetLong(Sc() + 0x14, 0x100);
    SetLong(Sc() + 0x20, 0x21);
    Sc()[9] = 0x10;
    Sc()[0xA] = 0x1E;
    Inc(Sc()[2]);
}

// original 0x4E1110: the step +0x20 down 2 unless it is 1; the radius +0x14
// less it; once the radius is under 0x30 (signed): the targets' flag 0x10,
// sound 0x101, 64 motes (DivineMote_Alloc, the index unchecked), each its
// +0x80 the owner, +1 0, +0xB its number i, +9 (Rand & 7) + 8 (i / 6) + 1,
// counted in the owner's +0xB; +2 on.
S29_EXPORT void __cdecl DivineBurst_Shrink(void) {
    {
        unsigned char* const s = Sc();
        if (Long(s + 0x20) != 1) SetLong(s + 0x20, Sub(Long(s + 0x20), 2));
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x14, Sub(Long(s + 0x14), Long(s + 0x20)));
    }
    if (Long(Sc() + 0x14) >= 0x30) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    MH_CALL(Sound_PlayById)(0x101);
    for (unsigned i = 0; i < 0x40; ++i) {
        const unsigned slot = MH_AT(AllocFn, bof3::addr::DivineMote_Alloc)();
        unsigned char* const m = Mote(slot);
        SetLong(m + 0x80, Long(Mem(at::kOwner)));
        m[1] = 0;
        m[0xB] = static_cast<unsigned char>(i);
        const std::uint32_t r = RandCall();
        m[9] = static_cast<unsigned char>((r & 7) + (i / 6) * 8 + 1);
        Inc(Owner()[0xB]);
    }
    Inc(Sc()[2]);
}

// original 0x4E11F0: the radius down one while over 8 (signed); on odd frames
// +9 down, at 0 the owner's +0xB down and the task freed.
S29_EXPORT void __cdecl DivineBurst_Fade(void) {
    {
        unsigned char* const s = Sc();
        if (Long(s + 0x14) > 8) SetLong(s + 0x14, Sub(Long(s + 0x14), 1));
    }
    if ((Frame_Counter & 1) == 0) return;
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E1240: a mote, a jmp through DivineMote_TaskTable (one entry,
// DivineMote_Run) by +1, unchecked.
S29_EXPORT void __cdecl DivineMote_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("DivineMote_Task", phase, 1);
    DataPhase(tbl::kMoteTask, phase)();
}

// original 0x4E1260: DivineMote_Steps (two) by +2, unchecked; then while +0
// and +2 are set: draw mode 0x35 linked at the mote (2, 0xC), its screen
// point (BattleActor_UpdateScreenXY), its star and ring, draw mode 0x15
// linked the same.
S29_EXPORT void __cdecl DivineMote_Run(void) {
    const unsigned phase = Sc()[2];
    if (phase >= 2) PastTable("DivineMote_Run", phase, 2);
    DataPhase(tbl::kMoteSteps, phase)();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    DrawMode(0x35);
    LinkAtSc(0xC);
    MH_CALL(BattleActor_UpdateScreenXY)();
    CallOwn(bof3::addr::DivineMote_DrawStar);
    CallOwn(bof3::addr::DivineMote_DrawRing);
    DrawMode(0x15);
    LinkAtSc(0xC);
}

namespace {

// A mote's angle: ((+0xB & 7) x 8 + +9) << 6, a word.
std::uint16_t MoteAngle(const unsigned char* s) {
    return static_cast<std::uint16_t>(((s[0xB] & 7u) * 8 + s[9]) << 6);
}

}  // namespace

// original 0x4E12F0: +9 down; at 0 the mote starts: distance +0xC 0x5000, its
// step +0x18 0x200, rise +0x14 0x80000, its step +0x20 0x40000; the point
// the owner's plus sin / cos of its angle x the distance >> 12, the owner's
// height; colour bytes (Rand & 7) + 5; +9 0, +0xA 0x18, +2 on.
S29_EXPORT void __cdecl DivineMote_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    {
        unsigned char* const s = Sc();
        SetLong(s + 0xC, 0x5000);
        SetLong(s + 0x18, 0x200);
        SetLong(s + 0x14, 0x80000);
        SetLong(s + 0x20, 0x40000);
        Put(cell::kR, Word(s + 0xC));
        Put(cell::kW4, MoteAngle(s));
    }
    {
        const int sn = Sin(SS(cell::kW4));
        const std::int32_t v = Add(Mul12(sn, SS(cell::kR)), Long(Owner() + 0x34));
        SetLong(Sc() + 0x34, v);
    }
    {
        const int c = Cos(SS(cell::kW4));
        const std::int32_t v = Add(Mul12(c, SS(cell::kR)), Long(Owner() + 0x38));
        SetLong(Sc() + 0x38, v);
    }
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    for (unsigned k = 0x5D; k <= 0x5F; ++k) {
        const std::uint32_t r = RandCall();
        Sc()[k] = static_cast<unsigned char>((r & 7) + 5);
    }
    Sc()[9] = 0;
    Sc()[0xA] = 0x18;
    Inc(Sc()[2]);
}

// original 0x4E1430: +9 up; the distance +0xC less its step +0x18, the rise
// +0x14 plus its step +0x20; the point moved by sin / cos of the angle x the
// distance >> 12 (each into the record the pointer held before the call), the
// height up the rise; at distance 0 the owner's +0xB down and the record
// freed (0x4F6290, MAGIC219's).
S29_EXPORT void __cdecl DivineMote_Fly(void) {
    Inc(Sc()[9]);
    {
        unsigned char* const s = Sc();
        SetLong(s + 0xC, Sub(Long(s + 0xC), Long(s + 0x18)));
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x14, Add(Long(s + 0x14), Long(s + 0x20)));
    }
    unsigned char* x = nullptr;
    {
        unsigned char* const s = Sc();
        Put(cell::kR, Word(s + 0xC));
        x = s + 0x34;
        Put(cell::kW4, MoteAngle(s));
    }
    {
        const int sn = Sin(SS(cell::kW4));
        SetLong(x, Add(Long(x), Mul12(sn, SS(cell::kR))));
    }
    unsigned char* const z = Sc() + 0x38;
    {
        const int c = Cos(SS(cell::kW4));
        SetLong(z, Add(Long(z), Mul12(c, SS(cell::kR))));
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x3C, Add(Long(s + 0x3C), Long(s + 0x14)));
    }
    if (Long(Sc() + 0xC) != 0) return;
    Dec(Owner()[0xB]);
    MH_AT(Handler, magic_s29::kFreeRecord)();
}

// original 0x4E1510: the mote's star in screen space: radius +0xA; eight
// semi-transparent POLY_G3 of the screen point (+0x2E, +0x30) and two rim
// points 0x200 apart (sin / cos x radius >> 12 plus the point, as words),
// the centre 0xF0 grey, the rim the colour bytes << 4; each linked at the
// mote (2, 0x34).
S29_EXPORT void __cdecl DivineMote_DrawStar(void) {
    Put(cell::kR, Sc()[0xA]);
    {
        const int s = Sin(0);
        Put(cell::kV0, Mul12(s, SS(cell::kR)) + Word(Sc() + 0x2E));
    }
    {
        const int c = Cos(0);
        Put(cell::kV0 + 2, Mul12(c, SS(cell::kR)) + Word(Sc() + 0x30));
    }
    {
        const unsigned char* const s = Sc();
        Put(cell::kW6, Mul(static_cast<signed char>(s[0x5D]), 16));
        Put(cell::kW8, Mul(static_cast<signed char>(s[0x5E]), 16));
        Put(cell::kWA, Mul(static_cast<signed char>(s[0x5F]), 16));
    }
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const unsigned char* const s = Sc();
            PutFloat(p + 8, S16(s + 0x2E));
            PutFloat(p + 0xC, S16(s + 0x30));
            PutFloat(p + 0x18, SS(cell::kV0));
            PutFloat(p + 0x1C, SS(cell::kV0 + 2));
        }
        {
            const int s = Sin(a);
            Put(cell::kV0, Mul12(s, SS(cell::kR)) + Word(Sc() + 0x2E));
            PutFloat(p + 0x28, SS(cell::kV0));
        }
        {
            const int c = Cos(a);
            Put(cell::kV0 + 2, Mul12(c, SS(cell::kR)) + Word(Sc() + 0x30));
            Fill3(p + 4, 0xF0);
            PutFloat(p + 0x2C, SS(cell::kV0 + 2));
            Shade3(p + 0x14);
            Shade3(p + 0x24);
        }
        LinkAtSc(0x34);
    }
}

// original 0x4E16E0: the mote's ring in screen space: radii +0xA and twice
// it; eight semi-transparent POLY_G4 between the two circles, 0x200 apart,
// the outer edge dark (1), the inner in the colour words the star left; each
// linked at the mote (2, 0x44).
S29_EXPORT void __cdecl DivineMote_DrawRing(void) {
    {
        const unsigned char* const s = Sc();
        Put(cell::kR, s[0xA]);
        Put(cell::kR2, s[0xA] * 2);
    }
    // the rim points: v0 on the outer circle, v1 on the inner, at angle a
    const auto rim = [](int a, std::uint32_t radius, std::uint32_t v) {
        {
            const int s = Sin(a);
            Put(v, Mul12(s, SS(radius)) + Word(Sc() + 0x2E));
        }
        const int c = Cos(a);
        Put(v + 2, Mul12(c, SS(radius)) + Word(Sc() + 0x30));
    };
    rim(0, cell::kR2, cell::kV0);
    rim(0, cell::kR, cell::kV1);
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(cell::kV0));
        PutFloat(p + 0xC, SS(cell::kV0 + 2));
        PutFloat(p + 0x28, SS(cell::kV1));
        PutFloat(p + 0x2C, SS(cell::kV1 + 2));
        {
            const int s = Sin(a);
            Put(cell::kV0, Mul12(s, SS(cell::kR2)) + Word(Sc() + 0x2E));
            PutFloat(p + 0x18, SS(cell::kV0));
        }
        {
            const int c = Cos(a);
            Put(cell::kV0 + 2, Mul12(c, SS(cell::kR2)) + Word(Sc() + 0x30));
            PutFloat(p + 0x1C, SS(cell::kV0 + 2));
        }
        {
            const int s = Sin(a);
            Put(cell::kV1, Mul12(s, SS(cell::kR)) + Word(Sc() + 0x2E));
            PutFloat(p + 0x38, SS(cell::kV1));
        }
        {
            const int c = Cos(a);
            Put(cell::kV1 + 2, Mul12(c, SS(cell::kR)) + Word(Sc() + 0x30));
            Fill3(p + 4, 1);
            Fill3(p + 0x14, 1);
            PutFloat(p + 0x3C, SS(cell::kV1 + 2));
            Shade3(p + 0x24);
            Shade3(p + 0x34);
        }
        LinkAtSc(0x44);
    }
}

// original 0x4E1940: the first free record of DivineMote_Pool (+0 bit 0
// clear) taken (bit 0 set); its index, or 0xFF when all 64 are in use.
S29_EXPORT unsigned char __cdecl DivineMote_Alloc(void) {
    for (unsigned n = 0; n < cell::kMotes; ++n) {
        unsigned char* const m = Mote(n);
        if (m[0] & 1) continue;
        m[0] |= 1;
        return static_cast<unsigned char>(n);
    }
    return 0xFF;
}

// ===========================================================================
// MAGIC126 (row 127, ShadowBreath read one id down)

// original 0x4E19A0: the kind-2 task. A two-entry stack table by +1
// (ShadowBreath_Start, BattleFx_Finish), unchecked; then each live record of
// ShadowMote_Pool made ShadowMote_Current, its +0x1C the owner, and run
// through ShadowMote_Task - the owner as it was after the phase put back
// after each (the current record is left as the last).
S29_EXPORT void __cdecl ShadowBreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::ShadowBreath_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("ShadowBreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < cell::kShades; ++i) {
        unsigned char* const r = Shade(i);
        if ((r[0] & 1) == 0) continue;
        const std::int32_t its = Long(r + 0x1C);
        SetPtr(Mem(cell::kShadeCurrent), r);
        SetLong(Mem(at::kOwner), its);
        CallOwn(bof3::addr::ShadowMote_Task);
        SetLong(Mem(at::kOwner), owner);
    }
}

// original 0x4E1A10: the pool emptied (bytes 0..2 of each record); the task
// at the side's centre and its screen point; +0xB +9 0, +1 on; eight kind-1
// children 0x4E, the seekers, each its +0x80 this task, +1 2, +0xB its number
// i, +9 i + 1, counted in +0xB (the slot index unchecked); sound 0x102; row
// 26 of Gfx_ClutStrip from its source with bit 15 set (semi-transparent),
// Gfx_ClutStripDirty.
S29_EXPORT void __cdecl ShadowBreath_Start(void) {
    for (unsigned i = 0; i < cell::kShades; ++i) {
        unsigned char* const r = Shade(i);
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned slot = NewTask(0x4E);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetPtr(child + 0x80, self);
        child[1] = 2;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>(i + 1);
        Inc(self[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x102);
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStripDirty = 1;
}

// original 0x4E1AE0: the children's kind-1 task, a jmp through
// ShadowBreathFx_Kinds (three entries: ShadowOrb_Run, ShadowGlow_Run,
// ShadowSeeker_Run) by +1, unchecked.
S29_EXPORT void __cdecl ShadowBreathFx_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("ShadowBreathFx_Task", phase, 3);
    DataPhase(tbl::kShadowKinds, phase)();
}

// original 0x4E1B00: ShadowOrb_Steps (four: MAGIC056's 0x4ADB50, Grow,
// WaitChildren, MAGIC060's 0x4B1740) by +2, unchecked; then while +0 and +2
// are set, under the actor's matrix: the rim, the band, the disc.
S29_EXPORT void __cdecl ShadowOrb_Run(void) {
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("ShadowOrb_Run", phase, 4);
    DataPhase(tbl::kOrbSteps, phase)();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    CallOwn(bof3::addr::ShadowOrb_DrawRim);
    CallOwn(bof3::addr::ShadowOrb_DrawBand);
    CallOwn(bof3::addr::ShadowOrb_DrawDisc);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E1B40: +9 up 2, +0xA up 0x18; at +9 0x10 +2 on.
S29_EXPORT void __cdecl ShadowOrb_Grow(void) {
    AddB(Sc()[9], 2);
    AddB(Sc()[0xA], 0x18);
    if (Sc()[9] == 0x10) Inc(Sc()[2]);
}

// original 0x4E1B70: +2 on once the owner's count +0xB is under 2.
S29_EXPORT void __cdecl ShadowOrb_WaitChildren(void) {
    if (Owner()[0xB] < 2) Inc(Sc()[2]);
}

// original 0x4E1B90: the orb's disc. Gpu_SetDrawMode(.., 0x55) and
// Gfx_CommitPrim(5, 0xC); the radius +0xA x 2, the shade +9 x 12; 32
// semi-transparent POLY_G3 of the centre and two rim points 0x100 apart, the
// centre in the shade, the rim 1; each Gfx_CommitPrim(5, 0x34).
S29_EXPORT void __cdecl ShadowOrb_DrawDisc(void) {
    DrawMode(0x55);
    Commit(5, 0xC);
    {
        const unsigned char* const s = Sc();
        Put(cell::kR, s[0xA] * 2);
        Put(cell::kW6, s[9] * 12);
        Put(cell::kW8, s[9] * 12);
        Put(cell::kWA, s[9] * 12);
    }
    {
        const int s = Sin(0);
        Put(cell::kV2, Mul12(s, SS(cell::kR)));
    }
    {
        const int c = Cos(0);
        const int y = Mul12(c, SS(cell::kR));
        Put(cell::kV2 + 4, 0);
        Put(cell::kV2 + 2, y);
        Put(cell::kV1 + 4, 0);
        Put(cell::kV0 + 4, 0);
    }
    for (int a = 0x100; a < 0x2100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = SW(cell::kV2);
        const std::uint16_t y = SW(cell::kV2 + 2);
        Put(cell::kV0, 0);
        Put(cell::kV0 + 2, 0);
        Put(cell::kV1, x);
        Put(cell::kV1 + 2, y);
        {
            const int s = Sin(a);
            Put(cell::kV2, Mul12(s, SS(cell::kR)));
        }
        {
            const int c = Cos(a);
            Put(cell::kV2 + 2, Mul12(c, SS(cell::kR)));
        }
        Project3(p);
        Shade3(p + 4);
        Fill3(p + 0x14, 1);
        Fill3(p + 0x24, 1);
        Commit(5, 0x34);
    }
}

namespace {

// The orb's two rings (0x4E1D40, 0x4E1FB0): sixteen semi-transparent POLY_G4
// between an outer circle (radius R2) and an inner (radius R), 0x100 apart,
// the first two points in `outer` colours and the last two in `inner` (the
// scratch shade, or 1), each Gfx_CommitPrim(5, 0x44).
void OrbRing(bool shade_first) {
    {
        const int s = Sin(0);
        Put(cell::kV1, Mul12(s, SS(cell::kR2)));
    }
    {
        const int c = Cos(0);
        Put(cell::kV1 + 2, Mul12(c, SS(cell::kR2)));
    }
    {
        const int s = Sin(0);
        Put(cell::kV3, Mul12(s, SS(cell::kR)));
    }
    {
        const int c = Cos(0);
        const int y = Mul12(c, SS(cell::kR));
        Put(cell::kV3 + 4, 0);
        Put(cell::kV3 + 2, y);
        Put(cell::kV2 + 4, 0);
        Put(cell::kV1 + 4, 0);
        Put(cell::kV0 + 4, 0);
    }
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x1 = SW(cell::kV1);
        const std::uint16_t y1 = SW(cell::kV1 + 2);
        const std::uint16_t x3 = SW(cell::kV3);
        Put(cell::kV0, x1);
        const std::uint16_t y3 = SW(cell::kV3 + 2);
        Put(cell::kV0 + 2, y1);
        Put(cell::kV2, x3);
        Put(cell::kV2 + 2, y3);
        {
            const int s = Sin(a);
            Put(cell::kV1, Mul12(s, SS(cell::kR2)));
        }
        {
            const int c = Cos(a);
            Put(cell::kV1 + 2, Mul12(c, SS(cell::kR2)));
        }
        {
            const int s = Sin(a);
            Put(cell::kV3, Mul12(s, SS(cell::kR)));
        }
        {
            const int c = Cos(a);
            Put(cell::kV3 + 2, Mul12(c, SS(cell::kR)));
        }
        Project4(p);
        if (shade_first) {
            Shade3(p + 4);
            Shade3(p + 0x14);
            Fill3(p + 0x24, 1);
            Fill3(p + 0x34, 1);
        } else {
            Fill3(p + 4, 1);
            Fill3(p + 0x14, 1);
            Shade3(p + 0x24);
            Shade3(p + 0x34);
        }
        Commit(5, 0x44);
    }
}

}  // namespace

// original 0x4E1D40: the orb's rim. Draw mode 0x35, Gfx_CommitPrim(5, 0xC);
// the inner radius +0xA x 2, the outer (Rand & 3) + +0xA x 3; the shade (+9 x
// 15, +9 x 4, +9 x 15) on the inner edge, the outer 1.
S29_EXPORT void __cdecl ShadowOrb_DrawRim(void) {
    DrawMode(0x35);
    Commit(5, 0xC);
    Put(cell::kR, Sc()[0xA] * 2);
    const std::uint32_t r = RandCall();
    {
        const unsigned char* const s = Sc();
        Put(cell::kR2, (r & 3) + s[0xA] * 3u);
        Put(cell::kW6, s[9] * 15);
        Put(cell::kW8, s[9] * 4);
        Put(cell::kWA, s[9] * 15);
    }
    OrbRing(false);
}

// original 0x4E1FB0: the orb's band. Draw mode 0x35, Gfx_CommitPrim(5, 0xC);
// the inner radius +0xA, the outer +0xA x 2; the shade (+9 x 15, +9 x 4,
// +9 x 15) on the outer edge, the inner 1.
S29_EXPORT void __cdecl ShadowOrb_DrawBand(void) {
    DrawMode(0x35);
    Commit(5, 0xC);
    {
        const unsigned char* const s = Sc();
        Put(cell::kR, s[0xA]);
        Put(cell::kR2, s[0xA] * 2);
        Put(cell::kW6, s[9] * 15);
        Put(cell::kW8, s[9] * 4);
        Put(cell::kWA, s[9] * 15);
    }
    OrbRing(true);
}

// original 0x4E2210: ShadowGlow_Steps (four: Wait, MAGIC040's 0x4A5D20, Grow,
// MAGIC040's 0x4A5D50) by +2, unchecked; then while +0 and +2 are set its
// screen point and the glow drawn.
S29_EXPORT void __cdecl ShadowGlow_Run(void) {
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("ShadowGlow_Run", phase, 4);
    DataPhase(tbl::kGlowSteps, phase)();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    CallOwn(bof3::addr::ShadowGlow_Draw);
}

// original 0x4E2240: +9 down; at 0 sound 0x100, the glow at the owner's
// point, +0xB 0, +9 0x14, +0xA 0, +2 on.
S29_EXPORT void __cdecl ShadowGlow_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sound_PlayById)(0x100);
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0xB] = 0;
    Sc()[9] = 0x14;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4E22C0: +9 up 2; at 0x96 +2 on.
S29_EXPORT void __cdecl ShadowGlow_Grow(void) {
    AddB(Sc()[9], 2);
    if (Sc()[9] == 0x96) Inc(Sc()[2]);
}

// original 0x4E22E0: the glow, two semi-transparent POLY_GT4 sprites at the
// screen point (+0x2E, +0x30): half-width 0x28, the upper from y - +9 - 0x10
// to y - 0x10, the lower 0x10 below it; shade +0xA << 3 (the lower's foot 1);
// Gpu_GetTPage(0, 1, 0x340, 0x100), Gpu_GetClut(0, 0x1FA), uv 8..0x57 by 8..
// +9 + 8 and +9 + 8 .. +9 + 0x18; draw mode 0xB5 linked (2, 0xC) first, each
// sprite linked at the task (2, 0x54).
S29_EXPORT void __cdecl ShadowGlow_Draw(void) {
    {
        const unsigned char* const s = Sc();
        Put(cell::kR, 0x28);
        Put(cell::kR2, s[9]);
        Put(cell::kW4, s[0xA] << 3);
        Put(cell::kX, Word(s + 0x2E));
        Put(cell::kY, Word(s + 0x30));
    }
    DrawMode(0xB5);
    LinkAtSc(0xC);
    for (unsigned k = 0; k < 2; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const int x = SS(cell::kX), y = SS(cell::kY), r = SS(cell::kR), h = SS(cell::kR2);
        const int top = k == 0 ? y - h - 0x10 : y - 0x10;
        const int foot = k == 0 ? y - 0x10 : y;
        PutFloat(p + 8, x - r);
        PutFloat(p + 0xC, top);
        PutFloat(p + 0x1C, x + r);
        PutFloat(p + 0x20, top);
        PutFloat(p + 0x30, x - r);
        PutFloat(p + 0x34, foot);
        PutFloat(p + 0x44, x + r);
        PutFloat(p + 0x48, foot);
        {
            const unsigned t = MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100);
            SetWord(p + 0x2A, t & 0xFFFFu);
        }
        {
            const unsigned c = MH_CALL(Gpu_GetClut)(0, 0x1FA);
            SetWord(p + 0x16, c & 0xFFFFu);
        }
        const auto v = [](unsigned add) { return static_cast<unsigned char>(SB(cell::kR2) + add); };
        if (k == 0) {
            p[0x14] = 8;
            p[0x15] = 8;
            p[0x28] = 0x57;
            p[0x29] = 8;
            p[0x3C] = 8;
            p[0x50] = 0x57;
            p[0x3D] = v(8);
            p[0x51] = v(8);
        } else {
            p[0x14] = 8;
            p[0x28] = 0x57;
            p[0x15] = v(8);
            p[0x3C] = 8;
            p[0x29] = v(8);
            p[0x50] = 0x57;
            p[0x3D] = v(0x18);
            p[0x51] = v(0x18);
        }
        const unsigned char g = SB(cell::kW4);
        Fill3(p + 4, g);
        Fill3(p + 0x18, g);
        Fill3(p + 0x2C, k == 0 ? g : 1);
        Fill3(p + 0x40, k == 0 ? g : 1);
        LinkAtSc(0x54);
    }
}

// original 0x4E26A0: ShadowSeeker_Steps (four) by +2, unchecked; then while
// +0 and +2 are set its screen point and the seeker drawn.
S29_EXPORT void __cdecl ShadowSeeker_Run(void) {
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("ShadowSeeker_Run", phase, 4);
    DataPhase(tbl::kSeekerSteps, phase)();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    CallOwn(bof3::addr::ShadowSeeker_Draw);
}

namespace {

// Math_Ratan2 of the owner's point less the task's, x first then z, each an
// integer made a float (fild / fstp dword): the heading toward the owner.
int HeadingToOwner() {
    const unsigned char* const o = Owner();
    const unsigned char* const s = Sc();
    const std::int32_t dz = Sub(Long(o + 0x38), Long(s + 0x38));
    const std::int32_t dx = Sub(Long(o + 0x34), Long(s + 0x34));
    return MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
}

}  // namespace

// original 0x4E26D0: +9 down; at 0 the seeker starts at the acting actor's
// sprite (0x904B3C): its direction byte and point; the offset (0x8000, 0)
// turned by the direction (0x446770) and added, the height up 0x800000; its
// heading +0xC toward the owner; +9 0, +2 on.
S29_EXPORT void __cdecl ShadowSeeker_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[8] = Pointer(cell::kActorSprite)[8];
    SetLong(Sc() + 0x34, Long(Pointer(cell::kActorSprite) + 0x34));
    SetLong(Sc() + 0x38, Long(Pointer(cell::kActorSprite) + 0x38));
    SetLong(Sc() + 0x3C, Long(Pointer(cell::kActorSprite) + 0x3C));
    SetLong(Sc() + 0xC, 0x8000);
    SetLong(Sc() + 0x10, 0);
    MH_AT(TaskFn, magic_s29::kTurnByFacing)(Sc());
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, Add(Long(s + 0x34), Long(s + 0xC)));
        SetLong(s + 0x38, Add(Long(s + 0x38), Long(s + 0x10)));
        SetLong(s + 0x3C, Add(Long(s + 0x3C), 0x800000));
    }
    const int heading = HeadingToOwner();
    SetLong(Sc() + 0xC, heading);
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4E27E0: +9 up 4 while under 0x10; a step toward the owner
// (MagicFx_StepToward, speed 0x60); the old heading to +0x10, the new to
// +0xC; +2 on when MagicFx_NearSprite(owner, 0xC000), or when the turn
// |(+0x10 & 0xFFF) - (+0xC & 0xFFF)| (kept in +0x10) lies strictly between
// 0x600 and 0xA00 - it has passed the owner.
S29_EXPORT void __cdecl ShadowSeeker_Home(void) {
    {
        unsigned char* const s = Sc();
        if (s[9] < 0x10) AddB(s[9], 4);
    }
    MH_CALL(MagicFx_StepToward)(Owner(), 0x60);
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x10, Long(s + 0xC));
    }
    const int heading = HeadingToOwner();
    SetLong(Sc() + 0xC, heading);
    const int near = MH_CALL(MagicFx_NearSprite)(Owner(), 0xC000);
    unsigned char* const s = Sc();
    if (near != 0) {
        Inc(s[2]);
        return;
    }
    SetLong(s + 0x10, Sub(Long(s + 0x10) & 0xFFF, Long(s + 0xC) & 0xFFF));
    if (Long(s + 0x10) < 0) SetLong(s + 0x10, Sub(0, Long(s + 0x10)));
    const std::int32_t turn = Long(s + 0x10);
    if (turn > 0x600 && turn < 0xA00) Inc(s[2]);
}

// original 0x4E28C0: a seeker with +0xB 0 (the first) bursts, any other only
// steps on. The burst: sound 0x101; two kind-1 children 0x4E of the owner -
// the orb (+1 0) and the glow (+1 1, +9 0x2E) - counted in the owner's +0xB;
// 64 motes (ShadowMote_Alloc, the index unchecked), each its +0x1C the owner,
// +1 0, +4 its number i, delay +5 (Rand & 0xF) + (i & 1) x 16 + 0x18, counted
// in the owner's +0xB; the targets' flag 0x10; +2 on.
S29_EXPORT void __cdecl ShadowSeeker_Burst(void) {
    if (Sc()[0xB] != 0) {
        Inc(Sc()[2]);
        return;
    }
    MH_CALL(Sound_PlayById)(0x101);
    {
        const unsigned slot = NewTask(0x4E);
        unsigned char* const o = Owner();
        unsigned char* const child = TaskSlot(slot);
        SetPtr(child + 0x80, o);
        child[1] = 0;
        Inc(o[0xB]);
    }
    {
        const unsigned slot = NewTask(0x4E);
        unsigned char* const o = Owner();
        unsigned char* const child = TaskSlot(slot);
        SetPtr(child + 0x80, o);
        child[1] = 1;
        child[9] = 0x2E;
        Inc(o[0xB]);
    }
    for (unsigned i = 0; i < 0x40; ++i) {
        const unsigned index = MH_AT(AllocFn, bof3::addr::ShadowMote_Alloc)();
        unsigned char* const r = Shade(index);
        SetLong(r + 0x1C, Long(Mem(at::kOwner)));
        r[1] = 0;
        r[4] = static_cast<unsigned char>(i);
        const std::uint32_t n = RandCall();
        r[5] = static_cast<unsigned char>((n & 0xF) + ((i & 1) << 4) + 0x18);
        Inc(Owner()[0xB]);
    }
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Inc(Sc()[2]);
}

// original 0x4E29E0: +9 down 2; once negative (as a signed byte) the owner's
// +0xB down and the task freed.
S29_EXPORT void __cdecl ShadowSeeker_Fade(void) {
    AddB(Sc()[9], 0xFE);
    if (static_cast<signed char>(Sc()[9]) >= 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E2A10: the seeker in screen space. Draw mode 0x35,
// Gfx_CommitPrim(3, 0xC); sixteen semi-transparent POLY_G3 fanned round the
// screen point (+0x2E, +0x30), radius 0x18, 0x100 apart (the rim sin / cos x
// 0x18 >> 12 plus the point, as integers); the centre ShadowSeeker_Colours
// [+0xB] (red, blue) x +9 and +9 green, the rim 1; each Gfx_CommitPrim(3,
// 0x34). The table's index +0xB is unchecked.
S29_EXPORT void __cdecl ShadowSeeker_Draw(void) {
    DrawMode(0x35);
    Commit(3, 0xC);
    {
        const unsigned char* const s = Sc();
        Put(cell::kW4, 0);
        Put(cell::kR, 0x18);
        Put(cell::kX, Word(s + 0x2E));
        Put(cell::kY, Word(s + 0x30));
        const unsigned char* const c = Mem(tbl::kSeekerColours + s[0xB] * 2u);
        Put(cell::kW6, c[0] * s[9]);
        Put(cell::kW8, s[9]);
        Put(cell::kWA, c[1] * s[9]);
    }
    for (int i = 0; i < 0x10; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(cell::kX));
        PutFloat(p + 0xC, SS(cell::kY));
        {
            const int s = Sin(SS(cell::kW4));
            PutFloat(p + 0x18, Add(Mul12(s, SS(cell::kR)), SS(cell::kX)));
        }
        {
            const int c = Cos(SS(cell::kW4));
            PutFloat(p + 0x1C, Add(Mul12(c, SS(cell::kR)), SS(cell::kY)));
        }
        Put(cell::kW4, (i + 1) << 8);
        {
            const int s = Sin(SS(cell::kW4));
            PutFloat(p + 0x28, Add(Mul12(s, SS(cell::kR)), SS(cell::kX)));
        }
        {
            const int c = Cos(SS(cell::kW4));
            PutFloat(p + 0x2C, Add(Mul12(c, SS(cell::kR)), SS(cell::kY)));
        }
        Shade3(p + 4);
        Fill3(p + 0x14, 1);
        Fill3(p + 0x24, 1);
        Commit(3, 0x34);
    }
}

// original 0x4E2BF0: a mote of the pool, ShadowMote_Current: a jmp through
// ShadowMote_TaskTable (one entry, ShadowMote_Run) by its +1, unchecked.
S29_EXPORT void __cdecl ShadowMote_Task(void) {
    const unsigned phase = Cur()[1];
    if (phase >= 1) PastTable("ShadowMote_Task", phase, 1);
    DataPhase(tbl::kShadeTask, phase)();
}

// original 0x4E2C10: ShadowMote_Steps (four) by the current record's +2,
// unchecked; then while its +0 and +2 are set, its screen point and the
// sprite drawn.
S29_EXPORT void __cdecl ShadowMote_Run(void) {
    const unsigned phase = Cur()[2];
    if (phase >= 4) PastTable("ShadowMote_Run", phase, 4);
    DataPhase(tbl::kShadeSteps, phase)();
    const unsigned char* const r = Cur();
    if (r[0] == 0 || r[2] == 0) return;
    CallOwn(bof3::addr::ShadowMote_Project);
    CallOwn(bof3::addr::ShadowMote_Draw);
}

namespace {

// A shadow mote's angle: (+4 & 0xF) << 8.
std::uint16_t ShadeAngle(const unsigned char* r) { return static_cast<std::uint16_t>((r[4] & 0xFu) << 8); }

// Its point round the owner: sin / cos of the angle x `radius` (not shifted)
// plus the owner's x / z, into the current record read again after each
// call; `radius` is read after the call too (the record's +7, or the scratch
// word).
template <typename Radius> void ShadeCircle(Radius radius) {
    {
        const int s = Sin(static_cast<short>(SW(cell::kW4)));
        unsigned char* const r = Cur();
        SetLong(r + 0x10, Add(Mul(s, radius(r)), Long(Owner() + 0x34)));
    }
    {
        const int c = Cos(SS(cell::kW4));
        unsigned char* const r = Cur();
        SetLong(r + 0x14, Add(Mul(c, radius(r)), Long(Owner() + 0x38)));
    }
}
int SpreadRadius(const unsigned char* r) { return r[7]; }

// The three moving steps' common middle: the angle, the point at radius +7,
// the height +0x18 up the rise +8.
void ShadeMove() {
    Put(cell::kW4, ShadeAngle(Cur()));
    ShadeCircle(&SpreadRadius);
    unsigned char* const r = Cur();
    SetLong(r + 0x18, Add(Long(r + 0x18), Long(r + 8)));
}

}  // namespace

// original 0x4E2C40: the current record's +5 down; at 0 it starts: angle
// (+4 & 0xF) << 8, radius (+4 >> 4) x 8 + 8; its point round the owner at
// that radius, the owner's height + 0x800000; rise +8 0, +5 +6 0, +7 the
// radius, +2 on.
S29_EXPORT void __cdecl ShadowMote_Wait(void) {
    Dec(Cur()[5]);
    const unsigned char* const r = Cur();
    if (r[5] != 0) return;
    Put(cell::kW4, ShadeAngle(r));
    Put(cell::kR, (r[4] >> 4) * 8 + 8);
    ShadeCircle([](const unsigned char*) { return static_cast<int>(SS(cell::kR)); });
    SetLong(Cur() + 0x18, Add(Long(Owner() + 0x3C), 0x800000));
    SetLong(Cur() + 8, 0);
    Cur()[5] = 0;
    Cur()[6] = 0;
    Cur()[7] = SB(cell::kR);
    Inc(Cur()[2]);
}

// original 0x4E2D30: on odd frames the size +5 and the radius +7 up; the
// point round the owner at +7, the height up +8; +6 up, at 0x10 +2 on.
S29_EXPORT void __cdecl ShadowMote_Spread(void) {
    if (Frame_Counter & 1) {
        Inc(Cur()[5]);
        Inc(Cur()[7]);
    }
    ShadeMove();
    Inc(Cur()[6]);
    if (Cur()[6] == 0x10) Inc(Cur()[2]);
}

// original 0x4E2DF0: on odd frames the radius +7 up; the rise +8 up 0x80000;
// the point and height as Spread; +2 on once the rise is 0x800000 or more
// (signed).
S29_EXPORT void __cdecl ShadowMote_Lift(void) {
    if (Frame_Counter & 1) Inc(Cur()[7]);
    {
        unsigned char* const r = Cur();
        SetLong(r + 8, Add(Long(r + 8), 0x80000));
    }
    ShadeMove();
    if (Long(Cur() + 8) >= 0x800000) Inc(Cur()[2]);
}

// original 0x4E2EA0: on odd frames the radius +7 up; the point and height as
// Spread; +6 down, at 0 the owner's +0xB down and the record freed
// (ShadowMote_Free).
S29_EXPORT void __cdecl ShadowMote_Fade(void) {
    if (Frame_Counter & 1) Inc(Cur()[7]);
    ShadeMove();
    Dec(Cur()[6]);
    if (Cur()[6] != 0) return;
    Dec(Owner()[0xB]);
    CallOwn(bof3::addr::ShadowMote_Free);
}

// original 0x4E2F60: the mote as a semi-transparent POLY_FT4 in screen space
// at its point (+0xC, +0xE): half-width +5, height +5 x 4 above it, shade +6
// x 6; Gpu_GetTPage(0, 1, 0x340, 0x100), Gpu_GetClut(0, 0x1FA), uv 0x58..0x68
// by 8..0x48; draw mode 0xB5 linked at the record (2, 0xC), the sprite
// linked (2, 0x48).
S29_EXPORT void __cdecl ShadowMote_Draw(void) {
    {
        const unsigned char* const r = Cur();
        Put(cell::kR, r[5]);
        Put(cell::kR2, r[5] * 4);
        Put(cell::kW4, r[6] * 6);
        Put(cell::kX, Word(r + 0xC));
        Put(cell::kY, Word(r + 0xE));
    }
    DrawMode(0xB5);
    LinkAtCur(0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    {
        const int x = SS(cell::kX), y = SS(cell::kY), r = SS(cell::kR), h = SS(cell::kR2);
        PutFloat(p + 8, x - r);
        PutFloat(p + 0xC, y - h);
        PutFloat(p + 0x18, x + r);
        PutFloat(p + 0x1C, y - h);
        PutFloat(p + 0x28, x - r);
        PutFloat(p + 0x2C, y);
        PutFloat(p + 0x38, x + r);
        PutFloat(p + 0x3C, y);
    }
    {
        const unsigned t = MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100);
        SetWord(p + 0x26, t & 0xFFFFu);
    }
    {
        const unsigned c = MH_CALL(Gpu_GetClut)(0, 0x1FA);
        SetWord(p + 0x16, c & 0xFFFFu);
    }
    p[0x15] = 8;
    p[0x25] = 8;
    p[0x14] = 0x58;
    p[0x24] = 0x68;
    p[0x34] = 0x58;
    p[0x35] = 0x48;
    p[0x44] = 0x68;
    p[0x45] = 0x48;
    Fill3(p + 4, SB(cell::kW4));
    LinkAtCur(0x48);
}

// original 0x4E3140: the first free record of ShadowMote_Pool (+0 bit 0
// clear) taken (bit 0 set); its index, or 0xFF when all 128 are in use.
S29_EXPORT unsigned char __cdecl ShadowMote_Alloc(void) {
    for (unsigned n = 0; n < cell::kShades; ++n) {
        unsigned char* const r = Shade(n);
        if (r[0] & 1) continue;
        r[0] |= 1;
        return static_cast<unsigned char>(n);
    }
    return 0xFF;
}

// original 0x4E3190: the current record's bytes 0..3 cleared.
S29_EXPORT void __cdecl ShadowMote_Free(void) {
    Cur()[0] = 0;
    Cur()[1] = 0;
    Cur()[2] = 0;
    Cur()[3] = 0;
}

// original 0x4E31C0: the current record's screen point, as
// BattleActor_UpdateScreenXY's for a task: (x >> 9 - 0x4000, z >> 9 - 0x4000,
// -(height word +0x1A / 2)) projected into a TILE_1 at Gfx_PacketNext -
// never committed, used for its vertex - and the float x / y truncated (the
// CRT's _ftol) to +0xC / +0xE.
S29_EXPORT void __cdecl ShadowMote_Project(void) {
    const unsigned char* const r = Cur();
    unsigned char* const p = Gfx_PacketNext;
    short v[4];
    v[0] = static_cast<short>((Long(r + 0x10) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(r + 0x14) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(r + 0x1A) / 2));
    v[3] = 0;
    MH_CALL(Gpu_SetTile1)(p);
    long depth;
    MH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(p + 8), &depth);
    MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
    SetWord(Cur() + 0xC, Ftol16(static_cast<std::uint32_t>(Long(p + 8))));
    SetWord(Cur() + 0xE, Ftol16(static_cast<std::uint32_t>(Long(p + 0xC))));
}

void MagicS29_Inject() {
    if (bof3::WantsShadow("magic_s29")) magic_s29::SelfTest();
    BOF3_INJECT(DivineBreath_Task);
    BOF3_INJECT(DivineBreath_Start);
    BOF3_INJECT(DivineBreathFx_Task);
    BOF3_INJECT(DivineBeam_Run);
    BOF3_INJECT(DivineBeam_Wait);
    BOF3_INJECT(DivineBeam_Descend);
    BOF3_INJECT(DivineBeam_Widen);
    BOF3_INJECT(DivineBeam_DrawDisc);
    BOF3_INJECT(DivineBeam_DrawWall);
    BOF3_INJECT(DivineBurst_Run);
    BOF3_INJECT(DivineBurst_Wait);
    BOF3_INJECT(DivineBurst_Shrink);
    BOF3_INJECT(DivineBurst_Fade);
    BOF3_INJECT(DivineMote_Task);
    BOF3_INJECT(DivineMote_Run);
    BOF3_INJECT(DivineMote_Launch);
    BOF3_INJECT(DivineMote_Fly);
    BOF3_INJECT(DivineMote_DrawStar);
    BOF3_INJECT(DivineMote_DrawRing);
    BOF3_INJECT(DivineMote_Alloc);
    BOF3_INJECT(ShadowBreath_Task);
    BOF3_INJECT(ShadowBreath_Start);
    BOF3_INJECT(ShadowBreathFx_Task);
    BOF3_INJECT(ShadowOrb_Run);
    BOF3_INJECT(ShadowOrb_Grow);
    BOF3_INJECT(ShadowOrb_WaitChildren);
    BOF3_INJECT(ShadowOrb_DrawDisc);
    BOF3_INJECT(ShadowOrb_DrawRim);
    BOF3_INJECT(ShadowOrb_DrawBand);
    BOF3_INJECT(ShadowGlow_Run);
    BOF3_INJECT(ShadowGlow_Wait);
    BOF3_INJECT(ShadowGlow_Grow);
    BOF3_INJECT(ShadowGlow_Draw);
    BOF3_INJECT(ShadowSeeker_Run);
    BOF3_INJECT(ShadowSeeker_Launch);
    BOF3_INJECT(ShadowSeeker_Home);
    BOF3_INJECT(ShadowSeeker_Burst);
    BOF3_INJECT(ShadowSeeker_Fade);
    BOF3_INJECT(ShadowSeeker_Draw);
    BOF3_INJECT(ShadowMote_Task);
    BOF3_INJECT(ShadowMote_Run);
    BOF3_INJECT(ShadowMote_Wait);
    BOF3_INJECT(ShadowMote_Spread);
    BOF3_INJECT(ShadowMote_Lift);
    BOF3_INJECT(ShadowMote_Fade);
    BOF3_INJECT(ShadowMote_Draw);
    BOF3_INJECT(ShadowMote_Alloc);
    BOF3_INJECT(ShadowMote_Free);
    BOF3_INJECT(ShadowMote_Project);
}
