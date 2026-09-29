// Three spell overlays compiled into the exe, round nine group S38
// (docs/magic_s38.md): the PSX's MAGIC223, MAGIC225 and MAGIC226/227.EMI,
// Magic_Rows rows 137, 140, 135 and 147. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Tempest / Hurricane
// (MAGIC223 serves both ids), an id with no label (MAGIC225) and MeteorStrike
// (MAGIC226; MAGIC227 is row 147, ability 227, which runs the same code); the
// names below use those labels as hypotheses and say what the code does.
//
//   - MAGIC223 0x4F8640..0x4F8F36: a full-screen flash (kind 1, 0x63) and a
//     pool of 64 gusts of its own (0x6B7BE0), 48 of them started round the
//     owner with delays; each drifts off at an angle by the owner's direction
//     and draws one textured quad, its CLUT row by the ability (0xDF or not);
//   - MAGIC225 0x4F8F40..0x4F9DD6: a veil (kind 1, 0x65) that moves the
//     target side's actors to a draw layer, shades the screen and draws four
//     coloured bursts at the screen's corners; 16 shards from a pool of 32
//     (0x6B9CE0) fly from the actor toward the task; then three stats of
//     MagicFx_BuffStats applied to every live actor of the side, each with a
//     popup (kind 1, 2);
//   - MAGIC226/227 0x4F9DE0..0x4FAFE6: a rock (kind 1, 0x5F) that falls from
//     high over the target side leaving a trail, lands with 32 chips, shakes
//     the camera and fades; trail and chips share a pool of 48 (0x6BAD60).
//     Its 0x4F9F70 (+9 down, then target flags 0x10) is a phase ten overlays'
//     tables hold, and 0x4FA440 (a ground ring) MAGIC053's too.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent). The .data value
// tables (the gust angles, the burst colours, the chips' shades and lifts)
// are read in place by any index, as the originals read them.
#include "game/magic_s38.h"

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

// The scratch the overlays keep their working values in: DamageScratch
// (0x903850.., Scratch_Swap at +0xC; words or dwords by function, MAGIC225's
// burst colours at +0x10..+0x1B) and the four SVECTORs of Prim_VertexScratch
// (0x9037A0..0x9037BF). Both are read again after every call, as the
// originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kActorRecord = 0x904B3C;   // unsigned char *: the acting actor's sprite record
constexpr std::uint32_t kEventBattle = 0x904AAA;   // u8: not 0 in an event battle (its kind)
constexpr std::uint32_t kAbility = 0x904B80;       // u16: the acting ability (battle_actions.cpp)
constexpr std::uint32_t kFrameSet = 0x9039D8;      // the sprite frame-offset table pointer (sprite_pose.h)
constexpr std::uint32_t kFrameSetBattle = 0x8B3580;
constexpr std::uint32_t kFrameSetEffect = 0x8E3580;
constexpr std::uint32_t kFrameCounter = 0x937F94;  // Frame_Counter, read as a byte

// The three overlays' pools of task-like records (0x84 bytes, bit 0 of +0 in
// use, +0x80 the owner), each walked by its kind-2 task.
constexpr std::uint32_t kGustPool = 0x6B7BE0;       // Tempest_GustPool, 64
constexpr unsigned kGusts = 64;
constexpr std::uint32_t kShardPool = 0x6B9CE0;      // Magic225_ShardPool, 32
constexpr unsigned kShards = 32;
constexpr std::uint32_t kRecordPool = 0x6BAD60;     // MeteorStrike_RecordPool, 48
constexpr unsigned kRecords = 48;

constexpr std::int32_t kRight = 0x439F8000;    // 319.0f
constexpr std::int32_t kBottom = 0x436F0000;   // 239.0f

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned char ActorByte() { return Mem(at::kActor)[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(kS + k)); }
void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(kS + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void AddLong(unsigned char* p, std::uint32_t v) {
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) + v));
}
void AddWord(unsigned char* p, std::uint32_t v) { SetWord(p, (Word(p) + v) & 0xFFFF); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `imul` alone: the 32-bit product, wrapped.
std::uint32_t Mul(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }
// `cdq / and edx, 2^n - 1 / add / sar n`: a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}
// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
unsigned char* PoolRecord(std::uint32_t pool, unsigned index) { return Mem(pool + index * 0x84u); }
// The side's records as MAGIC225 walks them: the party member i, the enemy i
// (from 0).
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyByOrder(unsigned i) { return Mem(at::kEnemies + i * at::kEnemyStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions, and other units' (bof3::addr), called by address,
// as the originals call them: in the game the jmp Inject put there (or
// Capcom's code), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
using TaskFn = void (__cdecl*)(unsigned char*);
using BurstFn = void (__cdecl*)(int, int, int, int);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
unsigned Alloc(std::uint32_t address) { return MH_AT(ByteFn, address)() & 0xFFu; }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = bof3::addr::Battle_TurnVectorC;
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }

// Other units' functions (docs/magic_s38.md section 3) are ours now and
// called by name: S37's MagicFx_FreeCurrentRecord (a tail jmp) and
// MagicFx_PushRecordMatrix, S35's MagicFx_WaitOwnerChildren.

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// A pool walked: every record with bit 0 becomes Sprite_Current, its +0x80
// the owner, for the record's task; both put back after each. The task and
// owner are read after the phase call that precedes the walk.
void WalkPool(std::uint32_t pool, unsigned records, std::uint32_t task) {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = PoolRecord(pool, i);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(at::kOwner), rec_owner);
        Call0(task);
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = self;
    }
}
// The first `records` of a pool without bit 0 of +0 get it; the index, 0xFF
// when all are taken.
unsigned char PoolAlloc(std::uint32_t pool, unsigned records) {
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = PoolRecord(pool, i);
        if ((rec[0] & 1) != 0) continue;
        rec[0] = static_cast<unsigned char>(rec[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}
void ClearPool(std::uint32_t pool, unsigned records) {
    for (unsigned i = 0; i < records; ++i) {
        unsigned char* const rec = PoolRecord(pool, i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
}

// A child task (kind 1): +0x80 this task, +1 0, counted in this task's +0xB.
// This task is read after BattleTask_Create; the slot is written unchecked
// (0xFF, none free, lands past the image, as in the original).
void AdoptChild(unsigned slot) {
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
    child[1] = 0;
    Inc(self[0xB]);
}

// The four projected points of Prim_VertexScratch at +8, +8 + step, ... of a
// primitive; Gte_RotTransPers4 gets the depth and flag pointers the originals
// pass.
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S38_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S38_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}
// A draw-mode packet (tpage `tpage`, dithered) committed to `slot`.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

}  // namespace

#define S38_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC223 (row 137, Tempest and Hurricane read one id down)

namespace {
constexpr std::uint32_t kGustAngles = 0x65C308;   // TempestGust_Angles: four dwords by direction
}  // namespace

// original 0x4F8640: the kind-2 task. A three-entry stack table by +1
// (Tempest_Start, MagicFx_CountDownFlag10, Tempest_End), then the gust pool
// walked.
S38_EXPORT void __cdecl Tempest_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Tempest_Start, addr::MagicFx_CountDownFlag10, addr::Tempest_End};
    Dispatch(kPhases, 3, Sc()[1], "Tempest_Task");
    WalkPool(kGustPool, kGusts, addr::TempestGust_Task);
}

// original 0x4F86C0: the gust pool's +0..+2 cleared; the owner's direction and
// position; +9 8, +0xB 0, +1 on; the flash (kind 1, 0x63); 48 gusts owned by
// this task, numbered, each with a delay +9 of (Rand & 0xF) + 16 x (i / 8) + 1
// (a full pool skips one); CLUT row 26's words 1..15 and 17..31 back with
// their STP bits, words 0 and 16 cleared; sound 0x100.
S38_EXPORT void __cdecl Tempest_Start(void) {
    ClearPool(kGustPool, kGusts);
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[9] = 8;
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    AdoptChild(NewTask(0x63));
    for (unsigned i = 0; i < 0x30; ++i) {
        const unsigned index = Alloc(addr::Tempest_GustAlloc);
        if (index == 0xFF) continue;
        unsigned char* const self = Sc();
        unsigned char* const rec = PoolRecord(kGustPool, index);
        SetLong(rec + 0x80, static_cast<std::int32_t>(Key(self)));
        rec[1] = 0;
        rec[0xB] = static_cast<unsigned char>(i);
        const std::uint32_t r = RandCall();
        rec[9] = static_cast<unsigned char>((r & 0xF) + ((i >> 3) << 4) + 1);
        Inc(Sc()[0xB]);
    }
    for (unsigned k = 1; k < 0x10; ++k) {
        Gfx_ClutStrip[0x1A00 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x1A00 + k] | 0x8000);
        Gfx_ClutStrip[0x1A10 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x1A10 + k] | 0x8000);
    }
    Gfx_ClutStrip[0x1A00] = 0;
    Gfx_ClutStrip[0x1A10] = 0;
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4F8830: once every child has ended (+0xB 0): the effect flags'
// word |= 0x2004 (the done bit and 0x2000), the target flagged 0x40, the task
// freed (a tail jmp).
S38_EXPORT void __cdecl Tempest_End(void) {
    if (Sc()[0xB] != 0) return;
    const unsigned char target = TargetByte();
    SetWord(Mem(at::kFlags), Word(Mem(at::kFlags)) | 0x2004u);
    MH_CALL(Battle_SetTargetFlag40)(target);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F8860: the flash's kind-1 task, a jmp through
// TempestFlash_TaskTable (one entry) by +1, unchecked.
S38_EXPORT void __cdecl TempestFlash_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::TempestFlash_Run};
    Dispatch(kKinds, 1, Sc()[1], "TempestFlash_Task");
}

// original 0x4F8880: a call through TempestFlash_Steps (four entries:
// MAGIC137's CoronaFlash_Start, MAGIC086's BarrierRing_Grow, MAGIC167's
// 0x4EF840, MAGIC060's MagicFx_CountDownRelease) by +2; then while +0 is set,
// the flash (a tail jmp).
S38_EXPORT void __cdecl TempestFlash_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {addr::CoronaFlash_Start, addr::BarrierRing_Grow,
                                                addr::MagicFx_WaitOwnerChildren, addr::MagicFx_CountDownRelease};
    Dispatch(kSteps, 4, Sc()[2], "TempestFlash_Run");
    if (Sc()[0] == 0) return;
    Call0(addr::TempestFlash_Draw);
}

// original 0x4F88B0: the flash - one semi-transparent gouraud quad over the
// whole 320 x 240 screen, its bright edge on the side the owner's direction
// +8 faces (bit 0): (+9 x 8) grey there, +9 grey at the other; +9 and +9 x 8
// kept in 0x903858 / Scratch_Swap; between two draw-mode packets (0x35,
// 0x15) on layer 2.
S38_EXPORT void __cdecl TempestFlash_Draw(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    if (Owner()[8] & 1) {
        SetLong(p + 8, 0);
        SetLong(p + 0xC, 0);
        SetLong(p + 0x18, kRight);
        SetLong(p + 0x1C, 0);
        SetLong(p + 0x28, 0);
        SetLong(p + 0x2C, kBottom);
        SetLong(p + 0x38, kRight);
    } else {
        SetLong(p + 8, kRight);
        SetLong(p + 0xC, 0);
        SetLong(p + 0x18, 0);
        SetLong(p + 0x1C, 0);
        SetLong(p + 0x28, kRight);
        SetLong(p + 0x2C, kBottom);
        SetLong(p + 0x38, 0);
    }
    SetLong(p + 0x3C, kBottom);
    {
        const unsigned char n = Sc()[9];
        SetSD(8, n);
        SetSD(0xC, static_cast<std::uint32_t>(n) << 3);
        p[0x34] = static_cast<unsigned char>(n << 3);
    }
    p[4] = SB(0xC);
    p[0x35] = SB(0xC);
    p[5] = SB(0xC);
    p[0x36] = SB(0xC);
    p[6] = SB(0xC);
    p[0x24] = SB(8);
    p[0x14] = SB(8);
    p[0x25] = SB(8);
    p[0x15] = SB(8);
    p[0x26] = SB(8);
    p[0x16] = SB(8);
    MH_CALL(Gfx_CommitPrim)(2, 0x44);
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
}

// original 0x4F89E0: a gust record's task, a jmp through TempestGust_TaskTable
// (one entry) by +1, unchecked.
S38_EXPORT void __cdecl TempestGust_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::TempestGust_Run};
    Dispatch(kKinds, 1, Sc()[1], "TempestGust_Task");
}

// original 0x4F8A00: a call through TempestGust_Steps (three entries) by +2;
// then while +0 and +2 are set, the gust (a tail jmp).
S38_EXPORT void __cdecl TempestGust_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::TempestGust_Launch, addr::TempestGust_Drift, addr::TempestGust_Fly};
    Dispatch(kSteps, 3, Sc()[2], "TempestGust_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(addr::TempestGust_Draw);
}

// original 0x4F8A30: +9 down; at 0 the gust starts: the owner's direction and
// position; its screen point; moved 72 x the sine / cosine (>> 12) of
// ((+0xB & 7) << 8) + TempestGust_Angles[direction] + 0x400 (kept in
// 0x903854, read back for the cosine); its heading +0x14 that table's entry;
// +0xB 0 (CLUT row 0) for ability 0xDF, else 0x10; +9 0, +0xA 0, +2 on.
S38_EXPORT void __cdecl TempestGust_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    MH_CALL(BattleActor_UpdateScreenXY)();
    {
        unsigned char* const s = Sc();
        unsigned char* const x = s + 0x2E;
        const std::uint32_t phase = (s[0xB] & 7u) << 8;
        SetSD(4, phase);
        const std::uint32_t angle = phase + static_cast<std::uint32_t>(Long(Mem(kGustAngles + 4u * s[8]))) + 0x400;
        SetSD(4, angle);
        const int v = Sin(static_cast<int>(angle));
        AddWord(x, static_cast<std::uint32_t>(Mul12(v, 72)));
    }
    {
        unsigned char* const y = Sc() + 0x30;
        const int v = Cos(SD(4));
        AddWord(y, static_cast<std::uint32_t>(Mul12(v, 72)));
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x14, Long(Mem(kGustAngles + 4u * s[8])));
    }
    Sc()[0xB] = Word(Mem(kAbility)) == 0xDF ? 0 : 0x10;
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4F8B50: the screen point moved 8 x the sine / cosine (>> 12) of
// the heading +0x14; +9 up, +0xA up by 4; at 0x10 +2 on.
S38_EXPORT void __cdecl TempestGust_Drift(void) {
    {
        unsigned char* const s = Sc();
        unsigned char* const x = s + 0x2E;
        const int v = Sin(Long(s + 0x14));
        AddWord(x, static_cast<std::uint32_t>(Mul12(v, 8)));
    }
    {
        unsigned char* const s = Sc();
        unsigned char* const y = s + 0x30;
        const int v = Cos(Long(s + 0x14));
        AddWord(y, static_cast<std::uint32_t>(Mul12(v, 8)));
    }
    Inc(Sc()[9]);
    AddB(Sc()[0xA], 4);
    if (Sc()[0xA] == 0x10) Inc(Sc()[2]);
}

// original 0x4F8BC0: the screen point moved 24 x the sine / cosine (>> 12) of
// the heading; +9 up; at 0x14 the owner's count +0xB down and MAGIC219's
// record free (a tail jmp).
S38_EXPORT void __cdecl TempestGust_Fly(void) {
    {
        unsigned char* const s = Sc();
        unsigned char* const x = s + 0x2E;
        const int v = Sin(Long(s + 0x14));
        AddWord(x, static_cast<std::uint32_t>(Mul12(v, 24)));
    }
    {
        unsigned char* const s = Sc();
        unsigned char* const y = s + 0x30;
        const int v = Cos(Long(s + 0x14));
        AddWord(y, static_cast<std::uint32_t>(Mul12(v, 24)));
    }
    Inc(Sc()[9]);
    if (Sc()[9] != 0x14) return;
    Dec(Owner()[0xB]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4F8C30: one semi-transparent textured quad (tpage 0x55, layer 3):
// its corners at the heading + 0xF78, + 0x888, + 0x88, + 0x773, radius 0x4A
// (the dword 0x903850, read back after every call; the angle in 0x903854)
// round the screen point; page (0x340, 0x100) abr 1, CLUT (+0xB, 0x1FA); the
// texture's column 8..0x98, its row (+9 & 3) x 0x20 + 8 .. + 0x28; the shade
// +0xA x 8 (0x903858).
S38_EXPORT void __cdecl TempestGust_Draw(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    {
        const unsigned char* const s = Sc();
        SetSD(0, 0x4A);
        SetSD(8, static_cast<std::uint32_t>(s[0xA]) << 3);
    }
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    static constexpr std::uint32_t kCorners[4] = {0xF78, 0x888, 0x88, 0x773};
    for (unsigned c = 0; c < 4; ++c) {
        const std::uint32_t angle = static_cast<std::uint32_t>(Long(Sc() + 0x14)) + kCorners[c];
        SetSD(4, angle);
        int v = Sin(static_cast<int>(angle));
        PutFloat(p + 8 + 0x10 * c, Mul12(v, SD(0)) + S16(Sc() + 0x2E));
        v = Cos(SD(4));
        PutFloat(p + 0xC + 0x10 * c, Mul12(v, SD(0)) + S16(Sc() + 0x30));
    }
    SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(Sc()[0xB], 0x1FA));
    p[0x14] = 8;
    {
        const unsigned char* const s = Sc();
        const auto row = static_cast<unsigned char>((s[9] & 3) << 5);
        p[0x15] = static_cast<unsigned char>(row + 8);
        p[0x24] = 0x98;
        p[0x34] = 8;
        p[0x25] = static_cast<unsigned char>(row + 8);
        p[0x44] = 0x98;
        p[0x35] = static_cast<unsigned char>(row + 0x28);
        p[0x45] = static_cast<unsigned char>(row + 0x28);
    }
    p[4] = SB(8);
    p[5] = SB(8);
    p[6] = SB(8);
    MH_CALL(Gfx_CommitPrim)(3, 0x48);
}

// original 0x4F8EE0: the first of the gust pool's 64 records without bit 0
// of +0 gets it; its index in al, 0xFF when all are taken.
S38_EXPORT unsigned char __cdecl Tempest_GustAlloc(void) { return PoolAlloc(kGustPool, kGusts); }

// ===========================================================================
// MAGIC225 (row 140, an id with no label read one id down)

namespace {

constexpr std::uint32_t kBurstColours = 0x65C31C;   // Magic225Veil_BurstColours: four RGB byte triples

// The side's live actors (Battle_ActorIsOut 0) given draw layer `layer`
// (+0x29): the enemies 0..7 when the target's side bit is set, else the party
// 0..2.
void LayerSide(unsigned char layer) {
    if (TargetByte() & 0x40) {
        for (unsigned i = 0; i < 8; ++i)
            if (MH_CALL(Battle_ActorIsOut)(i + 3) == 0) EnemyByOrder(i)[0x29] = layer;
    } else {
        for (unsigned i = 0; i < 3; ++i)
            if (MH_CALL(Battle_ActorIsOut)(i) == 0) PartyRecord(i)[0x29] = layer;
    }
}

// Magic225_Apply's three stats on one actor: MagicFx_BuffStats[j] applied, a
// popup (kind 1, 2) owned by this task with +4 j + 4 (8 when refused), +0xB
// the actor, +9 the delay 1 / 6 / 11, +0xA 12 j, counted in +0xB; none free
// skips the popup and the count.
void BuffActor(unsigned actor) {
    unsigned j = 0, shade = 0;
    for (unsigned delay = 1; delay < 0x10; delay += 5, ++j, shade += 12) {
        const unsigned char stat = MagicFx_BuffStats[j];
        const unsigned char took = MH_CALL(MagicFx_ApplyBuff)(stat, actor);
        const unsigned slot = MH_CALL(BattleTask_Create)(1, 2) & 0xFFu;
        if (slot == 0xFF) continue;
        unsigned char* const self = Sc();
        unsigned char* const popup = TaskSlot(slot);
        SetLong(popup + 0x80, static_cast<std::int32_t>(Key(self)));
        popup[4] = static_cast<unsigned char>(took != 0 ? j + 4 : 8);
        popup[0xB] = static_cast<unsigned char>(actor);
        popup[9] = static_cast<unsigned char>(delay);
        popup[0xA] = static_cast<unsigned char>(shade);
        Inc(self[0xB]);
    }
}

}  // namespace

// original 0x4F8F40: the kind-2 task. A four-entry stack table by +1
// (Magic225_Start, _Spawn, _Apply, BattleFx_Finish), then the shard pool
// walked.
S38_EXPORT void __cdecl Magic225_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::Magic225_Start, addr::Magic225_Spawn, addr::Magic225_Apply,
                                                 addr::BattleFx_Finish};
    Dispatch(kPhases, 4, Sc()[1], "Magic225_Task");
    WalkPool(kShardPool, kShards, addr::Magic225Shard_Task);
}

// original 0x4F8FD0: the shard pool's +0..+2 cleared; the task at the side's
// centre; +0xB 0, +9 0x18, +1 on; the veil (kind 1, 0x65); CLUT rows 2 and 26
// back from their source as they are (no STP bits); sound 0x101.
S38_EXPORT void __cdecl Magic225_Start(void) {
    ClearPool(kShardPool, kShards);
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[0xB] = 0;
    Sc()[9] = 0x18;
    Inc(Sc()[1]);
    AdoptChild(NewTask(0x65));
    for (unsigned k = 0; k < 0x100; ++k) {
        Gfx_ClutStrip[0x200 + k] = Gfx_ClutStripSource[0x200 + k];
        Gfx_ClutStrip[0x1A00 + k] = Gfx_ClutStripSource[0x1A00 + k];
    }
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x101);
}

// original 0x4F9090: +9 down; at 0 sixteen shards, each after sound 0x100:
// owned by this task, numbered, delay +9 8, 12, .. 0x44, counted in +0xB (a
// full pool skips one); +1 on.
S38_EXPORT void __cdecl Magic225_Spawn(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    unsigned i = 0;
    for (unsigned delay = 8; delay < 0x48; delay += 4, ++i) {
        MH_CALL(Sound_PlayById)(0x100);
        const unsigned index = Alloc(addr::Magic225_ShardAlloc);
        if (index == 0xFF) continue;
        unsigned char* const self = Sc();
        unsigned char* const rec = PoolRecord(kShardPool, index);
        SetLong(rec + 0x80, static_cast<std::int32_t>(Key(self)));
        rec[1] = 0;
        rec[0xB] = static_cast<unsigned char>(i);
        rec[9] = static_cast<unsigned char>(delay);
        Inc(self[0xB]);
    }
    Inc(Sc()[1]);
}

// original 0x4F9130: once every shard has ended (+0xB 0): each actor of the
// target's side (the enemies 3..10 with the side bit, else the party 0..2)
// that Battle_ActorIsOut says is in gets three stats (BuffActor); +1 on.
S38_EXPORT void __cdecl Magic225_Apply(void) {
    if (Sc()[0xB] != 0) return;
    if (TargetByte() & 0x40) {
        for (unsigned actor = 3; actor < 11; ++actor)
            if (MH_CALL(Battle_ActorIsOut)(actor) == 0) BuffActor(actor);
    } else {
        for (unsigned actor = 0; actor < 3; ++actor)
            if (MH_CALL(Battle_ActorIsOut)(actor) == 0) BuffActor(actor);
    }
    Inc(Sc()[1]);
}

// original 0x4F93A0: the veil's kind-1 task, a jmp through
// Magic225Veil_TaskTable (one entry) by +1, unchecked.
S38_EXPORT void __cdecl Magic225Veil_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::Magic225Veil_Run};
    Dispatch(kKinds, 1, Sc()[1], "Magic225Veil_Task");
}

// original 0x4F93C0: with the frame-offset table the effects' (0x8E3580), a
// call through Magic225Veil_Steps (five entries: _Start, _FadeIn, MAGIC086's
// BarrierRing_Grow, _WaitBuffs, _End) by +2; then while +0 and +2 are set,
// the shade and four bursts at the screen's corners; the battle's table
// back.
S38_EXPORT void __cdecl Magic225Veil_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {addr::Magic225Veil_Start, addr::Magic225Veil_FadeIn,
                                                addr::BarrierRing_Grow, addr::Magic225Veil_WaitBuffs,
                                                addr::Magic225Veil_End};
    const unsigned phase = Sc()[2];
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    Dispatch(kSteps, 5, phase, "Magic225Veil_Run");
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) {
        Call0(addr::Magic225Veil_DrawShade);
        MH_AT(BurstFn, addr::Magic225Veil_DrawBurst)(0x30, 0x10, 0x52, 0);
        MH_AT(BurstFn, addr::Magic225Veil_DrawBurst)(0x12C, 0, 0x80, 1);
        MH_AT(BurstFn, addr::Magic225Veil_DrawBurst)(0x40, 0xF0, 0x6C, 2);
        MH_AT(BurstFn, addr::Magic225Veil_DrawBurst)(0x140, 0xF0, 0x5C, 3);
    }
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x4F9450: the actor's sprite (0x904B3C) and the side's live
// actors to draw layer 2; the veil's own layer 7 in the event battle of kind
// 0x37, else 3; +0xB 1, +9 0, +0xA 0, +2 on.
S38_EXPORT void __cdecl Magic225Veil_Start(void) {
    Pointer(kActorRecord)[0x29] = 2;
    LayerSide(2);
    Sc()[0x29] = Mem(kEventBattle)[0] == 0x37 ? 7 : 3;
    Sc()[0xB] = 1;
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4F9510: +0xA up by 2; at 0x10 +0xB 0 and +2 on.
S38_EXPORT void __cdecl Magic225Veil_FadeIn(void) {
    AddB(Sc()[0xA], 2);
    if (Sc()[0xA] != 0x10) return;
    Sc()[0xB] = 0;
    Inc(Sc()[2]);
}

// original 0x4F9540: while the owner's +0xB (its shards and popups) is above
// 1, nothing; then +0xB 1 and +2 on.
S38_EXPORT void __cdecl Magic225Veil_WaitBuffs(void) {
    if (Owner()[0xB] > 1) return;
    Sc()[0xB] = 1;
    Inc(Sc()[2]);
}

// original 0x4F9560: +9 down by 2 while not 0; +0xA down; at 0 the actor's
// sprite and the side's live actors back to layer 4, the owner's count +0xB
// down, the task freed.
S38_EXPORT void __cdecl Magic225Veil_End(void) {
    {
        unsigned char* const s = Sc();
        if (s[9] != 0) s[9] = static_cast<unsigned char>(s[9] - 2);
    }
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Pointer(kActorRecord)[0x29] = 4;
    LayerSide(4);
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F9610 (x, y, radius, kind; the words' low halves, kind's low
// byte): a burst at the screen point (x, y) on the veil's layer +0x29 -
// thirty-two semi-transparent gouraud triangles from the centre to a circle
// of the radius (0x903850), then thirty-two gouraud quads between that circle
// (the colour) and one of 1.5 x the radius (0x903854, black 1); the colour
// Magic225Veil_BurstColours[kind] x +9 (0x903860..0x903868, read back at
// every use), 0x80 of angle a step; tpage 0x55.
S38_EXPORT void __cdecl Magic225Veil_DrawBurst(int x, int y, int radius, int kind) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(Sc()[0x29], 0xC);
    const int r = static_cast<short>(radius);
    SetSD(0, static_cast<std::uint32_t>(r));
    SetSD(4, static_cast<std::uint32_t>(r + DivPow2(r, 1)));
    {
        const unsigned char* const s = Sc();
        const unsigned char* const colour = Mem(kBurstColours + 3u * (static_cast<unsigned>(kind) & 0xFF));
        SetSD(0xC, static_cast<std::uint32_t>(s[9]) << 2);
        SetSD(0x10, Mul(colour[0], s[9]));
        SetSD(0x14, Mul(colour[1], s[9]));
        SetSD(0x18, Mul(colour[2], s[9]));
    }
    const int cx = static_cast<short>(x);
    const int cy = static_cast<short>(y);
    int a = 0;
    for (unsigned n = 0; n < 0x20; ++n) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, cx);
        PutFloat(p + 0xC, cy);
        int v = Sin(a);
        PutFloat(p + 0x18, Mul12(v, SD(0)) + cx);
        v = Cos(a);
        PutFloat(p + 0x1C, Mul12(v, SD(0)) + cy);
        a += 0x80;
        v = Sin(a);
        PutFloat(p + 0x28, Mul12(v, SD(0)) + cx);
        v = Cos(a);
        PutFloat(p + 0x2C, Mul12(v, SD(0)) + cy);
        for (unsigned k : {0u, 0x10u, 0x20u}) {
            p[4 + k] = SB(0x10);
            p[5 + k] = SB(0x14);
            p[6 + k] = SB(0x18);
        }
        MH_CALL(Gfx_CommitPrim)(Sc()[0x29], 0x34);
    }
    a = 0;
    for (unsigned n = 0; n < 0x20; ++n) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        int v = Sin(a);
        PutFloat(p + 8, Mul12(v, SD(4)) + cx);
        v = Cos(a);
        PutFloat(p + 0xC, Mul12(v, SD(4)) + cy);
        const int b = a + 0x80;
        v = Sin(b);
        PutFloat(p + 0x18, Mul12(v, SD(4)) + cx);
        v = Cos(b);
        PutFloat(p + 0x1C, Mul12(v, SD(4)) + cy);
        v = Sin(a);
        PutFloat(p + 0x28, Mul12(v, SD(0)) + cx);
        v = Cos(a);
        PutFloat(p + 0x2C, Mul12(v, SD(0)) + cy);
        a = b;
        v = Sin(a);
        PutFloat(p + 0x38, Mul12(v, SD(0)) + cx);
        v = Cos(a);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        PutFloat(p + 0x3C, Mul12(v, SD(0)) + cy);
        for (unsigned k : {0x20u, 0x30u}) {
            p[4 + k] = SB(0x10);
            p[5 + k] = SB(0x14);
            p[6 + k] = SB(0x18);
        }
        MH_CALL(Gfx_CommitPrim)(Sc()[0x29], 0x44);
    }
}

// original 0x4F9980: the shade - one flat quad over the whole screen, grey
// (+0xA + 1) x 15 (Scratch_Swap), semi-transparent by +0xB (its abr, and the
// tpage ((+0xB & 3) << 5) | 0x15), on the veil's layer.
S38_EXPORT void __cdecl Magic225Veil_DrawShade(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, ((Sc()[0xB] & 3u) << 5) | 0x15u, 0);
    MH_CALL(Gfx_CommitPrim)(Sc()[0x29], 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyF4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, Sc()[0xB]);
    SetSD(0xC, (static_cast<std::uint32_t>(Sc()[0xA]) + 1) * 15);
    SetLong(p + 8, 0);
    SetLong(p + 0xC, 0);
    SetLong(p + 0x14, kRight);
    SetLong(p + 0x18, 0);
    SetLong(p + 0x20, 0);
    SetLong(p + 0x24, kBottom);
    SetLong(p + 0x2C, kRight);
    SetLong(p + 0x30, kBottom);
    p[4] = SB(0xC);
    p[5] = SB(0xC);
    p[6] = SB(0xC);
    MH_CALL(Gfx_CommitPrim)(Sc()[0x29], 0x38);
}

// original 0x4F9A40: a shard record's task, a jmp through
// Magic225Shard_TaskTable (one entry) by +1, unchecked.
S38_EXPORT void __cdecl Magic225Shard_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::Magic225Shard_Run};
    Dispatch(kKinds, 1, Sc()[1], "Magic225Shard_Task");
}

// original 0x4F9A60: with the effects' frame-offset table, a call through
// Magic225Shard_Steps (three entries) by +2; then while +0 and +2 are set the
// sprite updated on the screen; the battle's table back.
S38_EXPORT void __cdecl Magic225Shard_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::Magic225Shard_Launch, addr::Magic225Shard_Fly,
                                                addr::Magic225Shard_Fall};
    const unsigned phase = Sc()[2];
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    Dispatch(kSteps, 3, phase, "Magic225Shard_Run");
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x4F9AA0: +9 down; at 0 the shard starts at the acting actor's
// sprite (0x904B3C, read once): its direction; (0x10000, 0) turned by it from
// the sprite's position, +0x3C the sprite's + 0x1000000; heading +0x14 Math_Ratan2 toward
// the owner (dx, dz as floats), Rand & 0xFF to one side or the other by bit 0
// of +0xB; its sprite fields (size 0x10000, frame table 0x1D, layer 2, ...);
// animation 0; speed +0xC 0xC, +9 0x10, +2 on.
S38_EXPORT void __cdecl Magic225Shard_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    unsigned char* const rec = Pointer(kActorRecord);
    Sc()[8] = rec[8];
    SetLong(Sc() + 0xC, 0x10000);
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0xC)) +
                                                    static_cast<std::uint32_t>(Long(rec + 0x34))));
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x10)) +
                                                    static_cast<std::uint32_t>(Long(rec + 0x38))));
    }
    SetLong(Sc() + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(rec + 0x3C)) + 0x1000000u));
    {
        const unsigned char* const o = Owner();
        const unsigned char* const s = Sc();
        const auto dz = static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(o + 0x38)) -
                                                  static_cast<std::uint32_t>(Long(s + 0x38)));
        const auto dx = static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(o + 0x34)) -
                                                  static_cast<std::uint32_t>(Long(s + 0x34)));
        const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
        SetLong(Sc() + 0x14, heading);
    }
    {
        unsigned char* const s = Sc();
        unsigned char* const heading = s + 0x14;
        if (s[0xB] & 1) {
            const std::uint32_t r = RandCall();
            AddLong(heading, r & 0xFF);
        } else {
            const std::uint32_t r = RandCall();
            AddLong(heading, 0u - (r & 0xFF));
        }
    }
    {
        unsigned char* const s = Sc();
        s[0x25] = 0x1D;
        s[0x26] = 0x80;
        SetLong(s + 0x40, 0x10000);
        SetLong(s + 0x44, 0x10000);
        s[0x48] = 2;
        s[0x27] = 2;
        s[0x28] = 1;
        s[0x24] = 0;
        s[0x5C] = 0;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        s[0x2A] = static_cast<unsigned char>(s[8] & 1);
        s[0x29] = 2;
        SetWord(s + 0x2C, 0);
        s[0x2B] = 1;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    SetLong(Sc() + 0xC, 0xC);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4F9C60: the script ticked (its answer unread); the size +0x40 /
// +0x44 up by 0x800; the position on by the sine / cosine of the heading x
// the speed +0xC; on odd frames the speed down by 1, and at 4 +2 on instead.
S38_EXPORT void __cdecl Magic225Shard_Fly(void) {
    MH_CALL(Sprite_ScriptTick)();
    AddLong(Sc() + 0x40, 0x800);
    AddLong(Sc() + 0x44, 0x800);
    {
        unsigned char* const s = Sc();
        unsigned char* const x = s + 0x34;
        const int v = Sin(Long(s + 0x14));
        AddLong(x, Mul(v, Long(Sc() + 0xC)));
    }
    {
        unsigned char* const s = Sc();
        unsigned char* const z = s + 0x38;
        const int v = Cos(Long(s + 0x14));
        AddLong(z, Mul(v, Long(Sc() + 0xC)));
    }
    if ((Mem(kFrameCounter)[0] & 1) == 0) return;
    unsigned char* const s = Sc();
    const std::int32_t speed = Long(s + 0xC);
    if (speed == 4) {
        Inc(s[2]);
        return;
    }
    SetLong(s + 0xC, speed - 1);
}

// original 0x4F9CF0: the script ticked; the size up by 0x800; the position on
// by 4 x the sine / cosine of the heading; +9 down, at 0 the owner's count
// +0xB down and MAGIC219's record free (a tail jmp).
S38_EXPORT void __cdecl Magic225Shard_Fall(void) {
    MH_CALL(Sprite_ScriptTick)();
    AddLong(Sc() + 0x40, 0x800);
    AddLong(Sc() + 0x44, 0x800);
    {
        unsigned char* const s = Sc();
        unsigned char* const x = s + 0x34;
        const int v = Sin(Long(s + 0x14));
        AddLong(x, static_cast<std::uint32_t>(Shl(v, 2)));
    }
    {
        unsigned char* const s = Sc();
        unsigned char* const z = s + 0x38;
        const int v = Cos(Long(s + 0x14));
        AddLong(z, static_cast<std::uint32_t>(Shl(v, 2)));
    }
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4F9D80: the first of the shard pool's 32 records without bit 0
// of +0 gets it; its index in al, 0xFF when all are taken.
S38_EXPORT unsigned char __cdecl Magic225_ShardAlloc(void) { return PoolAlloc(kShardPool, kShards); }

// ===========================================================================
// MAGIC226/227 (rows 135 and 147, MeteorStrike read one id down)

namespace {
constexpr std::uint32_t kChipShades = 0x65C364;   // MeteorStrikeChip_Shades: four bytes by i / 8
constexpr std::uint32_t kChipFades = 0x65C368;    // MeteorStrikeChip_Fades: four bytes by i / 8
constexpr std::uint32_t kChipLifts = 0x65C374;    // MeteorStrikeChip_Lifts: four dwords by +0xB & 3
}  // namespace

// original 0x4F9DE0: the kind-2 task. A three-entry stack table by +1
// (MeteorStrike_Start, MagicFx_CountDownFlag10, BattleFx_Finish), then the
// record pool walked.
S38_EXPORT void __cdecl MeteorStrike_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::MeteorStrike_Start, addr::MagicFx_CountDownFlag10,
                                                 addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "MeteorStrike_Task");
    WalkPool(kRecordPool, kRecords, addr::MeteorStrikeRecord_Task);
}

// original 0x4F9E60: the record pool's +0..+2 cleared; the task at the side's
// centre and on the screen; its direction the acting actor's sprite's,
// turned round (xor 2) when the target's side bit names the other side from
// the actor (0x40 with a party actor, none with an enemy); +0xB 0, +9 0x10,
// +1 on; the rock (kind 1, 0x5F); sound 0x100; CLUT row 26 and the first 16
// words of row 2 back with their STP bits, the first word of each without.
S38_EXPORT void __cdecl MeteorStrike_Start(void) {
    ClearPool(kRecordPool, kRecords);
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
    AdoptChild(NewTask(0x5F));
    MH_CALL(Sound_PlayById)(0x100);
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    for (unsigned k = 0x200; k < 0x210; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    {
        const unsigned short first26 = Gfx_ClutStripSource[0x1A00];
        const unsigned short first2 = Gfx_ClutStripSource[0x200];
        Gfx_ClutStrip[0x1A00] = first26;
        Gfx_ClutStrip[0x200] = first2;
    }
    Gfx_ClutStripDirty = 1;
}

// original 0x4F9F70: a phase ten overlays' stack tables hold (MAGIC053, 124,
// 129, 130, 132, 223, 226 and others): +9 down; at 0 the target flagged 0x10
// and +1 on.
S38_EXPORT void __cdecl MagicFx_CountDownFlag10(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Inc(Sc()[1]);
}

// original 0x4F9FB0: the rock's kind-1 task, a jmp through
// MeteorStrikeRock_TaskTable (one entry) by +1, unchecked.
S38_EXPORT void __cdecl MeteorStrikeRock_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::MeteorStrikeRock_Run};
    Dispatch(kKinds, 1, Sc()[1], "MeteorStrikeRock_Task");
}

// original 0x4F9FD0: with the effects' frame-offset table, a call through
// MeteorStrikeRock_Steps (five entries) by +2; then while bit 0 of +0 and +2
// are set: the screen point and update, MAGIC219's 0x4F6020 (the matrix),
// MAGIC053's LavaburstChild_DrawGlow, the ground ring, the matrix popped; the
// battle's table back.
S38_EXPORT void __cdecl MeteorStrikeRock_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {addr::MeteorStrikeRock_Launch, addr::MeteorStrikeRock_Fall,
                                                addr::MeteorStrikeRock_Shake, addr::MeteorStrikeRock_Hide,
                                                addr::MeteorStrikeRock_End};
    const unsigned phase = Sc()[2];
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    Dispatch(kSteps, 5, phase, "MeteorStrikeRock_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) != 0 && s[2] != 0) {
        MH_CALL(BattleActor_UpdateScreenXY)();
        MH_CALL(Sprite_UpdateScreen)();
        Call0(addr::MagicFx_PushRecordMatrix);
        MH_CALL(LavaburstChild_DrawGlow)();
        Call0(addr::MeteorStrikeRock_DrawRing);
        MH_CALL(Gte_PopMatrix)();
    }
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x4FA030: the rock starts at the owner, its direction; lifted by
// (0x20000, 0) turned and a height +0x14 0x9000000 added to the owner's; the step
// (-0x1000, 0) turned, the fall +0x20 0x300000; its sprite fields (size
// 0x10000, frame table 0x1D / 0x1A, layer 4, ...); animation 0; +4 0, +0xB 0,
// +9 0x20, +0xA 0x10, +2 on.
S38_EXPORT void __cdecl MeteorStrikeRock_Launch(void) {
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0xC, 0x20000);
    SetLong(Sc() + 0x10, 0);
    SetLong(Sc() + 0x14, 0x9000000);
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x34, static_cast<std::uint32_t>(Long(s + 0xC)));
    }
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x38, static_cast<std::uint32_t>(Long(s + 0x10)));
    }
    {
        const std::uint32_t height = static_cast<std::uint32_t>(Long(Owner() + 0x3C));
        unsigned char* const s = Sc();
        SetLong(s + 0x3C, static_cast<std::int32_t>(height + static_cast<std::uint32_t>(Long(s + 0x14))));
    }
    SetLong(Sc() + 0xC, static_cast<std::int32_t>(0xFFFFF000u));
    SetLong(Sc() + 0x10, 0);
    SetLong(Sc() + 0x20, 0x300000);
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        s[0x25] = 0x1D;
        s[0x26] = 0;
        s[0x27] = 0x1A;
        s[0x28] = 1;
        SetLong(s + 0x40, 0x10000);
        SetLong(s + 0x44, 0x10000);
        s[0x48] = 0;
        s[0x5D] = 0;
        s[0x5E] = 0;
        s[0x5F] = 0;
        s[0x5C] = 0;
        s[0x2A] = 0;
        s[0x29] = 4;
        SetWord(s + 0x2C, 0);
        s[0x2B] = 0;
        s[0x24] = 0;
    }
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[4] = 0;
    Sc()[0xB] = 0;
    Sc()[9] = 0x20;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4FA1C0: the rock on by its step, its height +0x14 down by the
// fall, set over the owner's; +0xB up by 6; every fourth frame a trail
// (record kind 1, +9 1) owned by the rock and counted in its +4; +9 down; at
// 0 thirty-two chips owned by the kind-2 task (the rock's owner) and counted
// in its +0xB: +0xB i & 7, +9 (i / 8) x 3 + 1, +0x5D and +0xA from
// MeteorStrikeChip_Shades / _Fades by i / 8 (a full pool skips one); the
// camera's first angle up by 0x14; sound 0x101; +9 8, +2 on.
S38_EXPORT void __cdecl MeteorStrikeRock_Fall(void) {
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x34, static_cast<std::uint32_t>(Long(s + 0xC)));
    }
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x38, static_cast<std::uint32_t>(Long(s + 0x10)));
    }
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x14, 0u - static_cast<std::uint32_t>(Long(s + 0x20)));
    }
    {
        const std::uint32_t height = static_cast<std::uint32_t>(Long(Owner() + 0x3C));
        unsigned char* const s = Sc();
        SetLong(s + 0x3C, static_cast<std::int32_t>(height + static_cast<std::uint32_t>(Long(s + 0x14))));
    }
    AddB(Sc()[0xB], 6);
    if ((Mem(kFrameCounter)[0] & 3) == 0) {
        const unsigned index = Alloc(addr::MeteorStrike_RecordAlloc);
        if (index != 0xFF) {
            unsigned char* const self = Sc();
            unsigned char* const rec = PoolRecord(kRecordPool, index);
            SetLong(rec + 0x80, static_cast<std::int32_t>(Key(self)));
            rec[1] = 1;
            rec[9] = 1;
            Inc(self[4]);
        }
    }
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    for (unsigned i = 0; i < 0x20; ++i) {
        const unsigned index = Alloc(addr::MeteorStrike_RecordAlloc);
        if (index == 0xFF) continue;
        unsigned char* const owner = Owner();
        unsigned char* const rec = PoolRecord(kRecordPool, index);
        SetLong(rec + 0x80, static_cast<std::int32_t>(Key(owner)));
        rec[1] = 0;
        rec[0x5D] = Mem(kChipShades)[i >> 3];
        rec[0xB] = static_cast<unsigned char>(i & 7);
        rec[9] = static_cast<unsigned char>((i >> 3) * 3 + 1);
        rec[0xA] = Mem(kChipFades)[i >> 3];
        Inc(owner[0xB]);
    }
    Camera_Angles[0] = static_cast<short>(Camera_Angles[0] + 0x14);
    MH_CALL(Sound_PlayById)(0x101);
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4FA340: the camera's first angle up or down by 0x14 by bit 0 of
// +9, +9 down; at 0 the angle 0xFD56, +9 0x3C, +2 on.
S38_EXPORT void __cdecl MeteorStrikeRock_Shake(void) {
    unsigned char* const s = Sc();
    Camera_Angles[0] = static_cast<short>(Camera_Angles[0] + ((s[9] & 1) ? 0x14 : -0x14));
    Dec(s[9]);
    unsigned char* const t = Sc();
    if (t[9] != 0) return;
    Camera_Angles[0] = static_cast<short>(0xFD56);
    t[9] = 0x3C;
    Inc(Sc()[2]);
}

// original 0x4FA390: +9 down; at 0 +0 |= 0x20 (the sprite hidden), +0x5C 1,
// the tint +0x5D..+0x5F 0, +2 on.
S38_EXPORT void __cdecl MeteorStrikeRock_Hide(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    Sc()[0x5C] = 1;
    Sc()[0x5D] = 0;
    Sc()[0x5E] = 0;
    Sc()[0x5F] = 0;
    Inc(Sc()[2]);
}

// original 0x4FA3E0: +0xA down by 2 while not 0; the tint +0x5D..+0x5F down
// by 8 each; at +0x5D 0x80 the owner's count +0xB down and the task freed (a
// tail jmp).
S38_EXPORT void __cdecl MeteorStrikeRock_End(void) {
    {
        unsigned char* const s = Sc();
        if (s[0xA] != 0) s[0xA] = static_cast<unsigned char>(s[0xA] - 2);
    }
    AddB(Sc()[0x5D], 0xF8);
    AddB(Sc()[0x5E], 0xF8);
    AddB(Sc()[0x5F], 0xF8);
    if (Sc()[0x5D] != 0x80) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4FA440: a ground ring under the pushed matrix (MAGIC053's
// Lavaburst calls it too): sixty-four semi-transparent gouraud quads between
// a circle of radius +0xB (0x903850) and one of 2 x +0xB (0x903852), 0x40 of
// angle a step, each vertex pair carried round in Prim_VertexScratch (the
// outer z 0); black 1 inside, grey +0xA x 4 (0x90385A) outside; tpage 0x55,
// layer 5.
S38_EXPORT void __cdecl MeteorStrikeRock_DrawRing(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[0xB]);
        SetSW(2, static_cast<unsigned>(s[0xB]) << 1);
        SetSW(0xA, static_cast<unsigned>(s[0xA]) << 2);
    }
    int v = Sin(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Cos(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Sin(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x1C, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0x14, 0);
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t x = VW(8), y = VW(0xA);
            SetVW(0, x);
            SetVW(2, y);
        }
        v = Sin(a);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(a);
        {
            const int t = Mul12(v, SS(2));
            const std::uint16_t x = VW(0x18);
            SetVW(0xA, static_cast<unsigned>(t));
            const std::uint16_t y = VW(0x1A);
            SetVW(0x10, x);
            SetVW(0x12, y);
        }
        v = Sin(a);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = SB(0xA);
        p[0x25] = SB(0xA);
        p[0x26] = SB(0xA);
        p[0x34] = SB(0xA);
        p[0x35] = SB(0xA);
        p[0x36] = SB(0xA);
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
}

// original 0x4FA670: a record's task, a jmp through MeteorStrikeRecord_Kinds
// (two entries: the chip, the trail) by +1, unchecked.
S38_EXPORT void __cdecl MeteorStrikeRecord_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {addr::MeteorStrikeChip_Run, addr::MeteorStrikeTrail_Run};
    Dispatch(kKinds, 2, Sc()[1], "MeteorStrikeRecord_Task");
}

// original 0x4FA690: a call through MeteorStrikeChip_Steps (three entries) by
// +2; then while +0 and +2 are set, the screen point and the chip (a tail
// jmp).
S38_EXPORT void __cdecl MeteorStrikeChip_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::MeteorStrikeChip_Launch, addr::MeteorStrikeChip_Rise,
                                                addr::MeteorStrikeChip_Fall};
    Dispatch(kSteps, 3, Sc()[2], "MeteorStrikeChip_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(addr::MeteorStrikeChip_Draw);
}

// original 0x4FA6C0: +9 down; at 0 the chip starts 24 x the sine / cosine of
// ((+0xB - 2) & 0xF) << 8 (the word 0x903854, read back for the cosine) round
// the owner, at its height; the lift +0x14 MeteorStrikeChip_Lifts[+0xB & 3],
// the pull +0x20 minus a sixteenth of it; +0x5E 0x10, +9 0x20, +2 on.
S38_EXPORT void __cdecl MeteorStrikeChip_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const auto angle = static_cast<short>(((Sc()[0xB] - 2u) & 0xFu) << 8);
    SetSW(4, static_cast<unsigned>(angle));
    int v = Sin(angle);
    {
        const std::uint32_t base = static_cast<std::uint32_t>(Long(Owner() + 0x34));
        SetLong(Sc() + 0x34, static_cast<std::int32_t>(base + Mul(v, 24)));
    }
    v = Cos(SS(4));
    {
        const std::uint32_t base = static_cast<std::uint32_t>(Long(Owner() + 0x38));
        SetLong(Sc() + 0x38, static_cast<std::int32_t>(base + Mul(v, 24)));
    }
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x14, Long(Mem(kChipLifts + 4u * (s[0xB] & 3u))));
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x20, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(DivPow2(Long(s + 0x14), 4))));
    }
    Sc()[0x5E] = 0x10;
    Sc()[9] = 0x20;
    Inc(Sc()[2]);
}

// The chip's drift: the position on by `scale` x the sine / cosine of
// ((+0xB - 2) & 0xF) << 8 (0x903854, read back), the height on by the lift,
// the lift on by the pull.
void ChipDrift(int scale) {
    {
        unsigned char* const s = Sc();
        unsigned char* const x = s + 0x34;
        const auto angle = static_cast<short>(((s[0xB] - 2u) & 0xFu) << 8);
        SetSW(4, static_cast<unsigned>(angle));
        const int v = Sin(angle);
        AddLong(x, Mul(v, scale));
    }
    {
        unsigned char* const s = Sc();
        const short angle = SS(4);
        unsigned char* const z = s + 0x38;
        const int v = Cos(angle);
        AddLong(z, Mul(v, scale));
    }
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x3C, static_cast<std::uint32_t>(Long(s + 0x14)));
    }
    {
        unsigned char* const s = Sc();
        AddLong(s + 0x14, static_cast<std::uint32_t>(Long(s + 0x20)));
    }
}

// original 0x4FA790: the drift at 2 x; +9 down; at 0 the lift again from
// MeteorStrikeChip_Lifts, the pull minus an eighth of it, +9 0x10, +2 on.
S38_EXPORT void __cdecl MeteorStrikeChip_Rise(void) {
    ChipDrift(2);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x14, Long(Mem(kChipLifts + 4u * (s[0xB] & 3u))));
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x20, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(DivPow2(Long(s + 0x14), 3))));
    }
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4FA860: the drift at 1 x; +0x5E down while not 0; +9 down; at 0
// the owner's count +0xB down and MAGIC219's record free (a tail jmp).
S38_EXPORT void __cdecl MeteorStrikeChip_Fall(void) {
    ChipDrift(1);
    {
        unsigned char* const s = Sc();
        if (s[0x5E] != 0) Dec(s[0x5E]);
    }
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4FA910: one semi-transparent textured quad (tpage 0x35) linked at
// the chip's map point, its corners at the angles 0x200, 0x600, 0xE00, 0xA00
// (cosine for x, sine for y), radius +0xA (0x903850) round the screen point;
// page (0x340, 0x100) abr 1, CLUT (0, 0x1E2), the texture's 0x20 x 0x20 at
// (0, 0x80); grey (signed +0x5E) x 6 (0x90385A), red plus the tint +0x5D.
S38_EXPORT void __cdecl MeteorStrikeChip_Draw(void) {
    DrawMode(0x35);
    {
        const unsigned char* const s = Sc();
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)),
                                    2, 0xC);
    }
    unsigned char* const p = Gfx_PacketNext;
    {
        const unsigned char* const s = Sc();
        SetSW(0xA, static_cast<unsigned>(static_cast<signed char>(s[0x5E]) * 6));
        SetSW(0, s[0xA]);
    }
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned c = 0; c < 4; ++c) {
        int v = Cos(kAngles[c]);
        PutFloat(p + 8 + 0x10 * c, Mul12(v, SS(0)) + S16(Sc() + 0x2E));
        v = Sin(kAngles[c]);
        PutFloat(p + 0xC + 0x10 * c, Mul12(v, SS(0)) + S16(Sc() + 0x30));
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
    p[4] = static_cast<unsigned char>(Sc()[0x5D] + SB(0xA));
    p[5] = SB(0xA);
    p[6] = SB(0xA);
    {
        const unsigned char* const s = Sc();
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)),
                                    2, 0x48);
    }
}

// original 0x4FAB70: a call through MeteorStrikeTrail_Steps (three entries) by
// +2; then while +0 and +2 are set, under the task's matrix
// (MagicFx_PushActorMatrix) the trail, the matrix popped (a tail jmp).
S38_EXPORT void __cdecl MeteorStrikeTrail_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::MeteorStrikeTrail_Start, addr::MeteorStrikeTrail_Follow,
                                                addr::MeteorStrikeTrail_Fade};
    Dispatch(kSteps, 3, Sc()[2], "MeteorStrikeTrail_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::MeteorStrikeTrail_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4FABB0: +9 down; at 0 the trail starts at the owner (the rock):
// its direction and position, its height - 0x3000000; the step +0x14
// 0x60000; +9 8, +0xA 0x10, +2 on.
S38_EXPORT void __cdecl MeteorStrikeTrail_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x3C)) - 0x3000000u));
    SetLong(Sc() + 0x14, 0x60000);
    Sc()[9] = 8;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// The trail at the rock's x / z, its height +0x3C down by +0x14 a frame.
void TrailFollow() {
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    unsigned char* const s = Sc();
    AddLong(s + 0x3C, 0u - static_cast<std::uint32_t>(Long(s + 0x14)));
}

// original 0x4FAC40: at the rock, its height down by +0x14; +9 up; at 0x18 +2 on.
S38_EXPORT void __cdecl MeteorStrikeTrail_Follow(void) {
    TrailFollow();
    Inc(Sc()[9]);
    if (Sc()[9] == 0x18) Inc(Sc()[2]);
}

// original 0x4FAC90: +9 up, +0xA down by 2; at the rock, its height down by +0x14; at
// +0xA 0 the owner's trail count +4 down and MAGIC219's record free (a tail
// jmp).
S38_EXPORT void __cdecl MeteorStrikeTrail_Fade(void) {
    Inc(Sc()[9]);
    AddB(Sc()[0xA], 0xFE);
    TrailFollow();
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[4]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4FAD00: the trail - thirty-two semi-transparent gouraud quads
// between a circle of radius +9 x 0x20 (0x903852) at z -0x40 and one of +9 x
// 24 (0x903850) at z 0, 0x80 of angle a step, each vertex pair carried round
// in Prim_VertexScratch; black 1 above, (+0xA x 12, x 6, x 6) (0x90385A..E)
// below; each linked at the task's map point plus its outer vertex << 9;
// tpage 0x35.
S38_EXPORT void __cdecl MeteorStrikeTrail_Draw(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0, s[9] * 24u);
        SetSW(2, static_cast<unsigned>(s[9]) << 5);
        SetSW(0xA, s[0xA] * 12u);
        SetSW(0xC, s[0xA] * 6u);
        SetSW(0xE, s[0xA] * 6u);
    }
    int v = Sin(0);
    SetVW(0, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Cos(0);
    SetVW(2, static_cast<unsigned>(Mul12(v, SS(2))));
    SetVW(0xC, 0xFFC0);
    SetVW(4, 0xFFC0);
    v = Sin(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x1C, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0x14, 0);
    for (int a = 0x80; a < 0x1080; a += 0x80) {
        {
            const std::uint16_t x = VW(0), y = VW(2);
            SetVW(8, x);
            SetVW(0xA, y);
        }
        v = Sin(a);
        SetVW(0, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(a);
        {
            const int t = Mul12(v, SS(2));
            const std::uint16_t x = VW(0x10);
            SetVW(2, static_cast<unsigned>(t));
            const std::uint16_t y = VW(0x12);
            SetVW(0x18, x);
            SetVW(0x1A, y);
        }
        v = Sin(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        const auto outer_z = static_cast<short>(Mul12(v, SS(0)));
        const int outer_x = VS(0x10);
        std::uint32_t map_x, map_z;
        {
            const unsigned char* const s = Sc();
            SetVW(0x12, static_cast<unsigned>(outer_z));
            map_x = static_cast<std::uint32_t>(Shl(outer_x, 9)) + static_cast<std::uint32_t>(Long(s + 0x34));
            map_z = static_cast<std::uint32_t>(Shl(outer_z, 9)) + static_cast<std::uint32_t>(Long(s + 0x38));
        }
        DrawMode(0x35);
        MH_CALL(MapView_LinkPrimAt)(map_x, map_z, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = SB(0xA);
        p[0x25] = SB(0xC);
        p[0x26] = SB(0xE);
        p[0x34] = SB(0xA);
        p[0x35] = SB(0xC);
        p[0x36] = SB(0xE);
        MH_CALL(MapView_LinkPrimAt)(map_x, map_z, 0, 0x48);
    }
}

// original 0x4FAF90: the first of the record pool's 48 records without bit 0
// of +0 gets it; its index in al, 0xFF when all are taken.
S38_EXPORT unsigned char __cdecl MeteorStrike_RecordAlloc(void) { return PoolAlloc(kRecordPool, kRecords); }

void MagicS38_Inject() {
    if (bof3::WantsShadow("magic_s38")) magic_s38::SelfTest();
    BOF3_INJECT(Tempest_Task);
    BOF3_INJECT(Tempest_Start);
    BOF3_INJECT(Tempest_End);
    BOF3_INJECT(TempestFlash_Task);
    BOF3_INJECT(TempestFlash_Run);
    BOF3_INJECT(TempestFlash_Draw);
    BOF3_INJECT(TempestGust_Task);
    BOF3_INJECT(TempestGust_Run);
    BOF3_INJECT(TempestGust_Launch);
    BOF3_INJECT(TempestGust_Drift);
    BOF3_INJECT(TempestGust_Fly);
    BOF3_INJECT(TempestGust_Draw);
    BOF3_INJECT(Tempest_GustAlloc);
    BOF3_INJECT(Magic225_Task);
    BOF3_INJECT(Magic225_Start);
    BOF3_INJECT(Magic225_Spawn);
    BOF3_INJECT(Magic225_Apply);
    BOF3_INJECT(Magic225Veil_Task);
    BOF3_INJECT(Magic225Veil_Run);
    BOF3_INJECT(Magic225Veil_Start);
    BOF3_INJECT(Magic225Veil_FadeIn);
    BOF3_INJECT(Magic225Veil_WaitBuffs);
    BOF3_INJECT(Magic225Veil_End);
    BOF3_INJECT(Magic225Veil_DrawBurst);
    BOF3_INJECT(Magic225Veil_DrawShade);
    BOF3_INJECT(Magic225Shard_Task);
    BOF3_INJECT(Magic225Shard_Run);
    BOF3_INJECT(Magic225Shard_Launch);
    BOF3_INJECT(Magic225Shard_Fly);
    BOF3_INJECT(Magic225Shard_Fall);
    BOF3_INJECT(Magic225_ShardAlloc);
    BOF3_INJECT(MeteorStrike_Task);
    BOF3_INJECT(MeteorStrike_Start);
    BOF3_INJECT(MagicFx_CountDownFlag10);
    BOF3_INJECT(MeteorStrikeRock_Task);
    BOF3_INJECT(MeteorStrikeRock_Run);
    BOF3_INJECT(MeteorStrikeRock_Launch);
    BOF3_INJECT(MeteorStrikeRock_Fall);
    BOF3_INJECT(MeteorStrikeRock_Shake);
    BOF3_INJECT(MeteorStrikeRock_Hide);
    BOF3_INJECT(MeteorStrikeRock_End);
    BOF3_INJECT(MeteorStrikeRock_DrawRing);
    BOF3_INJECT(MeteorStrikeRecord_Task);
    BOF3_INJECT(MeteorStrikeChip_Run);
    BOF3_INJECT(MeteorStrikeChip_Launch);
    BOF3_INJECT(MeteorStrikeChip_Rise);
    BOF3_INJECT(MeteorStrikeChip_Fall);
    BOF3_INJECT(MeteorStrikeChip_Draw);
    BOF3_INJECT(MeteorStrikeTrail_Run);
    BOF3_INJECT(MeteorStrikeTrail_Start);
    BOF3_INJECT(MeteorStrikeTrail_Follow);
    BOF3_INJECT(MeteorStrikeTrail_Fade);
    BOF3_INJECT(MeteorStrikeTrail_Draw);
    BOF3_INJECT(MeteorStrike_RecordAlloc);
}
