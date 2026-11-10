// Moved here by tools/move_funcs.py from battle_actor_copies.cpp, battle_e4.cpp, battle_menu_states.cpp, battle_misc.cpp, rest_3b.cpp (docs/source-layout-round.md).
#include "game/battle/battle_item_cmd.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/battle/battle_item_cmd_callees.h"
#include "game/battle_actor_copies_callees.h"
#include "game/battle_e4_callees.h"
#include "game/battle_menu_states_callees.h"
#include "game/battle_misc.h"
#include "game/battle_misc_callees.h"
#include "game/battle_text.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3b_callees.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = battle_item_cmd::at;

}  // namespace

using namespace battle_actor_copies;

using namespace battle_menu_states;

namespace {

using namespace battle_misc;

}  // namespace

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace battle_actor_copies {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

namespace {

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace battle_actor_copies

namespace {

unsigned char& BBattleActorCopies(U address) { return battle_actor_copies::At(address)[0]; }

// A pointer the original keeps in a dword of .bss, read afresh.
unsigned char* PtrAt(battle_actor_copies::U address) { return battle_actor_copies::At(static_cast<battle_actor_copies::U>(battle_actor_copies::Long(battle_actor_copies::At(address)))); }

// 0x939FA0 points at the menu member's +0x124, its action record
// (records::ActionRecord; BattleInput_NextMember stores it, battle_phases.cpp).
unsigned char* Command() { return PtrAt(::bof3::Img(battle_actor_copies::at::kCommand.v)); }

unsigned char* Acting() { return reinterpret_cast<unsigned char*>(BattleE4_MenuActor.Native()); }

unsigned char ItemFlags(unsigned id) { return BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemRecords.v) + id * battle_actor_copies::at::kItemRecordSize); }

unsigned char Low(battle_actor_copies::U eax) { return static_cast<unsigned char>(eax); }

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
bool PickHead(battle_actor_copies::U& held) {
    const unsigned pressed = Input_Pressed;
    if (pressed & Field_CancelButtons) {
        BattleE4_Step4 = 4;
        return false;
    }
    if (pressed & Field_ConfirmButtons) {
        BattleE4_Step4 = 3;
        return false;
    }
    held = battle_actor_copies::g.auto_repeat(pressed & 0xF000);
    return true;
}

// The target byte (+0 of the command, read as a signed byte) stepped by
// `delta` and wrapped by Battle_WrapIndex inside low .. high; the whole
// dword goes on. Everything is read before the call, as the original does.
battle_actor_copies::U Stepped(battle_actor_copies::U high, battle_actor_copies::U low, int delta) {
    const int value = static_cast<signed char>(Command()[0]) + delta;
    return battle_actor_copies::g.wrap_index(high, low, static_cast<battle_actor_copies::U>(value));
}

// The enemies' side of a pick: 0x2000 the next enemy up through
// Battle_DefaultTarget, 0x8000 the next down through 0x4457F0, among actors
// 3 .. enemy count + 2; each with cue 0x101. Both may run in one frame.
void PickAmongEnemies(battle_actor_copies::U held) {
    if (held & 0x2000) {
        const battle_actor_copies::U high = static_cast<std::uint32_t>(Battle_EnemyCount) + 2;
        const unsigned char t = Low(battle_actor_copies::g.default_target(Stepped(high, 3, +1)));
        Command()[0] = t;
        battle_actor_copies::g.play_effect(0x101);
    }
    if (held & 0x8000) {
        const battle_actor_copies::U high = static_cast<std::uint32_t>(Battle_EnemyCount) + 2;
        const unsigned char t = Low(battle_actor_copies::g.prev_target(Stepped(high, 3, -1)));
        Command()[0] = t;
        battle_actor_copies::g.play_effect(0x101);
    }
}

// The item under the item window's cursor: Char_AbilityList(the window's
// actor, its page, 1) - the actor's inventory page - read at the cursor.
// The actor and the page are read before the call, the cursor after.
unsigned ItemUnderCursor() {
    const unsigned page = Window16_Page;
    const unsigned actor = static_cast<unsigned char>(BattleActorCopies_ItemActor);
    const unsigned char* const list = battle_actor_copies::At(battle_actor_copies::g.ability_list(actor, page, 1));
    return list[BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v))];
}

// The window's page, scroll and cursor kept for the actor 0x8033AA names, in
// the original's order: page and scroll read, the stores, the cursor read
// between its neighbours' stores.
void KeepPage(unsigned char page, unsigned char scroll, unsigned owner) {
    BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kActorPages.v) + owner * 3) = page;
    const unsigned char cursor = BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v));
    BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kActorPages.v) + owner * 3 + 1) = scroll;
    BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kActorPages.v) + owner * 3 + 2) = cursor;
}

using U = std::uint32_t;

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* Ptr(U cell) { return move_script::At(static_cast<U>(move_script::Long(move_script::At(cell)))); }

// The party's records by a byte (0..2 in play), the enemies' objects by an
// actor byte (3..10: object actor - 3), as every original indexes them - the
// byte alone, the product wrapping as 32-bit arithmetic does.
unsigned char* PartyBattleE4(unsigned member) { return move_script::At(::bof3::Img(ObjTrio_addr) + (member & 0xFF) * battle_e4::at::kPartyStride); }

unsigned char* CharRecord(unsigned index) { return move_script::At(::bof3::Img(CharacterRecords_addr) + (index & 0xFF) * battle_e4::at::kCharStride); }

unsigned char* MenuActor() { return reinterpret_cast<unsigned char*>(BattleE4_MenuActor.Native()); }

// 0x939FA0 points at the menu member's +0x124, its action record
// (records::ActionRecord; BattleInput_NextMember stores it, battle_phases.cpp).
unsigned char* CommandBattleE4() { return Ptr(::bof3::Img(battle_e4::at::kCommand.v)); }

std::int32_t S32BattleE4(U v) { return static_cast<std::int32_t>(v); }

// The last-pushed entry of the battle message queue: ((write - 1) & 15) * 8.
unsigned LastQueueEntry() { return ((BattleActorCopies_QueueWrite - 1u) & 0xF) * 8; }

using ItemFlagsFn = unsigned char (__cdecl*)(unsigned, unsigned);

using VoidFn = void (__cdecl*)();

using ActorFn = void (__cdecl*)(unsigned);

// The item's flag byte (0x591810, Item_UseFlags - ours since group TWO, 2026-10-06).
unsigned char ItemFlagsBattleE4(unsigned category, unsigned item) { return BH_AT(ItemFlagsFn, battle_e4::at::kItemFlags.v)(category, item); }

// jmp [table + 4 * (dword 0x904AA4 & 0xFF)]: the table's `entries`
// handlers, a Fatal past them (the original jumps through the dword after).
void DispatchStep4(const char* who, U table, unsigned entries) {
    const unsigned state = static_cast<U>(move_script::Long(move_script::At(::bof3::Img(BattleE4_Step4_addr)))) & 0xFF;
    if (state >= entries)
        bof3::Fatal("%s: the sub-state 0x904AA4 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e4.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(move_script::Long(move_script::At(table + 4 * state)))))();
}

// The members' records refreshed from their character records after an
// equipment change (BattleEquip_Apply, BattleEquip_RemoveSlot, both the same
// code): for each of the party count's members, +0x92..+0x97 from the
// character's +0x12..+0x17 and the eight dwords +0xC0 from its +0x20;
// Formation_ApplyStatMods; the present members' +0xC0 block into +0xA0; BE5's
// 0x44FDE0; BE6's 0x453300 for each member, the count read again after each.
void RefreshMembers() {
    const unsigned n = static_cast<U>(move_script::Long(move_script::At(::bof3::Img(battle_e4::at::kPartyCount.v)))) & 0xFF;
    for (unsigned k = 0; k < n; ++k) {
        unsigned char* const p = PartyBattleE4(k);
        auto* const member_rec = rv::As<records::PartyMember>(p);
        const unsigned char* const r = CharRecord(member_rec->char_index);
        for (unsigned i = 0; i < 6; ++i) p[0x92 + i] = r[0x12 + i];
        std::memcpy(p + 0xC0, r + 0x20, 0x20);
    }
    BH_CALL(Formation_ApplyStatMods)();
    for (unsigned k = 0; k < 3; ++k) {
        unsigned char* const p = PartyBattleE4(k);
        auto* const member_rec = rv::As<records::PartyMember>(p);
        if (member_rec->flags & 1) std::memcpy(p + 0xA0, p + 0xC0, 0x20);
    }
    BH_AT(VoidFn, battle_e4::at::kAfterEquip.v)();
    unsigned char n2 = B(::bof3::Img(battle_e4::at::kPartyCount.v));
    if (n2 == 0) return;
    for (unsigned char m = 0;;) {
        BH_AT(ActorFn, battle_e4::at::kMemberRefresh.v)(m);
        n2 = B(::bof3::Img(battle_e4::at::kPartyCount.v));
        ++m;
        if (m >= n2) break;
    }
}

// The character record of the member the slots window shows: record 18's +0xC
// through 0x904065 and 0x66972C.
unsigned char* SlotsCharacter() {
    const unsigned member = static_cast<U>(move_script::Long(move_script::At(::bof3::Img(Window18_addr) + 0xC))) & 0xFF;
    return CharRecord(Character_IndexById[Cond_Flags[0xd5 + member]]);
}

// Up (0x1000) and down (0x4000) on the six slots: record 18's cursor +0xA
// one back (below 0 to 5) or on (above 5 to 0), cue 0x100 each.
void SlotCursor(unsigned repeat) {
    if (repeat & 0x1000) {
        BH_CALL(Sound_PlayEffect)(0x100);
        const auto c = static_cast<unsigned char>(B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA) - 1);
        B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA) = c;
        if (static_cast<signed char>(c) < 0) B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA) = 5;
    }
    if (repeat & 0x4000) {
        BH_CALL(Sound_PlayEffect)(0x100);
        const auto c = static_cast<unsigned char>(B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA) + 1);
        B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA) = c;
        if (static_cast<signed char>(c) > 5) B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA) = 0;
    }
}

// The hand beside the six slots (record 19 on unless record 18's +3 is set):
// x = record 18's x + 6, y = 13 * cursor + record 18's y + 0x57.
void SlotHand() {
    if (B(::bof3::Img(battle_e4::at::kSlots.v) + 3) == 0) {
        WindowRecords[19].in_use = 1;
        WindowRecords[0x13].x = static_cast<unsigned>(move_script::Long(move_script::At(::bof3::Img(Window18_addr) + 4))) + 6;
        WindowRecords[0x13].y = 13u * B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA) + WindowRecords[0x12].y + 0x57;
    } else {
        WindowRecords[19].in_use = 0;
    }
}

// The equipped item's word (the member's +0x126: category << 8 | item) and its
// flag byte, for the member choosing.
unsigned MemberItem() { return rv::As<records::PartyMember>(PartyBattleE4(rv::As<records::PartyMember>(MenuActor())->actor))->queued_id; }

// A side's target by the flag byte: bit 0x20 the enemies (0x40), else the
// party (0x80) - neg / sbb / and 0xC0 / add 0x80.
unsigned char SideTarget(unsigned char flags) { return (flags & 0x20) ? 0x40 : 0x80; }

// Left and right on the enemy side (0x449540), as BattleItemCmd_PickEnemy's:
// the target one on / one back wrapped to 3 .. enemy count + 2, through
// Battle_DefaultTarget / Battle_PrevTarget; the command pointer read before
// and again after the calls.
void StepEnemyTarget(unsigned repeat) {
    if (repeat & 0x2000) {
        const long high = Battle_EnemyCount + 2;
        const long value = static_cast<long>(static_cast<signed char>(CommandBattleE4()[0])) + 1;
        const long wrapped = BH_CALL(Battle_WrapIndex)(high, 3, value);
        const unsigned char t = BH_CALL(Battle_DefaultTarget)(static_cast<unsigned>(wrapped));
        CommandBattleE4()[0] = t;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
    if (repeat & 0x8000) {
        const long high = Battle_EnemyCount + 2;
        const long value = static_cast<long>(static_cast<signed char>(CommandBattleE4()[0])) - 1;
        const long wrapped = BH_CALL(Battle_WrapIndex)(high, 3, value);
        const unsigned char t = BH_CALL(Battle_PrevTarget)(static_cast<unsigned>(wrapped));
        CommandBattleE4()[0] = t;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
}

// The escape's field step: the kind-2 x / z high words (0x905E66, 0x905E62)
// moved by the pair of s8 at 0x64E4F4 + 2 * (dword 0x904AAC & 0xFF), `sign`
// +1 or -1, 16-bit.
void FieldStep(int sign) {
    const unsigned d = static_cast<U>(move_script::Long(move_script::At(::bof3::Img(battle_e4::at::kFaceStep.v)))) & 0xFF;
    const auto dx = static_cast<short>(static_cast<signed char>(B(::bof3::Img(battle_e4::at::kFacePairs.v) + 2 * d)));
    const auto dz = static_cast<short>(static_cast<signed char>(B(::bof3::Img(battle_e4::at::kFacePairs.v) + 2 * d + 1)));
    move_script::SetWord(move_script::At(::bof3::Img(Field_Kind2X_addr) + 2), static_cast<unsigned>(move_script::Word(move_script::At(::bof3::Img(Field_Kind2X_addr) + 2)) + sign * dx));
    move_script::SetWord(move_script::At(::bof3::Img(Field_Kind2Z_addr) + 2), static_cast<unsigned>(move_script::Word(move_script::At(::bof3::Img(Field_Kind2Z_addr) + 2)) + sign * dz));
}

// The two option messages above the list (Msg 0x2F / 0x31, or 0x30 / 0x32
// with `base` 0x30, by record 21's option +0xA) into the last queue entry.
const unsigned char* OptionMessage(unsigned base) { return BH_CALL(Msg_SystemPtr)(B(::bof3::Img(battle_e4::at::kAbove.v) + 0xA) ? base + 2 : base); }

}  // namespace

namespace battle_menu_states {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

}  // namespace battle_menu_states

namespace {

// A pointer the original keeps in a dword of .data / .bss, read afresh.
unsigned char* PtrAtBattleMenuStates(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(battle_menu_states::Long(battle_menu_states::At(address)))));
}

unsigned char& Byte(std::uint32_t address) { return battle_menu_states::At(address)[0]; }

// The command being built and the acting member's record, each re-read
// wherever the original re-reads it.
// 0x939FA0 points at the menu member's +0x124, its action record
// (records::ActionRecord; BattleInput_NextMember stores it, battle_phases.cpp).
unsigned char* CommandBattleMenuStates() { return PtrAtBattleMenuStates(::bof3::Img(battle_menu_states::at::kCommandRecord.v)); }

unsigned char* List() { return battle_menu_states::At(::bof3::Img(Window16_addr)); }

// Entry i of a dispatch table in .data, read at the dispatch: no bound, as
// the original has it - an index past the table reads the next one's.
Handler Entry(std::uint32_t table, unsigned i) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(battle_menu_states::Long(battle_menu_states::At(table + 4 * i)))));
}

// The list's item: the id at the cursor row of the category's id list. The
// category, the cursor and the list pointer are read in that order, with no
// call between.
unsigned ListItem() {
    const unsigned category = List()[0xA];
    const unsigned row = List()[0xC];
    return PtrAtBattleMenuStates(::bof3::Img(battle_menu_states::at::kInventoryIds.v) + 4 * category)[row];
}

// The list's item's flag byte: 0x591810(category, item).
unsigned char ListItemFlags() {
    const unsigned category = List()[0xA];
    const unsigned item = ListItem();
    return battle_menu_states::g.item_flags(category, item);
}

// Left and right on the enemy side: Battle_DefaultTarget / 0x4457F0 from the
// target one on / one back, wrapped to 3 .. enemy count + 2 by
// Battle_WrapIndex. Both of Battle_WrapIndex's results are passed on whole,
// as the original pushes eax. The command pointer is read before and again
// after the calls; the enemy count after Input_AutoRepeat (and, for the
// second, after the first's sound).
void StepEnemyTargetBattleMenuStates(unsigned repeat) {
    if (repeat & 0x2000) {
        const long high = Battle_EnemyCount + 2;
        const long value = static_cast<long>(static_cast<signed char>(CommandBattleMenuStates()[0])) + 1;
        const long wrapped = battle_menu_states::g.wrap_index(high, 3, value);
        const unsigned char target = battle_menu_states::g.default_target(static_cast<unsigned>(wrapped));
        CommandBattleMenuStates()[0] = target;
        battle_menu_states::g.sound(0x101);
    }
    if (repeat & 0x8000) {
        const long high = Battle_EnemyCount + 2;
        const long value = static_cast<long>(static_cast<signed char>(CommandBattleMenuStates()[0])) - 1;
        const long wrapped = battle_menu_states::g.wrap_index(high, 3, value);
        const unsigned char target = battle_menu_states::g.prev_target(static_cast<unsigned>(wrapped));
        CommandBattleMenuStates()[0] = target;
        battle_menu_states::g.sound(0x101);
    }
}

}  // namespace

namespace battle_misc {

namespace {

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace battle_misc

namespace {

unsigned char* AtBattleMisc(battle_misc::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

battle_misc::U LongBattleMisc(const unsigned char* p) {
    battle_misc::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

U L(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }

unsigned char* PtrRest3b(U cell) { return move_script::At(L(cell)); }

unsigned char* AbilityRow(unsigned id) { return move_script::At(::bof3::Img(rest_3b::at::kAbilityRows.v) + (id & 0xFFFF) * rest_3b::at::kAbilityStride); }

// jmp [table + 4 * byte]: the byte below `entries`, else a Fatal where the
// original jumps through the dword after its table.
void DispatchRest3b(const char* who, U cell, U table, unsigned entries) {
    const unsigned state = B(cell);
    if (state >= entries)
        bof3::Fatal("%s: the state byte 0x%X is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/rest_3b.md section 5)",
                    who, (unsigned)::bof3::VaOf(cell), state, entries, (unsigned)::bof3::VaOf(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * state)))();
}

// The cancels' common head: cue 0x106, then (for the two that close a
// window) records 3's and 2's +3 = 1, then BattleBanner_ShowName(the menu
// actor, the pointer read after the cue) and the pick flag cleared.
void CancelHead(bool windows) {
    BH_CALL(Sound_PlayEffect)(0x106);
    if (windows) {
        WindowRecords[3].state = 1;
        WindowRecords[2].state = 1;
    }
    BH_CALL(BattleBanner_ShowName)(reinterpret_cast<unsigned char*>(BattleE4_MenuActor.Native()));
    BattleE4_Picking = 0;
}

// The closing waits (BattleItem_Steps' 2 and BattleItemCmd_States' 2): once
// record 16's +3 is 0, step 1, the command and step 0, and a tail jump to
// ItemMenu_FreeWindows.
void CloseWait() {
    if (WindowRecords[16].state != 0) return;
    BattleActions_Step = 1;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BH_CALL(ItemMenu_FreeWindows)();
}

}  // namespace

// battle_menu_states's callee seam, moved here from battle_menu_states.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace battle_menu_states {
namespace {

template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

}  // namespace

const Callees kOriginals = {
    Battle_DefaultTarget,
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(kPrevTarget.v)),
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
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kItemFlags.v)),
    Battle_ReturnTrue,
};
Callees g = kOriginals;

}  // namespace battle_menu_states

// original 0x447140: entry (dword 0x904AA4 & 0xFF) of BattleTarget_Picks
// 0x64E3F4 - 0x447160, 0x447190, 0x447290 (the party's side, R3B's BattleTarget_PickParty),
// 0x447390, 0x4473E0 (the cancel, R3B's BattleTarget_Cancel).
extern "C" void __cdecl BattleTarget_PickDispatch(void) { Dispatch(BattleTarget_Picks, BattleE4_Step4); }

// original 0x447160: the target starts on Battle_DefaultTarget(3) - the first
// enemy able to be targeted - stored at the command's +0 (the pointer read
// after the call); then 0x904AAF = 1, the repeat latch 0x7E01B8 = 0 and the
// pick state counted on to 1 (read after the target's store).
extern "C" void __cdecl BattleTarget_Begin(void) {
    const unsigned char t = Low(battle_actor_copies::g.default_target(3));
    Command()[0] = t;
    const unsigned char sub = BattleE4_Step4;
    BattleE4_Picking = 1;
    battle_actor_copies::SetWord(battle_actor_copies::At(::bof3::Img(battle_actor_copies::at::kRepeatLatch.v)), 0);
    BattleE4_Step4 = static_cast<unsigned char>(sub + 1);
}

// original 0x447190: the pick among the enemies. The common head (cancel:
// sub 4, confirm: sub 3). Then 0x5000 (up or down, by the pad's bits) moves
// to the party's side - Battle_DefaultTarget(0), sub counted on to 2 before
// cue 0x101 - and ends; else 0x2000 and 0x8000 step among the enemies.
extern "C" void __cdecl BattleTarget_PickEnemy(void) {
    battle_actor_copies::U held;
    if (!PickHead(held)) return;
    if (held & 0x5000) {
        const unsigned char t = Low(battle_actor_copies::g.default_target(0));
        Command()[0] = t;
        BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 + 1);
        battle_actor_copies::g.play_effect(0x101);
        return;
    }
    PickAmongEnemies(held);
}

// original 0x447290 (BattleTarget_Picks entry 2): the pick among the party,
// BattleTarget_PickEnemy's twin. Input_Pressed read once: cancel -> sub-state
// 4, confirm -> 3. Else Input_AutoRepeat(it & 0xF000): 0x5000 back to the
// enemies (Battle_DefaultTarget(3) into the command's +0, the sub-state one
// down, cue 0x101) and done; else 0x2000 Battle_DefaultTarget(
// Battle_WrapIndex(count - 1, 0, target + 1)), and 0x8000
// Battle_PrevTarget(Battle_WrapIndex(count - 1, 0, target - 1)), each into
// the command's +0 (the pointer read again after the calls) with cue 0x101;
// both may run. The count is the byte 0x904AB0 (the party's), the target the
// command's +0 signed.
extern "C" void __cdecl BattleTarget_PickParty(void) {
    const unsigned pressed = Input_Pressed;
    if (Field_CancelButtons & pressed) {
        BattleE4_Step4 = 4;
        return;
    }
    if (Field_ConfirmButtons & pressed) {
        BattleE4_Step4 = 3;
        return;
    }
    const U held = BH_CALL(Input_AutoRepeat)(pressed & 0xF000);
    if (held & 0x5000) {
        const unsigned char t = BH_CALL(Battle_DefaultTarget)(3);
        PtrRest3b(::bof3::Img(rest_3b::at::kCommand.v))[0] = t;
        BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 - 1);
        BH_CALL(Sound_PlayEffect)(0x101);
        return;
    }
    if (held & 0x2000) {
        const long value = static_cast<signed char>(PtrRest3b(::bof3::Img(rest_3b::at::kCommand.v))[0]) + 1;
        const long high = static_cast<long>(B(::bof3::Img(rest_3b::at::kPartyCount.v))) - 1;
        const long wrapped = BH_CALL(Battle_WrapIndex)(high, 0, value);
        const unsigned char t = BH_CALL(Battle_DefaultTarget)(static_cast<unsigned>(wrapped));
        PtrRest3b(::bof3::Img(rest_3b::at::kCommand.v))[0] = t;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
    if (held & 0x8000) {
        const long value = static_cast<signed char>(PtrRest3b(::bof3::Img(rest_3b::at::kCommand.v))[0]) - 1;
        const long high = static_cast<long>(B(::bof3::Img(rest_3b::at::kPartyCount.v))) - 1;
        const long wrapped = BH_CALL(Battle_WrapIndex)(high, 0, value);
        const unsigned char t = BH_CALL(Battle_PrevTarget)(static_cast<unsigned>(wrapped));
        PtrRest3b(::bof3::Img(rest_3b::at::kCommand.v))[0] = t;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
}

// original 0x447390: the target confirmed. Cue 0x104; 0x904AAF = 0; the
// command's +1 = 1 (the pointer read after the cue); the acting context's +1 =
// 2 (its pointer read after that store); 0x904AC3 counted on; phase 1 = 1 and
// 0x904AA2..0x904AA4 = 0 - back to the command menu's next actor.
extern "C" void __cdecl BattleTarget_Confirm(void) {
    battle_actor_copies::g.play_effect(0x104);
    unsigned char* const c = Command();
    BattleE4_Picking = 0;
    c[1] = 1;
    Acting()[1] = 2;
    const unsigned char n = static_cast<unsigned char>(BattleActorCopies_EntryCount);
    BattleActions_Step = 1;
    BattleActorCopies_EntryCount = static_cast<unsigned char>(n + 1);
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
}

// original 0x4473E0 (BattleTarget_Picks entry 4): the pick cancelled - back
// to the command menu: step 2, the command, step and sub-state 0.
extern "C" void __cdecl BattleTarget_Cancel(void) {
    CancelHead(true);
    BattleActions_Step = 2;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
}

// original 0x447430 (the catalogue pairs it with PSX 0x80093F00, not read):
// entry 0x904AA3 of BattleItem_Steps 0x64E408 - 0x447440, 0x4474B0, 0x447880
// (the closing window, R3B's BattleItem_CloseWait), 0x4478B0, 0x447940, 0x447D70 (R3B's BattleItem_SideDispatch).
extern "C" void __cdecl BattleItem_Dispatch(void) { Dispatch(BattleItem_Steps, BattleE4_Step3); }

// original 0x447440: entry (dword 0x904AA4 & 0xFF) of BattleItem_OpenSteps
// 0x64E420 - 0x4481B0 (group CI's: a queued system message 0x4000), 0x447460.
extern "C" void __cdecl BattleItem_OpenDispatch(void) { Dispatch(BattleItem_OpenSteps, BattleE4_Step4); }

// original 0x447460: the command's +1 = 4, then ItemMenu_SetupForActor; with
// the command's flags (+0x10, the pointer read after the call) bit 1 and not
// bit 17 the page is 2; then the acting actor (its context's +5) to 0x929F06,
// sub = 0 and the step counted on.
extern "C" void __cdecl BattleItem_OpenWindow(void) {
    Command()[1] = 4;
    battle_actor_copies::g.setup_for_actor();
    const battle_actor_copies::U flags = static_cast<battle_actor_copies::U>(rv::As<records::ActionRecord>(Command())->flags2);
    if ((flags & 2) && !(flags & 0x20000)) Window16_Page = 2;
    const unsigned char actor = Acting()[5];
    BattleE4_Step4 = 0;
    BattleActorCopies_ItemActor = actor;
    BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
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
    if (WindowRecords[16].state != 0) return;
    WindowRecords[0x13].x = static_cast<unsigned>(battle_actor_copies::Long(battle_actor_copies::At(::bof3::Img(WindowRecords_addr) + 0x244))) + 7;
    WindowRecords[19].in_use = 1;
    const unsigned rows = static_cast<unsigned>(BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v))) - static_cast<unsigned>(battle_actor_copies::Long(battle_actor_copies::At(::bof3::Img(WindowRecords_addr) + 0x250))) + 2;
    const unsigned y = rows * 13 + WindowRecords[16].y;
    const unsigned pressed = Input_Pressed;
    WindowRecords[19].y = y;
    const battle_actor_copies::U held = battle_actor_copies::g.auto_repeat(pressed & Sigma(0xF00C));

    const battle_actor_copies::U flags = static_cast<battle_actor_copies::U>(rv::As<records::ActionRecord>(Command())->flags2);
    if (!(flags & 2) || (flags & 0x20000)) {
        if (held & 0x8000) {
            battle_actor_copies::g.play_effect(0x101);
            const unsigned char page = static_cast<unsigned char>(Window16_Page - 1);
            Window16_PageCue = 0x32;
            Window16_Page = page;
            if (static_cast<signed char>(page) < 0) Window16_Page = 3;
        } else if (held & 0x2000) {
            battle_actor_copies::g.play_effect(0x101);
            const unsigned char page = static_cast<unsigned char>(Window16_Page + 1);
            Window16_PageCue = 0x31;
            Window16_Page = page;
            if (page > 3) Window16_Page = 0;
        }
    }

    unsigned char cursor = BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v));
    const unsigned char was = cursor;
    if (held & 0x1000) {
        if (cursor != 0) BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v)) = --cursor;
        if (static_cast<short>(cursor) < static_cast<short>(battle_actor_copies::Word(battle_actor_copies::At(::bof3::Img(WindowRecords_addr) + 0x250)))) Window16_Scroll = 0xF0;
    } else if (held & 0x4000) {
        if (cursor < 9) BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v)) = ++cursor;
        const int last = static_cast<short>(battle_actor_copies::Word(battle_actor_copies::At(::bof3::Img(WindowRecords_addr) + 0x250))) + 7;
        if (!(static_cast<int>(BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v))) < last)) Window16_Scroll = 0x10;
    } else if (held & Sigma(4)) {
        const short scroll = static_cast<short>(battle_actor_copies::Word(battle_actor_copies::At(::bof3::Img(WindowRecords_addr) + 0x250)));
        if (scroll == 0) {
            cursor = 0;
            BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v)) = 0;
        } else if (scroll < 7) {
            const unsigned char s = BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemScroll.v));
            battle_actor_copies::SetWord(battle_actor_copies::At(::bof3::Img(WindowRecords_addr) + 0x250), 0);
            cursor = static_cast<unsigned char>(cursor - s);
            BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v)) = cursor;
        } else {
            cursor = static_cast<unsigned char>(cursor - 7);
            BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v)) = cursor;
            battle_actor_copies::SetWord(battle_actor_copies::At(::bof3::Img(WindowRecords_addr) + 0x250), static_cast<unsigned>(scroll - 7));
        }
    } else if (held & Sigma(8)) {
        const short scroll = static_cast<short>(battle_actor_copies::Word(battle_actor_copies::At(::bof3::Img(WindowRecords_addr) + 0x250)));
        if (scroll == 3) {
            cursor = 9;
            BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v)) = 9;
        } else if (scroll > -4) {
            const unsigned char s = BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemScroll.v));
            battle_actor_copies::SetWord(battle_actor_copies::At(::bof3::Img(WindowRecords_addr) + 0x250), 3);
            cursor = static_cast<unsigned char>(cursor + static_cast<unsigned char>(3 - s));
            BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v)) = cursor;
        } else {
            cursor = static_cast<unsigned char>(cursor + 7);
            BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v)) = cursor;
            battle_actor_copies::SetWord(battle_actor_copies::At(::bof3::Img(WindowRecords_addr) + 0x250), static_cast<unsigned>(scroll + 7));
        }
    }
    if (was != cursor) battle_actor_copies::g.play_effect(0x100);

    const battle_actor_copies::U entry = ((static_cast<std::uint32_t>(BattleActorCopies_QueueWrite) - 1) & 0xF) * 8;
    const unsigned id = ItemUnderCursor();
    const battle_actor_copies::U text = battle_actor_copies::g.msg_system(battle_actor_copies::Word(battle_actor_copies::At(::bof3::Img(battle_actor_copies::at::kItemRecords.v) + id * battle_actor_copies::at::kItemRecordSize + 6)));
    const bool scrolling = Window16_Scroll != 0;
    battle_actor_copies::SetLong(battle_actor_copies::At(::bof3::Img(battle_actor_copies::at::kQueueText.v) + entry), static_cast<std::int32_t>(text));
    if (scrolling) return;

    const unsigned buttons = Input_Pressed;
    if (buttons & Field_CancelButtons) {
        const unsigned char page = Window16_Page;
        const unsigned char scroll = BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemScroll.v));
        const unsigned owner = Window16_Owner;
        WindowRecords[16].state = 1;
        WindowRecords[19].in_use = 0;
        BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kQueueByte.v) + entry) = 1;
        KeepPage(page, scroll, owner);
        battle_actor_copies::g.play_effect(0x106);
        BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
        return;
    }
    if (!(buttons & Field_ConfirmButtons)) return;
    if (Low(battle_actor_copies::g.can_use()) != 0) {
        battle_actor_copies::g.play_effect(0x107);
        return;
    }
    battle_actor_copies::g.play_effect(0x103);
    const unsigned char page = Window16_Page;
    const unsigned char scroll = BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemScroll.v));
    const unsigned owner = Window16_Owner;
    KeepPage(page, scroll, owner);
    const unsigned actor = static_cast<unsigned char>(BattleActorCopies_ItemActor);
    const unsigned char* const list = battle_actor_copies::At(battle_actor_copies::g.ability_list(actor, page, 1));
    rv::Store<unsigned>(rv::As<records::ActionRecord>(Command())->action_id, list[BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kItemCursor.v))]);
    unsigned char* const c = Command();
    WindowRecords[16].state = 1;
    WindowRecords[19].in_use = 0;
    if (rv::As<records::ActionRecord>(c)->action_id == 0x97) {
        unsigned char* const a = Acting();
        WindowRecords[4].state = 2;
        c[0] = a[5];
        BattleE4_Step2 = 7;
        BattleE4_Step3 = 0;
        return;
    }
    BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kStep.v)) = 3;
}

// original 0x447840 (no PSX twin paired): whether the item under the item
// window's cursor can be used. list = 0x591E50(kItemActor, kItemWindowPage, 1)
// - the actor's inventory page - then 0x57DA70(2, actor, the list's byte at
// kItemWindowCursor), the actor byte read again after the first call; al 1 when
// that returns al 0. The second call's two arguments carry the list pointer's
// upper half above their bytes, as the original's registers do (movzx ax, then
// mov al); the first call's carry the entry's stale eax and ecx, which
// 0x591E50 masks off. The one caller (0x447776) tests al.
extern "C" unsigned char __cdecl ItemMenu_CanUseSelected(void) {
    const battle_misc::U list = battle_misc::g.item_list(static_cast<unsigned char>(BattleActorCopies_ItemActor), Window16_Page, 1);
    const battle_misc::U item = AtBattleMisc(list + AtBattleMisc(::bof3::Img(WindowRecords_addr) + 0x24c)[0])[0];
    const battle_misc::U high = list & 0xFFFF0000u;
    const battle_misc::U usable = battle_misc::g.item_usable(2, high | static_cast<unsigned char>(BattleActorCopies_ItemActor), high | item);
    return (usable & 0xFF) == 0;
}

// original 0x447880 (BattleItem_Steps entry 2).
extern "C" void __cdecl BattleItem_CloseWait(void) { CloseWait(); }

// original 0x4478B0: the chosen item's target, by its record's flags: 0x40 a
// choice - step 4 (0x447940) or, with 0x10, step 5 (0x447D70); else the target
// is set and the confirm (step 4, sub 3) follows - 0x80 all (0xC0), 0x10 one
// side whole (0x40 with 0x20, else 0x80), none the acting actor itself.
extern "C" void __cdecl BattleItem_Choose(void) {
    const unsigned char f = ItemFlags(ItemUnderCursor());
    if (f & 0x40) {
        BattleE4_Step3 = static_cast<unsigned char>(((f & 0x10) | 0x40) >> 4);
        return;
    }
    if (f & 0x80) {
        Command()[0] = 0xC0;
        BattleE4_Step3 = 4;
        BattleE4_Step4 = 3;
        return;
    }
    const unsigned char t = (f & 0x10) ? static_cast<unsigned char>((f & 0x20) ? 0x40 : 0x80) : Acting()[5];
    Command()[0] = t;
    BattleE4_Step3 = 4;
    BattleE4_Step4 = 3;
}

// original 0x447940: entry (dword 0x904AA4 & 0xFF) of BattleItem_TargetSteps
// 0x64E428 - 0x447960, 0x4479E0, 0x447B10, 0x447CC0, 0x447D30 (the cancel,
// R3B's BattleItem_TargetCancel).
extern "C" void __cdecl BattleItem_TargetDispatch(void) { Dispatch(BattleItem_TargetSteps, BattleE4_Step4); }

// original 0x447960: the item's target starts. The record of the command's
// item (+2) has 0x20: Battle_DefaultTarget(3) and sub 1 (the enemies); else
// the party: target 0 when Battle_ReturnTrue answers al non-zero (it always
// does), else Battle_DefaultTarget(0), and sub 2. Then the repeat latch
// 0x7E01B8 = 0 and 0x904AAF = 1.
extern "C" void __cdecl BattleItem_TargetBegin(void) {
    const unsigned id = rv::As<records::ActionRecord>(Command())->action_id;
    if (ItemFlags(id) & 0x20) {
        const unsigned char t = Low(battle_actor_copies::g.default_target(3));
        Command()[0] = t;
        BattleE4_Step4 = 1;
        battle_actor_copies::SetWord(battle_actor_copies::At(::bof3::Img(battle_actor_copies::at::kRepeatLatch.v)), 0);
        BattleE4_Picking = 1;
        return;
    }
    if (Low(battle_actor_copies::g.return_true()) != 0) {
        Command()[0] = 0;
    } else {
        const unsigned char t = Low(battle_actor_copies::g.default_target(0));
        Command()[0] = t;
    }
    BattleE4_Step4 = 2;
    battle_actor_copies::SetWord(battle_actor_copies::At(::bof3::Img(battle_actor_copies::at::kRepeatLatch.v)), 0);
    BattleE4_Picking = 1;
}

// original 0x4479E0: the item's pick among the enemies. The common head;
// then, for an item whose record (the item under the window's cursor, read
// again) has 0x80, 0x5000 moves to the party - Battle_DefaultTarget(0), sub
// counted on, cue 0x101 - and ends; else as BattleTarget_PickEnemy's steps.
extern "C" void __cdecl BattleItem_PickEnemy(void) {
    battle_actor_copies::U held;
    if (!PickHead(held)) return;
    const unsigned char f = ItemFlags(ItemUnderCursor());
    if ((f & 0x80) && (held & 0x5000)) {
        const unsigned char t = Low(battle_actor_copies::g.default_target(0));
        Command()[0] = t;
        BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 + 1);
        battle_actor_copies::g.play_effect(0x101);
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
    battle_actor_copies::U held;
    if (!PickHead(held)) return;
    const unsigned char f = ItemFlags(ItemUnderCursor());
    if ((f & 0x80) && (held & 0x5000)) {
        const unsigned char t = Low(battle_actor_copies::g.default_target(3));
        Command()[0] = t;
        BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 - 1);
        battle_actor_copies::g.play_effect(0x101);
        return;
    }
    if (held & 0x2000) {
        if (Low(battle_actor_copies::g.return_true()) != 0) {
            const battle_actor_copies::U high = static_cast<battle_actor_copies::U>(BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kPartyCount.v))) - 1;
            const unsigned char t = Low(Stepped(high, 0, +1));
            Command()[0] = t;
        } else {
            const battle_actor_copies::U high = static_cast<battle_actor_copies::U>(BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kPartyCount.v))) - 1;
            const unsigned char t = Low(battle_actor_copies::g.default_target(Stepped(high, 0, +1)));
            Command()[0] = t;
        }
        battle_actor_copies::g.play_effect(0x101);
    }
    if (held & 0x8000) {
        if (Low(battle_actor_copies::g.return_true()) != 0) {
            const battle_actor_copies::U high = static_cast<battle_actor_copies::U>(BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kPartyCount.v))) - 1;
            const unsigned char t = Low(Stepped(high, 0, -1));
            Command()[0] = t;
            battle_actor_copies::g.play_effect(0x101);
            return;
        }
        const battle_actor_copies::U high = static_cast<battle_actor_copies::U>(BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kPartyCount.v))) - 1;
        const unsigned char t = Low(battle_actor_copies::g.prev_target(Stepped(high, 0, -1)));
        Command()[0] = t;
        battle_actor_copies::g.play_effect(0x101);
    }
}

// original 0x447CC0: the item's target confirmed. Cue 0x104; 0x904AAF = 0;
// the command's +1 = 4 (the pointer read after the cue); the acting context's
// +1 = 2; 0x904AC3 counted on; the message queue entry before the write index
// gets +1 = 1; ItemMenu_FreeWindows; then 0x904AA2..0x904AA4 = 0 and phase 1
// = 1.
extern "C" void __cdecl BattleItem_Confirm(void) {
    battle_actor_copies::g.play_effect(0x104);
    unsigned char* const c = Command();
    BattleE4_Picking = 0;
    c[1] = 4;
    Acting()[1] = 2;
    BattleActorCopies_EntryCount = static_cast<unsigned char>(static_cast<unsigned char>(BattleActorCopies_EntryCount) + 1);
    const battle_actor_copies::U entry = (static_cast<std::uint32_t>(BattleActorCopies_QueueWrite) - 1) & 0xF;
    BBattleActorCopies(::bof3::Img(battle_actor_copies::at::kQueueByte.v) + entry * 8) = 1;
    battle_actor_copies::g.free_windows();
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
    BattleActions_Step = 1;
}

// original 0x447D30 (BattleItem_TargetSteps entry 4, and 0x64E43C's entry
// 3): the ability's pick cancelled - back to the list: cue 0x106, the
// actor's name, the pick flag cleared, step 4, the command 1, step 0,
// sub-state 1.
extern "C" void __cdecl BattleItem_TargetCancel(void) {
    CancelHead(false);
    BattleActions_Step = 4;
    BattleE4_Step2 = 1;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 1;
}

// original 0x447D70 (BattleItem_Steps entry 5): jmp [0x64E43C + 4 * (dword
// 0x904AA4 & 0xFF)] - BattleItem_SideSteps' four.
extern "C" void __cdecl BattleItem_SideDispatch(void) {
    DispatchRest3b("BattleItem_SideDispatch", ::bof3::Img(BattleE4_Step4_addr), ::bof3::Img(BattleItem_SideSteps_addr), 4);
}

// original 0x447D90 (BattleItem_SideSteps entry 0): a whole side as the
// target, by byte +0 of the command's word +2's NameTable_Abilities row: bit
// 0x20 the enemies (0x40), else the party (0x80); sub-state 1, the repeat
// latch 0, the pick flag set. BattleItemCmd_SideBegin's twin.
extern "C" void __cdecl BattleItem_SideBegin(void) {
    unsigned char* const c = PtrRest3b(::bof3::Img(rest_3b::at::kCommand.v));
    const unsigned char flags = AbilityRow(move_script::Word(c + 2))[0];
    c[0] = static_cast<unsigned char>((flags & 0x20) ? 0x40 : 0x80);
    BattleE4_Step4 = 1;
    move_script::SetWord(move_script::At(::bof3::Img(rest_3b::at::kRepeatLatch.v)), 0);
    BattleE4_Picking = 1;
}

// original 0x447DD0 (BattleItem_SideSteps entry 1): the side pick. Cancel ->
// sub-state 3, confirm -> 2. Else a direction Input_AutoRepeat(Input_Pressed
// & 0xF000) lets through (0xF000), with byte +0 bit 0x80 of the ability
// under the window's cursor - Char_AbilityList(byte 0x929F06, record 16's
// +0xB, 1)[record 16's dword +0xC & 0xFF] - flips the side (the command's +0
// ^= 0xC0) with cue 0x101. BattleItemCmd_SidePick's twin.
extern "C" void __cdecl BattleItem_SidePick(void) {
    const unsigned pressed = Input_Pressed;
    if (Field_CancelButtons & pressed) {
        BattleE4_Step4 = 3;
        return;
    }
    if (Field_ConfirmButtons & pressed) {
        BattleE4_Step4 = 2;
        return;
    }
    const U held = BH_CALL(Input_AutoRepeat)(pressed & 0xF000);
    if ((held & 0xF000) == 0) return;
    const unsigned char* const list = BH_CALL(Char_AbilityList)(static_cast<unsigned char>(BattleActorCopies_ItemActor), Window16_Page, 1);
    const unsigned id = list[L(::bof3::Img(rest_3b::at::kWin16Cursor.v)) & 0xFF];
    if ((AbilityRow(id)[0] & 0x80) == 0) return;
    unsigned char* const c = PtrRest3b(::bof3::Img(rest_3b::at::kCommand.v));
    c[0] = static_cast<unsigned char>(c[0] ^ 0xC0);
    BH_CALL(Sound_PlayEffect)(0x101);
}

// original 0x447E60 (no PSX twin paired): item window record 16 for the acting
// actor - the byte +5 of the context kActingActor points at. +0 = 1, +1 = 8, +2
// = 2, +3 = 2, +0xA = the actor, +0xD = 0xFF, +8 = 2, +0xB = its page byte
// (kActorPages + actor * 3), +9 = 0, word +0x10 = its second byte, +0xC = its
// cursor byte; word +0x14 = 2 when the dword +0x10 of what kCommandSource
// points at has bit 1 and not bit 17, else 0; word +4 = 0x140, word +6 = 0x3F;
// then 0x80340C = 0, 0x80340E = 0, kItemActor = the actor byte read again,
// 0x80340D = 8. In that order. The one caller (0x447469) reloads eax.
extern "C" void __cdecl ItemMenu_SetupForActor(void) {
    const battle_misc::U acting = Low32(BattleE4_MenuActor.Native());
    unsigned char* const w = AtBattleMisc(::bof3::Img(Window16_addr));
    auto* const window_rec = rv::As<records::WindowRecord>(w);
    window_rec->in_use = 1;
    window_rec->handler = 8;
    window_rec->kind = 2;
    window_rec->state = 2;
    const battle_misc::U actor = AtBattleMisc(acting + 5)[0];
    w[0xA] = static_cast<unsigned char>(actor);
    w[0xD] = 0xFF;
    const unsigned char page = AtBattleMisc(::bof3::Img(kActorPages.v) + actor * 3)[0];
    w[8] = 2;
    w[0xB] = page;
    w[9] = 0;
    PutWord(w + 0x10, AtBattleMisc(::bof3::Img(kActorPages.v) + 1 + actor * 3)[0]);
    w[0xC] = AtBattleMisc(::bof3::Img(kActorPages.v) + 2 + actor * 3)[0];
    const battle_misc::U flags = LongBattleMisc(AtBattleMisc(BattleActorCopies_Command + 0x10));
    PutWord(w + 0x14, ((flags & 2) && !(flags & 0x20000)) ? 2 : 0);
    window_rec->x = 0x140;
    window_rec->y = 0x3F;
    const unsigned char again = AtBattleMisc(acting + 5)[0];
    WindowRecords[19].in_use = 0;
    WindowRecords[0x13].kind = 0;
    BattleActorCopies_ItemActor = again;
    WindowRecords[0x13].handler = 8;
}

// original 0x447F40 (PSX 0x80094FB0): window record 16 set up as a list for
// the member choosing: +0 1, +1 8, +2 2, +3 2, +0xB 2, +0xA the member's +5,
// +0xD 0xFF, +8 2, +9 0, word +4 0x140, word +6 0x3F, word +0x14 the command
// record's +0x10 & 2; 0x929F06 the member's +5 again; the hand (record 19)
// off, kind 8. BE5's four Dragon-run steps call it.
extern "C" void __cdecl ItemMenu_SetupForMember(void) {
    unsigned char* const actor = MenuActor();
    WindowRecords[16].in_use = 1;
    B(::bof3::Img(battle_e4::at::kList.v) + 1) = 8;
    B(::bof3::Img(battle_e4::at::kList.v) + 2) = 2;
    B(::bof3::Img(battle_e4::at::kList.v) + 3) = 2;
    B(::bof3::Img(battle_e4::at::kList.v) + 0xB) = 2;
    B(::bof3::Img(battle_e4::at::kList.v) + 0xA) = actor[5];
    unsigned char* const command = CommandBattleE4();
    B(::bof3::Img(battle_e4::at::kList.v) + 0xD) = 0xFF;
    B(::bof3::Img(battle_e4::at::kList.v) + 8) = 2;
    B(::bof3::Img(battle_e4::at::kList.v) + 9) = 0;
    const unsigned bits = command[0x10] & 2u;
    WindowRecords[0x10].x = 0x140;
    WindowRecords[0x10].y = 0x3F;
    move_script::SetWord(move_script::At(::bof3::Img(Window16_addr) + 0x14), bits);
    BattleActorCopies_ItemActor = actor[5];
    WindowRecords[19].in_use = 0;
    B(::bof3::Img(battle_e4::at::kHand.v) + 1) = 8;
    B(::bof3::Img(battle_e4::at::kHand.v) + 2) = 0;
}

// original 0x447FD0 (PSX 0x80095070 by the catalogue's pairing; not read):
// the command's state byte 0x904AA3 through BattleAttackCmd_States 0x64E44C -
// 0x447FE0, 0x448020, 0x4480E0, 0x448140 (the cancel, never reached on the
// route and left Capcom's). The index is not checked, as the original has it.
extern "C" void __cdecl BattleAttackCmd_Dispatch(void) { Entry(::bof3::Img(BattleAttackCmd_States_addr), BattleE4_Step3)(); }

// original 0x447FE0: state 0. The target is Battle_DefaultTarget(3) - the
// first enemy not out - the banner is message 2 (BattleBanner_SetMessage(2,
// 0)), the pick flag 0x904AAF is set and the auto-repeat latch zeroed, and
// the state moves on. The state byte is read after the banner call.
extern "C" void __cdecl BattleAttackCmd_Begin(void) {
    const unsigned char target = battle_menu_states::g.default_target(3);
    CommandBattleMenuStates()[0] = target;
    battle_menu_states::g.banner_message(2, 0);
    const unsigned char state = static_cast<unsigned char>(BattleE4_Step3 + 1);
    BattleE4_Picking = 1;
    battle_menu_states::SetWord(battle_menu_states::At(::bof3::Img(battle_menu_states::at::kRepeatLatch.v)), 0);
    BattleE4_Step3 = state;
}

// original 0x448020: state 1. A cancel button pressed goes to state 3, a
// confirm button to state 2 (cancel first, both tested against the pressed
// word's 16 bits); otherwise the pressed directions go to Input_AutoRepeat
// and its right (0x2000) and left (0x8000) move the target (StepEnemyTargetBattleMenuStates),
// right first, each with cue 0x101.
extern "C" void __cdecl BattleAttackCmd_PickEnemy(void) {
    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        BattleE4_Step3 = 3;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        BattleE4_Step3 = 2;
        return;
    }
    StepEnemyTargetBattleMenuStates(battle_menu_states::g.auto_repeat(pad & 0xF000));
}

// original 0x4480E0: state 2, the confirm. Cue 0x104; the pick flag cleared;
// the command's action kind +1 = 0 and its dword +0xC |= 1; the acting
// member's state +1 = 2; one more command chosen (0x904AC3); back to the
// command step 1 with the command and its state 0.
extern "C" void __cdecl BattleAttackCmd_Confirm(void) {
    battle_menu_states::g.sound(0x104);
    unsigned char* command = CommandBattleMenuStates();
    BattleE4_Picking = 0;
    command[1] = 0;
    command = CommandBattleMenuStates();
    rv::Store<std::int32_t>(rv::As<records::ActionRecord>(command)->flags, static_cast<std::int32_t>(rv::As<records::ActionRecord>(command)->flags) | 1);
    Acting()[1] = 2;
    const unsigned char chosen = static_cast<unsigned char>(static_cast<unsigned char>(BattleActorCopies_EntryCount) + 1);
    BattleActions_Step = 1;
    BattleActorCopies_EntryCount = chosen;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
}

// original 0x448140 (BattleAttackCmd_States entry 3): the attack's pick
// cancelled - as BattleTarget_Cancel, the sub-state left as it is.
extern "C" void __cdecl BattleAttackCmd_Cancel(void) {
    CancelHead(true);
    BattleActions_Step = 2;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
}

// original 0x448180 (PSX 0x80095314 by the catalogue's pairing; not read): the
// state byte 0x904AA3 through BattleItemCmd_States 0x64E45C - 0 open
// (0x448190), 1 the list (0x448210), 2 the list closed (0x448600, left
// Capcom's), 3 the target kind (0x448630), 4 the target (0x4486C0), 5
// 0x448B80, 6 0x448C80, 7 0x4493E0, 8 0x449480, 9 0x4498F0 (the last five
// unread, not this group's). No bound, as the original has it.
extern "C" void __cdecl BattleItemCmd_Dispatch(void) { Entry(::bof3::Img(BattleItemCmd_States_addr), BattleE4_Step3)(); }

// original 0x448190: state 0, by the sub-state 0x904AA4 (read as a dword, its
// low byte) through BattleItemCmd_OpenSteps 0x64E484 - 0x4481B0, 0x4481E0.
extern "C" void __cdecl BattleItemCmd_OpenDispatch(void) {
    Entry(::bof3::Img(BattleItemCmd_OpenSteps_addr), static_cast<std::uint32_t>(battle_menu_states::Long(battle_menu_states::At(::bof3::Img(BattleE4_Step4_addr)))) & 0xFF)();
}

// original 0x4481B0: the prompt. System message 0x4000 (Msg_SystemPtr: bank 1,
// message 0) goes into the battle message queue as BattleQueue_Push(2, 0xFF,
// text), and the sub-state moves on. Also entry 13 of the item-menu table
// 0x64E3EC.. (0x64E420, group CH's). The text is Capcom's Chinese, drawn by the
// queue's reader, not here.
extern "C" void __cdecl BattleItemCmd_QueuePrompt(void) {
    const unsigned char* const text = battle_menu_states::g.system_ptr(0x4000);
    battle_menu_states::g.queue_push(2, 0xFF, Low32(text));
    BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 + 1);
}

// original 0x4481E0: the list opened. The command's action kind +1 = 5;
// ItemMenu_SetupForParty (window record 16 from the saved page, top and
// cursor 0x904605..7); the sub-state 0 and the state on.
extern "C" void __cdecl BattleItemCmd_OpenList(void) {
    CommandBattleMenuStates()[1] = 5;
    battle_menu_states::g.setup_for_party();
    const unsigned char state = static_cast<unsigned char>(BattleE4_Step3 + 1);
    BattleE4_Step4 = 0;
    BattleE4_Step3 = state;
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
    auto* const list_window = rv::As<records::WindowRecord>(list);
    if (list_window->state != 0) return;
    const unsigned actor = Acting()[5];
    auto* const hand_window = &WindowRecords[0x13];
    hand_window->in_use = 1;
    hand_window->x = static_cast<unsigned>(battle_menu_states::Long(list + 4)) + 7;
    const unsigned top_row = list[0xB];
    auto* const member_rec = rv::As<records::PartyMember>(battle_menu_states::At(::bof3::Img(ObjTrio_addr) + actor * battle_menu_states::at::kMemberSize));
    member_rec->flags_130 = static_cast<std::int32_t>(member_rec->flags_130) & 0xBFFF;
    const unsigned cursor_row = list[0xC];
    const unsigned pressed = Input_Pressed & Sigma(0xF00C);
    hand_window->y = (cursor_row - top_row + 2) * 13 + list_window->y;
    const unsigned repeat = battle_menu_states::g.auto_repeat(pressed);

    if (repeat & 0x8000) {
        battle_menu_states::g.sound(0x101);
        const auto category = static_cast<unsigned char>(list[0xA] - 1);
        battle_menu_states::SetWord(list + 0x10, 0x32);
        list[0xA] = category;
        if (static_cast<signed char>(category) < 0) list[0xA] = 3;
    } else if (repeat & 0x2000) {
        battle_menu_states::SetWord(list + 0x10, 0x31);
        battle_menu_states::g.sound(0x101);
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
        if (cursor < list[0xB]) battle_menu_states::SetWord(list + 0x12, 0xF0);
    } else if (repeat & 0x4000) {
        if (cursor < 0x7F) {
            ++cursor;
            list[0xC] = cursor;
        }
        if (static_cast<int>(list[0xC]) >= static_cast<int>(list[0xB]) + 7) battle_menu_states::SetWord(list + 0x12, 0x10);
    } else if (repeat & Sigma(4)) {
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
    } else if (repeat & Sigma(8)) {
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
    if (before != cursor) battle_menu_states::g.sound(0x100);

    const unsigned entry = LastQueueEntry();
    const unsigned category = list[0xA];
    const unsigned item = ListItem();
    const unsigned message = battle_menu_states::g.item_price(category, item);
    const unsigned char* const help = battle_menu_states::g.system_ptr(message);
    battle_menu_states::SetLong(battle_menu_states::At(::bof3::Img(battle_menu_states::at::kQueue.v) + 4 + entry), static_cast<std::int32_t>(Low32(help)));
    if (battle_menu_states::Word(list + 0x12) != 0) return;

    if (up && before == 0) {
        battle_menu_states::g.sound(0x100);
        const std::uint32_t x = static_cast<std::uint32_t>(battle_menu_states::Long(list + 4)) + 0x20;
        const unsigned y = list_window->y - 0x16u;
        unsigned char* const above = battle_menu_states::At(::bof3::Img(Window21_addr));
        auto* const above_window = rv::As<records::WindowRecord>(above);
        above_window->handler = 8;
        above_window->kind = 5;
        above_window->in_use = 1;
        above_window->x = x;
        above_window->y = y;
        above[0xA] = 0;
        above[0xB] = 0xFF;
        WindowRecords[4].in_use = 0;
        BattleE4_Step3 = 6;
        return;
    }

    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        const unsigned char page = list[0xA], top = list[0xB], row = list[0xC];
        BattleMisc_PartyPages = page;
        unsigned char* const acting = Acting();
        list_window->state = 1;
        hand_window->in_use = 0;
        battle_menu_states::At(::bof3::Img(battle_menu_states::at::kQueue.v) + 1 + entry)[0] = 1;
        Byte(::bof3::Img(battle_menu_states::at::kSavedList.v) + 1) = top;
        Byte(::bof3::Img(battle_menu_states::at::kSavedList.v) + 2) = row;
        acting[0x125] = 0;
        battle_menu_states::g.sound(0x106);
        BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
        return;
    }
    if (!(Field_ConfirmButtons & pad)) return;

    const unsigned chosen_category = list[0xA];
    const unsigned chosen_item = ListItem();
    rv::Store<unsigned>(rv::As<records::ActionRecord>(CommandBattleMenuStates())->action_id, (chosen_category << 8) + chosen_item);
    const unsigned use_category = list[0xA];
    const unsigned use_item = CommandBattleMenuStates()[2];
    const unsigned mode = list[8];
    if (battle_menu_states::g.item_can_use(mode, 0, use_category, use_item) == 0) {
        battle_menu_states::g.sound(0x107);
        return;
    }
    const unsigned char page = list[0xA], top = list[0xB], row = list[0xC];
    BattleMisc_PartyPages = page;
    Byte(::bof3::Img(battle_menu_states::at::kSavedList.v) + 1) = top;
    Byte(::bof3::Img(battle_menu_states::at::kSavedList.v) + 2) = row;
    battle_menu_states::g.sound(0x103);
    list_window->state = 1;
    hand_window->in_use = 0;
    BattleE4_Step3 = 3;
}

// original 0x448600 (BattleItemCmd_States entry 2).
extern "C" void __cdecl BattleItemCmd_CloseWait(void) { CloseWait(); }

// original 0x448630: state 3, the item's target kind from its flag byte
// (0x591810). Bit 0x40: state 4, or 5 with bit 0x10 too (the sub-state left
// as it is, 0 from the opening). Else bit 0x80: target 0xC0, state 4, sub-state
// 3 (the commit). Else the target is 0x40 (bits 0x10 and 0x20), 0x80 (0x10
// alone) or the acting member itself (its +5), and state 4, sub-state 3.
extern "C" void __cdecl BattleItemCmd_TargetKind(void) {
    const unsigned char flags = ListItemFlags();
    if (flags & 0x40) {
        BattleE4_Step3 = static_cast<unsigned char>(((flags & 0x10) | 0x40) >> 4);
        return;
    }
    if (flags & 0x80) {
        CommandBattleMenuStates()[0] = 0xC0;
        BattleE4_Step3 = 4;
        BattleE4_Step4 = 3;
        return;
    }
    unsigned char target;
    if (flags & 0x10) target = (flags & 0x20) ? 0x40 : 0x80;
    else target = Acting()[5];
    CommandBattleMenuStates()[0] = target;
    BattleE4_Step3 = 4;
    BattleE4_Step4 = 3;
}

// original 0x4486C0: state 4, by the sub-state 0x904AA4 (a dword's low byte)
// through BattleItemCmd_TargetSteps 0x64E48C - 0x4486E0 (begin), 0x448780
// (an enemy), 0x4488C0 (a member), 0x448A70 (commit), 0x448B40 (back to the
// list; unread, not this group's).
extern "C" void __cdecl BattleItemCmd_TargetDispatch(void) {
    Entry(::bof3::Img(BattleItemCmd_TargetSteps_addr), static_cast<std::uint32_t>(battle_menu_states::Long(battle_menu_states::At(::bof3::Img(BattleE4_Step4_addr)))) & 0xFF)();
}

// original 0x4486E0: the target pick begun. Flag bit 0x20: the first enemy not
// out (Battle_DefaultTarget(3)), sub-state 1. Else, while Battle_ReturnTrue
// answers al non-zero (always, in this port), target 0 outright; otherwise
// Battle_DefaultTarget(0); sub-state 2. Both: the latch zeroed, the pick flag
// set.
extern "C" void __cdecl BattleItemCmd_TargetBegin(void) {
    const unsigned char flags = ListItemFlags();
    if (flags & 0x20) {
        const unsigned char target = battle_menu_states::g.default_target(3);
        CommandBattleMenuStates()[0] = target;
        BattleE4_Step4 = 1;
        battle_menu_states::SetWord(battle_menu_states::At(::bof3::Img(battle_menu_states::at::kRepeatLatch.v)), 0);
        BattleE4_Picking = 1;
        return;
    }
    if (battle_menu_states::g.return_true() != 0) {
        CommandBattleMenuStates()[0] = 0;
    } else {
        const unsigned char target = battle_menu_states::g.default_target(0);
        CommandBattleMenuStates()[0] = target;
    }
    BattleE4_Step4 = 2;
    battle_menu_states::SetWord(battle_menu_states::At(::bof3::Img(battle_menu_states::at::kRepeatLatch.v)), 0);
    BattleE4_Picking = 1;
}

// original 0x448780: sub-state 1, an enemy. Cancel -> sub-state 4, confirm ->
// 3. Otherwise Input_AutoRepeat(pressed & 0xF000): up or down (0x5000) with
// the item's flag bit 0x80 crosses to the party - target 0 (Battle_ReturnTrue)
// or Battle_DefaultTarget(0), sub-state 2, cue 0x101 - and ends there; then
// right and left as the attack's (StepEnemyTargetBattleMenuStates).
extern "C" void __cdecl BattleItemCmd_PickEnemy(void) {
    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        BattleE4_Step4 = 4;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        BattleE4_Step4 = 3;
        return;
    }
    const unsigned repeat = battle_menu_states::g.auto_repeat(pad & 0xF000);
    if ((repeat & 0x5000) && (ListItemFlags() & 0x80)) {
        if (battle_menu_states::g.return_true() != 0) {
            CommandBattleMenuStates()[0] = 0;
        } else {
            const unsigned char target = battle_menu_states::g.default_target(0);
            CommandBattleMenuStates()[0] = target;
        }
        BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 + 1);
        battle_menu_states::g.sound(0x101);
        return;
    }
    StepEnemyTargetBattleMenuStates(repeat);
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
        BattleE4_Step4 = 4;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        BattleE4_Step4 = 3;
        return;
    }
    const unsigned repeat = battle_menu_states::g.auto_repeat(pad & 0xF000);
    if ((repeat & 0x5000) && (ListItemFlags() & 0x80)) {
        const unsigned char target = battle_menu_states::g.default_target(3);
        CommandBattleMenuStates()[0] = target;
        BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 - 1);
        battle_menu_states::g.sound(0x101);
        return;
    }
    const auto party_high = [] { return static_cast<long>(static_cast<std::uint32_t>(battle_menu_states::Long(battle_menu_states::At(::bof3::Img(battle_menu_states::at::kPartyCount.v)))) & 0xFF) - 1; };
    if (repeat & 0x2000) {
        if (battle_menu_states::g.return_true() != 0) {
            const long high = party_high();
            const long value = static_cast<long>(static_cast<signed char>(CommandBattleMenuStates()[0])) + 1;
            const long wrapped = battle_menu_states::g.wrap_index(high, 0, value);
            CommandBattleMenuStates()[0] = static_cast<unsigned char>(wrapped);
        } else {
            const long high = party_high();
            const long value = static_cast<long>(static_cast<signed char>(CommandBattleMenuStates()[0])) + 1;
            const long wrapped = battle_menu_states::g.wrap_index(high, 0, value);
            const unsigned char target = battle_menu_states::g.default_target(static_cast<unsigned>(wrapped));
            CommandBattleMenuStates()[0] = target;
        }
        battle_menu_states::g.sound(0x101);
    }
    if (repeat & 0x8000) {
        if (battle_menu_states::g.return_true() != 0) {
            const long high = party_high();
            const long value = static_cast<long>(static_cast<signed char>(CommandBattleMenuStates()[0])) - 1;
            const long wrapped = battle_menu_states::g.wrap_index(high, 0, value);
            CommandBattleMenuStates()[0] = static_cast<unsigned char>(wrapped);
        } else {
            const long high = party_high();
            const long value = static_cast<long>(static_cast<signed char>(CommandBattleMenuStates()[0])) - 1;
            const long wrapped = battle_menu_states::g.wrap_index(high, 0, value);
            const unsigned char target = battle_menu_states::g.prev_target(static_cast<unsigned>(wrapped));
            CommandBattleMenuStates()[0] = target;
        }
        battle_menu_states::g.sound(0x101);
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
    battle_menu_states::g.sound(0x104);
    unsigned char* command = CommandBattleMenuStates();
    BattleE4_Picking = 0;
    command[1] = 5;
    Acting()[1] = 2;
    const unsigned char chosen = static_cast<unsigned char>(static_cast<unsigned char>(BattleActorCopies_EntryCount) + 1);
    command = CommandBattleMenuStates();
    const unsigned char row = List()[0xC];
    BattleActorCopies_EntryCount = chosen;
    battle_menu_states::At(::bof3::Img(battle_menu_states::at::kQueue.v) + 1 + LastQueueEntry())[0] = 1;
    command[0xA] = row;
    const unsigned index = static_cast<std::uint32_t>(battle_menu_states::Long(List() + 0xC)) & 0xFF;
    const unsigned category = List()[0xA];
    unsigned char* const count = PtrAtBattleMenuStates(::bof3::Img(battle_menu_states::at::kInventoryCounts.v) + 4 * category) + index;
    if (*count > 1) {
        *count = static_cast<unsigned char>(*count - 1);
    } else {
        PtrAtBattleMenuStates(::bof3::Img(battle_menu_states::at::kInventoryIds.v) + 4 * category)[index] = 0;
        const unsigned index2 = static_cast<std::uint32_t>(battle_menu_states::Long(List() + 0xC)) & 0xFF;
        const unsigned category2 = List()[0xA];
        PtrAtBattleMenuStates(::bof3::Img(battle_menu_states::at::kInventoryCounts.v) + 4 * category2)[index2] = 0;
    }
    battle_menu_states::g.free_windows();
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
    BattleActions_Step = 1;
}

// original 0x448B40 (BattleItemCmd_TargetSteps entry 4, BattleItemCmd_SideSteps
// entry 3): the item's pick cancelled - step 4, the command 2, step 0,
// sub-state 1.
extern "C" void __cdecl BattleItemCmd_TargetCancel(void) {
    CancelHead(false);
    BattleActions_Step = 4;
    BattleE4_Step2 = 2;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 1;
}

// original 0x448B80 (BattleItemCmd_States entry 5): through BattleItemCmd_SideSteps (4).
extern "C" void __cdecl BattleItemCmd_SideDispatch(void) {
    DispatchRest3b("BattleItemCmd_SideDispatch", ::bof3::Img(BattleE4_Step4_addr), ::bof3::Img(BattleItemCmd_SideSteps_addr), 4);
}

// original 0x448BA0 (hidden, 0x64E4A0's entry 0; PSX none in the pairs):
// state 5 begun - a whole side's target by the list item's flag byte (bit
// 0x20 the enemies 0x40, else the party 0x80), sub-state 1, the auto-repeat
// latch zeroed, the pick flag set.
extern "C" void __cdecl BattleItemCmd_SideBegin(void) {
    const unsigned category = B(::bof3::Img(battle_e4::at::kList.v) + 0xA);
    const unsigned row = static_cast<U>(move_script::Long(move_script::At(::bof3::Img(Window16_addr) + 0xC))) & 0xFF;
    const unsigned item = Ptr(::bof3::Img(battle_e4::at::kInventoryIds.v) + 4 * category)[row];
    const unsigned char flags = ItemFlagsBattleE4(category, item);
    CommandBattleE4()[0] = SideTarget(flags);
    BattleE4_Step4 = 1;
    move_script::SetWord(move_script::At(::bof3::Img(battle_e4::at::kRepeatLatch.v)), 0);
    BattleE4_Picking = 1;
}

// original 0x448C00 (hidden, 0x64E4A0's entry 1): the side pick. Cancel ->
// sub-state 3, confirm -> 2; any direction Input_AutoRepeat lets through
// (0xF000) with the list item's flag bit 0x80 flips the side (target ^ 0xC0),
// cue 0x101.
extern "C" void __cdecl BattleItemCmd_SidePick(void) {
    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        BattleE4_Step4 = 3;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        BattleE4_Step4 = 2;
        return;
    }
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pad & 0xF000);
    if ((repeat & 0xF000) == 0) return;
    const unsigned category = B(::bof3::Img(battle_e4::at::kList.v) + 0xA);
    const unsigned row = static_cast<U>(move_script::Long(move_script::At(::bof3::Img(Window16_addr) + 0xC))) & 0xFF;
    const unsigned item = Ptr(::bof3::Img(battle_e4::at::kInventoryIds.v) + 4 * category)[row];
    if (ItemFlagsBattleE4(category, item) & 0x80) {
        CommandBattleE4()[0] ^= 0xC0;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
}

// original 0x448C80 (BattleItemCmd_States entry 6): through BattleItemCmd_EquipSteps (4).
extern "C" void __cdecl BattleItemCmd_EquipDispatch(void) {
    DispatchRest3b("BattleItemCmd_EquipDispatch", ::bof3::Img(BattleE4_Step4_addr), ::bof3::Img(BattleItemCmd_EquipSteps_addr), 4);
}

// original 0x448CA0 (hidden, 0x64E4B0's entry 0; PSX 0x8009632C): state 6,
// the window above the list with its two options (record 21's +0xA). Message
// 0x2F / 0x31 by the option into the last queue entry; the hand beside the
// option unless record 21's +3 is set (x = record 21's dword +4 + 48 *
// option, y its +6 + 4); 0x80345F = 0xFF. Left / right (0xA000) flip the
// option, cue 0x101 (Input_Pressed read again). Cancel, or down (0x4000, cue
// 0x100): the window off, record 4 on, back to the list (0x904AA2 = 2,
// 0x904AA3 = 1). Confirm, cue 0x103, the option kept in 0x80345F: option 0
// opens the slots for use (BattleEquip_OpenUse) at sub-state + 3, option 1 the
// slots for a change (BattleEquip_OpenChange) at sub-state + 1, record 17's
// +9 / +0xC the slot cursor and the member.
extern "C" void __cdecl BattleItemCmd_EquipMenu(void) {
    const unsigned char* const text = OptionMessage(0x2F);
    move_script::SetLong(move_script::At(::bof3::Img(battle_e4::at::kQueue.v) + 4 + LastQueueEntry()), S32BattleE4(Low32(text)));
    if (B(::bof3::Img(battle_e4::at::kAbove.v) + 3) == 0) {
        WindowRecords[19].in_use = 1;
        WindowRecords[0x13].x = B(::bof3::Img(battle_e4::at::kAbove.v) + 0xA) * 48u + static_cast<U>(move_script::Long(move_script::At(::bof3::Img(Window21_addr) + 4)));
        WindowRecords[0x13].y = WindowRecords[0x15].y + 4u;
    } else {
        WindowRecords[19].in_use = 0;
    }
    U pad = static_cast<U>(move_script::Long(move_script::At(::bof3::Img(Input_Pressed_addr))));
    B(::bof3::Img(battle_e4::at::kAbove.v) + 0xB) = 0xFF;
    if (pad & 0xA000) {
        BH_CALL(Sound_PlayEffect)(0x101);
        B(::bof3::Img(battle_e4::at::kAbove.v) + 0xA) ^= 1;
        pad = static_cast<U>(move_script::Long(move_script::At(::bof3::Img(Input_Pressed_addr))));
    }
    bool back = false;
    if (Field_CancelButtons & pad) {
        back = true;
    } else if (Field_ConfirmButtons & pad) {
        BH_CALL(Sound_PlayEffect)(0x103);
        const unsigned char option = B(::bof3::Img(battle_e4::at::kAbove.v) + 0xA);
        B(::bof3::Img(battle_e4::at::kAbove.v) + 0xB) = option;
        if (option == 0) {
            BH_CALL(BattleEquip_OpenUse)();
            BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 + 3);
            return;
        }
        BH_CALL(BattleEquip_OpenChange)();
        const auto sub = static_cast<unsigned char>(BattleE4_Step4 + 1);
        const unsigned char cursor = B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA);
        const unsigned char member = B(::bof3::Img(battle_e4::at::kSlots.v) + 0xC);
        B(::bof3::Img(battle_e4::at::kCands.v) + 9) = cursor;
        B(::bof3::Img(battle_e4::at::kCands.v) + 0xC) = member;
        B(::bof3::Img(battle_e4::at::kStep4.v)) = sub;
        return;
    } else if (pad & 0x4000) {
        BH_CALL(Sound_PlayEffect)(0x100);
        back = true;
    }
    if (!back) return;
    WindowRecords[21].in_use = 0;
    WindowRecords[4].in_use = 1;
    BattleE4_Step2 = 2;
    BattleE4_Step3 = 1;
}

// original 0x448DE0 (hidden, 0x64E4B0's entry 1; PSX 0x80096530): the six
// slots for a change. The hand at the cursor; message 0x30 / 0x32 by the
// option into the last queue entry; Input_AutoRepeat(pressed & 0x5000).
// Cancel (cue 0x106): the slots closed (record 18 +3 = 1), the list back on
// at its kept place (record 16 +0 = 1, +3 = 3, words +4 / +6 from record 17's
// +4 / +6), the candidates, the window above and the hand off, record 4 on,
// state 1, sub-state 0. Button 0x10: BattleEquip_RemoveSlot, cue 0x103 when it
// took the slot off, else 0x107. Confirm (cue 0x103): the slot chosen (record
// 18 +0xB), record 17's category +8 and item +0xD 0, its cursor +0xB = its
// top +0xA, 0x929F04 = 0, sub-state + 1. Otherwise up / down move the slot
// (SlotCursor) and record 17's +9 takes it.
extern "C" void __cdecl BattleItemCmd_EquipSlotPick(void) {
    SlotHand();
    const unsigned char* const text = OptionMessage(0x30);
    const unsigned entry = LastQueueEntry();
    const unsigned pressed = Input_Pressed;
    move_script::SetLong(move_script::At(::bof3::Img(battle_e4::at::kQueue.v) + 4 + entry), S32BattleE4(Low32(text)));
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pressed & 0x5000);
    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        BH_CALL(Sound_PlayEffect)(0x106);
        const unsigned x = WindowRecords[0x11].x;
        const unsigned y = WindowRecords[0x11].y;
        B(::bof3::Img(battle_e4::at::kSlots.v) + 3) = 1;
        WindowRecords[16].in_use = 1;
        WindowRecords[4].in_use = 1;
        BattleE4_Step3 = 1;
        B(::bof3::Img(battle_e4::at::kList.v) + 3) = 3;
        WindowRecords[0x10].x = x;
        WindowRecords[0x10].y = y;
        WindowRecords[17].in_use = 0;
        WindowRecords[21].in_use = 0;
        WindowRecords[19].in_use = 0;
        BattleE4_Step4 = 0;
        return;
    }
    if (pad & Sigma(0x10)) {   // DIVERGENCE DIV-0139: Japan's triangle through σ (0x448EDB)
        BH_CALL(Sound_PlayEffect)(BH_CALL(BattleEquip_RemoveSlot)() != 0 ? 0x103 : 0x107);
        return;
    }
    if (Field_ConfirmButtons & pad) {
        BH_CALL(Sound_PlayEffect)(0x103);
        const unsigned char cursor = B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA);
        const unsigned char top = B(::bof3::Img(battle_e4::at::kCands.v) + 0xA);
        B(::bof3::Img(battle_e4::at::kSlots.v) + 0xB) = cursor;
        const auto sub = static_cast<unsigned char>(BattleE4_Step4 + 1);
        B(::bof3::Img(battle_e4::at::kCands.v) + 8) = 0;
        B(::bof3::Img(battle_e4::at::kCands.v) + 0xD) = 0;
        B(::bof3::Img(battle_e4::at::kCands.v) + 0xB) = top;
        B(::bof3::Img(battle_e4::at::kListReset.v)) = 0;
        BattleE4_Step4 = sub;
        return;
    }
    SlotCursor(repeat);
    B(::bof3::Img(battle_e4::at::kCands.v) + 9) = B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA);
}

// original 0x448FC0 (hidden, 0x64E4B0's entry 2; PSX 0x800967F8): the
// candidates (record 17). The hand at its cursor unless its +3 is set;
// Input_AutoRepeat(pressed & 0xF00C) for its latch, then (pressed & 0x500C)
// for the moves: up / down the cursor +0xB one back (not below 0) / on (not
// above 0x7F), asking a scroll (word +0x10 = 0xF0 above the top, 0x10 at top
// + 7); 4 / 8 a page of 7 (the top +0xA capped at 0x79, the cursor at 0x7F);
// cue 0x100 on a move. The help line of record 17's item (Item_HelpMessage by
// +8, +0xD) into the last queue entry. Cancel (cue 0x106) or a confirmed
// change back to the slots: 0x8033F5 = 1, the cursor and record 18's chosen
// slot 0xFF, sub-state - 1. Else BattleEquip_Preview; confirm with an item:
// cue 0x103, BattleEquip_Apply and back; without: cue 0x107.
extern "C" void __cdecl BattleItemCmd_EquipItemPick(void) {
    if (B(::bof3::Img(battle_e4::at::kCands.v) + 3) == 0) {
        WindowRecords[19].in_use = 1;
        WindowRecords[0x13].x = static_cast<U>(move_script::Long(move_script::At(::bof3::Img(Window17_addr) + 4))) + 7;
        const U rows = static_cast<U>(B(::bof3::Img(battle_e4::at::kCands.v) + 0xB)) - B(::bof3::Img(battle_e4::at::kCands.v) + 0xA) + 2;
        WindowRecords[0x13].y = rows * 13 + WindowRecords[0x11].y;
    } else {
        WindowRecords[19].in_use = 0;
    }
    BH_CALL(Input_AutoRepeat)(Input_Pressed & Sigma(0xF00C));
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(Input_Pressed & Sigma(0x500C));
    unsigned char cursor = B(::bof3::Img(battle_e4::at::kCands.v) + 0xB);
    const unsigned char was = cursor;
    if (repeat & 0x1000) {
        if (cursor > 0) {
            --cursor;
            B(::bof3::Img(battle_e4::at::kCands.v) + 0xB) = cursor;
        }
        if (cursor < B(::bof3::Img(battle_e4::at::kCands.v) + 0xA)) move_script::SetWord(move_script::At(::bof3::Img(Window17_addr) + 0x10), 0xF0);
    } else if (repeat & 0x4000) {
        if (cursor < 0x7F) {
            ++cursor;
            B(::bof3::Img(battle_e4::at::kCands.v) + 0xB) = cursor;
        }
        if (static_cast<int>(cursor) >= static_cast<int>(B(::bof3::Img(battle_e4::at::kCands.v) + 0xA)) + 7) move_script::SetWord(move_script::At(::bof3::Img(Window17_addr) + 0x10), 0x10);
    } else if (repeat & Sigma(4)) {
        const unsigned char top = B(::bof3::Img(battle_e4::at::kCands.v) + 0xA);
        if (top == 0) {
            cursor = 0;
            B(::bof3::Img(battle_e4::at::kCands.v) + 0xB) = cursor;
        } else if (top < 7) {
            cursor = static_cast<unsigned char>(cursor - top);
            B(::bof3::Img(battle_e4::at::kCands.v) + 0xA) = 0;
            B(::bof3::Img(battle_e4::at::kCands.v) + 0xB) = cursor;
        } else {
            cursor = static_cast<unsigned char>(cursor - 7);
            B(::bof3::Img(battle_e4::at::kCands.v) + 0xB) = cursor;
            B(::bof3::Img(battle_e4::at::kCands.v) + 0xA) = static_cast<unsigned char>(top - 7);
        }
    } else if (repeat & Sigma(8)) {
        const unsigned char top = B(::bof3::Img(battle_e4::at::kCands.v) + 0xA);
        if (top == 0x79) {
            cursor = 0x7F;
            B(::bof3::Img(battle_e4::at::kCands.v) + 0xB) = cursor;
        } else if (top > 0x72) {
            B(::bof3::Img(battle_e4::at::kCands.v) + 0xA) = 0x79;
            cursor = static_cast<unsigned char>(cursor + (0x79 - top));
            B(::bof3::Img(battle_e4::at::kCands.v) + 0xB) = cursor;
        } else {
            cursor = static_cast<unsigned char>(cursor + 7);
            B(::bof3::Img(battle_e4::at::kCands.v) + 0xB) = cursor;
            B(::bof3::Img(battle_e4::at::kCands.v) + 0xA) = static_cast<unsigned char>(top + 7);
        }
    }
    if (was != cursor) BH_CALL(Sound_PlayEffect)(0x100);
    const unsigned char item = B(::bof3::Img(battle_e4::at::kCands.v) + 0xD);
    const unsigned char category = B(::bof3::Img(battle_e4::at::kCands.v) + 8);
    const unsigned help = BH_CALL(Item_HelpMessage)(category, item);
    const unsigned char* const text = BH_CALL(Msg_SystemPtr)(help);
    move_script::SetLong(move_script::At(::bof3::Img(battle_e4::at::kQueue.v) + 4 + LastQueueEntry()), S32BattleE4(Low32(text)));
    bool back = false;
    if (Field_CancelButtons & Input_Pressed) {
        BH_CALL(Sound_PlayEffect)(0x106);
        back = true;
    } else {
        BH_CALL(BattleEquip_Preview)();
        if (Field_ConfirmButtons & Input_Pressed) {
            if (item != 0) {
                BH_CALL(Sound_PlayEffect)(0x103);
                BH_CALL(BattleEquip_Apply)();
                back = true;
            } else {
                BH_CALL(Sound_PlayEffect)(0x107);
            }
        }
    }
    if (!back) return;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 0xD) = 1;
    B(::bof3::Img(battle_e4::at::kCands.v) + 0xB) = 0xFF;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 0xB) = 0xFF;
    BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 - 1);
}

// original 0x4491D0 (hidden, 0x64E4B0's entry 3; PSX 0x80096B3C, gap86): the
// six slots for use. The hand and the message as the change's slots; cancel
// (cue 0x106): the slots closed, record 4 on, the list at +3 = 3, the window
// above and the hand off, state 1, sub-state 0. Confirm with a word in record
// 18's +0x10 (cue 0x103): the member's +0x130 bit 0x4000, its +0x126 and the
// command's word +2 that word, the item's help line (Item_HelpMessage by the
// command's +3, +2) into the last queue entry, the slots closed, record 17's
// +3 = 2, the hand and the window above off, record 4 on, state 7; without
// one, cue 0x107. Then up / down move the slot either way (SlotCursor).
extern "C" void __cdecl BattleItemCmd_EquipUsePick(void) {
    SlotHand();
    const unsigned char* const text = OptionMessage(0x30);
    const unsigned entry = LastQueueEntry();
    const unsigned pressed = Input_Pressed;
    move_script::SetLong(move_script::At(::bof3::Img(battle_e4::at::kQueue.v) + 4 + entry), S32BattleE4(Low32(text)));
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pressed & 0x5000);
    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        BH_CALL(Sound_PlayEffect)(0x106);
        B(::bof3::Img(battle_e4::at::kSlots.v) + 3) = 1;
        WindowRecords[4].in_use = 1;
        BattleE4_Step3 = 1;
        B(::bof3::Img(battle_e4::at::kList.v) + 3) = 3;
        WindowRecords[21].in_use = 0;
        WindowRecords[19].in_use = 0;
        BattleE4_Step4 = 0;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        if (move_script::Word(move_script::At(::bof3::Img(Window18_addr) + 0x10)) != 0) {
            BH_CALL(Sound_PlayEffect)(0x103);
            unsigned char* const actor = MenuActor();
            auto* const member_rec = rv::As<records::PartyMember>(PartyBattleE4(actor[5]));
            member_rec->flags_130 = S32BattleE4(static_cast<U>(static_cast<std::int32_t>(member_rec->flags_130)) | 0x4000);
            unsigned char* const m2 = PartyBattleE4(actor[5]);
            const unsigned word = move_script::Word(move_script::At(::bof3::Img(Window18_addr) + 0x10));
            rv::Store<unsigned>(rv::As<records::PartyMember>(m2)->queued_id, word);
            rv::Store<unsigned>(rv::As<records::ActionRecord>(CommandBattleE4())->action_id, word);
            const unsigned char item = CommandBattleE4()[2];
            const unsigned char category = CommandBattleE4()[3];
            const unsigned help = BH_CALL(Item_HelpMessage)(category, item);
            const unsigned char* const line = BH_CALL(Msg_SystemPtr)(help);
            const unsigned last = LastQueueEntry();
            B(::bof3::Img(battle_e4::at::kSlots.v) + 3) = 1;
            B(::bof3::Img(battle_e4::at::kCands.v) + 3) = 2;
            WindowRecords[19].in_use = 0;
            WindowRecords[21].in_use = 0;
            move_script::SetLong(move_script::At(::bof3::Img(battle_e4::at::kQueue.v) + 4 + last), S32BattleE4(Low32(line)));
            WindowRecords[4].in_use = 1;
            BattleE4_Step3 = 7;
        } else {
            BH_CALL(Sound_PlayEffect)(0x107);
        }
    }
    SlotCursor(repeat);
}

// original 0x4493E0 (hidden, BattleItemCmd_States entry 7; PSX 0x80096E48,
// gap86): state 7, the equipped item's target kind by its flag byte (0x591810
// of the member's +0x126 word) - BattleItemCmd_Choose's for the list item.
// Bit 0x40: state 8 or 9 ((f & 0x10 | 0x8F) >> 4) at sub-state 0, cue 0x103.
// Bit 0x80: every target (0xC0), state 8, sub-state 3. Else the side (bit
// 0x10: 0x40 / 0x80 by bit 0x20) or the member itself (its +5), state 8,
// sub-state 3.
extern "C" void __cdecl BattleItemCmd_EquipUseKind(void) {
    const unsigned word = MemberItem();
    const unsigned char f = ItemFlagsBattleE4(word >> 8, word);
    if (f & 0x40) {
        BattleE4_Step4 = 0;
        BattleE4_Step3 = static_cast<unsigned char>(((f & 0x10) | 0x8F) >> 4);
        BH_CALL(Sound_PlayEffect)(0x103);
        return;
    }
    if (f & 0x80) {
        CommandBattleE4()[0] = 0xC0;
        BattleE4_Step3 = 8;
        BattleE4_Step4 = 3;
        return;
    }
    const unsigned char target = (f & 0x10) ? SideTarget(f) : rv::As<records::PartyMember>(MenuActor())->actor;
    CommandBattleE4()[0] = target;
    BattleE4_Step3 = 8;
    BattleE4_Step4 = 3;
}

// original 0x449480 (hidden, BattleItemCmd_States entry 8; PSX 0x80096F58):
// state 8 by the sub-state (the dword 0x904AA4's low byte) through 0x64E4C0 -
// EquipTargetBegin, EquipPickEnemy, EquipPickMember, EquipCommit,
// EquipCancel; a Fatal past five.
extern "C" void __cdecl BattleItemCmd_EquipTargetDispatch(void) {
    DispatchStep4("BattleItemCmd_EquipTargetDispatch", ::bof3::Img(0x64E4C0), 5);
}

// original 0x4494A0 (hidden, 0x64E4C0's entry 0; PSX Item_TargetSetup
// 0x80096F94, the sibling's hypothesis name, read the same): the equipped
// item's target pick begun, as BattleItemCmd_TargetBegin for the list item.
// Flag bit 0x20: Battle_DefaultTarget(3), sub-state 1. Else target 0 while
// Battle_ReturnTrue answers non-zero, else Battle_DefaultTarget(0); sub-state
// 2. Both: the latch zeroed, the pick flag set.
extern "C" void __cdecl BattleItemCmd_EquipTargetBegin(void) {
    const unsigned word = MemberItem();
    if (ItemFlagsBattleE4(word >> 8, word) & 0x20) {
        const unsigned char t = BH_CALL(Battle_DefaultTarget)(3);
        CommandBattleE4()[0] = t;
        BattleE4_Step4 = 1;
        move_script::SetWord(move_script::At(::bof3::Img(battle_e4::at::kRepeatLatch.v)), 0);
        BattleE4_Picking = 1;
        return;
    }
    if (BH_CALL(Battle_ReturnTrue)() != 0) {
        CommandBattleE4()[0] = 0;
    } else {
        const unsigned char t = BH_CALL(Battle_DefaultTarget)(0);
        CommandBattleE4()[0] = t;
    }
    BattleE4_Step4 = 2;
    move_script::SetWord(move_script::At(::bof3::Img(battle_e4::at::kRepeatLatch.v)), 0);
    BattleE4_Picking = 1;
}

// original 0x449540 (hidden, 0x64E4C0's entry 1; PSX 0x8009706C, gap86): an
// enemy, as BattleItemCmd_PickEnemy with the equipped item's flags. Cancel ->
// sub-state 4, confirm -> 3; up / down (0x5000) with flag bit 0x80 cross to
// the party (target 0 while Battle_ReturnTrue answers, else
// Battle_DefaultTarget(0)), sub-state + 1, cue 0x101; then right / left.
extern "C" void __cdecl BattleItemCmd_EquipPickEnemy(void) {
    unsigned char* const actor = MenuActor();
    const unsigned pad = Input_Pressed;
    const unsigned word = rv::As<records::PartyMember>(PartyBattleE4(actor[5]))->queued_id;
    if (Field_CancelButtons & pad) {
        BattleE4_Step4 = 4;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        BattleE4_Step4 = 3;
        return;
    }
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pad & 0xF000);
    if ((repeat & 0x5000) && (ItemFlagsBattleE4(word >> 8, word) & 0x80)) {
        if (BH_CALL(Battle_ReturnTrue)() != 0) {
            CommandBattleE4()[0] = 0;
        } else {
            const unsigned char t = BH_CALL(Battle_DefaultTarget)(0);
            CommandBattleE4()[0] = t;
        }
        BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 + 1);
        BH_CALL(Sound_PlayEffect)(0x101);
        return;
    }
    StepEnemyTarget(repeat);
}

// original 0x449680 (hidden, 0x64E4C0's entry 2; PSX 0x8009721C, gap86): a
// member, as BattleItemCmd_PickMember with the equipped item's flags. Cancel
// -> 4, confirm -> 3; up / down with flag bit 0x80 cross to the enemies
// (Battle_DefaultTarget(3), sub-state - 1, cue 0x101). Right: the target one
// on wrapped to 0 .. party count - 1, as it is while Battle_ReturnTrue
// answers, else through Battle_DefaultTarget; cue 0x101. Left: one back, as it
// is (cue 0x101, done) or through Battle_PrevTarget (cue 0x101).
extern "C" void __cdecl BattleItemCmd_EquipPickMember(void) {
    unsigned char* const actor = MenuActor();
    const unsigned pad = Input_Pressed;
    const unsigned word = rv::As<records::PartyMember>(PartyBattleE4(actor[5]))->queued_id;
    if (Field_CancelButtons & pad) {
        BattleE4_Step4 = 4;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        BattleE4_Step4 = 3;
        return;
    }
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pad & 0xF000);
    if ((repeat & 0x5000) && (ItemFlagsBattleE4(word >> 8, word) & 0x80)) {
        const unsigned char t = BH_CALL(Battle_DefaultTarget)(3);
        CommandBattleE4()[0] = t;
        BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 - 1);
        BH_CALL(Sound_PlayEffect)(0x101);
        return;
    }
    auto high = [] { return static_cast<long>(static_cast<U>(move_script::Long(move_script::At(::bof3::Img(battle_e4::at::kPartyCount.v)))) & 0xFF) - 1; };
    if (repeat & 0x2000) {
        if (BH_CALL(Battle_ReturnTrue)() != 0) {
            const long h = high();
            const long value = static_cast<long>(static_cast<signed char>(CommandBattleE4()[0])) + 1;
            const long w = BH_CALL(Battle_WrapIndex)(h, 0, value);
            CommandBattleE4()[0] = static_cast<unsigned char>(w);
        } else {
            const long h = high();
            const long value = static_cast<long>(static_cast<signed char>(CommandBattleE4()[0])) + 1;
            const long w = BH_CALL(Battle_WrapIndex)(h, 0, value);
            const unsigned char t = BH_CALL(Battle_DefaultTarget)(static_cast<unsigned>(w));
            CommandBattleE4()[0] = t;
        }
        BH_CALL(Sound_PlayEffect)(0x101);
    }
    if (repeat & 0x8000) {
        if (BH_CALL(Battle_ReturnTrue)() != 0) {
            const long h = high();
            const long value = static_cast<long>(static_cast<signed char>(CommandBattleE4()[0])) - 1;
            const long w = BH_CALL(Battle_WrapIndex)(h, 0, value);
            CommandBattleE4()[0] = static_cast<unsigned char>(w);
            BH_CALL(Sound_PlayEffect)(0x101);
            return;
        }
        const long h = high();
        const long value = static_cast<long>(static_cast<signed char>(CommandBattleE4()[0])) - 1;
        const long w = BH_CALL(Battle_WrapIndex)(h, 0, value);
        const unsigned char t = BH_CALL(Battle_PrevTarget)(static_cast<unsigned>(w));
        CommandBattleE4()[0] = t;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
}

// original 0x449830 (hidden, 0x64E4C0's entry 3 and 0x64E4D4's entry 2; PSX
// 0x80097430, gap86): the commit, as BattleItemCmd_Commit without spending an
// item: cue 0x104; the pick flag cleared; the command's +1 = 5; the member's
// +1 = 2; one more command chosen; the last queue entry's +1 = 1;
// ItemMenu_FreeWindows; 0x904AA2..4 = 0, 0x904AA1 = 1.
extern "C" void __cdecl BattleItemCmd_EquipCommit(void) {
    BH_CALL(Sound_PlayEffect)(0x104);
    unsigned char* const command = CommandBattleE4();
    BattleE4_Picking = 0;
    command[1] = 5;
    rv::As<records::PartyMember>(MenuActor())->state = 2;
    BattleActorCopies_EntryCount = static_cast<unsigned char>(static_cast<unsigned char>(BattleActorCopies_EntryCount) + 1);
    B(::bof3::Img(battle_e4::at::kQueue.v) + 1 + LastQueueEntry()) = 1;
    BH_CALL(ItemMenu_FreeWindows)();
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
    BattleActions_Step = 1;
}

// original 0x4498A0 (hidden, 0x64E4C0's entry 4 and 0x64E4D4's entry 3; PSX
// 0x800974E0, gap86): the pick cancelled, as 0x448B40 for the list item: cue
// 0x106, BattleBanner_ShowName(the member), the pick flag cleared, the list's
// +3 = 2 and word +4 = 0xFF38, 0x904AA1 = 4, 0x904AA2 = 2, 0x904AA3 = 1,
// 0x904AA4 = 0.
extern "C" void __cdecl BattleItemCmd_EquipCancel(void) {
    BH_CALL(Sound_PlayEffect)(0x106);
    BH_CALL(BattleBanner_ShowName)(MenuActor());
    BattleE4_Picking = 0;
    B(::bof3::Img(battle_e4::at::kList.v) + 3) = 2;
    WindowRecords[0x10].x = 0xFF38;
    BattleActions_Step = 4;
    BattleE4_Step2 = 2;
    BattleE4_Step3 = 1;
    BattleE4_Step4 = 0;
}

// original 0x4498F0 (hidden, BattleItemCmd_States entry 9; PSX 0x80097558):
// state 9 by the sub-state through 0x64E4D4 - EquipSideBegin, EquipSidePick,
// EquipCommit, EquipCancel; a Fatal past four.
extern "C" void __cdecl BattleItemCmd_EquipSideDispatch(void) {
    DispatchStep4("BattleItemCmd_EquipSideDispatch", ::bof3::Img(0x64E4D4), 4);
}

// original 0x449910 (hidden, 0x64E4D4's entry 0; PSX 0x80097594, gap86): a
// whole side for the equipped item, as BattleItemCmd_SideBegin.
extern "C" void __cdecl BattleItemCmd_EquipSideBegin(void) {
    const unsigned word = MemberItem();
    const unsigned char flags = ItemFlagsBattleE4(word >> 8, word);
    CommandBattleE4()[0] = SideTarget(flags);
    BattleE4_Step4 = 1;
    move_script::SetWord(move_script::At(::bof3::Img(battle_e4::at::kRepeatLatch.v)), 0);
    BattleE4_Picking = 1;
}

// original 0x449970 (hidden, 0x64E4D4's entry 1; PSX 0x8009762C, gap86): the
// side pick, as BattleItemCmd_SidePick: cancel -> 3, confirm -> 2, a direction
// with flag bit 0x80 flips the side, cue 0x101.
extern "C" void __cdecl BattleItemCmd_EquipSidePick(void) {
    unsigned char* const actor = MenuActor();
    const unsigned pad = Input_Pressed;
    const unsigned word = rv::As<records::PartyMember>(PartyBattleE4(actor[5]))->queued_id;
    if (Field_CancelButtons & pad) {
        BattleE4_Step4 = 3;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        BattleE4_Step4 = 2;
        return;
    }
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pad & 0xF000);
    if ((repeat & 0xF000) == 0) return;
    if (ItemFlagsBattleE4(word >> 8, word) & 0x80) {
        CommandBattleE4()[0] ^= 0xC0;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
}

// original 0x449A00 (PSX 0x8009770C): the previewed equipment put on. For each
// of the six slots whose preview byte (0x675F18) is set and differs from the
// character's (+0x12 + slot): the new one taken from the inventory
// (Inventory_Remove(0x64E4E4's category, new, 1)), the old one given back
// (Inventory_Add(category, old, 1)), the slot set. Then Char_RecalcStats of the
// record and the members refreshed (RefreshMembers). The original pushes a
// fourth word, 0, to both inventory calls; neither reads it.
extern "C" void __cdecl BattleEquip_Apply(void) {
    unsigned char* const r = SlotsCharacter();
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned char n = B(::bof3::Img(battle_e4::at::kPreview.v) + i);
        if (n == 0 || n == r[0x12 + i]) continue;
        BH_CALL(Inventory_Remove)(B(::bof3::Img(battle_e4::at::kEquipCats.v) + i), n, 1);
        const unsigned char old = r[0x12 + i];
        BH_CALL(Inventory_Add)(B(::bof3::Img(battle_e4::at::kEquipCats.v) + i), old, 1);
        r[0x12 + i] = B(::bof3::Img(battle_e4::at::kPreview.v) + i);
    }
    BH_CALL(Char_RecalcStats)(r);
    RefreshMembers();
}

// original 0x449BB0 (PSX 0x80097AB0): the candidate previewed. 0x8033F5 = 1
// when Item_EquipMask(record 17's category, item) has no bit for the
// character (1 << its index, an 8-bit shift: none from index 8 on); the six
// preview bytes 0x675F18 the character's +0x12..+0x17, the one at the slot
// cursor the candidate.
extern "C" void __cdecl BattleEquip_Preview(void) {
    const unsigned member = static_cast<U>(move_script::Long(move_script::At(::bof3::Img(Window18_addr) + 0xC))) & 0xFF;
    const unsigned char item = B(::bof3::Img(battle_e4::at::kCands.v) + 0xD);
    const unsigned char c = Character_IndexById[Cond_Flags[0xd5 + member]];
    const unsigned mask = BH_CALL(Item_EquipMask)(B(::bof3::Img(battle_e4::at::kCands.v) + 8), item);
    const auto bit = static_cast<unsigned char>((1u << (c & 0x1F)) & 0xFF);
    B(::bof3::Img(battle_e4::at::kSlots.v) + 0xD) = (bit & mask & 0xFF) == 0 ? 1 : 0;
    const unsigned char* const r = CharRecord(c);
    for (unsigned i = 0; i < 6; ++i) B(::bof3::Img(battle_e4::at::kPreview.v) + i) = r[0x12 + i];
    B(::bof3::Img(battle_e4::at::kPreview.v) + B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA)) = item;
}

// original 0x449C70 (PSX 0x80097BE0): the slot at the cursor taken off - not
// slot 0, and not an empty one (al 0). The item given back
// (Inventory_Add(0x64E4EC's category, item, 1); a fourth word 0 unread), the
// slot cleared at the cursor read again, Char_RecalcStats, the members
// refreshed; al 1. The original indexes six stack pointers by the cursor
// unchecked (past 5 it follows whatever the stack holds): ours aborts.
extern "C" unsigned char __cdecl BattleEquip_RemoveSlot(void) {
    unsigned char* const r = SlotsCharacter();
    auto slot = [](const char* when) {
        const unsigned char s = B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA);
        if (s > 5)
            bof3::Fatal("BattleEquip_RemoveSlot: the slot cursor 0x8033F2 is %u %s - the original indexes its six stack "
                        "pointers past the sixth (docs/battle_e4.md section 7)",
                        s, when);
        return s;
    };
    const unsigned char s = slot("at the start");
    const unsigned char item = r[0x12 + s];
    if (item == 0 || s == 0) return 0;
    BH_CALL(Inventory_Add)(B(::bof3::Img(battle_e4::at::kUnequipCats.v) + s), item, 1);
    r[0x12 + slot("after Inventory_Add")] = 0;
    BH_CALL(Char_RecalcStats)(r);
    RefreshMembers();
    return 1;
}

// original 0x449E00 (PSX 0x80097F44, `return 1` there too): al = 1. Thirteen
// callers, every one of which tests al: Item_TargetSetup's "0x80097F44() ? 0
// : .." on the PSX, where the answer is always the party side.
extern "C" unsigned char __cdecl Battle_ReturnTrue(void) { return 1; }

// original 0x449E10 (PSX 0x80097F4C, ItemMenu_Open's call): item window record
// 16 for the party's own inventory: +0 = 1, +2 = 1, +0xA / +0xB / +0xC the bytes
// kPartyPages..+2, +3 = 2, +8 = 2, +1 = 8, +0xD = 0xFF, +9 = 0, word +0x10 = 0,
// word +4 = 0xFF56, word +6 = 0x3F, 0x80340C = 0, 0x80340D = 8, 0x80340E = 0.
// eax is 0 at the end (xor eax, eax); ours returns it.
extern "C" unsigned long __cdecl ItemMenu_SetupForParty(void) {
    unsigned char* const w = AtBattleMisc(::bof3::Img(Window16_addr));
    auto* const window_rec = rv::As<records::WindowRecord>(w);
    const unsigned char a = BattleMisc_PartyPages;
    window_rec->in_use = 1;
    window_rec->kind = 1;
    w[0xA] = a;
    const unsigned char b = AtBattleMisc(::bof3::Img(BattleMisc_PartyPages_addr) + 1)[0];
    w[0xB] = b;
    const unsigned char c = AtBattleMisc(::bof3::Img(BattleMisc_PartyPages_addr) + 2)[0];
    window_rec->state = 2;
    w[8] = 2;
    window_rec->handler = 8;
    w[0xC] = c;
    w[0xD] = 0xFF;
    w[9] = 0;
    PutWord(w + 0x10, 0);
    window_rec->x = 0xFF56;
    window_rec->y = 0x3F;
    WindowRecords[19].in_use = 0;
    WindowRecords[0x13].handler = 8;
    WindowRecords[0x13].kind = 0;
    return 0;
}

// original 0x449E90 (PSX 0x80098004): the slots opened for a change - record
// 18 (+0 1, +1 8, +2 3, +3 2; cursor +0xA 0, chosen +0xB 0xFF, member +0xC the
// member's +5, +0xD 1; dword +0x20 0x675F18, words +4 0xFF56, +6 0x3F) and
// record 17 (+0 1, +3 1, +1 8, +2 4; +8..+0xD 0; words +4 / +6 the list's),
// the list off with its word +4 = 0x140, 0x929F06 the member's +5.
extern "C" void __cdecl BattleEquip_OpenChange(void) {
    unsigned char* const actor = MenuActor();
    WindowRecords[18].in_use = 1;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 1) = 8;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 2) = 3;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 3) = 2;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA) = 0;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 0xB) = 0xFF;
    const unsigned char member = actor[5];
    B(::bof3::Img(battle_e4::at::kSlots.v) + 0xD) = 1;
    WindowRecords[17].in_use = 1;
    B(::bof3::Img(battle_e4::at::kCands.v) + 3) = 1;
    const unsigned x = WindowRecords[0x10].x;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 0xC) = member;
    WindowRecords[0x11].x = x;
    const unsigned y = WindowRecords[0x10].y;
    move_script::SetLong(move_script::At(::bof3::Img(Window18_addr) + 0x20), static_cast<std::int32_t>(::bof3::Img(battle_e4::at::kPreview.v)));
    WindowRecords[0x12].x = 0xFF56;
    WindowRecords[0x12].y = 0x3F;
    B(::bof3::Img(battle_e4::at::kCands.v) + 1) = 8;
    B(::bof3::Img(battle_e4::at::kCands.v) + 2) = 4;
    B(::bof3::Img(battle_e4::at::kCands.v) + 0xC) = 0;
    B(::bof3::Img(battle_e4::at::kCands.v) + 0xA) = 0;
    B(::bof3::Img(battle_e4::at::kCands.v) + 0xB) = 0;
    B(::bof3::Img(battle_e4::at::kCands.v) + 8) = 0;
    B(::bof3::Img(battle_e4::at::kCands.v) + 0xD) = 0;
    B(::bof3::Img(battle_e4::at::kCands.v) + 9) = 0;
    WindowRecords[0x11].y = y;
    WindowRecords[16].in_use = 0;
    WindowRecords[0x10].x = 0x140;
    BattleActorCopies_ItemActor = actor[5];
}

// original 0x449F60 (PSX 0x80098128): the slots opened for use - record 18 as
// for a change but +3 3, and +0xD left 3; the list's +3 = 4; 0x929F06 the
// member's +5.
extern "C" void __cdecl BattleEquip_OpenUse(void) {
    unsigned char* const actor = MenuActor();
    WindowRecords[18].in_use = 1;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 1) = 8;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 2) = 3;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 3) = 3;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 0xA) = 0;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 0xB) = 0xFF;
    const unsigned char member = actor[5];
    B(::bof3::Img(battle_e4::at::kSlots.v) + 0xD) = 1;
    B(::bof3::Img(battle_e4::at::kSlots.v) + 0xC) = member;
    move_script::SetLong(move_script::At(::bof3::Img(Window18_addr) + 0x20), static_cast<std::int32_t>(::bof3::Img(battle_e4::at::kPreview.v)));
    WindowRecords[0x12].x = 0xFF56;
    WindowRecords[0x12].y = 0x3F;
    B(::bof3::Img(battle_e4::at::kList.v) + 3) = 4;
    BattleActorCopies_ItemActor = actor[5];
    B(::bof3::Img(battle_e4::at::kSlots.v) + 0xD) = 3;
}

// original 0x449FE0 (PSX 0x800981D4, records 0x10..0x13 there too): window
// records 16..19 freed - each made kWindowCurrent (the address in a register,
// not re-read) and Window_FreeCurrent called. Two of the callers tail-jump
// here, so the last call's eax is returned.
extern "C" unsigned long __cdecl ItemMenu_FreeWindows(void) {
    battle_misc::U eax = 0;
    for (battle_misc::U k = 0; k < 4; ++k) {
        BattleWinStates_Current = ::bof3::Img(Window16_addr) + k * 0x24;
        eax = battle_misc::g.free_window();
    }
    return eax;
}

// original 0x44A000 (Battle_MenuSteps[6]; PSX 0x8009823C, paired): jmp
// [0x64E4FC + 4 * byte 0x904AA3] - Escape_States' three.
extern "C" void __cdecl Escape_Dispatch(void) { DispatchRest3b("Escape_Dispatch", ::bof3::Img(BattleE4_Step3_addr), ::bof3::Img(Escape_States_addr), 3); }

// original 0x44A010 (hidden, 0x64E4FC's entry 0; PSX Escape_Roll 0x80098278,
// the sibling's name): the attempts 0x904AE6 counted. In an event battle
// (0x904AAA set) state + 1 - the failed path. From the third attempt on,
// state 2 - the escaped path. Else the enemies' mean u16 +0xB8 (actors 3..10
// not out, over 0x904AB3) against the party's mean +0xA8 (the members not out,
// over 0x904AB1): Escape_Chance of the difference; state 2 when Rand's low six
// bits are not above it, else 2 with 0x904AE4 == 1 and 1 without. As the
// original has it: the enemies' sum reads object actor (not actor - 3) - the
// objects 3..10, three of them past the eight - while Battle_ActorIsOut is asked
// of the actor; and both divisors are unchecked (ours aborts at 0).
extern "C" void __cdecl Escape_Roll(void) {
    const auto tries = static_cast<unsigned char>(BattlePhases_AE6 + 1);
    const unsigned char fight = EventBattle_Id;
    BattlePhases_AE6 = tries;
    if (fight != 0) {
        BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
        return;
    }
    if (BattlePhases_AE6 >= 3) {
        BattleE4_Step3 = 2;
        return;
    }
    U enemies = 0;
    for (unsigned a = 3; a <= 10; ++a) {
        const bool out = BH_CALL(Battle_ActorIsOut)(a) != 0;
        if (!out) enemies += move_script::Word(move_script::At(::bof3::Img(Battle_Enemies_addr) + a * battle_e4::at::kEnemyStride + 0xB8));
    }
    const unsigned e_div = Battle_EnemiesLeft;
    if (e_div == 0)
        bof3::Fatal("Escape_Roll: the enemies left 0x904AB3 is 0 - the original divides by it (docs/battle_e4.md section 7)");
    const U e_mean = enemies / e_div;
    U party = 0;
    if (B(::bof3::Img(battle_e4::at::kPartyCount.v)) != 0) {
        for (unsigned m = 0;;) {
            if (BH_CALL(Battle_ActorIsOut)(m) == 0) party += rv::As<records::PartyMember>(PartyBattleE4(m))->agi;
            ++m;
            if (!(static_cast<int>(m) < static_cast<int>(static_cast<U>(move_script::Long(move_script::At(::bof3::Img(battle_e4::at::kPartyCount.v)))) & 0xFF))) break;
        }
    }
    const unsigned p_div = Battle_PartyStanding;
    if (p_div == 0)
        bof3::Fatal("Escape_Roll: the party divisor 0x904AB1 is 0 - the original divides by it (docs/battle_e4.md section 7)");
    const U diff = party / p_div - e_mean;
    const unsigned chance = BH_CALL(Escape_Chance)(static_cast<int>(diff)) & 0xFF;
    const int roll = BH_CALL(Rand)() & 0x3F;
    if (roll > static_cast<int>(chance)) {
        BattleE4_Step3 = static_cast<unsigned char>((BattlePhases_Initiative == 1 ? 1 : 0) + 1);
        return;
    }
    BattleE4_Step3 = 2;
}

// original 0x44A130 (hidden, 0x64E4FC's entry 1; PSX 0x80098414): state 1,
// the failed path, by the sub-state through 0x64E508 - Escape_Begin,
// Escape_StepBack, Escape_StepOn, Escape_Failed; a Fatal past four.
extern "C" void __cdecl Escape_FailDispatch(void) { DispatchStep4("Escape_FailDispatch", AddressOf(Escape_FailSteps), 4); }

// original 0x44A150 (hidden, 0x64E508's entry 0 and 0x64E518's entry 0; PSX
// Escape_Begin 0x80098450, the sibling's name): the banner of 0x669E08's text
// (BattleBanner_Add(1, 0, 0, 0x3C, it); 0x93B8E0 = 0 first); window 4's +3 =
// 1; MoveScript_F3Divisor 0x20; the field kind-2 x / z high words stepped by
// 0x64E4F4's pair (FieldStep); MoveScript_FAWord the elevation there less
// MapView_Elevation, >> 5, on the escaped path (state 2), else 0;
// LoadDatFile(0xD1); each of the three members with command 5 (+0x125) has
// its item given back (Battle_ReturnItem(+0x12E, +0x126)); then each of the
// count's members not out turns (+8 ^ 2) with +0x125 = 0, +1 = 2, +2..+4 = 0;
// sub-state + 1.
extern "C" void __cdecl Escape_Begin(void) {
    const auto text = reinterpret_cast<const char*>(BattleE1_BannerText.Native());
    B(::bof3::Img(battle_e4::at::kFlee.v)) = 0;
    BH_CALL(BattleBanner_Add)(1, 0, 0, 0x3C, text);
    WindowRecords[4].state = 1;
    MoveScript_F3Divisor = 0x20;
    const unsigned char state = BattleE4_Step3;
    FieldStep(1);
    if (state == 2) {
        const long e = BH_CALL(AreaMap_Elevation)(Field_Kind2X, Field_Kind2Z);
        const auto d = static_cast<short>(static_cast<unsigned short>(static_cast<U>(e) - move_script::Word(move_script::At(::bof3::Img(MapView_Elevation_addr)))));
        MoveScript_FAWord = static_cast<unsigned short>(static_cast<short>(d >> 5));
    } else {
        MoveScript_FAWord = 0;
    }
    BH_CALL(LoadDatFile)(0xD1);
    for (unsigned k = 0; k < 3; ++k) {
        unsigned char* const p = PartyBattleE4(k);
        if (p[0x125] == 5) BH_CALL(Battle_ReturnItem)(p[0x12E], rv::As<records::PartyMember>(p)->queued_id);
    }
    if (B(::bof3::Img(battle_e4::at::kPartyCount.v)) != 0) {
        for (unsigned char m = 0;;) {
            if (BH_CALL(Battle_ActorIsOut)(m) == 0) {
                unsigned char* const p = PartyBattleE4(m);
                auto* const member_rec = rv::As<records::PartyMember>(p);
                member_rec->direction ^= 2;
                p[0x125] = 0;
                member_rec->state = 2;
                member_rec->step = 0;
                member_rec->state3 = 0;
                member_rec->state4 = 0;
            }
            ++m;
            if (m >= B(::bof3::Img(battle_e4::at::kPartyCount.v))) break;
        }
    }
    BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 + 1);
}

// original 0x44A2E0 (hidden, 0x64E508's entry 1; PSX 0x800986C8): unless
// Field_Kind2Hold is set, MoveScript_F3Divisor 0x80, the step taken back
// (FieldStep(-1)), sub-state + 2.
extern "C" void __cdecl Escape_StepBack(void) {
    if (Field_Kind2Hold != 0) return;
    MoveScript_F3Divisor = 0x80;
    const unsigned char sub = BattleE4_Step4;
    FieldStep(-1);
    BattleE4_Step4 = static_cast<unsigned char>(sub + 2);
}

// original 0x44A330 (hidden, 0x64E508's entry 2; PSX 0x8009875C): unless
// Field_Kind2Hold is set, MoveScript_F3Divisor 0x40, the step taken again,
// sub-state + 1.
extern "C" void __cdecl Escape_StepOn(void) {
    if (Field_Kind2Hold != 0) return;
    MoveScript_F3Divisor = 0x40;
    const unsigned char sub = BattleE4_Step4;
    FieldStep(1);
    BattleE4_Step4 = static_cast<unsigned char>(sub + 1);
}

// original 0x44A380 (hidden, 0x64E508's entry 3; PSX Escape_Failed
// 0x800987F0, the sibling's hypothesis name, read the same): unless
// Field_Kind2Hold is set, Battle_BuildTurnOrder; each of the count's members
// not out turns back (+8 ^ 2), +1 = 2, its queued item returned
// (Battle_ReturnQueuedItem) and it leaves the turn order
// (Battle_RemoveFromTurnOrder); the message window, the banner of system
// message 0xF; the round flag 8; phase 3 with its four steps 0.
extern "C" void __cdecl Escape_Failed(void) {
    if (Field_Kind2Hold != 0) return;
    BH_CALL(Battle_BuildTurnOrder)();
    if (B(::bof3::Img(battle_e4::at::kPartyCount.v)) != 0) {
        for (unsigned char m = 0;;) {
            if (BH_CALL(Battle_ActorIsOut)(m) == 0) {
                auto* const member_rec = rv::As<records::PartyMember>(PartyBattleE4(m));
                member_rec->direction ^= 2;
                member_rec->state = 2;
                BH_CALL(Battle_ReturnQueuedItem)(m);
                BH_CALL(Battle_RemoveFromTurnOrder)(m);
            }
            ++m;
            if (m >= B(::bof3::Img(battle_e4::at::kPartyCount.v))) break;
        }
    }
    BH_CALL(Battle_OpenMsgWindow)();
    const auto* const line = reinterpret_cast<const char*>(BH_CALL(Msg_SystemPtr)(0xF));
    BH_CALL(BattleBanner_Add)(2, 0, 0, 0x2D, line);
    B(::bof3::Img(battle_e4::at::kFlags.v)) |= 8;
    B(::bof3::Img(battle_e4::at::kPhase.v)) = 3;
    BattleActions_Step = 0;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
}

// original 0x44A450 (hidden, 0x64E4FC's entry 2; PSX 0x8009892C, gap44):
// state 2, the escaped path, through 0x64E518 - Escape_Begin, Escape_Leave,
// Escape_End; a Fatal past three.
extern "C" void __cdecl Escape_WinDispatch(void) { DispatchStep4("Escape_WinDispatch", AddressOf(Escape_WinSteps), 3); }

// original 0x44A470 (hidden, 0x64E518's entry 1; PSX 0x80098968): unless
// Field_Kind2Hold is set, MapView_SetElevation of the elevation at the kind-2
// x / z, every enemy's tint released (Sprite_ReleaseTint on the eight
// objects), 0x494E70 (the eight enemies' +0..+3 zeroed), the message window,
// the banner of system message 0x10, sub-state + 1.
extern "C" void __cdecl Escape_Leave(void) {
    if (Field_Kind2Hold != 0) return;
    const long e = BH_CALL(AreaMap_Elevation)(Field_Kind2X, Field_Kind2Z);
    BH_CALL(MapView_SetElevation)(static_cast<int>(e));
    for (unsigned k = 0; k < 8; ++k) BH_CALL(Sprite_ReleaseTint)(move_script::At(::bof3::Img(Battle_Enemies_addr) + k * battle_e4::at::kEnemyStride));
    BH_AT(VoidFn, battle_e4::at::kEnemiesClear.v)();
    BH_CALL(Battle_OpenMsgWindow)();
    const auto* const line = reinterpret_cast<const char*>(BH_CALL(Msg_SystemPtr)(0x10));
    BH_CALL(BattleBanner_Add)(2, 0, 0, 0x2D, line);
    BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 + 1);
}

// original 0x44A4F0 (hidden, 0x64E518's entry 2; PSX 0x80098A44): once
// 0x904AE9 is 0, MoveScript_F3Divisor 0 and the end phase at step 3 (phase 5,
// 0x904AA1 = 3, the other three steps 0).
extern "C" void __cdecl Escape_End(void) {
    if (BattleMisc_BannerKinds != 0) return;
    MoveScript_F3Divisor = 0;
    B(::bof3::Img(battle_e4::at::kPhase.v)) = 5;
    BattleActions_Step = 3;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
}

// original 0x44A520 (PSX Escape_Chance 0x80098A94, the sibling's name, read
// the same): the row index (diff + 0x20) / 16 (toward 0): below 0 gives 0x1C,
// above 5 0x38, else 0x64E524's byte; + 8 on the second attempt (0x904AE6 ==
// 2); + 8 once for the first member not out with +0x91 bit 0x20; capped at
// 0x40. al out.
extern "C" unsigned char __cdecl Escape_Chance(int diff) {
    const int index = S32BattleE4(static_cast<U>(diff) + 0x20) / 16;   // the add wraps, as the original's
    unsigned char c = 0;
    if (index < 0) c = 0x1C;
    if (index > 5) c = 0x38;
    else if (c == 0) c = B(::bof3::Img(battle_e4::at::kChanceRow.v) + static_cast<U>(index));
    if (BattlePhases_AE6 == 2) c = static_cast<unsigned char>(c + 8);
    if (B(::bof3::Img(battle_e4::at::kPartyCount.v)) != 0) {
        for (unsigned char m = 0;;) {
            if (BH_CALL(Battle_ActorIsOut)(m) == 0 && (PartyBattleE4(m)[0x91] & 0x20)) {
                c = static_cast<unsigned char>(c + 8);
                break;
            }
            ++m;
            if (m >= B(::bof3::Img(battle_e4::at::kPartyCount.v))) break;
        }
    }
    return c > 0x40 ? 0x40 : c;
}

void BattleItemCmd_Inject() {
    BOF3_INJECT(BattleTarget_PickDispatch);
    BOF3_INJECT(BattleTarget_Begin);
    BOF3_INJECT(BattleTarget_PickEnemy);
    BOF3_INJECT(BattleTarget_PickParty);
    BOF3_INJECT(BattleTarget_Confirm);
    BOF3_INJECT(BattleTarget_Cancel);
    BOF3_INJECT(BattleItem_Dispatch);
    BOF3_INJECT(BattleItem_OpenDispatch);
    BOF3_INJECT(BattleItem_OpenWindow);
    BOF3_INJECT(BattleItem_Browse);
    BOF3_INJECT(ItemMenu_CanUseSelected);
    BOF3_INJECT(BattleItem_CloseWait);
    BOF3_INJECT(BattleItem_Choose);
    BOF3_INJECT(BattleItem_TargetDispatch);
    BOF3_INJECT(BattleItem_TargetBegin);
    BOF3_INJECT(BattleItem_PickEnemy);
    BOF3_INJECT(BattleItem_PickParty);
    BOF3_INJECT(BattleItem_Confirm);
    BOF3_INJECT(BattleItem_TargetCancel);
    BOF3_INJECT(BattleItem_SideDispatch);
    BOF3_INJECT(BattleItem_SideBegin);
    BOF3_INJECT(BattleItem_SidePick);
    BOF3_INJECT(ItemMenu_SetupForActor);
    BOF3_INJECT(ItemMenu_SetupForMember);
    BOF3_INJECT(BattleAttackCmd_Dispatch);
    BOF3_INJECT(BattleAttackCmd_Begin);
    BOF3_INJECT(BattleAttackCmd_PickEnemy);
    BOF3_INJECT(BattleAttackCmd_Confirm);
    BOF3_INJECT(BattleAttackCmd_Cancel);
    BOF3_INJECT(BattleItemCmd_Dispatch);
    BOF3_INJECT(BattleItemCmd_OpenDispatch);
    BOF3_INJECT(BattleItemCmd_QueuePrompt);
    BOF3_INJECT(BattleItemCmd_OpenList);
    BOF3_INJECT(BattleItemCmd_List);
    BOF3_INJECT(BattleItemCmd_CloseWait);
    BOF3_INJECT(BattleItemCmd_TargetKind);
    BOF3_INJECT(BattleItemCmd_TargetDispatch);
    BOF3_INJECT(BattleItemCmd_TargetBegin);
    BOF3_INJECT(BattleItemCmd_PickEnemy);
    BOF3_INJECT(BattleItemCmd_PickMember);
    BOF3_INJECT(BattleItemCmd_Commit);
    BOF3_INJECT(BattleItemCmd_TargetCancel);
    BOF3_INJECT(BattleItemCmd_SideDispatch);
    BOF3_INJECT(BattleItemCmd_SideBegin);
    BOF3_INJECT(BattleItemCmd_SidePick);
    BOF3_INJECT(BattleItemCmd_EquipDispatch);
    BOF3_INJECT(BattleItemCmd_EquipMenu);
    BOF3_INJECT(BattleItemCmd_EquipSlotPick);
    BOF3_INJECT(BattleItemCmd_EquipItemPick);
    BOF3_INJECT(BattleItemCmd_EquipUsePick);
    BOF3_INJECT(BattleItemCmd_EquipUseKind);
    BOF3_INJECT(BattleItemCmd_EquipTargetDispatch);
    BOF3_INJECT(BattleItemCmd_EquipTargetBegin);
    BOF3_INJECT(BattleItemCmd_EquipPickEnemy);
    BOF3_INJECT(BattleItemCmd_EquipPickMember);
    BOF3_INJECT(BattleItemCmd_EquipCommit);
    BOF3_INJECT(BattleItemCmd_EquipCancel);
    BOF3_INJECT(BattleItemCmd_EquipSideDispatch);
    BOF3_INJECT(BattleItemCmd_EquipSideBegin);
    BOF3_INJECT(BattleItemCmd_EquipSidePick);
    BOF3_INJECT(BattleEquip_Apply);
    BOF3_INJECT(BattleEquip_Preview);
    BOF3_INJECT(BattleEquip_RemoveSlot);
    BOF3_INJECT(Battle_ReturnTrue);
    BOF3_INJECT(ItemMenu_SetupForParty);
    BOF3_INJECT(BattleEquip_OpenChange);
    BOF3_INJECT(BattleEquip_OpenUse);
    BOF3_INJECT(ItemMenu_FreeWindows);
    BOF3_INJECT(Escape_Dispatch);
    BOF3_INJECT(Escape_Roll);
    BOF3_INJECT(Escape_FailDispatch);
    BOF3_INJECT(Escape_Begin);
    BOF3_INJECT(Escape_StepBack);
    BOF3_INJECT(Escape_StepOn);
    BOF3_INJECT(Escape_Failed);
    BOF3_INJECT(Escape_WinDispatch);
    BOF3_INJECT(Escape_Leave);
    BOF3_INJECT(Escape_End);
    BOF3_INJECT(Escape_Chance);
}
