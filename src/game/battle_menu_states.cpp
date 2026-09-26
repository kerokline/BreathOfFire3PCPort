// The battle menu states: two commands' state machines, reached from the
// command step 0x42EED0, which tail-jumps through 0x64AE54 by the command byte
// 0x904AA2 (Battle_PhaseDispatch -> 0x42E990 -> 0x42EED0 -> here, every link
// a tail jump, so each function below returns to Battle_PhaseDispatch and
// takes nothing). Command 3 (0x447FD0 and 0x64E44C) picks an enemy and
// commits action kind 0 - the plain attack, by that (a hypothesis in the
// names); command 2 (0x448180 and 0x64E45C) is the item command: the prompt,
// the party's item list, the item's target kind, the target picks and the
// commit. docs/battle_menu_states.md.
//
// Every call goes through battle_menu_states::g (battle_menu_states_callees.h),
// so that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. Everything here is a faithful replacement.
#include "game/battle_menu_states.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_menu_states_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_menu_states {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Battle_DefaultTarget,
    Fn<unsigned char (__cdecl*)(unsigned)>(kPrevTarget),
    Battle_WrapIndex,
    BattleBanner_SetMessage,
    Input_AutoRepeat,
    Sound_PlayEffect,
    Msg_SystemPtr,
    BattleQueue_Push,
    ItemMenu_SetupForParty,
    ItemMenu_FreeWindows,
    Item_HelpMessage,
    Item_CanUse,
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(kItemFlags),
    Battle_ReturnTrue,
};
Callees g = kOriginals;

}  // namespace battle_menu_states

using namespace battle_menu_states;

namespace {

using Handler = void (__cdecl*)();

// A pointer the original keeps in a dword of .data / .bss, read afresh.
unsigned char* PtrAt(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(address)))));
}
unsigned char& Byte(std::uint32_t address) { return At(address)[0]; }
// The command being built and the acting member's record, each re-read
// wherever the original re-reads it.
unsigned char* Command() { return PtrAt(at::kCommandRecord); }
unsigned char* Acting() { return PtrAt(at::kActing); }
unsigned char* List() { return At(at::kList); }
// Entry i of a dispatch table in .data, read at the dispatch: no bound, as
// the original has it - an index past the table reads the next one's.
Handler Entry(std::uint32_t table, unsigned i) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(table + 4 * i)))));
}
// The list's item: the id at the cursor row of the category's id list. The
// category, the cursor and the list pointer are read in that order, with no
// call between.
unsigned ListItem() {
    const unsigned category = List()[0xA];
    const unsigned row = List()[0xC];
    return PtrAt(at::kInventoryIds + 4 * category)[row];
}
// The list's item's flag byte: 0x591810(category, item).
unsigned char ListItemFlags() {
    const unsigned category = List()[0xA];
    const unsigned item = ListItem();
    return g.item_flags(category, item);
}
// The low 3 bits of the queue's write index less one: the entry last pushed.
unsigned LastQueueEntry() { return ((Byte(at::kQueueWrite) - 1u) & 0xF) * 8; }

// Left and right on the enemy side: Battle_DefaultTarget / 0x4457F0 from the
// target one on / one back, wrapped to 3 .. enemy count + 2 by
// Battle_WrapIndex. Both of Battle_WrapIndex's results are passed on whole,
// as the original pushes eax. The command pointer is read before and again
// after the calls; the enemy count after Input_AutoRepeat (and, for the
// second, after the first's sound).
void StepEnemyTarget(unsigned repeat) {
    if (repeat & 0x2000) {
        const long high = static_cast<long>(Byte(at::kEnemyCount)) + 2;
        const long value = static_cast<long>(static_cast<signed char>(Command()[0])) + 1;
        const long wrapped = g.wrap_index(high, 3, value);
        const unsigned char target = g.default_target(static_cast<unsigned>(wrapped));
        Command()[0] = target;
        g.sound(0x101);
    }
    if (repeat & 0x8000) {
        const long high = static_cast<long>(Byte(at::kEnemyCount)) + 2;
        const long value = static_cast<long>(static_cast<signed char>(Command()[0])) - 1;
        const long wrapped = g.wrap_index(high, 3, value);
        const unsigned char target = g.prev_target(static_cast<unsigned>(wrapped));
        Command()[0] = target;
        g.sound(0x101);
    }
}

}  // namespace

// ===========================================================================
// Command 3: pick an enemy, commit action kind 0
// ===========================================================================

// original 0x447FD0 (PSX 0x80095070 by the catalogue's pairing; not read):
// the command's state byte 0x904AA3 through BattleAttackCmd_States 0x64E44C -
// 0x447FE0, 0x448020, 0x4480E0, 0x448140 (the cancel, never reached on the
// route and left Capcom's). The index is not checked, as the original has it.
extern "C" void __cdecl BattleAttackCmd_Dispatch(void) { Entry(at::kAttackStates, Byte(at::kState))(); }

// original 0x447FE0: state 0. The target is Battle_DefaultTarget(3) - the
// first enemy not out - the banner is message 2 (BattleBanner_SetMessage(2,
// 0)), the pick flag 0x904AAF is set and the auto-repeat latch zeroed, and
// the state moves on. The state byte is read after the banner call.
extern "C" void __cdecl BattleAttackCmd_Begin(void) {
    const unsigned char target = g.default_target(3);
    Command()[0] = target;
    g.banner_message(2, 0);
    const unsigned char state = static_cast<unsigned char>(Byte(at::kState) + 1);
    Byte(at::kPicking) = 1;
    SetWord(At(at::kRepeatLatch), 0);
    Byte(at::kState) = state;
}

// original 0x448020: state 1. A cancel button pressed goes to state 3, a
// confirm button to state 2 (cancel first, both tested against the pressed
// word's 16 bits); otherwise the pressed directions go to Input_AutoRepeat
// and its right (0x2000) and left (0x8000) move the target (StepEnemyTarget),
// right first, each with cue 0x101.
extern "C" void __cdecl BattleAttackCmd_PickEnemy(void) {
    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        Byte(at::kState) = 3;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        Byte(at::kState) = 2;
        return;
    }
    StepEnemyTarget(g.auto_repeat(pad & 0xF000));
}

// original 0x4480E0: state 2, the confirm. Cue 0x104; the pick flag cleared;
// the command's action kind +1 = 0 and its dword +0xC |= 1; the acting
// member's state +1 = 2; one more command chosen (0x904AC3); back to the
// command step 1 with the command and its state 0.
extern "C" void __cdecl BattleAttackCmd_Confirm(void) {
    g.sound(0x104);
    unsigned char* command = Command();
    Byte(at::kPicking) = 0;
    command[1] = 0;
    command = Command();
    SetLong(command + 0xC, Long(command + 0xC) | 1);
    Acting()[1] = 2;
    const unsigned char chosen = static_cast<unsigned char>(Byte(at::kCommandsChosen) + 1);
    Byte(at::kStep) = 1;
    Byte(at::kCommandsChosen) = chosen;
    Byte(at::kCommand) = 0;
    Byte(at::kState) = 0;
}

// ===========================================================================
// Command 2: the item command
// ===========================================================================

// original 0x448180 (PSX 0x80095314 by the catalogue's pairing; not read): the
// state byte 0x904AA3 through BattleItemCmd_States 0x64E45C - 0 open
// (0x448190), 1 the list (0x448210), 2 the list closed (0x448600, left
// Capcom's), 3 the target kind (0x448630), 4 the target (0x4486C0), 5
// 0x448B80, 6 0x448C80, 7 0x4493E0, 8 0x449480, 9 0x4498F0 (the last five
// unread, not this group's). No bound, as the original has it.
extern "C" void __cdecl BattleItemCmd_Dispatch(void) { Entry(at::kItemStates, Byte(at::kState))(); }

// original 0x448190: state 0, by the sub-state 0x904AA4 (read as a dword, its
// low byte) through BattleItemCmd_OpenSteps 0x64E484 - 0x4481B0, 0x4481E0.
extern "C" void __cdecl BattleItemCmd_OpenDispatch(void) {
    Entry(at::kItemOpenSteps, static_cast<std::uint32_t>(Long(At(at::kSubState))) & 0xFF)();
}

// original 0x4481B0: the prompt. System message 0x4000 (Msg_SystemPtr: bank 1,
// message 0) goes into the battle message queue as BattleQueue_Push(2, 0xFF,
// text), and the sub-state moves on. Also entry 13 of the item-menu table
// 0x64E3EC.. (0x64E420, group CH's). The text is Capcom's Chinese, drawn by the
// queue's reader, not here.
extern "C" void __cdecl BattleItemCmd_QueuePrompt(void) {
    const unsigned char* const text = g.system_ptr(0x4000);
    g.queue_push(2, 0xFF, static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(text)));
    Byte(at::kSubState) = static_cast<unsigned char>(Byte(at::kSubState) + 1);
}

// original 0x4481E0: the list opened. The command's action kind +1 = 5;
// ItemMenu_SetupForParty (window record 16 from the saved page, top and
// cursor 0x904605..7); the sub-state 0 and the state on.
extern "C" void __cdecl BattleItemCmd_OpenList(void) {
    Command()[1] = 5;
    g.setup_for_party();
    const unsigned char state = static_cast<unsigned char>(Byte(at::kState) + 1);
    Byte(at::kSubState) = 0;
    Byte(at::kState) = state;
}

// original 0x448210: state 1, the party's item list (window record 16), once a
// frame. Nothing while the record's +3 (done) is set. Otherwise:
//   1. Record 19 (the hand) shown at x + 7 and y + 13 * (cursor - top + 2);
//      the acting member's dword +0x130 &= 0xBFFF (bit 14 and the upper half
//      cleared).
//   2. Input_AutoRepeat(pressed & 0xF00C). Left (0x8000): cue 0x101, the
//      category one back (below 0 -> 3, a signed test), request word +0x10 =
//      0x32; else right (0x2000): +0x10 = 0x31, cue, the category on (above 3
//      -> 0). Then the rows: up (0x1000) one back to 0, request +0x12 = 0xF0
//      when above the top; else down (0x4000) one on to 0x7F, +0x12 = 0x10
//      when at or past top + 7; else L1 (4) a page back, R1 (8) a page on (the
//      top at most 0x79, the cursor 0x7F at the end). Cue 0x100 when the
//      cursor moved.
//   3. The last-pushed queue entry's text = Msg_SystemPtr(Item_HelpMessage(category,
//      item)) - the item's help line; Item_HelpMessage's word is a system message id
//      here.
//   4. With a scroll request pending, nothing more. Up pressed on row 0 opens
//      record 21 above the list (cue 0x100) and goes to state 6. A cancel
//      saves the page, top and cursor to 0x904605..7, closes the list, marks
//      the queue entry's +1, clears the member's +0x125, cue 0x106, state on.
//      A confirm stores category << 8 | item in the command's word +2 and asks
//      Item_CanUse(the list's mode +8, 0, category, item): refused, cue 0x107;
//      else the three bytes saved, cue 0x103, the list closed, state 3.
// As the original has it: the directions' cursor arithmetic is on bytes
// (the up test on 0, the down cap at 0x7F); the category, the queue index,
// record 16's x and y and the pad are read after the calls before them.
extern "C" void __cdecl BattleItemCmd_List(void) {
    unsigned char* const list = List();
    if (list[3] != 0) return;
    const unsigned actor = Acting()[5];
    unsigned char* const hand = At(at::kHand);
    hand[0] = 1;
    SetWord(hand + 4, static_cast<unsigned>(Long(list + 4)) + 7);
    const unsigned top_row = list[0xB];
    unsigned char* const member = At(at::kMembers + actor * at::kMemberSize);
    SetLong(member + 0x130, Long(member + 0x130) & 0xBFFF);
    const unsigned cursor_row = list[0xC];
    const unsigned pressed = Input_Pressed & 0xF00Cu;
    SetWord(hand + 6, (cursor_row - top_row + 2) * 13 + Word(list + 6));
    const unsigned repeat = g.auto_repeat(pressed);

    if (repeat & 0x8000) {
        g.sound(0x101);
        const auto category = static_cast<unsigned char>(list[0xA] - 1);
        SetWord(list + 0x10, 0x32);
        list[0xA] = category;
        if (static_cast<signed char>(category) < 0) list[0xA] = 3;
    } else if (repeat & 0x2000) {
        SetWord(list + 0x10, 0x31);
        g.sound(0x101);
        const auto category = static_cast<unsigned char>(list[0xA] + 1);
        list[0xA] = category;
        if (category > 3) list[0xA] = 0;
    }

    unsigned char cursor = list[0xC];
    const unsigned char before = cursor;
    const unsigned up = repeat & 0x1000;
    if (up) {
        if (cursor != 0) {
            --cursor;
            list[0xC] = cursor;
        }
        if (cursor < list[0xB]) SetWord(list + 0x12, 0xF0);
    } else if (repeat & 0x4000) {
        if (cursor < 0x7F) {
            ++cursor;
            list[0xC] = cursor;
        }
        if (static_cast<int>(list[0xC]) >= static_cast<int>(list[0xB]) + 7) SetWord(list + 0x12, 0x10);
    } else if (repeat & 4) {
        auto top = list[0xB];
        if (top == 0) {
            cursor = 0;
            list[0xC] = cursor;
        } else if (top < 7) {
            cursor = static_cast<unsigned char>(cursor - top);
            list[0xB] = 0;
            list[0xC] = cursor;
        } else {
            cursor = static_cast<unsigned char>(cursor - 7);
            top = static_cast<unsigned char>(top - 7);
            list[0xC] = cursor;
            list[0xB] = top;
        }
    } else if (repeat & 8) {
        auto top = list[0xB];
        if (top == 0x79) {
            cursor = 0x7F;
            list[0xC] = cursor;
        } else if (top > 0x72) {
            list[0xB] = 0x79;
            cursor = static_cast<unsigned char>(cursor + (0x79 - top));
            list[0xC] = cursor;
        } else {
            cursor = static_cast<unsigned char>(cursor + 7);
            top = static_cast<unsigned char>(top + 7);
            list[0xC] = cursor;
            list[0xB] = top;
        }
    }
    if (before != cursor) g.sound(0x100);

    const unsigned entry = LastQueueEntry();
    const unsigned category = list[0xA];
    const unsigned item = ListItem();
    const unsigned message = g.item_price(category, item);
    const unsigned char* const help = g.system_ptr(message);
    SetLong(At(at::kQueue + 4 + entry), static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(help)));
    if (Word(list + 0x12) != 0) return;

    if (up && before == 0) {
        g.sound(0x100);
        const std::uint32_t x = static_cast<std::uint32_t>(Long(list + 4)) + 0x20;
        const unsigned y = Word(list + 6) - 0x16u;
        unsigned char* const above = At(at::kAbove);
        above[1] = 8;
        above[2] = 5;
        above[0] = 1;
        SetWord(above + 4, x);
        SetWord(above + 6, y);
        above[0xA] = 0;
        above[0xB] = 0xFF;
        Byte(at::kRecord4) = 0;
        Byte(at::kState) = 6;
        return;
    }

    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        const unsigned char page = list[0xA], top = list[0xB], row = list[0xC];
        Byte(at::kSavedList) = page;
        unsigned char* const acting = Acting();
        list[3] = 1;
        hand[0] = 0;
        At(at::kQueue + 1 + entry)[0] = 1;
        Byte(at::kSavedList + 1) = top;
        Byte(at::kSavedList + 2) = row;
        acting[0x125] = 0;
        g.sound(0x106);
        Byte(at::kState) = static_cast<unsigned char>(Byte(at::kState) + 1);
        return;
    }
    if (!(Field_ConfirmButtons & pad)) return;

    const unsigned chosen_category = list[0xA];
    const unsigned chosen_item = ListItem();
    SetWord(Command() + 2, (chosen_category << 8) + chosen_item);
    const unsigned use_category = list[0xA];
    const unsigned use_item = Command()[2];
    const unsigned mode = list[8];
    if (g.item_can_use(mode, 0, use_category, use_item) == 0) {
        g.sound(0x107);
        return;
    }
    const unsigned char page = list[0xA], top = list[0xB], row = list[0xC];
    Byte(at::kSavedList) = page;
    Byte(at::kSavedList + 1) = top;
    Byte(at::kSavedList + 2) = row;
    g.sound(0x103);
    list[3] = 1;
    hand[0] = 0;
    Byte(at::kState) = 3;
}

// original 0x448630: state 3, the item's target kind from its flag byte
// (0x591810). Bit 0x40: state 4, or 5 with bit 0x10 too (the sub-state left
// as it is, 0 from the opening). Else bit 0x80: target 0xC0, state 4, sub-state
// 3 (the commit). Else the target is 0x40 (bits 0x10 and 0x20), 0x80 (0x10
// alone) or the acting member itself (its +5), and state 4, sub-state 3.
extern "C" void __cdecl BattleItemCmd_TargetKind(void) {
    const unsigned char flags = ListItemFlags();
    if (flags & 0x40) {
        Byte(at::kState) = static_cast<unsigned char>(((flags & 0x10) | 0x40) >> 4);
        return;
    }
    if (flags & 0x80) {
        Command()[0] = 0xC0;
        Byte(at::kState) = 4;
        Byte(at::kSubState) = 3;
        return;
    }
    unsigned char target;
    if (flags & 0x10) target = (flags & 0x20) ? 0x40 : 0x80;
    else target = Acting()[5];
    Command()[0] = target;
    Byte(at::kState) = 4;
    Byte(at::kSubState) = 3;
}

// original 0x4486C0: state 4, by the sub-state 0x904AA4 (a dword's low byte)
// through BattleItemCmd_TargetSteps 0x64E48C - 0x4486E0 (begin), 0x448780
// (an enemy), 0x4488C0 (a member), 0x448A70 (commit), 0x448B40 (back to the
// list; unread, not this group's).
extern "C" void __cdecl BattleItemCmd_TargetDispatch(void) {
    Entry(at::kItemTargetSteps, static_cast<std::uint32_t>(Long(At(at::kSubState))) & 0xFF)();
}

// original 0x4486E0: the target pick begun. Flag bit 0x20: the first enemy not
// out (Battle_DefaultTarget(3)), sub-state 1. Else, while Battle_ReturnTrue
// answers al non-zero (always, in this port), target 0 outright; otherwise
// Battle_DefaultTarget(0); sub-state 2. Both: the latch zeroed, the pick flag
// set.
extern "C" void __cdecl BattleItemCmd_TargetBegin(void) {
    const unsigned char flags = ListItemFlags();
    if (flags & 0x20) {
        const unsigned char target = g.default_target(3);
        Command()[0] = target;
        Byte(at::kSubState) = 1;
        SetWord(At(at::kRepeatLatch), 0);
        Byte(at::kPicking) = 1;
        return;
    }
    if (g.return_true() != 0) {
        Command()[0] = 0;
    } else {
        const unsigned char target = g.default_target(0);
        Command()[0] = target;
    }
    Byte(at::kSubState) = 2;
    SetWord(At(at::kRepeatLatch), 0);
    Byte(at::kPicking) = 1;
}

// original 0x448780: sub-state 1, an enemy. Cancel -> sub-state 4, confirm ->
// 3. Otherwise Input_AutoRepeat(pressed & 0xF000): up or down (0x5000) with
// the item's flag bit 0x80 crosses to the party - target 0 (Battle_ReturnTrue)
// or Battle_DefaultTarget(0), sub-state 2, cue 0x101 - and ends there; then
// right and left as the attack's (StepEnemyTarget).
extern "C" void __cdecl BattleItemCmd_PickEnemy(void) {
    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        Byte(at::kSubState) = 4;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        Byte(at::kSubState) = 3;
        return;
    }
    const unsigned repeat = g.auto_repeat(pad & 0xF000);
    if ((repeat & 0x5000) && (ListItemFlags() & 0x80)) {
        if (g.return_true() != 0) {
            Command()[0] = 0;
        } else {
            const unsigned char target = g.default_target(0);
            Command()[0] = target;
        }
        Byte(at::kSubState) = static_cast<unsigned char>(Byte(at::kSubState) + 1);
        g.sound(0x101);
        return;
    }
    StepEnemyTarget(repeat);
}

// original 0x4488C0: sub-state 2, a member. Cancel -> 4, confirm -> 3.
// Otherwise Input_AutoRepeat(pressed & 0xF000): up or down with flag bit 0x80
// crosses to the enemies (Battle_DefaultTarget(3), sub-state 1, cue 0x101) and
// ends there. Right (0x2000): the target one on, wrapped to 0 .. party count -
// 1 (0x904AB0's low byte) - taken as it is while Battle_ReturnTrue answers
// non-zero, else through Battle_DefaultTarget - cue 0x101. Left (0x8000): one
// back the same way, through 0x4457F0, cue 0x101.
// As the original has it: Battle_ReturnTrue is asked before each, and the
// command pointer and the party count are read after it.
extern "C" void __cdecl BattleItemCmd_PickMember(void) {
    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        Byte(at::kSubState) = 4;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        Byte(at::kSubState) = 3;
        return;
    }
    const unsigned repeat = g.auto_repeat(pad & 0xF000);
    if ((repeat & 0x5000) && (ListItemFlags() & 0x80)) {
        const unsigned char target = g.default_target(3);
        Command()[0] = target;
        Byte(at::kSubState) = static_cast<unsigned char>(Byte(at::kSubState) - 1);
        g.sound(0x101);
        return;
    }
    const auto party_high = [] { return static_cast<long>(static_cast<std::uint32_t>(Long(At(at::kPartyCount))) & 0xFF) - 1; };
    if (repeat & 0x2000) {
        if (g.return_true() != 0) {
            const long high = party_high();
            const long value = static_cast<long>(static_cast<signed char>(Command()[0])) + 1;
            const long wrapped = g.wrap_index(high, 0, value);
            Command()[0] = static_cast<unsigned char>(wrapped);
        } else {
            const long high = party_high();
            const long value = static_cast<long>(static_cast<signed char>(Command()[0])) + 1;
            const long wrapped = g.wrap_index(high, 0, value);
            const unsigned char target = g.default_target(static_cast<unsigned>(wrapped));
            Command()[0] = target;
        }
        g.sound(0x101);
    }
    if (repeat & 0x8000) {
        if (g.return_true() != 0) {
            const long high = party_high();
            const long value = static_cast<long>(static_cast<signed char>(Command()[0])) - 1;
            const long wrapped = g.wrap_index(high, 0, value);
            Command()[0] = static_cast<unsigned char>(wrapped);
        } else {
            const long high = party_high();
            const long value = static_cast<long>(static_cast<signed char>(Command()[0])) - 1;
            const long wrapped = g.wrap_index(high, 0, value);
            const unsigned char target = g.prev_target(static_cast<unsigned>(wrapped));
            Command()[0] = target;
        }
        g.sound(0x101);
    }
}

// original 0x448A70: sub-state 3, the commit. Cue 0x104; the pick flag
// cleared; the command's action kind +1 = 5 and its +0xA the list's cursor
// row; the acting member's state +1 = 2; one more command chosen; the
// last-pushed queue entry's +1 = 1. The item is spent here, at the choice: its
// count one less, or, at 1 or 0, its id and count both zeroed. Then
// ItemMenu_FreeWindows, and back to the command step 1 with the command, its
// state and sub-state 0.
// As the original has it: the category is not checked (category 4's count
// pointer 0x656B14[4] is 0; the list's left and right keep it to 0..3).
extern "C" void __cdecl BattleItemCmd_Commit(void) {
    g.sound(0x104);
    unsigned char* command = Command();
    Byte(at::kPicking) = 0;
    command[1] = 5;
    Acting()[1] = 2;
    const unsigned char chosen = static_cast<unsigned char>(Byte(at::kCommandsChosen) + 1);
    command = Command();
    const unsigned char row = List()[0xC];
    Byte(at::kCommandsChosen) = chosen;
    At(at::kQueue + 1 + LastQueueEntry())[0] = 1;
    command[0xA] = row;
    const unsigned index = static_cast<std::uint32_t>(Long(List() + 0xC)) & 0xFF;
    const unsigned category = List()[0xA];
    unsigned char* const count = PtrAt(at::kInventoryCounts + 4 * category) + index;
    if (*count > 1) {
        *count = static_cast<unsigned char>(*count - 1);
    } else {
        PtrAt(at::kInventoryIds + 4 * category)[index] = 0;
        const unsigned index2 = static_cast<std::uint32_t>(Long(List() + 0xC)) & 0xFF;
        const unsigned category2 = List()[0xA];
        PtrAt(at::kInventoryCounts + 4 * category2)[index2] = 0;
    }
    g.free_windows();
    Byte(at::kCommand) = 0;
    Byte(at::kState) = 0;
    Byte(at::kSubState) = 0;
    Byte(at::kStep) = 1;
}

// ===========================================================================

void BattleMenuStates_Inject() {
    if (bof3::WantsShadow("battle_menu_states")) battle_menu_states::SelfTest();
    BOF3_INJECT(BattleAttackCmd_Dispatch);
    BOF3_INJECT(BattleAttackCmd_Begin);
    BOF3_INJECT(BattleAttackCmd_PickEnemy);
    BOF3_INJECT(BattleAttackCmd_Confirm);
    BOF3_INJECT(BattleItemCmd_Dispatch);
    BOF3_INJECT(BattleItemCmd_OpenDispatch);
    BOF3_INJECT(BattleItemCmd_QueuePrompt);
    BOF3_INJECT(BattleItemCmd_OpenList);
    BOF3_INJECT(BattleItemCmd_List);
    BOF3_INJECT(BattleItemCmd_TargetKind);
    BOF3_INJECT(BattleItemCmd_TargetDispatch);
    BOF3_INJECT(BattleItemCmd_TargetBegin);
    BOF3_INJECT(BattleItemCmd_PickEnemy);
    BOF3_INJECT(BattleItemCmd_PickMember);
    BOF3_INJECT(BattleItemCmd_Commit);
}
