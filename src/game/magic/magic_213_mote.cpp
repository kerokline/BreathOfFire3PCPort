// Moved here by tools/move_funcs.py from magic_c1.cpp (docs/source-layout-round.md).
#include "game/magic/magic_213_mote.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_c1.h"
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
// sixteen bytes (0x903850..0x90385F) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF), read again after every call as the
// originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t Addr(const void* p) { return Low32(p); }

// The actor record by battle index as the originals compute it - an enemy
// (i - 3) * 0x128 from 0x93B960 when the target byte has 0x40, else a party
// member i * 0x14C from 0x802D40 - unchecked, 32-bit arithmetic.
unsigned char* ActorRecord(bool enemies, unsigned i) {
    return enemies ? Mem(::bof3::Img(Battle_Enemies_addr) + (i - 3u) * at::kEnemyStride) : Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions and the other units' called by address, as the
// originals call them: in the game Capcom's code or the jmp Inject put there,
// in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using ByteFn = unsigned char (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// A .data table's call, `call` / `jmp [table + phase * 4]`, read in place (the
// fuzz swaps the cells for recorders); the table's own entries only - what
// follows is the next table or data.
void CellCall(std::uint32_t table, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * phase)))))();
}

// Gfx_ClutStrip entries first..last - 1 from Gfx_ClutStripSource, with or
// without the STP bit: row 26's first CLUT (0x1A00..0x1A0F, MAGIC113 and 213)
// or entries 1..15 of its third (0x1A21..0x1A2F, MAGIC145 and 146, which then
// clear 0x1A20).
void ClutCopy(unsigned first, unsigned last, std::uint16_t stp) {
    for (unsigned k = first; k < last; ++k)
        Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | stp);
}

}  // namespace

#define C1_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// A pool of 64 records of 0x84 bytes (MAGIC146's, MAGIC213's), run as tasks:
// every record with bit 0 becomes Sprite_Current, its +0x80 the owner, for
// `run`; both put back after each. The task and owner are read after the
// phase call that precedes the walk.
void WalkPool(std::uint32_t pool, std::uint32_t run) {
    unsigned char* const self = Sprite_Current;
    const std::uint32_t owner = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(BattleE2_Owner_addr))));
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), rec_owner);
        Call0(run);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), static_cast<std::int32_t>(owner));
        Sprite_Current = self;
    }
}

void ClearPool(std::uint32_t pool) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
}

// A child task (kind 1, `parameter`) on every actor of the target's side that
// Battle_ActorIsOut does not answer for - the enemies 3..10 when the target
// byte has 0x40, else the party 0..2: its owner this task, +1 0, +4 the actor,
// +9 a delay of the count so far x 16 + 1 (and +0xB the count, `index`), the
// count +0xB up. Sprite_Current is read after each create.
void SpawnOnActors(unsigned parameter, bool index) {
    const bool enemies = (TargetByte() & 0x40) != 0;
    const unsigned first = enemies ? 3 : 0, count = enemies ? 8 : 3;
    for (unsigned a = first; a < first + count; ++a) {
        if (MH_CALL(Battle_ActorIsOut)(a) != 0) continue;
        const unsigned slot = NewTask(parameter);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Addr(self));
        child[1] = 0;
        child[4] = static_cast<unsigned char>(a);
        if (index) child[0xB] = self[0xB];
        child[9] = static_cast<unsigned char>((self[0xB] << 4) + 1);
        Inc(self[0xB]);
    }
}

// Records from a pool's alloc (`alloc`: an index in al, 0xFF when full), one
// per delay first, first + step, .. below `end`: owner this task, +1 0, +0xB
// their number, +9 the delay; counted in this task's +0xB. A full pool skips
// the record, not the number.
void SpawnRecords(std::uint32_t alloc, std::uint32_t pool, unsigned first, unsigned step, unsigned end) {
    for (unsigned i = 0, delay = first; delay < end; ++i, delay += step) {
        const unsigned idx = MH_WORD(ByteFn, alloc)();
        if (idx == 0xFF) continue;
        unsigned char* const rec = Mem(pool + idx * 0x84u);
        unsigned char* const self = Sc();
        SetLong(rec + 0x80, static_cast<std::int32_t>(Addr(self)));
        rec[1] = 0;
        rec[0xB] = static_cast<unsigned char>(i);
        rec[9] = static_cast<unsigned char>(delay);
        Inc(self[0xB]);
    }
}

// A pool's alloc: the first of 64 records with bit 0 clear gets it, its index
// in al; 0xFF when all are taken.
unsigned char PoolAlloc(std::uint32_t pool) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = Mem(pool + i * 0x84u);
        if (rec[0] & 1) continue;
        rec[0] |= 1;
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

}  // namespace

// original 0x4F4A60: the kind-2 task. A two-entry stack table by +1 -
// Magic213_Start, MAGIC131's end once +0xB is 0 (0x4E5200) - then the mote
// pool walked (Magic213Mote_Dispatch).
C1_EXPORT void __cdecl Magic213_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Magic213_Start, bof3::addr::MagicFx_EndWhenChildrenDone};
    StackCall(kPhases, 2, Sc()[1], "Magic213_Task");
    WalkPool(::bof3::Img(Magic213Mote_Pool_addr), bof3::addr::Magic213Mote_Dispatch);
}

// original 0x4F4AE0: the mote pool cleared; +0xB 0, +1 on; a child (kind 1,
// parameter 0x6B) on each living actor of the target's side (+0xB not
// written); row 26's first sixteen CLUT entries from their source, as they
// are; Gfx_ClutStripDirty; sound 0x100.
C1_EXPORT void __cdecl Magic213_Start(void) {
    ClearPool(::bof3::Img(Magic213Mote_Pool_addr));
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    SpawnOnActors(0x6B, false);
    ClutCopy(0x1A00, 0x1A10, 0);
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4F4C40: the actor child's kind-1 task, a jmp through
// Magic213Actor_Kinds (one entry) by +1.
C1_EXPORT void __cdecl Magic213Actor_Task(void) {
    CellCall(Addr(Magic213Actor_Kinds), 1, Sc()[1], "Magic213Actor_Task");
}

// original 0x4F4C60: a jmp through Magic213Actor_Phases (four entries:
// Start, ActorFx_TintUp, Magic213Actor_Untint, MagicFx_EndWithChildren) by
// +2.
C1_EXPORT void __cdecl Magic213Actor_Run(void) {
    CellCall(Addr(Magic213Actor_Phases), 4, Sc()[2], "Magic213Actor_Run");
}

// original 0x4F4C80: +9 down; at 0: the actor's record (+4, by the target
// byte's side bit, unchecked); the task at its position; +0xB 0, +9 0x10,
// +0xA 0, +2 on; seven motes (+9 1, 5, .. 25) from the pool; the actor's
// tints released and a tint (0, 0, 0, 1) set on it, its record in +0xA.
C1_EXPORT void __cdecl Magic213Actor_Start(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    if (s[9] != 0) return;
    unsigned char* const rec = ActorRecord((TargetByte() & 0x40) != 0, s[4]);
    ts->x = rv::As<records::SpriteHead>(rec)->x;
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(rec)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(rec)->height);
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
    SpawnRecords(bof3::addr::Magic213Mote_Alloc, ::bof3::Img(Magic213Mote_Pool_addr), 1, 4, 0x1D);
    MH_CALL(Sprite_ReleaseTint)(rec);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(rec, 0, 0, 0, 1);
    Sc()[0xA] = tint;
}

// original 0x4F4DA0 (MAGIC213's; also entry 2 of MAGIC073's and 074's child
// tables): the tint record +0xA's colour bytes up by one; +9 down, at 0 +2
// on.
C1_EXPORT void __cdecl ActorFx_TintUp(void) {
    unsigned char* const s = Sc();
    for (unsigned k = 2; k <= 4; ++k) Inc(MoveScript_TintRecords[s[0xA] * 12u + k]);
    Dec(s[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4F4E10: the tint record +0xA's colour bytes down by one; when
// its first is 0, the record released (Tint_Release), the actor's flag 0x40,
// +2 on.
C1_EXPORT void __cdecl Magic213Actor_Untint(void) {
    unsigned char* const s = Sc();
    for (unsigned k = 2; k <= 4; ++k) Dec(MoveScript_TintRecords[s[0xA] * 12u + k]);
    const unsigned char tint = s[0xA];
    if (MoveScript_TintRecords[tint * 12u + 2] != 0) return;
    MH_CALL(Tint_Release)(tint);
    MH_CALL(Battle_SetTargetFlag40)(Sc()[4]);
    Inc(Sc()[2]);
}

// original 0x4F4EA0: a mote record's dispatch, a jmp through
// Magic213Mote_Kinds (one entry) by +1.
C1_EXPORT void __cdecl Magic213Mote_Dispatch(void) {
    CellCall(Addr(Magic213Mote_Kinds), 1, Sc()[1], "Magic213Mote_Dispatch");
}

// original 0x4F4EC0: with the frame-offset table the effects' (0x8E3580), a
// call through Magic213Mote_Phases (two entries: Start, End) by +2; while +0
// and +2 are set, Sprite_UpdateScreen; the table back (0x8B3580).
C1_EXPORT void __cdecl Magic213Mote_Run(void) {
    const unsigned phase = Sc()[2];
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8E3580));
    CellCall(Addr(Magic213Mote_Phases), 2, phase, "Magic213Mote_Run");
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8B3580));
}

// original 0x4F4F00: +9 down; at 0 the mote on a circle of radius 12 round
// its owner at the angle (+0xB & 7) x 0x200, 0x180 above it (+0x3C's high
// word); its sprite fields set up (+0x25 0x1D, +0x26 0, +0x27 0xA0, +0x28 0,
// colour 0 with +0x5C 0, +0x2A 0, +0x29 and +0x24 4, +0x2C 0, +0x2B 1);
// animation +0xB % 3; +2 on.
C1_EXPORT void __cdecl Magic213Mote_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetSW(0, static_cast<unsigned>(Sc()[0xB] & 7) << 9);
    int v = MH_CALL(Math_Sin)(static_cast<short>(SW(0)));
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->x) +
                                                   static_cast<std::uint32_t>(v) * 12u));
    v = MH_CALL(Math_Cos)(static_cast<short>(SW(0)));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->z) +
                                                   static_cast<std::uint32_t>(v) * 12u));
    rv::Store(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height) + 0x1800000u));
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x27] = 0xA0;
    Sc()[0x28] = 0;
    Sc()[0x5D] = 0;
    Sc()[0x5E] = 0;
    Sc()[0x5F] = 0;
    Sc()[0x5C] = 0;
    Sc()[0x2A] = 0;
    Sc()[0x29] = 4;
    Sc()[0x24] = 4;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sc()[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sc()[0xB] % 3));
    Inc(Sc()[2]);
}

// original 0x4F5030: the mote's script ticked; when it reports an end, the
// owner's +0xB down and the record freed (0x4F6290).
C1_EXPORT void __cdecl Magic213Mote_End(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::MagicFx_FreeCurrentRecord);
}

// original 0x4F5050: the mote pool's alloc (0x6B2958, 64 records).
C1_EXPORT unsigned char __cdecl Magic213Mote_Alloc(void) { return PoolAlloc(::bof3::Img(Magic213Mote_Pool_addr)); }

void Magic213Mote_Inject() {
    BOF3_INJECT(Magic213_Task);
    BOF3_INJECT(Magic213_Start);
    BOF3_INJECT(Magic213Actor_Task);
    BOF3_INJECT(Magic213Actor_Run);
    BOF3_INJECT(Magic213Actor_Start);
    BOF3_INJECT(ActorFx_TintUp);
    BOF3_INJECT(Magic213Actor_Untint);
    BOF3_INJECT(Magic213Mote_Dispatch);
    BOF3_INJECT(Magic213Mote_Run);
    BOF3_INJECT(Magic213Mote_Start);
    BOF3_INJECT(Magic213Mote_End);
    BOF3_INJECT(Magic213Mote_Alloc);
}
