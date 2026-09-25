// The magic effects the recorded fight casts that only a pointer reaches:
// the PSX BMAGIC overlays' code compiled into the exe, run as battle tasks
// (BattleTask_RunAll's slots; Sprite_Current is the slot, 0x93B940 its
// owner). docs/magic_fx_reached.md.
//
//   - The disc-and-fan effect (kind-2 row 21, 0x4C4FC0): its start 0x4C5020,
//     the grow and fade 0x4AD130 / 0x4AD160 it shares with a twin overlay,
//     and its six ring tasks (kind 1, parameter 0x16, 0x4C5110) whose three
//     phases 0x4AD200 / 0x4AD270 / 0x4AD2B0 are FxRing_Phases 0x65B5B8's.
//   - The steal (kind-2 row 69, 0x4B54B0): the start 0x4B54F0 that rolls it,
//     the wait 0x4B5770, the report 0x4B57C0, the shared end 0x4F52D0; and the
//     thief's double (kind 1, parameter 0x45, 0x4B5810 through
//     StealClone_Types 0x65AC28 to 0x4B5830) with its phases 0x4B5880 /
//     0x4B58C0.
//   - The Healing Herb (kind-2 row 43, 0x4B8D70, MAGIC070.EMI on the PSX):
//     the task that walks the sparkle pool, its spawn 0x4B8E00 and end
//     0x4B8F50, and the sparkle's type-0 update 0x4B9000.
//   - The backdrop dim (kind 1, parameter 0x43, 0x4FAFF0 through FxDim_Phases
//     0x65C3A0) and its phases 0x4FB010 / 0x4FB050 / 0x4FB070.
//
// Every call goes through magic_fx_reached::g (magic_fx_reached_callees.h),
// so that the start-up fuzz can stand recorders in for them - for ours and
// for the originals' copies alike. No divergence: each is a faithful
// replacement, except that an index past one of the five stack tables aborts
// where the original would call through its own stack (the doc, section 3).
#include "game/magic_fx_reached.h"

#include "game/cheats.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_fx_reached_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_fx_reached {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
}  // namespace

const Callees kOriginals = {
    {FxDiscFan_Start, FxDiscFan_Grow, FxDiscFan_Fade},
    {Steal_Start, Steal_Wait, Steal_Report, MagicFx_EndWhenIdle},
    {Fn<Handler>(kCloneSize), StealClone_Run, StealClone_Finish, Fn<Handler>(kCloneFree)},
    {Sparkle_Spawn, Fn<Handler>(kSparkleTint), Fn<Handler>(kSparkleBrighten), Fn<Handler>(kSparkleThin), Sparkle_End,
     Fn<Handler>(kSparkleDone)},
    {Sparkle_Launch, Sparkle_Rise, Sparkle_Fade},
    BattleActor_UpdateScreenXY,
    MagicFx_DrawDisc,
    MagicFx_PushActorMatrix,
    MagicFx_DrawFan,
    Gte_PopMatrix,
    MagicFx_DrawRing,
    BattleTask_Create,
    BattleTask_FreeCurrent,
    Battle_SetTargetFlags,
    Battle_SetTargetFlag40,
    Sound_PlayById,
    Sound_PlayEffect,
    BattleActor_SetAnimation,
    Rand,
    Inventory_Add,
    Fn<void (__cdecl*)(unsigned, unsigned)>(kItemName),
    Msg_SystemPtr,
    BattleQueue_Push,
    Sprite_ScriptTickOnce,
    BattleActor_PlaySound,
    Sprite_UpdateScreen,
    Sparkle_Dispatch,
    Sparkle_Alloc,
    Sprite_ReleaseTint,
    BattleActor_Flash,
    Sparkle_DrawDisc,
    Sparkle_DrawRaysG2,
    Sparkle_DrawRaysG3,
    AreaMap_TintClut,
};
Callees g = kOriginals;

}  // namespace magic_fx_reached

using namespace magic_fx_reached;

namespace {

unsigned char* Ptr(std::uint32_t cell) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(cell)))));
}
std::int32_t PtrValue(const unsigned char* p) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)));
}
// A task slot by its index, unchecked as the originals index it.
unsigned char* Task(unsigned index) { return At(at::kTasks + index * at::kTaskStride); }
unsigned char Byte(std::uint32_t address) { return At(address)[0]; }
// An enemy by the battle index (3 and up), as the originals compute it: the
// index's byte minus 3, times 0x128 - below the records for 0..2.
unsigned char* Enemy(unsigned char index) {
    return At(at::kEnemies + static_cast<std::uint32_t>((static_cast<int>(index) - 3) * static_cast<int>(at::kEnemyStride)));
}
unsigned char* Party(unsigned char index) { return At(at::kParty + index * at::kPartyStride); }
// A .data dispatch table's entry, read as the original reads it.
Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(table + 4 * index)))));
}
// Sprite_Current's position copied from `from`: the actor byte +8 and the
// three dwords +0x34 / +0x38 / +0x3C, Sprite_Current read again for each.
void TakePosition(const unsigned char* from) {
    Sprite_Current[8] = from[8];
    SetLong(Sprite_Current + 0x34, Long(from + 0x34));
    SetLong(Sprite_Current + 0x38, Long(from + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(from + 0x3C));
}
void Bump(unsigned char& b, int by) { b = static_cast<unsigned char>(b + by); }

}  // namespace

#define MFX_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// The disc-and-fan effect and its rings

// original 0x4C4FC0: the effect task. Its phase +1 through a three-entry
// table the original builds on its stack (FxDiscFan_Start, _Grow, _Fade),
// then while the task lives (+0 not zero) the source's screen point, 16 up,
// the disc, and the fan under the actor's matrix. The index is not checked:
// 3..255 would call through the original's stack; ours aborts.
MFX_EXPORT void __cdecl FxDiscFan_Task(void) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= 3) bof3::Fatal("FxDiscFan_Task: phase %u, past the three-entry table", phase);
    g.disc_fan[phase]();
    if (Sprite_Current[0] == 0) return;
    g.update_screen_xy();
    unsigned char* const sc = Sprite_Current;
    SetWord(sc + 0x30, Word(sc + 0x30) - 0x10u);
    g.draw_disc();
    g.push_matrix();
    g.draw_fan();
    g.pop_matrix();
}

// original 0x4C5020: the start. The position of the source sprite 0x904B4C;
// six ring tasks (kind 1, parameter 0x16), each owned by this one with its
// delay +9 = 6 i + 1, counted in +0xB; the CLUT of strip row 26 made
// semi-transparent (cells 1..15 from 0x4000 below with bit 15, cell 0
// cleared) and marked dirty; the target's flags 0x10; sound 0x100; +9 and
// +0xA to 1 and the phase on.
//
// As the original has it: BattleTask_Create's index is not tested (0xFF
// writes past the slots); Sprite_Current is read again after each create;
// the source pointer is read once.
MFX_EXPORT void __cdecl FxDiscFan_Start(void) {
    TakePosition(Ptr(at::kSource));
    Sprite_Current[0xB] = 0;
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned index = g.task_create(1, 0x16);
        unsigned char* const sc = Sprite_Current;
        unsigned char* const t = Task(index);
        SetLong(t + 0x80, PtrValue(sc));
        t[9] = static_cast<unsigned char>(i * 6 + 1);
        Bump(sc[0xB], 1);
    }
    for (std::uint32_t a = at::kClutRow + 2; a < at::kClutRow + 0x20; a += 2)
        SetWord(At(a), Word(At(a - 0x4000)) | 0x8000u);
    const unsigned char target = Byte(at::kTarget);
    SetWord(At(at::kClutRow), 0);
    Gfx_ClutStripDirty = 1;
    g.set_target_flags(target, 0x10);
    g.play_by_id(0x100);
    Sprite_Current[9] = 1;
    Sprite_Current[0xA] = 1;
    Bump(Sprite_Current[1], 1);
}

// original 0x4AD130 (shared with the twin effect 0x4ACFE0): +9 and +0xA up
// by one; at +9 = 0x10 the phase on.
MFX_EXPORT void __cdecl FxDiscFan_Grow(void) {
    Bump(Sprite_Current[9], 1);
    Bump(Sprite_Current[0xA], 1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0x10) Bump(sc[1], 1);
}

// original 0x4AD160 (shared with 0x4ACFE0): the fade, as the rings go. With
// fewer than four rings left +9 steps down to 0, with fewer than three +0xA;
// with none left and +0xA at 0, the target's flag 0x40, 0x904AA8 bit 2 (the
// effect is done) and the task freed (a tail jmp in the original).
MFX_EXPORT void __cdecl FxDiscFan_Fade(void) {
    unsigned char* sc = Sprite_Current;
    if (sc[0xB] < 4 && sc[9] != 0) {
        Bump(sc[9], -1);
        sc = Sprite_Current;
    }
    if (sc[0xB] < 3 && sc[0xA] != 0) {
        Bump(sc[0xA], -1);
        sc = Sprite_Current;
    }
    if (sc[0xB] != 0 || sc[0xA] != 0) return;
    g.set_target_flag40(Byte(at::kTarget));
    At(at::kFlags)[0] = static_cast<unsigned char>(At(at::kFlags)[0] | 4);
    g.free_current();
}

// original 0x4C5110: a ring task. Its phase +1 through FxRing_Phases
// 0x65B5B8 (read in place, the index unchecked - four entries, the fourth
// 0x4AD300 never reached: phase 2 frees the task), then while it lives and
// is past its wait (+0 and +1 not zero) the ring under the actor's matrix.
MFX_EXPORT void __cdecl FxRing_Task(void) {
    Entry(at::kRingPhases, Sprite_Current[1])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[1] == 0) return;
    g.push_matrix();
    g.draw_ring();
    g.pop_matrix();
}

// original 0x4AD200: the ring's wait. +9 down; at 0 the owner's position
// (+0x34..+0x3C, not its actor byte), +9 = 0x40, +0xA and +0xB 0, phase on.
MFX_EXPORT void __cdecl FxRing_Wait(void) {
    Bump(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] != 0) return;
    SetLong(sc + 0x34, Long(Ptr(at::kOwner) + 0x34));
    SetLong(Sprite_Current + 0x38, Long(Ptr(at::kOwner) + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(Ptr(at::kOwner) + 0x3C));
    Sprite_Current[9] = 0x40;
    Sprite_Current[0xA] = 0;
    Sprite_Current[0xB] = 0;
    Bump(Sprite_Current[1], 1);
}

// original 0x4AD270: the ring rises. +0xB up one, +0xA up two, +9 down two;
// at +9 = 0x10 the phase on.
MFX_EXPORT void __cdecl FxRing_Rise(void) {
    Bump(Sprite_Current[0xB], 1);
    Bump(Sprite_Current[0xA], 2);
    Bump(Sprite_Current[9], -2);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0x10) Bump(sc[1], 1);
}

// original 0x4AD2B0: the ring fades. +0xB up one, +0xA down three, +9 down
// two; at +9 = 0 the owner's ring count +0xB down and the task freed (a tail
// jmp in the original).
MFX_EXPORT void __cdecl FxRing_Fade(void) {
    Bump(Sprite_Current[0xB], 1);
    Bump(Sprite_Current[0xA], -3);
    Bump(Sprite_Current[9], -2);
    if (Sprite_Current[9] != 0) return;
    Bump(Ptr(at::kOwner)[0xB], -1);
    g.free_current();
}

// ===========================================================================
// The steal

// original 0x4B54B0: the steal task. Its phase +1 through a four-entry table
// on the original's stack (Steal_Start, _Wait, _Report, MagicFx_EndWhenIdle);
// 4..255 would call through that stack, ours aborts.
MFX_EXPORT void __cdecl Steal_Task(void) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= 4) bof3::Fatal("Steal_Task: phase %u, past the four-entry table", phase);
    g.steal[phase]();
}

namespace {
// The thief's multiplier by the speed difference (its +0xA8 less the
// enemy's +0xB8): 12 from 49 up, down one at each of 29, 19, 9, -10, -20,
// -30 and -50 - 4 below -50.
int StealMultiplier(int diff) {
    int m = 0xC;
    if (diff < 0x31) m = 0xB;
    if (diff < 0x1D) m = 0xA;
    if (diff < 0x13) m = 9;
    if (diff < 9) m = 8;
    if (diff < -0xA) m = 7;
    if (diff < -0x14) m = 6;
    if (diff < -0x1E) m = 5;
    if (diff < -0x32) m = 4;
    return m;
}
}  // namespace

// original 0x4B54F0 (PSX 0x801EEC90's slot): the start. The owner's position;
// +0xB 0, the wait +9 = 0x1E, the phase on; the thief's animation 0xC; the
// thief's double - a kind-1 task (parameter 0x45) owned by this one, the
// first 0x80 bytes of the acting party record copied into it, then its +1 /
// +2 cleared and its kind and parameter set again (the copy overwrote them)
// - counted in +0xB; the owner's +0 bit 6. Then the roll: Rand & 0xFF below
// the enemy's rate (Steal_RateTable by its +0xAA, signed) times the speed
// multiplier steals the enemy's item (+0xA8) through Inventory_Add - message
// 0x38 and the item kept in +0x2C, the enemy's item and rate cleared - or,
// the bag full, 0x39; nothing to steal is 0x3A; a failed roll 0x39, or 0x3A
// when the enemy's rate is 0. The message id goes to +0xA.
//
// This is Pilfer's copy of the routine (the PSX's MAGIC065.EMI; Steal's is
// 0x4F5140, not ours - docs/cheats.md). DIV-0046's cheat lives in its roll:
// see Cheats_PilferRollMask.
//
// As the original has it: the double's index is not tested; the target is
// not checked to be an enemy (0..2 reads below the enemy records); the rate
// row is unbounded; the target byte is read again after Rand and after
// Inventory_Add; Inventory_Add gets a fourth 0 it does not read.
MFX_EXPORT void __cdecl Steal_Start(void) {
    TakePosition(Ptr(at::kOwner));
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0x1E;
    Bump(Sprite_Current[1], 1);
    g.set_animation(0xC, 2);
    const unsigned index = g.task_create(1, 0x45);
    unsigned char* const sc = Sprite_Current;
    unsigned char* const t = Task(index);
    const unsigned char actor = Byte(at::kActor);
    SetLong(t + 0x80, PtrValue(sc));
    {
        const unsigned char* const from = Party(actor);
        for (unsigned k = 0; k < 0x80; k += 4) SetLong(t + k, Long(from + k));   // rep movsd, dword by dword
    }
    t[1] = 0;
    t[2] = 0;
    t[6] = 1;
    t[5] = 0x45;
    Bump(sc[0xB], 1);
    unsigned char* const owner = Ptr(at::kOwner);
    owner[0] = static_cast<unsigned char>(owner[0] | 0x40);

    const unsigned char* enemy = Enemy(Byte(at::kTarget));
    const int diff = static_cast<int>(Word(Party(Byte(at::kActor)) + at::kThiefSpeed)) -
                     static_cast<int>(Word(enemy + at::kEnemySpeed));
    const int multiplier = StealMultiplier(diff);
    const int rate = static_cast<signed char>(At(at::kStealRates)[enemy[at::kStealRate]]);
    const int r = g.rand();
    unsigned char* const e = Enemy(Byte(at::kTarget));
    // DIV-0046 (BOF3X_STEAL=1): the mask is 0 when the cheat's patch is in
    // the original's `and eax, 0xFF` at 0x4B5690 - the roll then passes
    // whenever the rate times the multiplier is above 0.
    const std::uint32_t mask = Cheats_PilferRollMask();
    if (static_cast<int>(static_cast<std::uint32_t>(r) & mask) >= static_cast<int>(static_cast<std::uint32_t>(rate) * static_cast<std::uint32_t>(multiplier))) {
        Sprite_Current[0xA] = e[at::kStealRate] != 0 ? 0x39 : 0x3A;
        return;
    }
    const unsigned item = Word(e + at::kStealItem);
    if (item == 0) {
        Sprite_Current[0xA] = 0x3A;
        return;
    }
    if (g.inventory_add(item >> 8, item, 1) == 0) {
        Sprite_Current[0xA] = 0x39;
        return;
    }
    Sprite_Current[0xA] = 0x38;
    SetWord(Sprite_Current + 0x2C, item);
    unsigned char* const e2 = Enemy(Byte(at::kTarget));
    e2[at::kStealRate] = 0;
    SetWord(e2 + at::kStealItem, 0);
}

// original 0x4B5770: the wait for the double (+0xB counts it). When it is
// gone: at the first frame (+9 = 0x1E) the thief's animation 4 and the
// owner's +0 bit 6 cleared; +9 down, and at 0 the phase on.
MFX_EXPORT void __cdecl Steal_Wait(void) {
    unsigned char* sc = Sprite_Current;
    if (sc[0xB] != 0) return;
    if (sc[9] == 0x1E) {
        g.set_animation(4, 0);
        unsigned char* const owner = Ptr(at::kOwner);
        owner[0] = static_cast<unsigned char>(owner[0] & 0xBF);
        sc = Sprite_Current;
    }
    Bump(sc[9], -1);
    sc = Sprite_Current;
    if (sc[9] == 0) Bump(sc[1], 1);
}

// original 0x4B57C0: the report, once the message window is down
// (0x939F60). A theft first puts the item's name in Text_Records (0x4B58F0,
// index +0x2C, category +0x2D); the system message +0xA is queued
// (BattleQueue_Push(1, 0x1E, text)); the phase on.
//
// As the original has it: the message id travels in the low word of a dword
// whose high word is Sprite_Current's (Msg_SystemPtr reads the word).
MFX_EXPORT void __cdecl Steal_Report(void) {
    if (Byte(at::kMessageUp) != 0) return;
    const unsigned char* sc = Sprite_Current;
    if (sc[0xA] == 0x38) {
        g.item_name(sc[0x2C], sc[0x2D]);
        sc = Sprite_Current;
    }
    const unsigned char* const text = g.msg_system(sc[0xA]);
    g.queue_push(1, 0x1E, static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(text)));
    Bump(Sprite_Current[1], 1);
}

// original 0x4F52D0 (the last phase of the steal and of the effect at
// 0x4F50B0): once the message window is down, 0x904AA8 bit 2 and the task
// freed (a tail jmp in the original).
MFX_EXPORT void __cdecl MagicFx_EndWhenIdle(void) {
    if (Byte(at::kMessageUp) != 0) return;
    At(at::kFlags)[0] = static_cast<unsigned char>(At(at::kFlags)[0] | 4);
    g.free_current();
}

// original 0x4B5810: the thief's double (kind 1, parameter 0x45): a jmp
// through StealClone_Types 0x65AC28 by +1, read in place, unchecked (two
// entries; +1 stays 0 - Steal_Start clears it and nothing steps it).
MFX_EXPORT void __cdecl StealClone_Dispatch(void) { Entry(at::kCloneTypes, Sprite_Current[1])(); }

// original 0x4B5830: the double's type 0. Its phase +2 through a four-entry
// table on the original's stack (0x4ED5C0 its size, StealClone_Run,
// StealClone_Finish, 0x4AEE90 the free), then while it lives its sprite's
// screen update. 4..255 would call through that stack; ours aborts.
MFX_EXPORT void __cdecl StealClone_Task(void) {
    const unsigned phase = Sprite_Current[2];
    if (phase >= 4) bof3::Fatal("StealClone_Task: phase %u, past the four-entry table", phase);
    g.clone[phase]();
    if (Sprite_Current[0] != 0) g.update_screen();
}

// original 0x4B5880: the double runs its animation once a frame; +9 down,
// and at 0 the thief's sounds (0, 4) and the phase +2 on.
MFX_EXPORT void __cdecl StealClone_Run(void) {
    g.script_tick_once();
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    g.play_sound(0, 4);
    Bump(Sprite_Current[2], 1);
}

// original 0x4B58C0: the double's animation to its end; then the target's
// flag 0x40, the owner's count +0xB down and the phase +2 on.
MFX_EXPORT void __cdecl StealClone_Finish(void) {
    if (g.script_tick_once() == 0) return;
    g.set_target_flag40(Byte(at::kTarget));
    Bump(Ptr(at::kOwner)[0xB], -1);
    Bump(Sprite_Current[2], 1);
}

// ===========================================================================
// The Healing Herb's task and the sparkle update (MAGIC070.EMI)

// original 0x4B8D70 (PSX MAGIC070's entry): the task's phase +1 through a
// six-entry table on the original's stack (Sparkle_Spawn, 0x4B1E70,
// 0x4B1ED0, 0x4EE8A0, Sparkle_End, 0x4F7350; 6..255 would call through that
// stack, ours aborts); then every sparkle in use (bit 0 of +0) becomes the
// current one 0x685D90 with its +0x28 as 0x93B940 for Sparkle_Dispatch, and
// 0x93B940 is put back after each. The pool is walked by its own pointer;
// 0x93B940 is saved once, after the phase.
MFX_EXPORT void __cdecl Sparkle_Task(void) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= 6) bof3::Fatal("Sparkle_Task: phase %u, past the six-entry table", phase);
    g.sparkle_task[phase]();
    const std::int32_t saved = Long(At(at::kOwner));
    for (unsigned i = 0; i < at::kSparkles; ++i) {
        unsigned char* const s = At(at::kSparklePool + i * at::kSparkleStride);
        if ((s[0] & 1) == 0) continue;
        SetLong(At(at::kSparkleCurrent), PtrValue(s));
        SetLong(At(at::kOwner), Long(s + 0x28));
        g.sparkle_dispatch();
        SetLong(At(at::kOwner), saved);
    }
}

// original 0x4B8E00 (PSX MAGIC070's): the spawn. Bytes +0..+2 of all 128
// sparkles cleared; the task's kind +4 to 0; the source sprite's position;
// its screen point; +0xB 0, +9 = 8, +0xA 0, the phase on; then for each of
// Sparkle_CountByKind[kind] a sparkle: owned by the task, type and phase 0,
// colour row Rand & 3, the task's kind, offset row n, delay
// Sparkle_DelayByKind[kind] * (n >> 2) + 1 (a byte), counted in +0xB - a
// full pool skips one; sound 0x100.
//
// As the original has it: the kind is read again from the task each time
// (0 as it has just set it, unless a callee moves it); Sprite_Current is
// read again after each call; the loop's bound is re-read each round.
MFX_EXPORT void __cdecl Sparkle_Spawn(void) {
    for (unsigned i = 0; i < at::kSparkles; ++i) {
        unsigned char* const s = At(at::kSparklePool + i * at::kSparkleStride);
        s[0] = 0;
        s[1] = 0;
        s[2] = 0;
    }
    Sprite_Current[4] = 0;
    TakePosition(Ptr(at::kSource));
    g.update_screen_xy();
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 8;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[1], 1);
    unsigned char n = 0;
    if (At(at::kSparkleCounts)[Sprite_Current[4]] != 0) {
        do {
            const unsigned char a = g.sparkle_alloc();
            if (a != 0xFF) {
                unsigned char* const s = At(at::kSparklePool + a * at::kSparkleStride);
                SetLong(s + 0x28, PtrValue(Sprite_Current));
                s[1] = 0;
                s[2] = 0;
                const int r = g.rand();
                unsigned char* const sc = Sprite_Current;
                s[3] = static_cast<unsigned char>(r & 3);
                s[4] = sc[4];
                s[7] = n;
                s[5] = static_cast<unsigned char>(At(at::kSparkleSteps)[sc[4]] * static_cast<unsigned>(n >> 2) + 1);
                Bump(sc[0xB], 1);
            }
            ++n;
        } while (n < At(at::kSparkleCounts)[Sprite_Current[4]]);
    }
    g.play_effect(0x100);
}

// original 0x4B8F50 (PSX MAGIC070's): the end of the glow. The tint record
// +0xA's three colour bytes (+2..+4 of MoveScript_TintRecords' 12-byte
// record) down one; +9 down, and at 0 the source's tints released, the
// target flashed and the phase on.
MFX_EXPORT void __cdecl Sparkle_End(void) {
    unsigned char* const sc = Sprite_Current;
    for (unsigned k = 2; k <= 4; ++k) {
        unsigned char* const tint = At(at::kTints + sc[0xA] * 12u + k);
        Bump(*tint, -1);
    }
    Bump(sc[9], -1);
    if (Sprite_Current[9] != 0) return;
    g.release_tint(Ptr(at::kSource));
    g.flash(Byte(at::kTarget));
    Bump(Sprite_Current[1], 1);
}

// original 0x4B9000 (PSX MAGIC070's): the sparkle's type 0 (Sparkle_Types'
// one entry). Its phase +2 through a three-entry table on the original's
// stack (Sparkle_Launch, _Rise, _Fade); then while it lives (+0 bit 0) and
// is launched (+2 not 0) its disc, and on frames where its sway +0xC and 3 is
// not 0 its rays: two-point ones of radius +6 at Frame_Counter / 2, then
// three-point ones of radius +6 * 1.5 at Frame_Counter / 2 + 4.
//
// The phase is not checked by the original: 3 calls the dword above the
// table - its own return address into Sparkle_Task - and on up the caller's
// stack. Phases stay 0..2 (Launch and Rise step them, Sparkle_Free clears
// them); ours aborts past 2 (docs/battle_items.md section 3 left it Capcom's
// for this; the section 3 of this group's doc says why it is taken now).
// As the original has it: the rays' arguments are pushed with stale upper
// halves (their callees read the low words).
MFX_EXPORT void __cdecl Sparkle_Update(void) {
    const unsigned phase = Ptr(at::kSparkleCurrent)[2];
    if (phase >= 3) bof3::Fatal("Sparkle_Update: phase %u, past the three-entry table", phase);
    g.sparkle[phase]();
    const unsigned char* c = Ptr(at::kSparkleCurrent);
    if ((c[0] & 1) == 0 || c[2] == 0) return;
    g.sparkle_disc();
    c = Ptr(at::kSparkleCurrent);
    if ((c[0xC] & 3) == 0) return;
    g.rays_g2(Frame_Counter >> 1, c[6]);
    c = Ptr(at::kSparkleCurrent);
    const unsigned radius = c[6];
    g.rays_g3((Frame_Counter >> 1) + 4, (radius >> 1) + radius);
}

// ===========================================================================
// The backdrop dim

// original 0x4FAFF0: the dim task (kind 1, parameter 0x43): a jmp through
// FxDim_Phases 0x65C3A0 by +1, read in place, unchecked (entry 0 is
// 0x4332E0, group CE's; the fifth word is 0).
MFX_EXPORT void __cdecl FxDim_Dispatch(void) { Entry(at::kDimPhases, Sprite_Current[1])(); }

// original 0x4FB010: +9 down; at -6 the phase on; the map's CLUTs tinted by
// +9 (signed).
MFX_EXPORT void __cdecl FxDim_Down(void) {
    Bump(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0xFA) Bump(sc[1], 1);
    g.tint_clut(static_cast<signed char>(Sprite_Current[9]));
}

// original 0x4FB050: held until 0x904AA8 bit 2 (the effect is done), then
// the phase on.
MFX_EXPORT void __cdecl FxDim_Hold(void) {
    if (At(at::kFlags)[0] & 4) Bump(Sprite_Current[1], 1);
}

// original 0x4FB070: +9 up; at 0 the task freed; the CLUTs tinted by +9
// (signed), read after the free.
MFX_EXPORT void __cdecl FxDim_Up(void) {
    Bump(Sprite_Current[9], 1);
    if (Sprite_Current[9] == 0) g.free_current();
    g.tint_clut(static_cast<signed char>(Sprite_Current[9]));
}

#undef MFX_EXPORT

// ===========================================================================

void MagicFxReached_Inject() {
    if (bof3::WantsShadow("magic_fx_reached")) magic_fx_reached::SelfTest();
    BOF3_INJECT(FxDiscFan_Task);
    BOF3_INJECT(FxDiscFan_Start);
    BOF3_INJECT(FxDiscFan_Grow);
    BOF3_INJECT(FxDiscFan_Fade);
    BOF3_INJECT(FxRing_Task);
    BOF3_INJECT(FxRing_Wait);
    BOF3_INJECT(FxRing_Rise);
    BOF3_INJECT(FxRing_Fade);
    BOF3_INJECT(Steal_Task);
    BOF3_INJECT(Steal_Start);
    BOF3_INJECT(Steal_Wait);
    BOF3_INJECT(Steal_Report);
    BOF3_INJECT(MagicFx_EndWhenIdle);
    BOF3_INJECT(StealClone_Dispatch);
    BOF3_INJECT(StealClone_Task);
    BOF3_INJECT(StealClone_Run);
    BOF3_INJECT(StealClone_Finish);
    BOF3_INJECT(Sparkle_Task);
    BOF3_INJECT(Sparkle_Spawn);
    BOF3_INJECT(Sparkle_End);
    BOF3_INJECT(Sparkle_Update);
    BOF3_INJECT(FxDim_Dispatch);
    BOF3_INJECT(FxDim_Down);
    BOF3_INJECT(FxDim_Hold);
    BOF3_INJECT(FxDim_Up);
}
