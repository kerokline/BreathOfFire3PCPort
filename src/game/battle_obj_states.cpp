// The battle party objects' state handlers: what BattleObj_RunState 0x4411E0
// reaches through the state table 0x64DFE0 by a party object's state byte
// +1 - the set-up (state 0), the stand and idle (2, 3), the attack by
// character (4 and its slots 15..25), the swing (5), the cast (7), its end
// (8), state 12 - with the sub-state handlers those reach through the tables
// after 0x64DFE0 by the byte +2, and the action's end 0x442DD0 two of them
// tail-jump to. docs/battle_obj_states.md.
//
// Sprite_Current and Field_State are both the party object being run
// (BattleParty_RunStates sets both); each is re-read wherever the original
// re-reads it. Every call goes through battle_obj_states::g
// (battle_obj_states_callees.h), so that the start-up fuzz can stand
// recorders in for them - for ours and for the originals' copies alike.
//
// Every handler is `void`: the original's eax on return reaches only
// BattleObj_RunState's tail jump, BattleParty_RunStates' tail jump and the
// battle frame Battle_Frame 0x42E370, whose next instruction after `call 0x441100`
// (0x42E39E) is another call; the sub-state calls of states 3 and 8 overwrite
// or ignore it. Everything here is a faithful replacement.
#include "game/battle_obj_states.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_obj_states_callees.h"
#include "game/move_script_bytes.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_obj_states {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
const std::uint32_t* Table(std::uint32_t address) { return reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Table(BattleObj_StateTable_addr + 0x34), Table(BattleObj_StateTable_addr + 0x3c), Table(BattleObj_StateTable_addr + 0x68), Table(BattleObj_SwingSubs_addr),
    Table(BattleObj_CastSubs_addr), Table(BattleObj_CastDoneSubs_addr), Table(BattleObj_State12Subs_addr),
    BattleObj_EndAction,
    BattleObj_PickPose, BattleObj_ScriptTick, BattleObj_ScriptTickOnce, AreaMap_Elevation, Sprite_EnsureAnimation,
    Sprite_SetTint, Tint_Release, Battle_RollPendingFlag, Rand, Battle_PlayActorCue, BattleTask_Create,
    Battle_SetTargetFlag40, File_LoadDone, Battle_LoadSoundByKey, Battle_ClearActorBit,
};
Callees g = kOriginals;

}  // namespace battle_obj_states

using namespace battle_obj_states;

namespace {

unsigned char* S() { return Sprite_Current; }
unsigned char* F() { return Field_State; }
unsigned char* Task(unsigned index) { return At(at::kTasks + index * at::kTaskSize); }
// The party objects' states run on the member: Sprite_Current and Field_State are its record.
records::PartyMember* SMember() { return reinterpret_cast<records::PartyMember*>(S()); }
records::PartyMember* FMember() { return reinterpret_cast<records::PartyMember*>(F()); }
records::BattleTask* TaskRec(unsigned index) { return reinterpret_cast<records::BattleTask*>(Task(index)); }
// A code pointer out of one of the tables, called; the index is not checked
// (a byte, as the original's).
void Dispatch(const std::uint32_t* table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}
void Pose(unsigned add) { g.ensure_animation(static_cast<unsigned char>(SMember()->direction + add)); }

}  // namespace

// ===========================================================================
// State 0: the set-up
// ===========================================================================

// original 0x441200 (PSX 0x801DEEC8, paired as Battle_InitMemberActor): the
// dwords +0xC..+0x20 zeroed, +0x29 = 4, the pose base +8 from 0x904AAC,
// +0x48 = 0, the ground height +0x3E = AreaMap_Elevation(+0x34, +0x38),
// +0x2B = 0, then the state 2 with +2..+4 zeroed.
extern "C" void __cdecl BattleObj_StateInit(void) {
    for (unsigned o = 0xC; o <= 0x20; o += 4) SetLong(S() + o, 0);
    SMember()->ot_slot = 4;
    SMember()->direction = At(at::kPoseBase)[0];
    S()[0x48] = 0;
    auto* const member_s = SMember();
    const long h = g.elevation(member_s->x, member_s->z);
    SetWord(S() + 0x3E, static_cast<unsigned>(h));
    S()[0x2B] = 0;
    SMember()->state[0] = 2;
    SMember()->state[1] = 0;
    SMember()->state[2] = 0;
    SMember()->state[3] = 0;
}

// ===========================================================================
// States 2 and 3: standing
// ===========================================================================

// original 0x441550 (PSX 0x801DF36C): the standing pose (BattleObj_PickPose),
// one script tick, then the next state.
extern "C" void __cdecl BattleObj_StateStand(void) {
    g.pick_pose();
    g.tick();
    SMember()->state[0] = static_cast<unsigned char>(SMember()->state[0] + 1);
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
    Dispatch(g.idle_subs, SMember()->state[1]);
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    bool phase5 = false;
    if (At(at::kPhase)[0] == 1) {
        if (BattleActions_Step > 1) {
            const unsigned member = BattleE4_AutoMember;
            const unsigned char c = FMember()->char_id == At(ObjTrio_addr + member * at::kMemberSize + 0x89)[0] ? 0x78 : 0xE2;
            S()[0x5F] = c;
            S()[0x5E] = c;
            S()[0x5D] = c;
            phase5 = At(at::kPhase)[0] == 5;
        }
    } else {
        phase5 = At(at::kPhase)[0] == 5;
    }
    unsigned char* const f = F();
    auto* const member_f = reinterpret_cast<records::PartyMember*>(f);
    if (phase5) {
        if ((Long(At(at::kRoundFlags)) & 0x8000) != 0 && (F()[0x134] & 2) != 0) {
            SetLong(S() + 0x34, static_cast<std::int32_t>(BossSi_CentreX));
            SetLong(S() + 0x38, static_cast<std::int32_t>(BossSi_CentreZ));
        } else {
            SMember()->x = TaskRec(SMember()->member)->x;
            SetLong(S() + 0x38, Long(Task(SMember()->member) + 0x38));
        }
    } else if ((f[0x90] & 0x20) != 0) {
        unsigned char* const s = S();
        auto* const member_s = reinterpret_cast<records::PartyMember*>(s);
        bool flip;
        if ((static_cast<std::int32_t>(member_f->flags_130) & 0x800) != 0) {
            Task(member_s->member)[9] = static_cast<unsigned char>(Task(member_s->member)[9] + 1);
            flip = static_cast<signed char>(Task(member_s->member)[9]) >= 12;
        } else {
            Task(member_s->member)[9] = static_cast<unsigned char>(Task(member_s->member)[9] - 1);
            flip = static_cast<signed char>(Task(member_s->member)[9]) <= -12;
        }
        records::PartyMember* member_p = reinterpret_cast<records::PartyMember*>(s);
        if (flip) {
            member_f->flags_130 = static_cast<std::int32_t>(member_f->flags_130) ^ 0x800;
            member_p = reinterpret_cast<records::PartyMember*>(S());
        }
        const unsigned pose = member_p->direction;
        unsigned char* const t = Task(member_p->member);
        auto* const task_rec = reinterpret_cast<records::BattleTask*>(t);
        const std::int32_t step = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<signed char>(t[9])) << 9);
        if (pose == 1 || pose == 3) member_p->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(step) + static_cast<std::uint32_t>(task_rec->x));
        else member_p->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(step) + static_cast<std::uint32_t>(Long(t + 0x38)));
    } else {
        if ((static_cast<std::int32_t>(member_f->flags_130) & 0x1000) == 0) g.tick();
        return;
    }
    if ((static_cast<std::int32_t>(FMember()->flags_130) & 0x1000) == 0) g.tick();
}

// original 0x4417A0 (PSX 0x801DF71C), state 3's sub-state 0 (also slot 13 of
// 0x64DFE0): while 0x904AAF is set and the target byte (*0x939FA0) is this
// object's actor +5 or has bit 7 (all), the object is tinted -
// Sprite_SetTint(Sprite_Current, 0, 0, 0, 0) into +7 - and the sub-state
// becomes 1.
extern "C" void __cdecl BattleObj_IdleTintOn(void) {
    if (BattleE4_Picking == 0) return;
    unsigned char* const s = S();
    auto* const member_s = reinterpret_cast<records::PartyMember*>(s);
    const unsigned char target = At(BattleActorCopies_Command)[0];
    if (target != member_s->member && (target & 0x80) == 0) return;
    const unsigned char r = g.set_tint(s, 0, 0, 0, 0);
    S()[7] = r;
    SMember()->state[1] = 1;
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
    auto* const member_s = reinterpret_cast<records::PartyMember*>(s);
    const unsigned char level = At(at::kTintLevel)[0];
    At(MoveScript_TintRecords_addr + 4 + s[7] * 12u)[0] = level;
    At(MoveScript_TintRecords_addr + 3 + s[7] * 12u)[0] = level;
    At(MoveScript_TintRecords_addr + 2 + s[7] * 12u)[0] = level;
    if (BattleE4_Picking != 0) {
        const unsigned char target = At(BattleActorCopies_Command)[0];
        if (target == member_s->member) return;
        if (target & 0x80) return;
    }
    g.tint_release(s[7]);
    SMember()->state[1] = 0;
}

// ===========================================================================
// State 4: the attack, by character
// ===========================================================================

// original 0x441860 (PSX 0x801DF860, paired as Attack_DispatchByCharacter): a
// tail jump through the dword 0x64E048 while Field_State +0x134 has bit 0,
// else through 0x64E01C by the character +0x89 (slots 15.. of 0x64DFE0 -
// 0x441890 for the first eleven, 0x442E40 for the twelfth).
//
// As the original has it: the character is not checked.
extern "C" void __cdecl BattleObj_StateAttack(void) {
    unsigned char* const f = F();
    auto* const member_f = reinterpret_cast<records::PartyMember*>(f);
    if (f[0x134] & 1) {
        Dispatch(g.attack_special, 0);
        return;
    }
    Dispatch(g.attack_by_char, member_f->char_id);
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
        SMember()->step_frames = At(at::kAttackCountBy + BattlePhases_B89)[0];
        S()[0xA] = At(at::kAttackCountBy + BattlePhases_B89)[0];
    } else {
        SMember()->step_frames = At(at::kAttackCount + FMember()->char_id)[0];
        S()[0xA] = At(at::kAttackCount + FMember()->char_id)[0];
    }
    Pose(0xC);
    g.tick();
    SMember()->state[0] = static_cast<unsigned char>(SMember()->state[0] + 1);
}

// ===========================================================================
// State 5: the swing
// ===========================================================================

// original 0x441930 (PSX 0x801DF9D0): a tail jump through 0x64E074 by the
// sub-state +2 (0x441950, 0x4419D0), unchecked.
extern "C" void __cdecl BattleObj_StateSwing(void) { Dispatch(g.swing_subs, SMember()->state[1]); }

// original 0x441950 (PSX 0x801DFA14, paired as Battle_SwingCue_Step): one
// script tick; the counter +9 down by one; at 0 the cue: 3 when
// Battle_RollPendingFlag answers non-zero, else Rand() % 100 against
// Field_State +0xBA - below it 0x904AA8 |= 0x80 and cue 3, else cue 2; then
// cue 4, and the next sub-state.
//
// As the original has it: the remainder is the signed idiv's, compared signed
// with the unsigned byte; Field_State is read after Rand.
extern "C" void __cdecl BattleObj_SwingCue(void) {
    g.tick_once();
    SMember()->step_frames = static_cast<unsigned char>(SMember()->step_frames - 1);
    if (SMember()->step_frames != 0) return;
    unsigned cue;
    if (g.roll_pending() != 0) {
        cue = 3;
    } else {
        const int roll = g.rand_() % 100;
        if (static_cast<int>(F()[0xBA]) > roll) {
            At(at::kRoundFlags)[0] = static_cast<unsigned char>(At(at::kRoundFlags)[0] | 0x80);
            cue = 3;
        } else {
            cue = 2;
        }
    }
    g.play_cue(cue);
    g.play_cue(4);
    SMember()->state[1] = static_cast<unsigned char>(SMember()->state[1] + 1);
}

// original 0x4419D0 (PSX 0x801DFB18): once a script tick answers non-zero -
// with 0x904AA8 bit 7 (the roll above), BattleTask_Create(0, 2); then
// Battle_SetTargetFlag40(0x904B44), 0x904AA8 |= 4, and the action's end
// (a tail jump to 0x442DD0).
extern "C" void __cdecl BattleObj_SwingEnd(void) {
    if (g.tick_once() == 0) return;
    if (At(at::kRoundFlags)[0] & 0x80) g.task_create(0, 2);
    g.set_target_flag(At(at::kSwingTarget)[0]);
    At(at::kRoundFlags)[0] = static_cast<unsigned char>(At(at::kRoundFlags)[0] | 4);
    g.end_action();
}

// ===========================================================================
// State 7: the cast
// ===========================================================================

// original 0x4429E0 (PSX 0x801E1604): a tail jump through 0x64E0E4 by the
// sub-state +2 (0x442A00, 0x442B20, 0x442B80), unchecked.
extern "C" void __cdecl BattleObj_StateCast(void) { Dispatch(g.cast_subs, SMember()->state[1]); }

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
    if (g.load_done() == 0) return;
    unsigned char* const f = F();
    auto* const member_f = reinterpret_cast<records::PartyMember*>(f);
    if (f[0x125] == 4) {
        if ((f[0x134] & 2) == 0) {
            const unsigned char set = BattleE3_SoundSet;
            SMember()->step_frames = g.load_sound(member_f->char_id, set) != 0 ? 0 : 0x1E;
        } else {
            const unsigned char kind = BattlePhases_B89;
            if (kind == 4 || kind == 7 || kind == 8) {
                const unsigned char set = BattleE3_SoundSet;
                if (g.load_sound(member_f->char_id, set) != 0) SMember()->step_frames = 0;
            }
        }
    } else {
        SMember()->step_frames = 0;
    }
    const unsigned skill = Word(At(at::kSkillId));
    if ((At(at::kSkillWord + 1 + skill * 24)[0] & 8) == 0) {
        const unsigned char count = (F()[0x134] & 2) ? At(at::kCastCountBy + BattlePhases_B89)[0]
                                                     : At(at::kCastCount + FMember()->char_id)[0];
        SMember()->step_frames = count;
        Pose(0x2C);
        SMember()->state[1] = static_cast<unsigned char>(SMember()->state[1] + 1);
    } else {
        SMember()->state[0] = static_cast<unsigned char>(SMember()->state[0] + 1);
    }
    g.tick_once();
}

// original 0x442B20 (PSX 0x801E1814): the counter +9 down by one while it is
// not 0; at 0 cue 5, and cue 3 as well while Field_State +0x134 has bit 1 and
// 0x904B89 is none of 4, 7, 8; then the next sub-state. Either way a script
// tick (a tail jump).
extern "C" void __cdecl BattleObj_CastCue(void) {
    auto* const member_s = SMember();
    const unsigned char n = member_s->step_frames;
    if (n != 0) {
        member_s->step_frames = static_cast<unsigned char>(n - 1);
    } else {
        g.play_cue(5);
        if (F()[0x134] & 2) {
            const unsigned char kind = BattlePhases_B89;
            if (kind != 4 && kind != 7 && kind != 8) g.play_cue(3);
        }
        SMember()->state[1] = static_cast<unsigned char>(SMember()->state[1] + 1);
    }
    g.tick_once();
}

// original 0x442B80 (PSX 0x801E18C4): once a script tick answers non-zero,
// the next state and the sub-state 0.
extern "C" void __cdecl BattleObj_CastWait(void) {
    if (g.tick_once() == 0) return;
    auto* const member_s = SMember();
    member_s->state[0] = static_cast<unsigned char>(member_s->state[0] + 1);
    SMember()->state[1] = 0;
}

// ===========================================================================
// State 8: the cast's end
// ===========================================================================

// original 0x442BA0 (PSX 0x801E191C): the sub-state +2's handler through
// 0x64E118 (0x442BC0, 0x442BD0, the bare ret 0x437CC0, 0x442C40, 0x442C50),
// unchecked; then, when 0x904AA8 has bit 2, the action's end (from a second
// chunk, 0x442C60, a tail jump to 0x442DD0).
extern "C" void __cdecl BattleObj_StateCastDone(void) {
    Dispatch(g.cast_done_subs, SMember()->state[1]);
    if (At(at::kRoundFlags)[0] & 4) g.end_action();
}

// original 0x442BC0 (PSX 0x801E1968): a script tick, the next sub-state.
extern "C" void __cdecl BattleObj_CastDoneTick(void) {
    g.tick_once();
    SMember()->state[1] = static_cast<unsigned char>(SMember()->state[1] + 1);
}

// original 0x442BD0 (PSX 0x801E19A0, paired as Actor_SkillItemDone): for the
// action 4, Field_State's u16 +0x9A less 0x904B88; 0x904AA8 |= 0x800; the
// action cleared; then, when the skill 0x904B80's record has bit 11 of its
// u16 0x65C4DC, the next sub-state (2, the bare ret), else a script tick and
// the sub-state 4.
extern "C" void __cdecl BattleObj_CastDone(void) {
    unsigned char* f = F();
    records::PartyMember* member_f = reinterpret_cast<records::PartyMember*>(f);
    if (f[0x125] == 4) {
        member_f->ap = member_f->ap - BattleE3_Cost;
        f = F();
        member_f = reinterpret_cast<records::PartyMember*>(f);
    }
    SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) | 0x800);
    f[0x125] = 0;
    const unsigned skill = Word(At(at::kSkillId));
    if (Word(At(at::kSkillWord + skill * 24)) & 0x800) {
        SMember()->state[1] = static_cast<unsigned char>(SMember()->state[1] + 1);
        return;
    }
    g.tick_once();
    SMember()->state[1] = 4;
}

// original 0x442C50 (PSX 0x801E1A88): a script tick; its answer into +0xB.
extern "C" void __cdecl BattleObj_CastDoneWait(void) {
    const unsigned char r = g.tick_once();
    S()[0xB] = r;
}

// ===========================================================================
// State 12
// ===========================================================================

// original 0x442D60 (PSX 0x801E1C5C): a tail jump through 0x64E134 by the
// sub-state +2 (0x442D80, 0x442DB0), unchecked.
extern "C" void __cdecl BattleObj_State12(void) { Dispatch(g.state12_subs, SMember()->state[1]); }

// original 0x442D80 (PSX 0x801E1CA0): once File_LoadDone answers non-zero,
// the pose +8 + 0x24, a script tick, the next sub-state.
extern "C" void __cdecl BattleObj_State12Pose(void) {
    if (g.load_done() == 0) return;
    Pose(0x24);
    g.tick_once();
    SMember()->state[1] = static_cast<unsigned char>(SMember()->state[1] + 1);
}

// original 0x442DB0 (PSX 0x801E1D0C): once a script tick answers non-zero,
// state 8 with the sub-state 0.
extern "C" void __cdecl BattleObj_State12Wait(void) {
    if (g.tick_once() == 0) return;
    SMember()->state[0] = 8;
    SMember()->state[1] = 0;
}

// ===========================================================================
// The action's end
// ===========================================================================

// original 0x442DD0 (no PSX twin paired; reached only by the tail jumps of
// 0x4419D0 and of 0x442BA0's second chunk): back to state 2 with +2..+4
// zeroed; Battle_ClearActorBit(+5); bit 9 of Field_State +0x130 cleared; the
// action +0x125 cleared unless 0x904AA8 has bit 6.
extern "C" void __cdecl BattleObj_EndAction(void) {
    SMember()->state[0] = 2;
    SMember()->state[1] = 0;
    SMember()->state[2] = 0;
    SMember()->state[3] = 0;
    g.clear_actor_bit(SMember()->member);
    auto* const member_f = FMember();
    member_f->flags_130 = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(member_f->flags_130)) & ~0x200u);
    if ((At(at::kRoundFlags)[0] & 0x40) == 0) F()[0x125] = 0;
}

// ===========================================================================

void BattleObjStates_Inject() {
    if (bof3::WantsShadow("battle_obj_states")) battle_obj_states::SelfTest();
    BOF3_INJECT(BattleObj_StateInit);
    BOF3_INJECT(BattleObj_StateStand);
    BOF3_INJECT(BattleObj_StateIdle);
    BOF3_INJECT(BattleObj_IdleTintOn);
    BOF3_INJECT(BattleObj_IdleTintOff);
    BOF3_INJECT(BattleObj_StateAttack);
    BOF3_INJECT(BattleObj_AttackStart);
    BOF3_INJECT(BattleObj_StateSwing);
    BOF3_INJECT(BattleObj_SwingCue);
    BOF3_INJECT(BattleObj_SwingEnd);
    BOF3_INJECT(BattleObj_StateCast);
    BOF3_INJECT(BattleObj_CastStart);
    BOF3_INJECT(BattleObj_CastCue);
    BOF3_INJECT(BattleObj_CastWait);
    BOF3_INJECT(BattleObj_StateCastDone);
    BOF3_INJECT(BattleObj_CastDoneTick);
    BOF3_INJECT(BattleObj_CastDone);
    BOF3_INJECT(BattleObj_CastDoneWait);
    BOF3_INJECT(BattleObj_State12);
    BOF3_INJECT(BattleObj_State12Pose);
    BOF3_INJECT(BattleObj_State12Wait);
    BOF3_INJECT(BattleObj_EndAction);
}
