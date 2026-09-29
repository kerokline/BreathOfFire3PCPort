// Two spell overlays compiled into the exe, round nine group S11
// (docs/magic_s11.md): the PSX's MAGIC058 and MAGIC059.EMI, Magic_Rows rows 82
// and 83. Read one id down (docs/cut-content.md section 2) the sibling labels
// them Sanctuary and Tornado; the names below use those labels as hypotheses,
// and say what the code does.
//
//   - MAGIC058 0x4AF040..0x4AFB76: one child (kind 1, 0x3F), a flat ring of 64
//     gouraud quads round the caster that widens; each live actor it reaches
//     (0x4F5970, MAGIC218's range test) but the acting one gets two motes from
//     the effect's own pool of 36 at 0x680780, which drop onto the actor and
//     draw a smaller ring of 16 quads as they fade; then a battle message
//     (Msg_SystemPtr 0x14) and the end;
//   - MAGIC059 0x4AFB80..0x4B0D42: a column over the caster - one child (kind
//     1, 0x40) drawing sixteen bands of a turning funnel, 24 motes from the
//     effect's own pool of 24 at 0x681A20 in three rings, each an orbiting
//     textured quad - while the effect's task circles about a point beside the
//     caster at the height of the opposite side's average.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase / a .data
// table read in place), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies. Calls into functions other groups own go by
// name (MAGIC218's AuraBreath_InReach 0x4F5970, MAGIC219's
// MagicFx_FreeCurrentRecord 0x4F6290), the engine's 0x446770 by raw address.
//
// No divergence: each is a faithful replacement, except that a phase past a
// stack table aborts where the original would call through its own stack
// (docs/magic_fx_reached.md section 3, the precedent), and
// Tornado_AverageHeight aborts where the original divides by a count of 0
// live actors (the owner's call, round9 doc section 6). The .data tables are
// read in place, their indexes unchecked, as the originals read them.
#include "game/magic_s11.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words by function)
// and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are
// read again after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kActorRecord = 0x904B3C;   // unsigned char *: the acting actor's sprite record
constexpr std::uint32_t kFlagsB = 0x904AA9;        // u8: the battle flags' second byte (Tornado sets bit 1)
constexpr std::uint32_t kFacingPhase = 0x65AA70;   // Tornado_FacingPhase: a u8 in each of four dwords, by +8

// MAGIC058's mote pool (36 of 0x84 bytes, +0 bit 0 in use) and its reached
// bytes, one per battle index 0..10 (0xFF once the ring has sent motes).
constexpr std::uint32_t kSanctuaryPool = 0x680780;
constexpr unsigned kSanctuaryMotes = 36;
constexpr std::uint32_t kReached = 0x681A10;
// MAGIC059's (24 of 0x84 bytes).
constexpr std::uint32_t kTornadoPool = 0x681A20;
constexpr unsigned kTornadoMotes = 24;
constexpr std::uint32_t kPoolStride = 0x84;

// The .data dispatch tables, read in place.
constexpr std::uint32_t kRingTypes = 0x65AA68;      // SanctuaryRing_Types: 1 entry by +1
constexpr std::uint32_t kSMoteTypes = 0x65AA6C;     // SanctuaryMote_Types: 1 entry by +1
constexpr std::uint32_t kFunnelTypes = 0x65AA80;    // TornadoFunnel_Types: 1 entry by +1
constexpr std::uint32_t kFunnelPhases = 0x65AA84;   // TornadoFunnel_Phases: 4 entries by +2
constexpr std::uint32_t kTMoteTypes = 0x65AA94;     // TornadoMote_Types: 1 entry by +1
constexpr std::uint32_t kTMotePhases = 0x65AA98;    // TornadoMote_Phases: 4 entries by +2

// Callees other groups own: the engine's by address; S36's AuraBreath_InReach
// and S37's MagicFx_FreeCurrentRecord, ours now, by name.
constexpr std::uint32_t kTurnOffset = bof3::addr::Battle_TurnVectorC;   // engine, unnamed: a task's +0xC / +0x10 turned by its +8
using Fn0 = void (__cdecl*)();
using NearFn = int (__cdecl*)(unsigned char*);
using TaskFn = void (__cdecl*)(unsigned char*);
using BandFn = void (__cdecl*)(unsigned, unsigned);

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

// `imul` then `sar n`: the 32-bit product wraps, the shift is arithmetic.
int MulSar(int a, int b, unsigned n) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}
int Mul12(int a, int b) { return MulSar(a, b, 12); }
// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }
int Add(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + (slot & 0xFF) * at::kTaskStride); }
unsigned char* SMote(unsigned i) { return Mem(kSanctuaryPool + (i & 0xFF) * kPoolStride); }
unsigned char* TMote(unsigned i) { return Mem(kTornadoPool + (i & 0xFF) * kPoolStride); }
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
// The originals index the enemies by the battle index - 3.
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// A .data table's entry, read in place, the index unchecked (as the
// originals' `jmp / call [eax*4 + table]`).
magic_harness::Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, tpage, 0).
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }
// MapView_LinkPrimAt at Sprite_Current's position, read at the call.
void LinkAtCurrent(int dy, unsigned size) {
    const unsigned char* const s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), dy, size);
}

// Gte_RotTransPers4 with the ten arguments the originals push: the four
// SVECTORs of the vertex scratch, the primitive's four points, and the depth
// and flag cells of the caller's frame.
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S11_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))
void Rtp4(unsigned char* prim) {
    long p, flag;
    S11_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &flag, &p);
}

// The target's done flag, 0x904AA8 bit 2, and the task freed.
void MarkDoneAndFree() {
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// A pool run by the effect's own task (Sanctuary_Task, Tornado_Task): every
// live record (+0 bit 0) as Sprite_Current, 0x93B940 its +0x80, for the call;
// both put back to what they were before the loop after each.
void RunPool(std::uint32_t pool, unsigned count, std::uint32_t dispatch) {
    unsigned char* const saved = Sc();
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const r = Mem(pool + i * kPoolStride);
        if ((r[0] & 1) == 0) continue;
        const std::int32_t its = Long(r + 0x80);
        Sprite_Current = r;
        SetLong(Mem(at::kOwner), its);
        Call0(dispatch);
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = saved;
    }
}

// The first free record of a pool (+0 bit 0 clear) marked in use; its index
// in al, or 0xFF when all are.
unsigned char PoolAlloc(std::uint32_t pool, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const r = Mem(pool + i * kPoolStride);
        if ((r[0] & 1) == 0) {
            r[0] = static_cast<unsigned char>(r[0] | 1);
            return static_cast<unsigned char>(i);
        }
    }
    return 0xFF;
}

}  // namespace

// Every function here is called by Capcom's code through the jmp Inject
// writes at its address: extern "C" and no tail calls (a task's top frame).
#define S11_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC058 (row 82, Sanctuary read one id down)

// original 0x4AF040: the kind-2 task. Its phase +1 through a three-entry
// table on its stack - Sanctuary_Start, Sanctuary_Message, Sanctuary_End;
// 3..255 would call through the original's stack, ours aborts. Then every
// live mote of the pool 0x680780 run through SanctuaryMote_Dispatch.
S11_EXPORT void __cdecl Sanctuary_Task(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::Sanctuary_Start, bof3::addr::Sanctuary_Message,
                                                bof3::addr::Sanctuary_End};
    const unsigned phase = Sc()[1];
    if (phase >= 3) bof3::Fatal("Sanctuary_Task: phase %u, past the three-entry table", phase);
    magic_harness::Phase(kSteps[phase])();
    RunPool(kSanctuaryPool, kSanctuaryMotes, bof3::addr::SanctuaryMote_Dispatch);
}

// original 0x4AF0C0: the pool emptied (+0..+2 of all 36) and the eleven
// reached bytes cleared; the owner's facing +8 and position +0x34 / +0x38 /
// +0x3C to the task; +0xB 0; +1 on. The ring (kind 1, parameter 0x3F: this
// task its owner, +1 0), the count +0xB up; sound 0x100.
S11_EXPORT void __cdecl Sanctuary_Start(void) {
    for (unsigned i = 0; i < kSanctuaryMotes; ++i) {
        unsigned char* const m = SMote(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    SetLong(Mem(kReached), 0);
    SetLong(Mem(kReached + 4), 0);
    SetWord(Mem(kReached + 8), 0);
    Mem(kReached + 10)[0] = 0;
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    const unsigned slot = NewTask(0x3F);
    unsigned char* const s = Sc();
    unsigned char* const t = TaskSlot(slot);
    SetLong(t + 0x80, static_cast<std::int32_t>(Key(s)));
    t[1] = 0;
    Inc(s[0xB]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4AF190: once the count +0xB is 0 (the ring and its motes gone),
// the battle message Msg_SystemPtr(0x14) queued (BattleQueue_Push(1, 0x3C,
// text)) and +1 on.
S11_EXPORT void __cdecl Sanctuary_Message(void) {
    if (Sc()[0xB] != 0) return;
    const unsigned char* const text = MH_CALL(Msg_SystemPtr)(0x14);
    MH_CALL(BattleQueue_Push)(1, 0x3C, Key(text));
    Inc(Sc()[1]);
}

// original 0x4AF1C0: once the message window is down (0x939F60 0), the
// target's flag 0x40, the done flag, the task freed.
S11_EXPORT void __cdecl Sanctuary_End(void) {
    if (Mem(at::kMessageUp)[0] != 0) return;
    MarkDoneAndFree();
}

// original 0x4AF1F0: the ring (kind 1, parameter 0x3F): a jmp through
// SanctuaryRing_Types 0x65AA68 by +1, read in place (one entry).
S11_EXPORT void __cdecl SanctuaryRing_Dispatch(void) { Entry(kRingTypes, Sc()[1])(); }

// original 0x4AF210: its phase +2 through a three-entry table on its stack -
// SanctuaryRing_Start, SanctuaryRing_Spread, MagicFx_UncountAndFree; 3..255
// would call through the original's stack, ours aborts. Then while it lives
// (+0 not 0) the ring drawn under the actor matrix.
S11_EXPORT void __cdecl SanctuaryRing_Task(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::SanctuaryRing_Start, bof3::addr::SanctuaryRing_Spread,
                                                bof3::addr::MagicFx_UncountAndFree};
    const unsigned phase = Sc()[2];
    if (phase >= 3) bof3::Fatal("SanctuaryRing_Task: phase %u, past the three-entry table", phase);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::SanctuaryRing_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AF260: the owner's facing and position to the ring; its
// radius +0xC (dword) 8, +0xA 0x10; +2 on.
S11_EXPORT void __cdecl SanctuaryRing_Start(void) {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    SetLong(Sc() + 0xC, 8);
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

namespace {

// Two motes for the actor at battle index `actor` (its +0xB), delays +9 1 and
// 13, each from the pool (SanctuaryMote_Alloc, its answer unchecked), the
// owner its +0x80, +1 0; the owner's count +0xB up for each.
void SendMotes(unsigned char actor) {
    for (unsigned delay = 1; delay < 0x19; delay += 0xC) {
        const unsigned index = MH_CALL(SanctuaryMote_Alloc)() & 0xFFu;
        const std::int32_t owner = Long(Mem(at::kOwner));
        unsigned char* const m = SMote(index);
        SetLong(m + 0x80, owner);
        m[1] = 0;
        m[0xB] = actor;
        m[9] = static_cast<unsigned char>(delay);
        unsigned char* const o = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(owner)));
        Inc(o[0xB]);
    }
}

}  // namespace

// original 0x4AF2D0: the ring widens, +0xC (dword) += 0x20, its low word the
// reach (word 0x903850). Each actor - the enemies (battle index 3..10) first,
// then the party (0..2) - not yet reached (0x681A10 + index 0), not out
// (Battle_ActorIsOut), within reach of the ring (0x4F5970 on its record) and
// not the acting actor (byte 0x904B34) is marked reached (0xFF) and sent two
// motes. At +0xC past 0x600 (signed) +2 on.
S11_EXPORT void __cdecl SanctuaryRing_Spread(void) {
    SetLong(Sc() + 0xC, Add(Long(Sc() + 0xC), 0x20));
    SetSW(0, Word(Sc() + 0xC));
    for (unsigned i = 0; i < 8; ++i) {
        const auto actor = static_cast<unsigned char>(3 + i);
        unsigned char* const reached = Mem(kReached + actor);
        if (reached[0] != 0) continue;
        if ((MH_CALL(Battle_ActorIsOut)(actor) & 0xFF) != 0) continue;
        if (MH_AT(NearFn, bof3::addr::AuraBreath_InReach)(EnemyRecord(actor)) == 0) continue;
        if ((static_cast<std::uint32_t>(Long(Mem(at::kActor))) & 0xFF) == actor) continue;
        reached[0] = 0xFF;
        SendMotes(actor);
    }
    for (unsigned i = 0; i < 3; ++i) {
        const auto actor = static_cast<unsigned char>(i);
        unsigned char* const reached = Mem(kReached + actor);
        if (reached[0] != 0) continue;
        if ((MH_CALL(Battle_ActorIsOut)(actor) & 0xFF) != 0) continue;
        if (MH_AT(NearFn, bof3::addr::AuraBreath_InReach)(PartyRecord(i)) == 0) continue;
        if (Mem(at::kActor)[0] == actor) continue;
        reached[0] = 0xFF;
        SendMotes(actor);
    }
    if (Long(Sc() + 0xC) > 0x600) Inc(Sc()[2]);
}

// original 0x4AF490: the owner's count +0xB down and the task freed (a tail
// jmp). Entry 2 of SanctuaryRing_Task's table; ten files' tables hold it
// (magic_funcs.tsv), MAGIC006..143's among them.
S11_EXPORT void __cdecl MagicFx_UncountAndFree(void) {
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AF4A0: a mote: a jmp through SanctuaryMote_Types 0x65AA6C by
// +1, read in place (one entry).
S11_EXPORT void __cdecl SanctuaryMote_Dispatch(void) { Entry(kSMoteTypes, Sc()[1])(); }

// original 0x4AF4C0: its phase +2 through a three-entry table on its stack -
// SanctuaryMote_Wait, SanctuaryMote_Slow, SanctuaryMote_Fade; 3..255 would
// call through the original's stack, ours aborts. Then while it lives and has
// landed (+0, +2 not 0) its ring drawn under the actor matrix.
S11_EXPORT void __cdecl SanctuaryMote_Task(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::SanctuaryMote_Wait, bof3::addr::SanctuaryMote_Slow,
                                                bof3::addr::SanctuaryMote_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 3) bof3::Fatal("SanctuaryMote_Task: phase %u, past the three-entry table", phase);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::SanctuaryMote_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AF510: +9 down; at 0 the mote takes its actor's position
// (+0xB: the party record below 3, else the enemy's by index - 3, unchecked),
// its radius +0xC and step +0x18 (dwords) 0x10, +0xA 0x10, and +2 on by two
// (past SanctuaryMote_Slow, which nothing else reaches).
S11_EXPORT void __cdecl SanctuaryMote_Wait(void) {
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] != 0) return;
    const unsigned actor = s[0xB];
    const unsigned char* const r = actor < 3 ? PartyRecord(actor) : EnemyRecord(actor);
    SetLong(s + 0x34, Long(r + 0x34));
    SetLong(s + 0x38, Long(r + 0x38));
    SetLong(s + 0x3C, Long(r + 0x3C));
    SetLong(s + 0xC, 0x10);
    SetLong(s + 0x18, 0x10);
    s[0xA] = 0x10;
    Inc(s[2]);
    Inc(s[2]);
}

// original 0x4AF620: +0xC += +0x18, +0x18 down (dwords); at +0x18 0x10 +2 on.
// Unreached: SanctuaryMote_Wait moves +2 from 0 to 2.
S11_EXPORT void __cdecl SanctuaryMote_Slow(void) {
    unsigned char* const s = Sc();
    SetLong(s + 0xC, Add(Long(s + 0xC), Long(s + 0x18)));
    SetLong(s + 0x18, Add(Long(s + 0x18), -1));
    if (Long(s + 0x18) == 0x10) Inc(s[2]);
}

// original 0x4AF650: +0xC += +0x18; on odd frames (Frame_Counter bit 0)
// +0x18 and +0xA down; at +0xA 0 the owner's count +0xB down and MAGIC219's
// 0x4F6290 (the record freed: a tail jmp).
S11_EXPORT void __cdecl SanctuaryMote_Fade(void) {
    unsigned char* const s = Sc();
    SetLong(s + 0xC, Add(Long(s + 0xC), Long(s + 0x18)));
    if ((Frame_Counter & 1) == 0) return;
    SetLong(s + 0x18, Add(Long(s + 0x18), -1));
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::MagicFx_FreeCurrentRecord);
}

namespace {

// The ring draws' first edge: the outer point (radius word 0x903850) and
// the inner (0x903852) at angle 0, into vertices 1 and 3.
void RingFirstEdge() {
    int v = Sin(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Sin(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Cos(0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(2))));
}

// One quad of a ring at angle `a`: the last edge (vertices 1 and 3) becomes
// the first (0 and 2), the new edge at `a` the second; z 0 throughout.
void RingStep(int a) {
    SetVW(0, VW(8));
    SetVW(2, VW(0xA));
    SetVW(4, 0);
    int v = Sin(a);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(a);
    const std::uint16_t x3 = VW(0x18), y3 = VW(0x1A);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0xC, 0);
    SetVW(0x10, x3);
    SetVW(0x12, y3);
    SetVW(0x14, 0);
    v = Sin(a);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Cos(a);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(2))));
    SetVW(0x1C, 0);
}

void Shade(unsigned char* p, unsigned vertex, unsigned char c) {
    p[4 + vertex * 0x10] = c;
    p[5 + vertex * 0x10] = c;
    p[6 + vertex * 0x10] = c;
}

}  // namespace

// original 0x4AF6A0: the ring: a draw mode (tpage 0x35) committed to slot 5,
// then 64 semi-transparent gouraud quads round the ring's centre, angles 0x40
// apart, from radius +0xC (the inner edge, shade 1) to +0xC + 0x80 (the
// outer, shade 0x60), each projected, depth-cued and committed to slot 5;
// then a draw mode with tpage 0x15.
S11_EXPORT void __cdecl SanctuaryRing_Draw(void) {
    const unsigned char* const s = Sc();
    SetSW(0, Word(s + 0xC) + 0x80u);
    SetSW(2, Word(s + 0xC));
    RingFirstEdge();
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        RingStep(a);
        Shade(p, 0, 0x60);
        Shade(p, 1, 0x60);
        Shade(p, 2, 1);
        Shade(p, 3, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4AF8D0: a mote's ring: as SanctuaryRing_Draw's with 16 quads
// 0x100 apart, from radius +0xC to +0xC + 0x40, the outer edge's shade +0xA
// x 6 (word 0x903854, its low byte read at each use).
S11_EXPORT void __cdecl SanctuaryMote_Draw(void) {
    const unsigned char* const s = Sc();
    SetSW(4, s[0xA] * 6u);
    SetSW(0, Word(s + 0xC) + 0x40u);
    SetSW(2, Word(s + 0xC));
    RingFirstEdge();
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        RingStep(a);
        Shade(p, 0, SB(4));
        Shade(p, 1, SB(4));
        Shade(p, 2, 1);
        Shade(p, 3, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4AFB20: the first free mote of the pool 0x680780 marked in use;
// its index in al, or 0xFF when all 36 are (the upper bytes of eax garbage;
// its one caller keeps al, unchecked).
S11_EXPORT unsigned char __cdecl SanctuaryMote_Alloc(void) { return PoolAlloc(kSanctuaryPool, kSanctuaryMotes); }

// ===========================================================================
// MAGIC059 (row 83, Tornado read one id down)

// original 0x4AFB80: the kind-2 task. Its phase +1 through a two-entry table
// on its stack - Tornado_Start, Tornado_Spin; 2..255 would call through the
// original's stack, ours aborts. Its screen point, then every live mote of
// the pool 0x681A20 run through TornadoMote_Dispatch.
S11_EXPORT void __cdecl Tornado_Task(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::Tornado_Start, bof3::addr::Tornado_Spin};
    const unsigned phase = Sc()[1];
    if (phase >= 2) bof3::Fatal("Tornado_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kSteps[phase])();
    MH_CALL(BattleActor_UpdateScreenXY)();
    RunPool(kTornadoPool, kTornadoMotes, bof3::addr::TornadoMote_Dispatch);
}

namespace {

// Eight motes of one ring from the pool (TornadoMote_Alloc, unchecked): the
// task their owner, +1 0, +9 their angle (8 i + `angle`), +0xB `kind`, radius
// +0xC and height +0x14 (dwords); the task's count +0xB up for each.
void SpawnRing(unsigned angle, unsigned char kind, std::int32_t radius, std::int32_t height) {
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned index = MH_CALL(TornadoMote_Alloc)() & 0xFFu;
        unsigned char* const s = Sc();
        unsigned char* const m = TMote(index);
        SetLong(m + 0x80, static_cast<std::int32_t>(Key(s)));
        m[1] = 0;
        m[9] = static_cast<unsigned char>((i << 3) + angle);
        m[0xB] = kind;
        SetLong(m + 0xC, radius);
        SetLong(m + 0x14, height);
        Inc(s[0xB]);
    }
}

}  // namespace

// original 0x4AFC00: the pool emptied (+0..+2 of all 24); +0xB 0. The funnel
// (kind 1, parameter 0x40: this task its owner, +1 0), the count up; three
// rings of eight motes (angles 8 i, + 2, + 4; kinds 3, 2, 1; radius 0x80,
// 0xC0, 0x100; height 0x800000, 0x400000, 0). CLUT row 26's 16 cells from
// their sources with the semi-transparency bit, cell 0 without; battle flag
// 0x904AA9 bit 1; the strip dirty. The task takes the acting actor's facing
// (+8 of the record at 0x904B3C); its offset (0x40000, 0) turned by it
// (0x446770) and added to the field point (Field_Kind2X / Z) as its centre
// +0x18 / +0x1C; its position the centre plus sin / cos 0 << 6; its height
// Tornado_AverageHeight's; sound 0x100; +9 from Tornado_FacingPhase by the
// facing (unchecked); +1 on.
S11_EXPORT void __cdecl Tornado_Start(void) {
    for (unsigned i = 0; i < kTornadoMotes; ++i) {
        unsigned char* const m = TMote(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    Sc()[0xB] = 0;
    const unsigned slot = NewTask(0x40);
    {
        unsigned char* const s = Sc();
        unsigned char* const t = TaskSlot(slot);
        SetLong(t + 0x80, static_cast<std::int32_t>(Key(s)));
        t[1] = 0;
        Inc(s[0xB]);
    }
    SpawnRing(0, 3, 0x80, 0x800000);
    SpawnRing(2, 2, 0xC0, 0x400000);
    SpawnRing(4, 1, 0x100, 0);
    for (unsigned k = 0; k < 16; ++k) Gfx_ClutStrip[0x1A00 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x1A00 + k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = Gfx_ClutStripSource[0x1A00];
    Mem(kFlagsB)[0] = static_cast<unsigned char>(Mem(kFlagsB)[0] | 2);
    Gfx_ClutStripDirty = 1;
    Sc()[8] = Pointer(kActorRecord)[8];
    SetLong(Sc() + 0xC, 0x40000);
    SetLong(Sc() + 0x10, 0);
    MH_AT(TaskFn, kTurnOffset)(Sc());
    SetLong(Sc() + 0x18, Add(Long(Sc() + 0xC), static_cast<int>(Field_Kind2X)));
    SetLong(Sc() + 0x1C, Add(Long(Sc() + 0x10), static_cast<int>(Field_Kind2Z)));
    int v = Sin(0);
    SetLong(Sc() + 0x34, Add(Shl(v, 6), Long(Sc() + 0x18)));
    v = Cos(0);
    SetLong(Sc() + 0x38, Add(Shl(v, 6), Long(Sc() + 0x1C)));
    Call0(bof3::addr::Tornado_AverageHeight);
    MH_CALL(Sound_PlayById)(0x100);
    unsigned char* const s = Sc();
    s[9] = Mem(kFacingPhase + s[8] * 4u)[0];
    Inc(s[1]);
}

// original 0x4AFE70: +9 up; the angle +9 << 4 (word 0x90385E); the task
// circles its centre (+0x18 / +0x1C) at sin / cos << 6. At the count +0xB 0
// (the funnel and the motes gone) the target's flag 0x40, the done flag, the
// task freed.
S11_EXPORT void __cdecl Tornado_Spin(void) {
    Inc(Sc()[9]);
    const auto angle = static_cast<std::uint16_t>(Sc()[9] << 4);
    SetSW(0xE, angle);
    int v = Sin(static_cast<short>(angle));
    SetLong(Sc() + 0x34, Add(Shl(v, 6), Long(Sc() + 0x18)));
    v = Cos(SS(0xE));
    SetLong(Sc() + 0x38, Add(Shl(v, 6), Long(Sc() + 0x1C)));
    if (Sc()[0xB] != 0) return;
    MarkDoneAndFree();
}

// original 0x4AFF00: the funnel (kind 1, parameter 0x40): a jmp through
// TornadoFunnel_Types 0x65AA80 by +1, read in place (one entry).
S11_EXPORT void __cdecl TornadoFunnel_Dispatch(void) { Entry(kFunnelTypes, Sc()[1])(); }

// original 0x4AFF20: its phase +2 through TornadoFunnel_Phases 0x65AA84
// (four entries: TornadoFunnel_Reset, _Grow, TornadoFx_Hold and group S07's
// EnlightenRays_Fade), read in place, unchecked. Then while it lives and has
// begun (+0, +2 not 0) under its own matrix sixteen bands: eight of radius
// 0x20 at angles (8 k - +9) & 0x3F, eight of 0x50 at (2 (4 k + 2 - +9)) &
// 0x3F, k = 0..3, -4..-1; +9 read before each.
S11_EXPORT void __cdecl TornadoFunnel_Task(void) {
    Entry(kFunnelPhases, Sc()[2])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(bof3::addr::TornadoFunnel_PushMatrix);
    const auto band = MH_AT(BandFn, bof3::addr::TornadoFunnel_DrawBand);
    static constexpr int kInner[8] = {0, 8, 0x10, 0x18, -0x20, -0x18, -0x10, -8};
    for (const int k : kInner) band(static_cast<unsigned>(k - Sc()[9]) & 0x3F, 0x20);
    static constexpr int kOuter[8] = {2, 6, 0xA, 0xE, -0xE, -0xA, -6, -2};
    for (const int k : kOuter) band(static_cast<unsigned>((k - Sc()[9]) * 2) & 0x3F, 0x50);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4B0120: +9 and +0xA 0; +2 on.
S11_EXPORT void __cdecl TornadoFunnel_Reset(void) {
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4B0140: +9 up, +0xA up by two; at +0xA 0x20 +2 on.
S11_EXPORT void __cdecl TornadoFunnel_Grow(void) {
    unsigned char* const s = Sc();
    Inc(s[9]);
    s[0xA] = static_cast<unsigned char>(s[0xA] + 2);
    if (s[0xA] == 0x20) Inc(s[2]);
}

// original 0x4B0170: the funnel's matrix pushed: its position and height
// +0x3E the owner's; Camera_Matrix x no rotation, translated by RotTrans of
// (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height / 2)). One MATRIX block as the
// original lays it out on its stack, RotTrans writing its translation at
// +0x14.
S11_EXPORT void __cdecl TornadoFunnel_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short rot[4] = {0, 0, 0, 0};
    unsigned char* const s = Sc();
    const unsigned char* const o = Owner();
    SetLong(s + 0x34, Long(o + 0x34));
    SetLong(s + 0x38, Long(o + 0x38));
    SetWord(s + 0x3E, Word(o + 0x3E));
    short v[4];
    v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(s + 0x3E) / 2));
    v[3] = 0;
    struct Matrix {
        short m[10];
        long t[3];
    } m;
    static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S11_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

namespace {

// `shl 7` then `sar 0xC` of a cosine: the funnel's bob.
int Bob(unsigned frame_angle) { return Shl(Cos(static_cast<int>((frame_angle & 0x3F) << 6)), 7) >> 12; }

}  // namespace

// original 0x4B0250 (angle, radius): one band of the funnel. A draw mode
// (tpage 0x35) sorted at the funnel. Three rings of points round the axis at
// the band's angle - radius + 0x32 (word 0x903850), + 0x1A (0x903852) and the
// radius (0x903854), at heights 0xFF40, 0xFFA0 and 0 (0x903856..0x90385A),
// each point's y bobbed by a cosine of the frame. Then 15 steps, the angle one
// on, the radii 0xD out and the heights 0x40 up each: two semi-transparent
// gouraud quads a step - the first two rings' strip, then the last two's,
// shaded 1 and +0xA x 3 (Scratch_Swap's low byte, read at each use) - each
// projected, depth-cued and sorted at the funnel with size 0x44. The loop's
// carried points live in the original's registers and frame; the vertex
// scratch is read back where the original reads it.
S11_EXPORT void __cdecl TornadoFunnel_DrawBand(unsigned angle, unsigned radius) {
    DrawMode(0x35);
    LinkAtCurrent(2, 0xC);
    SetSW(0xC, Sc()[0xA] * 3u);
    SetSW(0xE, (angle & 0x3F) << 6);
    SetSW(4, radius);
    SetSW(0, radius + 0x32);
    SetSW(2, radius + 0x1A);
    std::uint32_t frame = Frame_Counter;
    SetSW(6, 0xFF40);
    SetSW(8, 0xFFA0);
    SetSW(0xA, 0);
    // the first points of the three rings
    int bob = Bob(frame + 4);
    int v = Sin(SS(0xE));
    std::uint16_t ax = static_cast<std::uint16_t>(Mul12(v, SS(0)));
    v = Cos(SS(0xE));
    std::uint16_t ay = static_cast<std::uint16_t>(Add(Mul12(v, SS(0)), bob));
    frame = Frame_Counter;
    std::uint16_t az = SW(6);
    bob = Bob(frame + 2);
    v = Sin(SS(0xE));
    std::uint16_t bx = static_cast<std::uint16_t>(Mul12(v, SS(2)));
    v = Cos(SS(0xE));
    std::uint16_t by = static_cast<std::uint16_t>(Add(Mul12(v, SS(2)), bob));
    frame = Frame_Counter;
    std::uint16_t bz = SW(8);
    bob = Bob(frame);
    v = Sin(SS(0xE));
    std::uint16_t cx = static_cast<std::uint16_t>(Mul12(v, SS(4)));
    v = Cos(SS(0xE));
    std::uint16_t cy = static_cast<std::uint16_t>(Add(Mul12(v, SS(4)), bob));
    std::uint16_t cz = SW(0xA);
    for (unsigned i = 1; i < 0x10; ++i) {
        // the first quad: ring A's last point and next, ring B's
        SetVW(0x10, bx);
        SetVW(4, az);
        frame = Frame_Counter;
        SetVW(0, ax);
        SetSW(0xE, ((angle + i) & 0x3F) << 6);
        SetSW(0, SW(0) + 0xDu);
        SetSW(2, SW(2) + 0xDu);
        SetSW(4, SW(4) + 0xDu);
        SetSW(6, SW(6) - 0x40u);
        SetSW(8, SW(8) - 0x40u);
        SetSW(0xA, SW(0xA) - 0x40u);
        SetVW(2, ay);
        SetVW(0x12, by);
        SetVW(0x14, bz);
        bob = Bob(frame + i + 4);
        v = Sin(SS(0xE));
        ax = static_cast<std::uint16_t>(Mul12(v, SS(0)));
        SetVW(8, ax);
        v = Cos(SS(0xE));
        ay = static_cast<std::uint16_t>(Add(Mul12(v, SS(0)), bob));
        frame = Frame_Counter;
        az = SW(6);
        SetVW(0xC, az);
        SetVW(0xA, ay);
        bob = Bob(frame + i + 2);
        v = Sin(SS(0xE));
        bx = static_cast<std::uint16_t>(Mul12(v, SS(2)));
        SetVW(0x18, bx);
        v = Cos(SS(0xE));
        by = static_cast<std::uint16_t>(Add(Mul12(v, SS(2)), bob));
        bz = SW(8);
        SetVW(0x1A, by);
        SetVW(0x1C, bz);
        unsigned char* p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        Shade(p, 0, 1);
        Shade(p, 1, 1);
        Shade(p, 2, SB(0xC));
        Shade(p, 3, SB(0xC));
        LinkAtCurrent(2, 0x44);
        // the second: ring B's two points (from the scratch), ring C's
        SetVW(0, VW(0x10));
        SetVW(2, VW(0x12));
        SetVW(8, VW(0x18));
        SetVW(4, VW(0x14));
        SetVW(0xA, VW(0x1A));
        SetVW(0xC, VW(0x1C));
        SetVW(0x10, cx);
        SetVW(0x12, cy);
        SetVW(0x14, cz);
        frame = Frame_Counter;
        bob = Bob(frame + i);
        v = Sin(SS(0xE));
        cx = static_cast<std::uint16_t>(Mul12(v, SS(4)));
        SetVW(0x18, cx);
        v = Cos(SS(0xE));
        cy = static_cast<std::uint16_t>(Add(Mul12(v, SS(4)), bob));
        p = Gfx_PacketNext;
        SetVW(0x1A, cy);
        cz = SW(0xA);
        SetVW(0x1C, cz);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        Shade(p, 0, SB(0xC));
        Shade(p, 1, SB(0xC));
        Shade(p, 2, 1);
        Shade(p, 3, 1);
        LinkAtCurrent(2, 0x44);
    }
}

// original 0x4B0800: a mote: a jmp through TornadoMote_Types 0x65AA94 by +1,
// read in place (one entry).
S11_EXPORT void __cdecl TornadoMote_Dispatch(void) { Entry(kTMoteTypes, Sc()[1])(); }

// original 0x4B0820: its phase +2 through TornadoMote_Phases 0x65AA98 (four
// entries: TornadoMote_Place, _Rise, TornadoFx_Hold, TornadoMote_Fade), read
// in place, unchecked. Then while it lives and has begun (+0, +2 not 0): its
// orbit angle (+9 x 3) & 0x3F << 6 (word 0x90385E), its radius +0xC plus a
// wobble sin(frame & 0xF) << 4 >> 12 (word 0x903850); its position the
// owner's plus cos / sin x radius >> 3, its y then bobbed by a cosine of the
// frame (the cell taken before the call, as the original takes it); its
// screen point, its quad.
S11_EXPORT void __cdecl TornadoMote_Task(void) {
    Entry(kTMotePhases, Sc()[2])();
    const unsigned char* const s0 = Sc();
    if (s0[0] == 0 || s0[2] == 0) return;
    SetSW(0xE, ((s0[9] * 3u) & 0x3F) << 6);
    int v = Sin(static_cast<int>((Frame_Counter & 0xF) << 6));
    SetSW(0, static_cast<unsigned>(Shl(v, 4) >> 12) + Word(Sc() + 0xC));
    v = Cos(SS(0xE));
    {
        const int r = SS(0);
        const std::int32_t ox = Long(Owner() + 0x34);
        SetLong(Sc() + 0x34, Add(MulSar(v, r, 3), ox));
    }
    v = Sin(SS(0xE));
    {
        const int r = SS(0);
        const std::int32_t oz = Long(Owner() + 0x38);
        SetLong(Sc() + 0x38, Add(MulSar(v, r, 3), oz));
    }
    const std::uint32_t frame = Frame_Counter;
    unsigned char* const cell = Sc() + 0x38;
    v = Cos(static_cast<int>((frame & 0x3F) << 6));
    SetLong(cell, Add(Long(cell), Shl(v, 7) >> 3));
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::TornadoMote_Draw);
}

// original 0x4B0920: the mote takes the owner's position, its y +0x3C the
// owner's plus its height +0x14; +0xA 0; +2 on.
S11_EXPORT void __cdecl TornadoMote_Place(void) {
    unsigned char* const s = Sc();
    const unsigned char* const o = Owner();
    SetLong(s + 0x34, Long(o + 0x34));
    SetLong(s + 0x38, Long(o + 0x38));
    SetLong(s + 0x3C, Add(Long(o + 0x3C), Long(s + 0x14)));
    s[0xA] = 0;
    Inc(s[2]);
}

// original 0x4B0970: +9 and +0xA up; at +0xA 0x20 +2 on.
S11_EXPORT void __cdecl TornadoMote_Rise(void) {
    unsigned char* const s = Sc();
    Inc(s[9]);
    Inc(s[0xA]);
    if (s[0xA] == 0x20) Inc(s[2]);
}

// original 0x4B09A0: +9 up; at 0xA0 +2 on. Entry 2 of both
// TornadoFunnel_Phases and TornadoMote_Phases.
S11_EXPORT void __cdecl TornadoFx_Hold(void) {
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (s[9] == 0xA0) Inc(s[2]);
}

// original 0x4B09C0: +9 up, +0xA down; at +0xA 0 the owner's count +0xB down
// and MAGIC219's 0x4F6290 (the record freed: a tail jmp).
S11_EXPORT void __cdecl TornadoMote_Fade(void) {
    unsigned char* const s = Sc();
    Inc(s[9]);
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::MagicFx_FreeCurrentRecord);
}

// original 0x4B0A00: a mote's quad: a draw mode (tpage 0x35) sorted at the
// mote, then a semi-transparent textured quad round its screen point (+0x2E,
// +0x30): corners at angles 0xD00, 0xA00, 0x200, 0x600 of radius 0x20 (word
// 0x903850, read at each use); tpage (0, 1, 0x340, 0x100), clut (0, 0x1FA),
// texture corners (0, 0), (0x1F, 0), (0, 0x20), (0x1F, 0x20); its shade +0xB x
// +0xA (word Scratch_Swap, its low byte); sorted at the mote, size 0x48.
S11_EXPORT void __cdecl TornadoMote_Draw(void) {
    DrawMode(0x35);
    LinkAtCurrent(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetSW(0, 0x20);
    static constexpr int kCorners[4] = {0xD00, 0xA00, 0x200, 0x600};
    for (unsigned k = 0; k < 4; ++k) {
        int v = Cos(kCorners[k]);
        PutFloat(p + 8 + k * 0x10, Add(Mul12(v, SS(0)), S16(Sc() + 0x2E)));
        v = Sin(kCorners[k]);
        PutFloat(p + 0xC + k * 0x10, Add(Mul12(v, SS(0)), S16(Sc() + 0x30)));
    }
    SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100) & 0xFFFF);
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA) & 0xFFFF);
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x24] = 0x1F;
    p[0x25] = 0;
    p[0x34] = 0;
    p[0x35] = 0x20;
    p[0x44] = 0x1F;
    p[0x45] = 0x20;
    const unsigned char* const s = Sc();
    SetSW(0xC, static_cast<unsigned>(s[0xB]) * s[0xA]);
    p[4] = SB(0xC);
    p[5] = SB(0xC);
    p[6] = SB(0xC);
    LinkAtCurrent(2, 0x48);
}

// original 0x4B0C50: the first free mote of the pool 0x681A20 marked in use;
// its index in al, or 0xFF when all 24 are (its one caller keeps al,
// unchecked).
S11_EXPORT unsigned char __cdecl TornadoMote_Alloc(void) { return PoolAlloc(kTornadoPool, kTornadoMotes); }

// original 0x4B0CB0: the task's height +0x3E the average (idiv, toward zero)
// of the heights +0x3E of the live actors (Battle_ActorIsOut) on the side
// opposite the acting actor's: the eight enemies when it is a party member
// (byte 0x904B34 below 3), else the three members. Where none is live the
// original divides by zero; ours aborts (round9 doc section 6).
S11_EXPORT void __cdecl Tornado_AverageHeight(void) {
    int sum = 0;
    int count = 0;
    if (Mem(at::kActor)[0] < 3) {
        for (unsigned i = 0; i < 8; ++i) {
            if ((MH_CALL(Battle_ActorIsOut)(3 + i) & 0xFF) != 0) continue;
            sum = Add(sum, S16(EnemyRecord(3 + i) + 0x3E));
            ++count;
        }
    } else {
        for (unsigned i = 0; i < 3; ++i) {
            if ((MH_CALL(Battle_ActorIsOut)(i) & 0xFF) != 0) continue;
            sum = Add(sum, S16(PartyRecord(i) + 0x3E));
            ++count;
        }
    }
    unsigned char* const s = Sc();
    if (count == 0) bof3::Fatal("Tornado_AverageHeight: no live actor on the opposite side (the original divides by zero)");
    SetWord(s + 0x3E, static_cast<unsigned>(sum / count) & 0xFFFF);
}

#undef S11_EXPORT

void MagicS11_Inject() {
    if (bof3::WantsShadow("magic_s11")) magic_s11::SelfTest();
    BOF3_INJECT(Sanctuary_Task);
    BOF3_INJECT(Sanctuary_Start);
    BOF3_INJECT(Sanctuary_Message);
    BOF3_INJECT(Sanctuary_End);
    BOF3_INJECT(SanctuaryRing_Dispatch);
    BOF3_INJECT(SanctuaryRing_Task);
    BOF3_INJECT(SanctuaryRing_Start);
    BOF3_INJECT(SanctuaryRing_Spread);
    BOF3_INJECT(MagicFx_UncountAndFree);
    BOF3_INJECT(SanctuaryMote_Dispatch);
    BOF3_INJECT(SanctuaryMote_Task);
    BOF3_INJECT(SanctuaryMote_Wait);
    BOF3_INJECT(SanctuaryMote_Slow);
    BOF3_INJECT(SanctuaryMote_Fade);
    BOF3_INJECT(SanctuaryRing_Draw);
    BOF3_INJECT(SanctuaryMote_Draw);
    BOF3_INJECT(SanctuaryMote_Alloc);
    BOF3_INJECT(Tornado_Task);
    BOF3_INJECT(Tornado_Start);
    BOF3_INJECT(Tornado_Spin);
    BOF3_INJECT(TornadoFunnel_Dispatch);
    BOF3_INJECT(TornadoFunnel_Task);
    BOF3_INJECT(TornadoFunnel_Reset);
    BOF3_INJECT(TornadoFunnel_Grow);
    BOF3_INJECT(TornadoFunnel_PushMatrix);
    BOF3_INJECT(TornadoFunnel_DrawBand);
    BOF3_INJECT(TornadoMote_Dispatch);
    BOF3_INJECT(TornadoMote_Task);
    BOF3_INJECT(TornadoMote_Place);
    BOF3_INJECT(TornadoMote_Rise);
    BOF3_INJECT(TornadoFx_Hold);
    BOF3_INJECT(TornadoMote_Fade);
    BOF3_INJECT(TornadoMote_Draw);
    BOF3_INJECT(TornadoMote_Alloc);
    BOF3_INJECT(Tornado_AverageHeight);
}
