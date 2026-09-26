// Four spell overlays compiled into the exe, round nine group S08
// (docs/magic_s08.md): the PSX's MAGIC041, MAGIC042, MAGIC043 and
// MAGIC044.EMI, Magic_Rows rows 80, 81, 110 and 96. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Berserk, Counter /
// Mind's Eye, WardOfLight / Resist and Evil Eye; the names below use those
// labels as hypotheses, and say what the code does.
//
//   - MAGIC041 0x4A6440..0x4A6DE9: the source sprite darkened through its tint
//     record, brightened and faded back; nine children (kind 1, 0x3D): eight
//     rings of 64 gouraud quads that rise and open one after another, and a
//     glow of sixteen gouraud triangles;
//   - MAGIC042 0x4A6DF0..0x4A7822: one child (kind 1, 0x3E) on the target's
//     screen point (moved by a per-character offset table for a party target):
//     a disc of sixteen triangles, spokes and three arcs of lines;
//   - MAGIC043 0x4A7830..0x4A8246: the source sprite tinted, a pool of its
//     own (64 records at 0x67BC40) of eight motes launched from the owner
//     toward the field's kind-2 point, each a band of eight gouraud quads;
//     for ability 0x2B a buff (MagicFx_ApplyBuff) and a child (kind 1, 0x48);
//   - MAGIC044 0x4A8250..0x4A9822: two beams (kind 1, 6) that seek the source
//     sprite leaving a trail of up to sixteen screen points (0x67DD40, 0x80
//     bytes a beam), drawn as a ribbon of gouraud quads, and sparks shed
//     along the way (the same kind, phase 1), each one textured quad.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent), and
// CounterMark_DrawSpokes aborts for a first angle of 0xE0 or more, where the
// original's byte counter never reaches its end (its one caller passes 0).
#include "game/magic_s08.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kAbility = 0x904B80;       // u16: the ability being cast
constexpr std::uint32_t kFormIndex = 0x904B89;     // u8: indexes CounterMark_FormIndex
constexpr std::uint32_t kTints = 0x7E0700;         // MoveScript_TintRecords, 12 bytes a record
constexpr std::uint32_t kWardPool = 0x67BC40;      // WardMote_Pool: 64 records of 0x84
constexpr std::uint32_t kTrails = 0x67DD40;        // EvilEyeBeam_Trails: 32 points of 4 bytes a beam
constexpr std::uint32_t kMemberOffsets = 0x65A838; // CounterMark_MemberOffsets
constexpr std::uint32_t kFormOffsets = 0x65A890;   // CounterMark_FormOffsets
constexpr std::uint32_t kFormBytes = 0x65A8E8;     // CounterMark_FormIndex
constexpr std::uint32_t kWardTilts = 0x65A918;     // WardMote_Tilts

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Source() { return Pointer(at::kSource); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
void AddLong(unsigned char* p, std::uint32_t v) {
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) + v));
}
void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }
// `cdq / and edx, 3 / add / sar 2`: a signed divide by 4 toward zero.
int Div4(int v) { return static_cast<int>(static_cast<std::uint32_t>(v) + (static_cast<std::uint32_t>(v >> 31) & 3u)) >> 2; }

// `fild dword` then `fstp dword`: an integer as a float.
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
// The enemy records by battle index - 3, unchecked, as the originals index them.
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}
unsigned char* TintRecord(unsigned index) { return Mem(kTints + index * 12u); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using Fn2 = void (__cdecl*)(unsigned, unsigned);
using Alloc = unsigned char (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
void Call2(std::uint32_t address, unsigned a, unsigned b) { MH_AT(Fn2, address)(a, b); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = 0x446770;
using TaskFn = void (__cdecl*)(unsigned char*);
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }

// The phase handler of another unit a stack table holds (docs/magic_s08.md
// section 3): called by its address.
constexpr std::uint32_t kGlowPhase3 = 0x4B1740;   // MAGIC060 (group S12)

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}
// A .data table's call, `call` / `jmp [table + phase * 4]`, read in place (the
// fuzz swaps the cells for recorders); the table's own entries only.
void CellCall(std::uint32_t table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))))();
}

// The task's position +0x34 / +0x38 / +0x3C from the owner's.
void OwnerPosition() {
    const unsigned char* const o = Owner();
    unsigned char* const s = Sc();
    SetLong(s + 0x34, Long(o + 0x34));
    SetLong(s + 0x38, Long(o + 0x38));
    SetLong(s + 0x3C, Long(o + 0x3C));
}

// The callees with the arguments the originals push: the projections get the
// depth and flag pointers (RotTransPers3 takes one of them).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S08_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four vertex-scratch SVECTORs projected to +8, +0x18, +0x28, +0x38.
void Rtp4(unsigned char* prim) {
    long p, flag;
    S08_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38, &p,
                                     &flag);
}
void Rtp3(unsigned char* prim) {
    long p, flag;
    S08_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }
void LinkAtTask(int dy, unsigned size) {
    const unsigned char* const s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), dy, size);
}
int Ratan2(int y, int x) { return MH_CALL(Math_Ratan2)(static_cast<float>(y), static_cast<float>(x)); }

// A pool of 64 records of 0x84 bytes (WardMote_Pool).
void ClearPool(std::uint32_t pool) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
}
// Every live record (bit 0 of +0) run as Sprite_Current with its +0x80 as the
// owner, then the task and owner as they were after the phase call.
void WalkPool(std::uint32_t pool, std::uint32_t run) {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(at::kOwner), rec_owner);
        Call0(run);
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = self;
    }
}

// EvilEyeBeam_Trails: point j of beam `beam` (+4 of the task), x then y.
unsigned char* Trail(unsigned beam, int j) {
    return Mem(kTrails + static_cast<std::uint32_t>((static_cast<int>(beam) * 32 + j) * 4));
}

}  // namespace

#define S08_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC041 (row 80, Berserk read one id down)

// original 0x4A6440: the kind-2 task. A six-entry stack table by +1:
// Berserk_Start, _Tint, _Brighten, _WaitChildren, _Fade, BattleFx_Finish.
S08_EXPORT void __cdecl Berserk_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::Berserk_Start,         bof3::addr::Berserk_Tint,
                                                 bof3::addr::Berserk_Brighten,      bof3::addr::Berserk_WaitChildren,
                                                 bof3::addr::Berserk_Fade,          bof3::addr::BattleFx_Finish};
    StackCall(kPhases, 6, Sc()[1], "Berserk_Task");
}

// original 0x4A6490: the owner's position; +0xB 0, +9 0xA, +0xA 0, +1 on;
// eight rings (kind 1, 0x3D; +1 0, +9 the delay 1, 9, .., 0x39) and a glow
// (+1 1), each owned by this task and counted in +0xB; sound 0x100.
S08_EXPORT void __cdecl Berserk_Start(void) {
    OwnerPosition();
    Sc()[0xB] = 0;
    Sc()[9] = 0xA;
    Sc()[0xA] = 0;
    Inc(Sc()[1]);
    for (unsigned delay = 1; delay < 0x41; delay += 8) {
        const unsigned slot = NewTask(0x3D);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[1] = 0;
        child[9] = static_cast<unsigned char>(delay);
        Inc(self[0xB]);
    }
    const unsigned slot = NewTask(0x3D);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
    child[1] = 1;
    Inc(self[0xB]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A6580: +9 down; at 0 the source sprite (0x904B4C) tinted black,
// its tint slot to +9, +1 on.
S08_EXPORT void __cdecl Berserk_Tint(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    const unsigned char tint = MH_CALL(Sprite_SetTint)(Source(), 0, 0, 0, 1);
    Sc()[9] = tint;
    Inc(Sc()[1]);
}

// original 0x4A65D0: the tint record +9 brighter (+2 by 2, +3 and +4 by 1),
// +0xA up; at 8 +1 on.
S08_EXPORT void __cdecl Berserk_Brighten(void) {
    unsigned char* const s = Sc();
    AddB(TintRecord(s[9])[2], 2);
    AddB(TintRecord(s[9])[3], 1);
    AddB(TintRecord(s[9])[4], 1);
    Inc(s[0xA]);
    if (Sc()[0xA] == 8) Inc(Sc()[1]);
}

// original 0x4A6640: +1 on once one child is left (+0xB 1).
S08_EXPORT void __cdecl Berserk_WaitChildren(void) {
    unsigned char* const s = Sc();
    if (s[0xB] == 1) Inc(s[1]);
}

// original 0x4A6650: the tint record darker by the same steps, +0xA down; at
// 0 the source's tint released, the target flashed, +1 on.
S08_EXPORT void __cdecl Berserk_Fade(void) {
    unsigned char* const s = Sc();
    AddB(TintRecord(s[9])[2], 0xFE);
    Dec(TintRecord(s[9])[3]);
    Dec(TintRecord(s[9])[4]);
    Dec(s[0xA]);
    if (Sc()[0xA] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    MH_CALL(BattleActor_Flash)(TargetByte());
    Inc(Sc()[1]);
}

// original 0x4A66E0: the children's kind-1 task, a jmp through
// BerserkChild_Kinds (two entries: the ring, the glow) by +1.
S08_EXPORT void __cdecl BerserkChild_Task(void) { CellCall(0x65A82C, 2, Sc()[1], "BerserkChild_Task"); }

// original 0x4A6700: a ring. A three-entry stack table by +2 (_Start, _Grow,
// _Fade); then while +0 bit 0 and +2: the actor matrix, the ring, the pop.
S08_EXPORT void __cdecl BerserkRing_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::BerserkRing_Start, bof3::addr::BerserkRing_Grow,
                                                bof3::addr::BerserkRing_Fade};
    StackCall(kSteps, 3, Sc()[2], "BerserkRing_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::BerserkRing_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4A6750: +9 (the delay) down; at 0 the owner's position, +9 and
// +0xA 0, +2 on.
S08_EXPORT void __cdecl BerserkRing_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    OwnerPosition();
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4A67C0: +0xA up by 4; at 0x20 +2 on.
S08_EXPORT void __cdecl BerserkRing_Grow(void) {
    AddB(Sc()[0xA], 4);
    if (Sc()[0xA] == 0x20) Inc(Sc()[2]);
}

// original 0x4A67E0: +9 up by 4, +0xA down; at 0 the owner's +0xB down and
// the task freed.
S08_EXPORT void __cdecl BerserkRing_Fade(void) {
    AddB(Sc()[9], 4);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A6820: the glow. A four-entry stack table by +2 (_Start,
// MagicFx_CountUp9By2, BarrierLine_Wait, MAGIC060's 0x4B1740); then while +0
// bit 0 and +2: the actor matrix, the glow, the pop.
S08_EXPORT void __cdecl BerserkGlow_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::BerserkGlow_Start, bof3::addr::MagicFx_CountUp9By2,
                                                bof3::addr::BarrierLine_Wait, kGlowPhase3};
    StackCall(kSteps, 4, Sc()[2], "BerserkGlow_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::BerserkGlow_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4A6880: the owner's position, +9 0, +2 on.
S08_EXPORT void __cdecl BerserkGlow_Start(void) {
    OwnerPosition();
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4A68D0: a ring of 64 semi-transparent gouraud quads (tpage 0x35,
// layer 0): the lower edge radius 0x80 on the ground, the upper edge radius
// +9 - sin x 16 + 0xA0 at height +0xA x 8 + 0x20 - sin x 8, the angle of
// each ring (+9 + i + Rand & 3) & 0xF; bright (0xC0, 0x30, 0x30) below,
// dark above; linked at the task's x / z each moved by the lower x << 9.
S08_EXPORT void __cdecl BerserkRing_Draw(void) {
    std::uint32_t r = RandCall();
    unsigned char* s = Sc();
    SetSW(0, 0x80);
    const unsigned first = (((r & 3) + s[9]) & 0xF) << 8;
    SetSW(4, first);
    int v = MH_CALL(Math_Sin)(static_cast<int>(first));
    s = Sc();
    SetSW(2, s[9] - static_cast<unsigned>(Shl(v, 4) >> 12) + 0xA0);
    v = MH_CALL(Math_Sin)(SS(4));
    s = Sc();
    SetSW(8, s[0xA] * 8u + 0x20 - static_cast<unsigned>(Shl(v, 3) >> 12));
    v = MH_CALL(Math_Cos)(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
    SetVW(0xC, 0u - SW(8));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x14, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0x1C, 0);
    for (unsigned i = 1; i < 0x41; ++i) {
        r = RandCall();
        s = Sc();
        unsigned angle = (((r & 3) + s[9] + i) & 0xF) << 8;
        if (i == 0x40) angle = first;
        SetSW(4, angle);
        v = MH_CALL(Math_Sin)(static_cast<int>(angle));
        s = Sc();
        SetSW(2, s[9] - static_cast<unsigned>(Shl(v, 4) >> 12) + 0xA0);
        v = MH_CALL(Math_Sin)(SS(4));
        s = Sc();
        SetSW(8, s[0xA] * 8u + 0x20 - static_cast<unsigned>(Shl(v, 3) >> 12));
        SetVW(0, VW(8));
        SetVW(2, VW(0xA));
        SetVW(4, VW(0xC));
        const int around = static_cast<int>(i << 6);
        v = MH_CALL(Math_Cos)(around);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Sin)(around);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
        SetVW(0xC, 0u - SW(8));
        SetVW(0x10, VW(0x18));
        SetVW(0x12, VW(0x1A));
        v = MH_CALL(Math_Cos)(around);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Sin)(around);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
        s = Sc();
        const std::uint32_t lift = static_cast<std::uint32_t>(Shl(VS(0x18), 9));
        const std::uint32_t x = static_cast<std::uint32_t>(Long(s + 0x34)) + lift;
        const std::uint32_t z = static_cast<std::uint32_t>(Long(s + 0x38)) + lift;
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 0xC0;
        p[0x25] = 0x30;
        p[0x26] = 0x30;
        p[0x34] = 0xC0;
        p[0x35] = 0x30;
        p[0x36] = 0x30;
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x44);
    }
}

// original 0x4A6C10: a glow of sixteen semi-transparent gouraud triangles
// (tpage 0x35, layer 5) round the centre, radius 0x88; the centre dark (1),
// the rim +9 x 8 in red and a quarter of it in green and blue.
S08_EXPORT void __cdecl BerserkGlow_Draw(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    const unsigned char* const s = Sc();
    SetSW(0, 0x88);
    SetSW(6, s[9] * 8u);
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const unsigned x = VW(0x10), y = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        v = MH_CALL(Math_Sin)(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Cos)(a);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = SB(6);
        p[0x15] = static_cast<unsigned char>(Div4(SS(6)));
        p[0x16] = static_cast<unsigned char>(Div4(SS(6)));
        p[0x24] = SB(6);
        p[0x25] = static_cast<unsigned char>(Div4(SS(6)));
        p[0x26] = static_cast<unsigned char>(Div4(SS(6)));
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// ===========================================================================
// MAGIC042 (row 81, Counter and Mind's Eye read one id down)

// original 0x4A6DF0: the kind-2 task. A two-entry stack table by +1:
// Counter_Start, BattleFx_Finish.
S08_EXPORT void __cdecl Counter_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Counter_Start, bof3::addr::BattleFx_Finish};
    StackCall(kPhases, 2, Sc()[1], "Counter_Task");
}

// original 0x4A6E20: the owner's direction and position; +0xB 0, +1 on; one
// mark (kind 1, 0x3E; +1 0) owned by this task and counted in +0xB; sound
// 0x100.
S08_EXPORT void __cdecl Counter_Start(void) {
    Sc()[8] = Owner()[8];
    OwnerPosition();
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    const unsigned slot = NewTask(0x3E);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
    child[1] = 0;
    Inc(self[0xB]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A6EC0: the mark's kind-1 task, a jmp through CounterChild_Kinds
// (one entry) by +1.
S08_EXPORT void __cdecl CounterChild_Task(void) { CellCall(0x65A834, 1, Sc()[1], "CounterChild_Task"); }

// original 0x4A6EE0: a five-entry stack table by +2 (_Start, _Open, _Grow,
// _Swell, _Fade); then while +0 bit 0 and +2: the disc, the spokes (0, +9)
// and three arcs (+0xB + 8, + 0x10, + 0x18; radius +0xA).
S08_EXPORT void __cdecl CounterMark_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::CounterMark_Start, bof3::addr::CounterMark_Open,
                                                bof3::addr::CounterMark_Grow, bof3::addr::CounterMark_Swell,
                                                bof3::addr::CounterMark_Fade};
    StackCall(kSteps, 5, Sc()[2], "CounterMark_Run");
    const unsigned char* s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    Call0(bof3::addr::CounterMark_DrawDisc);
    Call2(bof3::addr::CounterMark_DrawSpokes, 0, Sc()[9]);
    for (unsigned k = 8; k <= 0x18; k += 8) {
        s = Sc();
        Call2(bof3::addr::CounterMark_DrawArc, (s[0xB] + k) & 0xFFu, s[0xA]);
    }
}

// original 0x4A6F90: the owner's direction and position, the screen point
// (CounterMark_Place), +0xB, +9, +0xA 0, +2 on.
S08_EXPORT void __cdecl CounterMark_Start(void) {
    Sc()[8] = Owner()[8];
    OwnerPosition();
    Call0(bof3::addr::CounterMark_Place);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4A7000: +0xB (the spin) up, +0xA up; at 4 +2 on.
S08_EXPORT void __cdecl CounterMark_Open(void) {
    Inc(Sc()[0xB]);
    Inc(Sc()[0xA]);
    if (Sc()[0xA] == 4) Inc(Sc()[2]);
}

// original 0x4A7030: +0xB up, +9 up by 8; at 0x50 +2 on.
S08_EXPORT void __cdecl CounterMark_Grow(void) {
    Inc(Sc()[0xB]);
    AddB(Sc()[9], 8);
    if (Sc()[9] == 0x50) Inc(Sc()[2]);
}

// original 0x4A7060: +0xB up, +9 and +0xA up by 4; at +0xA 0x20 +2 on.
S08_EXPORT void __cdecl CounterMark_Swell(void) {
    Inc(Sc()[0xB]);
    AddB(Sc()[9], 4);
    AddB(Sc()[0xA], 4);
    if (Sc()[0xA] == 0x20) Inc(Sc()[2]);
}

// original 0x4A70A0: +0xB up; +9 down by 0xC unless 0; +0xA down by 4; at 0
// the owner's +0xB down and the task freed.
S08_EXPORT void __cdecl CounterMark_Fade(void) {
    Inc(Sc()[0xB]);
    unsigned char* const s = Sc();
    if (s[9] != 0) AddB(s[9], 0xF4);
    AddB(Sc()[0xA], 0xFC);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A70F0: the task's screen point (BattleActor_UpdateScreenXY);
// for a party target (0..2) moved by a word pair from a table by direction
// (+8 >> 1) and character: CounterMark_FormOffsets by
// CounterMark_FormIndex[0x904B89] while the member's +0x134 has bit 1, else
// CounterMark_MemberOffsets by the member's +0x89. The x is added for a
// direction of 0 or 3 and taken away otherwise; the y added.
S08_EXPORT void __cdecl CounterMark_Place(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    const unsigned target = static_cast<std::uint32_t>(Long(Mem(at::kTarget))) & 0xFFu;
    if (target >= 3) return;
    const unsigned char* const member = PartyRecord(target);
    std::uint32_t table;
    unsigned row;
    if (member[0x134] & 2) {
        table = kFormOffsets;
        row = Mem(kFormBytes + Mem(kFormIndex)[0])[0];
    } else {
        table = kMemberOffsets;
        row = member[0x89];
    }
    unsigned char* const s = Sc();
    const unsigned facing = s[8];
    const unsigned dx = Word(Mem(table + ((facing >> 1) + row * 2) * 4));
    if (facing == 0 || facing == 3) AddWord(s + 0x2E, dx);
    else AddWord(s + 0x2E, 0u - dx);
    AddWord(s + 0x30, Word(Mem(table + ((s[8] >> 1) + row * 2) * 4 + 2)));
}

// original 0x4A7220: from the screen point (+0x2E / +0x30, kept in the
// scratch): per step of the byte angle `first` (then + 0x10, while below
// `first` + 0x20; angle (step & 0x1F) << 7) three semi-transparent gouraud
// lines (layer 3) - one of `radius` (0xC0 to 1), two of two thirds of it a
// pixel above and below (0x80 to 1). Its one caller passes 0.
S08_EXPORT void __cdecl CounterMark_DrawSpokes(unsigned first, unsigned radius) {
    const unsigned char* const s = Sc();
    SetSW(0, Word(s + 0x2E));
    SetSW(2, Word(s + 0x30));
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned step = first & 0xFF;
    const int end = static_cast<int>(step) + 0x20;
    // The original's counter is a byte: from 0xE0 up it wraps below its end
    // forever, emitting lines until the packet buffer runs out.
    if (step >= 0xE0) bof3::Fatal("CounterMark_DrawSpokes: first angle 0x%X never reaches its end", step);
    const int r = static_cast<int>(radius & 0xFFFF);
    const unsigned two_thirds = static_cast<unsigned>(r / 3 * 2);
    do {
        const unsigned angle = (step & 0x1F) << 7;
        unsigned char* p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0));
        SetSW(8, angle);
        PutFloat(p + 0xC, SS(2));
        int v = MH_CALL(Math_Cos)(SS(8));
        PutFloat(p + 0x18, Mul12(v, r) + SS(0));
        v = MH_CALL(Math_Sin)(SS(8));
        PutFloat(p + 0x1C, Mul12(v, r) + SS(2));
        p[4] = 0xC0;
        p[5] = 0xC0;
        p[6] = 0xC0;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        MH_CALL(Gfx_CommitPrim)(3, 0x24);
        for (int side = -1; side <= 1; side += 2) {
            p = Gfx_PacketNext;
            if (side < 0) SetSW(4, two_thirds);
            MH_CALL(Gpu_SetLineG2)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            PutFloat(p + 8, SS(0));
            SetSW(8, angle);
            PutFloat(p + 0xC, SS(2) + side);
            v = MH_CALL(Math_Cos)(SS(8));
            PutFloat(p + 0x18, Mul12(v, SS(4)) + SS(0));
            v = MH_CALL(Math_Sin)(SS(8));
            PutFloat(p + 0x1C, Mul12(v, SS(4)) + SS(2) + side);
            p[4] = 0x80;
            p[5] = 0x80;
            p[6] = 0x80;
            p[0x14] = 1;
            p[0x15] = 1;
            p[0x16] = 1;
            MH_CALL(Gfx_CommitPrim)(3, 0x24);
        }
        step = (step + 0x10) & 0xFF;
    } while (static_cast<int>(step) < end);
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4A7530: from the screen point, four semi-transparent gouraud
// lines (layer 3; 0x80, 0x80, 0x20 to 1) of `radius`, at the word angles
// `first`, + 0x20, + 0x40, + 0x60 (angle (step & 0x7F) << 5).
S08_EXPORT void __cdecl CounterMark_DrawArc(unsigned first, unsigned radius) {
    const unsigned char* const s = Sc();
    SetSW(0, Word(s + 0x2E));
    SetSW(2, Word(s + 0x30));
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    const int end = static_cast<int>(first & 0xFF) + 0x80;
    const int r = static_cast<int>(radius & 0xFFFF);
    for (std::uint16_t step = static_cast<std::uint16_t>(first & 0xFF); static_cast<short>(step) < end;
         step = static_cast<std::uint16_t>(step + 0x20)) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0));
        SetSW(8, (step & 0x7Fu) << 5);
        PutFloat(p + 0xC, SS(2));
        int v = MH_CALL(Math_Cos)(SS(8));
        PutFloat(p + 0x18, Mul12(v, r) + SS(0));
        v = MH_CALL(Math_Sin)(SS(8));
        PutFloat(p + 0x1C, Mul12(v, r) + SS(2));
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x20;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        MH_CALL(Gfx_CommitPrim)(3, 0x24);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4A7690: a disc of sixteen semi-transparent gouraud triangles
// (layer 3) round the screen point, radius Rand & 3 + +0xA / 2; the centre
// 0x60, the rim 1.
S08_EXPORT void __cdecl CounterMark_DrawDisc(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    const std::uint32_t r = RandCall();
    const unsigned char* const s = Sc();
    SetSW(4, (r & 3) + (s[0xA] >> 1));
    SetSW(0, Word(s + 0x2E));
    SetSW(2, Word(s + 0x30));
    for (int a = 0; a < 0x2000;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0));
        PutFloat(p + 0xC, SS(2));
        int v = MH_CALL(Math_Sin)(a);
        PutFloat(p + 0x18, Mul12(v, SS(4)) + SS(0));
        v = MH_CALL(Math_Cos)(a);
        PutFloat(p + 0x1C, Mul12(v, SS(4)) + SS(2));
        a += 0x200;
        v = MH_CALL(Math_Sin)(a);
        PutFloat(p + 0x28, Mul12(v, SS(4)) + SS(0));
        v = MH_CALL(Math_Cos)(a);
        PutFloat(p + 0x2C, Mul12(v, SS(4)) + SS(2));
        p[4] = 0x60;
        p[5] = 0x60;
        p[6] = 0x60;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        MH_CALL(Gfx_CommitPrim)(3, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// ===========================================================================
// MAGIC043 (row 110, WardOfLight and Resist read one id down)

// original 0x4A7830: the kind-2 task. A five-entry stack table by +1
// (Ward_Start, _Tint, _Brighten, _Fade, BattleFx_Finish); then every live
// record of WardMote_Pool run (WardMote_Task).
S08_EXPORT void __cdecl Ward_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Ward_Start, bof3::addr::Ward_Tint, bof3::addr::Ward_Brighten,
                                                 bof3::addr::Ward_Fade, bof3::addr::BattleFx_Finish};
    StackCall(kPhases, 5, Sc()[1], "Ward_Task");
    WalkPool(kWardPool, bof3::addr::WardMote_Task);
}

// original 0x4A78C0: the pool cleared; the source sprite's direction and
// position; +0xB 0, +9 9, +1 on; eight motes from the pool (WardMote_Alloc;
// none when it answers 0xFF): +0x80 this task, +0xB the index 0..7, +9 the
// delay 1, 9, .., 0x39, counted in +0xB; row 26's first 32 CLUT entries from
// their source as they are; Gfx_ClutStripDirty; sound 0x100.
S08_EXPORT void __cdecl Ward_Start(void) {
    ClearPool(kWardPool);
    const unsigned char* const src = Source();
    Sc()[8] = src[8];
    SetLong(Sc() + 0x34, Long(src + 0x34));
    SetLong(Sc() + 0x38, Long(src + 0x38));
    SetLong(Sc() + 0x3C, Long(src + 0x3C));
    Sc()[0xB] = 0;
    Sc()[9] = 9;
    Inc(Sc()[1]);
    unsigned char index = 0;
    for (unsigned delay = 1; delay < 0x41; delay += 8) {
        const unsigned char got = MH_AT(Alloc, bof3::addr::WardMote_Alloc)();
        if (got != 0xFF) {
            unsigned char* const self = Sc();
            unsigned char* const rec = Mem(kWardPool + got * 0x84u);
            SetLong(rec + 0x80, static_cast<std::int32_t>(Key(self)));
            rec[0xB] = index;
            rec[9] = static_cast<unsigned char>(delay);
            Inc(self[0xB]);
        }
        ++index;
    }
    for (unsigned k = 0x1A00; k < 0x1A20; ++k) Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A79D0: +9 down; at 0 the source sprite tinted black, its tint
// slot to +0xA, +9 4, +1 on.
S08_EXPORT void __cdecl Ward_Tint(void) {
    unsigned char* const src = Source();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(src);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(src, 0, 0, 0, 1);
    Sc()[0xA] = tint;
    Sc()[9] = 4;
    Inc(Sc()[1]);
}

// original 0x4A7A30: the tint record +0xA brighter by 2 in each channel, +9
// down; at 0 +9 4, +1 on.
S08_EXPORT void __cdecl Ward_Brighten(void) {
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) AddB(TintRecord(s[0xA])[c], 2);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    Sc()[9] = 4;
    Inc(Sc()[1]);
}

// original 0x4A7AB0: the tint record darker by 2, +9 down; at 0, while two or
// more motes live (+0xB), +9 4 and back to _Brighten; else the tint released,
// the target flashed, and for ability 0x2B (the word 0x904B80) the target's
// buff 1 (MagicFx_ApplyBuff) and a child (kind 1, 0x48; +4 0 when the buff
// took, else 8; +9 1, +0xA 0) counted in +0xB; +1 on.
S08_EXPORT void __cdecl Ward_Fade(void) {
    unsigned char* s = Sc();
    for (unsigned c = 2; c < 5; ++c) AddB(TintRecord(s[0xA])[c], 0xFE);
    Dec(s[9]);
    s = Sc();
    if (s[9] != 0) return;
    if (s[0xB] >= 2) {
        s[9] = 4;
        Dec(Sc()[1]);
        return;
    }
    MH_CALL(Tint_Release)(s[0xA]);
    MH_CALL(BattleActor_Flash)(TargetByte());
    if (Word(Mem(kAbility)) == 0x2B) {
        const unsigned char took = MH_CALL(MagicFx_ApplyBuff)(1, TargetByte());
        const unsigned slot = NewTask(0x48);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[4] = took != 0 ? 0 : 8;
        child[9] = 1;
        child[0xA] = 0;
        Inc(self[0xB]);
    }
    Inc(Sc()[1]);
}

// original 0x4A7BE0: a mote's task, a jmp through WardMote_TaskTable (one
// entry) by +1.
S08_EXPORT void __cdecl WardMote_Task(void) { CellCall(0x65A904, 1, Sc()[1], "WardMote_Task"); }

// original 0x4A7C00: a call through WardMote_Steps (four entries) by +2; then
// while +0 and +2 are set: its matrix, its band, the pop.
S08_EXPORT void __cdecl WardMote_Run(void) {
    CellCall(0x65A908, 4, Sc()[2], "WardMote_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(bof3::addr::WardMote_PushMatrix);
    Call0(bof3::addr::WardMote_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4A7C40: +9 (the delay) down; at 0: the owner's direction; the
// offset (0x20000, 0) turned by it from the owner, 0x1000000 above it; the
// step (-0x1000, 0) turned; +0x14 the angle to the owner from the field's
// kind-2 point (Math_Ratan2 of dx, dz); +9 and +0xA 0, +2 on.
S08_EXPORT void __cdecl WardMote_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0xC, 0x20000);
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    {
        const unsigned char* const o = Owner();
        unsigned char* const s = Sc();
        SetLong(s + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(o + 0x34)) +
                                                    static_cast<std::uint32_t>(Long(s + 0xC))));
        SetLong(s + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(o + 0x38)) +
                                                    static_cast<std::uint32_t>(Long(s + 0x10))));
        SetLong(s + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(o + 0x3C)) + 0x1000000u));
        SetLong(s + 0xC, static_cast<std::int32_t>(0xFFFFF000u));
        SetLong(s + 0x10, 0);
    }
    Turn(Sc());
    const unsigned char* const o = Owner();
    const int dz = static_cast<int>(static_cast<std::uint32_t>(Long(o + 0x38)) - static_cast<std::uint32_t>(Field_Kind2Z));
    const int dx = static_cast<int>(static_cast<std::uint32_t>(Long(o + 0x34)) - static_cast<std::uint32_t>(Field_Kind2X));
    const int angle = Ratan2(dx, dz);
    SetLong(Sc() + 0x14, angle);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// The mote's steps: the position +0x34 / +0x38 on by the step +0xC / +0x10,
// +9 up.
namespace {
void MoteMove() {
    unsigned char* const s = Sc();
    AddLong(s + 0x34, static_cast<std::uint32_t>(Long(s + 0xC)));
    AddLong(s + 0x38, static_cast<std::uint32_t>(Long(s + 0x10)));
    Inc(s[9]);
}
}  // namespace

// original 0x4A7D50: moved; +0xA up by 2; at 0x10 +2 on.
S08_EXPORT void __cdecl WardMote_Open(void) {
    MoteMove();
    AddB(Sc()[0xA], 2);
    if (Sc()[0xA] == 0x10) Inc(Sc()[2]);
}

// original 0x4A7DA0: moved; at +9 0x18 +2 on.
S08_EXPORT void __cdecl WardMote_Fly(void) {
    MoteMove();
    if (Sc()[9] == 0x18) Inc(Sc()[2]);
}

// original 0x4A7DE0: moved; +0xA down by 2; at 0 the owner's +0xB down and
// the record's +0..+4 cleared (the pool slot free).
S08_EXPORT void __cdecl WardMote_End(void) {
    MoteMove();
    AddB(Sc()[0xA], 0xFE);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    unsigned char* const s = Sc();
    for (unsigned k = 0; k < 5; ++k) s[k] = 0;
}

// original 0x4A7E60: the mote's matrix pushed: Camera_Matrix x the rotation
// (0x400, WardMote_Tilts[+8], 0), translation RotTrans of (x >> 9 - 0x4000,
// z >> 9 - 0x4000, -(height / 2)). One MATRIX block as the original lays it
// out on its stack, RotTrans writing its translation at +0x14.
namespace {
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
}  // namespace
S08_EXPORT void __cdecl WardMote_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sc();
    const short rot[4] = {0x400, static_cast<short>(Word(Mem(kWardTilts + s[8] * 2u))), 0, 0};
    short v[4];
    v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(s + 0x3E) / 2));
    v[3] = 0;
    Matrix m;
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S08_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x4A7F10: a band of eight semi-transparent gouraud quads (tpage
// 0x35, layer 0, linked at the mote) between a ring of radius +9 x 12 and one
// of +9 x 8 on the mote's plane; the inner edge coloured from +0xA - (Rand &
// 7) + 4 times it in each channel for ability 0x2B, else x 4, x 8, x 4 -, the
// outer edge 1.
S08_EXPORT void __cdecl WardMote_Draw(void) {
    DrawMode(0x35);
    LinkAtTask(0, 0xC);
    const unsigned char* s = Sc();
    SetSW(0, s[9] * 8u);
    SetSW(2, s[9] * 12u);
    unsigned last;
    if (Word(Mem(kAbility)) == 0x2B) {
        std::uint32_t r = RandCall();
        SetSW(6, ((r & 7) + 4) * Sc()[0xA]);
        r = RandCall();
        SetSW(8, ((r & 7) + 4) * Sc()[0xA]);
        r = RandCall();
        last = ((r & 7) + 4) * Sc()[0xA];
    } else {
        SetSW(6, s[0xA] * 4u);
        SetSW(8, s[0xA] * 8u);
        last = s[0xA] * 4u;
    }
    SetSW(0xA, last);
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(2, static_cast<unsigned>(Mul12(v, SS(2))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x1C, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0x14, 0);
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetVW(8, VW(0));
        SetVW(0xA, VW(2));
        v = MH_CALL(Math_Sin)(a);
        SetVW(0, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(a);
        {
            const int inner = Mul12(v, SS(2));
            const unsigned x = VW(0x10);
            SetVW(2, static_cast<unsigned>(inner));
            SetVW(0x18, x);
            SetVW(0x1A, VW(0x12));
        }
        v = MH_CALL(Math_Sin)(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Cos)(a);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        p[4] = SB(6);
        p[5] = SB(8);
        p[6] = SB(0xA);
        p[0x14] = SB(6);
        p[0x15] = SB(8);
        p[0x16] = SB(0xA);
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        p[0x34] = 1;
        p[0x35] = 1;
        p[0x36] = 1;
        LinkAtTask(0, 0x44);
    }
}

// original 0x4A81F0: the first of WardMote_Pool's 64 records with bit 0 of
// +0 clear gets it, its index in al; 0xFF when all are taken.
S08_EXPORT unsigned char __cdecl WardMote_Alloc(void) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(kWardPool + i * 0x84u);
        if (rec[0] & 1) continue;
        rec[0] = static_cast<unsigned char>(rec[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// ===========================================================================
// MAGIC044 (row 96, Evil Eye read one id down)

// original 0x4A8250: the kind-2 task. A two-entry stack table by +1:
// EvilEye_Start, BattleFx_Finish.
S08_EXPORT void __cdecl EvilEye_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::EvilEye_Start, bof3::addr::BattleFx_Finish};
    StackCall(kPhases, 2, Sc()[1], "EvilEye_Task");
}

// original 0x4A8280: the owner's direction and position; +0xB 0, +1 on; two
// beams (kind 1, 6; +1 0, +4 the beam 0 / 1) owned by this task and counted
// in +0xB; row 26's third CLUT entries 1..15 from their source with STP,
// entry 0 cleared; Gfx_ClutStripDirty; sound 0x100.
S08_EXPORT void __cdecl EvilEye_Start(void) {
    Sc()[8] = Owner()[8];
    OwnerPosition();
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    for (unsigned beam = 0; beam < 2; ++beam) {
        const unsigned slot = NewTask(6);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[1] = 0;
        child[4] = static_cast<unsigned char>(beam);
        Inc(self[0xB]);
    }
    for (unsigned k = 0x1A21; k < 0x1A30; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStrip[0x1A20] = 0;
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A8360: the children's kind-1 task, a jmp through
// EvilEyeChild_Kinds (two entries: the beam, the spark) by +1.
S08_EXPORT void __cdecl EvilEyeChild_Task(void) { CellCall(0x65A924, 2, Sc()[1], "EvilEyeChild_Task"); }

// original 0x4A8380: a call through EvilEyeBeam_Steps (four entries) by +2;
// then while +0 and +2 are set, the trail drawn: thin for beam 1 (+4 not 0),
// wide for beam 0.
S08_EXPORT void __cdecl EvilEyeBeam_Run(void) {
    CellCall(0x65A92C, 4, Sc()[2], "EvilEyeBeam_Run");
    const unsigned char* const s = Sc();
    const bool thin = s[4] != 0;
    if (s[0] == 0 || s[2] == 0) return;
    Call0(thin ? bof3::addr::EvilEyeBeam_DrawThin : bof3::addr::EvilEyeBeam_DrawWide);
}

// original 0x4A83D0: the owner's direction; an offset by the acting actor (a
// party member (0x4000, 0), height 0x1000000; an enemy by its record's +0x8C:
// 0x9A (0x10000, 0) and 0x2000000, 0xB5 (-0xE000, 0) and 0x3800000, any other
// (0x2000, 0) and 0x1C00000) in +0xC / +0x10 / +0x14, turned by the direction,
// added to the owner's position; +0xC the angle to the source sprite
// (Math_Ratan2 of dx, dz), +0x10 0; the screen point, twice into the trail's
// first two points; +0xB 0, +9 0x10, +0xA 1, +2 on.
S08_EXPORT void __cdecl EvilEyeBeam_Start(void) {
    const unsigned char* const src = Source();
    Sc()[8] = Owner()[8];
    const unsigned actor = static_cast<std::uint32_t>(Long(Mem(at::kActor))) & 0xFFu;
    std::uint32_t dx, height;
    if (actor < 3) {
        dx = 0x4000;
        height = 0x1000000;
    } else {
        const unsigned kind = EnemyRecord(actor)[0x8C];
        if (kind == 0x9A) {
            dx = 0x10000;
            height = 0x2000000;
        } else if (kind == 0xB5) {
            dx = 0xFFFF2000u;
            height = 0x3800000;
        } else {
            dx = 0x2000;
            height = 0x1C00000;
        }
    }
    SetLong(Sc() + 0xC, static_cast<std::int32_t>(dx));
    SetLong(Sc() + 0x10, 0);
    SetLong(Sc() + 0x14, static_cast<std::int32_t>(height));
    Turn(Sc());
    {
        const unsigned char* const o = Owner();
        unsigned char* const s = Sc();
        SetLong(s + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(o + 0x34)) +
                                                    static_cast<std::uint32_t>(Long(s + 0xC))));
        SetLong(s + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(o + 0x38)) +
                                                    static_cast<std::uint32_t>(Long(s + 0x10))));
        SetLong(s + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(o + 0x3C)) +
                                                    static_cast<std::uint32_t>(Long(s + 0x14))));
    }
    {
        const unsigned char* const s = Sc();
        const int ddz = static_cast<int>(static_cast<std::uint32_t>(Long(src + 0x38)) - static_cast<std::uint32_t>(Long(s + 0x38)));
        const int ddx = static_cast<int>(static_cast<std::uint32_t>(Long(src + 0x34)) - static_cast<std::uint32_t>(Long(s + 0x34)));
        const int angle = Ratan2(ddx, ddz);
        SetLong(Sc() + 0xC, angle);
    }
    SetLong(Sc() + 0x10, 0);
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* const s = Sc();
    SetWord(Trail(s[4], 0), Word(s + 0x2E));
    SetWord(Trail(s[4], 0) + 2, Word(s + 0x30));
    SetWord(Trail(s[4], 1), Word(s + 0x2E));
    SetWord(Trail(s[4], 1) + 2, Word(s + 0x30));
    s[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4A85C0: +0xA (the trail's length) up to 0x10; a step toward the
// source sprite (MagicFx_StepTowardPoint, speed 0xC0), the old heading +0xC
// kept in +0x10 and the new one (Math_Ratan2 of dx, dz) in +0xC. Snapped onto
// the source, +2 on, when MagicFx_NearSprite(source, 0x8000) answers, or when
// the turn |(+0x10 & 0xFFF) - (+0xC & 0xFFF)| is between 0x600 and 0xA00
// (both open). Then the screen point, the trail recorded, and target flags
// 0x10 from beam 0 unless +2 is 1.
S08_EXPORT void __cdecl EvilEyeBeam_Seek(void) {
    {
        unsigned char* const s = Sc();
        if (s[0xA] < 0x10) Inc(s[0xA]);
    }
    unsigned char* const src = Source();
    {
        const std::uint32_t x = static_cast<std::uint32_t>((Long(src + 0x34) >> 9) - 0x4000);
        const std::uint32_t z = static_cast<std::uint32_t>((Long(src + 0x38) >> 9) - 0x4000);
        const std::uint32_t y =
            static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(src + 0x3C)) + 0x800000u) >> 0x11);
        MH_CALL(MagicFx_StepTowardPoint)(x, z, y, 0, 0xC0);
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x10, Long(s + 0xC));
        const int ddz = static_cast<int>(static_cast<std::uint32_t>(Long(src + 0x38)) - static_cast<std::uint32_t>(Long(s + 0x38)));
        const int ddx = static_cast<int>(static_cast<std::uint32_t>(Long(src + 0x34)) - static_cast<std::uint32_t>(Long(s + 0x34)));
        const int angle = Ratan2(ddx, ddz);
        SetLong(Sc() + 0xC, angle);
    }
    bool snap;
    if (MH_CALL(MagicFx_NearSprite)(src, 0x8000) != 0) {
        snap = true;
    } else {
        unsigned char* const s = Sc();
        SetLong(s + 0x10, static_cast<std::int32_t>((static_cast<std::uint32_t>(Long(s + 0x10)) & 0xFFF) -
                                                    (static_cast<std::uint32_t>(Long(s + 0xC)) & 0xFFF)));
        if (Long(s + 0x10) < 0) SetLong(s + 0x10, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(Long(s + 0x10))));
        const std::int32_t turn = Long(s + 0x10);
        snap = turn > 0x600 && turn < 0xA00;
    }
    if (snap) {
        SetLong(Sc() + 0x34, Long(src + 0x34));
        SetLong(Sc() + 0x38, Long(src + 0x38));
        Inc(Sc()[2]);
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::EvilEyeBeam_Record);
    const unsigned char* const s = Sc();
    if (s[2] == 1 || s[4] != 0) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
}

// original 0x4A8730: while the trail is shorter than 0x10, +0xB and +0xA up,
// the screen point and the trail recorded; every fourth frame a spark (kind 1,
// 6; +1 1) at the beam's position owned by the beam's owner (whose +0xB
// counts it), unless BattleTask_Create answers 0xFF; at a length of 0x10 +9
// 0x10 and +2 on.
S08_EXPORT void __cdecl EvilEyeBeam_Trail(void) {
    if (Sc()[0xA] < 0x10) {
        Inc(Sc()[0xB]);
        Inc(Sc()[0xA]);
        MH_CALL(BattleActor_UpdateScreenXY)();
        Call0(bof3::addr::EvilEyeBeam_Record);
    }
    if ((Frame_Counter & 3) == 0) {
        const unsigned slot = NewTask(6);
        if (slot != 0xFF) {
            const unsigned char* const s = Sc();
            unsigned char* const o = Owner();
            unsigned char* const child = TaskSlot(slot);
            SetLong(child + 0x80, static_cast<std::int32_t>(Key(o)));
            child[1] = 1;
            SetLong(child + 0x34, Long(s + 0x34));
            SetLong(child + 0x38, Long(s + 0x38));
            SetLong(child + 0x3C, Long(s + 0x3C));
            Inc(o[0xB]);
        }
    }
    unsigned char* const s = Sc();
    if (s[0xA] != 0x10) return;
    s[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4A87F0: the trail erased from its head (EvilEyeBeam_Erase), +9
// down; at 0 the owner's +0xB down and the task freed.
S08_EXPORT void __cdecl EvilEyeBeam_End(void) {
    Call0(bof3::addr::EvilEyeBeam_Erase);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// The beam's trail drawn as a ribbon of semi-transparent gouraud quads
// (layer 3, tpage 0x35): from the first point at or after +0xB that is not
// erased (x 0xFFFF), each segment up to +0xA; each quad's corners are the
// segment's two points pushed out along the perpendicular of the segment's
// Math_Ratan2 heading by the width (scratch word 0): `edges` +1 the left
// (heading + 0x400), -1 the right (heading - 0x400), 0 both. The first
// segment's heading is its own; each quad takes the previous segment's at its
// near end.
namespace {
// The erased points skipped, the first segment's heading into scratch word 2,
// the first point in scratch 8 / 0xA; j the point after it.
int TrailStart(const unsigned char* s) {
    int j = s[0xB];
    const unsigned beam = s[4];
    while (Word(Trail(beam, j)) == 0xFFFF) ++j;
    const unsigned x0 = Word(Trail(s[4], j)), y0 = Word(Trail(s[4], j) + 2);
    const unsigned x1 = Word(Trail(s[4], j) + 4), y1 = Word(Trail(s[4], j) + 6);
    SetSW(0xC, x0);
    SetSW(0xE, y0);
    SetSW(8, x1);
    SetSW(0xA, y1);
    const int heading = Ratan2(static_cast<short>(x1) - static_cast<short>(x0), static_cast<short>(y1) - static_cast<short>(y0));
    SetSW(2, static_cast<unsigned>(heading));
    const unsigned char* const t = Sc();
    SetSW(8, Word(Trail(t[4], j)));
    SetSW(0xA, Word(Trail(t[4], j) + 2));
    return j + 1;
}
// One corner pair: (x, y) + the width along `angle` into +xo / +yo.
void Corner(unsigned char* p, unsigned xo, unsigned angle, unsigned bx, unsigned by) {
    SetSW(4, angle & 0xFFF);
    int v = MH_CALL(Math_Sin)(static_cast<short>(angle & 0xFFF));
    PutFloat(p + xo, Mul12(v, SS(0)) + SS(bx));
    v = MH_CALL(Math_Cos)(SS(4));
    PutFloat(p + xo + 4, Mul12(v, SS(0)) + SS(by));
}
int SegmentHeading() {
    return Ratan2(SS(8) - SS(0xC), SS(0xA) - SS(0xE));
}
}  // namespace

// original 0x4A8820: beam 1's trail, 2 wide, both edges pushed out; the near
// end shaded 0xCD - 12 j in blue, the far end 0xC1 - 12 j (scratch word 4
// left at 0x61 - 6 j).
S08_EXPORT void __cdecl EvilEyeBeam_DrawThin(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    const unsigned char* s = Sc();
    SetSW(0, 2);
    int j = TrailStart(s);
    s = Sc();
    if (j < s[0xA]) {
        do {
            SetSW(0xC, SW(8));
            SetSW(0xE, SW(0xA));
            unsigned char* const p = Gfx_PacketNext;
            SetSW(8, Word(Trail(s[4], j)));
            SetSW(0xA, Word(Trail(s[4], j) + 2));
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            Corner(p, 8, SW(2) + 0x400u, 0xC, 0xE);
            Corner(p, 0x28, SW(2) - 0x400u, 0xC, 0xE);
            const int heading = SegmentHeading();
            SetSW(2, static_cast<unsigned>(heading));
            Corner(p, 0x18, static_cast<unsigned>(heading) + 0x400u, 8, 0xA);
            Corner(p, 0x38, SW(2) - 0x400u, 8, 0xA);
            const unsigned twelve = static_cast<unsigned>(12 * j);
            SetSW(6, 0xCD - twelve);
            p[4] = 1;
            p[5] = 1;
            p[6] = SB(6);
            p[0x24] = 1;
            p[0x25] = 1;
            p[0x26] = SB(6);
            SetSW(4, 0x61 - static_cast<unsigned>(6 * j));
            SetSW(6, 0xC1 - twelve);
            p[0x14] = 1;
            p[0x15] = 1;
            p[0x16] = SB(6);
            p[0x34] = 1;
            p[0x35] = 1;
            p[0x36] = SB(6);
            MH_CALL(Gfx_CommitPrim)(3, 0x44);
            s = Sc();
            ++j;
        } while (j < s[0xA]);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4A8C70: beam 0's trail, 0xC wide, drawn twice: the left edge
// (heading + 0x400) against the trail itself, shaded 0x67 - 6 j near and
// 0x61 - 6 j far in blue; then the right edge (heading - 0x400), 0x89 - 8 j
// and 0x81 - 8 j.
S08_EXPORT void __cdecl EvilEyeBeam_DrawWide(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    const unsigned char* s = Sc();
    SetSW(0, 0xC);
    for (int pass = 0; pass < 2; ++pass) {
        const unsigned turn = pass == 0 ? 0x400u : 0u - 0x400u;
        int j = TrailStart(s);
        s = Sc();
        if (j >= s[0xA]) continue;
        do {
            SetSW(0xC, SW(8));
            SetSW(0xE, SW(0xA));
            unsigned char* const p = Gfx_PacketNext;
            SetSW(8, Word(Trail(s[4], j)));
            SetSW(0xA, Word(Trail(s[4], j) + 2));
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            Corner(p, 8, SW(2) + turn, 0xC, 0xE);
            PutFloat(p + 0x28, SS(0xC));
            PutFloat(p + 0x2C, SS(0xE));
            const int heading = SegmentHeading();
            SetSW(2, static_cast<unsigned>(heading));
            Corner(p, 0x18, static_cast<unsigned>(heading) + turn, 8, 0xA);
            PutFloat(p + 0x38, SS(8));
            PutFloat(p + 0x3C, SS(0xA));
            const unsigned near = pass == 0 ? 0x67 - static_cast<unsigned>(6 * j) : 0x89 - static_cast<unsigned>(8 * j);
            const unsigned far = pass == 0 ? 0x61 - static_cast<unsigned>(6 * j) : 0x81 - static_cast<unsigned>(8 * j);
            SetSW(6, near);
            p[4] = 1;
            p[5] = 1;
            p[6] = 1;
            p[0x25] = 1;
            p[0x24] = SB(6);
            p[0x26] = SB(6);
            SetSW(6, far);
            p[0x14] = 1;
            p[0x15] = 1;
            p[0x16] = 1;
            p[0x34] = SB(6);
            p[0x35] = 1;
            p[0x36] = SB(6);
            MH_CALL(Gfx_CommitPrim)(3, 0x44);
            s = Sc();
            ++j;
        } while (j < s[0xA]);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// The trail pushed back one point, from +0xA - 1 down to +0xB (points of beam
// +4 in EvilEyeBeam_Trails, 32 a beam, unchecked).
namespace {
void TrailShift(const unsigned char* s) {
    for (int i = static_cast<int>(s[0xA]) - 1; i >= static_cast<int>(s[0xB]); --i) {
        SetWord(Trail(s[4], i + 1), Word(Trail(s[4], i)));
        SetWord(Trail(s[4], i + 1) + 2, Word(Trail(s[4], i) + 2));
    }
}
}  // namespace

// original 0x4A9350: the trail pushed back, the screen point (+0x2E, +0x30)
// into point +0xB.
S08_EXPORT void __cdecl EvilEyeBeam_Record(void) {
    const unsigned char* const s = Sc();
    TrailShift(s);
    SetWord(Trail(s[4], s[0xB]), Word(s + 0x2E));
    SetWord(Trail(s[4], s[0xB]) + 2, Word(s + 0x30));
}

// original 0x4A93E0: the trail pushed back, point +0xB erased (0xFFFF, 0xFFFF).
S08_EXPORT void __cdecl EvilEyeBeam_Erase(void) {
    const unsigned char* const s = Sc();
    TrailShift(s);
    SetWord(Trail(s[4], s[0xB]), 0xFFFF);
    SetWord(Trail(s[4], s[0xB]) + 2, 0xFFFF);
}

// original 0x4A9470: a spark. A call through EvilEyeSpark_Steps (three
// entries) by +2; then while +0 and +2 are set, the spark drawn.
S08_EXPORT void __cdecl EvilEyeSpark_Run(void) {
    CellCall(0x65A93C, 3, Sc()[2], "EvilEyeSpark_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(bof3::addr::EvilEyeSpark_Draw);
}

// original 0x4A94A0: the screen point, 8 lower; +0x5D..+0x5F each Rand & 3 +
// 1 (the colour weights); +9 0, +0xA 0x10, +2 on.
S08_EXPORT void __cdecl EvilEyeSpark_Start(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    AddWord(Sc() + 0x30, 8);
    for (unsigned k = 0x5D; k < 0x60; ++k) {
        const std::uint32_t r = RandCall();
        Sc()[k] = static_cast<unsigned char>((r & 3) + 1);
    }
    Sc()[9] = 0;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4A9510: a Rand whose answer is tested and dropped; the screen
// point up and left by 1; +9 up by 4; at 0x10 +2 on.
S08_EXPORT void __cdecl EvilEyeSpark_Grow(void) {
    RandCall();
    AddWord(Sc() + 0x2E, 0xFFFF);
    AddWord(Sc() + 0x30, 0xFFFF);
    AddB(Sc()[9], 4);
    if (Sc()[9] == 0x10) Inc(Sc()[2]);
}

// original 0x4A9550: the screen point up by 2, +9 up; a Rand dropped as in
// _Grow; left by 1; +0xA down; at 0 the owner's +0xB down and the task freed.
S08_EXPORT void __cdecl EvilEyeSpark_Fade(void) {
    AddWord(Sc() + 0x30, 0xFFFE);
    Inc(Sc()[9]);
    RandCall();
    AddWord(Sc() + 0x2E, 0xFFFF);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A95A0: one semi-transparent textured quad (tpage 0x55, layer
// 2, linked at the task) round the screen point, radius +9 x 2 at the angles
// 0x200, 0x600, 0xE00, 0xA00; texture page (0, 2, 0x340, 0x100), CLUT (0x20,
// 0x1FA), u / v 0x80..0xA0 x 0x20..0x40; the colour +0x5D..+0x5F (signed)
// x +0xA.
S08_EXPORT void __cdecl EvilEyeSpark_Draw(void) {
    DrawMode(0x55);
    LinkAtTask(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SetSW(0, Sc()[9] * 2u);
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned k = 0; k < 4; ++k) {
        int v = MH_CALL(Math_Cos)(kAngles[k]);
        PutFloat(p + 8 + 0x10 * k, Mul12(v, SS(0)) + S16(Sc() + 0x2E));
        v = MH_CALL(Math_Sin)(kAngles[k]);
        PutFloat(p + 0xC + 0x10 * k, Mul12(v, SS(0)) + S16(Sc() + 0x30));
    }
    SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 2, 0x340, 0x100));
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0x20, 0x1FA));
    p[0x14] = 0x80;
    p[0x34] = 0x80;
    p[0x15] = 0x20;
    p[0x24] = 0xA0;
    p[0x25] = 0x20;
    p[0x35] = 0x40;
    p[0x44] = 0xA0;
    p[0x45] = 0x40;
    const unsigned char* const s = Sc();
    SetSW(0xA, static_cast<unsigned>(static_cast<signed char>(s[0x5D]) * static_cast<int>(s[0xA])));
    SetSW(0xC, static_cast<unsigned>(static_cast<signed char>(s[0x5E]) * static_cast<int>(s[0xA])));
    const unsigned char red = SB(0xA);
    SetSW(0xE, static_cast<unsigned>(static_cast<signed char>(s[0x5F]) * static_cast<int>(s[0xA])));
    p[4] = red;
    p[5] = SB(0xC);
    p[6] = SB(0xE);
    LinkAtTask(2, 0x48);
}

void MagicS08_Inject() {
    if (bof3::WantsShadow("magic_s08")) magic_s08::SelfTest();
    BOF3_INJECT(Berserk_Task);
    BOF3_INJECT(Berserk_Start);
    BOF3_INJECT(Berserk_Tint);
    BOF3_INJECT(Berserk_Brighten);
    BOF3_INJECT(Berserk_WaitChildren);
    BOF3_INJECT(Berserk_Fade);
    BOF3_INJECT(BerserkChild_Task);
    BOF3_INJECT(BerserkRing_Run);
    BOF3_INJECT(BerserkRing_Start);
    BOF3_INJECT(BerserkRing_Grow);
    BOF3_INJECT(BerserkRing_Fade);
    BOF3_INJECT(BerserkGlow_Run);
    BOF3_INJECT(BerserkGlow_Start);
    BOF3_INJECT(BerserkRing_Draw);
    BOF3_INJECT(BerserkGlow_Draw);
    BOF3_INJECT(Counter_Task);
    BOF3_INJECT(Counter_Start);
    BOF3_INJECT(CounterChild_Task);
    BOF3_INJECT(CounterMark_Run);
    BOF3_INJECT(CounterMark_Start);
    BOF3_INJECT(CounterMark_Open);
    BOF3_INJECT(CounterMark_Grow);
    BOF3_INJECT(CounterMark_Swell);
    BOF3_INJECT(CounterMark_Fade);
    BOF3_INJECT(CounterMark_Place);
    BOF3_INJECT(CounterMark_DrawSpokes);
    BOF3_INJECT(CounterMark_DrawArc);
    BOF3_INJECT(CounterMark_DrawDisc);
    BOF3_INJECT(Ward_Task);
    BOF3_INJECT(Ward_Start);
    BOF3_INJECT(Ward_Tint);
    BOF3_INJECT(Ward_Brighten);
    BOF3_INJECT(Ward_Fade);
    BOF3_INJECT(WardMote_Task);
    BOF3_INJECT(WardMote_Run);
    BOF3_INJECT(WardMote_Launch);
    BOF3_INJECT(WardMote_Open);
    BOF3_INJECT(WardMote_Fly);
    BOF3_INJECT(WardMote_End);
    BOF3_INJECT(WardMote_PushMatrix);
    BOF3_INJECT(WardMote_Draw);
    BOF3_INJECT(WardMote_Alloc);
    BOF3_INJECT(EvilEye_Task);
    BOF3_INJECT(EvilEye_Start);
    BOF3_INJECT(EvilEyeChild_Task);
    BOF3_INJECT(EvilEyeBeam_Run);
    BOF3_INJECT(EvilEyeBeam_Start);
    BOF3_INJECT(EvilEyeBeam_Seek);
    BOF3_INJECT(EvilEyeBeam_Trail);
    BOF3_INJECT(EvilEyeBeam_End);
    BOF3_INJECT(EvilEyeBeam_DrawThin);
    BOF3_INJECT(EvilEyeBeam_DrawWide);
    BOF3_INJECT(EvilEyeBeam_Record);
    BOF3_INJECT(EvilEyeBeam_Erase);
    BOF3_INJECT(EvilEyeSpark_Run);
    BOF3_INJECT(EvilEyeSpark_Start);
    BOF3_INJECT(EvilEyeSpark_Grow);
    BOF3_INJECT(EvilEyeSpark_Fade);
    BOF3_INJECT(EvilEyeSpark_Draw);
}
