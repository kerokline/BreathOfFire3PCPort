// Five spell overlays compiled into the exe, round nine group S10
// (docs/magic_s10.md): the PSX's MAGIC052, MAGIC053, MAGIC054, MAGIC055 and
// MAGIC056.EMI, Magic_Rows rows 107, 134, 56, 65 and 112. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Ovum, Lavaburst,
// Howling, Ebonfire and Sacrifice; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC052 0x4ABCA0..0x4ABF16: children (kind 1, 0x50) near the source
//     sprite, one every eighth frame, each playing animation 5 with a sound
//     and freeing itself;
//   - MAGIC053 0x4ABF20..0x4ACAA6: eight children (kind 1, 0x5E) that fly up
//     round the side's centre, shake the camera and fill a pool of 32
//     records of its own (0x67F700), each a textured quad that spreads and
//     fades; the children draw a glow of 64 triangles;
//   - MAGIC054 0x4ACAB0..0x4ACFDA: every live actor of the target's side
//     copied into a child (kind 1, 0x2C) that plays its animation and scales
//     out and back; in an event battle whose byte 0x904AAA is 0x37, only
//     the target flags and a jump to the table's last step;
//   - MAGIC055 0x4ACFE0..0x4AD892: the disc-and-fan effect (FxDiscFan_Task's
//     twin) with its own start and six rings of its own draw;
//   - MAGIC056 0x4AD8A0..0x4AE7B6: the acting actor copied into a child that
//     darkens, splits off a ring (eight bands of 32 quads) and a disc (16
//     triangles), and comes back.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s10.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

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

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, words) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kActorRecord = 0x904B3C;   // unsigned char *: the acting actor's sprite record
constexpr std::uint32_t kEventBattle = 0x904AAA;   // u8: not 0 in an event battle (its kind)
constexpr std::uint32_t kFrameSet = 0x9039D8;      // the sprite frame-offset table pointer (sprite_pose.h)
constexpr std::uint32_t kFrameSetBattle = 0x8B3580;
constexpr std::uint32_t kFrameSetEffect = 0x8E3580;
constexpr std::uint32_t kFrameSetBlast = 0x8C5D80;

// MAGIC053's own pool of 32 task-like records (Lavaburst_Pool) and the byte
// table its start reads (Lavaburst_ChildDelays), both read in place.
constexpr std::uint32_t kPool = 0x67F700;
constexpr unsigned kPoolRecords = 32;
constexpr std::uint32_t kChildDelays = 0x65A9F4;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned char ActorByte() { return Mem(at::kActor)[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void AddLong(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v));
}

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `imul` alone: the 32-bit product, wrapped.
std::uint32_t Mul(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }
// `shl n` on a dword.
std::uint32_t Shl(int v, unsigned n) { return static_cast<std::uint32_t>(v) << n; }

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
unsigned char* PoolRecord(unsigned index) { return Mem(kPool + index * 0x84u); }
// The originals index the records by the battle index, unchecked: the party
// member's by index, the enemy's by index - 3 (a party index lands below the
// enemy records).
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}
// MAGIC054's walk counts the enemies from 0.
unsigned char* EnemyByOrder(unsigned i) { return Mem(at::kEnemies + i * at::kEnemyStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// A task's copy of a record's first 0x80 bytes (`rep movsd`: dword by dword,
// forward).
void CopyRecord(unsigned char* to, const unsigned char* from) {
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(to + k, Long(from + k));
}

// This group's functions, and other groups' (bof3::addr), called by address,
// as the originals call them: in the game the jmp Inject put there (or
// Capcom's code), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
using TaskFn = void (__cdecl*)(unsigned char*);
using AnimFn = void (__cdecl*)(std::uint32_t, std::uint32_t);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = bof3::addr::Battle_TurnVectorC;
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }
// The engine's enemy animations, unnamed, in no group: an enemy by battle
// index - 3 (the index's low byte) given animation `arg` through
// BattleEnemy_SetAnimation; 0x435A20 makes it Sprite_Current for the call,
// 0x435A70 does not (docs/magic_s01.md).
constexpr std::uint32_t kEnemyAnimCurrent = bof3::addr::BattleEnemy_SetAnimationAs;   // 0x435A20
constexpr std::uint32_t kEnemyAnim = bof3::addr::BattleEnemy_SetAnimationOf;   // 0x435A70

// The phase handlers and callees of other units (docs/magic_s10.md section 3)
// are ours now and called by name: S38's MagicFx_CountDownFlag10 and
// MeteorStrikeRock_DrawRing, S11's MagicFx_UncountAndFree, S37's
// MagicFx_PushRecordMatrix, MagicFx_FreeCurrentRecord and CombustionMote_Start.

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// The callees with the arguments the originals push: both projections get a
// flag pointer past the depth, which ours does not read.
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S10_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// Prim_VertexScratch's first three SVECTORs projected to +8, +0x18, +0x28.
void Rtp3(unsigned char* prim) {
    long p, flag;
    S10_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}
// All four projected to +8, +8 + step, +8 + 2 step, +8 + 3 step.
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S10_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}

}  // namespace

#define S10_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC052 (row 107, Ovum read one id down)

// original 0x4ABCA0: the kind-2 task. A three-entry stack table by +1:
// Ovum_Start, Ovum_Spawn, Ovum_End.
S10_EXPORT void __cdecl Ovum_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Ovum_Start, addr::Ovum_Spawn, addr::Ovum_End};
    Dispatch(kPhases, 3, Sc()[1], "Ovum_Task");
}

// original 0x4ABCD0: +9 8 (the children), +0xA 4 (the first delay), +1 on.
S10_EXPORT void __cdecl Ovum_Start(void) {
    Sc()[9] = 8;
    Sc()[0xA] = 4;
    Inc(Sc()[1]);
}

// original 0x4ABCF0: +0xA down; once it was 0, a child (kind 1, 0x50) owned
// by this task, +1 / +2 0, at the source sprite's screen point jittered by
// Rand (the first Rand's answer dropped), its +9 this task's +9; +9 down,
// and +0xA 8 until it was 0, then 0x2D and +1 on.
S10_EXPORT void __cdecl Ovum_Spawn(void) {
    {
        unsigned char* const s = Sc();
        const unsigned char was = s[0xA];
        s[0xA] = static_cast<unsigned char>(was - 1);
        if (was != 0) return;
    }
    const unsigned slot = NewTask(0x50);
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(Sc())));
    child[1] = 0;
    child[2] = 0;
    RandCall();
    const std::uint32_t r1 = RandCall();
    SetWord(child + 0x2E, (r1 & 0xF) + Word(Pointer(at::kSource) + 0x2E));
    const std::uint32_t r2 = RandCall();
    const unsigned char* const src = Pointer(at::kSource);
    SetWord(child + 0x30, Word(src + 0x30) - (r2 & 0x1F));
    SetWord(child + 0x32, Word(src + 0x32));
    unsigned char* const s = Sc();
    child[9] = s[9];
    const unsigned char count = s[9];
    s[9] = static_cast<unsigned char>(count - 1);
    if (count != 0) {
        s[0xA] = 8;
        return;
    }
    s[0xA] = 0x2D;
    Inc(s[1]);
}

// original 0x4ABDD0: +0xA down; once it was 0, the target flagged 0x40, the
// effect's done flag, the task freed.
S10_EXPORT void __cdecl Ovum_End(void) {
    unsigned char* const s = Sc();
    const unsigned char was = s[0xA];
    s[0xA] = static_cast<unsigned char>(was - 1);
    if (was != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(at::kFlags)[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4ABE00: the child's kind-1 task, a one-entry stack table by +1.
S10_EXPORT void __cdecl OvumChild_Task(void) {
    static constexpr std::uint32_t kPhases[1] = {addr::OvumChild_Run};
    Dispatch(kPhases, 1, Sc()[1], "OvumChild_Task");
}

// original 0x4ABE20: with the frame-offset table 0x9039D8 the blast's
// (0x8C5D80), a three-entry stack table by +2 (OvumChild_Start, group S31's
// MainCannonBlast_Play, BattleFx_FreeTask); the screen update while +0 is
// set; the battle's table back.
S10_EXPORT void __cdecl OvumChild_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::OvumChild_Start, addr::MainCannonBlast_Play, addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBlast));
    Dispatch(kSteps, 3, phase, "OvumChild_Run");
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x4ABE80: the sprite fields (+0x24..+0x2D); sound 0x102 for the
// last child (+9 0), else 0x101 or 0x100 by +9's parity; animation 5; +2 on.
S10_EXPORT void __cdecl OvumChild_Start(void) {
    Sc()[0x29] = 0;
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x24] = 0x84;
    Sc()[0x27] = 0xB4;
    Sc()[0x28] = 0;
    SetWord(Sc() + 0x2C, 0);
    Sc()[0x2B] = 0;
    Sc()[0x2A] = 0;
    const unsigned char n = Sc()[9];
    MH_CALL(Sound_PlayById)(static_cast<unsigned short>(n == 0 ? 0x102 : (n & 1) ? 0x101 : 0x100));
    MH_CALL(Sprite_SetAnimation)(5);
    Inc(Sc()[2]);
}

// ===========================================================================
// MAGIC053 (row 134, Lavaburst read one id down)

namespace {

// The pool walked: every record with bit 0 becomes Sprite_Current, its +0x80
// the owner, for LavaburstRecord_Task; both put back after each. The task and
// owner are read after the phase call that precedes the walk.
void WalkPool() {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < kPoolRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(at::kOwner), rec_owner);
        Call0(addr::LavaburstRecord_Task);
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = self;
    }
}

// The radius word 0x903850 x the sine or cosine of `angle`, sar 12.
short Scaled(int v) { return static_cast<short>(Mul12(v, SS(0))); }

}  // namespace

// original 0x4ABF20: the kind-2 task. A three-entry stack table by +1
// (Lavaburst_Start, MAGIC226/227's 0x4F9F70, BattleFx_Finish), then the pool
// walked.
S10_EXPORT void __cdecl Lavaburst_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Lavaburst_Start, addr::MagicFx_CountDownFlag10,
                                                 addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "Lavaburst_Task");
    WalkPool();
}

// original 0x4ABFA0: the pool's +0..+2 cleared; the task at the side's
// centre and on the screen; its direction the acting actor's sprite's,
// turned round (xor 2) when the target's side bit names the other side from
// the actor (0x40 with a party actor, none with an enemy);
// +0xB 0, +9 0x10, +1 on; eight children (kind 1, 0x5E), +4 / +9 from
// Lavaburst_ChildDelays; CLUT row 26 and the first 16 words of row 2 back
// with their STP bits, the first word of each without.
S10_EXPORT void __cdecl Lavaburst_Start(void) {
    for (unsigned i = 0; i < kPoolRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    const bool enemies = (TargetByte() & 0x40) != 0;
    const unsigned actor = ActorByte();
    unsigned char facing = Pointer(kActorRecord)[8];
    if (enemies ? actor < 3 : actor >= 3) facing = static_cast<unsigned char>(facing ^ 2);
    Sc()[8] = facing;
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned slot = NewTask(0x5E);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        const unsigned char delay = Mem(kChildDelays)[i];
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[1] = 0;
        child[4] = static_cast<unsigned char>(delay >> 4);
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>(delay + 1);
        Inc(self[0xB]);
    }
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    for (unsigned k = 0x200; k < 0x210; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = Gfx_ClutStripSource[0x1A00];
    Gfx_ClutStrip[0x200] = Gfx_ClutStripSource[0x200];
    Gfx_ClutStripDirty = 1;
}

// original 0x4AC0E0: the child's kind-1 task, a jmp through
// LavaburstChild_TaskTable (one entry) by +1, unchecked.
S10_EXPORT void __cdecl LavaburstChild_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::LavaburstChild_Run};
    Dispatch(kKinds, 1, Sc()[1], "LavaburstChild_Task");
}

// original 0x4AC100: with the frame-offset table the effects' (0x8E3580), a
// call through LavaburstChild_Steps (five) by +2; then while bit 0 of +0 is
// set and +2 is 1..3: the screen point and update, MAGIC219's 0x4F6020, the
// glow, MAGIC226/227's 0x4FA440, the matrix popped; the battle's table back.
S10_EXPORT void __cdecl LavaburstChild_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {addr::LavaburstChild_Launch, addr::LavaburstChild_Rise,
                                                addr::LavaburstChild_Shake, addr::LavaburstChild_Settle,
                                                addr::LavaburstChild_End};
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    Dispatch(kSteps, 5, Sc()[2], "LavaburstChild_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) != 0 && s[2] != 0 && s[2] < 4) {
        MH_CALL(BattleActor_UpdateScreenXY)();
        MH_CALL(Sprite_UpdateScreen)();
        Call0(addr::MagicFx_PushRecordMatrix);
        Call0(addr::LavaburstChild_DrawGlow);
        Call0(addr::MeteorStrikeRock_DrawRing);
        MH_CALL(Gte_PopMatrix)();
    }
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x4AC160: +9 down; at 0 the child starts: at an angle +0xB << 9
// round the owner, radius 0x18 or 0x28 (Rand & 0x10); the owner's direction;
// lifted by (0x20000, 0) turned plus a height 0x6000000 over the owner's;
// the step (-0x2000, 0) turned, the fall 0x600000; its sprite fields,
// animation 0; +0xB 0, +9 and +0xA 0x10, +2 on.
S10_EXPORT void __cdecl LavaburstChild_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetSW(4, static_cast<unsigned>(Sc()[0xB]) << 9);
    const std::uint32_t r = RandCall();
    const int angle = SS(4);
    SetSW(0, (r & 0x10) + 0x18);
    int v = Sin(angle);
    {
        const std::uint32_t x = Mul(v, SS(0)) + static_cast<std::uint32_t>(Long(Owner() + 0x34));
        SetLong(Sc() + 0x34, static_cast<std::int32_t>(x));
    }
    v = Cos(SS(4));
    {
        const std::uint32_t z = Mul(v, SS(0)) + static_cast<std::uint32_t>(Long(Owner() + 0x38));
        SetLong(Sc() + 0x38, static_cast<std::int32_t>(z));
    }
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0xC, 0x20000);
    SetLong(Sc() + 0x10, 0);
    SetLong(Sc() + 0x14, 0x6000000);
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x34, static_cast<std::uint32_t>(Long(s + 0xC)));
        AddLong(s + 0x38, static_cast<std::uint32_t>(Long(s + 0x10)));
        SetLong(s + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x3C)) +
                                                    static_cast<std::uint32_t>(Long(s + 0x14))));
        SetLong(s + 0xC, static_cast<std::int32_t>(0xFFFFE000u));
        SetLong(s + 0x10, 0);
        SetLong(s + 0x20, 0x600000);
    }
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        s[0x25] = 0x1D;
        s[0x26] = 0;
        s[0x27] = 0x1A;
        s[0x28] = 1;
        SetLong(s + 0x40, 0x10000);
        SetLong(s + 0x44, 0x10000);
        s[0x48] = 2;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        s[0x5C] = 0;
        s[0x2A] = 0;
        s[0x29] = 4;
        SetWord(s + 0x2C, 0);
        s[0x2B] = 1;
        s[0x24] = 0;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4AC360: the child rises: +0x34 / +0x38 on by the step, the
// height +0x14 down by the fall +0x20 and set over the owner's, +0xB up by 8,
// +9 down; at 0 a sound (0x101 or 0x100 by bit 0 of +4), eight pool records
// (Lavaburst_PoolAlloc; a full pool skips one) owned by this task and
// numbered, counted in +4; animation 1; the camera's first angle up by 0x14;
// +9 4, +2 on.
S10_EXPORT void __cdecl LavaburstChild_Rise(void) {
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x34, static_cast<std::uint32_t>(Long(s + 0xC)));
        AddLong(s + 0x38, static_cast<std::uint32_t>(Long(s + 0x10)));
        AddLong(s + 0x14, 0u - static_cast<std::uint32_t>(Long(s + 0x20)));
        SetLong(s + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x3C)) +
                                                    static_cast<std::uint32_t>(Long(s + 0x14))));
        AddB(s[0xB], 8);
        Dec(s[9]);
        if (s[9] != 0) return;
    }
    MH_CALL(Sound_PlayById)(static_cast<unsigned short>((Sc()[4] & 1) ? 0x101 : 0x100));
    Sc()[4] = 0;
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned index = MH_AT(ByteFn, addr::Lavaburst_PoolAlloc)() & 0xFFu;
        if (index == 0xFF) continue;
        unsigned char* const rec = PoolRecord(index);
        unsigned char* const self = Sc();
        SetLong(rec + 0x80, static_cast<std::int32_t>(Key(self)));
        rec[1] = 0;
        rec[0xB] = static_cast<unsigned char>(i);
        Inc(self[4]);
    }
    MH_CALL(Sprite_SetAnimation)(1);
    Camera_Angles[0] = static_cast<short>(Camera_Angles[0] + 0x14);
    Sc()[9] = 4;
    Inc(Sc()[2]);
}

// original 0x4AC470: the script ticked once (its answer unread); the camera's
// first angle up or down by 0x14 by bit 0 of +9, +9 down; at 0 the angle
// 0xFD56 and +2 on.
S10_EXPORT void __cdecl LavaburstChild_Shake(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    unsigned char* s = Sc();
    Camera_Angles[0] = static_cast<short>(Camera_Angles[0] + ((s[9] & 1) ? 0x14 : -0x14));
    Dec(s[9]);
    s = Sc();
    if (s[9] != 0) return;
    Camera_Angles[0] = static_cast<short>(0xFD56);
    Inc(s[2]);
}

// original 0x4AC4C0: +0xA down by 2 unless 0; the script ticked once, and at
// its end +2 on.
S10_EXPORT void __cdecl LavaburstChild_Settle(void) {
    unsigned char* const s = Sc();
    if (s[0xA] != 0) s[0xA] = static_cast<unsigned char>(s[0xA] - 2);
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFF) != 0) Inc(Sc()[2]);
}

// original 0x4AC4F0: once its records are gone (+4 0), the owner's count
// +0xB down and the task freed.
S10_EXPORT void __cdecl LavaburstChild_End(void) {
    if (Sc()[4] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AC510: a glow of 64 semi-transparent gouraud triangles round
// the origin, radius +0xB, all nine colour bytes +0xA x 4; tpage 0x55,
// layer 5.
S10_EXPORT void __cdecl LavaburstChild_DrawGlow(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xB]);
        SetSW(0xA, static_cast<unsigned>(s[0xA]) << 2);
    }
    int v = Sin(0);
    SetVW(0x10, static_cast<unsigned>(Scaled(v)));
    v = Cos(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Scaled(v)));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), y = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        v = Sin(a);
        SetVW(0x10, static_cast<unsigned>(Scaled(v)));
        v = Cos(a);
        SetVW(0x12, static_cast<unsigned>(Scaled(v)));
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = SB(0xA);
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
}

// original 0x4AC6B0: a pool record's task, a jmp through
// LavaburstRecord_TaskTable (one entry) by +1, unchecked.
S10_EXPORT void __cdecl LavaburstRecord_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::LavaburstRecord_Run};
    Dispatch(kKinds, 1, Sc()[1], "LavaburstRecord_Task");
}

// original 0x4AC6D0: a call through LavaburstRecord_Steps (three: MAGIC222's
// 0x4F7C40, _Grow, _Shrink) by +2; then while +0 and +2 are set the actor
// matrix, the quad, the matrix popped.
S10_EXPORT void __cdecl LavaburstRecord_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::CombustionMote_Start, addr::LavaburstRecord_Grow,
                                                addr::LavaburstRecord_Shrink};
    Dispatch(kSteps, 3, Sc()[2], "LavaburstRecord_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::LavaburstRecord_Draw);
    MH_CALL(Gte_PopMatrix)();
}

namespace {

// The record at the angle +0xB << 9 round the owner, at the radius +0xC (no
// shift): the angle word 0x903854 read again for the cosine.
void RecordOrbit() {
    const auto angle = static_cast<std::uint16_t>(static_cast<unsigned>(Sc()[0xB]) << 9);
    SetSW(4, angle);
    int v = Sin(static_cast<short>(angle));
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, static_cast<std::int32_t>(Mul(v, Long(s + 0xC)) + static_cast<std::uint32_t>(Long(Owner() + 0x34))));
    }
    v = Cos(SS(4));
    unsigned char* const s = Sc();
    SetLong(s + 0x38, static_cast<std::int32_t>(Mul(v, Long(s + 0xC)) + static_cast<std::uint32_t>(Long(Owner() + 0x38))));
}

}  // namespace

// original 0x4AC710: the radius +0xC up by 2, the record on its orbit; +9
// down, at 0 +2 on.
S10_EXPORT void __cdecl LavaburstRecord_Grow(void) {
    AddLong(Sc() + 0xC, 2);
    RecordOrbit();
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4AC7A0: the radius up by 1, the orbit; +0xA down, at 0 the
// owner's record count +4 down and MAGIC219's pool free (a tail jmp).
S10_EXPORT void __cdecl LavaburstRecord_Shrink(void) {
    AddLong(Sc() + 0xC, 1);
    RecordOrbit();
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[4]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4AC840: one semi-transparent textured quad of radius 0x100 at
// the angles 0x200, 0x600, 0xE00, 0xA00; page (0x340, 0x100) abr 1, clut
// (0, 0x1E2), the texture's 0x20 x 0x20 at (0, 0x80); the colour +0xA x 6;
// tpage 0x55, layer 5.
S10_EXPORT void __cdecl LavaburstRecord_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetSW(0, 0x100);
    SetSW(0xA, Sc()[0xA] * 6u);
    static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned c = 0; c < 4; ++c) {
        int v = Cos(kAngles[c]);
        SetVW(c * 8, static_cast<unsigned>(Scaled(v)));
        v = Sin(kAngles[c]);
        SetVW(c * 8 + 4, 0);
        SetVW(c * 8 + 2, static_cast<unsigned>(Scaled(v)));
    }
    SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1E2));
    p[0x15] = 0x80;
    p[0x25] = 0x80;
    p[0x14] = 0;
    p[0x24] = 0x20;
    p[0x34] = 0;
    p[0x35] = 0xA0;
    p[0x44] = 0x20;
    p[0x45] = 0xA0;
    p[4] = SB(0xA);
    p[5] = SB(0xA);
    p[6] = SB(0xA);
    Rtp4(p, 0x10);
    MH_CALL(Gte_PrimDepths4_10)(p);
    MH_CALL(Gfx_CommitPrim)(5, 0x48);
}

// original 0x4ACA50: the first of the pool's 32 records without bit 0 of +0
// gets it; its index in al, 0xFF when all are taken.
S10_EXPORT unsigned char __cdecl Lavaburst_PoolAlloc(void) {
    for (unsigned i = 0; i < kPoolRecords; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if ((rec[0] & 1) != 0) continue;
        rec[0] = static_cast<unsigned char>(rec[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// ===========================================================================
// MAGIC054 (row 56, Howling read one id down)

// original 0x4ACAB0: the kind-2 task. A four-entry stack table by +1:
// Howling_Start, group S23's Simoon_Wait, MagicFx_DoneAndFree, group S26's
// Magic114_End.
S10_EXPORT void __cdecl Howling_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::Howling_Start, addr::Simoon_Wait, addr::MagicFx_DoneAndFree,
                                                 addr::Magic114_End};
    Dispatch(kPhases, 4, Sc()[1], "Howling_Task");
}

namespace {

// A child (kind 1, 0x2C) holding a copy of the record's first 0x80 bytes,
// owned by this task, +1 / +2 0, +6 1, +5 0x2C, +0xB the record's number;
// counted in +0xB; the record's +0 | 0x40.
void CopyToChild(unsigned char* rec, unsigned number) {
    const unsigned slot = NewTask(0x2C);
    unsigned char* const child = TaskSlot(slot);
    CopyRecord(child, rec);
    unsigned char* const self = Sc();
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
    child[1] = 0;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x2C;
    child[0xB] = static_cast<unsigned char>(number);
    Inc(self[0xB]);
    rec[0] = static_cast<unsigned char>(rec[0] | 0x40);
}

}  // namespace

// original 0x4ACAF0: in an event battle of kind 0x37 only the target flags
// 0x10, +1 3 (Magic114_End) and +9 0x1E. Else +0xB 0, +1 on, and every live
// actor of the target's side (the enemies when its 0x40 bit is set, else the
// party) given an animation - an enemy 4 through the engine's 0x435A20, a
// member its +8 + 0x10 - and copied into a child; sound 0x100.
S10_EXPORT void __cdecl Howling_Start(void) {
    if (Mem(kEventBattle)[0] == 0x37) {
        MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
        Sc()[1] = 3;
        Sc()[9] = 0x1E;
        return;
    }
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    if ((TargetByte() & 0x40) != 0) {
        for (unsigned i = 0; i < 8; ++i) {
            if ((MH_CALL(Battle_ActorIsOut)(i + 3) & 0xFF) != 0) continue;
            MH_AT(AnimFn, kEnemyAnimCurrent)(i + 3, 4);
            CopyToChild(EnemyByOrder(i), i);
        }
    } else {
        for (unsigned i = 0; i < 3; ++i) {
            if ((MH_CALL(Battle_ActorIsOut)(i) & 0xFF) != 0) continue;
            unsigned char* const self = Sprite_Current;
            unsigned char* const rec = PartyRecord(i);
            Sprite_Current = rec;
            MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(rec[8] + 0x10));
            Sprite_Current = self;
            CopyToChild(rec, i);
        }
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4ACCE0: the child's kind-1 task, a jmp through
// HowlingChild_TaskTable (one entry) by +1, unchecked.
S10_EXPORT void __cdecl HowlingChild_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::HowlingChild_Run};
    Dispatch(kKinds, 1, Sc()[1], "HowlingChild_Task");
}

// original 0x4ACD00: a six-entry stack table by +2 (_Start, _Out, _Hold,
// _Back, _End, MAGIC058's 0x4AF490); the screen update while +0 is set.
S10_EXPORT void __cdecl HowlingChild_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {addr::HowlingChild_Start, addr::HowlingChild_Out,
                                                addr::HowlingChild_Hold,  addr::HowlingChild_Back,
                                                addr::HowlingChild_End,   addr::MagicFx_UncountAndFree};
    Dispatch(kSteps, 6, Sc()[2], "HowlingChild_Run");
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

namespace {

// The record of the child's number +0xB (unchecked): the enemy when the
// target byte has 0x40, else the party member.
unsigned char* CopiedRecord() {
    const unsigned i = Sc()[0xB];
    return (TargetByte() & 0x40) != 0 ? EnemyByOrder(i) : PartyRecord(i);
}

// The scale steps: +0xC / +0x10 on by +0x18 / +0x1C, the sprite's scale
// +0x40 / +0x44 on by them.
void ScaleStep() {
    unsigned char* const s = Sc();
    AddLong(s + 0xC, static_cast<std::uint32_t>(Long(s + 0x18)));
    AddLong(s + 0x10, static_cast<std::uint32_t>(Long(s + 0x1C)));
    AddLong(s + 0x40, static_cast<std::uint32_t>(Long(s + 0xC)));
    AddLong(s + 0x44, static_cast<std::uint32_t>(Long(s + 0x10)));
}

}  // namespace

// original 0x4ACD60: the child at its record's position; the scale 0x10000
// and its steps (-0xC60, 0x1080) growing by (0x60, -0x80); +0x48 2; +2 on.
S10_EXPORT void __cdecl HowlingChild_Start(void) {
    const unsigned char* const rec = CopiedRecord();
    unsigned char* const s = Sc();
    SetLong(s + 0x34, Long(rec + 0x34));
    SetLong(s + 0x38, Long(rec + 0x38));
    SetLong(s + 0x3C, Long(rec + 0x3C));
    SetLong(s + 0x40, 0x10000);
    SetLong(s + 0x44, 0x10000);
    s[0x48] = 2;
    SetLong(s + 0xC, static_cast<std::int32_t>(0xFFFFF3A0u));
    SetLong(s + 0x10, 0x1080);
    SetLong(s + 0x18, 0x60);
    SetLong(s + 0x1C, static_cast<std::int32_t>(0xFFFFFF80u));
    Inc(s[2]);
}

// original 0x4ACE70: the script ticked (its answer unread), a scale step;
// once +0xC is 0, +9 0x10 and +2 on.
S10_EXPORT void __cdecl HowlingChild_Out(void) {
    MH_CALL(Sprite_ScriptTick)();
    ScaleStep();
    unsigned char* const s = Sc();
    if (Long(s + 0xC) != 0) return;
    s[9] = 0x10;
    Inc(s[2]);
}

// original 0x4ACED0: the script ticked; +9 down; at 0 the steps (0x60,
// -0x80) again and +2 on.
S10_EXPORT void __cdecl HowlingChild_Hold(void) {
    MH_CALL(Sprite_ScriptTick)();
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    SetLong(s + 0x18, 0x60);
    SetLong(s + 0x1C, static_cast<std::int32_t>(0xFFFFFF80u));
    Inc(s[2]);
}

// original 0x4ACF10: the script ticked, a scale step; at +0xC 0xC00 +2 on.
S10_EXPORT void __cdecl HowlingChild_Back(void) {
    MH_CALL(Sprite_ScriptTick)();
    ScaleStep();
    unsigned char* const s = Sc();
    if (Long(s + 0xC) == 0xC00) Inc(s[2]);
}

// original 0x4ACF70: the script ticked once; at its end the record's 0x40
// bit cleared and +2 on.
S10_EXPORT void __cdecl HowlingChild_End(void) {
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFF) == 0) return;
    unsigned char* const rec = CopiedRecord();
    rec[0] = static_cast<unsigned char>(rec[0] & 0xBF);
    Inc(Sc()[2]);
}

// ===========================================================================
// MAGIC055 (row 65, Ebonfire read one id down)

// original 0x4ACFE0: FxDiscFan_Task's twin. A three-entry stack table by +1
// (Ebonfire_Start, FxDiscFan_Grow, FxDiscFan_Fade), then while the task
// lives its screen point 16 up, the disc, and the fan under the actor's
// matrix.
S10_EXPORT void __cdecl Ebonfire_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Ebonfire_Start, addr::FxDiscFan_Grow, addr::FxDiscFan_Fade};
    Dispatch(kPhases, 3, Sc()[1], "Ebonfire_Task");
    if (Sc()[0] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    {
        unsigned char* const s = Sc();
        SetWord(s + 0x30, Word(s + 0x30) - 0x10u);
    }
    MH_CALL(MagicFx_DrawDisc)();
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(MagicFx_DrawFan)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AD040: the source sprite's direction and position; six rings
// (kind 1, 0x34) owned by this task, +9 6 i + 1, counted in +0xB; words
// 1..15 of CLUT row 26 back with their STP bits, word 0 cleared; sound
// 0x100; the target flagged 0x10 (the target read after the sound); +9 and
// +0xA 1, +1 on.
S10_EXPORT void __cdecl Ebonfire_Start(void) {
    {
        const unsigned char* const src = Pointer(at::kSource);
        Sc()[8] = src[8];
        SetLong(Sc() + 0x34, Long(src + 0x34));
        SetLong(Sc() + 0x38, Long(src + 0x38));
        SetLong(Sc() + 0x3C, Long(src + 0x3C));
    }
    Sc()[0xB] = 0;
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned slot = NewTask(0x34);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[9] = static_cast<unsigned char>(i * 6 + 1);
        Inc(self[0xB]);
    }
    for (unsigned k = 0x1A01; k < 0x1A10; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = 0;
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Sc()[9] = 1;
    Sc()[0xA] = 1;
    Inc(Sc()[1]);
}

// original 0x4AD1C0: a ring task: a call through FxRing_PhasesTwin (four:
// FxRing_Wait, _Rise, _Fade, EbonfireRing_End) by +1; then while it lives
// and is past its wait the ring under the actor's matrix.
S10_EXPORT void __cdecl EbonfireRing_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::FxRing_Wait, addr::FxRing_Rise, addr::FxRing_Fade,
                                                 addr::EbonfireRing_End};
    Dispatch(kPhases, 4, Sc()[1], "EbonfireRing_Task");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[1] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::EbonfireRing_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AD300: the rings' fourth phase (after FxRing_Fade, which frees
// the task): +9 down, +0xB up by 2, +0xA up; at +0xA 0x10 the owner's +0xB
// 0xFF and the task freed.
S10_EXPORT void __cdecl EbonfireRing_End(void) {
    Dec(Sc()[9]);
    AddB(Sc()[0xB], 2);
    Inc(Sc()[0xA]);
    if (Sc()[0xA] != 0x10) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AD350: MagicFx_DrawRing with tpage 0x55 and abr 2 - a band of
// 32 textured gouraud quads round the actor, inner radius +9 x 4 lifted by
// sin(((+0xB + i) & 0xF) << 8) of +0xA x 4 + 0x40, outer +9 x 3 on the
// ground; page (0x340, 0x100), clut (0, 0x1FA), columns i x 4, rows
// (+0xB + 8) x 4 / (+0xB + 12) x 4, the rim shade 0x68 - +0xB x 3; each quad
// sorted at the actor's position moved by its inner x << 9 on both axes.
S10_EXPORT void __cdecl EbonfireRing_Draw(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0, static_cast<unsigned>(s[9]) << 2);
        SetSW(4, s[9] * 3u);
        SetSW(6, s[0xA] * 4u + 0x40);
        SetSW(8, static_cast<unsigned>(s[0xB] & 0xF) << 8);
    }
    int v = Cos(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Sin(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Sin(SS(8));
    {
        const std::uint16_t lift = SW(6);
        SetVW(0xC, static_cast<unsigned>(Mul12(v, static_cast<short>(lift))) - lift);
    }
    v = Cos(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
    v = Sin(0);
    SetVW(0x1C, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
    unsigned i = 1;
    for (int a = 0x80; a < 0x1080; a += 0x80, ++i) {
        {
            const unsigned char* const s = Sc();
            SetVW(0, VW(8));
            SetSW(8, static_cast<unsigned>((s[0xB] + static_cast<unsigned char>(i)) & 0xF) << 8);
            SetVW(2, VW(0xA));
            SetVW(4, VW(0xC));
        }
        v = Cos(a);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Sin(a);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Sin(SS(8));
        {
            const std::uint16_t lift = SW(6);
            SetVW(0xC, static_cast<unsigned>(Mul12(v, static_cast<short>(lift))) - lift);
        }
        SetVW(0x10, VW(0x18));
        SetVW(0x12, VW(0x1A));
        SetVW(0x14, 0);
        v = Cos(a);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
        v = Sin(a);
        const unsigned char* s = Sc();
        unsigned char* p = Gfx_PacketNext;
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
        const std::uint32_t lean = Shl(VS(8), 9);
        SetVW(0x1C, 0);
        const auto x = static_cast<unsigned long>(static_cast<std::uint32_t>(Long(s + 0x34)) + lean);
        const auto z = static_cast<unsigned long>(static_cast<std::uint32_t>(Long(s + 0x38)) + lean);
        MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x55, 0);
        MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0xC);
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetWord(p + 0x2A, MH_CALL(Gpu_GetTPage)(0, 2, 0x340, 0x100));
        SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
        s = Sc();
        const auto u0 = static_cast<unsigned char>(i << 2);
        const auto u1 = static_cast<unsigned char>((i + 1) << 2);
        p[0x14] = u0;
        p[0x15] = static_cast<unsigned char>((s[0xB] + 8) << 2);
        p[0x28] = u1;
        p[0x29] = static_cast<unsigned char>((s[0xB] + 8) << 2);
        p[0x3C] = u0;
        p[0x50] = u1;
        p[0x3D] = static_cast<unsigned char>((s[0xB] + 0xC) << 2);
        p[0x51] = static_cast<unsigned char>((s[0xB] + 0xC) << 2);
        SetSW(0xA, 0x68u - s[0xB] * 3u);
        for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) p[k] = 1;
        for (unsigned k : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = SB(0xA);
        Rtp4(p, 0x14);
        MH_CALL(Gte_PrimDepths4_14)(p);
        MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0x54);
    }
}

// ===========================================================================
// MAGIC056 (row 112, Sacrifice read one id down)

// original 0x4AD8A0: the kind-2 task. A three-entry stack table by +1:
// Sacrifice_Start, Sacrifice_Wait, MagicFx_DoneAndFree.
S10_EXPORT void __cdecl Sacrifice_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Sacrifice_Start, addr::Sacrifice_Wait, addr::MagicFx_DoneAndFree};
    Dispatch(kPhases, 3, Sc()[1], "Sacrifice_Task");
}

namespace {

// The acting actor's record by the byte 0x904B34 (read once): a party member
// at 2 or less, else the enemy by index - 3, unchecked.
unsigned char* ActingRecord(unsigned actor) { return actor < 3 ? PartyRecord(actor) : EnemyRecord(actor); }

}  // namespace

// original 0x4AD8D0: the task at the side's centre; +0xB and +9 0, +1 on; the
// actor's animation 8; a child (kind 1, 0x55, +1 3: the actor) holding a copy
// of the acting actor's record; CLUT row 26's words 1..15 and 17..31 back
// with their STP bits, words 0 and 16 cleared; the owner's CLUT copied to the
// effect row (its answer to +0x27), +0x28 / +0x24 the owner's, the STP bits
// of this task's CLUT; the owner's +0 | 0x40.
S10_EXPORT void __cdecl Sacrifice_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(8, 0);
    const unsigned slot = NewTask(0x55);
    unsigned char* const child = TaskSlot(slot);
    CopyRecord(child, ActingRecord(ActorByte()));
    unsigned char* const self = Sc();
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
    child[1] = 3;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x55;
    Inc(self[0xB]);
    for (unsigned k = 0x1A01; k < 0x1A10; ++k) {
        Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
        Gfx_ClutStrip[k + 0x10] = static_cast<unsigned short>(Gfx_ClutStripSource[k + 0x10] | 0x8000);
    }
    Gfx_ClutStrip[0x1A00] = 0;
    Gfx_ClutStrip[0x1A10] = 0;
    Gfx_ClutStripDirty = 1;
    const unsigned row = MH_CALL(SpriteClut_CopyToFxRow)(Owner());
    Sc()[0x27] = static_cast<unsigned char>(row);
    Sc()[0x28] = Owner()[0x28];
    Sc()[0x24] = Owner()[0x24];
    MH_CALL(SpriteClut_SetStp)(Sc());
    Owner()[0] = static_cast<unsigned char>(Owner()[0] | 0x40);
}

// original 0x4ADA30: once the children are gone (+0xB 0): the owner's 0x40
// bit cleared; the effect row back; the actor's animation 0x1C; the target
// flagged 0x40; the acting actor's record's state +1 6, +2 4, two words
// (+0x98 / +0x90 a member's, +0xA4 / +0x92 an enemy's) 0 and 0x4000, its tint
// released; +1 on.
S10_EXPORT void __cdecl Sacrifice_Wait(void) {
    if (Sc()[0xB] != 0) return;
    Owner()[0] = static_cast<unsigned char>(Owner()[0] & 0xBF);
    MH_CALL(SpriteClut_RestoreFxRow)();
    MH_CALL(BattleActor_SetAnimation)(0x1C, 0);
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    const unsigned actor = ActorByte();
    unsigned char* const rec = ActingRecord(actor);
    rec[1] = 6;
    rec[2] = 4;
    if (actor < 3) {
        SetWord(rec + 0x98, 0);
        SetWord(rec + 0x90, 0x4000);
    } else {
        SetWord(rec + 0xA4, 0);
        SetWord(rec + 0x92, 0x4000);
    }
    MH_CALL(Sprite_ReleaseTint)(rec);
    Inc(Sc()[1]);
}

// original 0x4ADAF0: the children's kind-1 task, a jmp through
// SacrificeChild_Kinds (four: the ring twice, the disc, the actor) by +1,
// unchecked.
S10_EXPORT void __cdecl SacrificeChild_Task(void) {
    static constexpr std::uint32_t kKinds[4] = {addr::SacrificeRing_Run, addr::SacrificeRing_Run, addr::SacrificeDisc_Run,
                                                addr::SacrificeActor_Run};
    Dispatch(kKinds, 4, Sc()[1], "SacrificeChild_Task");
}

// original 0x4ADB10: a call through SacrificeRing_Steps (three) by +2; then
// while +0 and +2 are set the actor matrix, the ring, the matrix popped.
S10_EXPORT void __cdecl SacrificeRing_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::SacrificeFx_TakeOwnerPos, addr::SacrificeRing_Grow,
                                                addr::SacrificeRing_Fade};
    Dispatch(kSteps, 3, Sc()[2], "SacrificeRing_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::SacrificeRing_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4ADB50: the owner's position; +9 and +0xA 0; +2 on.
S10_EXPORT void __cdecl SacrificeFx_TakeOwnerPos(void) {
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4ADBA0: at +9 = 4 the target flagged 0x10; the radius word
// 0x903850 +9 x 28; +9 up; at 0xC +2 on.
S10_EXPORT void __cdecl SacrificeRing_Grow(void) {
    unsigned char* s = Sc();
    if (s[9] == 4) {
        MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
        s = Sc();
    }
    SetSW(0, s[9] * 28u);
    Inc(s[9]);
    if (s[9] == 0xC) Inc(s[2]);
}

// original 0x4ADBF0: the radius +9 x 4 + 0x120; +0xA and +9 up; at +9 0x28
// the owner's count +0xB down and the task freed.
S10_EXPORT void __cdecl SacrificeRing_Fade(void) {
    unsigned char* const s = Sc();
    SetSW(0, s[9] * 4u + 0x120);
    Inc(s[0xA]);
    Inc(s[9]);
    if (s[9] != 0x28) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4ADC40: eight bands of 32 semi-transparent textured gouraud
// quads, band n (1..8) between the circles at the angles A - 0x80 and A (A
// from 0xC00 down by 0x80) of a sphere of radius the word 0x903850; the
// shades 0x81, fading by 8 a band once +9 has passed 0x10 + n; page (0x340,
// 0x100) abr 1, clut (0, 0x1FA), columns k x 8, rows (m + 0x14) x 8 /
// (m + 0x15) x 8 (m 0x18 down); each quad sorted at the task's position
// moved by its outer vertex << 9; tpage 0x35.
S10_EXPORT void __cdecl SacrificeRing_Draw(void) {
    int m = 0x18, n = 1, band = 0xC00;
    do {
        {
            const unsigned char* const s = Sc();
            SetSW(6, 0x81);
            SetSW(8, 0x81);
            const int b = s[9];
            if (n - 1 < b - 0x10) {
                const auto d = static_cast<short>(b - n - 0xF);
                if (d < 0x11) SetSW(6, 0x81u - Shl(d, 3));
                else SetSW(6, 1);
            }
            if (n < b - 0x10) {
                const auto d = static_cast<short>(b - n - 0x10);
                SetSW(8, static_cast<unsigned>(d));
                if (d < 0x11) SetSW(8, 0x81u - Shl(SS(8), 3));
                else SetSW(8, 1);
            }
        }
        const int inner = band - 0x80;
        int v = Cos(inner);
        SetSW(2, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(band);
        SetSW(4, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Sin(0);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(0);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Sin(inner);
        {
            const auto h = static_cast<unsigned>(Mul12(v, SS(0)));
            SetVW(4, h);
            SetVW(0xC, h);
        }
        v = Sin(0);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
        v = Cos(0);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
        v = Sin(band);
        {
            const auto h = static_cast<unsigned>(Mul12(v, SS(0)));
            SetVW(0x14, h);
            SetVW(0x1C, h);
        }
        const auto row0 = static_cast<unsigned char>((m + 0x14) << 3);
        const auto row1 = static_cast<unsigned char>((m + 0x15) << 3);
        unsigned k = 1;
        for (int a = 0x80; a < 0x1080; a += 0x80, ++k) {
            SetVW(0, VW(8));
            SetVW(2, VW(0xA));
            v = Sin(a);
            SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
            v = Cos(a);
            SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
            SetVW(0x10, VW(0x18));
            SetVW(0x12, VW(0x1A));
            v = Sin(a);
            SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
            v = Cos(a);
            const auto y = static_cast<short>(Mul12(v, SS(4)));
            const std::uint32_t dx = Shl(VS(0x18), 9);
            const unsigned char* const s = Sc();
            SetVW(0x1A, static_cast<unsigned>(y));
            const auto x = static_cast<unsigned long>(static_cast<std::uint32_t>(Long(s + 0x34)) + dx);
            const auto z = static_cast<unsigned long>(static_cast<std::uint32_t>(Long(s + 0x38)) + Shl(y, 9));
            MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
            MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyGT4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            Rtp4(p, 0x14);
            MH_CALL(Gte_PrimDepths4_14)(p);
            SetWord(p + 0x2A, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
            SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
            const auto u0 = static_cast<unsigned char>(k << 3);
            const auto u1 = static_cast<unsigned char>((k + 1) << 3);
            p[0x15] = row0;
            p[0x14] = u0;
            p[0x3C] = u0;
            p[0x28] = u1;
            p[0x29] = row0;
            p[0x3D] = row1;
            p[0x50] = u1;
            p[0x51] = row1;
            for (unsigned c : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) p[c] = SB(8);
            for (unsigned c : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[c] = SB(6);
            MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0x54);
        }
        --m;
        band -= 0x80;
        ++n;
    } while (band > 0x800);
}

// original 0x4AE060: a call through SacrificeDisc_Steps (four) by +2; then
// while +0 and +2 are set the actor matrix, the disc, the matrix popped.
S10_EXPORT void __cdecl SacrificeDisc_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {addr::SacrificeFx_TakeOwnerPos, addr::SacrificeDisc_Grow,
                                                addr::SacrificeDisc_Hold, addr::SacrificeDisc_Fade};
    Dispatch(kSteps, 4, Sc()[2], "SacrificeDisc_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::SacrificeDisc_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AE0A0: +0xA up by 2, +9 up; at 0xC +2 on.
S10_EXPORT void __cdecl SacrificeDisc_Grow(void) {
    AddB(Sc()[0xA], 2);
    Inc(Sc()[9]);
    if (Sc()[9] == 0xC) Inc(Sc()[2]);
}

// original 0x4AE0D0: +9 up; at 0x1E +2 on.
S10_EXPORT void __cdecl SacrificeDisc_Hold(void) {
    Inc(Sc()[9]);
    if (Sc()[9] == 0x1E) Inc(Sc()[2]);
}

// original 0x4AE0F0: +9 up unless 0x2E; +0xA down; at 0 the owner's count
// +0xB down and the task freed.
S10_EXPORT void __cdecl SacrificeDisc_Fade(void) {
    unsigned char* const s = Sc();
    if (s[9] != 0x2E) Inc(s[9]);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AE130: 16 semi-transparent gouraud triangles round the origin,
// radius Math_Cos(0x800) x (+9 + 0x84) x 4 past step 1, else x +9 x 48 (sar
// 12); the centre (+0xA x 6, +0xA x 8, +0xA x 8), the rim black; tpage 0x55,
// layer 5.
S10_EXPORT void __cdecl SacrificeDisc_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    if (Sc()[2] > 1) {
        const int v = Cos(0x800);
        const unsigned char* const s = Sc();
        SetSW(0, static_cast<unsigned>(static_cast<int>(Mul(v, s[9] + 0x84) << 2) >> 12));
    } else {
        const int v = Cos(0x800);
        const unsigned char* const s = Sc();
        SetSW(0, static_cast<unsigned>(static_cast<int>(Mul(v, s[9]) * 3u << 4) >> 12));
    }
    {
        const unsigned char* const s = Sc();
        SetSW(6, static_cast<unsigned>(s[0xA]) << 3);
        SetSW(8, s[0xA] * 6u);
    }
    int v = Sin(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), y = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        v = Sin(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        p[4] = SB(8);
        p[5] = SB(6);
        p[6] = SB(6);
        for (unsigned c : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[c] = 0;
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
}

// original 0x4AE320: the actor copy's steps, a nine-entry stack table by +2
// (_Start, _Play, _Darken, _Lighten, _Split, _Wait, _Return, _End,
// BattleFx_FreeTask); the screen update while +0 and +2 are set.
S10_EXPORT void __cdecl SacrificeActor_Run(void) {
    static constexpr std::uint32_t kSteps[9] = {
        addr::SacrificeActor_Start, addr::SacrificeActor_Play,  addr::SacrificeActor_Darken,
        addr::SacrificeActor_Lighten, addr::SacrificeActor_Split, addr::SacrificeActor_Wait,
        addr::SacrificeActor_Return, addr::SacrificeActor_End,  addr::BattleFx_FreeTask};
    Dispatch(kSteps, 9, Sc()[2], "SacrificeActor_Run");
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4AE3A0: +0x27 (the CLUT) the owner's; +2 on.
S10_EXPORT void __cdecl SacrificeActor_Start(void) {
    Sc()[0x27] = Owner()[0x27];
    Inc(Sc()[2]);
}

namespace {

// The copy tinted (+0 | 0x20, +0x5C 1, the tint +0x5D..+0x5F 0) and +2 on.
void TintOn(unsigned char* s) {
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    s[0x5C] = 1;
    s[0x5D] = 0;
    s[0x5E] = 0;
    s[0x5F] = 0;
    Inc(s[2]);
}
void TintBy(unsigned char* s, unsigned by) {
    AddB(s[0x5D], by);
    AddB(s[0x5E], by);
    AddB(s[0x5F], by);
}
// Animation `base` + the direction on the acting member's record (made
// Sprite_Current for the call) and then on this task's.
void MemberAndCopyAnimation(unsigned actor, unsigned base) {
    unsigned char* const self = Sprite_Current;
    unsigned char* const rec = PartyRecord(actor);
    Sprite_Current = rec;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(rec[8] + base));
    Sprite_Current = self;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(self[8] + base));
}

}  // namespace

// original 0x4AE3C0: the script ticked once; at its end the tint on and +2
// on.
S10_EXPORT void __cdecl SacrificeActor_Play(void) {
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFF) == 0) return;
    TintOn(Sc());
}

// original 0x4AE410: the tint down by 0x10; at +0x5D 0x80 the owner's
// position and +2 on.
S10_EXPORT void __cdecl SacrificeActor_Darken(void) {
    unsigned char* const s = Sc();
    TintBy(s, 0xF0);
    if (s[0x5D] != 0x80) return;
    SetLong(s + 0x34, Long(Owner() + 0x34));
    SetLong(s + 0x38, Long(Owner() + 0x38));
    SetLong(s + 0x3C, Long(Owner() + 0x3C));
    Inc(s[2]);
}

// original 0x4AE480: the tint up by 0x10; at +0x5D 0 the tint off, +9 8, +2
// on.
S10_EXPORT void __cdecl SacrificeActor_Lighten(void) {
    unsigned char* const s = Sc();
    TintBy(s, 0x10);
    if (s[0x5D] != 0) return;
    s[0] = static_cast<unsigned char>(s[0] & 0xDF);
    s[0x5C] = 0;
    s[9] = 8;
    Inc(s[2]);
}

// original 0x4AE4E0: +9 down; at 0 a ring (+1 0) and the disc (+1 2),
// children (kind 1, 0x55) of the owner (0x93B940, not this task) and counted
// in its +0xB; sound 0x100; a member actor's cue 0 (with Field_State its
// record) and animation 0x10 + the direction on the member and on this copy,
// an enemy's animation 4 through the engine's 0x435A70; +9 0x1E, +2 on.
S10_EXPORT void __cdecl SacrificeActor_Split(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    for (unsigned char kind : {0, 2}) {
        const unsigned slot = NewTask(0x55);
        unsigned char* const child = TaskSlot(slot);
        unsigned char* const owner = Owner();
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(owner)));
        child[1] = kind;
        Inc(owner[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
    std::uint32_t actor = static_cast<std::uint32_t>(Long(Mem(at::kActor)));
    if ((actor & 0xFF) < 3) {
        unsigned char* const kept = Field_State;
        Field_State = PartyRecord(actor & 0xFF);
        MH_CALL(Battle_PlayActorCue)(0);
        actor = static_cast<std::uint32_t>(Long(Mem(at::kActor)));
        Field_State = kept;
        if ((actor & 0xFF) < 3) {
            MemberAndCopyAnimation(actor & 0xFF, 0x10);
            Sc()[9] = 0x1E;
            Inc(Sc()[2]);
            return;
        }
    }
    MH_AT(AnimFn, kEnemyAnim)(actor, 4);
    Sc()[9] = 0x1E;
    Inc(Sc()[2]);
}

// original 0x4AE620: the script ticked (its answer unread); +9 down; at 0
// the tint on and +2 on.
S10_EXPORT void __cdecl SacrificeActor_Wait(void) {
    MH_CALL(Sprite_ScriptTick)();
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] == 0) TintOn(s);
}

// original 0x4AE680: the script ticked; the tint down by 0x10; at +0x5D
// 0x80 the position of the acting actor's sprite (0x904B3C), a member's
// animation 0x1C + the direction on it and on this copy, an enemy's 4
// through 0x435A70; +2 on.
S10_EXPORT void __cdecl SacrificeActor_Return(void) {
    MH_CALL(Sprite_ScriptTick)();
    {
        unsigned char* const s = Sc();
        TintBy(s, 0xF0);
        if (s[0x5D] != 0x80) return;
        const unsigned char* const actor_sprite = Pointer(kActorRecord);
        SetLong(s + 0x34, Long(actor_sprite + 0x34));
        SetLong(s + 0x38, Long(Pointer(kActorRecord) + 0x38));
        SetLong(s + 0x3C, Long(Pointer(kActorRecord) + 0x3C));
    }
    const std::uint32_t actor = static_cast<std::uint32_t>(Long(Mem(at::kActor)));
    if ((actor & 0xFF) < 3) MemberAndCopyAnimation(actor & 0xFF, 0x1C);
    else MH_AT(AnimFn, kEnemyAnim)(actor, 4);
    Inc(Sc()[2]);
}

// original 0x4AE760: while the owner's count +0xB is 1 the tint up by 0x10;
// at +0x5D 0 the owner's count down and +2 on.
S10_EXPORT void __cdecl SacrificeActor_End(void) {
    if (Owner()[0xB] != 1) return;
    unsigned char* const s = Sc();
    TintBy(s, 0x10);
    if (Sc()[0x5D] != 0) return;
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

void MagicS10_Inject() {
    if (bof3::WantsShadow("magic_s10")) magic_s10::SelfTest();
    BOF3_INJECT(Ovum_Task);
    BOF3_INJECT(Ovum_Start);
    BOF3_INJECT(Ovum_Spawn);
    BOF3_INJECT(Ovum_End);
    BOF3_INJECT(OvumChild_Task);
    BOF3_INJECT(OvumChild_Run);
    BOF3_INJECT(OvumChild_Start);
    BOF3_INJECT(Lavaburst_Task);
    BOF3_INJECT(Lavaburst_Start);
    BOF3_INJECT(LavaburstChild_Task);
    BOF3_INJECT(LavaburstChild_Run);
    BOF3_INJECT(LavaburstChild_Launch);
    BOF3_INJECT(LavaburstChild_Rise);
    BOF3_INJECT(LavaburstChild_Shake);
    BOF3_INJECT(LavaburstChild_Settle);
    BOF3_INJECT(LavaburstChild_End);
    BOF3_INJECT(LavaburstChild_DrawGlow);
    BOF3_INJECT(LavaburstRecord_Task);
    BOF3_INJECT(LavaburstRecord_Run);
    BOF3_INJECT(LavaburstRecord_Grow);
    BOF3_INJECT(LavaburstRecord_Shrink);
    BOF3_INJECT(LavaburstRecord_Draw);
    BOF3_INJECT(Lavaburst_PoolAlloc);
    BOF3_INJECT(Howling_Task);
    BOF3_INJECT(Howling_Start);
    BOF3_INJECT(HowlingChild_Task);
    BOF3_INJECT(HowlingChild_Run);
    BOF3_INJECT(HowlingChild_Start);
    BOF3_INJECT(HowlingChild_Out);
    BOF3_INJECT(HowlingChild_Hold);
    BOF3_INJECT(HowlingChild_Back);
    BOF3_INJECT(HowlingChild_End);
    BOF3_INJECT(Ebonfire_Task);
    BOF3_INJECT(Ebonfire_Start);
    BOF3_INJECT(EbonfireRing_Task);
    BOF3_INJECT(EbonfireRing_End);
    BOF3_INJECT(EbonfireRing_Draw);
    BOF3_INJECT(Sacrifice_Task);
    BOF3_INJECT(Sacrifice_Start);
    BOF3_INJECT(Sacrifice_Wait);
    BOF3_INJECT(SacrificeChild_Task);
    BOF3_INJECT(SacrificeRing_Run);
    BOF3_INJECT(SacrificeFx_TakeOwnerPos);
    BOF3_INJECT(SacrificeRing_Grow);
    BOF3_INJECT(SacrificeRing_Fade);
    BOF3_INJECT(SacrificeRing_Draw);
    BOF3_INJECT(SacrificeDisc_Run);
    BOF3_INJECT(SacrificeDisc_Grow);
    BOF3_INJECT(SacrificeDisc_Hold);
    BOF3_INJECT(SacrificeDisc_Fade);
    BOF3_INJECT(SacrificeDisc_Draw);
    BOF3_INJECT(SacrificeActor_Run);
    BOF3_INJECT(SacrificeActor_Start);
    BOF3_INJECT(SacrificeActor_Play);
    BOF3_INJECT(SacrificeActor_Darken);
    BOF3_INJECT(SacrificeActor_Lighten);
    BOF3_INJECT(SacrificeActor_Split);
    BOF3_INJECT(SacrificeActor_Wait);
    BOF3_INJECT(SacrificeActor_Return);
    BOF3_INJECT(SacrificeActor_End);
}
