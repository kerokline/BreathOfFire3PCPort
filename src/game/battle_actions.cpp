// The action phases: the battle's phase 3 (0x42F220, the fourth entry of
// Battle_PhaseDispatch's table) and the seven step tables under it. An
// action goes: the next actor out of the turn order (Battle_BeginAction), its
// command kind (0x904B35) dispatched - kinds 0 and 2, kind 1, the ability
// (kind 4) and the item (kind 5) each with their checks, and kind 3 through
// 0x42F5E0, which no group owns - then the effect steps, the steps after it
// (the enemy messages among them), and the end, where a set 0x40 in the round
// flags swaps the actor and the target and runs one more action. What the
// kinds are in game terms is not read here. docs/battle_actions.md.
//
// Every call goes through battle_actions::g (battle_actions_callees.h), so
// that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. The step tables are read in .data, afresh at
// each dispatch, as the originals read them. Everything here is a faithful
// replacement.
#include "game/battle_actions.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_actions_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_actions {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Battle_MemberAutoTarget,
    Fn<void (__cdecl*)(unsigned)>(kEnemyPickAction),
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
    Fn<unsigned char (__cdecl*)()>(kPickTarget),
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

using namespace battle_actions;

namespace {

unsigned char B(std::uint32_t address) { return At(address)[0]; }
void SetB(std::uint32_t address, unsigned v) { At(address)[0] = static_cast<unsigned char>(v); }
unsigned char* PtrAt(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(address)))));
}
void SetPtrAt(std::uint32_t address, const unsigned char* p) {
    SetLong(At(address), static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(p)));
}
std::int32_t L(std::uint32_t address) { return Long(At(address)); }

// The actor objects, by the actor byte: a member's (0..2), or an enemy's
// (actor - 3, as a 32-bit difference - the originals' `sub eax, 3` after
// `and eax, 0xFF`).
unsigned char* Party(unsigned actor) { return At(at::kParty + (actor & 0xFF) * at::kPartyStride); }
unsigned char* Enemy(unsigned actor) { return At(at::kEnemy + ((actor & 0xFF) - 3u) * at::kEnemyStride); }

// One entry of a step table in .data, read afresh; the byte picks it and is
// not checked (a byte past the table's count reaches the next table's
// entries, or its data, as in the original).
using Handler = void (__cdecl*)();
Handler Step(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(L(table + (index & 0xFF) * 4))));
}
using HookFn = void (__cdecl*)(int);
void EventHook(int n) { reinterpret_cast<HookFn>(PtrAt(at::kEventHook))(n); }

// A block's actor fields - 0x904B34's or 0x904B44's (+8 the object, +0xC
// its action record, +2 / +4 / +6 the screen position) - for a member: the
// object's words +0x2E / +0x30 plus the s8 pair of 0x64DF70 at (+8) + (char
// id +0x89) * 4, and +0x32; for an enemy: +0x2E / +0x30 plus its s8 +0xF2 /
// +0xF3, and +0x32. Each sum is 16-bit. No call is made in between, so the
// order of the reads and stores is not observable; it is the original's.
void SetMemberBlock(std::uint32_t block, unsigned actor) {
    unsigned char* const o = Party(actor);
    SetPtrAt(block + at::kBAction, o + 0x124);
    SetPtrAt(block + at::kBObject, o);
    const unsigned stand = o[8] + o[0x89] * 4u;
    const unsigned char* const pair = At(at::kStandOffsets + stand * 2);
    SetWord(At(block + at::kBX), static_cast<unsigned>(static_cast<signed char>(pair[0])) + Word(o + 0x2E));
    SetWord(At(block + at::kBY), static_cast<unsigned>(static_cast<signed char>(pair[1])) + Word(o + 0x30));
    SetWord(At(block + at::kBZ), Word(o + 0x32));
}
void SetEnemyBlock(std::uint32_t block, unsigned actor) {
    unsigned char* const o = Enemy(actor);
    SetPtrAt(block + at::kBAction, o + 0x104);
    SetPtrAt(block + at::kBObject, o);
    SetWord(At(block + at::kBX), static_cast<unsigned>(static_cast<signed char>(o[0xF2])) + Word(o + 0x2E));
    SetWord(At(block + at::kBY), static_cast<unsigned>(static_cast<signed char>(o[0xF3])) + Word(o + 0x30));
    SetWord(At(block + at::kBZ), Word(o + 0x32));
}

// The actor's effective stat block (a member's +0xA0, an enemy's +0xB0; the
// sibling's BATTLE_RAM.md: what Battle_BeginAction copies per action) to
// 0x939FE0, 32 bytes.
void CopyStats(unsigned actor) {
    const unsigned a = actor & 0xFF;
    std::memmove(At(at::kStats), a < 3 ? Party(a) + 0xA0 : Enemy(a) + 0xB0, 32);
}

// A system message as a banner, as the kind-2 enemy and both checks show one: the message
// window opened, then a kind-2 banner of the system message `id`, timer 0x2D.
void SystemBanner(unsigned id) {
    g.open_msg_window();
    const unsigned char* const text = g.msg(id);
    g.banner_add(2, 0, 0, 0x2D, reinterpret_cast<const char*>(text));
}

// The enemy's name record (+0x80, 12 bytes) into Text_Records[0], zeroed first.
void EnemyNameToText0(const unsigned char* record) {
    std::memset(At(at::kText0), 0, 32);
    std::memcpy(At(at::kText0), record, 12);
}

// A member's or an enemy's u16 at +0x90 / +0x92 (status) and AP +0x9A / +0xA6.
struct Standing { unsigned status, ap; };
Standing ActorStanding(unsigned actor) {
    const unsigned a = actor & 0xFF;
    if (a <= 2) return {Word(Party(a) + 0x90), Word(Party(a) + 0x9A)};
    return {Word(Enemy(a) + 0x92), Word(Enemy(a) + 0xA6)};
}

}  // namespace

// ===========================================================================
// Phase 3 and the step dispatches
// ===========================================================================

// original 0x42F220 (PSX 0x801D2AA0, catalogue pair): the battle's phase 3,
// from Battle_PhaseDispatch. Calls entry 0x904AA1 of BattleAction_Steps
// (0x64AE80, five entries); then, when any button of Field_CancelButtons is
// held (Input_Held), clears bit 4 of the round flags 0x904AA8.
//
// As the original has it: both words are read after the step; the step byte
// is not checked.
extern "C" void __cdecl Battle_ActionPhase(void) {
    Step(at::kSteps, B(at::kStep))();
    if ((Word(At(at::kCancelButtons)) & Word(At(at::kInputHeld))) != 0)
        SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & 0xFFEFu);
}

// original 0x42F250 (PSX 0x801D2B14): step 0, a tail jump through
// BattleAction_BeginSteps (0x64AE94) by 0x904AA2: 0 Battle_BeginAction, 1
// BattleAction_EnterKind.
extern "C" void __cdecl BattleAction_BeginStep(void) { Step(at::kBeginSteps, B(at::kSub))(); }

// original 0x42F500 (PSX 0x801D300C): step 1, a tail jump through
// BattleAction_KindSteps (0x64AE9C) by 0x904AA2, which BattleAction_EnterKind
// set to the command kind: 0 and 2 BattleAction_KindPlain, 1
// BattleAction_KindOne, 3 0x42F5E0 (no group's), 4 BattleAction_AbilityStep,
// 5 BattleAction_ItemStep.
extern "C" void __cdecl BattleAction_KindStep(void) { Step(at::kKindSteps, B(at::kSub))(); }

// original 0x42F670 (PSX 0x801D32C0): kind 4's, through
// BattleAction_AbilitySteps (0x64AEBC) by 0x904AA3.
extern "C" void __cdecl BattleAction_AbilityStep(void) { Step(at::kAbilitySteps, B(at::kSub2))(); }

// original 0x42FAF0 (PSX 0x801D3A08): kind 5's, through BattleAction_ItemSteps
// (0x64AF08) by 0x904AA3.
extern "C" void __cdecl BattleAction_ItemStep(void) { Step(at::kItemSteps, B(at::kSub2))(); }

// original 0x42FC50 (PSX 0x801D3C68): step 2, through BattleAction_EffectSteps
// (0x64AF14) by 0x904AA2.
extern "C" void __cdecl BattleAction_EffectStep(void) { Step(at::kEffectSteps, B(at::kSub))(); }

// original 0x42FDD0 (PSX 0x801D3FCC): step 3, through BattleAction_AfterSteps
// (0x64AF20) by 0x904AA2: 0 BattleAction_AfterSettle, 1 0x42FE20 (no group's),
// 2 BattleAction_EnemyMessages.
extern "C" void __cdecl BattleAction_AfterStep(void) { Step(at::kAfterSteps, B(at::kSub))(); }

// ===========================================================================
// Step 0: the next actor
// ===========================================================================

// original 0x42F260 (the sibling's Battle_BeginAction, catalogue pair; PSX address not read): from
// the turn order 0x904ACC, the first slot from 0x904AE2 up to (not including)
// 0x904AE3 that is not 0xFF - 0x904AE2 follows the search. None left: the
// phase becomes 4, the step 0. Else the actor picks its action
// (Battle_MemberAutoTarget, or 0x435AB0 with actor - 3), becomes 0x904B34 and
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
    unsigned char i = B(at::kQueueAt);
    const unsigned char end = B(at::kQueueEnd);
    for (;;) {
        if (i >= end) {
            SetB(at::kPhase, 4);
            SetB(at::kStep, 0);
            return;
        }
        if (B(at::kQueue + i) != 0xFF) break;
        ++i;
        SetB(at::kQueueAt, i);
    }
    const unsigned actor = B(at::kQueue + i);
    if (actor < 3) g.member_auto_target(actor);
    else g.enemy_pick_action((actor - 3) & 0xFF);
    SetB(at::kActor, actor);
    if (actor < 3) SetMemberBlock(at::kActor, actor);
    else SetEnemyBlock(at::kActor, actor);
    CopyStats(actor);
    const unsigned target = B(at::kTarget);
    if (target < 3) SetMemberBlock(at::kTarget, target);
    else if (target <= 10) SetEnemyBlock(at::kTarget, target);
    SetB(at::kSub, B(at::kSub) + 1);
    SetB(at::kQueueAt, B(at::kQueueAt) + 1);
}

// original 0x42F4C0 (PSX 0x801D2FB8): step 0's second part. In an event
// battle (0x904AAA) the hook 0x904B6C is called with 1 first; then the step
// becomes 1 and 0x904AA2 the command kind 0x904B35, read after the hook.
extern "C" void __cdecl BattleAction_EnterKind(void) {
    if (B(at::kEventBattle) != 0) EventHook(1);
    const unsigned kind = B(at::kActor + at::kBKind);
    SetB(at::kStep, 1);
    SetB(at::kSub, kind);
}

// ===========================================================================
// Step 1: the command kinds
// ===========================================================================

// original 0x42F510 (PSX 0x801D3048): kinds 0 and 2. Battle_ClearActingFlags;
// then for kind 2 (0x904AA2) by an enemy (0x904B34 of 3 or more) the enemy's
// name record goes to Text_Records[0] and a kind-2 banner shows system
// message 0x7F. The phase stays 3 with step 0 and 0x904AA2 0: the next actor.
//
// As the original has it: both bytes are read after Battle_ClearActingFlags.
extern "C" void __cdecl BattleAction_KindPlain(void) {
    g.clear_acting_flags();
    if (B(at::kSub) == 2 && B(at::kActor) >= 3) {
        EnemyNameToText0(Enemy(B(at::kActor)) + 0x80);
        SystemBanner(0x7F);
    }
    SetB(at::kPhase, 3);
    SetB(at::kStep, 0);
    SetB(at::kSub, 0);
}

// original 0x42F5B0 (PSX 0x801D3160): kind 1. The actor's pending bit
// (Battle_SetActorBit), its object's state byte +1 becomes 4, and the action
// goes to step 2 (the effects) with 0x904AA2 0.
//
// As the original has it: the object pointer 0x904B3C is read after the call.
extern "C" void __cdecl BattleAction_KindOne(void) {
    g.set_actor_bit(B(at::kActor));
    PtrAt(at::kActor + at::kBObject)[1] = 4;
    SetB(at::kPhase, 3);
    SetB(at::kStep, 2);
    SetB(at::kSub, 0);
}

// ===========================================================================
// Kind 4: the ability
// ===========================================================================

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
    SetWord(At(at::kAbility), Word(PtrAt(at::kActor + at::kBAction) + 2));
    if (g.action_suits_target() != 0) {
        g.remove_from_turn_order(B(at::kActor));
        const unsigned a = B(at::kActor);
        SetB(at::kPhase, 3);
        unsigned char* const o = Party(a);
        o[1] = 2;
        o[2] = 0;
        SetB(at::kStep, 0);
        SetB(at::kSub, 0);
        return;
    }
    if (Word(At(at::kAbility)) != 0x97) {
        const unsigned id = Word(At(at::kAbility));
        SetB(at::kBannerShown, 1);
        g.str_copy_n(reinterpret_cast<char*>(At(at::kNameBuf)), reinterpret_cast<const char*>(At(at::kAbilityRecords + id * 24)), 0x10);
        g.banner_add(1, 1, 0, 0x3C, reinterpret_cast<const char*>(At(at::kNameBuf)));
    }
    const unsigned a = B(at::kActor);
    const Standing s = ActorStanding(a);
    if ((s.status & 0x10) != 0 && (At(at::kAbilityRecords + Word(At(at::kAbility)) * 24u + 0x15)[0] & 4) != 0) {
        SystemBanner(0x37);
        SetB(at::kStep, 0);
        SetB(at::kSub, 0);
        return;
    }
    unsigned char cost;
    if (Word(At(at::kAbility)) != 0x97) {
        const unsigned id = B(at::kAbility);
        const unsigned who = a <= 2 ? Party(a)[5] : Enemy(a)[5];
        cost = g.skill_ap_cost(who, id, 1);
    } else {
        cost = B(at::kApCost);
    }
    SetB(at::kCostShown, cost);
    if (cost > (s.ap & 0xFFFF)) {
        SystemBanner(0x36);
        SetB(at::kStep, 0);
        SetB(at::kSub, 0);
        return;
    }
    SetB(at::kCounter, 0);
    if (g.pick_flag8_member() == 0) SetB(at::kRoundFlags, B(at::kRoundFlags) | 0x20);
    SetB(at::kSub2, B(at::kSub2) + 1);
}

// original 0x42F880 (PSX 0x801D360C): waits for 0x904AA8 bit 5. Then clears
// it, the actor object's +1 = 7 and +2 = 0, the actor's pending bit,
// 0x904B8D = the id's low byte; for the ids 0x24, 0x25 and 0x8C the target
// is 0x42F9D0's answer (it picks the id again) and fills the target block -
// a member's for 0..2, an enemy's for anything above; then the magic file
// (Magic_LoadForAbility, the id's low byte re-read) and 0x904AA3 counts one.
//
// As the original has it: 0x904B3C is re-read for the second store; the id
// is read after Battle_SetActorBit and again after 0x42F9D0; the target
// block has no upper bound here (Battle_BeginAction's stops at 10), so a
// side's 0x40 / 0x80 / 0xC0 is taken as an enemy 0x3D.. far past the eight
// (docs/battle_actions.md section 3).
extern "C" void __cdecl BattleAction_AbilityCommit(void) {
    if ((B(at::kRoundFlags) & 0x20) == 0) return;
    unsigned char* const o = PtrAt(at::kActor + at::kBObject);
    SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & 0xFFDFu);
    o[1] = 7;
    PtrAt(at::kActor + at::kBObject)[2] = 0;
    g.set_actor_bit(B(at::kActor));
    SetB(at::kAbilityShown, B(at::kAbility));
    const unsigned id = Word(At(at::kAbility));
    if (id == 0x24 || id == 0x25 || id == 0x8C) {
        const unsigned t = g.pick_target();
        SetB(at::kTarget, t);
        if (t <= 2) SetMemberBlock(at::kTarget, t);
        else SetEnemyBlock(at::kTarget, t);
    }
    g.load_for_ability(B(at::kAbility));
    SetB(at::kSub2, B(at::kSub2) + 1);
}

// original 0x42FAB0 (PSX 0x801D399C): once File_LoadDone answers non-zero,
// Battle_StartAbilityMagic(the action record's +2 byte, the actor object)
// and the action goes to step 2 with both sub-steps 0.
extern "C" void __cdecl BattleAction_AbilityStart(void) {
    if (g.file_load_done() == 0) return;
    const unsigned id = PtrAt(at::kActor + at::kBAction)[2];
    const unsigned owner = static_cast<std::uint32_t>(L(at::kActor + at::kBObject));
    g.start_ability_magic(id, owner);
    SetB(at::kPhase, 3);
    SetB(at::kStep, 2);
    SetB(at::kSub, 0);
    SetB(at::kSub2, 0);
}

// ===========================================================================
// Kind 5: the item
// ===========================================================================

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
    SetWord(At(at::kAbility), Word(PtrAt(at::kActor + at::kBAction) + 2));
    if (g.item_suits_target() != 0) {
        g.return_queued_item(B(at::kActor));
        g.remove_from_turn_order(B(at::kActor));
        const unsigned a = B(at::kActor);
        SetB(at::kPhase, 3);
        SetB(at::kStep, 0);
        SetB(at::kSub, 0);
        unsigned char* const o = Party(a);
        o[1] = 2;
        o[2] = 0;
        return;
    }
    const unsigned char* const r = PtrAt(at::kActor + at::kBAction);
    const unsigned item = r[2], category = r[3];
    const unsigned char* const name = g.item_name(category, item);
    g.str_copy_n(reinterpret_cast<char*>(At(at::kNameBuf)), reinterpret_cast<const char*>(name), 0x10);
    g.banner_add(1, 1, 0, 0x1E, reinterpret_cast<const char*>(At(at::kNameBuf)));
    const unsigned sub2 = B(at::kSub2);
    SetB(at::kCounter, 0);
    SetB(at::kBannerShown, 1);
    SetB(at::kSub2, sub2 + 1);
}

// original 0x42FBD0 (PSX 0x801D3B94): the actor object's +1 = 0xC, the
// actor's pending bit, the item's magic file (Magic_LoadForItem of the
// record's u16 +2, read after the bit), and 0x904AA3 counts one.
extern "C" void __cdecl BattleAction_ItemCommit(void) {
    PtrAt(at::kActor + at::kBObject)[1] = 0xC;
    g.set_actor_bit(B(at::kActor));
    g.load_for_item(Word(PtrAt(at::kActor + at::kBAction) + 2));
    SetB(at::kSub2, B(at::kSub2) + 1);
}

// original 0x42FC10 (PSX 0x801D3BFC): once File_LoadDone answers non-zero,
// Battle_StartItemMagic(the record's u16 +2, the actor object) and step 2
// with both sub-steps 0.
extern "C" void __cdecl BattleAction_ItemStart(void) {
    if (g.file_load_done() == 0) return;
    const unsigned id = Word(PtrAt(at::kActor + at::kBAction) + 2);
    const unsigned owner = static_cast<std::uint32_t>(L(at::kActor + at::kBObject));
    g.start_item_magic(id, owner);
    SetB(at::kPhase, 3);
    SetB(at::kStep, 2);
    SetB(at::kSub, 0);
    SetB(at::kSub2, 0);
}

// ===========================================================================
// Step 2: the effects
// ===========================================================================

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
        if ((o[0x130] & 0x40) == 0 || o[1] == 6) continue;
        o[1] = 6;
        o[2] = 0;
        o[3] = 0;
        o[4] = 0;
        g.set_actor_bit(a);
    }
    for (unsigned a = 3; a <= 10; ++a) {
        unsigned char* const o = Enemy(a);
        if ((o[0x110] & 0x40) == 0 || o[1] == 6) continue;
        o[1] = 6;
        o[2] = 0;
        o[3] = 0;
        o[4] = 0;
        g.set_actor_bit(a);
    }
    if (g.any_flag_f0() != 0) return;
    if (Word(At(at::kPending)) != 0) return;
    if ((B(at::kRoundFlags) & 4) == 0) return;
    SetB(at::kSub, B(at::kSub) + 1);
}

// original 0x42FD20 (PSX 0x801D3E60): for kind 4, the ability row of the id
// 0x904B80 (by the whole u16), for kind 5 the item row of the record's u16
// +2 (battle_flow.md's ItemRow); when that row's magic file is not 0xFFFF,
// LoadDatFile(0xD0). Any kind: 0x904AA2 counts one.
//
// As the original has it: the ability row is indexed by the whole word
// (Magic_LoadForAbility uses its low byte); neither index is checked.
extern "C" void __cdecl BattleAction_EffectPreload(void) {
    const unsigned kind = B(at::kActor + at::kBKind);
    bool has_file = false;
    if (kind == 4) {
        const unsigned row = At(at::kAbilityRows + Word(At(at::kAbility)))[0];
        has_file = Word(At(at::kMagicFiles + row * 8)) != 0xFFFF;
    } else if (kind == 5) {
        const unsigned w = Word(PtrAt(at::kActor + at::kBAction) + 2);
        const auto* const tables = reinterpret_cast<const unsigned char* const*>(static_cast<std::uintptr_t>(at::kItemRows));
        const unsigned row = tables[w >> 8][w & 0xFF];
        has_file = Word(At(at::kMagicFiles + row * 8)) != 0xFFFF;
    }
    if (has_file) g.load_dat(0xD0);
    SetB(at::kSub, B(at::kSub) + 1);
}

// original 0x42FD90 (PSX 0x801D3F4C): once File_LoadDone answers non-zero:
// in an event battle the hook with 4; 0x904AA8 loses 0x400; step 3 with
// 0x904AA2 0; Gfx_ClutStripCopyRow(0x1A).
//
// As the original has it: 0x904AAA is read after File_LoadDone; the stores
// come before the row copy.
extern "C" void __cdecl BattleAction_EffectLoaded(void) {
    if (g.file_load_done() == 0) return;
    if (B(at::kEventBattle) != 0) EventHook(4);
    SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & 0xFBFFu);
    SetB(at::kStep, 3);
    SetB(at::kSub, 0);
    g.clut_copy_row(0x1A);
}

// ===========================================================================
// Step 3: after the effects
// ===========================================================================

// original 0x42FDE0 (PSX 0x801D4008): with 0x904AA8 bit 2 and no actor
// pending: Battle_SettleFlag8 non-zero gives 0x904AA5 = 0 and 0x904AA2 + 1;
// zero gives 0x904AA2 = 2 (past 0x42FE20 to the enemy messages).
extern "C" void __cdecl BattleAction_AfterSettle(void) {
    if ((B(at::kRoundFlags) & 4) == 0) return;
    if (Word(At(at::kPending)) != 0) return;
    if (g.settle_flag8() != 0) {
        const unsigned sub = B(at::kSub);
        SetB(at::kCounter, 0);
        SetB(at::kSub, sub + 1);
        return;
    }
    SetB(at::kSub, 2);
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
    if (B(at::kMsgBusy) != 0) return;
    const unsigned n = B(at::kMsgCount);
    if (n == 0) {
        SetB(at::kStep, 4);
        SetB(at::kSub, 0);
        return;
    }
    const unsigned e = B(at::kMsgList + n * 4);
    EnemyNameToText0(At(at::kEnemy + e * at::kEnemyStride + 0x80));
    const unsigned id = Word(At(at::kMsgList + n * 4 + 2));
    const unsigned char* const text = g.msg(id);
    g.queue_push(1, 0, static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(text)));
    SetB(at::kMsgCount, B(at::kMsgCount) - 1);
}

// ===========================================================================
// Step 4: the action's end
// ===========================================================================

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
extern "C" void __cdecl BattleAction_End(void) {
    if ((B(at::kRoundFlags) & 4) == 0) return;
    if (Word(At(at::kPending)) != 0) return;
    const unsigned char event = B(at::kEventBattle);
    SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & 0xD7FBu);
    SetB(at::kStep, 0);
    SetB(at::kSub, 0);
    if (event != 0) EventHook(0);
    if (B(at::kBattleEnd) != 0) {
        SetB(at::kPhase, 4);
        SetB(at::kStep, 2);
        return;
    }
    g.clear_acting_flags();
    if ((L(at::kRoundFlags) & 0x1000) != 0) SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & 0xEFFFu);
    if ((B(at::kRoundFlags) & 0x40) != 0) {
        unsigned char actor[0x10], target[0x10];
        std::memcpy(actor, At(at::kActor), sizeof actor);
        std::memcpy(target, At(at::kTarget), sizeof target);
        std::memcpy(At(at::kActor), target, sizeof target);
        std::memcpy(At(at::kTarget), actor, sizeof actor);
        SetB(at::kActor + at::kBKind, 1);
        const unsigned a = B(at::kActor);
        const std::uint32_t flags = static_cast<std::uint32_t>(Long(a <= 2 ? Party(a) + 0x130 : Enemy(a) + 0x110));
        CopyStats(a);
        const std::uint32_t text = static_cast<std::uint32_t>(L((flags & 0x8000) != 0 ? at::kTextCounter : at::kTextMiss));
        g.banner_add(1, 0, 0, 0xF, reinterpret_cast<const char*>(static_cast<std::uintptr_t>(text)));
        SetB(at::kBannerShown, 1);
        SetB(at::kStep, 0);
        SetB(at::kSub, 1);
    }
    if (B(at::kEventBattle) != 0) EventHook(5);
}

// ===========================================================================

void BattleActions_Inject() {
    if (bof3::WantsShadow("battle_actions")) battle_actions::SelfTest();
    BOF3_INJECT(Battle_ActionPhase);
    BOF3_INJECT(BattleAction_BeginStep);
    BOF3_INJECT(Battle_BeginAction);
    BOF3_INJECT(BattleAction_EnterKind);
    BOF3_INJECT(BattleAction_KindStep);
    BOF3_INJECT(BattleAction_KindPlain);
    BOF3_INJECT(BattleAction_KindOne);
    BOF3_INJECT(BattleAction_AbilityStep);
    BOF3_INJECT(BattleAction_AbilityCheck);
    BOF3_INJECT(BattleAction_AbilityCommit);
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
    BOF3_INJECT(BattleAction_EnemyMessages);
    BOF3_INJECT(BattleAction_End);
}
