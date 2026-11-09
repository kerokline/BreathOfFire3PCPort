// Moved here by tools/move_funcs.py from battle_e3.cpp, battle_obj_states.cpp, battle_windows.cpp (docs/source-layout-round.md).
#include "game/battle/battle_obj.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e3.h"
#include "game/battle_e3_callees.h"
#include "game/battle_obj_states.h"
#include "game/battle_obj_states_callees.h"
#include "game/battle_windows.h"
#include "game/battle_windows_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/text_pairs.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e3::at;

}  // namespace

using namespace battle_obj_states;

using namespace battle_windows;

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

U Key(const void* p) { return Low32(p); }

unsigned char* PtrAt(U cell) { return move_script::At(static_cast<U>(move_script::Long(move_script::At(cell)))); }

void SetPtrAt(U cell, const void* p) { SetLong(move_script::At(cell), static_cast<std::int32_t>(Key(p))); }

unsigned char* S() { return Sprite_Current; }

unsigned char* F() { return Field_State; }

U ULong(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

void SetULong(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// The ability record `id` (24 bytes at 0x65C4D8): a flag byte, or the word +4.
unsigned AbilityId() { return move_script::Word(move_script::At(::bof3::Img(battle_e3::at::kAbility.v))); }

unsigned char AbilityByte(U base) { return move_script::At(base + AbilityId() * battle_e3::at::kAbilityStride)[0]; }

unsigned AbilityWord4() { return move_script::Word(move_script::At(::bof3::Img(battle_e3::at::kAbilityWord4.v) + AbilityId() * battle_e3::at::kAbilityStride)); }

// CharacterRecords' record for a member's character byte +0x148 (unchecked, as
// the original: a character byte, not a table index of this group's).
unsigned char* CharRecord(unsigned character) { return move_script::At(::bof3::Img(CharacterRecords_addr) + character * battle_e3::at::kCharStride); }

unsigned char* Member(unsigned i) { return move_script::At(::bof3::Img(ObjTrio_addr) + i * battle_e3::at::kPartyStride); }

U EntryBattleE3(U table, unsigned index) { return ULong(move_script::At(table + 4 * index)); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, as read
// (the fuzz swaps the cells for recorders); a Fatal past them (the original
// jumps through the dword after, another table's entry or data).
unsigned CheckedState(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e3.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return state;
}

// The party objects' dispatchers: the same jmp, with nothing forwarded -
// BattleObj_RunState's entries read no word, and their eax reaches only
// callers that drop it (battle_obj_states.md section 2).
void Run(const char* who, U table, unsigned entries, unsigned at) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(EntryBattleE3(table, CheckedState(who, table, entries, at))))();
}

// BattleTask_Create's slot: a Fatal on 0xFF (all 48 taken), where the original
// writes at 0x93A000 + 0xFF * 0x84, past the pool (docs/battle_e3.md section 6).
unsigned char* TaskSlot(const char* who, unsigned n) {
    n &= 0xFF;
    if (n >= battle_e3::at::kTaskCount)
        bof3::Fatal("%s: BattleTask_Create answered slot %u (all 48 taken) - the original writes at 0x93A000 + %u * 0x84, "
                    "past the pool (docs/battle_e3.md section 6)",
                    who, n, n);
    return move_script::At(::bof3::Img(BattleTask_Slots_addr) + n * battle_e3::at::kTaskStride);
}

// The party count 0x904AB0 as the loops over ObjTrio read it: a Fatal past its
// three members, where the original walks on into the window records.
unsigned PartyCount(const char* who) {
    const unsigned n = B(::bof3::Img(battle_e3::at::kPartyCount.v));
    if (n > battle_e3::at::kPartyMax)
        bof3::Fatal("%s: the party count 0x904AB0 is %u, past ObjTrio's three members - the original walks on into "
                    "WindowRecords (docs/battle_e3.md section 6)",
                    who, n);
    return n;
}

// The two moves the objects make: +0x34 / +0x38 by the velocity +0xC / +0x10,
// forward or back.
void MoveBy(unsigned char* s, bool back) {
    const U x = ULong(s + 0x34), vx = ULong(s + 0xC);
    SetULong(s + 0x34, back ? x - vx : x + vx);
    const U y = ULong(s + 0x38), vy = ULong(s + 0x10);
    SetULong(s + 0x38, back ? y - vy : y + vy);
}

// 0x904AA8 bit 14 cleared when 0x904B8A is the object's actor (the fall's two
// sites; the object read after the flags, as the original).
void ClearFormFlag() {
    const bool bit = (Word(move_script::At(::bof3::Img(battle_e3::at::kRoundFlags.v))) & 0x4000) != 0;
    unsigned char* const s = S();
    if (bit && BattleFlow_ActorAt == s[5]) SetWord(move_script::At(::bof3::Img(battle_e3::at::kRoundFlags.v)), Word(move_script::At(::bof3::Img(battle_e3::at::kRoundFlags.v))) & 0xBFFF);
}

}  // namespace

namespace battle_obj_states {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

}  // namespace battle_obj_states

namespace {

unsigned char* Task(unsigned index) { return battle_obj_states::At(::bof3::Img(BattleTask_Slots_addr) + index * battle_obj_states::at::kTaskSize); }

// The party objects' states run on the member: Sprite_Current and Field_State are its record.
// A code pointer out of one of the tables, called; the index is not checked
// (a byte, as the original's).
void Dispatch(const std::uint32_t* table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

void Pose(unsigned add) { battle_obj_states::g.ensure_animation(static_cast<unsigned char>(rv::As<records::PartyMember>(S())->direction + add)); }

}  // namespace

namespace battle_windows {

using move_script::At;

}  // namespace battle_windows

namespace {

std::uint32_t Addr(const void* p) { return Low32(p); }

unsigned char* FieldState() { return Field_State; }

unsigned char* Sprite() { return Sprite_Current; }

// A party member's battle object (ObjTrio, stride 0x14C) and its words.
unsigned char* MemberBattleWindows(unsigned i) { return battle_windows::At(::bof3::Img(ObjTrio_addr) + battle_windows::at::kObjectStride * i); }

}  // namespace

// original 0x441100: each of the three party objects 0x802D40 + 0x14C i made
// Field_State and Sprite_Current, then its state step (the third a tail jump,
// so the answer is its eax).
extern "C" unsigned __cdecl BattleParty_RunStates() {
    for (unsigned i = 0; i < 2; ++i) {
        Field_State = MemberBattleWindows(i);
        Sprite_Current = MemberBattleWindows(i);
        battle_windows::g.run_state();
    }
    Field_State = MemberBattleWindows(2);
    Sprite_Current = MemberBattleWindows(2);
    return battle_windows::g.run_state();
}

// original 0x441140: each party object made Field_State and Sprite_Current,
// and its screen slot updated when +0 has bit 0 and not bit 6.
extern "C" void __cdecl BattleParty_UpdateScreens() {
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const o = MemberBattleWindows(i);
        auto* const member_rec = rv::As<records::PartyMember>(o);
        Field_State = o;
        Sprite_Current = o;
        const unsigned char b = member_rec->flags;
        if ((b & 1) && !(b & 0x40)) battle_windows::g.update_screen();
    }
}

// original 0x441180 (PSX 0x801DEDA0): Sprite_ScriptTick for the current
// object unless Field_State +0x90 has bit 2, or the gate 0x904B8E is set and
// +0x134 lacks bit 4; 1 when it does not tick.
extern "C" unsigned char __cdecl BattleObj_ScriptTick() {
    unsigned char* const f = FieldState();
    if (f[0x90] & 4) return 1;
    if (BattleFlow_AnimGate != 0 && !(f[0x134] & 0x10)) return 1;
    return static_cast<unsigned char>(battle_windows::g.script_tick());
}

// original 0x4411B0 (PSX 0x801DEE08): the same gate before
// Sprite_ScriptTickOnce.
extern "C" unsigned char __cdecl BattleObj_ScriptTickOnce() {
    unsigned char* const f = FieldState();
    if (f[0x90] & 4) return 1;
    if (BattleFlow_AnimGate != 0 && !(f[0x134] & 0x10)) return 1;
    return static_cast<unsigned char>(battle_windows::g.script_tick_once());
}

// original 0x4411E0 (PSX 0x801DEE70): the current object's state handler -
// when bit 0 of Sprite_Current +0 is set, tail-jump through the 27-entry
// table 0x64DFE0 by the state byte +1 (unbounded, as the PlayStation's), else
// return with eax still Sprite_Current.
extern "C" unsigned __cdecl BattleObj_RunState() {
    unsigned char* const s = Sprite();
    if ((s[0] & 1) == 0) return Addr(s);
    using Handler = unsigned (__cdecl*)();
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(battle_windows::g.state_table[s[1]]))();
}

// original 0x441200 (PSX 0x801DEEC8, paired as Battle_InitMemberActor): the
// dwords +0xC..+0x20 zeroed, +0x29 = 4, the pose base +8 from 0x904AAC,
// +0x48 = 0, the ground height +0x3E = AreaMap_Elevation(+0x34, +0x38),
// +0x2B = 0, then the state 2 with +2..+4 zeroed.
extern "C" void __cdecl BattleObj_StateInit(void) {
    for (unsigned o = 0xC; o <= 0x20; o += 4) SetLong(S() + o, 0);
    rv::As<records::PartyMember>(S())->ot_slot = 4;
    rv::As<records::PartyMember>(S())->direction = battle_obj_states::At(::bof3::Img(battle_obj_states::at::kPoseBase.v))[0];
    S()[0x48] = 0;
    auto* const member_s = rv::As<records::PartyMember>(S());
    const long h = battle_obj_states::g.elevation(member_s->x, member_s->z);
    rv::Store<unsigned>(rv::As<records::PartyMember>(S())->height_whole, static_cast<unsigned>(h));
    S()[0x2B] = 0;
    rv::As<records::PartyMember>(S())->state = 2;
    rv::As<records::PartyMember>(S())->step = 0;
    rv::As<records::PartyMember>(S())->state3 = 0;
    rv::As<records::PartyMember>(S())->state4 = 0;
}

// original 0x4412B0 (PSX 0x801DEFA0): the pose a party object stands in.
// Under battle kind 5 with 0x904AE8 bit 1, a pose from the set 0x8C5D80
// (0x589110): +8 + 0x1C when Field_State +0x91 has bit 6, else +0x4B's pose
// (0x904AE5 bit 5 set) or +8 + 0x20. Otherwise Sprite_EnsureAnimation of +8
// plus: 0x1C (+0x91 bit 6), 0x30 (+0x91 bit 3); under 0x904AA8 bit 4, 4
// (+0x90 bit 2), 0x18 (any of +0x90's 0x20C0) or 8; else 0x14 (+0x130 bit 1)
// or by the action +0x125: 0 -> 0x18 / 4 as before, 1 -> 8, 2 -> 0x14,
// 4 -> 0x38 (+0x134 bit 2) or, by bit 3 of the skill +0x126's flags
// (0x65C4DD + 24 skill), 8 or 0x28, 5 -> 4; 3 and above 5 change nothing and
// answer the action. The answer is the callee's eax.
extern "C" unsigned __cdecl BattleObj_PickPose() {
    const bool own_poses = battle_windows::At(::bof3::Img(battle_windows::at::kBattleKind.v))[0] == 5 && (Battle_EndFlags & 2) != 0;
    if (own_poses) {
        const bool high = (EventBattle_Flags & 0x20) != 0;
        if (FieldState()[0x91] & 0x40)
            return battle_windows::g.pose_from(static_cast<unsigned char>(Sprite()[8] + 0x1C), ::bof3::Img(battle_windows::at::kPoseSet.v), 0x1800);
        if (high) return battle_windows::g.pose_from(Sprite()[0x4B], ::bof3::Img(battle_windows::at::kPoseSet.v), 0x1800);
        return battle_windows::g.pose_from(static_cast<unsigned char>(Sprite()[8] + 0x20), ::bof3::Img(battle_windows::at::kPoseSet.v), 0x1800);
    }
    unsigned char* const f = FieldState();
    auto* const member_rec = rv::As<records::PartyMember>(f);
    const unsigned flags = member_rec->status;
    const auto pose = [](unsigned add) { return battle_windows::g.ensure_pose(static_cast<unsigned char>(Sprite()[8] + add)); };
    if (flags & 0x4000) return pose(0x1C);
    if (flags & 0x0800) return pose(0x30);
    if (battle_windows::At(::bof3::Img(battle_windows::at::kPoseMode.v))[0] & 0x10) {
        if (flags & 4) return pose(4);
        if (flags & 0x20C0) return pose(0x18);
        return pose(8);
    }
    if (f[0x130] & 2) return pose(0x14);
    const unsigned action = f[0x125];
    switch (action) {
    case 0: return pose((flags & 0x20C0) ? 0x18 : 4);
    case 1: return pose(8);
    case 2: return pose(0x14);
    case 4: {
        if (f[0x134] & 4) return pose(0x38);
        const unsigned skill = rv::As<records::PartyMember>(f)->queued_id;  // f: Field_State, the member
        return pose((battle_windows::At(::bof3::Img(battle_windows::at::kSkillFlags.v) + skill * 24)[0] & 8) ? 8 : 0x28);
    }
    case 5: return pose(4);
    default: return action;   // 3, and anything past the table's six
    }
}

// original 0x441510 (PSX 0x801DF300; in no start list; after BattleObj_PickPose's switch
// table): Sprite_EnsureAnimation(+8 + 0x34) with Field_State +0x91 bit 3, else
// (+8 + 0x10). Called by BattleObj_HitReceive and BattleObj_State11.
extern "C" void __cdecl BattleObj_HitPose(void) {
    if (F()[0x91] & 8)
        BH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(rv::As<records::PartyMember>(S())->direction + 0x34));
    else
        BH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(rv::As<records::PartyMember>(S())->direction + 0x10));
}

// original 0x441550 (PSX 0x801DF36C): the standing pose (BattleObj_PickPose),
// one script tick, then the next state.
extern "C" void __cdecl BattleObj_StateStand(void) {
    battle_obj_states::g.pick_pose();
    battle_obj_states::g.tick();
    rv::As<records::PartyMember>(S())->state = static_cast<unsigned char>(rv::As<records::PartyMember>(S())->state + 1);
}

// original 0x441570 (PSX 0x801DF3AC): standing, each frame.
//   1. The sub-state +2's handler through 0x64E014 (BattleObj_IdleTintOn /
//      BattleObj_IdleTintOff).
//   2. The colour +0x5F, +0x5E, +0x5D zeroed; in phase 1 with 0x904AA1 above
//      1 it is 0xE2 each when Field_State's character +0x89 is not that of
//      the member 0x904AAE, else 0x78.
//   3. Unless the phase is 5: while Field_State +0x90 has bit 5, the byte +9
//      of battle-task slot +5 steps by one - up while +0x130 has bit 11, to
//      12, else down, to -12 - and at either end bit 11 flips; then the slot's
//      +0x38 (poses 1 and 3: +0x34) plus that s8 << 9 becomes the object's
//      +0x38 (+0x34).
//      In phase 5: +0x34 / +0x38 from 0x903780 / 0x903784 when 0x904AA8 has
//      bit 15 and +0x134 bit 1, else from the slot's +0x34 / +0x38.
//   4. BattleObj_ScriptTick (a tail jump) unless +0x130 has bit 12.
//
// As the original has it: the slot index +5 and the character are not
// checked; phase 1 with 0x904AA1 at 0 or 1 skips the phase-5 test (it cannot
// be 5); bit 5 of +0x90 clear skips the step and the position alike, but not
// the tick.
extern "C" void __cdecl BattleObj_StateIdle(void) {
    Dispatch(battle_obj_states::g.idle_subs, rv::As<records::PartyMember>(S())->step);
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    bool phase5 = false;
    if (battle_obj_states::At(::bof3::Img(battle_obj_states::at::kPhase.v))[0] == 1) {
        if (BattleActions_Step > 1) {
            const unsigned member = BattleE4_AutoMember;
            const unsigned char c = rv::As<records::PartyMember>(F())->char_id == battle_obj_states::At(::bof3::Img(ObjTrio_addr) + member * battle_obj_states::at::kMemberSize + 0x89)[0] ? 0x78 : 0xE2;
            S()[0x5F] = c;
            S()[0x5E] = c;
            S()[0x5D] = c;
            phase5 = battle_obj_states::At(::bof3::Img(battle_obj_states::at::kPhase.v))[0] == 5;
        }
    } else {
        phase5 = battle_obj_states::At(::bof3::Img(battle_obj_states::at::kPhase.v))[0] == 5;
    }
    unsigned char* const f = F();
    auto* const member_f = rv::As<records::PartyMember>(f);
    if (phase5) {
        if ((Long(battle_obj_states::At(::bof3::Img(battle_obj_states::at::kRoundFlags.v))) & 0x8000) != 0 && (F()[0x134] & 2) != 0) {
            rv::Store<std::int32_t>(rv::As<records::PartyMember>(S())->x, static_cast<std::int32_t>(BossSi_CentreX));
            rv::Store<std::int32_t>(rv::As<records::PartyMember>(S())->z, static_cast<std::int32_t>(BossSi_CentreZ));
        } else {
            rv::As<records::PartyMember>(S())->x = rv::As<records::BattleTask>(Task(rv::As<records::PartyMember>(S())->actor))->x;
            rv::Store<std::int32_t>(rv::As<records::PartyMember>(S())->z, rv::As<records::BattleTask>(Task(rv::As<records::PartyMember>(S())->actor))->z);
        }
    } else if ((f[0x90] & 0x20) != 0) {
        unsigned char* const s = S();
        auto* const member_s = rv::As<records::PartyMember>(s);
        bool flip;
        if ((static_cast<std::int32_t>(member_f->flags_130) & 0x800) != 0) {
            Task(member_s->actor)[9] = static_cast<unsigned char>(Task(member_s->actor)[9] + 1);
            flip = static_cast<signed char>(Task(member_s->actor)[9]) >= 12;
        } else {
            Task(member_s->actor)[9] = static_cast<unsigned char>(Task(member_s->actor)[9] - 1);
            flip = static_cast<signed char>(Task(member_s->actor)[9]) <= -12;
        }
        records::PartyMember* member_p = rv::As<records::PartyMember>(s);
        if (flip) {
            member_f->flags_130 = static_cast<std::int32_t>(member_f->flags_130) ^ 0x800;
            member_p = rv::As<records::PartyMember>(S());
        }
        const unsigned pose = member_p->direction;
        unsigned char* const t = Task(member_p->actor);
        auto* const task_rec = rv::As<records::BattleTask>(t);
        const std::int32_t step = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<signed char>(t[9])) << 9);
        if (pose == 1 || pose == 3) member_p->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(step) + static_cast<std::uint32_t>(task_rec->x));
        else member_p->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(step) + static_cast<std::uint32_t>(task_rec->z));
    } else {
        if ((static_cast<std::int32_t>(member_f->flags_130) & 0x1000) == 0) battle_obj_states::g.tick();
        return;
    }
    if ((static_cast<std::int32_t>(rv::As<records::PartyMember>(F())->flags_130) & 0x1000) == 0) battle_obj_states::g.tick();
}

// original 0x4417A0 (PSX 0x801DF71C), state 3's sub-state 0 (also slot 13 of
// 0x64DFE0): while 0x904AAF is set and the target byte (*0x939FA0) is this
// object's actor +5 or has bit 7 (all), the object is tinted -
// Sprite_SetTint(Sprite_Current, 0, 0, 0, 0) into +7 - and the sub-state
// becomes 1.
extern "C" void __cdecl BattleObj_IdleTintOn(void) {
    if (BattleE4_Picking == 0) return;
    unsigned char* const s = S();
    auto* const member_s = rv::As<records::PartyMember>(s);
    const unsigned char target = battle_obj_states::At(BattleActorCopies_Command)[0];
    if (target != member_s->actor && (target & 0x80) == 0) return;
    const unsigned char r = battle_obj_states::g.set_tint(s, 0, 0, 0, 0);
    S()[7] = r;
    rv::As<records::PartyMember>(S())->step = 1;
}

// original 0x4417F0 (PSX 0x801DF7A0), state 3's sub-state 1 (also slot 14):
// the tint record +7's bytes +4, +3, +2 (MoveScript_TintRecords, 12 each) set
// to 0x904AC8; unless 0x904AAF is still set and the target byte still this
// actor or all, Tint_Release(+7) and the sub-state back to 0.
//
// As the original has it: the record index is not checked, and is read again
// for each of the three stores.
extern "C" void __cdecl BattleObj_IdleTintOff(void) {
    unsigned char* const s = S();
    auto* const member_s = rv::As<records::PartyMember>(s);
    const unsigned char level = battle_obj_states::At(::bof3::Img(battle_obj_states::at::kTintLevel.v))[0];
    battle_obj_states::At(::bof3::Img(MoveScript_TintRecords_addr) + 4 + s[7] * 12u)[0] = level;
    battle_obj_states::At(::bof3::Img(MoveScript_TintRecords_addr) + 3 + s[7] * 12u)[0] = level;
    battle_obj_states::At(::bof3::Img(MoveScript_TintRecords_addr) + 2 + s[7] * 12u)[0] = level;
    if (BattleE4_Picking != 0) {
        const unsigned char target = battle_obj_states::At(BattleActorCopies_Command)[0];
        if (target == member_s->actor) return;
        if (target & 0x80) return;
    }
    battle_obj_states::g.tint_release(s[7]);
    rv::As<records::PartyMember>(S())->step = 0;
}

// original 0x441860 (PSX 0x801DF860, paired as Attack_DispatchByCharacter): a
// tail jump through the dword 0x64E048 while Field_State +0x134 has bit 0,
// else through 0x64E01C by the character +0x89 (slots 15.. of 0x64DFE0 -
// 0x441890 for the first eleven, 0x442E40 for the twelfth).
//
// As the original has it: the character is not checked.
extern "C" void __cdecl BattleObj_StateAttack(void) {
    unsigned char* const f = F();
    auto* const member_f = rv::As<records::PartyMember>(f);
    if (f[0x134] & 1) {
        Dispatch(battle_obj_states::g.attack_special, 0);
        return;
    }
    Dispatch(battle_obj_states::g.attack_by_char, member_f->char_id);
}

// original 0x441890 (PSX 0x801DF8C8), the attack's first frame for each of the
// first eleven characters (slots 15..25 of 0x64DFE0): the counters +9 and
// +0xA from 0x64E058 by 0x904B89 while Field_State +0x134 has bit 1, else
// from 0x64E04C by the character; the pose +8 + 0xC, a script tick, the next
// state.
//
// As the original has it: both indices are unchecked bytes, and each read
// again for the second counter.
extern "C" void __cdecl BattleObj_AttackStart(void) {
    if (F()[0x134] & 2) {
        rv::As<records::PartyMember>(S())->step_frames = battle_obj_states::At(::bof3::Img(battle_obj_states::at::kAttackCountBy.v) + BattlePhases_B89)[0];
        S()[0xA] = battle_obj_states::At(::bof3::Img(battle_obj_states::at::kAttackCountBy.v) + BattlePhases_B89)[0];
    } else {
        rv::As<records::PartyMember>(S())->step_frames = battle_obj_states::At(::bof3::Img(battle_obj_states::at::kAttackCount.v) + rv::As<records::PartyMember>(F())->char_id)[0];
        S()[0xA] = battle_obj_states::At(::bof3::Img(battle_obj_states::at::kAttackCount.v) + rv::As<records::PartyMember>(F())->char_id)[0];
    }
    Pose(0xC);
    battle_obj_states::g.tick();
    rv::As<records::PartyMember>(S())->state = static_cast<unsigned char>(rv::As<records::PartyMember>(S())->state + 1);
}

// original 0x441930 (PSX 0x801DF9D0): a tail jump through 0x64E074 by the
// sub-state +2 (0x441950, 0x4419D0), unchecked.
extern "C" void __cdecl BattleObj_StateSwing(void) { Dispatch(battle_obj_states::g.swing_subs, rv::As<records::PartyMember>(S())->step); }

// original 0x441950 (PSX 0x801DFA14, paired as Battle_SwingCue_Step): one
// script tick; the counter +9 down by one; at 0 the cue: 3 when
// Battle_RollPendingFlag answers non-zero, else Rand() % 100 against
// Field_State +0xBA - below it 0x904AA8 |= 0x80 and cue 3, else cue 2; then
// cue 4, and the next sub-state.
//
// As the original has it: the remainder is the signed idiv's, compared signed
// with the unsigned byte; Field_State is read after Rand.
extern "C" void __cdecl BattleObj_SwingCue(void) {
    battle_obj_states::g.tick_once();
    rv::As<records::PartyMember>(S())->step_frames = static_cast<unsigned char>(rv::As<records::PartyMember>(S())->step_frames - 1);
    if (rv::As<records::PartyMember>(S())->step_frames != 0) return;
    unsigned cue;
    if (battle_obj_states::g.roll_pending() != 0) {
        cue = 3;
    } else {
        const int roll = battle_obj_states::g.rand_() % 100;
        if (static_cast<int>(F()[0xBA]) > roll) {
            battle_obj_states::At(::bof3::Img(battle_obj_states::at::kRoundFlags.v))[0] = static_cast<unsigned char>(battle_obj_states::At(::bof3::Img(battle_obj_states::at::kRoundFlags.v))[0] | 0x80);
            cue = 3;
        } else {
            cue = 2;
        }
    }
    battle_obj_states::g.play_cue(cue);
    battle_obj_states::g.play_cue(4);
    rv::As<records::PartyMember>(S())->step = static_cast<unsigned char>(rv::As<records::PartyMember>(S())->step + 1);
}

// original 0x4419D0 (PSX 0x801DFB18): once a script tick answers non-zero -
// with 0x904AA8 bit 7 (the roll above), BattleTask_Create(0, 2); then
// Battle_SetTargetFlag40(0x904B44), 0x904AA8 |= 4, and the action's end
// (a tail jump to 0x442DD0).
extern "C" void __cdecl BattleObj_SwingEnd(void) {
    if (battle_obj_states::g.tick_once() == 0) return;
    if (battle_obj_states::At(::bof3::Img(battle_obj_states::at::kRoundFlags.v))[0] & 0x80) battle_obj_states::g.task_create(0, 2);
    battle_obj_states::g.set_target_flag(battle_obj_states::At(::bof3::Img(battle_obj_states::at::kSwingTarget.v))[0]);
    battle_obj_states::At(::bof3::Img(battle_obj_states::at::kRoundFlags.v))[0] = static_cast<unsigned char>(battle_obj_states::At(::bof3::Img(battle_obj_states::at::kRoundFlags.v))[0] | 4);
    battle_obj_states::g.end_action();
}

// original 0x441A10 (PSX 0x801DFB90): BattleObj_StateTable[6] - jmp [BattleObj_HitSubs + 4 * +2]
// (6: the entry twice, the receive, the step, the fall, the HP change).
extern "C" void __cdecl BattleObj_StateHit(void) { Run("BattleObj_StateHit", AddressOf(BattleObj_HitSubs), 6, 2); }

// original 0x441A30 (PSX 0x801DFBD4): BattleObj_HitSubs[0] and [2] - Field_State +0x12F =
// Sprite_Current +0x4B, the word +0x140 = its word +0x58; +4 = 0, +2 = 1.
extern "C" void __cdecl BattleObj_HitEnter(void) {
    F()[0x12F] = rv::As<records::PartyMember>(S())->animation;
    rv::Store<unsigned>(rv::As<records::PartyMember>(F())->saved_script_pos, rv::As<records::PartyMember>(S())->script_pos);
    rv::As<records::PartyMember>(S())->state4 = 0;
    rv::As<records::PartyMember>(S())->step = 1;
}

// original 0x441A70 (PSX 0x801DFC24): BattleObj_HitSubs[1] - jmp [BattleObj_HitReceiveSubs + 4 *
// +3] (3: BattleObj_HitReceive, _HitWaitPose, _HitEnd).
extern "C" void __cdecl BattleObj_HitReceiveDispatch(void) {
    Run("BattleObj_HitReceiveDispatch", AddressOf(BattleObj_HitReceiveSubs), 3, 3);
}

// original 0x441A90: the hit received (the party twin of
// EnemyOp_ReceiveAction; the sibling's hypothesis for the PSX twin is
// Battle_ResolveAction_Party).
//   1. The words 0x904B9A / 0x904B98 = 0; 0x904B5C = Sprite_Current, 0x904B54
//      = its +5; 0x904B60 = Field_State + 0x124, whose words +4 / +6 and byte
//      +8 are zeroed; 32 bytes of Field_State +0xA0 to 0x939F80.
//   2. The member's +0x90 bit 11: the word 0x939F86 = 1. Its +0x130 bit 0:
//      0x939F9B up by 25, to 100 at most.
//   3. By the action's kind 0x904B35: 1 - the word Battle_ApplyDamage(the
//      actor 0x904B34, 0x904B54) to 0x904B60's +4; 4, 5 -
//      Effect_ApplyResult; else nothing.
//   4. The member's +0x12C bit 0 with its word +0x128 at 0: +2 = 3, and done.
//   5. Unless its +0x130 bit 9: the pose BattleObj_HitPose for kind 1, for
//      kind 4 when the ability's byte +0 lacks bit 2, for kind 5 when
//      0x591810(0x904B81, 0x904B80) lacks bit 2.
//   6. +0x12C bit 0: Battle_SetDamagePopup(+0x128, Sprite_Current +5); bit 1:
//      0x453EB0(+0x12A, +5) (BE6's).
//   7. +0x90 |= 0x904B98, which is zeroed.
//   8. The s16 +0x128 above 0: with the word +0x98 (HP) at 0, +0x91 |= 0x40
//      under +0x134 bit 1, else +0x90 = 0x4000, then Battle_PlayActorCue(1)
//      unless +0x134 bit 1 and Sprite_ReleaseTint(Sprite_Current); HP left,
//      Battle_PlayActorCue(0). Either s16 +0x128 or +0x12A below 0:
//      Sound_PlayById(0x206).
//   9. Unless 0x904AA8 bit 13: Battle_PlayHitSound and Battle_SetHitPopup for
//      kind 1, and for kind 4 when the ability's word +4 has bit 11;
//      0x904AA8 &= ~0x80.
//  10. BattleObj_ScriptTick; +3 up by one.
// Field_State is read again after every call, and 0x904B35 at each test.
extern "C" void __cdecl BattleObj_HitReceive(void) {
    unsigned char* const s = S();
    auto* const member_s = rv::As<records::PartyMember>(s);
    unsigned char* f = F();
    records::PartyMember* member_f = rv::As<records::PartyMember>(f);
    SetWord(move_script::At(::bof3::Img(battle_e3::at::kStatusOr2.v)), 0);
    SetWord(move_script::At(::bof3::Img(battle_e3::at::kStatusOr.v)), 0);
    const unsigned char actor = member_s->actor;
    BattleE3_HitSprite = s;
    B(::bof3::Img(battle_e3::at::kHitActor.v)) = actor;
    SetPtrAt(::bof3::Img(battle_e3::at::kResult.v), f + 0x124);
    rv::Store<unsigned>(member_f->hp_change, 0);
    rv::Store<unsigned>(rv::As<records::ActionRecord>(PtrAt(::bof3::Img(battle_e3::at::kResult.v)))->ap_change, 0);
    PtrAt(::bof3::Img(battle_e3::at::kResult.v))[8] = 0;
    std::memcpy(move_script::At(::bof3::Img(battle_e3::at::kStatCopy.v)), f + 0xA0, 32);
    if (member_f->status & 0x800) BattleDamage_TargetDef = 1;
    if (f[0x130] & 1) {
        if (BattleE3_EvadeBoost + 0x19u >= 0x64u)
            BattleE3_EvadeBoost = 0x64;
        else
            BattleE3_EvadeBoost += 0x19;
    }
    unsigned kind = B(::bof3::Img(battle_e3::at::kActKind.v));
    if (kind == 1) {
        // the target goes out in eax with Field_State's upper bytes (the
        // register held the pointer); the actor's in ecx, whose upper bytes the
        // original had zeroed
        const U target = (Key(f) & 0xFFFFFF00u) | B(::bof3::Img(battle_e3::at::kHitActor.v));
        const short dealt = BH_CALL(Battle_ApplyDamage)(B(::bof3::Img(battle_e3::at::kActor.v)), target);
        rv::Store<unsigned>(rv::As<records::ActionRecord>(PtrAt(::bof3::Img(battle_e3::at::kResult.v)))->hp_change, static_cast<unsigned short>(dealt));
        kind = B(::bof3::Img(battle_e3::at::kActKind.v));
        f = F();
        member_f = rv::As<records::PartyMember>(f);
    } else if (kind >= 4 && kind <= 5) {
        BH_CALL(Effect_ApplyResult)();
        kind = B(::bof3::Img(battle_e3::at::kActKind.v));
        f = F();
        member_f = rv::As<records::PartyMember>(f);
    }
    if ((f[0x12C] & 1) && static_cast<std::uint16_t>(member_f->hp_change) == 0) {
        rv::As<records::PartyMember>(S())->step = 3;
        return;
    }
    if ((member_f->flags_130 & 0x200) == 0) {
        if (kind == 1) {
            BH_CALL(BattleObj_HitPose)();
            f = F();
            member_f = rv::As<records::PartyMember>(f);
        } else if (kind == 4) {
            if ((AbilityByte(::bof3::Img(battle_e3::at::kAbilityFlags0.v)) & 4) == 0) {
                BH_CALL(BattleObj_HitPose)();
                f = F();
                member_f = rv::As<records::PartyMember>(f);
            }
        } else if (kind == 5) {
            const unsigned char cls = BH_AT(unsigned char (__cdecl*)(unsigned, unsigned), battle_e3::at::kItemClass.v)(B(::bof3::Img(battle_e3::at::kItemCategory.v)),
                                                                                                        B(::bof3::Img(battle_e3::at::kAbility.v)));
            if ((cls & 4) == 0) BH_CALL(BattleObj_HitPose)();
            f = F();
            member_f = rv::As<records::PartyMember>(f);
        }
    }
    if (f[0x12C] & 1) {
        BH_CALL(Battle_SetDamagePopup)(static_cast<std::uint16_t>(member_f->hp_change), rv::As<records::PartyMember>(S())->actor);
        f = F();
        member_f = rv::As<records::PartyMember>(f);
    }
    if (f[0x12C] & 2) {
        BH_AT(void (__cdecl*)(unsigned, unsigned), battle_e3::at::kSecondPopup.v)(static_cast<std::uint16_t>(member_f->ap_change), rv::As<records::PartyMember>(S())->actor);
        f = F();
        member_f = rv::As<records::PartyMember>(f);
    }
    member_f->status = member_f->status | Word(move_script::At(::bof3::Img(battle_e3::at::kStatusOr.v)));
    f = F();
    member_f = rv::As<records::PartyMember>(f);
    SetWord(move_script::At(::bof3::Img(battle_e3::at::kStatusOr.v)), 0);
    if (member_f->hp_change > 0) {
        if (member_f->hp == 0) {
            if (f[0x134] & 2)
                f[0x91] |= 0x40;
            else
                member_f->status = 0x4000;
            if ((F()[0x134] & 2) == 0) BH_CALL(Battle_PlayActorCue)(1);
            BH_CALL(Sprite_ReleaseTint)(S());
        } else {
            BH_CALL(Battle_PlayActorCue)(0);
        }
        f = F();
        member_f = rv::As<records::PartyMember>(f);
    }
    if (member_f->hp_change < 0 || member_f->ap_change < 0) BH_CALL(Sound_PlayById)(0x206);
    if ((Word(move_script::At(::bof3::Img(battle_e3::at::kRoundFlags.v))) & 0x2000) == 0) {
        if (B(::bof3::Img(battle_e3::at::kActKind.v)) == 1) {
            BH_CALL(Battle_PlayHitSound)();
            BH_CALL(Battle_SetHitPopup)();
        }
        if (B(::bof3::Img(battle_e3::at::kActKind.v)) == 4 && (AbilityWord4() & 0x800) != 0) {
            BH_CALL(Battle_PlayHitSound)();
            BH_CALL(Battle_SetHitPopup)();
        }
        SetWord(move_script::At(::bof3::Img(battle_e3::at::kRoundFlags.v)), Word(move_script::At(::bof3::Img(battle_e3::at::kRoundFlags.v))) & 0xFF7F);
    }
    BH_CALL(BattleObj_ScriptTick)();
    rv::As<records::PartyMember>(S())->state3 += 1;
}

// original 0x441D50 (PSX 0x801E0140): BattleObj_HitReceiveSubs[1] and BattleObj_HpSubs[1] -
// when Sprite_ScriptTickOnce answers, +3 up by one and, unless Field_State
// +0x90 bit 2, a tail jump to BattleObj_PickPose.
extern "C" void __cdecl BattleObj_HitWaitPose(void) {
    if (BH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    rv::As<records::PartyMember>(S())->state3 += 1;
    if (F()[0x90] & 4) return;
    BH_CALL(BattleObj_PickPose)();
}

// original 0x441D80: BattleObj_HitReceiveSubs[2] and BattleObj_HitStepSubs[4]
// (the party twin of EnemyOp_HitEnd) - BattleObj_ScriptTickOnce; then
//   +2 not 3 and Field_State +0x91 bit 6: 0x904AA8 &= ~0x40, +1..+3 = 6, 4, 0
//   (the fall), and done;
//   0x904AA8 bit 6: the word 0x904AA8 = (it & ~0x40) | 0x1000;
//   else, unless the target 0x904B44 has bit 6 or 7: 0x904AA8 |= 0x40 when
//   0x446810 (BE4's) answers for kind 1, and for kind 4 when the ability's
//   byte +5 has bit 3, 0x446810 answers and its byte +0 lacks bit 4;
// then Battle_ClearActorBit(+5); the member's +0x130 &= ~0x70;
// BattleParty_AllDown sets 0x904AE8 bit 0 (the loss); +0x130 &= ~0x200;
// +1..+3 = 2, 0, 0.
extern "C" void __cdecl BattleObj_HitEnd(void) {
    BH_CALL(BattleObj_ScriptTickOnce)();
    unsigned char* const s = S();
    auto* const member_s = rv::As<records::PartyMember>(s);
    if (s[2] != 3 && (F()[0x91] & 0x40)) {
        SetWord(move_script::At(::bof3::Img(battle_e3::at::kRoundFlags.v)), Word(move_script::At(::bof3::Img(battle_e3::at::kRoundFlags.v))) & 0xFFBF);
        member_s->state = 6;
        rv::As<records::PartyMember>(S())->step = 4;
        rv::As<records::PartyMember>(S())->state3 = 0;
        return;
    }
    if (B(::bof3::Img(battle_e3::at::kRoundFlags.v)) & 0x40) {
        SetWord(move_script::At(::bof3::Img(battle_e3::at::kRoundFlags.v)), (Word(move_script::At(::bof3::Img(battle_e3::at::kRoundFlags.v))) & 0xFFBF) | 0x1000);
    } else if ((B(::bof3::Img(battle_e3::at::kTarget.v)) & 0xC0) == 0) {
        if (B(::bof3::Img(battle_e3::at::kActKind.v)) == 1 && BH_AT(unsigned char (__cdecl*)(), battle_e3::at::kRollB9.v)() != 0) B(::bof3::Img(battle_e3::at::kRoundFlags.v)) |= 0x40;
        if (B(::bof3::Img(battle_e3::at::kActKind.v)) == 4 && (AbilityByte(::bof3::Img(battle_e3::at::kAbilityFlags5.v)) & 8) != 0 &&
            BH_AT(unsigned char (__cdecl*)(), battle_e3::at::kRollB9.v)() != 0 && (AbilityByte(::bof3::Img(battle_e3::at::kAbilityFlags0.v)) & 0x10) == 0)
            B(::bof3::Img(battle_e3::at::kRoundFlags.v)) |= 0x40;
    }
    BH_CALL(Battle_ClearActorBit)(rv::As<records::PartyMember>(S())->actor);
    SetULong(F() + 0x130, ULong(F() + 0x130) & 0xFFFFFF8Fu);
    if (BH_CALL(BattleParty_AllDown)() != 0) Battle_EndFlags |= 1;
    SetULong(F() + 0x130, ULong(F() + 0x130) & ~0x200u);
    rv::As<records::PartyMember>(S())->state = 2;
    rv::As<records::PartyMember>(S())->step = 0;
    rv::As<records::PartyMember>(S())->state3 = 0;
}

// original 0x441EB0 (PSX 0x801E0400): BattleObj_HitSubs[3] - jmp [BattleObj_HitStepSubs + 4 * +3]
// (5: _HitStepStart, _HitStepOut, _HitStepBack, _HitStepPose, BattleObj_HitEnd).
extern "C" void __cdecl BattleObj_HitStepDispatch(void) {
    Run("BattleObj_HitStepDispatch", AddressOf(BattleObj_HitStepSubs), 5, 3);
}

// original 0x441ED0 (PSX 0x801E0444): the step's start - with Field_State +0x91 bit 6, +3 = 2
// and +0xA = 0; else the word +0x128 = 0, the velocity +0xC = 0, +0x10 =
// -0x2000 turned by the pose (0x446770, BE4's), +0xA = 4,
// BattleObj_ScriptTickOnce, Sound_PlayById(0x205), +3 up by one.
extern "C" void __cdecl BattleObj_HitStepStart(void) {
    unsigned char* const f = F();
    if (f[0x91] & 0x40) {
        rv::As<records::PartyMember>(S())->state3 = 2;
        S()[0xA] = 0;
        return;
    }
    rv::Store<unsigned>(rv::As<records::PartyMember>(f)->hp_change, 0);
    SetULong(S() + 0xC, 0);
    SetULong(S() + 0x10, 0xFFFFE000u);
    BH_AT(void (__cdecl*)(unsigned char*), battle_e3::at::kTurnVelocityC.v)(S());
    S()[0xA] = 4;
    BH_CALL(BattleObj_ScriptTickOnce)();
    BH_CALL(Sound_PlayById)(0x205);
    rv::As<records::PartyMember>(S())->state3 += 1;
}

// original 0x441F50 (PSX 0x801E04E8): the step out - +0xA down by one; at 0, +0xA = 4 and +3 up
// by one; the position +0x34 / +0x38 up by the velocity; a tail jump to
// BattleObj_ScriptTickOnce.
extern "C" void __cdecl BattleObj_HitStepOut(void) {
    unsigned char* const s = S();
    auto* const member_s = rv::As<records::PartyMember>(s);
    s[0xA] -= 1;
    if (s[0xA] == 0) {
        s[0xA] = 4;
        member_s->state3 += 1;
    }
    MoveBy(s, false);
    BH_CALL(BattleObj_ScriptTickOnce)();
}

// original 0x441FA0 (PSX 0x801E0580): the step back - +0xA not 0: the position down by the
// velocity, +0xA down by one, BattleObj_ScriptTickOnce. At 0: with Field_State
// +0x12C bit 4, Battle_SetDamagePopup(its word +0x128, +5) and +3 up by one;
// else BattleTask_Create(0, 1) - the slot's owner +0x80 = Sprite_Current, its
// +7 = 3, +0x27 = 0 - and +3 up by one.
extern "C" void __cdecl BattleObj_HitStepBack(void) {
    unsigned char* const s = S();
    auto* const member_s = rv::As<records::PartyMember>(s);
    if (s[0xA] != 0) {
        MoveBy(s, true);
        s[0xA] -= 1;
        BH_CALL(BattleObj_ScriptTickOnce)();
        return;
    }
    unsigned char* const f = F();
    if (f[0x12C] & 0x10) {
        BH_CALL(Battle_SetDamagePopup)(static_cast<std::uint16_t>(rv::As<records::PartyMember>(f)->hp_change), member_s->actor);
        rv::As<records::PartyMember>(S())->state3 += 1;
        return;
    }
    unsigned char* const t = TaskSlot("BattleObj_HitStepBack", BH_CALL(BattleTask_Create)(0, 1));
    auto* const task_rec = rv::As<records::BattleTask>(t);
    unsigned char* const owner = S();
    auto* const owner_rec = rv::As<records::PartyMember>(owner);
    SetPtrAt(Key(t + 0x80), owner);
    t[7] = 3;
    task_rec->clut = 0;
    owner_rec->state3 += 1;
}

// original 0x442050 (PSX 0x801E0680): the step's pose - +3 up by one; unless Field_State +0x90
// bit 2, a tail jump to BattleObj_PickPose.
extern "C" void __cdecl BattleObj_HitStepPose(void) {
    rv::As<records::PartyMember>(S())->state3 += 1;
    if (F()[0x90] & 4) return;
    BH_CALL(BattleObj_PickPose)();
}

// original 0x442080: BattleObj_HitSubs[4] - jmp [BattleObj_FallSubs + 4 * +3]
// (7: _Fall, _Revive, _ReviveEnd, _ReviveByEquip, _FallTaskDispatch,
// _ReviveByEquip, _ReviveByEquip).
extern "C" void __cdecl BattleObj_FallDispatch(void) { Run("BattleObj_FallDispatch", AddressOf(BattleObj_FallSubs), 7, 3); }

// original 0x4420A0: the member falls - Battle_ReturnQueuedItem(+5),
// Battle_RemoveFromTurnOrder(+5); then by Field_State:
//   +0x130 bit 2: +9 = 0x1E, +3 = 1 (the revive), +4 = 0, BattleObj_ScriptTick;
//   +0x134 bit 1: 0x904AA8 bit 14 cleared for this actor, +3 = 4 (the task),
//     BattleObj_ScriptTick;
//   +0x96 at 0x18: its character record's +0x16 = 0, +9 = 0x1E, +3 = 3;
//   +0x97 at 0x18: the record's +0x17 = 0, +9 = 0x1E, +3 = 6;
//   +0x95 at 0x43: the record's +0x15 = 0, +9 = 0x1E, +3 = 5 (each then
//     BattleObj_ScriptTick);
//   else: 0x904AB1 down by one; +0x138..+0x13F zeroed; +0x134 &= 0xFFFB84FF;
//     +0x12D, +0x142..+0x145 = 0; 0x453300(+5) (BE6's); BattleParty_AllDown or
//     0x904AB1 at 0 sets 0x904AE8 bit 0 (the loss); 0x904AA8 bit 14 cleared
//     for this actor; with +0x134 bit 0 +3 = 4 and BattleObj_ScriptTick; else
//     Battle_ClearActorBit(+5), +0x130 &= ~0x70 and ~0x200, +1..+3 = 3, 0, 0,
//     BattleObj_ScriptTick.
extern "C" void __cdecl BattleObj_Fall(void) {
    BH_CALL(Battle_ReturnQueuedItem)(rv::As<records::PartyMember>(S())->actor);
    BH_CALL(Battle_RemoveFromTurnOrder)(rv::As<records::PartyMember>(S())->actor);
    unsigned char* const f = F();
    auto* const member_f = rv::As<records::PartyMember>(f);
    if (f[0x130] & 4) {
        rv::As<records::PartyMember>(S())->step_frames = 0x1E;
        rv::As<records::PartyMember>(S())->state3 = 1;
        rv::As<records::PartyMember>(S())->state4 = 0;
        BH_CALL(BattleObj_ScriptTick)();
        return;
    }
    if (f[0x134] & 2) {
        ClearFormFlag();
        rv::As<records::PartyMember>(S())->state3 = 4;
        BH_CALL(BattleObj_ScriptTick)();
        return;
    }
    struct Equip { unsigned at; unsigned char value; unsigned record_at; unsigned char next; };
    static const Equip kEquip[] = {{0x96, 0x18, 0x16, 3}, {0x97, 0x18, 0x17, 6}, {0x95, 0x43, 0x15, 5}};
    for (const Equip& q : kEquip) {
        if (f[q.at] != q.value) continue;
        CharRecord(member_f->char_index)[q.record_at] = 0;
        rv::As<records::PartyMember>(S())->step_frames = 0x1E;
        rv::As<records::PartyMember>(S())->state3 = q.next;
        BH_CALL(BattleObj_ScriptTick)();
        return;
    }
    Battle_PartyStanding -= 1;
    for (unsigned i = 0; i < 8; ++i) F()[0x138 + i] = 0;
    SetULong(F() + 0x134, ULong(F() + 0x134) & 0xFFFB84FFu);
    F()[0x12D] = 0;
    rv::As<records::PartyMember>(F())->count_4000 = 0;
    rv::As<records::PartyMember>(F())->count_800 = 0;
    rv::As<records::PartyMember>(F())->count_40 = 0;
    rv::As<records::PartyMember>(F())->count_80 = 0;
    BH_AT(void (__cdecl*)(unsigned), battle_e3::at::kPass453300.v)(rv::As<records::PartyMember>(S())->actor);
    if (BH_CALL(BattleParty_AllDown)() != 0 || Battle_PartyStanding == 0) Battle_EndFlags |= 1;
    ClearFormFlag();
    auto* const member_s = rv::As<records::PartyMember>(S());
    if (F()[0x134] & 1) {
        member_s->state3 = 4;
        BH_CALL(BattleObj_ScriptTick)();
        return;
    }
    BH_CALL(Battle_ClearActorBit)(member_s->actor);
    SetULong(F() + 0x130, ULong(F() + 0x130) & 0xFFFFFF8Fu);
    SetULong(F() + 0x130, ULong(F() + 0x130) & ~0x200u);
    rv::As<records::PartyMember>(S())->state = 3;
    rv::As<records::PartyMember>(S())->step = 0;
    rv::As<records::PartyMember>(S())->state3 = 0;
    BH_CALL(BattleObj_ScriptTick)();
}

// original 0x442310: the party's stats again - Char_RecalcStats(Field_State's
// character record, by its +0x148); for each member below the party count
// 0x904AB0, its +0x92..+0x97 from its record's +0x12..+0x17 and 32 bytes of
// +0xC0 from the record's +0x20; Formation_ApplyStatMods; for each member, 32
// bytes of +0xC0 to +0xA0; 0x44FDE0 (BE5's); 0x453300(member) (BE6's) for each
// member. The count is read again after each call.
extern "C" void __cdecl BattleParty_RecalcStats(void) {
    BH_CALL(Char_RecalcStats)(CharRecord(rv::As<records::PartyMember>(F())->char_index));
    unsigned n = PartyCount("BattleParty_RecalcStats");
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const m = Member(i);
        auto* const member_rec = rv::As<records::PartyMember>(m);
        const unsigned char* const r = CharRecord(member_rec->char_index);
        for (unsigned k = 0; k < 6; ++k) m[0x92 + k] = r[0x12 + k];
        std::memcpy(m + 0xC0, r + 0x20, 32);
    }
    BH_CALL(Formation_ApplyStatMods)();
    n = PartyCount("BattleParty_RecalcStats");
    for (unsigned i = 0; i < n; ++i) std::memmove(Member(i) + 0xA0, Member(i) + 0xC0, 32);
    BH_AT(void (__cdecl*)(), battle_e3::at::kPass44FDE0.v)();
    if (B(::bof3::Img(battle_e3::at::kPartyCount.v)) == 0) return;
    unsigned member = 0;
    do {
        BH_AT(void (__cdecl*)(unsigned), battle_e3::at::kPass453300.v)(member);
        member = static_cast<unsigned char>(member + 1);
    } while (member < B(::bof3::Img(battle_e3::at::kPartyCount.v)));
}

// original 0x442420 (PSX 0x801E0DA0): al 0 if any in-use member (+0 bit 0) of the three is up -
// its +0x90 has neither bit 2 nor 14, or its +0x130 bit 2 or +0x134 bit 1 is
// set, or with +0x90 bit 14 its +0x96 or +0x97 is 0x18 or its +0x95 0x43 (the
// states BattleObj_Fall revives from); else 1 (the party is down). The upper
// bytes of eax are the caller's.
extern "C" unsigned char __cdecl BattleParty_AllDown(void) {
    for (unsigned i = 0; i < battle_e3::at::kPartyMax; ++i) {
        const unsigned char* const m = Member(i);
        auto* const member_rec = rv::AsConst<records::PartyMember>(m);
        if ((member_rec->flags & 1) == 0) continue;
        const unsigned w = member_rec->status;
        if ((w & 0x4004) == 0) return 0;
        if (m[0x130] & 4) return 0;
        if (m[0x134] & 2) return 0;
        if ((w & 0x4000) != 0 && (member_rec->accessory[0] == 0x18 || member_rec->accessory[1] == 0x18 || m[0x95] == 0x43)) return 0;
    }
    return 1;
}

// original 0x4424A0 (PSX 0x801E0EA0): BattleObj_FallSubs[1], the revive - +9 down by one; at 0:
// Field_State +0x90 &= ~0x4000, +0x130 &= ~4, the word +0x98 = 1, +0x9C
// halved; BattleTask_Create(0, 1) - its owner Sprite_Current, +7 = 1, +0x27 =
// 6; Sprite_SetAnimation(+8 + 4); Sound_PlayById(0x206); unless 0x904AE9,
// Battle_OpenMsgWindow, 0x44A910(+5) and 0x44AA90(0, Field_State +0x89) (BE4's);
// +3 = 2; BattleObj_ScriptTick.
extern "C" void __cdecl BattleObj_Revive(void) {
    rv::As<records::PartyMember>(S())->step_frames -= 1;
    if (rv::As<records::PartyMember>(S())->step_frames != 0) return;
    rv::As<records::PartyMember>(F())->status = rv::As<records::PartyMember>(F())->status & 0xBFFF;
    SetULong(F() + 0x130, ULong(F() + 0x130) & 0xFFFFFFFBu);
    rv::As<records::PartyMember>(F())->hp = 1;
    rv::As<records::PartyMember>(F())->survive >>= 1;
    unsigned char* const t = TaskSlot("BattleObj_Revive", BH_CALL(BattleTask_Create)(0, 1));
    auto* const task_rec = rv::As<records::BattleTask>(t);
    unsigned char* const s = S();
    auto* const member_s = rv::As<records::PartyMember>(s);
    SetPtrAt(Key(t + 0x80), s);
    t[7] = 1;
    task_rec->clut = 6;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(member_s->direction + 4));
    BH_CALL(Sound_PlayById)(0x206);
    if (BattleMisc_BannerKinds == 0) {
        BH_CALL(Battle_OpenMsgWindow)();
        BH_AT(void (__cdecl*)(unsigned), battle_e3::at::kNameToText.v)(rv::As<records::PartyMember>(S())->actor);
        BH_AT(void (__cdecl*)(unsigned, unsigned), battle_e3::at::kBannerByPair.v)(0, rv::As<records::PartyMember>(F())->char_id);
    }
    rv::As<records::PartyMember>(S())->state3 = 2;
    BH_CALL(BattleObj_ScriptTick)();
}

// original 0x4425A0 (PSX 0x801E0FE4): BattleObj_FallSubs[2] - Battle_ClearActorBit(+5);
// Field_State +0x130's low byte &= 0x0F and its bit 9 cleared; +1..+3 = 3, 0, 0
// (standing again).
extern "C" void __cdecl BattleObj_ReviveEnd(void) {
    BH_CALL(Battle_ClearActorBit)(rv::As<records::PartyMember>(S())->actor);
    SetULong(F() + 0x130, ULong(F() + 0x130) & 0xFFFFFF0Fu);
    SetULong(F() + 0x130, ULong(F() + 0x130) & ~0x200u);
    rv::As<records::PartyMember>(S())->state = 3;
    rv::As<records::PartyMember>(S())->step = 0;
    rv::As<records::PartyMember>(S())->state3 = 0;
}

// original 0x442600: BattleObj_FallSubs[3], [5], [6] - +9 down by one; at 0: by
// +3, 3 - Field_State +0x96 = 0; 5 - +0x95 = 0 and BattleParty_RecalcStats;
// 6 - +0x97 = 0 (Sprite_Current read again for each test); then +0x90 &=
// ~0x4000, the word +0x98 = its +0xA0, +0x9C = +0xAE; BattleTask_Create(0, 1)
// - owner Sprite_Current, +7 = 2, +0x27 = 6; Sprite_SetAnimation(+8 + 4);
// Sound_PlayById(0x206); unless 0x904AE9, Battle_OpenMsgWindow and
// BattleBanner_Add(2, 0, 0, 0x2D, Msg_SystemPtr(0x2E for +3 at 5, else 0x3B));
// +3 = 2; BattleObj_ScriptTick.
extern "C" void __cdecl BattleObj_ReviveByEquip(void) {
    rv::As<records::PartyMember>(S())->step_frames -= 1;
    unsigned char* s = S();
    if (s[9] != 0) return;
    if (s[3] == 3) {
        rv::As<records::PartyMember>(F())->accessory[0] = 0;
        s = S();
    }
    if (s[3] == 5) {
        F()[0x95] = 0;
        BH_CALL(BattleParty_RecalcStats)();
        s = S();
    }
    if (s[3] == 6) rv::As<records::PartyMember>(F())->accessory[1] = 0;
    rv::As<records::PartyMember>(F())->status = rv::As<records::PartyMember>(F())->status & 0xBFFF;
    rv::As<records::PartyMember>(F())->hp = rv::As<records::PartyMember>(F())->max_hp;
    rv::As<records::PartyMember>(F())->survive = rv::As<records::PartyMember>(F())->byte_ae;
    unsigned char* const t = TaskSlot("BattleObj_ReviveByEquip", BH_CALL(BattleTask_Create)(0, 1));
    auto* const task_rec = rv::As<records::BattleTask>(t);
    s = S();
    SetPtrAt(Key(t + 0x80), s);
    t[7] = 2;
    task_rec->clut = 6;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(s[8] + 4));
    BH_CALL(Sound_PlayById)(0x206);
    if (BattleMisc_BannerKinds == 0) {
        BH_CALL(Battle_OpenMsgWindow)();
        const unsigned id = rv::As<records::PartyMember>(S())->state3 == 5 ? 0x2E : 0x3B;
        const unsigned char* const text = BH_CALL(Msg_SystemPtr)(id);
        BH_CALL(BattleBanner_Add)(2, 0, 0, 0x2D, reinterpret_cast<const char*>(text));
    }
    rv::As<records::PartyMember>(S())->state3 = 2;
    BH_CALL(BattleObj_ScriptTick)();
}

// original 0x442740 (PSX 0x801E122C): BattleObj_FallSubs[4] - jmp [BattleObj_FallTaskSubs + 4 *
// +4] (2: _FallTaskStart, _FallTaskWait).
extern "C" void __cdecl BattleObj_FallTaskDispatch(void) {
    Run("BattleObj_FallTaskDispatch", AddressOf(BattleObj_FallTaskSubs), 2, 4);
}

// original 0x442760 (PSX 0x801E1270): nothing while any member below the party count 0x904AB0
// has +0x130 bit 13; else Field_State +0x130 |= 0x2000, BattleTask_Create(0,
// 0xB for the character +0x89 at 4, else 0xC) - its owner Sprite_Current - and
// +4 up by one.
extern "C" void __cdecl BattleObj_FallTaskStart(void) {
    const unsigned n = PartyCount("BattleObj_FallTaskStart");
    for (unsigned i = 0; i < n; ++i)
        if (ULong(Member(i) + 0x130) & 0x2000) return;
    SetULong(F() + 0x130, ULong(F() + 0x130) | 0x2000);
    const unsigned kind = rv::As<records::PartyMember>(F())->char_id == 4 ? 0xB : 0xC;
    unsigned char* const t = TaskSlot("BattleObj_FallTaskStart", BH_CALL(BattleTask_Create)(0, kind));
    unsigned char* const s = S();
    auto* const member_s = rv::As<records::PartyMember>(s);
    SetPtrAt(Key(t + 0x80), s);
    member_s->state4 += 1;
}

// original 0x442800 (PSX 0x801E134C): nothing while Field_State +0x130 has bit 13 (the task
// clears it); then +0x130 &= ~0x70 and ~0x200, Battle_ClearActorBit(+5), +1..+4
// = 2, 0, 0, 0.
extern "C" void __cdecl BattleObj_FallTaskWait(void) {
    unsigned char* const f = F();
    const U flags = ULong(f + 0x130);
    if (flags & 0x2000) return;
    SetULong(f + 0x130, flags & 0xFFFFFF8Fu);
    SetULong(F() + 0x130, ULong(F() + 0x130) & ~0x200u);
    BH_CALL(Battle_ClearActorBit)(rv::As<records::PartyMember>(S())->actor);
    rv::As<records::PartyMember>(S())->state = 2;
    rv::As<records::PartyMember>(S())->step = 0;
    rv::As<records::PartyMember>(S())->state3 = 0;
    rv::As<records::PartyMember>(S())->state4 = 0;
}

// original 0x442870 (PSX 0x801E13E4): BattleObj_HitSubs[5] - jmp [BattleObj_HpSubs + 4 * +3] (3:
// _HpApply, BattleObj_HitWaitPose, _HpEnd).
extern "C" void __cdecl BattleObj_HpDispatch(void) { Run("BattleObj_HpDispatch", AddressOf(BattleObj_HpSubs), 3, 3); }

// original 0x442890 (PSX 0x801E1428): the change applied - with the word +0x98 (HP) at or below
// the s16 +0x128 (signed compare): +0x90 = 0x4000, +0x98 = 0,
// Battle_PlayActorCue(1); else Battle_PlayActorCue(0) for +0x128 not negative,
// Sound_PlayById(0x206) for negative, +0x98 -= +0x128, and at most +0xA0.
// Then +0x12C bit 0: Battle_SetDamagePopup(+0x128, +5); bit 1: 0x453EB0(+0x12A,
// +5) (BE6's); BattleObj_ScriptTick; +3 up by one.
extern "C" void __cdecl BattleObj_HpApply(void) {
    unsigned char* f = F();
    records::PartyMember* member_f = rv::As<records::PartyMember>(f);
    const int change = member_f->hp_change;
    if (static_cast<int>(member_f->hp) <= change) {
        member_f->status = 0x4000;
        rv::As<records::PartyMember>(F())->hp = 0;
        BH_CALL(Battle_PlayActorCue)(1);
    } else {
        if (change >= 0)
            BH_CALL(Battle_PlayActorCue)(0);
        else
            BH_CALL(Sound_PlayById)(0x206);
        f = F();
        member_f = rv::As<records::PartyMember>(f);
        member_f->hp = member_f->hp - static_cast<std::uint16_t>(member_f->hp_change);
        f = F();
        member_f = rv::As<records::PartyMember>(f);
        if (member_f->hp > member_f->max_hp) member_f->hp = member_f->max_hp;
    }
    f = F();
    member_f = rv::As<records::PartyMember>(f);
    if (f[0x12C] & 1) {
        BH_CALL(Battle_SetDamagePopup)(static_cast<std::uint16_t>(member_f->hp_change), rv::As<records::PartyMember>(S())->actor);
        f = F();
        member_f = rv::As<records::PartyMember>(f);
    }
    if (f[0x12C] & 2) BH_AT(void (__cdecl*)(unsigned, unsigned), battle_e3::at::kSecondPopup.v)(static_cast<std::uint16_t>(member_f->ap_change), rv::As<records::PartyMember>(S())->actor);
    BH_CALL(BattleObj_ScriptTick)();
    rv::As<records::PartyMember>(S())->state3 += 1;
}

// original 0x442980 (PSX 0x801E1560): BattleObj_HpSubs[2] - BattleObj_ScriptTick; with
// Field_State +0x91 bit 6, +1..+3 = 6, 4, 0 (the fall); else
// Battle_ClearActorBit(+5) and +1..+3 = 3, 0, 0.
extern "C" void __cdecl BattleObj_HpEnd(void) {
    BH_CALL(BattleObj_ScriptTick)();
    unsigned char* const f = F();
    auto* const member_s = rv::As<records::PartyMember>(S());
    if (f[0x91] & 0x40) {
        member_s->state = 6;
        rv::As<records::PartyMember>(S())->step = 4;
        rv::As<records::PartyMember>(S())->state3 = 0;
        return;
    }
    BH_CALL(Battle_ClearActorBit)(member_s->actor);
    rv::As<records::PartyMember>(S())->state = 3;
    rv::As<records::PartyMember>(S())->step = 0;
    rv::As<records::PartyMember>(S())->state3 = 0;
}

// original 0x4429E0 (PSX 0x801E1604): a tail jump through 0x64E0E4 by the
// sub-state +2 (0x442A00, 0x442B20, 0x442B80), unchecked.
extern "C" void __cdecl BattleObj_StateCast(void) { Dispatch(battle_obj_states::g.cast_subs, rv::As<records::PartyMember>(S())->step); }

// original 0x442A00 (PSX 0x801E1648): once File_LoadDone answers non-zero.
//   - The counter +9: for the action 4 (a skill) without +0x134 bit 1, 0 when
//     Battle_LoadSoundByKey(character, 0x904B8D) answers non-zero, else 0x1E;
//     with bit 1, 0 when 0x904B89 is 4, 7 or 8 and the same call answers
//     non-zero (else +9 is left); for any other action 0.
//   - Then: when the skill 0x904B80's record has bit 3 of 0x65C4DD, the next
//     state; else +9 from 0x64E0FC by 0x904B89 (+0x134 bit 1) or from
//     0x64E0F0 by the character, the pose +8 + 0x2C, the next sub-state.
//   - Either way a script tick (a tail jump to BattleObj_ScriptTickOnce).
extern "C" void __cdecl BattleObj_CastStart(void) {
    if (battle_obj_states::g.load_done() == 0) return;
    unsigned char* const f = F();
    auto* const member_f = rv::As<records::PartyMember>(f);
    if (f[0x125] == 4) {
        if ((f[0x134] & 2) == 0) {
            const unsigned char set = BattleE3_SoundSet;
            rv::As<records::PartyMember>(S())->step_frames = battle_obj_states::g.load_sound(member_f->char_id, set) != 0 ? 0 : 0x1E;
        } else {
            const unsigned char kind = BattlePhases_B89;
            if (kind == 4 || kind == 7 || kind == 8) {
                const unsigned char set = BattleE3_SoundSet;
                if (battle_obj_states::g.load_sound(member_f->char_id, set) != 0) rv::As<records::PartyMember>(S())->step_frames = 0;
            }
        }
    } else {
        rv::As<records::PartyMember>(S())->step_frames = 0;
    }
    const unsigned skill = Word(battle_obj_states::At(::bof3::Img(battle_obj_states::at::kSkillId.v)));
    if ((battle_obj_states::At(::bof3::Img(battle_obj_states::at::kSkillWord.v) + 1 + skill * 24)[0] & 8) == 0) {
        const unsigned char count = (F()[0x134] & 2) ? battle_obj_states::At(::bof3::Img(battle_obj_states::at::kCastCountBy.v) + BattlePhases_B89)[0]
                                                     : battle_obj_states::At(::bof3::Img(battle_obj_states::at::kCastCount.v) + rv::As<records::PartyMember>(F())->char_id)[0];
        rv::As<records::PartyMember>(S())->step_frames = count;
        Pose(0x2C);
        rv::As<records::PartyMember>(S())->states[1] = static_cast<unsigned char>(rv::As<records::PartyMember>(S())->states[1] + 1);
    } else {
        rv::As<records::PartyMember>(S())->states[0] = static_cast<unsigned char>(rv::As<records::PartyMember>(S())->states[0] + 1);
    }
    battle_obj_states::g.tick_once();
}

// original 0x442B20 (PSX 0x801E1814): the counter +9 down by one while it is
// not 0; at 0 cue 5, and cue 3 as well while Field_State +0x134 has bit 1 and
// 0x904B89 is none of 4, 7, 8; then the next sub-state. Either way a script
// tick (a tail jump).
extern "C" void __cdecl BattleObj_CastCue(void) {
    auto* const member_s = rv::As<records::PartyMember>(S());
    const unsigned char n = member_s->step_frames;
    if (n != 0) {
        member_s->step_frames = static_cast<unsigned char>(n - 1);
    } else {
        battle_obj_states::g.play_cue(5);
        if (F()[0x134] & 2) {
            const unsigned char kind = BattlePhases_B89;
            if (kind != 4 && kind != 7 && kind != 8) battle_obj_states::g.play_cue(3);
        }
        rv::As<records::PartyMember>(S())->step = static_cast<unsigned char>(rv::As<records::PartyMember>(S())->step + 1);
    }
    battle_obj_states::g.tick_once();
}

// original 0x442B80 (PSX 0x801E18C4): once a script tick answers non-zero,
// the next state and the sub-state 0.
extern "C" void __cdecl BattleObj_CastWait(void) {
    if (battle_obj_states::g.tick_once() == 0) return;
    auto* const member_s = rv::As<records::PartyMember>(S());
    member_s->state = static_cast<unsigned char>(member_s->state + 1);
    rv::As<records::PartyMember>(S())->step = 0;
}

// original 0x442BA0 (PSX 0x801E191C): the sub-state +2's handler through
// 0x64E118 (0x442BC0, 0x442BD0, the bare ret 0x437CC0, 0x442C40, 0x442C50),
// unchecked; then, when 0x904AA8 has bit 2, the action's end (from a second
// chunk, 0x442C60, a tail jump to 0x442DD0).
extern "C" void __cdecl BattleObj_StateCastDone(void) {
    Dispatch(battle_obj_states::g.cast_done_subs, rv::As<records::PartyMember>(S())->step);
    if (battle_obj_states::At(::bof3::Img(battle_obj_states::at::kRoundFlags.v))[0] & 4) battle_obj_states::g.end_action();
}

// original 0x442BC0 (PSX 0x801E1968): a script tick, the next sub-state.
extern "C" void __cdecl BattleObj_CastDoneTick(void) {
    battle_obj_states::g.tick_once();
    rv::As<records::PartyMember>(S())->step = static_cast<unsigned char>(rv::As<records::PartyMember>(S())->step + 1);
}

// original 0x442BD0 (PSX 0x801E19A0, paired as Actor_SkillItemDone): for the
// action 4, Field_State's u16 +0x9A less 0x904B88; 0x904AA8 |= 0x800; the
// action cleared; then, when the skill 0x904B80's record has bit 11 of its
// u16 0x65C4DC, the next sub-state (2, the bare ret), else a script tick and
// the sub-state 4.
extern "C" void __cdecl BattleObj_CastDone(void) {
    unsigned char* f = F();
    records::PartyMember* member_f = rv::As<records::PartyMember>(f);
    if (f[0x125] == 4) {
        member_f->ap = member_f->ap - BattleE3_Cost;
        f = F();
        member_f = rv::As<records::PartyMember>(f);
    }
    SetWord(battle_obj_states::At(::bof3::Img(battle_obj_states::at::kRoundFlags.v)), Word(battle_obj_states::At(::bof3::Img(battle_obj_states::at::kRoundFlags.v))) | 0x800);
    f[0x125] = 0;
    const unsigned skill = Word(battle_obj_states::At(::bof3::Img(battle_obj_states::at::kSkillId.v)));
    if (Word(battle_obj_states::At(::bof3::Img(battle_obj_states::at::kSkillWord.v) + skill * 24)) & 0x800) {
        rv::As<records::PartyMember>(S())->step = static_cast<unsigned char>(rv::As<records::PartyMember>(S())->step + 1);
        return;
    }
    battle_obj_states::g.tick_once();
    rv::As<records::PartyMember>(S())->step = 4;
}

// original 0x442C40 (PSX 0x801E1A68): BattleObj_CastDoneSubs[3] - a tail jump to
// BattleObj_ScriptTick (its answer dropped by BattleObj_StateCastDone's call).
extern "C" void __cdecl BattleObj_CastDoneScript(void) { BH_CALL(BattleObj_ScriptTick)(); }

// original 0x442C50 (PSX 0x801E1A88): a script tick; its answer into +0xB.
extern "C" void __cdecl BattleObj_CastDoneWait(void) {
    const unsigned char r = battle_obj_states::g.tick_once();
    S()[0xB] = r;
}

// original 0x442C70 (PSX 0x801E1AE8): BattleObj_StateTable[10] - jmp [BattleObj_State10Subs + 4
// * +2] (2: _State10Task, _State10End).
extern "C" void __cdecl BattleObj_State10(void) { Run("BattleObj_State10", AddressOf(BattleObj_State10Subs), 2, 2); }

// original 0x442C90 (PSX 0x801E1B2C): BattleTask_Create(0, 5); the slot's words +0x2E / +0x30 =
// Sprite_Current's +0x2E / +0x30 less the s8 pair at 0x6699A0 + 2 * the
// character Field_State +0x89 (each read again); +2 up by one.
extern "C" void __cdecl BattleObj_State10Task(void) {
    unsigned char* const t = TaskSlot("BattleObj_State10Task", BH_CALL(BattleTask_Create)(0, 5));
    const auto dx = static_cast<signed char>(move_script::At(::bof3::Img(battle_e3::at::kSpecialOffsets.v) + 2 * rv::As<records::PartyMember>(F())->char_id)[0]);
    rv::Store<unsigned>(rv::As<records::BattleTask>(t)->screen_x, static_cast<unsigned>(static_cast<std::uint16_t>(rv::As<records::PartyMember>(S())->screen_x) - dx) & 0xFFFF);
    const auto dy = static_cast<signed char>(move_script::At(::bof3::Img(battle_e3::at::kSpecialOffsets.v) + 1 + 2 * rv::As<records::PartyMember>(F())->char_id)[0]);
    rv::Store<unsigned>(rv::As<records::BattleTask>(t)->screen_y, static_cast<unsigned>(Word(S() + 0x30) - dy) & 0xFFFF);
    rv::As<records::PartyMember>(S())->step += 1;
}

// original 0x442D10 (PSX 0x801E1BDC): +1 = 2, +2 = 0.
extern "C" void __cdecl BattleObj_State10End(void) {
    rv::As<records::PartyMember>(S())->state = 2;
    rv::As<records::PartyMember>(S())->step = 0;
}

// original 0x442D30 (PSX 0x801E1BFC): BattleObj_StateTable[11] - BattleObj_HitPose; then by
// Field_State +0x130: bit 4 a tail jump to BattleObj_ScriptTick, else bit 5 to
// BattleObj_ScriptTickOnce.
extern "C" void __cdecl BattleObj_State11(void) {
    BH_CALL(BattleObj_HitPose)();
    const U flags = ULong(F() + 0x130);
    if (flags & 0x10)
        BH_CALL(BattleObj_ScriptTick)();
    else if (flags & 0x20)
        BH_CALL(BattleObj_ScriptTickOnce)();
}

// original 0x442D60 (PSX 0x801E1C5C): a tail jump through 0x64E134 by the
// sub-state +2 (0x442D80, 0x442DB0), unchecked.
extern "C" void __cdecl BattleObj_State12(void) { Dispatch(battle_obj_states::g.state12_subs, rv::As<records::PartyMember>(S())->step); }

// original 0x442D80 (PSX 0x801E1CA0): once File_LoadDone answers non-zero,
// the pose +8 + 0x24, a script tick, the next sub-state.
extern "C" void __cdecl BattleObj_State12Pose(void) {
    if (battle_obj_states::g.load_done() == 0) return;
    Pose(0x24);
    battle_obj_states::g.tick_once();
    rv::As<records::PartyMember>(S())->step = static_cast<unsigned char>(rv::As<records::PartyMember>(S())->step + 1);
}

// original 0x442DB0 (PSX 0x801E1D0C): once a script tick answers non-zero,
// state 8 with the sub-state 0.
extern "C" void __cdecl BattleObj_State12Wait(void) {
    if (battle_obj_states::g.tick_once() == 0) return;
    rv::As<records::PartyMember>(S())->state = 8;
    rv::As<records::PartyMember>(S())->step = 0;
}

// original 0x442DD0 (no PSX twin paired; reached only by the tail jumps of
// 0x4419D0 and of 0x442BA0's second chunk): back to state 2 with +2..+4
// zeroed; Battle_ClearActorBit(+5); bit 9 of Field_State +0x130 cleared; the
// action +0x125 cleared unless 0x904AA8 has bit 6.
extern "C" void __cdecl BattleObj_EndAction(void) {
    rv::As<records::PartyMember>(S())->state = 2;
    rv::As<records::PartyMember>(S())->step = 0;
    rv::As<records::PartyMember>(S())->state3 = 0;
    rv::As<records::PartyMember>(S())->state4 = 0;
    battle_obj_states::g.clear_actor_bit(rv::As<records::PartyMember>(S())->actor);
    auto* const member_f = rv::As<records::PartyMember>(F());
    member_f->flags_130 = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(member_f->flags_130)) & ~0x200u);
    if ((battle_obj_states::At(::bof3::Img(battle_obj_states::at::kRoundFlags.v))[0] & 0x40) == 0) F()[0x125] = 0;
}

// original 0x442E40 (PSX 0x801E1E00): BattleObj_StateTable[26] (the special attack: state 4's
// jump under +0x134 bit 0, and the character table's twelfth entry) - jmp
// [BattleObj_SpecialSubs + 4 * +2] (3).
extern "C" void __cdecl BattleObj_StateSpecial(void) { Run("BattleObj_StateSpecial", AddressOf(BattleObj_SpecialSubs), 3, 2); }

// original 0x442E60 (PSX 0x801E1E44): BattleTask_Create(0, 0xF); the slot's first 0x80 bytes
// copied from the acting member 0x904B34's object, then its +6 = 0, +5 = 0xF,
// +1..+4 = 0, +0xB = the slot, the owner +0x80 = Sprite_Current; Sprite_Current
// +0 |= 0x40, +9 = +0xA = 0x10, +2 up by one.
extern "C" void __cdecl BattleObj_SpecialStart(void) {
    const unsigned char slot = BH_CALL(BattleTask_Create)(0, 0xF);
    unsigned char* const t = TaskSlot("BattleObj_SpecialStart", slot);
    auto* const task_rec = rv::As<records::BattleTask>(t);
    const unsigned actor = B(::bof3::Img(battle_e3::at::kActor.v));
    if (actor >= battle_e3::at::kPartyMax)
        bof3::Fatal("BattleObj_SpecialStart: the actor 0x904B34 is %u, past ObjTrio's three members - the original copies "
                    "from past them (docs/battle_e3.md section 6)",
                    actor);
    std::memmove(t, Member(actor), 0x80);
    task_rec->runner = 0;
    task_rec->kind = 0xF;
    task_rec->state = 0;
    t[2] = 0;
    t[3] = 0;
    t[4] = 0;
    t[0xB] = slot;
    unsigned char* const s = S();
    auto* const member_s = rv::As<records::PartyMember>(s);
    SetPtrAt(Key(t + 0x80), s);
    member_s->flags |= 0x40;
    rv::As<records::PartyMember>(S())->step_frames = 0x10;
    S()[0xA] = 0x10;
    rv::As<records::PartyMember>(S())->step += 1;
}

// original 0x442F10 (PSX 0x801E1FA8): +9 down by one; at 0 the cue - 3 when
// Battle_RollPendingFlag answers, else Rand() % 100 (signed) below the member's
// +0xBA (read after Rand) sets 0x904AA8 bit 7 and cue 3, otherwise cue 2 -
// then cue 4, +2 up by one. BattleObj_SwingCue without its leading tick (the
// sibling's Battle_SwingCue_Step2 says the same of the PSX twin).
extern "C" void __cdecl BattleObj_SpecialCue(void) {
    rv::As<records::PartyMember>(S())->step_frames -= 1;
    if (rv::As<records::PartyMember>(S())->step_frames != 0) return;
    unsigned cue;
    if (BH_CALL(Battle_RollPendingFlag)() != 0) {
        cue = 3;
    } else {
        const int roll = static_cast<int>(BH_CALL(Rand)()) % 100;
        if (static_cast<int>(F()[0xBA]) > roll) {
            B(::bof3::Img(battle_e3::at::kRoundFlags.v)) |= 0x80;
            cue = 3;
        } else {
            cue = 2;
        }
    }
    BH_CALL(Battle_PlayActorCue)(cue);
    BH_CALL(Battle_PlayActorCue)(4);
    rv::As<records::PartyMember>(S())->step += 1;
}

// original 0x442F80 (PSX 0x801E20A4): once Sprite_Current +0 has lost bit 6 (the task clears
// it), 0x904AA8 |= 4 and a tail jump to BattleObj_EndAction.
extern "C" void __cdecl BattleObj_SpecialWait(void) {
    if (rv::As<records::PartyMember>(S())->flags & 0x40) return;
    B(::bof3::Img(battle_e3::at::kRoundFlags.v)) |= 4;
    BH_CALL(BattleObj_EndAction)();
}

void BattleObj_Inject() {
    BOF3_INJECT(BattleParty_RunStates);
    BOF3_INJECT(BattleParty_UpdateScreens);
    BOF3_INJECT(BattleObj_ScriptTick);
    BOF3_INJECT(BattleObj_ScriptTickOnce);
    BOF3_INJECT(BattleObj_RunState);
    BOF3_INJECT(BattleObj_StateInit);
    BOF3_INJECT(BattleObj_PickPose);
    BOF3_INJECT(BattleObj_HitPose);
    BOF3_INJECT(BattleObj_StateStand);
    BOF3_INJECT(BattleObj_StateIdle);
    BOF3_INJECT(BattleObj_IdleTintOn);
    BOF3_INJECT(BattleObj_IdleTintOff);
    BOF3_INJECT(BattleObj_StateAttack);
    BOF3_INJECT(BattleObj_AttackStart);
    BOF3_INJECT(BattleObj_StateSwing);
    BOF3_INJECT(BattleObj_SwingCue);
    BOF3_INJECT(BattleObj_SwingEnd);
    BOF3_INJECT(BattleObj_StateHit);
    BOF3_INJECT(BattleObj_HitEnter);
    BOF3_INJECT(BattleObj_HitReceiveDispatch);
    BOF3_INJECT(BattleObj_HitReceive);
    BOF3_INJECT(BattleObj_HitWaitPose);
    BOF3_INJECT(BattleObj_HitEnd);
    BOF3_INJECT(BattleObj_HitStepDispatch);
    BOF3_INJECT(BattleObj_HitStepStart);
    BOF3_INJECT(BattleObj_HitStepOut);
    BOF3_INJECT(BattleObj_HitStepBack);
    BOF3_INJECT(BattleObj_HitStepPose);
    BOF3_INJECT(BattleObj_FallDispatch);
    BOF3_INJECT(BattleObj_Fall);
    BOF3_INJECT(BattleParty_RecalcStats);
    BOF3_INJECT(BattleParty_AllDown);
    BOF3_INJECT(BattleObj_Revive);
    BOF3_INJECT(BattleObj_ReviveEnd);
    BOF3_INJECT(BattleObj_ReviveByEquip);
    BOF3_INJECT(BattleObj_FallTaskDispatch);
    BOF3_INJECT(BattleObj_FallTaskStart);
    BOF3_INJECT(BattleObj_FallTaskWait);
    BOF3_INJECT(BattleObj_HpDispatch);
    BOF3_INJECT(BattleObj_HpApply);
    BOF3_INJECT(BattleObj_HpEnd);
    BOF3_INJECT(BattleObj_StateCast);
    BOF3_INJECT(BattleObj_CastStart);
    BOF3_INJECT(BattleObj_CastCue);
    BOF3_INJECT(BattleObj_CastWait);
    BOF3_INJECT(BattleObj_StateCastDone);
    BOF3_INJECT(BattleObj_CastDoneTick);
    BOF3_INJECT(BattleObj_CastDone);
    BOF3_INJECT(BattleObj_CastDoneScript);
    BOF3_INJECT(BattleObj_CastDoneWait);
    BOF3_INJECT(BattleObj_State10);
    BOF3_INJECT(BattleObj_State10Task);
    BOF3_INJECT(BattleObj_State10End);
    BOF3_INJECT(BattleObj_State11);
    BOF3_INJECT(BattleObj_State12);
    BOF3_INJECT(BattleObj_State12Pose);
    BOF3_INJECT(BattleObj_State12Wait);
    BOF3_INJECT(BattleObj_EndAction);
    BOF3_INJECT(BattleObj_StateSpecial);
    BOF3_INJECT(BattleObj_SpecialStart);
    BOF3_INJECT(BattleObj_SpecialCue);
    BOF3_INJECT(BattleObj_SpecialWait);
}
