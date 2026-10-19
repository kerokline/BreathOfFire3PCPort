// The per-turn steps: group CC of the eighth round, twenty functions of the
// battle engine (PC 0x4302B0..0x43192F, the PSX's BATTLE.EMI compiled into
// the exe), each read whole. docs/battle_turn_steps.md.
//
// Battle_PhaseDispatch (battle_flow.cpp) calls the phase's entry of its
// six-entry table; phases 4 and 5 are 0x4302B0 and 0x4311E0, dispatch stubs
// `xor eax, eax; mov al, [state]; jmp [table + eax * 4]` over tables in .data.
// The steps they reach take no argument and answer nothing any caller reads.
//
//   - Phase 4, the round's end (BattleRoundEnd_*): by 0x904AA1, the faster
//     side's extra round (0x4302C0 over BattleRoundEnd_FasterSteps), the
//     status chain 0x430510 that runs the nine BattleSteps, and 0x431090,
//     which waits for the round's busy words, then starts the next round
//     (phase 1) or, once 0x904AE8 is set, the battle's end (phase 5).
//   - Phase 5, the battle's end (BattleEnd_*): by 0x904AA1, the members'
//     tasks (0x4311F0), the win (0x4314B0, whose third step 0x431910 is the
//     result through 0x431920 and group CD's pages), the other way out (0x431540 and
//     0x4315B0, not reached by the combat route, left Capcom's), and the
//     exit (0x431760: Battle_WriteBackParty, the exit hook, the finish).
//
// Every call goes through battle_turn_steps::g (battle_turn_steps_callees.h),
// so that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. Everything here is a faithful replacement.
#include "game/battle_turn_steps.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_turn_steps_callees.h"
#include "game/move_script_bytes.h"
#include "game/records.h"
#include "game/psp_code.h"
#include "hook/detour.h"

namespace battle_turn_steps {

namespace {
template <typename F> F Fn(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Fn<unsigned (__cdecl*)()>(bof3::addr::Battle_MarkFasterSide),
    Fn<void (__cdecl*)()>(bof3::addr::Battle_TickCounters),
    {Fn<unsigned (__cdecl*)()>(bof3::addr::BattleStep_Expire4000), Fn<unsigned (__cdecl*)()>(bof3::addr::BattleStep_Restore800),
     Fn<unsigned (__cdecl*)()>(bof3::addr::BattleStep_PartyWake40), Fn<unsigned (__cdecl*)()>(bof3::addr::BattleStep_EnemyWake40),
     Fn<unsigned (__cdecl*)()>(bof3::addr::BattleStep_PartyWake20), Fn<unsigned (__cdecl*)()>(bof3::addr::BattleStep_EnemyWake20),
     Fn<unsigned (__cdecl*)()>(bof3::addr::BattleStep_Status80), Fn<unsigned (__cdecl*)()>(bof3::addr::BattleStep_HpDrift),
     Fn<unsigned (__cdecl*)()>(bof3::addr::BattleStep_ApUpkeep)},
    Fn<void (__cdecl*)(int)>(bof3::addr::LoadDatFile),
    Fn<void (__cdecl*)()>(bof3::addr::Battle_ClearCommands),
    Fn<unsigned (__cdecl*)(unsigned)>(bof3::addr::Battle_ActorIsOut),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(bof3::addr::BattleTask_Create),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(bof3::addr::Battle_ClearStatus),
    Fn<void (__cdecl*)(unsigned char*)>(bof3::addr::Sprite_ReleaseTint),
    Fn<void (__cdecl*)(unsigned)>(bof3::addr::Battle_ReturnQueuedItem),
    Fn<void (__cdecl*)(unsigned)>(bof3::addr::Port_DroppedCall),  // Port_DroppedCall (ours; called by its address)
    Fn<void (__cdecl*)(int)>(bof3::addr::Music_FadeOutStop),
    Fn<void (__cdecl*)(unsigned, int)>(bof3::addr::Music_Play),
    Fn<void (__cdecl*)()>(bof3::addr::Battle_OpenMsgWindow),
    Fn<void (__cdecl*)()>(bof3::addr::Battle_OpeningMessage),
    Fn<int (__cdecl*)()>(bof3::addr::File_LoadDone),
    Fn<void (__cdecl*)(unsigned)>(kWriteBackMember),
    Fn<void (__cdecl*)()>(bof3::addr::Window_ResetAll),
    Fn<void (__cdecl*)()>(kClearEnemies),
    Fn<void (__cdecl*)()>(bof3::addr::BattleTask_ClearAll),
    Fn<void (__cdecl*)(unsigned char*)>(bof3::addr::Char_RecalcStats),
    Fn<void (__cdecl*)()>(kReloadParty),
};
Callees g = kOriginals;

}  // namespace battle_turn_steps

using namespace battle_turn_steps;

namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* Member(unsigned i) { return At(ObjTrio_addr + (i & 0xFF) * at::kPartyStride); }
records::PartyMember* MemberRec(unsigned i) { return reinterpret_cast<records::PartyMember*>(Member(i)); }
bool Yes(unsigned al) { return (al & 0xFFu) != 0; }
unsigned char& B(std::uint32_t address) { return *At(address); }
// `inc byte ptr [m]` / `dec byte ptr [m]`: read, step, store, as one.
void Inc(std::uint32_t address) { ++*At(address); }

// A dispatch stub's jump: entry `index` of the table in .data, read as the
// original reads it. The index is not checked, as the original's is not: every
// state byte of the combat route stays inside its table, and one past it
// reads the next table's first entry, the same code either way.
Step Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Step>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(table + index * 4)))));
}

}  // namespace

// ===========================================================================
// Phase 4: the round's end
// ===========================================================================

// original 0x4302B0: phase 4's dispatch - entry 0x904AA1 of
// BattleRoundEnd_Steps (0x64AF2C: BattleRoundEnd_FasterStep,
// BattleRoundEnd_StatusChain, BattleRoundEnd_NextRound). A jump, as the
// original's: the step returns to Battle_PhaseDispatch.
extern "C" void __cdecl BattleRoundEnd_Step() {
    [[clang::musttail]] return Entry(BattleRoundEnd_Steps_addr, BattleActions_Step)();
}

// original 0x4302C0: entry 0x904AA2 of BattleRoundEnd_FasterSteps (0x64AF38:
// BattleRoundEnd_CheckFaster, BattleRoundEnd_FasterNext,
// BattleRoundEnd_NextRound).
extern "C" void __cdecl BattleRoundEnd_FasterStep() {
    [[clang::musttail]] return Entry(BattleRoundEnd_FasterSteps_addr, BattleE4_Step2)();
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
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleRoundEnd_CheckFaster() {
    if (BattlePhases_B7A == 0 && BattleFlow_AnimGate == 0) {
        switch (g.mark_faster() & 0xFF) {
        case 1:
            if (BattlePhases_Initiative != 2) {
                BattlePhases_B7A = 1;
                Inc(BattleE4_Step2_addr);
                return;
            }
            Inc(BattleE4_Step2_addr); // then the round ends, which zeroes it after Battle_TickCounters
            break;
        case 2:
            BattlePhases_B7A = 2;
            Inc(BattleE4_Step2_addr);
            return;
        case 3:
            BattlePhases_B7A = BattlePhases_Initiative != 2 ? 3 : 2;
            Inc(BattleE4_Step2_addr);
            return;
        default:
            break;
        }
    }
    for (unsigned i = 0; i < 2; ++i) Member(i)[at::kFlags + 1] &= 0x7F;
    for (unsigned i = 0; i < 7; ++i) At(at::kEnemy + i * at::kEnemyStride)[at::kEFlags + 1] &= 0x7F;
    SetLong(At(at::kRounds), static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(At(at::kRounds))) + 1));
    BattlePhases_B7A = 0;
    g.tick_counters();
    BattleE4_Step2 = 0;
    Inc(BattleActions_Step_addr);
}

// original 0x430500: `inc byte ptr [0x904AA2]` - the extra round's frame
// between the check and BattleRoundEnd_NextRound.
extern "C" void __cdecl BattleRoundEnd_FasterNext() { Inc(BattleE4_Step2_addr); }

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
        Inc(BattleE4_Step2_addr);
        if (Yes(g.steps[0]())) return;
        Inc(BattleE4_Step2_addr);
        [[fallthrough]];
    case 2:
        Inc(BattleE4_Step2_addr);
        if (Yes(g.steps[1]())) return;
        [[fallthrough]];
    case 3:
        Inc(BattleE4_Step2_addr);
        if (Yes(g.steps[2]())) return;
        [[fallthrough]];
    case 4:
        Inc(BattleE4_Step2_addr);
        if (Yes(g.steps[3]())) return;
        [[fallthrough]];
    case 5:
        Inc(BattleE4_Step2_addr);
        if (Yes(g.steps[4]())) return;
        [[fallthrough]];
    case 6:
        Inc(BattleE4_Step2_addr);
        if (Yes(g.steps[5]())) return;
        [[fallthrough]];
    case 7:
        Inc(BattleE4_Step2_addr);
        if (Yes(g.steps[6]())) return;
        Inc(BattleE4_Step2_addr);
        [[fallthrough]];
    case 9:
        Inc(BattleE4_Step2_addr);
        if (Yes(g.steps[7]())) return;
        Inc(BattleE4_Step2_addr);
        [[fallthrough]];
    case 11:
        Inc(BattleE4_Step2_addr);
        if (Yes(g.steps[8]())) return;
        [[fallthrough]];
    case 13:
        BattleE4_Step2 = 0;
        Inc(BattleActions_Step_addr);
        return;
    case 1:
    case 8:
    case 10:
    case 12:
        if (BattleActions_Pending != 0) return;
        B(at::kRoundFlags) &= 0xFB;
        Inc(BattleE4_Step2_addr);
        return;
    default:
        return;
    }
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
        if (Long(At(0x803500 + i * 4)) != 0) return;
    if (BattleActions_EventBattle != 0) {
        const auto hook = reinterpret_cast<unsigned (__cdecl*)(int)>(
            BattleHook_Event);
        if ((hook(2) & 0xFF) == 0xFF) return;
    }
    if (BattleActions_BattleEnd != 0 && !psp_code::DuelLossGoesOn(BattleActions_EventBattle, BattleActions_BattleEnd)) {
        B(at::kRoundFlags) &= 0xEF;
        for (unsigned w : {0u, 3u, 1u, 11u, 10u, 9u, 8u, 7u, 6u, 5u, 4u}) B(at::kWindows + w * at::kWindowStride) = 0;
        B(at::kPhase) = 5;
        BattleActions_Step = 0;
        return;
    }
    if ((B(at::kRoundFlags) & 0x10) == 0) {
        SetLong(At(at::kCmdLong), 0);
        B(at::kCmdKind) = 8;
        SetWord(At(at::kCmdWord), 0);
        B(at::kCmdByte) = 0;
        g.load_dat(0x131);
    }
    g.clear_commands();
    for (unsigned i = 0; i <= 2; ++i)
        if (!Yes(g.actor_is_out(i))) MemberRec(i)->state[0] = 2;
    B(at::kRoundFlags) &= 0xF7;
    BattlePhases_Initiative = 0;
    B(at::kPhase) = 1;
    BattleActions_Step = 0;
    BattleE4_Step2 = 0;
}

// ===========================================================================
// Phase 5: the battle's end
// ===========================================================================

// original 0x4311E0: phase 5's dispatch - entry 0x904AA1 of BattleEnd_Steps
// (0x64AF44: BattleEnd_TaskStep, BattleEnd_WinStep, BattleEnd_LossDispatch 0x431540 and BattleEnd_RestoreDispatch 0x4315B0
// (the other way out, Capcom's), BattleEnd_ExitStep).
extern "C" void __cdecl BattleEnd_Step() {
    [[clang::musttail]] return Entry(BattleEnd_Steps_addr, BattleActions_Step)();
}

// original 0x4311F0: entry 0x904AA2 of BattleEnd_TaskSteps (0x64AF58:
// BattleEnd_TasksBegin, BattleEnd_StartMemberTask, BattleEnd_AwaitMemberTasks).
extern "C" void __cdecl BattleEnd_TaskStep() {
    [[clang::musttail]] return Entry(BattleEnd_TaskSteps_addr, BattleE4_Step2)();
}

// original 0x431200: 0x802D20 and the member cursor 0x904AA5 zeroed, 0x904AA2
// stepped on. Also the other way out's first step (0x64AF7C).
extern "C" void __cdecl BattleEnd_TasksBegin() {
    BossSa_WindowPass = 0;
    BossSb_Countdown = 0;
    Inc(BattleE4_Step2_addr);
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
    if (cursor < B(at::kMembers)) {
        for (;;) {
            const unsigned out = g.actor_is_out(cursor);
            cursor = BossSb_Countdown;
            if (!Yes(out) && (Member(cursor)[at::kFlags] & 3) != 0) {
                Member(cursor)[at::kFlags2 + 1] |= 0x20;
                unsigned parameter = 0xB;
                if (MemberRec(cursor)->char_id != 4) {
                    for (unsigned k = 0; k < 3; ++k) At(at::kCopies + k * at::kPartyStride)[at::kFlags] &= 0xFE;
                    parameter = 0xC;
                }
                const unsigned task = g.task_create(0, parameter) & 0xFF;
                SetLong(At(at::kTasks + task * at::kTaskSize + 0x80),
                        static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(Member(BossSb_Countdown))));
                Inc(BossSb_Countdown_addr);
                break;
            }
            const unsigned count = B(at::kMembers);
            cursor = (cursor + 1) & 0xFF;
            BossSb_Countdown = static_cast<unsigned char>(cursor);
            if (cursor >= count) break;
        }
    }
    Inc(BattleE4_Step2_addr);
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
    const unsigned count = B(at::kMembers);
    for (unsigned i = 0; i < count; ++i)
        if (Long(Member(i) + at::kFlags2) & 0x2000) return;
    if (BossSb_Countdown != count) {
        --*At(BattleE4_Step2_addr);
        return;
    }
    unsigned end = BattleActions_BattleEnd;
    if ((end & 1) == 0) {
        if (count != 0) {
            for (unsigned i = 0;;) {
                unsigned char* const m = Member(i);
                auto* const member_rec = reinterpret_cast<records::PartyMember*>(m);
                Sprite_Current = m;
                if (m[at::kStatus + 1] & 8) {
                    member_rec->hp = member_rec->max_hp;
                    member_rec->ap = member_rec->max_ap;
                    member_rec->survive = member_rec->byte_ae;
                    g.clear_status(i, member_rec->status & 0xBFFF);
                }
                g.clear_status(i, member_rec->status & 0xBF5F);
                g.release_tint(m);
                g.return_item(i);
                i = (i + 1) & 0xFF;
                if (i >= B(at::kMembers)) break;
            }
            end = BattleActions_BattleEnd;
        }
        BattleFlow_AnimGate = 0;
    }
    if (BattleActions_EventBattle == 0) {
        if (end & 1) BattleActions_Step = 2;
        if (end & 2) BattleActions_Step = 1;
    }
    reinterpret_cast<Step>(BattleHook_End)();
    g.dropped_call(0);
    if (BattleActions_Step == 1) g.load_dat(0xCD);
    if (BattleActions_Step == 2) g.load_dat(0xCE);
    BattleE4_Step2 = 0;
}

// original 0x4314B0: the win's dispatch - entry 0x904AA2 of BattleEnd_WinSteps
// (0x64AF64: BattleEnd_WinBegin, BattleEnd_WinAwaitInput, BattleEnd_ResultStep).
extern "C" void __cdecl BattleEnd_WinStep() {
    [[clang::musttail]] return Entry(BattleEnd_WinSteps_addr, BattleE4_Step2)();
}

// original 0x4314C0: each of the three members present (+0 bit 0) to state 2;
// unless 0x904AE5 bit 0x40 keeps the battle's music, Music_FadeOutStop(10)
// and Music_Play(0xA5, 10); Battle_OpenMsgWindow, Battle_OpeningMessage (its
// lines by 0x904B90 - named for the opening, but here it is the win's), and
// 0x904AA2 on.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_WinBegin() {
    for (unsigned i = 0; i < 3; ++i)
        if (MemberRec(i)->flags & 1) MemberRec(i)->state[0] = 2;
    if ((BattleTurnSteps_MusicFlags & 0x40) == 0) {
        g.fade_out(10);
        g.music_play(0xA5, 10);
    }
    g.open_window();
    g.opening_message();
    Inc(BattleE4_Step2_addr);
}

// original 0x431520: 0x904AA2 on once File_LoadDone answers (it always does)
// and Input_Pressed is not 0.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_WinAwaitInput() {
    if (g.load_done() != 0 && Input_Pressed != 0) Inc(BattleE4_Step2_addr);
}

// original 0x431910: entry 0x904AA3 of BattleEnd_ResultSteps (0x64AFA0:
// BattleEnd_ResultPage, then group CD's 0x431B60 and 0x431D50).
extern "C" void __cdecl BattleEnd_ResultStep() {
    [[clang::musttail]] return Entry(BattleEnd_ResultSteps_addr, BattleE4_Step3)();
}

// original 0x431920: entry (dword 0x904AA4 & 0xFF) of BattleResult_ExpSteps
// (0x64AFAC: group CD's 0x431940, 0x431A20, 0x431A90, 0x431AB0, 0x431B30).
extern "C" void __cdecl BattleEnd_ResultPage() {
    [[clang::musttail]] return Entry(BattleResult_ExpSteps_addr, BattleE4_Step4)();
}

// original 0x431760: the exit's dispatch - entry 0x904AA2 of
// BattleEnd_ExitSteps (0x64AF90: Battle_WriteBackParty, BattleEnd_ExitHook,
// BattleEnd_Finish, BattleEnd_ExitAwaitFade 0x4318F0 (R3A's)).
extern "C" void __cdecl BattleEnd_ExitStep() {
    [[clang::musttail]] return Entry(BattleEnd_ExitSteps_addr, BattleE4_Step2)();
}

// original 0x431770 (PSX Battle_WriteBackParty 0x801D71B0, the sibling's
// name: Battle_WriteBackMember(0..2), 0x8015990C, 0x801629CC(0x256), the
// state on): 0x446A80(0), (1), (2), Window_ResetAll, LoadDatFile(2), 0x904AA2
// on.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Battle_WriteBackParty() {
    g.write_back_member(0);
    g.write_back_member(1);
    g.write_back_member(2);
    g.window_reset();
    g.load_dat(2);
    Inc(BattleE4_Step2_addr);
}

// original 0x4317B0: once File_LoadDone answers - the hook 0x904B68; unless
// 0x904AE5 bit 0x40, Music_FadeOutStop(10) and, when Music_Track is then
// 0xFF, 0x904AE8 |= 8 - 0x904AA2 on.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_ExitHook() {
    if (g.load_done() == 0) return;
    reinterpret_cast<Step>(BattleHook_Exit)();
    if ((BattleTurnSteps_MusicFlags & 0x40) == 0) {
        g.fade_out(10);
        if (Music_Track == 0xFF) BattleActions_BattleEnd |= 8;
    }
    Inc(BattleE4_Step2_addr);
}

// original 0x4317F0 (PSX 0x801D7320): 0x494E70, BattleTask_ClearAll; each
// member (to the count 0x904AB0, re-read after each) with status 0x4000 (+0x91
// bit 6): its CharacterRecord (+0x148) gets HP 1, +0x1C = 1, +0x1E up by one
// while below 5, status 0x4000 and +0xB bit 2 cleared, and Char_RecalcStats.
// Then 0x446600, Game_Step up by one, 0x904AE9 and the five state bytes
// 0x904AA0..0x904AA4 zeroed.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_Finish() {
    g.clear_enemies();
    g.task_clear_all();
    if (B(at::kMembers) != 0) {
        for (unsigned i = 0;;) {
            const unsigned char* const m = Member(i);
            auto* const member_rec = reinterpret_cast<const records::PartyMember*>(m);
            if (m[at::kStatus + 1] & 0x40) {
                unsigned char* const r = At(at::kRecords + member_rec->char_index * at::kRecordStride);
                SetWord(r + 0x18, 1);
                r[0x1C] = 1;
                if (r[0x1E] < 5) ++r[0x1E];
                SetWord(r + 0x10, Word(r + 0x10) & 0xBFFF);
                r[0xB] &= 0xFB;
                g.recalc_stats(r);
            }
            i = (i + 1) & 0xFF;
            if (i >= B(at::kMembers)) break;
        }
    }
    g.reload_party();
    Game_Step = Game_Step + 1;
    BattleMisc_BannerKinds = 0;
    B(at::kPhase) = 0;
    BattleActions_Step = 0;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
}

// ===========================================================================

void BattleTurnSteps_Inject() {
    if (bof3::WantsShadow("battle_turn_steps")) battle_turn_steps::SelfTest();
    BOF3_INJECT(BattleRoundEnd_Step);
    BOF3_INJECT(BattleRoundEnd_FasterStep);
    BOF3_INJECT(BattleRoundEnd_CheckFaster);
    BOF3_INJECT(BattleRoundEnd_FasterNext);
    BOF3_INJECT(BattleRoundEnd_StatusChain);
    BOF3_INJECT(BattleRoundEnd_NextRound);
    BOF3_INJECT(BattleEnd_Step);
    BOF3_INJECT(BattleEnd_TaskStep);
    BOF3_INJECT(BattleEnd_TasksBegin);
    BOF3_INJECT(BattleEnd_StartMemberTask);
    BOF3_INJECT(BattleEnd_AwaitMemberTasks);
    BOF3_INJECT(BattleEnd_WinStep);
    BOF3_INJECT(BattleEnd_WinBegin);
    BOF3_INJECT(BattleEnd_WinAwaitInput);
    BOF3_INJECT(BattleEnd_ExitStep);
    BOF3_INJECT(Battle_WriteBackParty);
    BOF3_INJECT(BattleEnd_ExitHook);
    BOF3_INJECT(BattleEnd_Finish);
    BOF3_INJECT(BattleEnd_ResultStep);
    BOF3_INJECT(BattleEnd_ResultPage);
}
