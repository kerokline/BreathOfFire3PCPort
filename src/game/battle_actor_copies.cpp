// Group CH of the eighth takeover round (docs/battle_actor_copies.md): the
// sixteen functions pe_funcs.py folded into Battle_SpawnActorCopies 0x446FF0
// and ItemMenu_CanUseSelected 0x447840 - two entries of the battle's phase-1
// table 0x64AE28 (11 and 12, reached from 0x42E990 by a tail jump) and the
// states below them: the target choice of a command, and the battle's item
// window with the item's target choice. Each was read to its last
// instruction with capstone against bof3/BOF3.exe on 2026-09-25. Faithful:
// no divergence. The start-up fuzz is battle_actor_copies_fuzz.cpp.
//
//   BattleTarget_Dispatch     0x447110..0x44711D (0xE)    phase-1 entry 11: BattleTarget_Steps by 0x904AA3
//   BattleTarget_ShowPrompt   0x447120..0x447138 (0x19)   step 0
//   BattleTarget_PickDispatch 0x447140..0x447150 (0x11)   step 1: BattleTarget_Picks by 0x904AA4
//   BattleTarget_Begin        0x447160..0x44718E (0x2F)   pick 0
//   BattleTarget_PickEnemy    0x447190..0x447281 (0xF2)   pick 1
//   BattleTarget_Confirm      0x447390..0x4473DC (0x4D)   pick 3
//   BattleItem_Dispatch       0x447430..0x44743D (0xE)    phase-1 entry 12: BattleItem_Steps by 0x904AA3
//   BattleItem_OpenDispatch   0x447440..0x447450 (0x11)   step 0: BattleItem_OpenSteps by 0x904AA4
//   BattleItem_OpenWindow     0x447460..0x4474AA (0x4B)   step 0, sub 1
//   BattleItem_Browse         0x4474B0..0x447836 (0x387)  step 1
//   BattleItem_Choose         0x4478B0..0x44793F (0x90)   step 3
//   BattleItem_TargetDispatch 0x447940..0x447950 (0x11)   step 4: BattleItem_TargetSteps by 0x904AA4
//   BattleItem_TargetBegin    0x447960..0x4479DE (0x7F)   pick 0
//   BattleItem_PickEnemy      0x4479E0..0x447B05 (0x126)  pick 1
//   BattleItem_PickParty      0x447B10..0x447CBB (0x1AC)  pick 2
//   BattleItem_Confirm        0x447CC0..0x447D28 (0x69)   pick 3, and step 5's pick 2
//
// Every function is `void (void)`: the stubs are entered by a tail jump, and
// the chain's first call is Battle_PhaseDispatch's (ours), which discards eax.
// Every call goes through battle_actor_copies::g, so that the fuzz can stand
// recorders in for them. The five stubs read their table afresh from .data.
#include "game/battle_actor_copies.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_actor_copies_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_actor_copies {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(U address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
using F0 = U (__cdecl*)();
using F1 = U (__cdecl*)(U);
using F2 = U (__cdecl*)(U, U);
using F3 = U (__cdecl*)(U, U, U);
}  // namespace

const Callees kOriginals = {
    As<F2>(BattleBanner_SetMessage),
    As<F1>(Battle_DefaultTarget),
    Fn<F1>(kPrevTarget),
    As<F3>(Battle_WrapIndex),
    As<F1>(Input_AutoRepeat),
    As<F1>(Sound_PlayEffect),
    As<F0>(ItemMenu_SetupForActor),
    As<F3>(Char_AbilityList),
    As<F1>(Msg_SystemPtr),
    As<F0>(ItemMenu_CanUseSelected),
    As<F0>(Battle_ReturnTrue),
    As<F0>(ItemMenu_FreeWindows),
};
Callees g = kOriginals;

}  // namespace battle_actor_copies

using namespace battle_actor_copies;

namespace {

unsigned char& B(U address) { return At(address)[0]; }
// A pointer the original keeps in a dword of .bss, read afresh.
unsigned char* PtrAt(U address) { return At(static_cast<U>(Long(At(address)))); }
unsigned char* Command() { return PtrAt(at::kCommand); }
unsigned char* Acting() { return PtrAt(at::kActing); }
unsigned char ItemFlags(unsigned id) { return B(at::kItemRecords + id * at::kItemRecordSize); }
unsigned char Low(U eax) { return static_cast<unsigned char>(eax); }

// `mov al, [step]; jmp [eax*4 + table]`: entry `index` of a .data table,
// read afresh. As the original has it, the index is not checked - one past
// the end reads the next table's first entry (the tables sit back to back),
// so ours reads exactly what the original's jmp would.
using Handler = void (__cdecl*)();
void Dispatch(const unsigned long* table, unsigned index) { reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))(); }

// The pick states' common head: the cancel buttons against Input_Pressed
// (sub = 4), then the confirm buttons (sub = 3); else Input_AutoRepeat of its
// d-pad bits. Input_Pressed is read once, each mask at its test. Returns
// false when the head has set sub.
bool PickHead(U& held) {
    const unsigned pressed = Word(At(at::kPressed));
    if (pressed & Word(At(at::kCancel))) {
        B(at::kSub) = 4;
        return false;
    }
    if (pressed & Word(At(at::kConfirm))) {
        B(at::kSub) = 3;
        return false;
    }
    held = g.auto_repeat(pressed & 0xF000);
    return true;
}

// The target byte (+0 of the command, read as a signed byte) stepped by
// `delta` and wrapped by Battle_WrapIndex inside low .. high; the whole
// dword goes on. Everything is read before the call, as the original does.
U Stepped(U high, U low, int delta) {
    const int value = static_cast<signed char>(Command()[0]) + delta;
    return g.wrap_index(high, low, static_cast<U>(value));
}

// The enemies' side of a pick: 0x2000 the next enemy up through
// Battle_DefaultTarget, 0x8000 the next down through 0x4457F0, among actors
// 3 .. enemy count + 2; each with cue 0x101. Both may run in one frame.
void PickAmongEnemies(U held) {
    if (held & 0x2000) {
        const U high = static_cast<U>(B(at::kEnemyCount)) + 2;
        const unsigned char t = Low(g.default_target(Stepped(high, 3, +1)));
        Command()[0] = t;
        g.play_effect(0x101);
    }
    if (held & 0x8000) {
        const U high = static_cast<U>(B(at::kEnemyCount)) + 2;
        const unsigned char t = Low(g.prev_target(Stepped(high, 3, -1)));
        Command()[0] = t;
        g.play_effect(0x101);
    }
}

// The item under the item window's cursor: Char_AbilityList(the window's
// actor, its page, 1) - the actor's inventory page - read at the cursor.
// The actor and the page are read before the call, the cursor after.
unsigned ItemUnderCursor() {
    const unsigned page = B(at::kItemPage);
    const unsigned actor = B(at::kItemActor);
    const unsigned char* const list = At(g.ability_list(actor, page, 1));
    return list[B(at::kItemCursor)];
}

// The window's page, scroll and cursor kept for the actor 0x8033AA names, in
// the original's order: page and scroll read, the stores, the cursor read
// between its neighbours' stores.
void KeepPage(unsigned char page, unsigned char scroll, unsigned owner) {
    B(at::kActorPages + owner * 3) = page;
    const unsigned char cursor = B(at::kItemCursor);
    B(at::kActorPages + owner * 3 + 1) = scroll;
    B(at::kActorPages + owner * 3 + 2) = cursor;
}

}  // namespace

// ===========================================================================
// The target choice: phase-1 entry 11
// ===========================================================================

// original 0x447110 (the catalogue pairs it with PSX 0x80093A74, not read):
// entry 0x904AA3 of BattleTarget_Steps 0x64E3EC - 0x447120, 0x447140.
extern "C" void __cdecl BattleTarget_Dispatch(void) { Dispatch(BattleTarget_Steps, B(at::kStep)); }

// original 0x447120: BattleBanner_SetMessage(0, 0) - message 0, the prompt -
// then the step counted on (read after the call).
extern "C" void __cdecl BattleTarget_ShowPrompt(void) {
    g.set_message(0, 0);
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x447140: entry (dword 0x904AA4 & 0xFF) of BattleTarget_Picks
// 0x64E3F4 - 0x447160, 0x447190, 0x447290 (the party's side, not ours),
// 0x447390, 0x4473E0 (the cancel, not ours).
extern "C" void __cdecl BattleTarget_PickDispatch(void) { Dispatch(BattleTarget_Picks, B(at::kSub)); }

// original 0x447160: the target starts on Battle_DefaultTarget(3) - the first
// enemy able to be targeted - stored at the command's +0 (the pointer read
// after the call); then 0x904AAF = 1, the repeat latch 0x7E01B8 = 0 and the
// pick state counted on to 1 (read after the target's store).
extern "C" void __cdecl BattleTarget_Begin(void) {
    const unsigned char t = Low(g.default_target(3));
    Command()[0] = t;
    const unsigned char sub = B(at::kSub);
    B(at::kTargetCursor) = 1;
    SetWord(At(at::kRepeatLatch), 0);
    B(at::kSub) = static_cast<unsigned char>(sub + 1);
}

// original 0x447190: the pick among the enemies. The common head (cancel:
// sub 4, confirm: sub 3). Then 0x5000 (up or down, by the pad's bits) moves
// to the party's side - Battle_DefaultTarget(0), sub counted on to 2 before
// cue 0x101 - and ends; else 0x2000 and 0x8000 step among the enemies.
extern "C" void __cdecl BattleTarget_PickEnemy(void) {
    U held;
    if (!PickHead(held)) return;
    if (held & 0x5000) {
        const unsigned char t = Low(g.default_target(0));
        Command()[0] = t;
        B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 1);
        g.play_effect(0x101);
        return;
    }
    PickAmongEnemies(held);
}

// original 0x447390: the target confirmed. Cue 0x104; 0x904AAF = 0; the
// command's +1 = 1 (the pointer read after the cue); the acting context's +1 =
// 2 (its pointer read after that store); 0x904AC3 counted on; phase 1 = 1 and
// 0x904AA2..0x904AA4 = 0 - back to the command menu's next actor.
extern "C" void __cdecl BattleTarget_Confirm(void) {
    g.play_effect(0x104);
    unsigned char* const c = Command();
    B(at::kTargetCursor) = 0;
    c[1] = 1;
    Acting()[1] = 2;
    const unsigned char n = B(at::kEntryCount);
    B(at::kPhase1) = 1;
    B(at::kEntryCount) = static_cast<unsigned char>(n + 1);
    B(at::kPhase2) = 0;
    B(at::kStep) = 0;
    B(at::kSub) = 0;
}

// ===========================================================================
// The item window: phase-1 entry 12
// ===========================================================================

// original 0x447430 (the catalogue pairs it with PSX 0x80093F00, not read):
// entry 0x904AA3 of BattleItem_Steps 0x64E408 - 0x447440, 0x4474B0, 0x447880
// (the closing window, not ours), 0x4478B0, 0x447940, 0x447D70 (not ours).
extern "C" void __cdecl BattleItem_Dispatch(void) { Dispatch(BattleItem_Steps, B(at::kStep)); }

// original 0x447440: entry (dword 0x904AA4 & 0xFF) of BattleItem_OpenSteps
// 0x64E420 - 0x4481B0 (group CI's: a queued system message 0x4000), 0x447460.
extern "C" void __cdecl BattleItem_OpenDispatch(void) { Dispatch(BattleItem_OpenSteps, B(at::kSub)); }

// original 0x447460: the command's +1 = 4, then ItemMenu_SetupForActor; with
// the command's flags (+0x10, the pointer read after the call) bit 1 and not
// bit 17 the page is 2; then the acting actor (its context's +5) to 0x929F06,
// sub = 0 and the step counted on.
extern "C" void __cdecl BattleItem_OpenWindow(void) {
    Command()[1] = 4;
    g.setup_for_actor();
    const U flags = static_cast<U>(Long(Command() + 0x10));
    if ((flags & 2) && !(flags & 0x20000)) B(at::kItemPage) = 2;
    const unsigned char actor = Acting()[5];
    B(at::kSub) = 0;
    B(at::kItemActor) = actor;
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x4474B0: the item window's frame, until 0x8033A3 is set.
//   1. The cursor's place: x = the window's x + 7, y = (cursor - scroll + 2)
//      * 13 + the window's y (16 bits each), 0x80340C = 1.
//   2. Input_AutoRepeat(Input_Pressed & 0xF00C). Unless the command's flags
//      have bit 1 and not bit 17 (the page is then held at 2): 0x8000 a page
//      left (0..3, wrapping, 0x8033A9 = 0x32), else 0x2000 a page right
//      (0x31), each with cue 0x101 first.
//   3. One of: 0x1000 the cursor up (stops at 0; below the scroll, the scroll
//      motion 0xF0), 0x4000 down (stops at 9; at scroll + 7 or past, 0x10),
//      4 a page of rows up, 8 a page of rows down (the scroll at 3 at most).
//      Cue 0x100 when the cursor moved.
//   4. The description of the item under the cursor (its record's u16 +6
//      through Msg_SystemPtr) into the message queue entry before the write
//      index; the queue index is read before the list call.
//   5. While the list is not scrolling: cancel keeps the page, scroll and
//      cursor for 0x8033AA, closes the window (0x8033A3 = 1, 0x80340C = 0,
//      the queue entry's +1 = 1), cue 0x106, step + 1 (0x447880 frees it);
//      confirm, when ItemMenu_CanUseSelected answers 1, only cue 0x107, else
//      cue 0x103, the page kept, the item to the command's +2, the window
//      closed, and step 3 - or for item 0x97 the acting actor as the target,
//      window record 4's +3 = 2, phase 2 = 7 and step 0.
extern "C" void __cdecl BattleItem_Browse(void) {
    if (B(at::kItemState) != 0) return;
    SetWord(At(at::kItemCursorX), static_cast<unsigned>(Long(At(at::kItemBaseX))) + 7);
    B(at::kItemCursorOn) = 1;
    const unsigned rows = static_cast<unsigned>(B(at::kItemCursor)) - static_cast<unsigned>(Long(At(at::kItemScroll))) + 2;
    const unsigned y = rows * 13 + Word(At(at::kItemBaseY));
    const unsigned pressed = Word(At(at::kPressed));
    SetWord(At(at::kItemCursorY), y);
    const U held = g.auto_repeat(pressed & 0xF00C);

    const U flags = static_cast<U>(Long(Command() + 0x10));
    if (!(flags & 2) || (flags & 0x20000)) {
        if (held & 0x8000) {
            g.play_effect(0x101);
            const unsigned char page = static_cast<unsigned char>(B(at::kItemPage) - 1);
            B(at::kItemPageCue) = 0x32;
            B(at::kItemPage) = page;
            if (static_cast<signed char>(page) < 0) B(at::kItemPage) = 3;
        } else if (held & 0x2000) {
            g.play_effect(0x101);
            const unsigned char page = static_cast<unsigned char>(B(at::kItemPage) + 1);
            B(at::kItemPageCue) = 0x31;
            B(at::kItemPage) = page;
            if (page > 3) B(at::kItemPage) = 0;
        }
    }

    unsigned char cursor = B(at::kItemCursor);
    const unsigned char was = cursor;
    if (held & 0x1000) {
        if (cursor != 0) B(at::kItemCursor) = --cursor;
        if (static_cast<short>(cursor) < static_cast<short>(Word(At(at::kItemScroll)))) SetWord(At(at::kItemScrollMotion), 0xF0);
    } else if (held & 0x4000) {
        if (cursor < 9) B(at::kItemCursor) = ++cursor;
        const int last = static_cast<short>(Word(At(at::kItemScroll))) + 7;
        if (!(static_cast<int>(B(at::kItemCursor)) < last)) SetWord(At(at::kItemScrollMotion), 0x10);
    } else if (held & 4) {
        const short scroll = static_cast<short>(Word(At(at::kItemScroll)));
        if (scroll == 0) {
            cursor = 0;
            B(at::kItemCursor) = 0;
        } else if (scroll < 7) {
            const unsigned char s = B(at::kItemScroll);
            SetWord(At(at::kItemScroll), 0);
            cursor = static_cast<unsigned char>(cursor - s);
            B(at::kItemCursor) = cursor;
        } else {
            cursor = static_cast<unsigned char>(cursor - 7);
            B(at::kItemCursor) = cursor;
            SetWord(At(at::kItemScroll), static_cast<unsigned>(scroll - 7));
        }
    } else if (held & 8) {
        const short scroll = static_cast<short>(Word(At(at::kItemScroll)));
        if (scroll == 3) {
            cursor = 9;
            B(at::kItemCursor) = 9;
        } else if (scroll > -4) {
            const unsigned char s = B(at::kItemScroll);
            SetWord(At(at::kItemScroll), 3);
            cursor = static_cast<unsigned char>(cursor + static_cast<unsigned char>(3 - s));
            B(at::kItemCursor) = cursor;
        } else {
            cursor = static_cast<unsigned char>(cursor + 7);
            B(at::kItemCursor) = cursor;
            SetWord(At(at::kItemScroll), static_cast<unsigned>(scroll + 7));
        }
    }
    if (was != cursor) g.play_effect(0x100);

    const U entry = ((static_cast<U>(B(at::kQueueWrite)) - 1) & 0xF) * 8;
    const unsigned id = ItemUnderCursor();
    const U text = g.msg_system(Word(At(at::kItemRecords + id * at::kItemRecordSize + 6)));
    const bool scrolling = Word(At(at::kItemScrollMotion)) != 0;
    SetLong(At(at::kQueueText + entry), static_cast<std::int32_t>(text));
    if (scrolling) return;

    const unsigned buttons = Word(At(at::kPressed));
    if (buttons & Word(At(at::kCancel))) {
        const unsigned char page = B(at::kItemPage);
        const unsigned char scroll = B(at::kItemScroll);
        const unsigned owner = B(at::kItemOwner);
        B(at::kItemState) = 1;
        B(at::kItemCursorOn) = 0;
        B(at::kQueueByte + entry) = 1;
        KeepPage(page, scroll, owner);
        g.play_effect(0x106);
        B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
        return;
    }
    if (!(buttons & Word(At(at::kConfirm)))) return;
    if (Low(g.can_use()) != 0) {
        g.play_effect(0x107);
        return;
    }
    g.play_effect(0x103);
    const unsigned char page = B(at::kItemPage);
    const unsigned char scroll = B(at::kItemScroll);
    const unsigned owner = B(at::kItemOwner);
    KeepPage(page, scroll, owner);
    const unsigned actor = B(at::kItemActor);
    const unsigned char* const list = At(g.ability_list(actor, page, 1));
    SetWord(Command() + 2, list[B(at::kItemCursor)]);
    unsigned char* const c = Command();
    B(at::kItemState) = 1;
    B(at::kItemCursorOn) = 0;
    if (Word(c + 2) == 0x97) {
        unsigned char* const a = Acting();
        B(at::kWindow4State) = 2;
        c[0] = a[5];
        B(at::kPhase2) = 7;
        B(at::kStep) = 0;
        return;
    }
    B(at::kStep) = 3;
}

// original 0x4478B0: the chosen item's target, by its record's flags: 0x40 a
// choice - step 4 (0x447940) or, with 0x10, step 5 (0x447D70); else the target
// is set and the confirm (step 4, sub 3) follows - 0x80 all (0xC0), 0x10 one
// side whole (0x40 with 0x20, else 0x80), none the acting actor itself.
extern "C" void __cdecl BattleItem_Choose(void) {
    const unsigned char f = ItemFlags(ItemUnderCursor());
    if (f & 0x40) {
        B(at::kStep) = static_cast<unsigned char>(((f & 0x10) | 0x40) >> 4);
        return;
    }
    if (f & 0x80) {
        Command()[0] = 0xC0;
        B(at::kStep) = 4;
        B(at::kSub) = 3;
        return;
    }
    const unsigned char t = (f & 0x10) ? static_cast<unsigned char>((f & 0x20) ? 0x40 : 0x80) : Acting()[5];
    Command()[0] = t;
    B(at::kStep) = 4;
    B(at::kSub) = 3;
}

// original 0x447940: entry (dword 0x904AA4 & 0xFF) of BattleItem_TargetSteps
// 0x64E428 - 0x447960, 0x4479E0, 0x447B10, 0x447CC0, 0x447D30 (the cancel,
// not ours).
extern "C" void __cdecl BattleItem_TargetDispatch(void) { Dispatch(BattleItem_TargetSteps, B(at::kSub)); }

// original 0x447960: the item's target starts. The record of the command's
// item (+2) has 0x20: Battle_DefaultTarget(3) and sub 1 (the enemies); else
// the party: target 0 when Battle_ReturnTrue answers al non-zero (it always
// does), else Battle_DefaultTarget(0), and sub 2. Then the repeat latch
// 0x7E01B8 = 0 and 0x904AAF = 1.
extern "C" void __cdecl BattleItem_TargetBegin(void) {
    const unsigned id = Word(Command() + 2);
    if (ItemFlags(id) & 0x20) {
        const unsigned char t = Low(g.default_target(3));
        Command()[0] = t;
        B(at::kSub) = 1;
        SetWord(At(at::kRepeatLatch), 0);
        B(at::kTargetCursor) = 1;
        return;
    }
    if (Low(g.return_true()) != 0) {
        Command()[0] = 0;
    } else {
        const unsigned char t = Low(g.default_target(0));
        Command()[0] = t;
    }
    B(at::kSub) = 2;
    SetWord(At(at::kRepeatLatch), 0);
    B(at::kTargetCursor) = 1;
}

// original 0x4479E0: the item's pick among the enemies. The common head;
// then, for an item whose record (the item under the window's cursor, read
// again) has 0x80, 0x5000 moves to the party - Battle_DefaultTarget(0), sub
// counted on, cue 0x101 - and ends; else as BattleTarget_PickEnemy's steps.
extern "C" void __cdecl BattleItem_PickEnemy(void) {
    U held;
    if (!PickHead(held)) return;
    const unsigned char f = ItemFlags(ItemUnderCursor());
    if ((f & 0x80) && (held & 0x5000)) {
        const unsigned char t = Low(g.default_target(0));
        Command()[0] = t;
        B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 1);
        g.play_effect(0x101);
        return;
    }
    PickAmongEnemies(held);
}

// original 0x447B10: the item's pick among the party. The common head; then
// with the record's 0x80, 0x5000 moves to the enemies - Battle_DefaultTarget(3),
// sub counted back, cue 0x101 - and ends. Else 0x2000 the next member up and
// 0x8000 the next down, wrapped by Battle_WrapIndex inside 0 .. party count
// - 1: while Battle_ReturnTrue answers al non-zero the wrapped index's low
// byte itself (and after 0x8000 the function ends), else through
// Battle_DefaultTarget / 0x4457F0 - each with cue 0x101.
extern "C" void __cdecl BattleItem_PickParty(void) {
    U held;
    if (!PickHead(held)) return;
    const unsigned char f = ItemFlags(ItemUnderCursor());
    if ((f & 0x80) && (held & 0x5000)) {
        const unsigned char t = Low(g.default_target(3));
        Command()[0] = t;
        B(at::kSub) = static_cast<unsigned char>(B(at::kSub) - 1);
        g.play_effect(0x101);
        return;
    }
    if (held & 0x2000) {
        if (Low(g.return_true()) != 0) {
            const U high = static_cast<U>(B(at::kPartyCount)) - 1;
            const unsigned char t = Low(Stepped(high, 0, +1));
            Command()[0] = t;
        } else {
            const U high = static_cast<U>(B(at::kPartyCount)) - 1;
            const unsigned char t = Low(g.default_target(Stepped(high, 0, +1)));
            Command()[0] = t;
        }
        g.play_effect(0x101);
    }
    if (held & 0x8000) {
        if (Low(g.return_true()) != 0) {
            const U high = static_cast<U>(B(at::kPartyCount)) - 1;
            const unsigned char t = Low(Stepped(high, 0, -1));
            Command()[0] = t;
            g.play_effect(0x101);
            return;
        }
        const U high = static_cast<U>(B(at::kPartyCount)) - 1;
        const unsigned char t = Low(g.prev_target(Stepped(high, 0, -1)));
        Command()[0] = t;
        g.play_effect(0x101);
    }
}

// original 0x447CC0: the item's target confirmed. Cue 0x104; 0x904AAF = 0;
// the command's +1 = 4 (the pointer read after the cue); the acting context's
// +1 = 2; 0x904AC3 counted on; the message queue entry before the write index
// gets +1 = 1; ItemMenu_FreeWindows; then 0x904AA2..0x904AA4 = 0 and phase 1
// = 1.
extern "C" void __cdecl BattleItem_Confirm(void) {
    g.play_effect(0x104);
    unsigned char* const c = Command();
    B(at::kTargetCursor) = 0;
    c[1] = 4;
    Acting()[1] = 2;
    B(at::kEntryCount) = static_cast<unsigned char>(B(at::kEntryCount) + 1);
    const U entry = (static_cast<U>(B(at::kQueueWrite)) - 1) & 0xF;
    B(at::kQueueByte + entry * 8) = 1;
    g.free_windows();
    B(at::kPhase2) = 0;
    B(at::kStep) = 0;
    B(at::kSub) = 0;
    B(at::kPhase1) = 1;
}

void BattleActorCopies_Inject() {
    if (bof3::WantsShadow("battle_actor_copies")) battle_actor_copies::SelfTest();
    BOF3_INJECT(BattleTarget_Dispatch);
    BOF3_INJECT(BattleTarget_ShowPrompt);
    BOF3_INJECT(BattleTarget_PickDispatch);
    BOF3_INJECT(BattleTarget_Begin);
    BOF3_INJECT(BattleTarget_PickEnemy);
    BOF3_INJECT(BattleTarget_Confirm);
    BOF3_INJECT(BattleItem_Dispatch);
    BOF3_INJECT(BattleItem_OpenDispatch);
    BOF3_INJECT(BattleItem_OpenWindow);
    BOF3_INJECT(BattleItem_Browse);
    BOF3_INJECT(BattleItem_Choose);
    BOF3_INJECT(BattleItem_TargetDispatch);
    BOF3_INJECT(BattleItem_TargetBegin);
    BOF3_INJECT(BattleItem_PickEnemy);
    BOF3_INJECT(BattleItem_PickParty);
    BOF3_INJECT(BattleItem_Confirm);
}
