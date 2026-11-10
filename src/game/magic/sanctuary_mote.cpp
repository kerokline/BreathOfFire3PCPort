// Moved here by tools/move_funcs.py from magic_s11.cpp (docs/source-layout-round.md).
#include "game/magic/sanctuary_mote.h"

#include <cstdint>
#include <cstring>

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

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words by function)
// and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are
// read again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// MAGIC058's mote pool (36 of 0x84 bytes, +0 bit 0 in use) and its reached
// bytes, one per battle index 0..10 (0xFF once the ring has sent motes).
// kSanctuaryPool: now SanctuaryMote_Pool (symbols.toml).
constexpr unsigned kSanctuaryMotes = 36;

constexpr std::uint32_t kPoolStride = 0x84;

using Fn0 = void (__cdecl*)();

using NearFn = int (__cdecl*)(unsigned char*);

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

// `imul` then `sar n`: the 32-bit product wraps, the shift is arithmetic.
int MulSar(int a, int b, unsigned n) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}

int Mul12(int a, int b) { return MulSar(a, b, 12); }

int Add(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFF) * at::kTaskStride); }

unsigned char* SMote(unsigned i) { return Mem(::bof3::Img(SanctuaryMote_Pool_addr) + (i & 0xFF) * kPoolStride); }

unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

// The originals index the enemies by the battle index - 3.
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// A .data table's entry, read in place, the index unchecked (as the
// originals' `jmp / call [eax*4 + table]`).
magic_harness::Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, tpage, 0).
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

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
    Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// A pool run by the effect's own task (Sanctuary_Task, Tornado_Task): every
// live record (+0 bit 0) as Sprite_Current, 0x93B940 its +0x80, for the call;
// both put back to what they were before the loop after each.
void RunPool(std::uint32_t pool, unsigned count, std::uint32_t dispatch) {
    unsigned char* const saved = Sc();
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const r = Mem(pool + i * kPoolStride);
        if ((r[0] & 1) == 0) continue;
        const std::int32_t its = Long(r + 0x80);
        Sprite_Current = r;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        Call0(dispatch);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
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

namespace {

// Two motes for the actor at battle index `actor` (its +0xB), delays +9 1 and
// 13, each from the pool (SanctuaryMote_Alloc, its answer unchecked), the
// owner its +0x80, +1 0; the owner's count +0xB up for each.
void SendMotes(unsigned char actor) {
    for (unsigned delay = 1; delay < 0x19; delay += 0xC) {
        const unsigned index = MH_CALL(SanctuaryMote_Alloc)() & 0xFFu;
        const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
        unsigned char* const m = SMote(index);
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        tm->owner = owner;
        m[1] = 0;
        m[0xB] = actor;
        m[9] = static_cast<unsigned char>(delay);
        unsigned char* const o = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(owner)));
        Inc(o[0xB]);
    }
}

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
    RunPool(::bof3::Img(SanctuaryMote_Pool_addr), kSanctuaryMotes, bof3::addr::SanctuaryMote_Dispatch);
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
    SetLong(Mem(::bof3::Img(Sanctuary_Reached_addr)), 0);
    SetLong(Mem(::bof3::Img(Sanctuary_Reached_addr) + 4), 0);
    SetWord(Mem(::bof3::Img(Sanctuary_Reached_addr) + 8), 0);
    Mem(::bof3::Img(Sanctuary_Reached_addr) + 10)[0] = 0;
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    const unsigned slot = NewTask(0x3F);
    unsigned char* const s = Sc();
    unsigned char* const t = TaskSlot(slot);
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    tt->owner = static_cast<std::int32_t>(Key(s));
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
    if (Mem(::bof3::Img(BattleE1_MessageUp_addr))[0] != 0) return;
    MarkDoneAndFree();
}

// original 0x4AF1F0: the ring (kind 1, parameter 0x3F): a jmp through
// SanctuaryRing_Types 0x65AA68 by +1, read in place (one entry).
S11_EXPORT void __cdecl SanctuaryRing_Dispatch(void) { Entry(::bof3::Img(SanctuaryRing_Types_addr), Sc()[1])(); }

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
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    rv::Current<records::BattleTask>()->step_x = 8;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4AF2D0: the ring widens, +0xC (dword) += 0x20, its low word the
// reach (word 0x903850). Each actor - the enemies (battle index 3..10) first,
// then the party (0..2) - not yet reached (0x681A10 + index 0), not out
// (Battle_ActorIsOut), within reach of the ring (0x4F5970 on its record) and
// not the acting actor (byte 0x904B34) is marked reached (0xFF) and sent two
// motes. At +0xC past 0x600 (signed) +2 on.
S11_EXPORT void __cdecl SanctuaryRing_Spread(void) {
    rv::Store(rv::Current<records::BattleTask>()->step_x, Add(rv::Current<records::BattleTask>()->step_x, 0x20));
    SetSW(0, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_x));
    for (unsigned i = 0; i < 8; ++i) {
        const auto actor = static_cast<unsigned char>(3 + i);
        unsigned char* const reached = Mem(::bof3::Img(Sanctuary_Reached_addr) + actor);
        if (reached[0] != 0) continue;
        if ((MH_CALL(Battle_ActorIsOut)(actor) & 0xFF) != 0) continue;
        if (MH_AT(NearFn, bof3::addr::AuraBreath_InReach)(EnemyRecord(actor)) == 0) continue;
        if ((static_cast<std::uint32_t>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFF) == actor) continue;
        reached[0] = 0xFF;
        SendMotes(actor);
    }
    for (unsigned i = 0; i < 3; ++i) {
        const auto actor = static_cast<unsigned char>(i);
        unsigned char* const reached = Mem(::bof3::Img(Sanctuary_Reached_addr) + actor);
        if (reached[0] != 0) continue;
        if ((MH_CALL(Battle_ActorIsOut)(actor) & 0xFF) != 0) continue;
        if (MH_AT(NearFn, bof3::addr::AuraBreath_InReach)(PartyRecord(i)) == 0) continue;
        if (Mem(::bof3::Img(at::kActor))[0] == actor) continue;
        reached[0] = 0xFF;
        SendMotes(actor);
    }
    if (rv::Current<records::BattleTask>()->step_x > 0x600) Inc(Sc()[2]);
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
S11_EXPORT void __cdecl SanctuaryMote_Dispatch(void) { Entry(::bof3::Img(SanctuaryMote_Types_addr), Sc()[1])(); }

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
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    Dec(s[9]);
    if (s[9] != 0) return;
    const unsigned actor = s[0xB];
    const unsigned char* const r = actor < 3 ? PartyRecord(actor) : EnemyRecord(actor);
    ts->x = Long(r + 0x34);
    ts->z = Long(r + 0x38);
    rv::Store(ts->height, Long(r + 0x3C));
    ts->step_x = 0x10;
    ts->work_18 = 0x10;
    s[0xA] = 0x10;
    Inc(s[2]);
    Inc(s[2]);
}

// original 0x4AF620: +0xC += +0x18, +0x18 down (dwords); at +0x18 0x10 +2 on.
// Unreached: SanctuaryMote_Wait moves +2 from 0 to 2.
S11_EXPORT void __cdecl SanctuaryMote_Slow(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->step_x = Add(ts->step_x, ts->work_18);
    ts->work_18 = Add(ts->work_18, -1);
    if (ts->work_18 == 0x10) Inc(s[2]);
}

// original 0x4AF650: +0xC += +0x18; on odd frames (Frame_Counter bit 0)
// +0x18 and +0xA down; at +0xA 0 the owner's count +0xB down and MAGIC219's
// 0x4F6290 (the record freed: a tail jmp).
S11_EXPORT void __cdecl SanctuaryMote_Fade(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->step_x = Add(ts->step_x, ts->work_18);
    if ((Frame_Counter & 1) == 0) return;
    ts->work_18 = Add(ts->work_18, -1);
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::MagicFx_FreeCurrentRecord);
}

// original 0x4AF6A0: the ring: a draw mode (tpage 0x35) committed to slot 5,
// then 64 semi-transparent gouraud quads round the ring's centre, angles 0x40
// apart, from radius +0xC (the inner edge, shade 1) to +0xC + 0x80 (the
// outer, shade 0x60), each projected, depth-cued and committed to slot 5;
// then a draw mode with tpage 0x15.
S11_EXPORT void __cdecl SanctuaryRing_Draw(void) {
    const unsigned char* const s = Sc();
    SetSW(0, Word(s + 0xC) + 0x80u);
    SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_x));
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
    SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_x));
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
S11_EXPORT unsigned char __cdecl SanctuaryMote_Alloc(void) { return PoolAlloc(::bof3::Img(SanctuaryMote_Pool_addr), kSanctuaryMotes); }

void SanctuaryMote_Inject() {
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
}
