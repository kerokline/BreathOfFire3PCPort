// Moved here by tools/move_funcs.py from battle_actions.cpp, battle_e1.cpp, battle_phases.cpp, battle_setup.cpp, battle_turn_steps.cpp, rest_3a.cpp (docs/source-layout-round.md).
#include "game/battle/battle_action.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/battle/battle_action_callees.h"
#include "game/battle_actions.h"
#include "game/battle_actions_callees.h"
#include "game/battle_e1_callees.h"
#include "game/battle_phases.h"
#include "game/battle_phases_callees.h"
#include "game/battle_setup_callees.h"
#include "game/battle_turn_steps.h"
#include "game/battle_turn_steps_callees.h"
#include "game/boss_harness.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/psp_code.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3a_callees.h"
#include "game/sound.h"
#include "game/sound_st.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = battle_action::at;

}  // namespace

using namespace battle_actions;

using namespace battle_phases;

using namespace battle_setup;

using namespace battle_turn_steps;

namespace battle_actions {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

}  // namespace battle_actions

// DIVERGENCE DIV-0179, the unlearned skill in blue (BOF3X_UNLEARNED_BLUE=1,
// unlearned_blue= in bof3x.ini; off by default). The banner pool 0x93B8E0, 8
// entries of 0xC: +0xA is the text colour BattleWin_BannerRun 0x597C70 hands
// Text_DrawAt, which FillBanner always sets to 0. Colour 3 is the sky-blue
// row of FIRST.DAT's text CLUT strip (kind 0, tag 0x8000).
namespace battle_actions { bool g_unlearned_blue = false; }

namespace {

unsigned char B(std::uint32_t address) { return battle_actions::At(address)[0]; }

void SetB(std::uint32_t address, unsigned v) { battle_actions::At(address)[0] = static_cast<unsigned char>(v); }

unsigned char* PtrAt(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(battle_actions::Long(battle_actions::At(address)))));
}

void SetPtrAt(std::uint32_t address, const unsigned char* p) {
    battle_actions::SetLong(battle_actions::At(address), static_cast<std::int32_t>(Low32(p)));
}

std::int32_t L(std::uint32_t address) { return battle_actions::Long(battle_actions::At(address)); }

// The actor objects, by the actor byte: a member's (0..2), or an enemy's
// (actor - 3, as a 32-bit difference - the originals' `sub eax, 3` after
// `and eax, 0xFF`).
unsigned char* Party(unsigned actor) { return battle_actions::At(::bof3::Img(ObjTrio_addr) + (actor & 0xFF) * battle_actions::at::kPartyStride); }

unsigned char* Enemy(unsigned actor) { return battle_actions::At(::bof3::Img(Battle_Enemies_addr) + ((actor & 0xFF) - 3u) * battle_actions::at::kEnemyStride); }

// One entry of a step table in .data, read afresh; the byte picks it and is
// not checked (a byte past the table's count reaches the next table's
// entries, or its data, as in the original).
using Handler = void (__cdecl*)();

Handler StepBattleActions(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(L(table + (index & 0xFF) * 4))));
}

using HookFn = void (__cdecl*)(int);

void EventHook(int n) { reinterpret_cast<HookFn>(PtrAt(::bof3::Img(battle_actions::at::kEventHook.v)))(n); }

// A block's actor fields - 0x904B34's or 0x904B44's (+8 the object, +0xC
// its action record, +2 / +4 / +6 the screen position) - for a member: the
// object's words +0x2E / +0x30 plus the s8 pair of 0x64DF70 at (+8) + (char
// id +0x89) * 4, and +0x32; for an enemy: +0x2E / +0x30 plus its s8 +0xF2 /
// +0xF3, and +0x32. Each sum is 16-bit. No call is made in between, so the
// order of the reads and stores is not observable; it is the original's.
void SetMemberBlock(std::uint32_t block, unsigned actor) {
    unsigned char* const o = Party(actor);
    auto* const member_rec = rv::As<records::PartyMember>(o);
    SetPtrAt(block + battle_actions::at::kBAction, o + 0x124);
    SetPtrAt(block + battle_actions::at::kBObject, o);
    const unsigned stand = member_rec->direction + member_rec->char_id * 4u;
    const unsigned char* const pair = battle_actions::At(::bof3::Img(BattleWin_MemberTargetOffsets_addr) + stand * 2);
    battle_actions::SetWord(battle_actions::At(block + battle_actions::at::kBX), static_cast<unsigned>(static_cast<signed char>(pair[0])) + static_cast<std::uint16_t>(member_rec->screen_x));
    battle_actions::SetWord(battle_actions::At(block + battle_actions::at::kBY), static_cast<unsigned>(static_cast<signed char>(pair[1])) + static_cast<std::uint16_t>(member_rec->screen_y));
    battle_actions::SetWord(battle_actions::At(block + battle_actions::at::kBZ), member_rec->draw_key);
}

void SetEnemyBlock(std::uint32_t block, unsigned actor) {
    unsigned char* const o = Enemy(actor);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(o);
    SetPtrAt(block + battle_actions::at::kBAction, o + 0x104);
    SetPtrAt(block + battle_actions::at::kBObject, o);
    battle_actions::SetWord(battle_actions::At(block + battle_actions::at::kBX), static_cast<unsigned>(static_cast<signed char>(o[0xF2])) + static_cast<std::uint16_t>(enemy_rec->screen_x));
    battle_actions::SetWord(battle_actions::At(block + battle_actions::at::kBY), static_cast<unsigned>(static_cast<signed char>(o[0xF3])) + static_cast<std::uint16_t>(enemy_rec->screen_y));
    battle_actions::SetWord(battle_actions::At(block + battle_actions::at::kBZ), enemy_rec->draw_key);
}

// The actor's effective stat block (a member's +0xA0, an enemy's +0xB0; the
// sibling's BATTLE_RAM.md: what Battle_BeginAction copies per action) to
// 0x939FE0, 32 bytes.
void CopyStats(unsigned actor) {
    const unsigned a = actor & 0xFF;
    std::memmove(battle_actions::At(::bof3::Img(battle_actions::at::kStats.v)), a < 3 ? Party(a) + 0xA0 : Enemy(a) + 0xB0, 32);
}

// A system message as a banner, as the kind-2 enemy and both checks show one: the message
// window opened, then a kind-2 banner of the system message `id`, timer 0x2D.
void SystemBanner(unsigned id) {
    battle_actions::g.open_msg_window();
    const unsigned char* const text = battle_actions::g.msg(id);
    battle_actions::g.banner_add(2, 0, 0, 0x2D, reinterpret_cast<const char*>(text));
}

constexpr ::bof3::image::Va kBannerPool{0x93B8E0};

constexpr unsigned kBannerColour = 0xA, kBlue = 3;

// After BattleAction_AbilityCheck's banner (`entry` BattleBanner_Add's eax,
// the entry's offset, or 0x15 when every entry was taken or there is no
// banner; `actor` and `id` the step's own reads): the name in blue when the
// actor is an enemy, the ability can be learnt (its record's u16 +0x14 bit
// 0x200, the bit BattleAction_AbilityNotice and Battle_SettleFlag8 test) and
// the party has not learnt it (Battle_ActionBitSet 0x453A90, the set 0x904088
// that Examine and the masters fill, by the id's low byte, as
// Battle_MemberRollByAction asks it).
void UnlearnedBlue(unsigned long entry, unsigned actor, unsigned id) {
    if (!battle_actions::g_unlearned_blue || entry % 0xC != 0) return;
    if (actor <= 2) return;
    if ((battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kAbilityRecords.v) + id * 24 + 0x14)) & 0x200) == 0) return;
    if (Battle_ActionBitSet(id & 0xFF) != 0) return;
    SetB(::bof3::Img(kBannerPool.v) + entry + kBannerColour, kBlue);
}

// The enemy's name record (+0x80, 12 bytes) into Text_Records[0], zeroed first.
void EnemyNameToText0(const unsigned char* record) {
    std::memset(battle_actions::At(::bof3::Img(battle_actions::at::kText0.v)), 0, 32);
    std::memcpy(battle_actions::At(::bof3::Img(battle_actions::at::kText0.v)), record, 12);
}

// A member's or an enemy's u16 at +0x90 / +0x92 (status) and AP +0x9A / +0xA6.
struct Standing { unsigned status, ap; };

Standing ActorStanding(unsigned actor) {
    const unsigned a = actor & 0xFF;
    if (a <= 2) return {rv::As<records::PartyMember>(Party(a))->status, rv::As<records::PartyMember>(Party(a))->ap};
    return {rv::As<records::BattleEnemy>(Enemy(a))->status, rv::As<records::BattleEnemy>(Enemy(a))->ap};
}

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& BBattleE1(U address) { return move_script::At(address)[0]; }

U LBattleE1(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }

unsigned char* PtrAtBattleE1(U address) { return move_script::At(LBattleE1(address)); }

unsigned char* Member(U index) { return move_script::At(::bof3::Img(ObjTrio_addr) + index * battle_e1::at::kMemberSize); }

// jmp [table + 4 * index]: the entry as read (the fuzz swaps the table's
// cells for its recorders), a Fatal past `entries` (the original jumps
// through the dword after: docs/battle_e1.md section 7). The jmp leaves the
// caller's stack word and the entry's eax in place: ours hands the word on
// and answers the entry's eax (round 11 doc section 5.1).
using Entry = unsigned long (__cdecl*)(unsigned long);

unsigned long Dispatch(const char* who, U table, unsigned index, unsigned entries, unsigned long through) {
    if (index >= entries)
        bof3::Fatal("%s: the step byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e1.md section 7)",
                    who, index, entries, (unsigned)table);
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(LBattleE1(table + 4 * index)))(through);
}

// The party's +0x130 dword (the loss steps' and the notices' flag bits).
U MemberFlags(U index) { return static_cast<U>(static_cast<std::int32_t>(rv::As<records::PartyMember>(Member(index))->flags_130)); }

}  // namespace

namespace battle_phases {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

// Group HT-2 of the button round (docs/button-tests.md 3): the command
// cross's L1 / R1 are Japan's bits, asked through sigma.
using bof3x::input::sigma::Sigma;

}  // namespace battle_phases

namespace {

unsigned char& BBattlePhases(std::uint32_t address) { return *battle_phases::At(address); }

unsigned char* PtrAtBattlePhases(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(battle_phases::Long(battle_phases::At(address)))));
}

void SetPtrAtBattlePhases(std::uint32_t address, const unsigned char* p) {
    battle_phases::SetLong(battle_phases::At(address), static_cast<std::int32_t>(Low32(p)));
}

unsigned char* MemberBattlePhases(unsigned index) { return battle_phases::At(::bof3::Img(ObjTrio_addr) + index * battle_phases::at::kMemberSize); }

unsigned char* EnemyObject(unsigned index) { return battle_phases::At(::bof3::Img(Battle_Enemies_addr) + index * battle_phases::at::kEnemySize); }

unsigned char* Window(unsigned index) { return battle_phases::At(::bof3::Img(WindowRecords_addr) + index * battle_phases::at::kWindowSize); }

// The member the menu is for (0x939EC4) and its command record (0x939FA0),
// each re-read wherever the original re-reads it.
unsigned char* MenuActor() { return reinterpret_cast<unsigned char*>(BattleE4_MenuActor.Native()); }

unsigned char* MenuRecord() { return PtrAtBattlePhases(::bof3::Img(battle_phases::at::kMenuRecord.v)); }

// A tail jump through a .data table, as the three stubs have it: the entry
// read at the jump, the index not checked (the tables are followed by more
// tables and data; docs/battle_phases.md section 3).
void JumpThrough(std::uint32_t table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(battle_phases::Long(battle_phases::At(table + index * 4)))))();
}

// The command record's +0x10 (the actor's +0x134) bit 1 greys commands 2 and
// 3 (1-based: the cross's down and left arms).
bool CommandsGreyed() { return (MenuRecord()[0x10] & 2) != 0; }

unsigned char* P(unsigned actor) { return move_script::At(battle_setup::at::Party(actor)); }

unsigned char* E(unsigned actor) { return move_script::At(battle_setup::at::Enemy(actor)); }

bool Yes(unsigned al) { return (al & 0xFFu) != 0; }

// The state bytes an effect sets, in the originals' order: +1 = 6, +2 = 5,
// +4 = 0, +3 = 0.
void MarkEffect(unsigned char* o) {
    o[battle_setup::at::kState1] = 6;
    o[battle_setup::at::kState2] = 5;
    o[battle_setup::at::kState4] = 0;
    o[battle_setup::at::kState3] = 0;
}

// The message every step ends with: Battle_OpenMsgWindow, then the line's
// text, then the line (2, 0, 0, 0x2D, text).
void ShowLine(const unsigned char* text) { battle_setup::g.message(2, 0, 0, 0x2D, text); }

// The four wake steps, one template in the original's code: each actor of
// the side the chain does not skip whose status low byte has `bit` rolls
// 0x446CB0; on a hit its counter (party +0x12D, enemy +0x10D) is zeroed and
// the bit cleared, else the counter goes up by one (after the roll, which
// reads it). If any woke, the window, and with one its name and line
// `single`, with more line `many`.
unsigned WakeStep(bool party, unsigned bit, unsigned single, unsigned many) {
    unsigned count = 0, who = 0;
    const unsigned from = party ? 0 : 3, to = party ? 2 : 10;
    for (unsigned i = from; i <= to; ++i) {
        if (Yes(battle_setup::g.skipped(i))) continue;
        unsigned char* const o = party ? P(i) : E(i);
        unsigned char* const status = o + (party ? battle_setup::at::kPStatus : battle_setup::at::kEStatus);
        unsigned char* const counter = o + (party ? battle_setup::at::kPCounter : battle_setup::at::kECounter);
        if ((status[0] & bit) == 0) continue;
        if (Yes(battle_setup::g.wake_roll(i))) {
            *counter = 0;
            move_script::SetWord(status, move_script::Word(status) & ~bit);
            who = i;
            count = (count + 1) & 0xFF;
        } else {
            ++*counter;
        }
    }
    if (count == 0) return 0;
    battle_setup::g.open_window();
    const unsigned char* text;
    if (count == 1) {
        (party ? battle_setup::g.party_name : battle_setup::g.enemy_name)(who);
        text = battle_setup::g.msg(single);
    } else {
        text = battle_setup::g.msg(many);
    }
    ShowLine(text);
    return 1;
}

unsigned char* MemberBattleTurnSteps(unsigned i) { return move_script::At(::bof3::Img(ObjTrio_addr) + (i & 0xFF) * battle_turn_steps::at::kPartyStride); }

unsigned char& BBattleTurnSteps(std::uint32_t address) { return *move_script::At(address); }

// `inc byte ptr [m]` / `dec byte ptr [m]`: read, step, store, as one.
void Inc(std::uint32_t address) { ++*move_script::At(address); }

// A dispatch stub's jump: entry `index` of the table in .data, read as the
// original reads it. The index is not checked, as the original's is not: every
// state byte of the combat route stays inside its table, and one past it
// reads the next table's first entry, the same code either way.
Step EntryBattleTurnSteps(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Step>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(move_script::Long(move_script::At(table + index * 4)))));
}

U LRest3a(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }

unsigned long DispatchRest3a(const char* who, U table, unsigned index, unsigned entries, unsigned long through) {
    if (index >= entries)
        bof3::Fatal("%s: the step byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3a.md section 7)",
                    who, index, entries, (unsigned)::bof3::VaOf(table));
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(LRest3a(table + 4 * index)))(through);
}

}  // namespace

// battle_setup's callee seam, moved here from battle_setup.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace battle_setup {
namespace {

template <typename F> F Fn(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }

}  // namespace

const Callees kOriginals = {
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(kActorAbsent.v)),
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(kActorCanAct.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kSetPending.v)),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, const unsigned char*)>(::bof3::Code(kMessage.v)),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned, const unsigned char*)>(::bof3::Code(kMessageAt.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kStatusTint.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kPartyName.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kEnemyName.v)),
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(kWakeRoll.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kSetHpChange.v)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(kEnemyOutpaces.v)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kReturnItem.v)),
    Sprite_ReleaseTint,
    Msg_SystemPtr,
    Window_Alloc,
    Rand,
    Battle_ActorSkipped,
    Battle_OpenMsgWindow,
    Battle_ClearStatus,
    Battle_ActorStanding,
    Battle_PartyOutpaces,
};
Callees g = kOriginals;

}  // namespace battle_setup

// battle_actions's callee seam, moved here from battle_actions.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace battle_actions {
namespace {

template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

}  // namespace

const Callees kOriginals = {
    Battle_MemberAutoTarget,
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kEnemyPickAction.v)),
    Battle_ClearActingFlags,
    Battle_OpenMsgWindow,
    Msg_SystemPtr,
    BattleBanner_Add,
    Battle_SetActorBit,
    Battle_ActionSuitsTarget,
    Battle_RemoveFromTurnOrder,
    Str_CopyN,
    Skill_ApCost,
    Battle_PickFlag8Member,
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(kPickTarget.v)),
    Magic_LoadForAbility,
    File_LoadDone,
    Battle_StartAbilityMagic,
    Battle_ItemSuitsTarget,
    Battle_ReturnQueuedItem,
    Item_NamePtr,
    Magic_LoadForItem,
    Battle_StartItemMagic,
    Battle_AnyFlagF0,
    LoadDatFile,
    Gfx_ClutStripCopyRow,
    Battle_SettleFlag8,
    BattleQueue_Push,
};
Callees g = kOriginals;

}  // namespace battle_actions

// battle_turn_steps's callee seam, moved here from battle_turn_steps.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace battle_turn_steps {
namespace {

template <typename F> F Fn(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }

}  // namespace

const Callees kOriginals = {
    Fn<unsigned (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_MarkFasterSide)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_TickCounters)),
    {Fn<unsigned (__cdecl*)()>(::bof3::Code(bof3::addr::BattleStep_Expire4000)), Fn<unsigned (__cdecl*)()>(::bof3::Code(bof3::addr::BattleStep_Restore800)),
     Fn<unsigned (__cdecl*)()>(::bof3::Code(bof3::addr::BattleStep_PartyWake40)), Fn<unsigned (__cdecl*)()>(::bof3::Code(bof3::addr::BattleStep_EnemyWake40)),
     Fn<unsigned (__cdecl*)()>(::bof3::Code(bof3::addr::BattleStep_PartyWake20)), Fn<unsigned (__cdecl*)()>(::bof3::Code(bof3::addr::BattleStep_EnemyWake20)),
     Fn<unsigned (__cdecl*)()>(::bof3::Code(bof3::addr::BattleStep_Status80)), Fn<unsigned (__cdecl*)()>(::bof3::Code(bof3::addr::BattleStep_HpDrift)),
     Fn<unsigned (__cdecl*)()>(::bof3::Code(bof3::addr::BattleStep_ApUpkeep))},
    Fn<void (__cdecl*)(int)>(::bof3::Code(bof3::addr::LoadDatFile)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_ClearCommands)),
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_ActorIsOut)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::BattleTask_Create)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_ClearStatus)),
    Fn<void (__cdecl*)(unsigned char*)>(::bof3::Code(bof3::addr::Sprite_ReleaseTint)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_ReturnQueuedItem)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Port_DroppedCall)),  // Port_DroppedCall (ours; called by its address)
    Fn<void (__cdecl*)(int)>(::bof3::Code(bof3::addr::Music_FadeOutStop)),
    Fn<void (__cdecl*)(unsigned, int)>(::bof3::Code(bof3::addr::Music_Play)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_OpenMsgWindow)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_OpeningMessage)),
    Fn<int (__cdecl*)()>(::bof3::Code(bof3::addr::File_LoadDone)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kWriteBackMember.v)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Window_ResetAll)),
    Fn<void (__cdecl*)()>(::bof3::Code(kClearEnemies.v)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::BattleTask_ClearAll)),
    Fn<void (__cdecl*)(unsigned char*)>(::bof3::Code(bof3::addr::Char_RecalcStats)),
    Fn<void (__cdecl*)()>(::bof3::Code(kReloadParty.v)),
};
Callees g = kOriginals;

}  // namespace battle_turn_steps

// original 0x42E990 (no PSX twin paired): phase 1's entry. A tail jump
// through Battle_InputSteps 0x64AE28 by the step 0x904AA1: Battle_RoundStart,
// BattleInput_NextMember, BattleMenu_CommandSelect, BattleHold_Dispatch 0x42ED90 (BE1's; a sub-step
// dispatch through 0x64AE48, not this group's), BattleMenu_ConfirmDispatch.
extern "C" void __cdecl BattleInput_Dispatch(void) { JumpThrough(::bof3::Img(Battle_InputSteps_addr), BattleActions_Step); }

// original 0x42E9A0 (PSX Battle_RoundStart 0x801D1C88): the commands
// cleared, the entry order built, the menu index 0x904AC3 zeroed. With auto
// battle on (the round flags' bit 4, read after the two calls) the commands
// are filled (0x446720, PSX AutoBattle_FillCommands) and the phase set to 2;
// else the actor copies are spawned and the step goes on.
extern "C" void __cdecl Battle_RoundStart(void) {
    battle_phases::g.clear_commands();
    battle_phases::g.build_entry_order();
    const unsigned char flags = BBattlePhases(::bof3::Img(battle_phases::at::kRoundFlags.v));
    BattleActorCopies_EntryCount = 0;
    if ((flags & 0x10) != 0) {
        battle_phases::g.auto_fill_commands();
        BBattlePhases(::bof3::Img(battle_phases::at::kPhase.v)) = 2;
        return;
    }
    battle_phases::g.spawn_actor_copies();
    BattleActions_Step = static_cast<unsigned char>(BattleActions_Step + 1);
}

// original 0x42E9E0 (PSX 0x801D1D08): once the files are loaded, the next
// member who can take a command. From the menu index 0x904AC3 (s8; at 0
// Input_Held is zeroed first, at 3 or more there is none) each entry of the
// order 0x904AB6 that is not 0xFF becomes 0x904AAE, and is taken unless its
// +0x90 has bit 5 or its +0x134 any of 0x14001; the index is stored on each
// step past one. A member taken is the menu actor 0x939EC4, its +0x124 the
// command record 0x939FA0, its name shown; windows 4, 3 and 2 get +3 = 1;
// the step on. None left: the tap bytes 0x904AA5 / 0x904AA6 zeroed, the
// wait 0x904B70 = 14, phase on, step 0.
extern "C" void __cdecl BattleInput_NextMember(void) {
    if (battle_phases::g.file_load_done() == 0) return;
    signed char index = BattleActorCopies_EntryCount;
    if (index == 0) Input_Held = 0;
    if (index < 3) {
        for (;;) {
            const unsigned c = BBattlePhases(static_cast<std::uint32_t>(static_cast<std::int32_t>(::bof3::Img(battle_phases::at::kEntryOrder.v)) + index));
            if (c != 0xFF) {
                BattleE4_AutoMember = static_cast<unsigned char>(c);
                const unsigned char* const q = MemberBattlePhases(c);
                auto* const member_q = rv::AsConst<records::PartyMember>(q);
                if ((q[0x90] & 0x20) == 0 && (static_cast<std::uint32_t>(static_cast<std::int32_t>(member_q->flags_134)) & 0x14001u) == 0) {
                    unsigned char* const p = MemberBattlePhases(c);
                    BattleE4_MenuActor = reinterpret_cast<records::PartyMember*>(p);
                    SetPtrAtBattlePhases(::bof3::Img(battle_phases::at::kMenuRecord.v), p + 0x124);
                    battle_phases::g.banner_show_name(p);
                    rv::As<records::WindowRecord>(Window(4))->state = 1;
                    rv::As<records::WindowRecord>(Window(3))->state = 1;
                    rv::As<records::WindowRecord>(Window(2))->state = 1;
                    BattleActions_Step = static_cast<unsigned char>(BattleActions_Step + 1);
                    return;
                }
            }
            index = static_cast<signed char>(index + 1);
            BattleActorCopies_EntryCount = static_cast<unsigned char>(index);
            if (index >= 3) break;
        }
    }
    const unsigned char phase = BBattlePhases(::bof3::Img(battle_phases::at::kPhase.v));
    BossSb_Countdown = 0;
    BattleE5_GeneRow = 0;
    BattleResult_TickStep = 0xE;
    BBattlePhases(::bof3::Img(battle_phases::at::kPhase.v)) = static_cast<unsigned char>(phase + 1);
    BattleActions_Step = 0;
}

// original 0x42EAD0 (PSX 0x801D1E84): the command cross, once a frame. The
// tap timer 0x904AA5 counts down first. Then, on the buttons pressed this
// frame (Input_Pressed, read once):
//  - Cancel (Field_CancelButtons) past the first member: sound 0x106, the
//    menu index and the step back one, the previous member the menu actor,
//    its +0x134 bit 2 cleared, +1 = 2; an item command (+0x125 = 5) gives
//    its item back (0x446D90(+0x12E, +0x126)) unless +0x130 has bit 14,
//    which is cleared instead; +0x130 and +0x125 zeroed.
//  - Confirm (Field_ConfirmButtons): a greyed command 2 or 3 sounds 0x107;
//    else sound 0x104, the step on two, the sub-step the command
//    0x904AB4, windows 3 and 2 get +3 = 2.
//  - Select held (Input_Held bit 8): sound 0x104; windows 2, 1 and 3 get
//    +3 = 2; the cross drawn at window 2's x, y, the command's label, the
//    party's status at window 1's; the step on.
//  - Otherwise each of the six directions of Battle_CommandPadMasks held,
//    from the sixth down, makes 0x904AB4 its number (1..6); one pressed this
//    frame sounds 0x101 and, pressed again within 8 frames of the first
//    press, chooses it (a greyed 2 or 3 clears the tap instead): 0x904AB4 and
//    the sub-step the command, the step on two, windows 3 and 2 get +3 = 2.
//    With none held 0x904AB4 is zeroed.
//
// As the original has it: after each press the pressed and held words are
// read again, and the cancel path's previous member is read after the index
// and the step are stored.
extern "C" void __cdecl BattleMenu_CommandSelect(void) {
    if (BossSb_Countdown != 0) BossSb_Countdown = static_cast<unsigned char>(BossSb_Countdown - 1);
    unsigned pressed = Input_Pressed;
    if ((Field_CancelButtons & pressed) != 0 && static_cast<unsigned char>(BattleActorCopies_EntryCount) != 0) {
        battle_phases::g.play_effect(0x106);
        const unsigned char index = static_cast<unsigned char>(static_cast<unsigned char>(BattleActorCopies_EntryCount) - 1);
        const unsigned char step = static_cast<unsigned char>(BattleActions_Step - 1);
        BattleActorCopies_EntryCount = index;
        BattleActions_Step = step;
        unsigned char* p = MemberBattlePhases(BBattlePhases(static_cast<std::uint32_t>(static_cast<std::int32_t>(::bof3::Img(battle_phases::at::kEntryOrder.v)) + static_cast<signed char>(index))));
        records::PartyMember* member_rec = rv::As<records::PartyMember>(p);
        BattleE4_MenuActor = reinterpret_cast<records::PartyMember*>(p);
        member_rec->flags_134 = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(member_rec->flags_134)) & ~4u);
        rv::As<records::PartyMember>(MenuActor())->state = 2;
        p = MenuActor();
        member_rec = rv::As<records::PartyMember>(p);
        if (p[0x125] == 5) {
            const std::uint32_t flags = static_cast<std::uint32_t>(static_cast<std::int32_t>(member_rec->flags_130));
            if ((flags & 0x4000) == 0) battle_phases::g.return_item(p[0x12E], member_rec->queued_id);
            else member_rec->flags_130 = static_cast<std::int32_t>(flags & ~0x4000u);
            p = MenuActor();
            member_rec = rv::As<records::PartyMember>(p);
        }
        member_rec->flags_130 = 0;
        MenuActor()[0x125] = 0;
        return;
    }
    if ((Field_ConfirmButtons & pressed) != 0) {
        const unsigned command = BBattlePhases(::bof3::Img(battle_phases::at::kCommand.v));
        if ((command == 2 || command == 3) && CommandsGreyed()) {
            battle_phases::g.play_effect(0x107);
            return;
        }
        battle_phases::g.play_effect(0x104);
        const unsigned char step = static_cast<unsigned char>(BattleActions_Step + 2);
        const unsigned char chosen = BBattlePhases(::bof3::Img(battle_phases::at::kCommand.v));
        rv::As<records::WindowRecord>(Window(3))->state = 2;
        rv::As<records::WindowRecord>(Window(2))->state = 2;
        BattleActions_Step = step;
        BattleE4_Step2 = chosen;
        return;
    }
    unsigned held = Input_Held;
    if ((held & 0x100) != 0) {
        battle_phases::g.play_effect(0x104);
        const unsigned y = rv::As<records::WindowRecord>(Window(2))->y;
        const unsigned x = rv::As<records::WindowRecord>(Window(2))->x;
        rv::As<records::WindowRecord>(Window(2))->state = 2;
        rv::As<records::WindowRecord>(Window(1))->state = 2;
        rv::As<records::WindowRecord>(Window(3))->state = 2;
        battle_phases::g.draw_command_cross(static_cast<int>(x), static_cast<int>(y));
        battle_phases::g.draw_command_label(BBattlePhases(::bof3::Img(battle_phases::at::kCommand.v)));
        battle_phases::g.draw_party_status(static_cast<int>(rv::As<records::WindowRecord>(Window(1))->x), static_cast<int>(rv::As<records::WindowRecord>(Window(1))->y));
        BattleActions_Step = static_cast<unsigned char>(BattleActions_Step + 1);
        return;
    }
    // The d-pad passes through sigma; L1 / R1 (0x0C) become the Change and
    // View lines' buttons now. Battle_CommandPadMasks stays Capcom's bytes:
    // each mask is read as it is and asked through sigma.
    if ((held & Sigma(0xF00C)) == 0) {
        if (BBattlePhases(::bof3::Img(battle_phases::at::kCommand.v)) != 0) BBattlePhases(::bof3::Img(battle_phases::at::kCommand.v)) = 0;
        return;
    }
    for (int k = 5; k >= 0; --k) {
        const unsigned mask = Sigma(static_cast<std::uint16_t>(battle_phases::Word(battle_phases::At(::bof3::Img(Battle_CommandPadMasks_addr) + 2 * static_cast<unsigned>(k)))));
        if ((mask & held) == 0) continue;
        if ((pressed & mask) != 0) {
            battle_phases::g.play_effect(0x101);
            if (BattleE5_GeneRow == static_cast<unsigned>(k + 1) && BossSb_Countdown != 0) {
                if ((k == 1 || k == 2) && CommandsGreyed()) {
                    BossSb_Countdown = 0;
                    BattleE5_GeneRow = 0;
                    return;
                }
                const unsigned char step = static_cast<unsigned char>(BattleActions_Step + 2);
                BBattlePhases(::bof3::Img(battle_phases::at::kCommand.v)) = static_cast<unsigned char>(k + 1);
                BattleE4_Step2 = static_cast<unsigned char>(k + 1);
                rv::As<records::WindowRecord>(Window(3))->state = 2;
                rv::As<records::WindowRecord>(Window(2))->state = 2;
                BattleActions_Step = step;
                BossSb_Countdown = 0;
                BattleE5_GeneRow = 0;
                return;
            }
            pressed = Input_Pressed;
            BossSb_Countdown = 8;
            BattleE5_GeneRow = static_cast<unsigned char>(k + 1);
            held = Input_Held;
        }
        BBattlePhases(::bof3::Img(battle_phases::at::kCommand.v)) = static_cast<unsigned char>(k + 1);
    }
}

// original 0x42ED90 (Battle_InputSteps[3]; PSX 0x801D22A4): by 0x904AA2 through
// BattleHold_Steps 0x64AE48 (3).
extern "C" unsigned long __cdecl BattleHold_Dispatch(unsigned long through) {
    return Dispatch("BattleHold_Dispatch", ::bof3::Img(BattleHold_Steps_addr), BattleE4_Step2, 3, through);
}

// original 0x42EDA0 (BattleHold_Steps[0]; PSX 0x801D22E0): the menu member's cross size
// 0x904ABC[0x904AB4] down by 2 while above 0, else 0x904AA2 + 1; then its
// command label, the party status at window 1's (x, y) and the command cross
// at window 2's. The original pushes the words in whole registers
// whose upper halves are left over (the callees keep the low halves: the
// fuzz compares those); ours passes the words.
extern "C" void __cdecl BattleHold_Shrink(void) {
    unsigned char& size = BBattleE1(::bof3::Img(battle_e1::at::kCrossGrow.v) + BBattleE1(::bof3::Img(battle_e1::at::kMenuMember.v)));
    if (size > 0)
        size = static_cast<unsigned char>(size - 2);
    else
        BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
    BH_CALL(BattleWin_DrawCommandLabel)(BBattleE1(::bof3::Img(battle_e1::at::kMenuMember.v)));
    BH_CALL(BattleWin_DrawPartyStatus)(WindowRecords[1].x, WindowRecords[1].y);
    BH_CALL(BattleWin_DrawCommandCross)(WindowRecords[2].x, WindowRecords[2].y);
}

// original 0x42EE00 (BattleHold_Steps[1]; PSX 0x801D2384): while Input_Held has no 0x100,
// windows 2, 1 and 3's +3 = 1 and 0x904AA2 + 1; then the party status,
// BE4's 0x444660(), and all seven command labels with their icons
// (Menu_DrawIcon(i, 0x88 + 0x64E2AC[2i], 0x58 + 0x64E2AD[2i], 0x10, 0x10,
// 0x80)).
extern "C" void __cdecl BattleHold_ShowAll(void) {
    if (!(Input_Held & 0x100)) {
        BBattleE1(::bof3::Img(battle_e1::at::kWin2.v) + 3) = 1;
        BBattleE1(::bof3::Img(battle_e1::at::kWin1.v) + 3) = 1;
        WindowRecords[3].state = 1;
        BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
    }
    BH_CALL(BattleWin_DrawPartyStatus)(WindowRecords[1].x, WindowRecords[1].y);
    BH_AT(void (__cdecl*)(), battle_e1::at::kBe4Draw444660.v)();
    for (unsigned i = 0; i < 7; ++i) {
        BH_CALL(BattleWin_DrawCommandLabel)(i);
        BH_CALL(Menu_DrawIcon)(i, BBattleE1(::bof3::Img(battle_e1::at::kHoldIcons.v) + 2 * i) + 0x88u, BBattleE1(::bof3::Img(battle_e1::at::kHoldIcons.v) + 2 * i + 1) + 0x58u, 0x10, 0x10, 0x80);
    }
}

// original 0x42EEA0 (BattleHold_Steps[2]; PSX 0x801D2478): the menu member's cross size up by
// 4 while below 8, else 0x904AA1 = 2 (BattleMenu_CommandSelect) and
// 0x904AA2 = 0.
extern "C" void __cdecl BattleHold_Regrow(void) {
    unsigned char& size = BBattleE1(::bof3::Img(battle_e1::at::kCrossGrow.v) + BBattleE1(::bof3::Img(battle_e1::at::kMenuMember.v)));
    if (size < 8) {
        size = static_cast<unsigned char>(size + 4);
        return;
    }
    BattleActions_Step = 2;
    BattleE4_Step2 = 0;
}

// original 0x42EED0 (PSX BattleMenu_ConfirmDispatch 0x801D24CC): the menu
// pulse stepped, then a tail jump through Battle_MenuSteps 0x64AE54 by the
// chosen command 0x904AA2 (read after the pulse): 0x447110, 0x447430,
// 0x448180, 0x447FD0 (groups CH and CI), Cmd_ConfirmDefend, Cmd_AutoBattle 0x42EF50 (BE1's; the
// PSX's Cmd_AutoBattle_Begin, unowned), 0x44A000, 0x44FF00.
extern "C" void __cdecl BattleMenu_ConfirmDispatch(void) {
    battle_phases::g.pulse_step();
    JumpThrough(::bof3::Img(Battle_MenuSteps_addr), BattleE4_Step2);
}

// original 0x42EEF0 (PSX 0x801D2514, the Defend confirm the sibling's
// BATTLE_RAM.md places after BattleMenu_ConfirmDispatch): the command record
// gets command 2, the actor's +5 as its target, +0xC bit 1; the actor +1 =
// 2; the menu index on one, step 1, sub-steps 0. Both pointers re-read at
// each use, as the original.
extern "C" void __cdecl Cmd_ConfirmDefend(void) {
    MenuRecord()[1] = 2;
    const unsigned char target = rv::As<records::PartyMember>(MenuActor())->actor;
    MenuRecord()[0] = target;
    unsigned char* const r = MenuRecord();
    auto* const action = rv::As<records::ActionRecord>(r);  // 0x939FA0: the member's +0x124 (BattleInput_NextMember)
    rv::Store<std::int32_t>(action->flags, static_cast<std::int32_t>(action->flags | 2u));
    rv::As<records::PartyMember>(MenuActor())->state = 2;
    const unsigned char index = static_cast<unsigned char>(static_cast<unsigned char>(BattleActorCopies_EntryCount) + 1);
    BattleActions_Step = 1;
    BattleActorCopies_EntryCount = index;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
}

// original 0x42EF50 (Battle_MenuSteps[5]; PSX Cmd_AutoBattle_Begin
// 0x801D2598 by the sibling's names/functions.toml, read and agreeing):
// window 4's +3 = 2; each of the s8 0x904AC3 entries 0x904AB6[i] (the
// count re-read each pass) not 0xFF and not out whose command +0x125 is 5
// (an item) - an entry is indexed into ObjTrio unchecked -, BE4's
// 0x446D90(+0x12E, word +0x126) gives the item back; then each member not
// out (0x904AB0 of them, re-read) - with 0x904B8E set only one whose +0x134
// has 0x10 - goes to state 2 (+1); round flags |= 0x10, LoadDatFile(0xD1),
// 0x904AA1 = 0x904AA2 = 0.
extern "C" void __cdecl Cmd_AutoBattle(void) {
    const auto entries = BattleActorCopies_EntryCount;
    WindowRecords[4].state = 2;
    if (entries > 0) {
        unsigned i = 0;
        do {
            const unsigned char actor = BBattleE1(::bof3::Img(battle_e1::at::kEntryFirst.v) + i);
            if (actor != 0xFF && !BH_CALL(Battle_ActorIsOut)(actor)) {
                unsigned char* const m = Member(actor);
                if (m[0x125] == 5)
                    BH_AT(unsigned char (__cdecl*)(unsigned, unsigned), battle_e1::at::kBe4ReturnItem.v)(m[0x12E], rv::As<records::PartyMember>(m)->queued_id);
            }
            i = (i + 1) & 0xFF;
        } while (static_cast<int>(i) < BattleActorCopies_EntryCount);
    }
    if (BBattleE1(::bof3::Img(battle_e1::at::kPartyCount.v)) != 0) {
        unsigned char i = 0;
        do {
            if (!BH_CALL(Battle_ActorIsOut)(i)) {
                if (BattleFlow_AnimGate == 0 || (Member(i)[0x134] & 0x10)) rv::As<records::PartyMember>(Member(i))->state = 2;
            }
            ++i;
        } while (i < BBattleE1(::bof3::Img(battle_e1::at::kPartyCount.v)));
    }
    BBattleE1(::bof3::Img(battle_e1::at::kFlags.v)) = static_cast<unsigned char>(BBattleE1(::bof3::Img(battle_e1::at::kFlags.v)) | 0x10);
    BH_CALL(LoadDatFile)(0xD1);
    BattleActions_Step = 0;
    BattleE4_Step2 = 0;
}

// original 0x42F070 (no PSX twin paired): phase 2's entry. A tail jump
// through Battle_CommitSteps 0x64AE74 by the step 0x904AA1:
// Battle_CommitRound, BattleCommit_QueueMessages, BattleCommit_WaitLoad.
extern "C" void __cdecl BattleCommit_Dispatch(void) { JumpThrough(::bof3::Img(Battle_CommitSteps_addr), BattleActions_Step); }

// original 0x42F080 (PSX Battle_CommitRound 0x801D2774): the turn order
// built; window 4's +3 = 2 unless auto battle, windows 3 and 2 too;
// 0x93B8E0 zeroed. Each member not out with an item command (+0x125 = 5, an
// item word +0x126 with a high byte) gives the item back (0x446D90(+0x12E,
// +0x126)) unless +0x130 has bit 14, which is cleared instead. The round
// flags get bit 3; the enemies choose their actions; unless auto battle
// (re-read) DAT file 0xD1 is loaded; the step on.
extern "C" void __cdecl Battle_CommitRound(void) {
    battle_phases::g.build_turn_order();
    if ((BBattlePhases(::bof3::Img(battle_phases::at::kRoundFlags.v)) & 0x10) == 0) rv::As<records::WindowRecord>(Window(4))->state = 2;
    rv::As<records::WindowRecord>(Window(3))->state = 2;
    rv::As<records::WindowRecord>(Window(2))->state = 2;
    BBattlePhases(::bof3::Img(battle_phases::at::kB8E0.v)) = 0;
    for (unsigned m = 0; m <= 2; ++m) {
        if (battle_phases::g.actor_is_out(m) != 0) continue;
        unsigned char* const p = MemberBattlePhases(m);
        auto* const member_rec = rv::As<records::PartyMember>(p);
        if (p[0x125] != 5) continue;
        const unsigned item = member_rec->queued_id;
        if ((item & 0xFF00) == 0) continue;
        const std::uint32_t flags = static_cast<std::uint32_t>(static_cast<std::int32_t>(member_rec->flags_130));
        if ((flags & 0x4000) == 0) battle_phases::g.return_item(p[0x12E], item);
        else member_rec->flags_130 = static_cast<std::int32_t>(flags & ~0x4000u);
    }
    BBattlePhases(::bof3::Img(battle_phases::at::kRoundFlags.v)) = static_cast<unsigned char>(BBattlePhases(::bof3::Img(battle_phases::at::kRoundFlags.v)) | 8);
    battle_phases::g.choose_actions();
    if ((BBattlePhases(::bof3::Img(battle_phases::at::kRoundFlags.v)) & 0x10) == 0) battle_phases::g.load_dat(0xD1);
    BattleActions_Step = static_cast<unsigned char>(BattleActions_Step + 1);
}

// original 0x42F130 (PSX 0x801D28E0): while 0x939F60 is 0, one queued
// enemy message a frame, the last first: the count 0x93C2A2 at 0 moves the
// step on; else Text_Records[0] is cleared, its first 12 bytes the named
// enemy's +0x80.. (the record 0x939FBC + 4 n: its byte the enemy, its word
// +2 the system message), and the message queued at (1, 0); the count
// (re-read) down one.
extern "C" void __cdecl BattleCommit_QueueMessages(void) {
    if (BattleE1_MessageUp != 0) return;
    const unsigned count = BattleE5_EnemyMessageCount;
    if (count == 0) {
        BattleActions_Step = static_cast<unsigned char>(BattleActions_Step + 1);
        return;
    }
    unsigned char* const text = battle_phases::At(::bof3::Img(battle_phases::at::kTextRecord0.v));
    std::memset(text, 0, 0x20);
    const unsigned char* const record = battle_phases::At(::bof3::Img(battle_phases::at::kMessages.v) + count * 4);
    const unsigned char* const name = EnemyObject(record[0]) + 0x80;
    battle_phases::SetLong(text, battle_phases::Long(name));
    battle_phases::SetLong(text + 4, battle_phases::Long(name + 4));
    const unsigned message = battle_phases::Word(record + 2);
    battle_phases::SetLong(text + 8, battle_phases::Long(name + 8));
    battle_phases::g.queue_push(1, 0, Low32(battle_phases::g.msg_system_ptr(message)));
    BattleE5_EnemyMessageCount = static_cast<unsigned char>(BattleE5_EnemyMessageCount - 1);
}

// original 0x42F1D0 (PSX 0x801D2A1C): once the files are loaded - and,
// unless auto battle, once the wait 0x904B70 has counted down through 0
// (the word decremented on each frame it is tested) - the wait zeroed,
// phase on, step 0.
extern "C" void __cdecl BattleCommit_WaitLoad(void) {
    if (battle_phases::g.file_load_done() == 0) return;
    if ((BBattlePhases(::bof3::Img(battle_phases::at::kRoundFlags.v)) & 0x10) == 0) {
        const unsigned wait = BattleResult_TickStep;
        BattleResult_TickStep = wait - 1;
        if (wait != 0) return;
    }
    const unsigned char phase = BBattlePhases(::bof3::Img(battle_phases::at::kPhase.v));
    BattleResult_TickStep = 0;
    BBattlePhases(::bof3::Img(battle_phases::at::kPhase.v)) = static_cast<unsigned char>(phase + 1);
    BattleActions_Step = 0;
}

// original 0x42F220 (PSX 0x801D2AA0, catalogue pair): the battle's phase 3,
// from Battle_PhaseDispatch. Calls entry 0x904AA1 of BattleAction_Steps
// (0x64AE80, five entries); then, when any button of Field_CancelButtons is
// held (Input_Held), clears bit 4 of the round flags 0x904AA8.
//
// As the original has it: both words are read after the step; the step byte
// is not checked.
extern "C" void __cdecl Battle_ActionPhase(void) {
    StepBattleActions(::bof3::Img(BattleAction_Steps_addr), BattleActions_Step)();
    if ((Field_CancelButtons & Input_Held) != 0)
        battle_actions::SetWord(battle_actions::At(::bof3::Img(battle_actions::at::kRoundFlags.v)), battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kRoundFlags.v))) & 0xFFEFu);
}

// original 0x42F250 (PSX 0x801D2B14): step 0, a tail jump through
// BattleAction_BeginSteps (0x64AE94) by 0x904AA2: 0 Battle_BeginAction, 1
// BattleAction_EnterKind.
extern "C" void __cdecl BattleAction_BeginStep(void) { StepBattleActions(::bof3::Img(BattleAction_BeginSteps_addr), BattleE4_Step2)(); }

// original 0x42F260 (the sibling's Battle_BeginAction, catalogue pair; PSX address not read): from
// the turn order 0x904ACC, the first slot from 0x904AE2 up to (not including)
// 0x904AE3 that is not 0xFF - 0x904AE2 follows the search. None left: the
// phase becomes 4, the step 0. Else the actor picks its action
// (Battle_MemberAutoTarget, or BattleEnemy_PickAction 0x435AB0 with actor - 3), becomes 0x904B34 and
// fills the acting block, its stat block goes to 0x939FE0, a target of 0..10
// fills the target block (a side, 0x40 / 0x80 / 0xC0, or anything above 10
// does not), and 0x904AA2 and 0x904AE2 each count one.
//
// As the original has it: 0x904AE3 is read once; the actor is stored after
// its pick, and the target byte, the objects and both counters are read after
// it. The member's argument carries the stale upper bits of the original's
// `push ecx` slot; ours passes the byte (0x453FA0 and each of its callees
// read the low byte - their disassembly, 2026-09-25).
extern "C" void __cdecl Battle_BeginAction(void) {
    unsigned char i = BossSe_RoundSlot;
    const unsigned char end = BattleDamage_TurnCount;
    for (;;) {
        if (i >= end) {
            SetB(::bof3::Img(battle_actions::at::kPhase.v), 4);
            BattleActions_Step = 0;
            return;
        }
        if (B(::bof3::Img(battle_actions::at::kQueue.v) + i) != 0xFF) break;
        ++i;
        BossSe_RoundSlot = i;
    }
    const unsigned actor = B(::bof3::Img(battle_actions::at::kQueue.v) + i);
    if (actor < 3) battle_actions::g.member_auto_target(actor);
    else battle_actions::g.enemy_pick_action((actor - 3) & 0xFF);
    SetB(::bof3::Img(battle_actions::at::kActor.v), actor);
    if (actor < 3) SetMemberBlock(::bof3::Img(battle_actions::at::kActor.v), actor);
    else SetEnemyBlock(::bof3::Img(battle_actions::at::kActor.v), actor);
    CopyStats(actor);
    const unsigned target = B(::bof3::Img(battle_actions::at::kTarget.v));
    if (target < 3) SetMemberBlock(::bof3::Img(battle_actions::at::kTarget.v), target);
    else if (target <= 10) SetEnemyBlock(::bof3::Img(battle_actions::at::kTarget.v), target);
    BattleE4_Step2 = BattleE4_Step2 + 1;
    BossSe_RoundSlot = BossSe_RoundSlot + 1;
}

// original 0x42F4C0 (PSX 0x801D2FB8): step 0's second part. In an event
// battle (0x904AAA) the hook 0x904B6C is called with 1 first; then the step
// becomes 1 and 0x904AA2 the command kind 0x904B35, read after the hook.
extern "C" void __cdecl BattleAction_EnterKind(void) {
    if (EventBattle_Id != 0) EventHook(1);
    const unsigned kind = B(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBKind);
    BattleActions_Step = 1;
    BattleE4_Step2 = kind;
}

// original 0x42F500 (PSX 0x801D300C): step 1, a tail jump through
// BattleAction_KindSteps (0x64AE9C) by 0x904AA2, which BattleAction_EnterKind
// set to the command kind: 0 and 2 BattleAction_KindPlain, 1
// BattleAction_KindOne, 3 BattleAction_Kind3Dispatch 0x42F5E0 (BE1's), 4 BattleAction_AbilityStep,
// 5 BattleAction_ItemStep.
extern "C" void __cdecl BattleAction_KindStep(void) { StepBattleActions(::bof3::Img(BattleAction_KindSteps_addr), BattleE4_Step2)(); }

// original 0x42F510 (PSX 0x801D3048): kinds 0 and 2. Battle_ClearActingFlags;
// then for kind 2 (0x904AA2) by an enemy (0x904B34 of 3 or more) the enemy's
// name record goes to Text_Records[0] and a kind-2 banner shows system
// message 0x7F. The phase stays 3 with step 0 and 0x904AA2 0: the next actor.
//
// As the original has it: both bytes are read after Battle_ClearActingFlags.
extern "C" void __cdecl BattleAction_KindPlain(void) {
    battle_actions::g.clear_acting_flags();
    if (BattleE4_Step2 == 2 && B(::bof3::Img(battle_actions::at::kActor.v)) >= 3) {
        EnemyNameToText0(Enemy(B(::bof3::Img(battle_actions::at::kActor.v))) + 0x80);
        SystemBanner(0x7F);
    }
    SetB(::bof3::Img(battle_actions::at::kPhase.v), 3);
    BattleActions_Step = 0;
    BattleE4_Step2 = 0;
}

// original 0x42F5B0 (PSX 0x801D3160): kind 1. The actor's pending bit
// (Battle_SetActorBit), its object's state byte +1 becomes 4, and the action
// goes to step 2 (the effects) with 0x904AA2 0.
//
// As the original has it: the object pointer 0x904B3C is read after the call.
extern "C" void __cdecl BattleAction_KindOne(void) {
    battle_actions::g.set_actor_bit(B(::bof3::Img(battle_actions::at::kActor.v)));
    PtrAt(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBObject)[1] = 4;
    SetB(::bof3::Img(battle_actions::at::kPhase.v), 3);
    BattleActions_Step = 2;
    BattleE4_Step2 = 0;
}

// original 0x42F5E0 (BattleAction_KindSteps[3]; PSX 0x801D31B8): by 0x904AA3 through
// BattleAction_Kind3Steps 0x64AEB4 (5: the two below, then
// BattleAction_AbilitySteps' three).
extern "C" unsigned long __cdecl BattleAction_Kind3Dispatch(unsigned long through) {
    return Dispatch("BattleAction_Kind3Dispatch", ::bof3::Img(BattleAction_Kind3Steps_addr), BattleE4_Step3, 5, through);
}

// original 0x42F5F0 (Kind3Steps[0]; PSX 0x801D31F4): Battle_ClearActingFlags;
// BattleBanner_Add(1, 0, 0, 0x1E, the string 0x669E08 points at); window 4's
// +3 = 1; Battle_SetActorBit(0x904B34); the acting sprite's +1 = 9;
// 0x904AA3 + 1.
extern "C" void __cdecl BattleAction_Kind3Banner(void) {
    BH_CALL(Battle_ClearActingFlags)();
    BH_CALL(BattleBanner_Add)(1, 0, 0, 0x1E, reinterpret_cast<const char*>(BattleE1_BannerText.Native()));
    const unsigned char actor = BBattleE1(::bof3::Img(battle_e1::at::kActor.v));
    WindowRecords[4].state = 1;
    BH_CALL(Battle_SetActorBit)(actor);
    static_cast<unsigned char*>(MagicS23_ActorSprite)[1] = 9;
    BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
}

// original 0x42F640 (Kind3Steps[1]; PSX 0x801D3278): once the word 0x904B82 is 0, round
// flags |= 4, 0x904AA1 = 3, 0x904AA2 = 0x904AA3 = 0.
extern "C" void __cdecl BattleAction_Kind3Wait(void) {
    if (BattleActions_Pending != 0) return;
    BBattleE1(::bof3::Img(battle_e1::at::kFlags.v)) = static_cast<unsigned char>(BBattleE1(::bof3::Img(battle_e1::at::kFlags.v)) | 4);
    BattleActions_Step = 3;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
}

// original 0x42F670 (PSX 0x801D32C0): kind 4's, through
// BattleAction_AbilitySteps (0x64AEBC) by 0x904AA3.
extern "C" void __cdecl BattleAction_AbilityStep(void) { StepBattleActions(::bof3::Img(BattleAction_AbilitySteps_addr), BattleE4_Step3)(); }

// original 0x42F680 (PSX 0x801D32FC): the ability's checks. The id (the
// action record's u16 +2) to 0x904B80.
//   - Battle_ActionSuitsTarget answers non-zero: the actor leaves the turn
//     order, and ObjTrio + 0x14C * actor gets +1 = 2 and +2 = 0 - a member's
//     object by the formula even for an enemy actor, as the original has it
//     (docs/battle_actions.md section 3) - and the phase stays 3 at step 0,
//     0x904AA2 0.
//   - Unless the id is 0x97, the ability's name (its 24-byte record at
//     0x65C4C8) is copied to 0x904EA0 by Str_CopyN(.., 0x10) and shown as a
//     kind-1 banner (1, 1, 0, 0x3C); 0x8031F3 = 1.
//   - The actor's status bit 4 (a member's +0x90, an enemy's +0x92) with the
//     record's +0x15 bit 2: system message 0x37, step 0, 0x904AA2 0.
//   - The cost - Skill_ApCost(the object's +5, the id's low byte, 1), or the
//     byte 0x904B78 for 0x97 - goes to 0x904B88; above the actor's AP (+0x9A
//     / +0xA6): system message 0x36, step 0, 0x904AA2 0.
//   - Else 0x904AA5 = 0, 0x904AA8 |= 0x20 when Battle_PickFlag8Member
//     answers 0, and 0x904AA3 counts one.
//
// As the original has it: the actor, its status and AP are read after the
// banner and before the cost; the id is re-read at each test; the cost's
// actor is the one read before (not re-read after Skill_ApCost).
extern "C" void __cdecl BattleAction_AbilityCheck(void) {
    battle_actions::SetWord(battle_actions::At(::bof3::Img(battle_actions::at::kAbility.v)), battle_actions::Word(PtrAt(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBAction) + 2));
    if (battle_actions::g.action_suits_target() != 0) {
        battle_actions::g.remove_from_turn_order(B(::bof3::Img(battle_actions::at::kActor.v)));
        const unsigned a = B(::bof3::Img(battle_actions::at::kActor.v));
        SetB(::bof3::Img(battle_actions::at::kPhase.v), 3);
        auto* const member_rec = rv::As<records::PartyMember>(Party(a));
        member_rec->state = 2;
        member_rec->step = 0;
        BattleActions_Step = 0;
        BattleE4_Step2 = 0;
        return;
    }
    unsigned long banner = 0x15;   // DIV-0179: no banner, as BattleBanner_Add's "every entry taken"
    unsigned named = 0;
    if (battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kAbility.v))) != 0x97) {
        const unsigned id = battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kAbility.v)));
        WindowRecords[4].state = 1;
        battle_actions::g.str_copy_n(reinterpret_cast<char*>(battle_actions::At(::bof3::Img(battle_actions::at::kNameBuf.v))), reinterpret_cast<const char*>(battle_actions::At(::bof3::Img(battle_actions::at::kAbilityRecords.v) + id * 24)), 0x10);
        banner = battle_actions::g.banner_add(1, 1, 0, 0x3C, reinterpret_cast<const char*>(battle_actions::At(::bof3::Img(battle_actions::at::kNameBuf.v))));
        named = id;
    }
    const unsigned a = B(::bof3::Img(battle_actions::at::kActor.v));
    UnlearnedBlue(banner, a, named);
    const Standing s = ActorStanding(a);
    if ((s.status & 0x10) != 0 && (battle_actions::At(::bof3::Img(battle_actions::at::kAbilityRecords.v) + battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kAbility.v))) * 24u + 0x15)[0] & 4) != 0) {
        SystemBanner(0x37);
        BattleActions_Step = 0;
        BattleE4_Step2 = 0;
        return;
    }
    unsigned char cost;
    if (battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kAbility.v))) != 0x97) {
        const unsigned id = B(::bof3::Img(battle_actions::at::kAbility.v));
        const unsigned who = a <= 2 ? rv::As<records::PartyMember>(Party(a))->actor : rv::As<records::BattleEnemy>(Enemy(a))->actor;
        cost = battle_actions::g.skill_ap_cost(who, id, 1);
    } else {
        cost = BattleActions_ApCost;
    }
    BattleE3_Cost = cost;
    if (cost > (s.ap & 0xFFFF)) {
        SystemBanner(0x36);
        BattleActions_Step = 0;
        BattleE4_Step2 = 0;
        return;
    }
    BossSb_Countdown = 0;
    if (battle_actions::g.pick_flag8_member() == 0) SetB(::bof3::Img(battle_actions::at::kRoundFlags.v), B(::bof3::Img(battle_actions::at::kRoundFlags.v)) | 0x20);
    BattleE4_Step3 = BattleE4_Step3 + 1;
}

// original 0x42F880 (PSX 0x801D360C): waits for 0x904AA8 bit 5. Then clears
// it, the actor object's +1 = 7 and +2 = 0, the actor's pending bit,
// 0x904B8D = the id's low byte; for the ids 0x24, 0x25 and 0x8C the target
// is BattleAction_PickRandomAbility's (0x42F9D0) answer (it picks the id again) and fills the target block -
// a member's for 0..2, an enemy's for anything above; then the magic file
// (Magic_LoadForAbility, the id's low byte re-read) and 0x904AA3 counts one.
//
// As the original has it: 0x904B3C is re-read for the second store; the id
// is read after Battle_SetActorBit and again after BattleAction_PickRandomAbility; the target
// block has no upper bound here (Battle_BeginAction's stops at 10), so a
// side's 0x40 / 0x80 / 0xC0 is taken as an enemy 0x3D.. far past the eight
// (docs/battle_actions.md section 3).
extern "C" void __cdecl BattleAction_AbilityCommit(void) {
    if ((B(::bof3::Img(battle_actions::at::kRoundFlags.v)) & 0x20) == 0) return;
    unsigned char* const o = PtrAt(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBObject);
    battle_actions::SetWord(battle_actions::At(::bof3::Img(battle_actions::at::kRoundFlags.v)), battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kRoundFlags.v))) & 0xFFDFu);
    o[1] = 7;
    PtrAt(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBObject)[2] = 0;
    battle_actions::g.set_actor_bit(B(::bof3::Img(battle_actions::at::kActor.v)));
    BattleE3_SoundSet = B(::bof3::Img(battle_actions::at::kAbility.v));
    const unsigned id = battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kAbility.v)));
    if (id == 0x24 || id == 0x25 || id == 0x8C) {
        const unsigned t = battle_actions::g.pick_target();
        SetB(::bof3::Img(battle_actions::at::kTarget.v), t);
        if (t <= 2) SetMemberBlock(::bof3::Img(battle_actions::at::kTarget.v), t);
        else SetEnemyBlock(::bof3::Img(battle_actions::at::kTarget.v), t);
    }
    battle_actions::g.load_for_ability(B(::bof3::Img(battle_actions::at::kAbility.v)));
    BattleE4_Step3 = BattleE4_Step3 + 1;
}

// original 0x42F9D0: for the action word 0x904B80 0x24, an id from
// 0x64AEC8[Rand() & 0x1F]; for 0x25 or 0x8C (the word re-read after that
// Rand), from 0x64AEE8[Rand() & 0x1F]; any other gives 0. The id to
// 0x904B80 and the action record's (0x904B40, read first) +2; then by the
// id's flags byte (NameTable_Abilities + 24 id): 0x10 - 0xC0 for 0x80
// without 0x40, else a side by the actor (party: 0x40 with 0x20, 0x80
// without; enemy: 0x80 with, 0x40 without); no 0x10 and no 0x40 - the actor
// 0x904B34; 0x40 - 0x452F10() or 0x452EB0() by the actor's side and 0x20.
// al out.
extern "C" unsigned char __cdecl BattleAction_PickRandomAbility(void) {
    unsigned char id = 0;
    unsigned action = move_script::Word(move_script::At(::bof3::Img(battle_e1::at::kActionId.v)));
    if (action == 0x24) {
        id = BBattleE1(::bof3::Img(battle_e1::at::kRandomIds24.v) + (static_cast<U>(BH_CALL(Rand)()) & 0x1F));
        action = move_script::Word(move_script::At(::bof3::Img(battle_e1::at::kActionId.v)));
    }
    if (action == 0x25 || action == 0x8C) id = BBattleE1(::bof3::Img(battle_e1::at::kRandomIds25.v) + (static_cast<U>(BH_CALL(Rand)()) & 0x1F));
    unsigned char* const record = PtrAtBattleE1(::bof3::Img(battle_e1::at::kActing2.v));
    move_script::SetWord(move_script::At(::bof3::Img(battle_e1::at::kActionId.v)), id);
    const unsigned char flags = BBattleE1(::bof3::Img(bof3::addr::NameTable_Abilities) + 24u * id);
    rv::Store<unsigned>(rv::As<records::ActionRecord>(record)->action_id, id);  // 0x904B40: the acting block's action record
    const bool party = BBattleE1(::bof3::Img(battle_e1::at::kActor.v)) <= 2;
    if (flags & 0x10) {
        if ((flags & 0x80) && !(flags & 0x40)) return 0xC0;
        if (party) return (flags & 0x20) ? 0x40 : 0x80;
        return (flags & 0x20) ? 0x80 : 0x40;
    }
    if (!(flags & 0x40)) return BBattleE1(::bof3::Img(battle_e1::at::kActor.v));
    const bool second = party ? (flags & 0x20) != 0 : (flags & 0x20) == 0;
    return BH_WORD(unsigned char (__cdecl*)(), second ? battle_e1::at::kSideTargetB.v : battle_e1::at::kSideTargetA.v)();
}

// original 0x42FAB0 (PSX 0x801D399C): once File_LoadDone answers non-zero,
// Battle_StartAbilityMagic(the action record's +2 byte, the actor object)
// and the action goes to step 2 with both sub-steps 0.
extern "C" void __cdecl BattleAction_AbilityStart(void) {
    if (battle_actions::g.file_load_done() == 0) return;
    const unsigned id = PtrAt(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBAction)[2];
    const unsigned owner = static_cast<std::uint32_t>(L(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBObject));
    battle_actions::g.start_ability_magic(id, owner);
    SetB(::bof3::Img(battle_actions::at::kPhase.v), 3);
    BattleActions_Step = 2;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
}

// original 0x42FAF0 (PSX 0x801D3A08): kind 5's, through BattleAction_ItemSteps
// (0x64AF08) by 0x904AA3.
extern "C" void __cdecl BattleAction_ItemStep(void) { StepBattleActions(::bof3::Img(BattleAction_ItemSteps_addr), BattleE4_Step3)(); }

// original 0x42FB00 (PSX 0x801D3A44): the item's check. The action record's
// u16 +2 to 0x904B80. Battle_ItemSuitsTarget non-zero: the queued item goes
// back (Battle_ReturnQueuedItem), the actor leaves the turn order, ObjTrio +
// 0x14C * actor gets +1 = 2 and +2 = 0 (as the ability's), step 0 with
// 0x904AA2 0. Else the item's name (Item_NamePtr(the record's +3, +2)) is
// copied to 0x904EA0 and shown as a kind-1 banner (1, 1, 0, 0x1E); 0x904AA3
// counts one, 0x904AA5 = 0, 0x8031F3 = 1.
//
// As the original has it: the actor byte is re-read for each call and after
// them; the record pointer is re-read after Battle_ItemSuitsTarget.
extern "C" void __cdecl BattleAction_ItemCheck(void) {
    battle_actions::SetWord(battle_actions::At(::bof3::Img(battle_actions::at::kAbility.v)), battle_actions::Word(PtrAt(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBAction) + 2));
    if (battle_actions::g.item_suits_target() != 0) {
        battle_actions::g.return_queued_item(B(::bof3::Img(battle_actions::at::kActor.v)));
        battle_actions::g.remove_from_turn_order(B(::bof3::Img(battle_actions::at::kActor.v)));
        const unsigned a = B(::bof3::Img(battle_actions::at::kActor.v));
        SetB(::bof3::Img(battle_actions::at::kPhase.v), 3);
        BattleActions_Step = 0;
        BattleE4_Step2 = 0;
        auto* const member_rec = rv::As<records::PartyMember>(Party(a));
        member_rec->state = 2;
        member_rec->step = 0;
        return;
    }
    const unsigned char* const r = PtrAt(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBAction);
    const unsigned item = r[2], category = r[3];
    const unsigned char* const name = battle_actions::g.item_name(category, item);
    battle_actions::g.str_copy_n(reinterpret_cast<char*>(battle_actions::At(::bof3::Img(battle_actions::at::kNameBuf.v))), reinterpret_cast<const char*>(name), 0x10);
    battle_actions::g.banner_add(1, 1, 0, 0x1E, reinterpret_cast<const char*>(battle_actions::At(::bof3::Img(battle_actions::at::kNameBuf.v))));
    const unsigned sub2 = BattleE4_Step3;
    BossSb_Countdown = 0;
    WindowRecords[4].state = 1;
    BattleE4_Step3 = sub2 + 1;
}

// original 0x42FBD0 (PSX 0x801D3B94): the actor object's +1 = 0xC, the
// actor's pending bit, the item's magic file (Magic_LoadForItem of the
// record's u16 +2, read after the bit), and 0x904AA3 counts one.
extern "C" void __cdecl BattleAction_ItemCommit(void) {
    PtrAt(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBObject)[1] = 0xC;
    battle_actions::g.set_actor_bit(B(::bof3::Img(battle_actions::at::kActor.v)));
    battle_actions::g.load_for_item(battle_actions::Word(PtrAt(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBAction) + 2));
    BattleE4_Step3 = BattleE4_Step3 + 1;
}

// original 0x42FC10 (PSX 0x801D3BFC): once File_LoadDone answers non-zero,
// Battle_StartItemMagic(the record's u16 +2, the actor object) and step 2
// with both sub-steps 0.
extern "C" void __cdecl BattleAction_ItemStart(void) {
    if (battle_actions::g.file_load_done() == 0) return;
    const unsigned id = battle_actions::Word(PtrAt(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBAction) + 2);
    const unsigned owner = static_cast<std::uint32_t>(L(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBObject));
    battle_actions::g.start_item_magic(id, owner);
    SetB(::bof3::Img(battle_actions::at::kPhase.v), 3);
    BattleActions_Step = 2;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
}

// original 0x42FC50 (PSX 0x801D3C68): step 2, through BattleAction_EffectSteps
// (0x64AF14) by 0x904AA2.
extern "C" void __cdecl BattleAction_EffectStep(void) { StepBattleActions(::bof3::Img(BattleAction_EffectSteps_addr), BattleE4_Step2)(); }

// original 0x42FC60 (PSX 0x801D3CA4): every actor, members 0..2 then enemies
// 3..10, whose flags byte (a member's +0x130, an enemy's +0x110) has bit 6
// and whose state byte +1 is not 6 gets +1 = 6, +2..+4 = 0 and its pending
// bit. Then, when Battle_AnyFlagF0 answers 0, no actor is pending (0x904B82)
// and 0x904AA8 has bit 2, 0x904AA2 counts one.
//
// As the original has it: each actor is read after the previous one's call.
extern "C" void __cdecl BattleAction_EffectWait(void) {
    for (unsigned a = 0; a <= 2; ++a) {
        unsigned char* const o = Party(a);
        auto* const member_rec = rv::As<records::PartyMember>(o);
        if ((o[0x130] & 0x40) == 0 || member_rec->state == 6) continue;
        member_rec->state = 6;
        member_rec->step = 0;
        member_rec->state3 = 0;
        member_rec->state4 = 0;
        battle_actions::g.set_actor_bit(a);
    }
    for (unsigned a = 3; a <= 10; ++a) {
        unsigned char* const o = Enemy(a);
        auto* const enemy_rec = rv::As<records::BattleEnemy>(o);
        if ((o[0x110] & 0x40) == 0 || enemy_rec->state == 6) continue;
        enemy_rec->state = 6;
        enemy_rec->step = 0;
        enemy_rec->state3 = 0;
        enemy_rec->state4 = 0;
        battle_actions::g.set_actor_bit(a);
    }
    if (battle_actions::g.any_flag_f0() != 0) return;
    if (BattleActions_Pending != 0) return;
    if ((B(::bof3::Img(battle_actions::at::kRoundFlags.v)) & 4) == 0) return;
    BattleE4_Step2 = BattleE4_Step2 + 1;
}

// original 0x42FD20 (PSX 0x801D3E60): for kind 4, the ability row of the id
// 0x904B80 (by the whole u16), for kind 5 the item row of the record's u16
// +2 (battle_flow.md's ItemRow); when that row's magic file is not 0xFFFF,
// LoadDatFile(0xD0). Any kind: 0x904AA2 counts one.
//
// As the original has it: the ability row is indexed by the whole word
// (Magic_LoadForAbility uses its low byte); neither index is checked.
//
// DIVERGENCE DIV-0167: the PlayStation first keys off voices 16..23
// (`SsUtKeyOffV` 0x8016F10C for s0 = 0x10..0x17, JP 0x801D3E68..0x801D3E84;
// US 0x801D3E88.., the same words but the call's target), cutting what the
// previous action's effect still sounds; the PC has no such step. With the
// switch (PatchBytes("EffectKeyOff") in BattleActions_Inject, after the
// self-test), the same point stops those voices' channels
// (sound::KeyOffEffectVoices, game/sound.h).
//
// DIVERGENCE DIV-0174: the same key-off reaching a one-shot - the buffer each
// of those channels last played without a loop, remembered DLL-side by
// Sound_PlayEffect - under its own switch (PatchBytes("OneShotKeyOff"), after
// the self-test as above; sound_st::KeyOffOneShots, game/sound_st.h), so the
// loops' stop and the one-shots' bisect apart.
namespace battle_actions { std::uint8_t g_effect_key_off = 0; }

extern "C" void __cdecl BattleAction_EffectPreload(void) {
    if (battle_actions::g_effect_key_off) sound::KeyOffEffectVoices();  // DIV-0167
    if (sound_st::g_one_shot_key_off) sound_st::KeyOffOneShots();  // DIV-0174
    const unsigned kind = B(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBKind);
    bool has_file = false;
    if (kind == 4) {
        const unsigned row = battle_actions::At(::bof3::Img(battle_actions::at::kAbilityRows.v) + battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kAbility.v))))[0];
        has_file = battle_actions::Word(battle_actions::At(::bof3::Img(Magic_Rows_addr) + row * 8)) != 0xFFFF;
    } else if (kind == 5) {
        const unsigned w = battle_actions::Word(PtrAt(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBAction) + 2);
        const unsigned row = PtrAt(::bof3::Img(battle_actions::at::kItemRows.v) + 4u * (w >> 8))[w & 0xFF];   // four 32-bit table places
        has_file = battle_actions::Word(battle_actions::At(::bof3::Img(Magic_Rows_addr) + row * 8)) != 0xFFFF;
    }
    if (has_file) battle_actions::g.load_dat(0xD0);
    BattleE4_Step2 = BattleE4_Step2 + 1;
}

// original 0x42FD90 (PSX 0x801D3F4C): once File_LoadDone answers non-zero:
// in an event battle the hook with 4; 0x904AA8 loses 0x400; step 3 with
// 0x904AA2 0; Gfx_ClutStripCopyRow(0x1A).
//
// As the original has it: 0x904AAA is read after File_LoadDone; the stores
// come before the row copy.
extern "C" void __cdecl BattleAction_EffectLoaded(void) {
    if (battle_actions::g.file_load_done() == 0) return;
    if (EventBattle_Id != 0) EventHook(4);
    battle_actions::SetWord(battle_actions::At(::bof3::Img(battle_actions::at::kRoundFlags.v)), battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kRoundFlags.v))) & 0xFBFFu);
    BattleActions_Step = 3;
    BattleE4_Step2 = 0;
    battle_actions::g.clut_copy_row(0x1A);
}

// original 0x42FDD0 (PSX 0x801D3FCC): step 3, through BattleAction_AfterSteps
// (0x64AF20) by 0x904AA2: 0 BattleAction_AfterSettle, 1 BattleAction_AbilityNotice 0x42FE20 (BE1's),
// 2 BattleAction_EnemyMessages.
extern "C" void __cdecl BattleAction_AfterStep(void) { StepBattleActions(::bof3::Img(BattleAction_AfterSteps_addr), BattleE4_Step2)(); }

// original 0x42FDE0 (PSX 0x801D4008): with 0x904AA8 bit 2 and no actor
// pending: Battle_SettleFlag8 non-zero gives 0x904AA5 = 0 and 0x904AA2 + 1;
// zero gives 0x904AA2 = 2 (past BattleAction_AbilityNotice to the enemy messages).
extern "C" void __cdecl BattleAction_AfterSettle(void) {
    if ((B(::bof3::Img(battle_actions::at::kRoundFlags.v)) & 4) == 0) return;
    if (BattleActions_Pending != 0) return;
    if (battle_actions::g.settle_flag8() != 0) {
        const unsigned sub = BattleE4_Step2;
        BossSb_Countdown = 0;
        BattleE4_Step2 = sub + 1;
        return;
    }
    BattleE4_Step2 = 2;
}

// original 0x42FE20 (BattleAction_AfterSteps[1]): nothing while the message
// window is up (0x939F60); else from the party slot after 0x904AA5 (stored
// as it goes; none left past slot 2: 0x904AA2 + 1) the first member whose
// +0x130 has bit 3: the bit cleared, and by the action's ability (the
// record 0x904B40's word +2) flag 0x200 in Ability_Records +0x14 - BE4's
// 0x44A910(member) (the name into Text_Records), Text_Records + 0x20 zeroed
// (32 bytes) and the acting enemy's ability record (+0x106 of enemy
// 0x904B34 - 3, unchecked) copied in (16 bytes), BattleQueue_Push(1, 0,
// message 0x24); without it, BattleQueue_Push(1, 0, message 0x23).
extern "C" void __cdecl BattleAction_AbilityNotice(void) {
    if (BattleE1_MessageUp != 0) return;
    unsigned char slot = BossSb_Countdown;
    if (slot <= 2) {
        const unsigned char* const record = PtrAtBattleE1(::bof3::Img(battle_e1::at::kActing2.v));
        do {
            ++slot;
            BossSb_Countdown = slot;
            const U id = rv::As<records::ActionRecord>(record)->action_id;  // 0x904B40: the acting block's action record
            const bool learns = (move_script::Word(move_script::At(::bof3::Img(0x65C4DC) + 24 * id)) & 0x200) != 0;
            unsigned char* const flags = move_script::At(::bof3::Img(0x802D24) + slot * battle_e1::at::kMemberSize);
            if (!(flags[0] & 8)) continue;
            move_script::SetLong(flags, static_cast<std::int32_t>(static_cast<U>(move_script::Long(flags)) & ~8u));
            if (!learns) {
                BH_CALL(BattleQueue_Push)(1, 0, Low32(BH_CALL(Msg_SystemPtr)(0x23)));
                return;
            }
            BH_AT(void (__cdecl*)(unsigned), battle_e1::at::kBe4MemberName.v)(slot - 1u);
            std::memset(move_script::At(::bof3::Img(bof3::addr::Text_Records) + 0x20), 0, 32);
            const U enemy = static_cast<U>(BBattleE1(::bof3::Img(battle_e1::at::kActor.v))) - 3;
            const U ability = move_script::Word(move_script::At(::bof3::Img(0x93BA66) + enemy * 0x128));
            const U from = ::bof3::Img(bof3::addr::Ability_Records) + 24 * ability;
            move_script::SetLong(move_script::At(::bof3::Img(bof3::addr::Text_Records) + 0x20), move_script::Long(move_script::At(from)));
            move_script::SetLong(move_script::At(::bof3::Img(bof3::addr::Text_Records) + 0x24), move_script::Long(move_script::At(from + 4)));
            move_script::SetLong(move_script::At(::bof3::Img(bof3::addr::Text_Records) + 0x28), move_script::Long(move_script::At(from + 8)));
            move_script::SetLong(move_script::At(::bof3::Img(bof3::addr::Text_Records) + 0x2C), move_script::Long(move_script::At(from + 0xC)));
            BH_CALL(BattleQueue_Push)(1, 0, Low32(BH_CALL(Msg_SystemPtr)(0x24)));
            return;
        } while (slot <= 2);
    }
    BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// original 0x42FF70 (PSX 0x801D4284): unless 0x939F60 is set, one of the
// enemy messages a turn: with 0x93C2A2 = n messages left, entry n of the
// 4-byte list 0x939FBC (+0 an enemy index, +2 a u16 system message) puts that
// enemy's name record into Text_Records[0] (zeroed first) and queues
// (1, 0, Msg_SystemPtr(the message)) with BattleQueue_Push; n counts down.
// None left: step 4 with 0x904AA2 0.
//
// As the original has it: the enemy index is used as it is (no - 3: it
// indexes the eight objects from 0); the count is re-read after the push.
// The message argument's upper 16 bits are the next dword's in the original
// (Msg_SystemPtr reads the word); ours passes the word.
extern "C" void __cdecl BattleAction_EnemyMessages(void) {
    if (BattleE1_MessageUp != 0) return;
    const unsigned n = BattleE5_EnemyMessageCount;
    if (n == 0) {
        BattleActions_Step = 4;
        BattleE4_Step2 = 0;
        return;
    }
    const unsigned e = B(::bof3::Img(battle_actions::at::kMsgList.v) + n * 4);
    EnemyNameToText0(battle_actions::At(::bof3::Img(Battle_Enemies_addr) + e * battle_actions::at::kEnemyStride + 0x80));
    const unsigned id = battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kMsgList.v) + n * 4 + 2));
    const unsigned char* const text = battle_actions::g.msg(id);
    battle_actions::g.queue_push(1, 0, Low32(text));
    BattleE5_EnemyMessageCount = BattleE5_EnemyMessageCount - 1;
}

// original 0x430010 (PSX 0x801D43BC): with 0x904AA8 bit 2 and no actor
// pending. 0x904AA8 loses 0x2804 and the step and 0x904AA2 become 0; in an
// event battle the hook with 0. When 0x904AE8 is set the phase becomes 4 at
// step 2 and nothing else happens. Else Battle_ClearActingFlags, 0x904AA8
// loses 0x1000, and with 0x904AA8 bit 6 the acting and target blocks are
// swapped (0x904B34..0x904B43 with 0x904B44..0x904B53) and the new actor's
// kind (0x904B35) is 1: its stat block to 0x939FE0, a kind-1 banner (1, 0,
// 0, 0xF) of the text 0x669E0C when its flags (a member's +0x130, an enemy's
// +0x110) have bit 15, else of 0x669DF8, 0x8031F3 = 1, step 0 with 0x904AA2
// 1 - BattleAction_EnterKind next. Last, in an event battle, the hook with 5.
//
// As the original has it: 0x904AAA is read before the first hook and again
// before the last; the flags word is re-read after Battle_ClearActingFlags;
// the hook with 5 comes on the swap's path and on the plain one alike.
//
// DIV-0108 (psp_code.h): with the PSP's duel-loss on, an end byte with bit
// 0 set and bit 2 clear in event battle 0x1B does not end the battle - the
// PSP's added test (PSP-JP 0x11DFA4), which reads the event byte again after
// the hook, as here. Off, the original's test alone.
extern "C" void __cdecl BattleAction_End(void) {
    if ((B(::bof3::Img(battle_actions::at::kRoundFlags.v)) & 4) == 0) return;
    if (BattleActions_Pending != 0) return;
    const unsigned char event = EventBattle_Id;
    battle_actions::SetWord(battle_actions::At(::bof3::Img(battle_actions::at::kRoundFlags.v)), battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kRoundFlags.v))) & 0xD7FBu);
    BattleActions_Step = 0;
    BattleE4_Step2 = 0;
    if (event != 0) EventHook(0);
    if (Battle_EndFlags != 0 && !psp_code::DuelLossGoesOn(EventBattle_Id, Battle_EndFlags)) {
        SetB(::bof3::Img(battle_actions::at::kPhase.v), 4);
        BattleActions_Step = 2;
        return;
    }
    battle_actions::g.clear_acting_flags();
    if ((L(::bof3::Img(battle_actions::at::kRoundFlags.v)) & 0x1000) != 0) battle_actions::SetWord(battle_actions::At(::bof3::Img(battle_actions::at::kRoundFlags.v)), battle_actions::Word(battle_actions::At(::bof3::Img(battle_actions::at::kRoundFlags.v))) & 0xEFFFu);
    if ((B(::bof3::Img(battle_actions::at::kRoundFlags.v)) & 0x40) != 0) {
        unsigned char actor[0x10], target[0x10];
        std::memcpy(actor, battle_actions::At(::bof3::Img(battle_actions::at::kActor.v)), sizeof actor);
        std::memcpy(target, battle_actions::At(::bof3::Img(battle_actions::at::kTarget.v)), sizeof target);
        std::memcpy(battle_actions::At(::bof3::Img(battle_actions::at::kActor.v)), target, sizeof target);
        std::memcpy(battle_actions::At(::bof3::Img(battle_actions::at::kTarget.v)), actor, sizeof actor);
        SetB(::bof3::Img(battle_actions::at::kActor.v) + battle_actions::at::kBKind, 1);
        const unsigned a = B(::bof3::Img(battle_actions::at::kActor.v));
        const std::uint32_t flags = static_cast<std::uint32_t>(battle_actions::Long(a <= 2 ? Party(a) + 0x130 : Enemy(a) + 0x110));
        CopyStats(a);
        const std::uint32_t text = static_cast<std::uint32_t>(L((flags & 0x8000) != 0 ? ::bof3::Img(BattleActions_TextCounter_addr) : ::bof3::Img(BattleActions_TextMiss_addr)));
        battle_actions::g.banner_add(1, 0, 0, 0xF, reinterpret_cast<const char*>(static_cast<std::uintptr_t>(text)));
        WindowRecords[4].state = 1;
        BattleActions_Step = 0;
        BattleE4_Step2 = 1;
    }
    if (EventBattle_Id != 0) EventHook(5);
}

// original 0x4301B0 (PSX 0x801D4820, the same but for reading the actor again
// after the first part - nothing between can change it): the acting actor
// (0x904B34's low byte) loses flag 0x40 (party +0x134, enemy +0x114) with its
// counter +0x144 / +0x124 zeroed, unless the command kind (the next byte) is 4
// with ability 0x27 (0x904B80); then flag 0x80 with +0x145 / +0x125, unless
// kind 4 with ability 0xA3. An actor above 2 is an enemy, (actor - 3) * 0x128
// - unbounded, as the original's.
extern "C" void __cdecl Battle_ClearActingFlags() {
    const std::uint32_t acting = static_cast<std::uint32_t>(move_script::Long(move_script::At(::bof3::Img(battle_setup::at::kActing.v))));
    const unsigned a = acting & 0xFF, kind = (acting >> 8) & 0xFF;
    const unsigned ability = move_script::Word(move_script::At(::bof3::Img(battle_setup::at::kAbility.v)));
    const auto clear = [a](unsigned bit, unsigned party_count, unsigned enemy_count) {
        const bool party = a <= 2;
        unsigned char* const o = party ? P(a) : E(a);
        unsigned char* const flags = o + (party ? battle_setup::at::kPFlags : battle_setup::at::kEFlags);
        if ((flags[0] & bit) == 0) return;
        o[party ? party_count : enemy_count] = 0;
        move_script::SetLong(flags, static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(flags)) & ~bit));
    };
    if (!(kind == 4 && ability == 0x27)) clear(0x40, battle_setup::at::kPCount40, battle_setup::at::kECount40);
    if (!(kind == 4 && ability == 0xA3)) clear(0x80, battle_setup::at::kPCount80, battle_setup::at::kECount80);
}

// original 0x4302B0: phase 4's dispatch - entry 0x904AA1 of
// BattleRoundEnd_Steps (0x64AF2C: BattleRoundEnd_FasterStep,
// BattleRoundEnd_StatusChain, BattleRoundEnd_NextRound). A jump, as the
// original's: the step returns to Battle_PhaseDispatch.
extern "C" void __cdecl BattleRoundEnd_Step() {
    [[clang::musttail]] return EntryBattleTurnSteps(::bof3::Img(BattleRoundEnd_Steps_addr), BattleActions_Step)();
}

// original 0x4302C0: entry 0x904AA2 of BattleRoundEnd_FasterSteps (0x64AF38:
// BattleRoundEnd_CheckFaster, BattleRoundEnd_FasterNext,
// BattleRoundEnd_NextRound).
extern "C" void __cdecl BattleRoundEnd_FasterStep() {
    [[clang::musttail]] return EntryBattleTurnSteps(::bof3::Img(BattleRoundEnd_FasterSteps_addr), BattleE4_Step2)();
}

// original 0x4302D0 (PSX 0x801D4B08, read: the same branches and the same loop
// bounds): unless an extra round is running (0x904B7A) or the timer 0x904B8E
// is, Battle_MarkFasterSide's al (0..3, through a four-entry jump table at
// 0x4303B4) picks the extra round 0x904B7A, 1..3 - 2 in place of 1 or 3 when
// 0x904AE4 is 2, and none in place of 1 then - and steps 0x904AA2 on. With
// none (or al above 3), the round ends: flag 0x8000 cleared from members 0
// and 1 and enemies 0..6 (as the original has it - neither loop reaches the
// last of its array; section 4 of the doc), the round counted (0x904B90), the
// extra round cleared, Battle_TickCounters, and 0x904AA1 on to the chain.
//
// DIVERGENCE DIV-0154: 1 through PatchBytes("ExtraRoundClear") in
// BattleTurnSteps_Inject, after the self-test. The loops then clear the mark
// from all three members and all eight enemies, the setter's bounds (D63).
namespace battle_turn_steps { std::uint8_t g_extra_round_clear = 0; }

extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleRoundEnd_CheckFaster() {
    if (BattlePhases_B7A == 0 && BattleFlow_AnimGate == 0) {
        switch (battle_turn_steps::g.mark_faster() & 0xFF) {
        case 1:
            if (BattlePhases_Initiative != 2) {
                BattlePhases_B7A = 1;
                Inc(::bof3::Img(BattleE4_Step2_addr));
                return;
            }
            Inc(::bof3::Img(BattleE4_Step2_addr)); // then the round ends, which zeroes it after Battle_TickCounters
            break;
        case 2:
            BattlePhases_B7A = 2;
            Inc(::bof3::Img(BattleE4_Step2_addr));
            return;
        case 3:
            BattlePhases_B7A = BattlePhases_Initiative != 2 ? 3 : 2;
            Inc(::bof3::Img(BattleE4_Step2_addr));
            return;
        default:
            break;
        }
    }
    const unsigned members = battle_turn_steps::g_extra_round_clear ? 3 : 2, enemies = battle_turn_steps::g_extra_round_clear ? 8 : 7;
    for (unsigned i = 0; i < members; ++i) MemberBattleTurnSteps(i)[battle_turn_steps::at::kFlags + 1] &= 0x7F;
    for (unsigned i = 0; i < enemies; ++i) move_script::At(::bof3::Img(Battle_Enemies_addr) + i * battle_turn_steps::at::kEnemyStride)[battle_turn_steps::at::kEFlags + 1] &= 0x7F;
    move_script::SetLong(move_script::At(::bof3::Img(battle_turn_steps::at::kRounds.v)), static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(move_script::At(::bof3::Img(battle_turn_steps::at::kRounds.v)))) + 1));
    BattlePhases_B7A = 0;
    battle_turn_steps::g.tick_counters();
    BattleE4_Step2 = 0;
    Inc(::bof3::Img(BattleActions_Step_addr));
}

// original 0x4303D0 (PSX 0x801D4D14, the same): for each member the chain
// does not skip, +0x142 up by one while at most 5 with flag 1 (+0x134), +0x143
// likewise with status 0x800 (+0x91 bit 3), then +0x142 again while at most 2
// with flag 0x4000 (read after the first step); for each enemy, +0x122 while
// at most 2 with flag 0x4000. Then the timer 0x904B8E, if running, counts
// down, and when it reaches 0 every actor 0x4456C0 does not rule out loses
// flag 0x10.
extern "C" void __cdecl Battle_TickCounters() {
    for (unsigned i = 0; i <= 2; ++i) {
        if (Yes(battle_setup::g.skipped(i))) continue;
        unsigned char* const o = P(i);
        auto* const member_rec = rv::As<records::PartyMember>(o);
        if ((o[battle_setup::at::kPFlags] & 1) && member_rec->count_4000 <= 5) ++member_rec->count_4000;
        if ((o[battle_setup::at::kPStatus + 1] & 8) && member_rec->count_800 <= 5) ++member_rec->count_800;
        if ((static_cast<std::int32_t>(member_rec->flags_134) & 0x4000) && member_rec->count_4000 <= 2) ++member_rec->count_4000;
    }
    for (unsigned i = 3; i <= 10; ++i) {
        if (Yes(battle_setup::g.skipped(i))) continue;
        auto* const enemy_rec = rv::As<records::BattleEnemy>(E(i));
        if ((static_cast<std::int32_t>(enemy_rec->flags_114) & 0x4000) && enemy_rec->count_4000 <= 2) ++enemy_rec->count_4000;
    }
    unsigned char* const timer = move_script::At(::bof3::Img(BattleFlow_AnimGate_addr));
    if (*timer == 0) return;
    if (--*timer != 0) return;
    for (unsigned i = 0; i <= 2; ++i)
        if (!Yes(battle_setup::g.actor_absent(i))) P(i)[battle_setup::at::kPFlags] &= 0xEF;
    for (unsigned i = 3; i <= 10; ++i)
        if (!Yes(battle_setup::g.actor_absent(i))) E(i)[battle_setup::at::kEFlags] &= 0xEF;
}

// original 0x430500: `inc byte ptr [0x904AA2]` - the extra round's frame
// between the check and BattleRoundEnd_NextRound.
extern "C" void __cdecl BattleRoundEnd_FasterNext() { Inc(::bof3::Img(BattleE4_Step2_addr)); }

// original 0x430510 (the PSX's sub-state machine over 0x801D524C ..
// 0x801D62A8): by 0x904AA2, through a 14-entry jump table at 0x4305FC, the
// nine BattleSteps in order. Entering a step steps 0x904AA2 on; a step that
// answers 1 (it showed something) ends the frame there. After Expire4000,
// Status80, HpDrift and ApUpkeep the next state (1, 8, 10, 12) waits until
// the pending word 0x904B82 is 0, then clears round flag 4 and steps on; after
// the other five the next state is the next step's. State 13 (after the
// last) zeroes 0x904AA2 and steps 0x904AA1 on; states above 13 do nothing.
// Each step of 0x904AA2 is the original's `inc byte ptr`, re-read from memory.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleRoundEnd_StatusChain() {
    switch (BattleE4_Step2) {
    case 0:
        Inc(::bof3::Img(BattleE4_Step2_addr));
        if (Yes(battle_turn_steps::g.steps[0]())) return;
        Inc(::bof3::Img(BattleE4_Step2_addr));
        [[fallthrough]];
    case 2:
        Inc(::bof3::Img(BattleE4_Step2_addr));
        if (Yes(battle_turn_steps::g.steps[1]())) return;
        [[fallthrough]];
    case 3:
        Inc(::bof3::Img(BattleE4_Step2_addr));
        if (Yes(battle_turn_steps::g.steps[2]())) return;
        [[fallthrough]];
    case 4:
        Inc(::bof3::Img(BattleE4_Step2_addr));
        if (Yes(battle_turn_steps::g.steps[3]())) return;
        [[fallthrough]];
    case 5:
        Inc(::bof3::Img(BattleE4_Step2_addr));
        if (Yes(battle_turn_steps::g.steps[4]())) return;
        [[fallthrough]];
    case 6:
        Inc(::bof3::Img(BattleE4_Step2_addr));
        if (Yes(battle_turn_steps::g.steps[5]())) return;
        [[fallthrough]];
    case 7:
        Inc(::bof3::Img(BattleE4_Step2_addr));
        if (Yes(battle_turn_steps::g.steps[6]())) return;
        Inc(::bof3::Img(BattleE4_Step2_addr));
        [[fallthrough]];
    case 9:
        Inc(::bof3::Img(BattleE4_Step2_addr));
        if (Yes(battle_turn_steps::g.steps[7]())) return;
        Inc(::bof3::Img(BattleE4_Step2_addr));
        [[fallthrough]];
    case 11:
        Inc(::bof3::Img(BattleE4_Step2_addr));
        if (Yes(battle_turn_steps::g.steps[8]())) return;
        [[fallthrough]];
    case 13:
        BattleE4_Step2 = 0;
        Inc(::bof3::Img(BattleActions_Step_addr));
        return;
    case 1:
    case 8:
    case 10:
    case 12:
        if (BattleActions_Pending != 0) return;
        BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kRoundFlags.v)) &= 0xFB;
        Inc(::bof3::Img(BattleE4_Step2_addr));
        return;
    default:
        return;
    }
}

// original 0x430640 (PSX 0x801D524C, the same): each actor the chain does not
// skip with flag 0x4000 and its counter at 3 loses the flag, the counter 0,
// the HP change word takes HP (party +0x128 = +0x98, enemy +0x108 = +0xA4),
// the effect is marked (+0x12C / +0x10C = 0x11) and 0x446FB0 told; if any,
// the window and line 0x2D. Answers 1 when it showed a line.
// As the original has it (D-number in docs/battle_setup.md section 6): an
// enemy's counter is read from the PARTY array at the enemy's actor index -
// 0x802D40 + actor * 0x14C + 0x142, past the three members - while the one
// it zeroes is the enemy's own +0x122. The PSX reads 0x80145E8C + actor *
// 0x140 + 0x136 the same way.
extern "C" unsigned __cdecl BattleStep_Expire4000() {
    unsigned count = 0;
    for (unsigned i = 0; i <= 2; ++i) {
        if (Yes(battle_setup::g.skipped(i))) continue;
        unsigned char* const o = P(i);
        auto* const member_rec = rv::As<records::PartyMember>(o);
        unsigned char* const flags = o + battle_setup::at::kPFlags;
        if ((move_script::Long(flags) & 0x4000) == 0 || member_rec->count_4000 != 3) continue;
        member_rec->count_4000 = 0;
        move_script::SetLong(flags, static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(flags)) & ~0x4000u));
        rv::Store<unsigned>(member_rec->hp_change, member_rec->hp);
        MarkEffect(o);
        o[battle_setup::at::kPEffect] = 0x11;
        count = (count + 1) & 0xFF;
        battle_setup::g.set_pending(i);
    }
    for (unsigned i = 3; i <= 10; ++i) {
        if (Yes(battle_setup::g.skipped(i))) continue;
        unsigned char* const o = E(i);
        auto* const enemy_rec = rv::As<records::BattleEnemy>(o);
        unsigned char* const flags = o + battle_setup::at::kEFlags;
        if ((move_script::Long(flags) & 0x4000) == 0 || rv::As<records::PartyMember>(P(i))->count_4000 != 3) continue;   // the party array, as the original
        enemy_rec->count_4000 = 0;
        move_script::SetLong(flags, static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(flags)) & ~0x4000u));
        enemy_rec->hp_change = enemy_rec->hp;
        MarkEffect(o);
        enemy_rec->shown = 0x11;
        count = (count + 1) & 0xFF;
        battle_setup::g.set_pending(i);
    }
    if (count == 0) return 0;
    battle_setup::g.open_window();
    ShowLine(battle_setup::g.msg(0x2D));
    return 1;
}

// original 0x430790 (PSX 0x801D54C8, the same): each member the chain does not
// skip with status 0x800 and its counter +0x143 at 5: the counter 0, HP =
// max HP, AP = max AP, +0x9C = +0xAE, then Battle_ClearStatus(member, 0x8FF).
// If any, the window, and with one member its name (0x44A910) and line 0x2B,
// with more line 0x2C. Enemies are not looked at.
extern "C" unsigned __cdecl BattleStep_Restore800() {
    unsigned count = 0, who = 0;
    for (unsigned i = 0; i <= 2; ++i) {
        if (Yes(battle_setup::g.skipped(i))) continue;
        unsigned char* const o = P(i);
        auto* const member_rec = rv::As<records::PartyMember>(o);
        if ((o[battle_setup::at::kPStatus + 1] & 8) == 0 || member_rec->count_800 != 5) continue;
        count = (count + 1) & 0xFF;
        member_rec->count_800 = 0;
        member_rec->hp = member_rec->max_hp;
        member_rec->ap = member_rec->max_ap;
        member_rec->survive = member_rec->byte_ae;
        who = i;
        battle_setup::g.clear_status(i, 0x8FF);
    }
    if (count == 0) return 0;
    battle_setup::g.open_window();
    const unsigned char* text;
    if (count == 1) {
        battle_setup::g.party_name(who);
        text = battle_setup::g.msg(0x2B);
    } else {
        text = battle_setup::g.msg(0x2C);
    }
    ShowLine(text);
    return 1;
}

// original 0x430890 (PSX 0x801D5628, the same): the members' status 0x40,
// lines 0x25 / 0x26.
extern "C" unsigned __cdecl BattleStep_PartyWake40() { return WakeStep(true, 0x40, 0x25, 0x26); }

// original 0x430970 (PSX 0x801D577C, the same): the enemies' status 0x40,
// lines 0x25 / 0x27.
extern "C" unsigned __cdecl BattleStep_EnemyWake40() { return WakeStep(false, 0x40, 0x25, 0x27); }

// original 0x430A50 (PSX 0x801D58DC, the same): the members' status 0x20,
// lines 0x28 / 0x29.
extern "C" unsigned __cdecl BattleStep_PartyWake20() { return WakeStep(true, 0x20, 0x28, 0x29); }

// original 0x430B30 (PSX 0x801D5A30, the same): the enemies' status 0x20,
// lines 0x28 / 0x2A.
extern "C" unsigned __cdecl BattleStep_EnemyWake20() { return WakeStep(false, 0x20, 0x28, 0x2A); }

// original 0x430C10 (PSX 0x801D5B90, the same): each actor the chain does not
// skip with status 0x80: both change words zeroed (party +0x12A then +0x128,
// enemy +0x10A / +0x108), 0x446540(actor) sets the HP change, the effect
// marked (+0x12C / +0x10C = 0x11) and 0x446FB0 told; if any, the window and
// line 0x18.
extern "C" unsigned __cdecl BattleStep_Status80() {
    unsigned count = 0;
    for (unsigned i = 0; i <= 10; ++i) {
        if (Yes(battle_setup::g.skipped(i))) continue;
        const bool party = i <= 2;
        unsigned char* const o = party ? P(i) : E(i);
        if ((o[party ? battle_setup::at::kPStatus : battle_setup::at::kEStatus] & 0x80) == 0) continue;
        move_script::SetWord(o + (party ? battle_setup::at::kPApChange : battle_setup::at::kEApChange), 0);
        move_script::SetWord(o + (party ? battle_setup::at::kPHpChange : battle_setup::at::kEHpChange), 0);
        battle_setup::g.set_hp_change(i);
        MarkEffect(o);
        count = (count + 1) & 0xFF;
        o[party ? battle_setup::at::kPEffect : battle_setup::at::kEEffect] = 0x11;
        battle_setup::g.set_pending(i);
    }
    if (count == 0) return 0;
    battle_setup::g.open_window();
    ShowLine(battle_setup::g.msg(0x18));
    return 1;
}

// original 0x430D40 (PSX 0x801D5D9C, the same): each member the chain does not
// skip gets an HP change word (+0x128, and +0x12A zeroed), a u16 built from
// 0: less 1 when 0x904060 is 5; less (max HP + 10) / 20 for character id 6
// (+0x89); again for id 7 or 0 with flag 2 (+0x134) while 0x904B89 is 0x12;
// with weapon 0x52 (+0x92) plus (max HP + 10) / 20, then cut to HP - 1 when
// it reaches HP (signed against the HP word); less one for armour byte 3
// (+0x95) 0x1F, for each accessory (+0x96, +0x97) 0x16, and each 0x17; less
// max HP / 2 with status 1 (+0x90). Not 0: the effect marked (+0x12C = 0x11)
// and 0x446FB0 told. Each enemy with status 1 (+0x92): HP change (+0x108)
// minus max HP (+0xB0) / 2, marked (+0x10C = 0x11), 0x446FB0. Answers whether
// any actor's bit is set in 0x904B82.
// The divisions are the original's: (max HP + 10) / -20 through the
// 0x99999999 multiply, which truncates as C does. The word is built in a
// local and stored once - nothing it passes reads it back.
extern "C" unsigned __cdecl BattleStep_HpDrift() {
    for (unsigned i = 0; i <= 2; ++i) {
        if (Yes(battle_setup::g.skipped(i))) continue;
        unsigned char* const o = P(i);
        auto* const member_rec = rv::As<records::PartyMember>(o);
        std::uint16_t w = 0;
        rv::Store<unsigned>(member_rec->ap_change, 0);
        if (Cond_Flags[0xd0] == 5) --w;
        const int n = static_cast<int>(member_rec->max_hp) + 10;
        if (member_rec->char_id == 6) w = static_cast<std::uint16_t>(w + n / -20);
        const unsigned char id = member_rec->char_id;
        if ((id == 7 || id == 0) && (o[battle_setup::at::kPFlags] & 2) && BattlePhases_B89 == 0x12)
            w = static_cast<std::uint16_t>(w + n / -20);
        if (member_rec->weapon == 0x52) {
            w = static_cast<std::uint16_t>(w + n / 20);
            const unsigned hp = member_rec->hp;
            if (static_cast<int>(static_cast<std::int16_t>(w)) >= static_cast<int>(hp)) w = static_cast<std::uint16_t>(hp - 1);
        }
        if (o[battle_setup::at::kPArmour3] == 0x1F) --w;
        if (member_rec->accessory[0] == 0x16) --w;
        if (member_rec->accessory[1] == 0x16) --w;
        if (member_rec->accessory[0] == 0x17) --w;
        if (member_rec->accessory[1] == 0x17) --w;
        if (o[battle_setup::at::kPStatus] & 1) w = static_cast<std::uint16_t>(w - (member_rec->max_hp >> 1));
        rv::Store<unsigned>(member_rec->hp_change, w);
        if (w == 0) continue;
        MarkEffect(o);
        o[battle_setup::at::kPEffect] = 0x11;
        battle_setup::g.set_pending(i);
    }
    for (unsigned i = 3; i <= 10; ++i) {
        if (Yes(battle_setup::g.skipped(i))) continue;
        unsigned char* const o = E(i);
        auto* const enemy_rec = rv::As<records::BattleEnemy>(o);
        if ((o[battle_setup::at::kEStatus] & 1) == 0) continue;
        enemy_rec->hp_change = 0u - (enemy_rec->max_hp >> 1);
        MarkEffect(o);
        enemy_rec->shown = 0x11;
        battle_setup::g.set_pending(i);
    }
    return BattleActions_Pending != 0 ? 1 : 0;
}

// original 0x430F40 (PSX 0x801D62A8, the same): each member the chain does not
// skip with flag 2 and not flag 0x20 (+0x134) pays the AP cost, 0x904B78's
// low byte + 1 halved: with less AP (+0x9A) than that, +1 +2 +3 = 6, 4, 4 and
// 0x446FB0 told; otherwise both change words zeroed, the AP lowered, the AP
// change (+0x12A) the cost, and a cost not 0 marks the effect (+0x12C = 2)
// and tells 0x446FB0. A member without flag 2, or with 0x20, loses 0x20.
// Answers whether any actor's bit is set in 0x904B82.
extern "C" unsigned __cdecl BattleStep_ApUpkeep() {
    for (unsigned i = 0; i <= 2; ++i) {
        if (Yes(battle_setup::g.skipped(i))) continue;
        unsigned char* const o = P(i);
        auto* const member_rec = rv::As<records::PartyMember>(o);
        const std::uint32_t flags = static_cast<std::uint32_t>(static_cast<std::int32_t>(member_rec->flags_134));
        if ((flags & 2) == 0 || (flags & 0x20) != 0) {
            member_rec->flags_134 = static_cast<std::int32_t>(flags & ~0x20u);
            continue;
        }
        const int cost = static_cast<int>((static_cast<std::uint32_t>(move_script::Long(move_script::At(::bof3::Img(BattleActions_ApCost_addr)))) & 0xFF) + 1) / 2;
        if (static_cast<int>(member_rec->ap) < cost) {
            member_rec->state = 6;
            member_rec->step = 4;
            member_rec->state3 = 4;
            battle_setup::g.set_pending(i);
            continue;
        }
        rv::Store<unsigned>(member_rec->hp_change, 0);
        rv::Store<unsigned>(member_rec->ap_change, 0);
        member_rec->ap = member_rec->ap - static_cast<unsigned>(cost);
        rv::Store<unsigned>(member_rec->ap_change, static_cast<unsigned>(cost));
        if ((cost & 0xFFFF) == 0) continue;
        MarkEffect(o);
        o[battle_setup::at::kPEffect] = 2;
        battle_setup::g.set_pending(i);
    }
    return BattleActions_Pending != 0 ? 1 : 0;
}

// original 0x431030 (PSX 0x801D6494, the same): 1 when the actor is left out
// of the chain's steps - while the timer 0x904B8E runs, any actor without
// flag 0x10 (party +0x134, enemy +0x114); otherwise when 0x4456C0 says so
// (not present, or status 0x4000). The actor is the argument's low byte; the
// whole argument goes to 0x4456C0, which masks it. The answer is al (every
// caller, all in this file, tests al alone).
extern "C" unsigned __cdecl Battle_ActorSkipped(unsigned actor) {
    const unsigned a = actor & 0xFF;
    if (BattleFlow_AnimGate != 0) {
        const unsigned char flags = a <= 2 ? P(a)[battle_setup::at::kPFlags] : E(a)[battle_setup::at::kEFlags];
        if ((flags & 0x10) == 0) return 1;
    }
    return Yes(battle_setup::g.actor_absent(actor)) ? 1 : 0;
}

// original 0x431090 (PSX 0x801D653C): nothing while any of the eight dwords
// 0x803540.. is set, or while the event hook 0x904B6C, called with 2 in an
// event battle, answers al 0xFF. With 0x904AE8 set the battle is over: round
// flag 0x10 cleared, byte +0x24 of eleven message windows zeroed (all but the
// third), phase 5 from its step 0. Otherwise the next round: unless round
// flag 0x10 is set, the command bytes 0x904AB4 / 0x904ABC.. reset and file
// 0x131 loaded; Battle_ClearCommands; each member Battle_ActorIsOut does not
// rule out to state 2; round flag 8 cleared, 0x904AE4 zeroed, phase 1 from 0.
// The member index goes out as a byte (the original's local slot carries the
// caller's upper bits, which Battle_ActorIsOut masks off).
//
// DIV-0108 (psp_code.h): with the PSP's duel-loss on, an end byte with bit
// 0 set and bit 2 clear in event battle 0x1B is not the battle's end - the
// PSP's added test (PSP-JP 0x11F36C), after the hook. Off, the original's.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleRoundEnd_NextRound() {
    for (unsigned i = 0x10; i < 0x18; ++i)
        if (move_script::Long(move_script::At(::bof3::Img(0x803500) + i * 4)) != 0) return;
    if (EventBattle_Id != 0) {
        const auto hook = reinterpret_cast<unsigned (__cdecl*)(int)>(
            BattleHook_Event);
        if ((hook(2) & 0xFF) == 0xFF) return;
    }
    if (Battle_EndFlags != 0 && !psp_code::DuelLossGoesOn(EventBattle_Id, Battle_EndFlags)) {
        BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kRoundFlags.v)) &= 0xEF;
        for (unsigned w : {0u, 3u, 1u, 11u, 10u, 9u, 8u, 7u, 6u, 5u, 4u}) BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kWindows.v) + w * battle_turn_steps::at::kWindowStride) = 0;
        BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kPhase.v)) = 5;
        BattleActions_Step = 0;
        return;
    }
    if ((BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kRoundFlags.v)) & 0x10) == 0) {
        move_script::SetLong(move_script::At(::bof3::Img(battle_turn_steps::at::kCmdLong.v)), 0);
        BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kCmdKind.v)) = 8;
        move_script::SetWord(move_script::At(::bof3::Img(battle_turn_steps::at::kCmdWord.v)), 0);
        BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kCmdByte.v)) = 0;
        battle_turn_steps::g.load_dat(0x131);
    }
    battle_turn_steps::g.clear_commands();
    for (unsigned i = 0; i <= 2; ++i)
        if (!Yes(battle_turn_steps::g.actor_is_out(i))) rv::As<records::PartyMember>(MemberBattleTurnSteps(i))->state = 2;
    BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kRoundFlags.v)) &= 0xF7;
    BattlePhases_Initiative = 0;
    BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kPhase.v)) = 1;
    BattleActions_Step = 0;
    BattleE4_Step2 = 0;
}

// original 0x4311E0: phase 5's dispatch - entry 0x904AA1 of BattleEnd_Steps
// (0x64AF44: BattleEnd_TaskStep, BattleEnd_WinStep, BattleEnd_LossDispatch 0x431540 and BattleEnd_RestoreDispatch 0x4315B0
// (the other way out, Capcom's), BattleEnd_ExitStep).
extern "C" void __cdecl BattleEnd_Step() {
    [[clang::musttail]] return EntryBattleTurnSteps(::bof3::Img(BattleEnd_Steps_addr), BattleActions_Step)();
}

// original 0x4311F0: entry 0x904AA2 of BattleEnd_TaskSteps (0x64AF58:
// BattleEnd_TasksBegin, BattleEnd_StartMemberTask, BattleEnd_AwaitMemberTasks).
extern "C" void __cdecl BattleEnd_TaskStep() {
    [[clang::musttail]] return EntryBattleTurnSteps(::bof3::Img(BattleEnd_TaskSteps_addr), BattleE4_Step2)();
}

// original 0x431200: 0x802D20 and the member cursor 0x904AA5 zeroed, 0x904AA2
// stepped on. Also the other way out's first step (0x64AF7C).
extern "C" void __cdecl BattleEnd_TasksBegin() {
    BossSa_WindowPass = 0;
    BossSb_Countdown = 0;
    Inc(::bof3::Img(BattleE4_Step2_addr));
}

// original 0x431220: from the cursor 0x904AA5 on, the first member (up to the
// count 0x904AB0) that Battle_ActorIsOut does not rule out and that has flag
// 1 or 2 (+0x134) gets flag 0x2000 (+0x130) and a kind-0 battle task,
// parameter 0xB for character 4 and otherwise 0xC - then, for the other
// characters, bit 0 of +0x134 cleared in the three records at 0x939AE0 -
// owned by the member (the task's +0x80); the cursor steps past it. Either
// way 0x904AA2 steps on. Also the other way out's second step (0x64AF80).
// As the original has it: the cursor is re-read after each call and the
// count at each test; BattleTask_Create's 0xFF (no slot free) is not tested,
// so the owner then lands at 0x9423FC, past the slots.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_StartMemberTask() {
    unsigned cursor = BossSb_Countdown;
    if (cursor < BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kMembers.v))) {
        for (;;) {
            const unsigned out = battle_turn_steps::g.actor_is_out(cursor);
            cursor = BossSb_Countdown;
            if (!Yes(out) && (MemberBattleTurnSteps(cursor)[battle_turn_steps::at::kFlags] & 3) != 0) {
                MemberBattleTurnSteps(cursor)[battle_turn_steps::at::kFlags2 + 1] |= 0x20;
                unsigned parameter = 0xB;
                if (rv::As<records::PartyMember>(MemberBattleTurnSteps(cursor))->char_id != 4) {
                    for (unsigned k = 0; k < 3; ++k) move_script::At(::bof3::Img(battle_turn_steps::at::kCopies.v) + k * battle_turn_steps::at::kPartyStride)[battle_turn_steps::at::kFlags] &= 0xFE;
                    parameter = 0xC;
                }
                const unsigned task = battle_turn_steps::g.task_create(0, parameter) & 0xFF;
                move_script::SetLong(move_script::At(::bof3::Img(BattleTask_Slots_addr) + task * battle_turn_steps::at::kTaskSize + 0x80),
                        static_cast<std::int32_t>(Low32(MemberBattleTurnSteps(BossSb_Countdown))));
                Inc(::bof3::Img(BossSb_Countdown_addr));
                break;
            }
            const unsigned count = BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kMembers.v));
            cursor = (cursor + 1) & 0xFF;
            BossSb_Countdown = static_cast<unsigned char>(cursor);
            if (cursor >= count) break;
        }
    }
    Inc(::bof3::Img(BattleE4_Step2_addr));
}

// original 0x431320 (PSX 0x801D69E4): nothing while a member (to the count
// 0x904AB0 as it was on entry) still has flag 0x2000; back to the previous
// step while the cursor has not reached that count. Then, unless 0x904AE8
// bit 0 is set, each member (the count re-read after each) - Sprite_Current
// set to it - with status 0x800 gets HP and AP back to their maxima, +0x9C
// from +0xAE and Battle_ClearStatus(status & 0xBFFF); then, every member,
// Battle_ClearStatus(status & 0xBF5F), Sprite_ReleaseTint and
// Battle_ReturnQueuedItem; and the timer 0x904B8E zeroed. Then, outside an
// event battle, 0x904AA1 = 2 for 0x904AE8 bit 0 and 1 for bit 1 (bit 1 wins);
// the hook 0x904B64; Port_DroppedCall(0); file 0xCD for step 1, 0xCE for step 2
// (0x904AA1 re-read for each); 0x904AA2 zeroed.
// The 0x904AE8 the way out is picked from is the one read before the members
// (bit 0 set) or re-read after them.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_AwaitMemberTasks() {
    const unsigned count = BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kMembers.v));
    for (unsigned i = 0; i < count; ++i)
        if (move_script::Long(MemberBattleTurnSteps(i) + battle_turn_steps::at::kFlags2) & 0x2000) return;
    if (BossSb_Countdown != count) {
        --*move_script::At(::bof3::Img(BattleE4_Step2_addr));
        return;
    }
    unsigned end = Battle_EndFlags;
    if ((end & 1) == 0) {
        if (count != 0) {
            for (unsigned i = 0;;) {
                unsigned char* const m = MemberBattleTurnSteps(i);
                auto* const member_rec = rv::As<records::PartyMember>(m);
                Sprite_Current = m;
                if (m[battle_turn_steps::at::kStatus + 1] & 8) {
                    member_rec->hp = member_rec->max_hp;
                    member_rec->ap = member_rec->max_ap;
                    member_rec->survive = member_rec->byte_ae;
                    battle_turn_steps::g.clear_status(i, member_rec->status & 0xBFFF);
                }
                battle_turn_steps::g.clear_status(i, member_rec->status & 0xBF5F);
                battle_turn_steps::g.release_tint(m);
                battle_turn_steps::g.return_item(i);
                i = (i + 1) & 0xFF;
                if (i >= BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kMembers.v))) break;
            }
            end = Battle_EndFlags;
        }
        BattleFlow_AnimGate = 0;
    }
    if (EventBattle_Id == 0) {
        if (end & 1) BattleActions_Step = 2;
        if (end & 2) BattleActions_Step = 1;
    }
    reinterpret_cast<Step>(BattleHook_End)();
    battle_turn_steps::g.dropped_call(0);
    if (BattleActions_Step == 1) battle_turn_steps::g.load_dat(0xCD);
    if (BattleActions_Step == 2) battle_turn_steps::g.load_dat(0xCE);
    BattleE4_Step2 = 0;
}

// original 0x4314B0: the win's dispatch - entry 0x904AA2 of BattleEnd_WinSteps
// (0x64AF64: BattleEnd_WinBegin, BattleEnd_WinAwaitInput, BattleEnd_ResultStep).
extern "C" void __cdecl BattleEnd_WinStep() {
    [[clang::musttail]] return EntryBattleTurnSteps(::bof3::Img(BattleEnd_WinSteps_addr), BattleE4_Step2)();
}

// original 0x4314C0: each of the three members present (+0 bit 0) to state 2;
// unless 0x904AE5 bit 0x40 keeps the battle's music, Music_FadeOutStop(10)
// and Music_Play(0xA5, 10); Battle_OpenMsgWindow, Battle_OpeningMessage (its
// lines by 0x904B90 - named for the opening, but here it is the win's), and
// 0x904AA2 on.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_WinBegin() {
    for (unsigned i = 0; i < 3; ++i)
        if (rv::As<records::PartyMember>(MemberBattleTurnSteps(i))->flags & 1) rv::As<records::PartyMember>(MemberBattleTurnSteps(i))->state = 2;
    if ((EventBattle_Flags & 0x40) == 0) {
        battle_turn_steps::g.fade_out(10);
        battle_turn_steps::g.music_play(0xA5, 10);
    }
    battle_turn_steps::g.open_window();
    battle_turn_steps::g.opening_message();
    Inc(::bof3::Img(BattleE4_Step2_addr));
}

// original 0x431520: 0x904AA2 on once File_LoadDone answers (it always does)
// and Input_Pressed is not 0.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_WinAwaitInput() {
    if (battle_turn_steps::g.load_done() != 0 && Input_Pressed != 0) Inc(::bof3::Img(BattleE4_Step2_addr));
}

// original 0x431540 (BattleEnd_Steps[2], 0x904AE8 bit 0's way): jmp
// [BattleEnd_LossSteps + 4 * byte 0x904AA2], 3 (BattleEnd_LossBanner,
// BattleEnd_LossAwaitLoad, BattleLoss_Dispatch).
extern "C" unsigned long __cdecl BattleEnd_LossDispatch(unsigned long through) {
    return DispatchRest3a("BattleEnd_LossDispatch", ::bof3::Img(BattleEnd_LossSteps_addr), BattleE4_Step2, 3, through);
}

// original 0x431550 (BattleEnd_LossSteps[0]): unless 0x904AE5 bit 0x40 keeps
// the battle's music, Music_FadeOutStop(10) and Music_Play(0xA6, 10);
// Battle_OpenMsgWindow; BattleBanner_Set(0, 2, 0, 0, 0xFF, Msg_SystemPtr(0x11));
// 0x904AA2 + 1 (read after the calls). The win's BattleEnd_WinBegin plays 0xA5.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_LossBanner(void) {
    if ((EventBattle_Flags & 0x40) == 0) {
        BH_CALL(Music_FadeOutStop)(10);
        BH_CALL(Music_Play)(0xA6, 10);
    }
    BH_CALL(Battle_OpenMsgWindow)();
    const unsigned char* const text = BH_CALL(Msg_SystemPtr)(0x11);
    BH_CALL(BattleBanner_Set)(0, 2, 0, 0, 0xFF, reinterpret_cast<const char*>(text));
    BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// original 0x4315A0 (BattleEnd_LossSteps[1]): 0x904AA2 + 1 once File_LoadDone
// answers (its whole eax tested).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_LossAwaitLoad(void) {
    if (BH_CALL(File_LoadDone)() != 0) BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// original 0x4315B0 (BattleEnd_Steps[3]): jmp [BattleEnd_RestoreSteps + 4 *
// byte 0x904AA2], 5 (BattleEnd_TasksBegin, BattleEnd_StartMemberTask,
// BattleEnd_AwaitRestore, BattleEnd_RestoreLoadBank, BattleEnd_RestoreAwaitBank).
extern "C" unsigned long __cdecl BattleEnd_RestoreDispatch(unsigned long through) {
    return DispatchRest3a("BattleEnd_RestoreDispatch", ::bof3::Img(BattleEnd_RestoreSteps_addr), BattleE4_Step2, 5, through);
}

// original 0x4315C0 (0x64AF7C[2], the third way out's steps by 0x904AA2; PSX 0x801D6EF8):
// nothing while a member (of 0x904AB0, read once) has 0x2000 in its +0x130;
// back a step (0x904AA2 - 1) while 0x904AA5 is short of the count; then
// Port_DroppedCall(0); each member Battle_ClearStatus(i, word +0x90 &
// 0xBF5F) and Sprite_ReleaseTint; each member with +0 bit 0, as
// Sprite_Current, Sprite_AnimFromSet(+0x4B, word +0x58 - 2, 0x8C5D80,
// 0x1800) - the count re-read after each of those calls only -;
// PartySet_Select(0x90412C & 0x7F, 0); 0x904AA2 + 1.
extern "C" void __cdecl BattleEnd_AwaitRestore(void) {
    const unsigned char n = BBattleE1(::bof3::Img(battle_e1::at::kPartyCount.v));
    for (unsigned char i = 0; i < n; ++i)
        if (MemberFlags(i) & 0x2000) return;
    if (BossSb_Countdown != n) {
        BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 - 1);
        return;
    }
    BH_CALL(Port_DroppedCall)(0);
    unsigned char count = BBattleE1(::bof3::Img(battle_e1::at::kPartyCount.v));
    for (unsigned char i = 0; i < count;) {
        unsigned char* const m = Member(i);
        auto* const member_rec = rv::As<records::PartyMember>(m);
        BH_CALL(Battle_ClearStatus)(i, member_rec->status & 0xBF5Fu);
        BH_CALL(Sprite_ReleaseTint)(m);
        count = BBattleE1(::bof3::Img(battle_e1::at::kPartyCount.v));
        ++i;
    }
    for (unsigned char i = 0; i < count;) {
        unsigned char* const m = Member(i);
        auto* const member_rec = rv::As<records::PartyMember>(m);
        if (member_rec->flags & 1) {
            Sprite_Current = m;
            BH_CALL(Sprite_AnimFromSet)(member_rec->animation, static_cast<std::uint16_t>(member_rec->script_pos - 2), move_script::At(::bof3::Img(battle_e1::at::kAnimBuffer.v)), 0x1800);
            count = BBattleE1(::bof3::Img(battle_e1::at::kPartyCount.v));
        }
        ++i;
    }
    BH_CALL(PartySet_Select)(AreaW0b_PartySet & 0x7Fu, 0);
    BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// original 0x431710 (BattleEnd_RestoreSteps[3]): once File_LoadDone answers,
// Snd_LoadBankFile(Game_AreaNumber + 3) (the word zero-extended) and
// 0x904AA2 + 1 (read after the call).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_RestoreLoadBank(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    BH_CALL(Snd_LoadBankFile)(Game_AreaNumber + 3);
    BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// original 0x431740 (BattleEnd_RestoreSteps[4]): once File_LoadDone answers,
// 0x904AA1 = 4 (BattleEnd_ExitStep) and 0x904AA2 = 0.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_RestoreAwaitBank(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    BattleActions_Step = 4;
    BattleE4_Step2 = 0;
}

// original 0x431760: the exit's dispatch - entry 0x904AA2 of
// BattleEnd_ExitSteps (0x64AF90: Battle_WriteBackParty, BattleEnd_ExitHook,
// BattleEnd_Finish, BattleEnd_ExitAwaitFade 0x4318F0 (R3A's)).
extern "C" void __cdecl BattleEnd_ExitStep() {
    [[clang::musttail]] return EntryBattleTurnSteps(::bof3::Img(BattleEnd_ExitSteps_addr), BattleE4_Step2)();
}

// original 0x431770 (PSX Battle_WriteBackParty 0x801D71B0, the sibling's
// name: Battle_WriteBackMember(0..2), 0x8015990C, 0x801629CC(0x256), the
// state on): 0x446A80(0), (1), (2), Window_ResetAll, LoadDatFile(2), 0x904AA2
// on.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Battle_WriteBackParty() {
    battle_turn_steps::g.write_back_member(0);
    battle_turn_steps::g.write_back_member(1);
    battle_turn_steps::g.write_back_member(2);
    battle_turn_steps::g.window_reset();
    battle_turn_steps::g.load_dat(2);
    Inc(::bof3::Img(BattleE4_Step2_addr));
}

// original 0x4317B0: once File_LoadDone answers - the hook 0x904B68; unless
// 0x904AE5 bit 0x40, Music_FadeOutStop(10) and, when Music_Track is then
// 0xFF, 0x904AE8 |= 8 - 0x904AA2 on.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_ExitHook() {
    if (battle_turn_steps::g.load_done() == 0) return;
    reinterpret_cast<Step>(BattleHook_Exit)();
    if ((EventBattle_Flags & 0x40) == 0) {
        battle_turn_steps::g.fade_out(10);
        if (Music_Track == 0xFF) Battle_EndFlags |= 8;
    }
    Inc(::bof3::Img(BattleE4_Step2_addr));
}

// original 0x4317F0 (PSX 0x801D7320): 0x494E70, BattleTask_ClearAll; each
// member (to the count 0x904AB0, re-read after each) with status 0x4000 (+0x91
// bit 6): its CharacterRecord (+0x148) gets HP 1, +0x1C = 1, +0x1E up by one
// while below 5, status 0x4000 and +0xB bit 2 cleared, and Char_RecalcStats.
// Then 0x446600, Game_Step up by one, 0x904AE9 and the five state bytes
// 0x904AA0..0x904AA4 zeroed.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_Finish() {
    battle_turn_steps::g.clear_enemies();
    battle_turn_steps::g.task_clear_all();
    if (BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kMembers.v)) != 0) {
        for (unsigned i = 0;;) {
            const unsigned char* const m = MemberBattleTurnSteps(i);
            auto* const member_rec = rv::AsConst<records::PartyMember>(m);
            if (m[battle_turn_steps::at::kStatus + 1] & 0x40) {
                unsigned char* const r = move_script::At(::bof3::Img(CharacterRecords_addr) + member_rec->char_index * battle_turn_steps::at::kRecordStride);
                auto* const char_rec = rv::As<records::CharRecord>(r);  // 0x903A70 + 0xA4 n
                static_assert(battle_turn_steps::at::kRecordStride == sizeof(records::CharRecord));
                rv::Store<unsigned>(char_rec->hp, 1);
                r[0x1C] = 1;
                if (r[0x1E] < 5) ++r[0x1E];
                rv::Store<unsigned>(char_rec->status, char_rec->status & 0xBFFF);
                r[0xB] &= 0xFB;
                battle_turn_steps::g.recalc_stats(r);
            }
            i = (i + 1) & 0xFF;
            if (i >= BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kMembers.v))) break;
        }
    }
    battle_turn_steps::g.reload_party();
    Game_Step = Game_Step + 1;
    BattleMisc_BannerKinds = 0;
    BBattleTurnSteps(::bof3::Img(battle_turn_steps::at::kPhase.v)) = 0;
    BattleActions_Step = 0;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
}

// original 0x4318F0 (BattleEnd_ExitSteps[3]): once Transition_Busy is 0,
// Draw_PassFlags = 0 and 0x904AA2 - 1 (back to BattleEnd_Finish).
extern "C" void __cdecl BattleEnd_ExitAwaitFade(void) {
    if (Transition_Busy != 0) return;
    const unsigned char step = BattleE4_Step2;
    Draw_PassFlags = 0;
    BattleE4_Step2 = static_cast<unsigned char>(step - 1);
}

// original 0x431910: entry 0x904AA3 of BattleEnd_ResultSteps (0x64AFA0:
// BattleEnd_ResultPage, then group CD's 0x431B60 and 0x431D50).
extern "C" void __cdecl BattleEnd_ResultStep() {
    [[clang::musttail]] return EntryBattleTurnSteps(::bof3::Img(BattleEnd_ResultSteps_addr), BattleE4_Step3)();
}

// original 0x431920: entry (dword 0x904AA4 & 0xFF) of BattleResult_ExpSteps
// (0x64AFAC: group CD's 0x431940, 0x431A20, 0x431A90, 0x431AB0, 0x431B30).
extern "C" void __cdecl BattleEnd_ResultPage() {
    [[clang::musttail]] return EntryBattleTurnSteps(::bof3::Img(BattleResult_ExpSteps_addr), BattleE4_Step4)();
}

void BattleActionFromBattleActions_Inject() {
    {
        // DIV-0167: after the self-test, which compared Capcom's preload.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("EffectKeyOff", Low32(&battle_actions::g_effect_key_off), &was, &is, 1);
        bof3::Log("DIV-0167    a battle action's effect stops the sound-effect channels the last one left (on unless the line above says OFF)");
    }
    sound_st::ArmOneShotKeyOff();  // DIV-0174: after the self-test, which compared Capcom's preload
    // DIV-0179: BOF3X_UNLEARNED_BLUE = 0 (the original's white, the default)
    // or 1; read after the self-test, which compared the banner with it off,
    // and after the injects, so that IsEnabled sees the check ours.
    {
        char v[8] = "0";
        const DWORD n = GetEnvironmentVariableA("BOF3X_UNLEARNED_BLUE", v, sizeof v);
        if (n >= sizeof v || (std::strcmp(v, "0") != 0 && std::strcmp(v, "1") != 0))
            bof3::Fatal("BOF3X_UNLEARNED_BLUE is \"%s\": 0 or 1", n >= sizeof v ? "(too long)" : v);
        battle_actions::g_unlearned_blue = v[0] == '1';
        // its check that BattleAction_AbilityCheck is ours: in BattleAction_Inject,
        // after the bind it tests
        if (battle_actions::g_unlearned_blue) bof3::Log("DIV-0179    an enemy's skill the party can still learn has its name in blue");
    }
}

void BattleActionFromBattleTurnSteps_Inject() {
    {
        // DIV-0154: after the self-test, which compared Capcom's bounds.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("ExtraRoundClear", Low32(&battle_turn_steps::g_extra_round_clear), &was, &is, 1);
        bof3::Log("DIV-0154    the round's end clears the faster-side mark from every member and enemy (on unless the line above says OFF)");
    }
}

void BattleAction_Inject() {
    BOF3_INJECT(BattleInput_Dispatch);
    BOF3_INJECT(Battle_RoundStart);
    BOF3_INJECT(BattleInput_NextMember);
    BOF3_INJECT(BattleMenu_CommandSelect);
    BOF3_INJECT(BattleHold_Dispatch);
    BOF3_INJECT(BattleHold_Shrink);
    BOF3_INJECT(BattleHold_ShowAll);
    BOF3_INJECT(BattleHold_Regrow);
    BOF3_INJECT(BattleMenu_ConfirmDispatch);
    BOF3_INJECT(Cmd_ConfirmDefend);
    BOF3_INJECT(Cmd_AutoBattle);
    BOF3_INJECT(BattleCommit_Dispatch);
    BOF3_INJECT(Battle_CommitRound);
    BOF3_INJECT(BattleCommit_QueueMessages);
    BOF3_INJECT(BattleCommit_WaitLoad);
    BOF3_INJECT(Battle_ActionPhase);
    BOF3_INJECT(BattleAction_BeginStep);
    BOF3_INJECT(Battle_BeginAction);
    BOF3_INJECT(BattleAction_EnterKind);
    BOF3_INJECT(BattleAction_KindStep);
    BOF3_INJECT(BattleAction_KindPlain);
    BOF3_INJECT(BattleAction_KindOne);
    BOF3_INJECT(BattleAction_Kind3Dispatch);
    BOF3_INJECT(BattleAction_Kind3Banner);
    BOF3_INJECT(BattleAction_Kind3Wait);
    BOF3_INJECT(BattleAction_AbilityStep);
    BOF3_INJECT(BattleAction_AbilityCheck);
    if (battle_actions::g_unlearned_blue && !bof3::IsEnabled(bof3::addr::BattleAction_AbilityCheck))
        bof3::Fatal("BOF3X_UNLEARNED_BLUE=1 needs BattleAction_AbilityCheck ours, and it is disabled");
    BOF3_INJECT(BattleAction_AbilityCommit);
    BOF3_INJECT(BattleAction_PickRandomAbility);
    BOF3_INJECT(BattleAction_AbilityStart);
    BOF3_INJECT(BattleAction_ItemStep);
    BOF3_INJECT(BattleAction_ItemCheck);
    BOF3_INJECT(BattleAction_ItemCommit);
    BOF3_INJECT(BattleAction_ItemStart);
    BOF3_INJECT(BattleAction_EffectStep);
    BOF3_INJECT(BattleAction_EffectWait);
    BOF3_INJECT(BattleAction_EffectPreload);
    BOF3_INJECT(BattleAction_EffectLoaded);
    BOF3_INJECT(BattleAction_AfterStep);
    BOF3_INJECT(BattleAction_AfterSettle);
    BOF3_INJECT(BattleAction_AbilityNotice);
    BOF3_INJECT(BattleAction_EnemyMessages);
    BOF3_INJECT(BattleAction_End);
    BOF3_INJECT(Battle_ClearActingFlags);
    BOF3_INJECT(BattleRoundEnd_Step);
    BOF3_INJECT(BattleRoundEnd_FasterStep);
    BOF3_INJECT(BattleRoundEnd_CheckFaster);
    BOF3_INJECT(Battle_TickCounters);
    BOF3_INJECT(BattleRoundEnd_FasterNext);
    BOF3_INJECT(BattleRoundEnd_StatusChain);
    BOF3_INJECT(BattleStep_Expire4000);
    BOF3_INJECT(BattleStep_Restore800);
    BOF3_INJECT(BattleStep_PartyWake40);
    BOF3_INJECT(BattleStep_EnemyWake40);
    BOF3_INJECT(BattleStep_PartyWake20);
    BOF3_INJECT(BattleStep_EnemyWake20);
    BOF3_INJECT(BattleStep_Status80);
    BOF3_INJECT(BattleStep_HpDrift);
    BOF3_INJECT(BattleStep_ApUpkeep);
    BOF3_INJECT(Battle_ActorSkipped);
    BOF3_INJECT(BattleRoundEnd_NextRound);
    BOF3_INJECT(BattleEnd_Step);
    BOF3_INJECT(BattleEnd_TaskStep);
    BOF3_INJECT(BattleEnd_TasksBegin);
    BOF3_INJECT(BattleEnd_StartMemberTask);
    BOF3_INJECT(BattleEnd_AwaitMemberTasks);
    BOF3_INJECT(BattleEnd_WinStep);
    BOF3_INJECT(BattleEnd_WinBegin);
    BOF3_INJECT(BattleEnd_WinAwaitInput);
    BOF3_INJECT(BattleEnd_LossDispatch);
    BOF3_INJECT(BattleEnd_LossBanner);
    BOF3_INJECT(BattleEnd_LossAwaitLoad);
    BOF3_INJECT(BattleEnd_RestoreDispatch);
    BOF3_INJECT(BattleEnd_AwaitRestore);
    BOF3_INJECT(BattleEnd_RestoreLoadBank);
    BOF3_INJECT(BattleEnd_RestoreAwaitBank);
    BOF3_INJECT(BattleEnd_ExitStep);
    BOF3_INJECT(Battle_WriteBackParty);
    BOF3_INJECT(BattleEnd_ExitHook);
    BOF3_INJECT(BattleEnd_Finish);
    BOF3_INJECT(BattleEnd_ExitAwaitFade);
    BOF3_INJECT(BattleEnd_ResultStep);
    BOF3_INJECT(BattleEnd_ResultPage);
}
