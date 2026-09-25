// The battle effect tasks: the three battle-task kinds BattleTask_RunAll
// dispatches by +6 - kind 0's nineteen effects (0x4352A0), kind 1's 110 magic
// effects (0x435350) and kind 2's magic rows (0x4378B0) - and, under kind 0,
// the damage-number popup and its five states (0x432B70..0x432F0B), the pose
// task (0x433190..0x4332F1), the actor watch (0x433460..0x433549) and the
// sprite that follows its owner (0x4337F0..0x433969); plus the two round
// hooks an enemy state stores at 0x904B64 / 0x904B68 (0x437720, 0x437750).
// docs/battle_fx_tasks.md.
//
// Every call goes through battle_fx_tasks::g (battle_fx_tasks_callees.h), so
// that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. Everything here is a faithful replacement.
//
// Two pointers are read throughout, and they are not the same read: the
// battle-task slot being run (0x93B8C4) and Sprite_Current (0x937F88).
// BattleTask_RunAll sets both to the slot, but each function reads the one
// the original reads, afresh wherever the original re-reads it.
#include "game/battle_fx_tasks.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_fx_tasks_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_fx_tasks {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
Handler H(std::uint32_t address) { return Fn<Handler>(address); }
}  // namespace

const Callees kOriginals = {
    {H(0x437CC0), H(0x432B70), H(0x432F90), H(0x433190), H(0x4332B0), H(0x433380), H(0x433460),
     H(0x4337F0), H(0x43C740), H(0x4348E0), H(0x434B90), H(0x433970), H(0x433B80), H(0x434D70),
     H(0x434F40), H(0x452680), H(0x452AD0), H(0x434310), H(0x452B60)},
    {H(0x437CC0), H(0x49AB60), H(0x4FB260), H(0x4D6E30), H(0x4AB570), H(0x4C0620), H(0x4A8360), H(0x4D80C0),
     H(0x4F1500), H(0x4A3C80), H(0x4CCAA0), H(0x4D0730), H(0x4CBAE0), H(0x4BDC40), H(0x4CAC40), H(0x4DAF00),
     H(0x4D5780), H(0x4B5B10), H(0x4B16C0), H(0x4B6A40), H(0x4C9E30), H(0x4C75B0), H(0x4C5110), H(0x4C8970),
     H(0x4F3640), H(0x4D2E30), H(0x4D3F30), H(0x4CDA00), H(0x4B7A10), H(0x4C2A50), H(0x4AAD50), H(0x4A2460),
     H(0x4C4930), H(0x4A1400), H(0x4C1730), H(0x4A00C0), H(0x4BBF40), H(0x4BCDF0), H(0x4BB1A0), H(0x4C3700),
     H(0x4C58A0), H(0x49EBC0), H(0x49DA80), H(0x4BEC00), H(0x4ACCE0), H(0x4CF770), H(0x4A3510), H(0x4DA3D0),
     H(0x4D9630), H(0x4A99A0), H(0x4AA1A0), H(0x4D18C0), H(0x4AD1C0), H(0x4E8700), H(0x4E9270), H(0x49E240),
     H(0x4DD9F0), H(0x4B7F40), H(0x4B4590), H(0x4A4DA0), H(0x4A5C10), H(0x4A66E0), H(0x4A6EC0), H(0x4AF1F0),
     H(0x4AFF00), H(0x4AEA40), H(0x4BFAE0), H(0x4FAFF0), H(0x4EB640), H(0x4B5810), H(0x49C170), H(0x4EF7D0),
     H(0x4FB0A0), H(0x499170), H(0x4B2120), H(0x4E7630), H(0x4C8E70), H(0x4C6560), H(0x4E1AE0), H(0x499780),
     H(0x4ABE00), H(0x4F1F70), H(0x4ED310), H(0x49C4C0), H(0x4EE460), H(0x4ADAF0), H(0x4E81A0), H(0x4D6300),
     H(0x4EDEC0), H(0x4A46F0), H(0x4DC260), H(0x4DF820), H(0x4E0A60), H(0x43FE90), H(0x4AC0E0), H(0x4F9FB0),
     H(0x4F7380), H(0x4F6440), H(0x4BD2B0), H(0x4F8860), H(0x4E6BA0), H(0x4F93A0), H(0x4E4540), H(0x4F5400),
     H(0x4B30F0), H(0x4E5220), H(0x4E33B0), H(0x4F4C40), H(0x4E9B70), H(0x4EA0F0)},
    {H(0x432C40), H(0x432DB0), H(0x432DE0), H(0x432E50), H(0x432EA0)},
    {H(0x4331D0), H(0x433290)},
    {H(0x4334C0), H(0x433550), H(0x433640), H(0x433650), H(0x433790)},
    {H(0x433810)},
    BattleFx_RollingDigits, Battle_DrawNumber, Battle_DrawLabel, BattleTask_FreeCurrent, Battle_ActorIsOut,
    Sprite_SetAnimation, Sprite_QueueOverlay, Sprite_ScriptTick, Sprite_UpdateScreen, ScriptFlags_Set40,
    Fn<void (__cdecl*)()>(kAfterAreaScript), Transition_Start,
};
Callees g = kOriginals;

}  // namespace battle_fx_tasks

using namespace battle_fx_tasks;

namespace {

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
// A pointer the original keeps in a dword, read afresh.
unsigned char* PtrAt(std::uint32_t address) { return At(static_cast<std::uint32_t>(Long(At(address)))); }
// The battle-task slot being run (0x93B8C4), and a slot's owner (+0x80).
unsigned char* Cur() { return PtrAt(at::kTaskCurrent); }
unsigned char* OwnerOf(const unsigned char* slot) { return At(static_cast<std::uint32_t>(Long(slot + 0x80))); }
// Sprite_Current, read afresh at every use.
unsigned char* Sc() { return Sprite_Current; }

// An actor's flag dword: a member's ObjTrio +0x130, an enemy's +0x110 (its
// low byte's bit 7 is set while a popup of its is up). As the original has
// it, the enemy index (actor - 3) is not checked, and the arithmetic is the
// original's 32-bit wrap.
unsigned char* ActorFlagByte(unsigned actor) {
    if (actor <= 2) return At(at::kMembers + 0x130 + actor * at::kMemberSize);
    return At(at::kEnemies + 0x110 + (actor - 3u) * at::kEnemySize);
}

void CallIndex(const Handler* table, unsigned n, unsigned index, const char* who) {
    if (index >= n) bof3::Fatal("%s: index %u, past the %u-entry table the original builds on its stack", who, index, n);
    table[index]();
}

}  // namespace

// ===========================================================================
// The three kinds
// ===========================================================================

// original 0x4352A0 (no PSX twin paired): battle-task kind 0. Entry +5 of the
// running slot (0x93B8C4) of a nineteen-entry table the original builds on
// its stack: 0x437CC0 (a bare ret), the damage popup 0x432B70, 0x432F90, the
// pose task 0x433190, 0x4332B0, 0x433380, the actor watch 0x433460, the
// follower 0x4337F0, then 0x43C740 .. 0x452B60 (unread).
//
// As the original has it: the index is not checked; 19..255 would call
// through the words above the table on the original's stack. Ours aborts
// instead (CLAUDE.md rule 4, as battle_flow's BattleTask_RunAll).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_Dispatch(void) {
    CallIndex(g.fx, 19, Cur()[5], "BattleFx_Dispatch");
}

// original 0x435350 (no PSX twin paired): battle-task kind 1, the magic
// effects BattleTask_Create(1, n)'s callers start. Entry +5 of the running
// slot of a 110-entry table the original builds on its stack (0x1B8 bytes):
// 0x437CC0 first, then the effects (battle_fx_tasks_callees.h). Unchecked as
// kind 0's; ours aborts past 109.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMagicFx_Dispatch(void) {
    CallIndex(g.magic_fx, 110, Cur()[5], "BattleMagicFx_Dispatch");
}

// original 0x4378B0 (no PSX twin paired): battle-task kind 2, the task
// Battle_StartItemMagic / Battle_StartAbilityMagic start with a magic row as
// its parameter. While the round flags 0x904AA8 have 0x800, it tail-jumps to
// the code pointer of row Sprite_Current +5 of Magic_Rows (0x64C2B8 + 8 row
// + 4, the dword after the row's DAT file); otherwise it returns.
//
// As the original has it: the row is read through Sprite_Current, not the
// slot pointer, and not checked; ours aborts past row 150 (the table's 151
// rows all hold .text pointers; row 151's is 0x9A1A9B1B, not code).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMagicRow_Run(void) {
    if ((static_cast<std::uint32_t>(Long(At(at::kRoundFlags))) & 0x800) == 0) return;
    const unsigned row = Sc()[5];
    if (row >= at::kMagicRowCount) bof3::Fatal("BattleMagicRow_Run: row %u, past Magic_Rows' 151", row);
    reinterpret_cast<Handler>(PtrAt(at::kMagicRows + row * 8 + 4))();
}

// ===========================================================================
// Kind 0, effect 1: the damage-number popup
// ===========================================================================

// original 0x432B70 (PSX 0x801E5BE4 by the catalogue's pairing, not read):
// state +1 of the running slot through a five-entry stack table (start,
// hold, rise, bounce, end - 0x432C40 .. 0x432EA0); then, while
// Sprite_Current's bit 0 is set, the slot's face by its mode +7 (a 4-entry
// jump table at 0x432C28):
//   0  a number: during states 0..2 the rolling digits (BattleFx_RollingDigits
//      with the CLUT row +0x27), from state 3 on Battle_DrawNumber(+0x36 - 12,
//      +0x3A, +0x27, the value +0x60);
//   1, 2, 3  a 24 x 8 label: Battle_DrawLabel(+0x36, +0x3A, +0x27, cell 2, 1, 0).
// Modes above 3 draw nothing.
//
// As the original has it: the slot pointer is re-read after the state call
// and Sprite_Current tested then; the state index is not checked (ours
// aborts past 4). The registers it pushes carry the upper bits of whatever
// they held before - the slot's or Sprite_Current's address above a byte or
// a word; the callees read only the byte or the word, and ours pushes the
// same bits.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_DamagePopup(void) {
    CallIndex(g.popup, 5, Cur()[1], "BattleFx_DamagePopup");
    const unsigned char* const sp = Sc();
    const std::uint32_t sc = Address(sp);
    if ((sp[0] & 1) == 0) return;
    unsigned char* const t = Cur();
    const std::uint32_t ta = Address(t);
    switch (t[7]) {
    case 0:
        if (t[1] < 3) {
            g.rolling_digits((ta & 0xFFFFFF00u) | t[0x27]);
            return;
        }
        g.draw_number(static_cast<int>((sc & 0xFFFF0000u) | ((Word(t + 0x36) - 0xCu) & 0xFFFFu)),
                      static_cast<int>(Word(t + 0x3A)), (sc & 0xFFFFFF00u) | t[0x27], Word(t + 0x60));
        return;
    case 1:
    case 2:
    case 3:
        g.draw_label(static_cast<int>((ta & 0xFFFF0000u) | Word(t + 0x36)),
                     static_cast<int>((sc & 0xFFFF0000u) | Word(t + 0x3A)), t[0x27], 3u - t[7]);
        return;
    default:
        return;
    }
}

// original 0x432C40 (state 0; inside 0x432B70's catalogue extent, so it was
// never listed on its own): the popup placed over its owner (+0x80) and
// sized.
//   - x +0x36 and y +0x3A: the owner's +0x2E / +0x30 plus a signed offset -
//     for a member (owner +5 <= 2) the s8 pair at 0x64DF70 by (owner +0x2C)
//     * 4 + owner +8; for an enemy its object's s8 +0xF2 / +0xF3.
//   - y 9 higher when +0xB is 1.
//   - the digit count +0xA: 2 for a value (+0x60, s32) of 100 or more, 1 for
//     10 or more, else 0 (the original divides by 100, then by 10, signed).
//   - the owner's flag byte (a member's +0x130, an enemy's +0x110) |= 0x80.
//   - the timer +9 = 8, the digit phase +0x32 = 0, the state + 1.
//
// As the original has it: the slot and the owner are re-read between x and
// y, the enemy index is not checked, and the count is stored through the
// slot pointer read before the owner's flag.
extern "C" void __cdecl BattleFx_PopupStart(void) {
    unsigned char* s = Cur();
    unsigned char* o = OwnerOf(s);
    unsigned actor = o[5];
    if (actor <= 2) {
        std::uint32_t k = Word(o + 0x2C) * 4u + o[8];
        SetWord(s + 0x36, static_cast<unsigned>(static_cast<signed char>(At(at::kPartyOffsets + k * 2)[0])) + Word(o + 0x2E));
        s = Cur();
        o = OwnerOf(s);
        k = Word(o + 0x2C) * 4u + o[8];
        SetWord(s + 0x3A, static_cast<unsigned>(static_cast<signed char>(At(at::kPartyOffsets + 1 + k * 2)[0])) + Word(o + 0x30));
    } else {
        SetWord(s + 0x36, static_cast<unsigned>(static_cast<signed char>(At(at::kEnemies + 0xF2 + (actor - 3u) * at::kEnemySize)[0])) +
                              Word(o + 0x2E));
        s = Cur();
        o = OwnerOf(s);
        actor = o[5];
        SetWord(s + 0x3A, static_cast<unsigned>(static_cast<signed char>(At(at::kEnemies + 0xF3 + (actor - 3u) * at::kEnemySize)[0])) +
                              Word(o + 0x30));
    }
    unsigned char* d = Cur();
    if (d[0xB] == 1) {
        SetWord(d + 0x3A, Word(d + 0x3A) - 9u);
        d = Cur();
    }
    const std::int32_t value = Long(d + 0x60);
    const unsigned char digits = value / 100 > 0 ? 2 : value / 10 > 0 ? 1 : 0;
    unsigned char* const flag = ActorFlagByte(OwnerOf(d)[5]);
    flag[0] = static_cast<unsigned char>(flag[0] | 0x80);
    d[0xA] = digits;
    Cur()[9] = 8;
    SetWord(Cur() + 0x32, 0);
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
}

// original 0x432DB0 (state 1): the timer +9 counts down; on the frame it is
// found 0 (it wraps to 0xFF first) the state + 1 and the timer 4.
extern "C" void __cdecl BattleFx_PopupHold(void) {
    unsigned char* const s = Cur();
    const unsigned char timer = s[9];
    s[9] = static_cast<unsigned char>(timer - 1);
    if (timer != 0) return;
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
    Cur()[9] = 4;
}

// original 0x432DE0 (state 2): the dword +0x38 + 0x40000 (y +0x3A down 4)
// every frame; the timer as the hold's, and when it was 0: the state + 1,
// the velocity +0xC = 0x8000 (x, 16.16) and +0x10 = -6.0 (y), and the start
// +0x1C / +0x18 = the s16 y / x.
extern "C" void __cdecl BattleFx_PopupRise(void) {
    unsigned char* s = Cur();
    SetLong(s + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x38)) + 0x40000u));
    s = Cur();
    const unsigned char timer = s[9];
    s[9] = static_cast<unsigned char>(timer - 1);
    if (timer != 0) return;
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
    SetLong(Cur() + 0xC, 0x8000);
    SetLong(Cur() + 0x10, static_cast<std::int32_t>(0xFFFA0000u));
    s = Cur();
    SetLong(s + 0x1C, static_cast<short>(Word(s + 0x3A)));
    s = Cur();
    SetLong(s + 0x18, static_cast<short>(Word(s + 0x36)));
}

// original 0x432E50 (state 3): the bounce. It ends - timer 10, state + 1 -
// once y +0x3A is back to or below its start (+0x1C <= the s16 y) while x has
// moved (+0x18 != the s16 x); otherwise the 16.16 position +0x34 / +0x38
// moves by the velocity +0xC / +0x10, and +0x10 gains 0x14000 (1.25).
//
// As the original has it: the three adds are 32-bit and wrap; the slot is
// re-read before the second and the third.
extern "C" void __cdecl BattleFx_PopupBounce(void) {
    unsigned char* s = Cur();
    if (!(Long(s + 0x1C) > static_cast<short>(Word(s + 0x3A))) && Long(s + 0x18) != static_cast<short>(Word(s + 0x36))) {
        s[9] = 0xA;
        Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
        return;
    }
    SetLong(s + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x34)) + static_cast<std::uint32_t>(Long(s + 0xC))));
    s = Cur();
    SetLong(s + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x38)) + static_cast<std::uint32_t>(Long(s + 0x10))));
    s = Cur();
    SetLong(s + 0x10, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x10)) + 0x14000u));
}

// original 0x432EA0 (state 4): the timer as the hold's; when it was 0 the
// owner's flag byte loses 0x80 again and the slot is freed
// (BattleTask_FreeCurrent, tail-jumped to).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_PopupEnd(void) {
    unsigned char* const s = Cur();
    const unsigned char timer = s[9];
    s[9] = static_cast<unsigned char>(timer - 1);
    if (timer != 0) return;
    unsigned char* const flag = ActorFlagByte(OwnerOf(Cur())[5]);
    flag[0] = static_cast<unsigned char>(flag[0] & 0x7F);
    g.free_current();
}

// ===========================================================================
// Kind 0, effect 3: the pose task
// ===========================================================================

// original 0x433190 (no PSX twin paired): the sprite animation set pointer
// 0x9039D8 = 0x8C5D80 around state Sprite_Current +1 of a two-entry stack
// table (0x4331D0, 0x433290), and 0x8B3580 after it.
//
// As the original has it: the pointer is set before the index is read and
// put back unconditionally; the index is not checked (ours aborts past 1).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_PoseTask(void) {
    unsigned char* const sc = Sc();
    SetLong(At(at::kAnimSet), static_cast<std::int32_t>(at::kAnimSetBattle));
    CallIndex(g.pose, 2, sc[1], "BattleFx_PoseTask");
    SetLong(At(at::kAnimSet), static_cast<std::int32_t>(at::kAnimSetField));
}

// original 0x4331D0 (state 0): Sprite_Current's draw bytes - +0x29 = 0, +0x25
// = 0x1D, +0x26 = 0, +0x24 = 0x84, the CLUT row +0x27 = byte 0x64B058 [mode
// +7] - 0x50, +0x28 = 0, the word +0x2C = 0, +0x2B = 0; the flip +0x2A = 1
// when (mode 6: +8 odd; any other mode: +8 is 1 or 2), else 0; the state + 1;
// then Sprite_SetAnimation(byte 0x64B064 [mode * 2 + (+8 >> 1)]) and
// Sprite_QueueOverlay (tail-jumped to).
//
// As the original has it: Sprite_Current is re-read for every store; neither
// table index is checked (both tables are constant data in the image).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_PoseStart(void) {
    Sc()[0x29] = 0;
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x24] = 0x84;
    {
        unsigned char* const sc = Sc();
        sc[0x27] = static_cast<unsigned char>(At(at::kPoseClut + sc[7])[0] - 0x50);
    }
    Sc()[0x28] = 0;
    SetWord(Sc() + 0x2C, 0);
    Sc()[0x2B] = 0;
    {
        unsigned char* const sc = Sc();
        const unsigned mode = sc[7], b = sc[8];
        if (mode == 6) sc[0x2A] = (b & 1) != 0 ? 1 : 0;
        else sc[0x2A] = b == 1 || b == 2 ? 1 : 0;
    }
    Sc()[1] = static_cast<unsigned char>(Sc()[1] + 1);
    unsigned char* const sc = Sc();
    g.set_animation(At(at::kPoseAnim + (sc[8] >> 1) + sc[7] * 2u)[0]);
    g.queue_overlay();
}

// original 0x433290 (state 1): Sprite_ScriptTick; the slot freed when it
// answers non-zero (the animation ended), else Sprite_QueueOverlay - each
// tail-jumped to.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_PosePlay(void) {
    if (g.script_tick() != 0) g.free_current();
    else g.queue_overlay();
}

// original 0x4332E0: Sprite_Current's timer +9 = 0 and its state + 1. State
// 0 of 0x4332B0's stack table (kind 0, effect 4), and entry 0 of the .data
// state table at 0x65C3A0 whose other three entries are 0x4FB010, 0x4FB050,
// 0x4FB070 (group CJ's magic effect).
extern "C" void __cdecl BattleFx_StepReset(void) {
    Sc()[9] = 0;
    Sc()[1] = static_cast<unsigned char>(Sc()[1] + 1);
}

// ===========================================================================
// Kind 0, effect 6: the actor watch
// ===========================================================================

// original 0x433460 (no PSX twin paired): nothing in phase 5 (byte
// 0x904AA0); otherwise state Sprite_Current +1 of a five-entry stack table -
// 0x4334C0, then 0x433550, 0x433640, 0x433650, 0x433790 (unread) - with the
// animation set pointer 0x9039D8 at 0x8C5D80 around it, as the pose task.
//
// As the original has it: the phase is read before Sprite_Current; the
// index is not checked (ours aborts past 4).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_ActorWatch(void) {
    if (At(at::kPhase)[0] == 5) return;
    unsigned char* const sc = Sc();
    SetLong(At(at::kAnimSet), static_cast<std::int32_t>(at::kAnimSetBattle));
    CallIndex(g.watch, 5, sc[1], "BattleFx_ActorWatch");
    SetLong(At(at::kAnimSet), static_cast<std::int32_t>(at::kAnimSetField));
}

// original 0x4334C0 (state 0): waits on its owner's actor (the owner
// 0x93B940's +5). Nothing while the round flags have 0x400 and the actor is
// byte 0x904B34, or while Battle_ActorIsOut(actor); otherwise the state + 1
// once the actor's status byte (a member's ObjTrio +0x90, an enemy's +0x92)
// has any of 0x58.
//
// As the original has it: the owner pointer is read before the flags test
// and again after Battle_ActorIsOut, and the actor is re-read from it then -
// the branch (member or enemy) is the one taken before the call, and the
// enemy index is not checked. Battle_ActorIsOut is handed the owner's
// address with the actor in its low byte; it reads the byte.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_ActorWatchTest(void) {
    const std::uint32_t flags = static_cast<std::uint32_t>(Long(At(at::kRoundFlags)));
    const unsigned char* o = PtrAt(at::kTaskOwner);
    if ((flags & 0x400) != 0 && o[5] == At(at::kTurnGate)[0]) return;
    const unsigned actor = o[5];
    const unsigned arg = (Address(o) & 0xFFFFFF00u) | actor;
    if (actor <= 2) {
        if (g.actor_is_out(arg) != 0) return;
        const unsigned now = PtrAt(at::kTaskOwner)[5];
        if ((At(at::kMembers + 0x90 + now * at::kMemberSize)[0] & 0x58) == 0) return;
    } else {
        if (g.actor_is_out(arg) != 0) return;
        const unsigned now = PtrAt(at::kTaskOwner)[5];
        if ((At(at::kEnemies + 0x92 + (now - 3u) * at::kEnemySize)[0] & 0x58) == 0) return;
    }
    Sc()[1] = static_cast<unsigned char>(Sc()[1] + 1);
}

// ===========================================================================
// Kind 0, effect 7: the follower
// ===========================================================================

// original 0x4337F0 (no PSX twin paired): state Sprite_Current +1 of a
// one-entry stack table, 0x433810. Any state but 0 would call through the
// original's stack; ours aborts.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_Follow(void) {
    CallIndex(g.follow, 1, Sc()[1], "BattleFx_Follow");
}

// original 0x433810 (PSX 0x800A5360 by the catalogue's pairing, not read):
// Sprite_Current takes its owner's (0x93B940) place and look - byte +0 with
// 0x20 added, then +0x48, the dwords +0x40 / +0x44, the words +0x58 / +0x5A,
// the bytes +0x4B / +0x4A / +0x49, the dwords +0x4C / +0x50 / +0x54, the
// bytes +0x27 / +0x28 / +0x2A, the dwords +0x34 / +0x38 / +0x3C, in that
// order; Sprite_UpdateScreen; then the slot is freed when the owner's bit 0
// is clear, and freed (again, tail-jumped) outside phase 1.
//
// As the original has it: both pointers are re-read for every copy, the
// owner again after Sprite_UpdateScreen, the phase after the first free; a
// dead owner outside phase 1 frees the slot twice.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_FollowOwner(void) {
    struct Copy { unsigned char offset, size; };
    static constexpr Copy kCopies[] = {{0x48, 1}, {0x40, 4}, {0x44, 4}, {0x58, 2}, {0x5A, 2}, {0x4B, 1}, {0x4A, 1},
                                       {0x49, 1}, {0x4C, 4}, {0x50, 4}, {0x54, 4}, {0x27, 1}, {0x28, 1}, {0x2A, 1},
                                       {0x34, 4}, {0x38, 4}, {0x3C, 4}};
    {
        const unsigned char b = PtrAt(at::kTaskOwner)[0];
        Sc()[0] = static_cast<unsigned char>(b | 0x20);
    }
    for (const Copy& c : kCopies) {
        const unsigned char* const o = PtrAt(at::kTaskOwner);
        unsigned char* const sc = Sc();
        if (c.size == 1) sc[c.offset] = o[c.offset];
        else if (c.size == 2) SetWord(sc + c.offset, Word(o + c.offset));
        else SetLong(sc + c.offset, Long(o + c.offset));
    }
    g.update_screen();
    if ((PtrAt(at::kTaskOwner)[0] & 1) == 0) g.free_current();
    if (At(at::kPhase)[0] != 1) g.free_current();
}

// ===========================================================================
// The round hooks
// ===========================================================================

// original 0x437720 (no PSX twin paired): stored at 0x904B64 by the enemy
// state 0x436110 (group CF's) and called by 0x431320 (group CC's). With bit 0
// of 0x904AE8 and the previous area (0x802290) 0xBD: MoveScript_Var7 = 1,
// the byte after it 0x19, ScriptFlags_Set40, then 0x446E20 (tail-jumped to,
// unread). The name is from what it tests; what area 0xBD is was not looked
// up.
//
// As the original has it: the flag byte is read once, before the area word.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleHook_Area189Script(void) {
    if ((At(at::kBattleEnd)[0] & 1) == 0) return;
    if (Word(At(at::kLastArea)) != 0xBD) return;
    At(at::kScriptVar7)[0] = 1;
    At(at::kScriptVar7 + 1)[0] = 0x19;
    g.script_flags_set40();
    g.after_area_script();
}

// original 0x437750 (no PSX twin paired): stored at 0x904B68 by the same
// state and called by 0x4317B0 (group CC's). With the same two conditions:
// Transition_Start(4), then byte 0x904AA2 + 1 (read after the call).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleHook_Area189Transition(void) {
    if ((At(at::kBattleEnd)[0] & 1) == 0) return;
    if (Word(At(at::kLastArea)) != 0xBD) return;
    g.transition_start(4);
    At(at::kRoundCount)[0] = static_cast<unsigned char>(At(at::kRoundCount)[0] + 1);
}

// ===========================================================================

void BattleFxTasks_Inject() {
    if (bof3::WantsShadow("battle_fx_tasks")) battle_fx_tasks::SelfTest();
    BOF3_INJECT(BattleFx_Dispatch);
    BOF3_INJECT(BattleMagicFx_Dispatch);
    BOF3_INJECT(BattleMagicRow_Run);
    BOF3_INJECT(BattleFx_DamagePopup);
    BOF3_INJECT(BattleFx_PopupStart);
    BOF3_INJECT(BattleFx_PopupHold);
    BOF3_INJECT(BattleFx_PopupRise);
    BOF3_INJECT(BattleFx_PopupBounce);
    BOF3_INJECT(BattleFx_PopupEnd);
    BOF3_INJECT(BattleFx_PoseTask);
    BOF3_INJECT(BattleFx_PoseStart);
    BOF3_INJECT(BattleFx_PosePlay);
    BOF3_INJECT(BattleFx_StepReset);
    BOF3_INJECT(BattleFx_ActorWatch);
    BOF3_INJECT(BattleFx_ActorWatchTest);
    BOF3_INJECT(BattleFx_Follow);
    BOF3_INJECT(BattleFx_FollowOwner);
    BOF3_INJECT(BattleHook_Area189Script);
    BOF3_INJECT(BattleHook_Area189Transition);
}
