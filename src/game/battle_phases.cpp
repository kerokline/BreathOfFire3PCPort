// The battle's frame and the phase table's first half: the battle mode's
// frame (0x42E370); phase 0, the start - its dispatch (0x42E470),
// Battle_Init (0x42E4A0), the intro's dispatch and its four steps
// (0x42E730..0x42E98D); phase 1, the command input - its dispatch
// (0x42E990), the round start (0x42E9A0), the next member (0x42E9E0), the
// command cross (0x42EAD0), the confirm dispatch (0x42EED0) and Defend
// (0x42EEF0); phase 2, the commit - its dispatch (0x42F070), the commit
// (0x42F080), the enemy messages (0x42F130) and the wait (0x42F1D0).
// docs/battle_phases.md.
//
// Every call goes through battle_phases::g (battle_phases_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. Everything here is a faithful replacement.
#include "game/battle_phases.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_phases_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace battle_phases {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;
// Group HT-2 of the button round (docs/button-tests.md 3): the command
// cross's L1 / R1 are Japan's bits, asked through sigma.
using bof3x::input::sigma::Sigma;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Battle_PhaseDispatch, BattleParty_RunStates, BattleEnemy_RunAll, BattleBanner_Dispatch, AreaMap_Frame,
    BattleTask_RunAll, BattleParty_UpdateScreens, BattleEnemy_UpdateScreenAll, Field_ObjectsScreen,
    Sprite_UpdateObjectScreens, Party_ExtraScreens, Effect_RunObjects, Field_RunSlots, MoveScript_TintFrame,
    Field_RunTaskRecords, Field_DrawFrame,
    {Battle_Init, BattleIntro_Dispatch},
    {BattleIntro_OpenWindows, BattleIntro_WaitWindow, BattleIntro_PrepareCross, BattleIntro_Finish},
    BattleBanner_ClearAll, Battle_ActorIsOut, Battle_InitEncounterKind, BattleTask_ClearAll,
    Battle_InitActorContexts, Formation_ApplyStatMods, Gfx_ClutStripCopyRow, Sprite_SetClutStp, LoadDatFile,
    Window_Alloc, Msg_SystemPtr, BattleQueue_Push, BattleWin_OpenStatus, Battle_OpenEnemyNames,
    BattleWin_OpenSub1, BattleQueue_Pending, File_LoadDone, BattleWin_OpenSub2, BattleWin_OpenSub3,
    Battle_ClearCommands, Battle_BuildEntryOrder, Fn<void (__cdecl*)()>(::bof3::Code(kAutoFillCommands.v)),
    Battle_SpawnActorCopies, BattleBanner_ShowName, Sound_PlayEffect,
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kReturnItem.v)), BattleWin_DrawCommandCross,
    BattleWin_DrawCommandLabel, BattleWin_DrawPartyStatus, Battle_PulseStep,
    Battle_BuildTurnOrder, EnemyAI_ChooseActions,
};
Callees g = kOriginals;

}  // namespace battle_phases

using namespace battle_phases;

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
unsigned char* MenuRecord() { return PtrAtBattlePhases(::bof3::Img(at::kMenuRecord.v)); }

// A tail jump through a .data table, as the three stubs have it: the entry
// read at the jump, the index not checked (the tables are followed by more
// tables and data; docs/battle_phases.md section 3).
void JumpThrough(std::uint32_t table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(battle_phases::Long(battle_phases::At(table + index * 4)))))();
}

}  // namespace

// ===========================================================================
// The battle's frame
// ===========================================================================

// ===========================================================================
// Phase 0: the battle's start
// ===========================================================================

// original 0x42E4A0 (PSX Battle_Init 0x801D1228): once, at the battle's
// start.
//  1. The banners cleared. 0x904AB1 counts the actors below the byte
//     0x904AB0 (re-read for each) that Battle_ActorIsOut says are in.
//  2. The menu actor is member 0; the battle's globals reset (the turn
//     counter 0x904B90 to 1, the phase sub-bytes, the round flags, the
//     totals, the drop count, 0x904B82 ...).
//  3. The encounter kind, the battle tasks and the actor contexts set up.
//  4. For each member in use: its stat block +0xA0..+0xBF snapshotted to
//     +0xC0; +0x90 bit 13 (the byte +0x91's bit 5) set when the HP +0x98 is
//     below max HP +0xA0 / 4, else cleared; its command fields +0x125,
//     +0x12D, +0x130..+0x13F, +0x142..+0x145 zeroed. Then
//     Formation_ApplyStatMods, and each member in use has +0xC0 copied back
//     over +0xA0 (bit 0 of +0 re-read).
//  5. 16 words 0x80D560 -> 0x811560; CLUT strip rows 0x1A and 0x1B.
//  6. Actors 0..10: each member in use, and each enemy that is not out
//     unless 0x904AAA is 0x25, becomes Sprite_Current for Sprite_SetClutStp.
//  7. Unless 0x904AAA is set, the DAT file 0x904EFC + 0x15D is loaded (the
//     battle's music, PSX: the battle-start sound); 0x904B24..0x904B33
//     zeroed; while 0x904AAA (re-read) is set, the event hook with 6. Then
//     the step on.
//
// As the original has it: the actor passed to Battle_ActorIsOut is a byte
// in a dword whose upper bytes are the caller's ecx; Battle_ActorIsOut reads
// the byte.
//
// As the original has it, and the JP disc: step 2 leaves the target-pick
// flag BattleE4_Picking (0x904AAF) as the last battle left it. The US disc's
// Battle_Init (BATTLE.EMI#3 0x801D1228) adds its clear (US 0x801462EF, the
// JP's 0x801462EB) among the same resets.
//
// DIVERGENCE DIV-0163: 1 through PatchBytes("BattleInitPickClear") in
// BattlePhases_Inject, after the self-test. Step 2 then clears the flag
// with its neighbours, as the US disc does (D349), so a battle cannot start
// with a target highlighted that nobody is picking.
namespace battle_phases { std::uint8_t g_init_pick_clear = 0; }

void BattlePhases_ArmPickClear() {
    const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("BattleInitPickClear", Low32(&battle_phases::g_init_pick_clear), &was, &is, 1);
    bof3::Log("DIV-0163    a battle's start clears the target-pick flag, as the US disc (on unless the line above says OFF)");
}

// ===========================================================================
// Phase 1: the command input
// ===========================================================================

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
    const unsigned char flags = BBattlePhases(::bof3::Img(at::kRoundFlags.v));
    BattleActorCopies_EntryCount = 0;
    if ((flags & 0x10) != 0) {
        battle_phases::g.auto_fill_commands();
        BBattlePhases(::bof3::Img(at::kPhase.v)) = 2;
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
            const unsigned c = BBattlePhases(static_cast<std::uint32_t>(static_cast<std::int32_t>(::bof3::Img(at::kEntryOrder.v)) + index));
            if (c != 0xFF) {
                BattleE4_AutoMember = static_cast<unsigned char>(c);
                const unsigned char* const q = MemberBattlePhases(c);
                auto* const member_q = rv::AsConst<records::PartyMember>(q);
                if ((q[0x90] & 0x20) == 0 && (static_cast<std::uint32_t>(static_cast<std::int32_t>(member_q->flags_134)) & 0x14001u) == 0) {
                    unsigned char* const p = MemberBattlePhases(c);
                    BattleE4_MenuActor = reinterpret_cast<records::PartyMember*>(p);
                    SetPtrAtBattlePhases(::bof3::Img(at::kMenuRecord.v), p + 0x124);
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
    const unsigned char phase = BBattlePhases(::bof3::Img(at::kPhase.v));
    BossSb_Countdown = 0;
    BattleE5_GeneRow = 0;
    BattleResult_TickStep = 0xE;
    BBattlePhases(::bof3::Img(at::kPhase.v)) = static_cast<unsigned char>(phase + 1);
    BattleActions_Step = 0;
}

namespace {

// The command record's +0x10 (the actor's +0x134) bit 1 greys commands 2 and
// 3 (1-based: the cross's down and left arms).
bool CommandsGreyed() { return (MenuRecord()[0x10] & 2) != 0; }

}  // namespace

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
        unsigned char* p = MemberBattlePhases(BBattlePhases(static_cast<std::uint32_t>(static_cast<std::int32_t>(::bof3::Img(at::kEntryOrder.v)) + static_cast<signed char>(index))));
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
        const unsigned command = BBattlePhases(::bof3::Img(at::kCommand.v));
        if ((command == 2 || command == 3) && CommandsGreyed()) {
            battle_phases::g.play_effect(0x107);
            return;
        }
        battle_phases::g.play_effect(0x104);
        const unsigned char step = static_cast<unsigned char>(BattleActions_Step + 2);
        const unsigned char chosen = BBattlePhases(::bof3::Img(at::kCommand.v));
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
        battle_phases::g.draw_command_label(BBattlePhases(::bof3::Img(at::kCommand.v)));
        battle_phases::g.draw_party_status(static_cast<int>(rv::As<records::WindowRecord>(Window(1))->x), static_cast<int>(rv::As<records::WindowRecord>(Window(1))->y));
        BattleActions_Step = static_cast<unsigned char>(BattleActions_Step + 1);
        return;
    }
    // The d-pad passes through sigma; L1 / R1 (0x0C) become the Change and
    // View lines' buttons now. Battle_CommandPadMasks stays Capcom's bytes:
    // each mask is read as it is and asked through sigma.
    if ((held & Sigma(0xF00C)) == 0) {
        if (BBattlePhases(::bof3::Img(at::kCommand.v)) != 0) BBattlePhases(::bof3::Img(at::kCommand.v)) = 0;
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
                BBattlePhases(::bof3::Img(at::kCommand.v)) = static_cast<unsigned char>(k + 1);
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
        BBattlePhases(::bof3::Img(at::kCommand.v)) = static_cast<unsigned char>(k + 1);
    }
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

// ===========================================================================
// Phase 2: the commit
// ===========================================================================

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
    if ((BBattlePhases(::bof3::Img(at::kRoundFlags.v)) & 0x10) == 0) rv::As<records::WindowRecord>(Window(4))->state = 2;
    rv::As<records::WindowRecord>(Window(3))->state = 2;
    rv::As<records::WindowRecord>(Window(2))->state = 2;
    BBattlePhases(::bof3::Img(at::kB8E0.v)) = 0;
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
    BBattlePhases(::bof3::Img(at::kRoundFlags.v)) = static_cast<unsigned char>(BBattlePhases(::bof3::Img(at::kRoundFlags.v)) | 8);
    battle_phases::g.choose_actions();
    if ((BBattlePhases(::bof3::Img(at::kRoundFlags.v)) & 0x10) == 0) battle_phases::g.load_dat(0xD1);
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
    unsigned char* const text = battle_phases::At(::bof3::Img(at::kTextRecord0.v));
    std::memset(text, 0, 0x20);
    const unsigned char* const record = battle_phases::At(::bof3::Img(at::kMessages.v) + count * 4);
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
    if ((BBattlePhases(::bof3::Img(at::kRoundFlags.v)) & 0x10) == 0) {
        const unsigned wait = BattleResult_TickStep;
        BattleResult_TickStep = wait - 1;
        if (wait != 0) return;
    }
    const unsigned char phase = BBattlePhases(::bof3::Img(at::kPhase.v));
    BattleResult_TickStep = 0;
    BBattlePhases(::bof3::Img(at::kPhase.v)) = static_cast<unsigned char>(phase + 1);
    BattleActions_Step = 0;
}

// The module's inject, in the live file as every module's is (the 64-bit
// DLL leaves the fuzz files out, docs/x64-build-round.md 2); the self-test
// is battle_phases_fuzz.cpp's, hosted only.
void BattlePhases_Inject() {
    if (bof3::WantsShadow("battle_phases")) battle_phases::SelfTest();
    BattlePhases_ArmPickClear();  // DIV-0163: after the self-test, which compared Capcom's resets
    BOF3_INJECT(BattleInput_Dispatch);
    BOF3_INJECT(Battle_RoundStart);
    BOF3_INJECT(BattleInput_NextMember);
    BOF3_INJECT(BattleMenu_CommandSelect);
    BOF3_INJECT(BattleMenu_ConfirmDispatch);
    BOF3_INJECT(Cmd_ConfirmDefend);
    BOF3_INJECT(BattleCommit_Dispatch);
    BOF3_INJECT(Battle_CommitRound);
    BOF3_INJECT(BattleCommit_QueueMessages);
    BOF3_INJECT(BattleCommit_WaitLoad);
}
