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
    Fn<void (__cdecl*)(unsigned)>(0x4DF820),  // Port_DroppedCall (a macro in symbols.gen.h: not ours)
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

unsigned char* Member(unsigned i) { return At(at::kParty + (i & 0xFF) * at::kPartyStride); }
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
    [[clang::musttail]] return Entry(at::kRoundEndSteps, B(at::kState1))();
}

// original 0x4302C0: entry 0x904AA2 of BattleRoundEnd_FasterSteps (0x64AF38:
// BattleRoundEnd_CheckFaster, BattleRoundEnd_FasterNext,
// BattleRoundEnd_NextRound).
extern "C" void __cdecl BattleRoundEnd_FasterStep() {
    [[clang::musttail]] return Entry(at::kRoundEndFaster, B(at::kState2))();
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
    if (B(at::kExtraRound) == 0 && B(at::kTimer) == 0) {
        switch (g.mark_faster() & 0xFF) {
        case 1:
            if (B(at::kSideMode) != 2) {
                B(at::kExtraRound) = 1;
                Inc(at::kState2);
                return;
            }
            Inc(at::kState2);   // then the round ends, which zeroes it after Battle_TickCounters
            break;
        case 2:
            B(at::kExtraRound) = 2;
            Inc(at::kState2);
            return;
        case 3:
            B(at::kExtraRound) = B(at::kSideMode) != 2 ? 3 : 2;
            Inc(at::kState2);
            return;
        default:
            break;
        }
    }
    for (unsigned i = 0; i < 2; ++i) Member(i)[at::kFlags + 1] &= 0x7F;
    for (unsigned i = 0; i < 7; ++i) At(at::kEnemy + i * at::kEnemyStride)[at::kEFlags + 1] &= 0x7F;
    SetLong(At(at::kRounds), static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(At(at::kRounds))) + 1));
    B(at::kExtraRound) = 0;
    g.tick_counters();
    B(at::kState2) = 0;
    Inc(at::kState1);
}

// original 0x430500: `inc byte ptr [0x904AA2]` - the extra round's frame
// between the check and BattleRoundEnd_NextRound.
extern "C" void __cdecl BattleRoundEnd_FasterNext() { Inc(at::kState2); }

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
    switch (B(at::kState2)) {
    case 0:
        Inc(at::kState2);
        if (Yes(g.steps[0]())) return;
        Inc(at::kState2);
        [[fallthrough]];
    case 2:
        Inc(at::kState2);
        if (Yes(g.steps[1]())) return;
        [[fallthrough]];
    case 3:
        Inc(at::kState2);
        if (Yes(g.steps[2]())) return;
        [[fallthrough]];
    case 4:
        Inc(at::kState2);
        if (Yes(g.steps[3]())) return;
        [[fallthrough]];
    case 5:
        Inc(at::kState2);
        if (Yes(g.steps[4]())) return;
        [[fallthrough]];
    case 6:
        Inc(at::kState2);
        if (Yes(g.steps[5]())) return;
        [[fallthrough]];
    case 7:
        Inc(at::kState2);
        if (Yes(g.steps[6]())) return;
        Inc(at::kState2);
        [[fallthrough]];
    case 9:
        Inc(at::kState2);
        if (Yes(g.steps[7]())) return;
        Inc(at::kState2);
        [[fallthrough]];
    case 11:
        Inc(at::kState2);
        if (Yes(g.steps[8]())) return;
        [[fallthrough]];
    case 13:
        B(at::kState2) = 0;
        Inc(at::kState1);
        return;
    case 1:
    case 8:
    case 10:
    case 12:
        if (Word(At(at::kPending)) != 0) return;
        B(at::kRoundFlags) &= 0xFB;
        Inc(at::kState2);
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
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleRoundEnd_NextRound() {
    for (unsigned i = 0x10; i < 0x18; ++i)
        if (Long(At(0x803500 + i * 4)) != 0) return;
    if (B(at::kEventBattle) != 0) {
        const auto hook = reinterpret_cast<unsigned (__cdecl*)(int)>(
            static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(at::kHookEvent)))));
        if ((hook(2) & 0xFF) == 0xFF) return;
    }
    if (B(at::kBattleEnd) != 0) {
        B(at::kRoundFlags) &= 0xEF;
        for (unsigned w : {0u, 3u, 1u, 11u, 10u, 9u, 8u, 7u, 6u, 5u, 4u}) B(at::kWindows + w * at::kWindowStride) = 0;
        B(at::kPhase) = 5;
        B(at::kState1) = 0;
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
        if (!Yes(g.actor_is_out(i))) Member(i)[at::kState] = 2;
    B(at::kRoundFlags) &= 0xF7;
    B(at::kSideMode) = 0;
    B(at::kPhase) = 1;
    B(at::kState1) = 0;
    B(at::kState2) = 0;
}

// ===========================================================================
// Phase 5: the battle's end
// ===========================================================================

// original 0x4311E0: phase 5's dispatch - entry 0x904AA1 of BattleEnd_Steps
// (0x64AF44: BattleEnd_TaskStep, BattleEnd_WinStep, 0x431540 and 0x4315B0
// (the other way out, Capcom's), BattleEnd_ExitStep).
extern "C" void __cdecl BattleEnd_Step() {
    [[clang::musttail]] return Entry(at::kEndSteps, B(at::kState1))();
}

// original 0x4311F0: entry 0x904AA2 of BattleEnd_TaskSteps (0x64AF58:
// BattleEnd_TasksBegin, BattleEnd_StartMemberTask, BattleEnd_AwaitMemberTasks).
extern "C" void __cdecl BattleEnd_TaskStep() {
    [[clang::musttail]] return Entry(at::kEndTaskSteps, B(at::kState2))();
}

// original 0x431200: 0x802D20 and the member cursor 0x904AA5 zeroed, 0x904AA2
// stepped on. Also the other way out's first step (0x64AF7C).
extern "C" void __cdecl BattleEnd_TasksBegin() {
    B(at::kPartyByte) = 0;
    B(at::kCursor) = 0;
    Inc(at::kState2);
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
    unsigned cursor = B(at::kCursor);
    if (cursor < B(at::kMembers)) {
        for (;;) {
            const unsigned out = g.actor_is_out(cursor);
            cursor = B(at::kCursor);
            if (!Yes(out) && (Member(cursor)[at::kFlags] & 3) != 0) {
                Member(cursor)[at::kFlags2 + 1] |= 0x20;
                unsigned parameter = 0xB;
                if (Member(cursor)[at::kCharId] != 4) {
                    for (unsigned k = 0; k < 3; ++k) At(at::kCopies + k * at::kPartyStride)[at::kFlags] &= 0xFE;
                    parameter = 0xC;
                }
                const unsigned task = g.task_create(0, parameter) & 0xFF;
                SetLong(At(at::kTasks + task * at::kTaskSize + 0x80),
                        static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(Member(B(at::kCursor)))));
                Inc(at::kCursor);
                break;
            }
            const unsigned count = B(at::kMembers);
            cursor = (cursor + 1) & 0xFF;
            B(at::kCursor) = static_cast<unsigned char>(cursor);
            if (cursor >= count) break;
        }
    }
    Inc(at::kState2);
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
    if (B(at::kCursor) != count) {
        --*At(at::kState2);
        return;
    }
    unsigned end = B(at::kBattleEnd);
    if ((end & 1) == 0) {
        if (count != 0) {
            for (unsigned i = 0;;) {
                unsigned char* const m = Member(i);
                Sprite_Current = m;
                if (m[at::kStatus + 1] & 8) {
                    SetWord(m + at::kHp, Word(m + at::kMaxHp));
                    SetWord(m + at::kAp, Word(m + at::kMaxAp));
                    m[at::kByte9C] = m[at::kByteAE];
                    g.clear_status(i, Word(m + at::kStatus) & 0xBFFF);
                }
                g.clear_status(i, Word(m + at::kStatus) & 0xBF5F);
                g.release_tint(m);
                g.return_item(i);
                i = (i + 1) & 0xFF;
                if (i >= B(at::kMembers)) break;
            }
            end = B(at::kBattleEnd);
        }
        B(at::kTimer) = 0;
    }
    if (B(at::kEventBattle) == 0) {
        if (end & 1) B(at::kState1) = 2;
        if (end & 2) B(at::kState1) = 1;
    }
    reinterpret_cast<Step>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(at::kHookEnd)))))();
    g.dropped_call(0);
    if (B(at::kState1) == 1) g.load_dat(0xCD);
    if (B(at::kState1) == 2) g.load_dat(0xCE);
    B(at::kState2) = 0;
}

// original 0x4314B0: the win's dispatch - entry 0x904AA2 of BattleEnd_WinSteps
// (0x64AF64: BattleEnd_WinBegin, BattleEnd_WinAwaitInput, BattleEnd_ResultStep).
extern "C" void __cdecl BattleEnd_WinStep() {
    [[clang::musttail]] return Entry(at::kEndWinSteps, B(at::kState2))();
}

// original 0x4314C0: each of the three members present (+0 bit 0) to state 2;
// unless 0x904AE5 bit 0x40 keeps the battle's music, Music_FadeOutStop(10)
// and Music_Play(0xA5, 10); Battle_OpenMsgWindow, Battle_OpeningMessage (its
// lines by 0x904B90 - named for the opening, but here it is the win's), and
// 0x904AA2 on.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_WinBegin() {
    for (unsigned i = 0; i < 3; ++i)
        if (Member(i)[0] & 1) Member(i)[at::kState] = 2;
    if ((B(at::kMusicFlags) & 0x40) == 0) {
        g.fade_out(10);
        g.music_play(0xA5, 10);
    }
    g.open_window();
    g.opening_message();
    Inc(at::kState2);
}

// original 0x431520: 0x904AA2 on once File_LoadDone answers (it always does)
// and Input_Pressed is not 0.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_WinAwaitInput() {
    if (g.load_done() != 0 && Word(At(at::kInput)) != 0) Inc(at::kState2);
}

// original 0x431910: entry 0x904AA3 of BattleEnd_ResultSteps (0x64AFA0:
// BattleEnd_ResultPage, then group CD's 0x431B60 and 0x431D50).
extern "C" void __cdecl BattleEnd_ResultStep() {
    [[clang::musttail]] return Entry(at::kEndResultSteps, B(at::kState3))();
}

// original 0x431920: entry (dword 0x904AA4 & 0xFF) of BattleEnd_ResultPages
// (0x64AFAC: group CD's 0x431940, 0x431A20, 0x431A90, 0x431AB0, 0x431B30).
extern "C" void __cdecl BattleEnd_ResultPage() {
    [[clang::musttail]] return Entry(at::kEndResultPages, B(at::kState4))();
}

// original 0x431760: the exit's dispatch - entry 0x904AA2 of
// BattleEnd_ExitSteps (0x64AF90: Battle_WriteBackParty, BattleEnd_ExitHook,
// BattleEnd_Finish, 0x4318F0 (Capcom's)).
extern "C" void __cdecl BattleEnd_ExitStep() {
    [[clang::musttail]] return Entry(at::kEndExitSteps, B(at::kState2))();
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
    Inc(at::kState2);
}

// original 0x4317B0: once File_LoadDone answers - the hook 0x904B68; unless
// 0x904AE5 bit 0x40, Music_FadeOutStop(10) and, when Music_Track is then
// 0xFF, 0x904AE8 |= 8 - 0x904AA2 on.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnd_ExitHook() {
    if (g.load_done() == 0) return;
    reinterpret_cast<Step>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(at::kHookExit)))))();
    if ((B(at::kMusicFlags) & 0x40) == 0) {
        g.fade_out(10);
        if (B(at::kMusicTrack) == 0xFF) B(at::kBattleEnd) |= 8;
    }
    Inc(at::kState2);
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
            if (m[at::kStatus + 1] & 0x40) {
                unsigned char* const r = At(at::kRecords + m[at::kRecord] * at::kRecordStride);
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
    SetWord(At(at::kGameStep), Word(At(at::kGameStep)) + 1);
    B(at::kBattleEnd2) = 0;
    B(at::kPhase) = 0;
    B(at::kState1) = 0;
    B(at::kState2) = 0;
    B(at::kState3) = 0;
    B(at::kState4) = 0;
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
