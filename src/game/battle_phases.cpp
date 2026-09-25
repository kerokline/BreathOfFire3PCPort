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
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_phases {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

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
    Battle_ClearCommands, Battle_BuildEntryOrder, Fn<void (__cdecl*)()>(kAutoFillCommands),
    Battle_SpawnActorCopies, BattleBanner_ShowName, Sound_PlayEffect,
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(kReturnItem), BattleWin_DrawCommandCross,
    BattleWin_DrawCommandLabel, BattleWin_DrawPartyStatus, Battle_PulseStep,
    Battle_BuildTurnOrder, EnemyAI_ChooseActions,
};
Callees g = kOriginals;

}  // namespace battle_phases

using namespace battle_phases;

namespace {

unsigned char& B(std::uint32_t address) { return *At(address); }
unsigned char* PtrAt(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(address)))));
}
void SetPtrAt(std::uint32_t address, const unsigned char* p) {
    SetLong(At(address), static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(p)));
}
unsigned char* Member(unsigned index) { return At(at::kMembers + index * at::kMemberSize); }
unsigned char* EnemyObject(unsigned index) { return At(at::kEnemies + index * at::kEnemySize); }
unsigned char* Window(unsigned index) { return At(at::kWindows + index * at::kWindowSize); }
// The member the menu is for (0x939EC4) and its command record (0x939FA0),
// each re-read wherever the original re-reads it.
unsigned char* MenuActor() { return PtrAt(at::kMenuActor); }
unsigned char* MenuRecord() { return PtrAt(at::kMenuRecord); }
int S8(std::uint32_t address) { return static_cast<signed char>(B(address)); }

// A tail jump through a .data table, as the three stubs have it: the entry
// read at the jump, the index not checked (the tables are followed by more
// tables and data; docs/battle_phases.md section 3).
void JumpThrough(std::uint32_t table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(table + index * 4)))))();
}

}  // namespace

// ===========================================================================
// The battle's frame
// ===========================================================================

// original 0x42E370 (no PSX twin paired): state 5 of the battle mode's state
// table 0x656A84 (reached by the tail jump at 0x495E98), once a frame. The
// phase dispatch runs unless the round flags' bit 1 is set, or window 0 is in
// use (0x803160) or 0x904AE9 has bit 1 - the last two only while the phase is
// not 5. Then, in this order: the party's states, the enemies', the banners,
// the area map, the battle tasks, the party's and the enemies' screen
// updates, the object screens (Field_ObjectsScreen while 0x904AAA is set,
// else Sprite_UpdateObjectScreens - the byte read after all of those), the
// party's extra screens, the effects, the field slots, the tint, the window
// tasks, and a tail jump to Field_DrawFrame.
extern "C" void __cdecl Battle_Frame(void) {
    if ((B(at::kRoundFlags) & 2) == 0) {
        const unsigned phase = B(at::kPhase);
        const bool window_held = B(at::kWindows) != 0 && phase != 5;
        const bool flag_held = (B(at::kAE9) & 2) != 0 && phase != 5;
        if (!window_held && !flag_held) g.phase_dispatch();
    }
    g.party_run_states();
    g.enemy_run_all();
    g.banner_dispatch();
    g.area_map_frame();
    g.task_run_all();
    g.party_update_screens();
    g.enemy_update_screens();
    if (B(at::kEventBattle) != 0) g.objects_screen();
    else g.update_object_screens();
    g.party_extra_screens();
    g.effect_run_objects();
    g.field_run_slots();
    g.tint_frame();
    g.run_task_records();
    g.draw_frame();
}

// ===========================================================================
// Phase 0: the battle's start
// ===========================================================================

// original 0x42E470 (no PSX twin paired): phase 0's entry in
// Battle_PhaseDispatch's table. Entry 0x904AA1 of a two-entry table the
// original builds on its own stack - Battle_Init, BattleIntro_Dispatch. The
// index is not checked: 2..255 would call through the words above the table
// on the original's stack, its own return address first; ours aborts
// (CLAUDE.md rule 4, as Battle_PhaseDispatch).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleStart_Dispatch(void) {
    const unsigned index = B(at::kStep);
    if (index >= 2) bof3::Fatal("BattleStart_Dispatch: step %u, past the two-entry table", index);
    g.start_steps[index]();
}

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
extern "C" void __cdecl Battle_Init(void) {
    g.banner_clear_all();
    const unsigned count = B(at::kActorCount);
    B(at::kActorsIn) = 0;
    if (count != 0) {
        unsigned i = 0;
        do {
            if (g.actor_is_out(i) == 0) B(at::kActorsIn) = static_cast<unsigned char>(B(at::kActorsIn) + 1);
            ++i;
        } while (i < B(at::kActorCount));
    }
    SetPtrAt(at::kMenuActor, Member(0));
    B(at::kTapTimer) = 0;
    B(at::kTapCommand) = 0;
    B(at::kAA7) = 0;
    B(at::kCommand) = 0;
    B(at::kCrossGrow) = 8;
    SetWord(At(at::kRoundFlags), 0);
    B(at::kBattleEnd) = 0;
    SetLong(At(at::kTurnCounter), 1);
    SetLong(At(at::kZennyTotal), 0);
    SetLong(At(at::kExpTotal), 0);
    B(at::kAE6) = 0;
    B(at::kAAD) = 0;
    B(at::kB7A) = 0;
    SetWord(At(at::kB98), 0);
    B(at::kDropCount) = 0;
    SetWord(At(at::kTurnBits), 0);
    B(at::kB8B) = 0;
    B(at::kActorAt) = 0;
    B(at::kAnimGate) = 0;
    B(at::kB97) = 0;
    B(at::kB89) = 0;
    SetWord(At(at::kEFE), 0);
    g.init_encounter_kind();
    g.task_clear_all();
    g.init_actor_contexts();
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = Member(m);
        if ((p[0] & 1) == 0) continue;
        std::memcpy(p + 0xC0, p + 0xA0, 0x20);
        const unsigned quarter = static_cast<unsigned>(Word(p + 0xA0)) >> 2;
        if (Word(p + 0x98) < quarter) p[0x91] = static_cast<unsigned char>(p[0x91] | 0x20);
        else SetWord(p + 0x90, Word(p + 0x90) & 0xDFFFu);
        SetLong(p + 0x130, 0);
        SetLong(p + 0x134, 0);
        p[0x125] = 0;
        p[0x12D] = 0;
        p[0x142] = 0;
        p[0x143] = 0;
        p[0x144] = 0;
        p[0x145] = 0;
        SetLong(p + 0x138, 0);
        SetLong(p + 0x13C, 0);
    }
    g.apply_stat_mods();
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = Member(m);
        if ((p[0] & 1) != 0) std::memcpy(p + 0xA0, p + 0xC0, 0x20);
    }
    for (unsigned i = 0; i < 16; ++i) SetWord(At(at::kClutTo + 2 * i), Word(At(at::kClutFrom + 2 * i)));
    g.clut_strip_copy_row(0x1A);
    g.clut_strip_copy_row(0x1B);
    for (unsigned a = 0; a <= 10; ++a) {
        if (a <= 2) {
            unsigned char* const p = Member(a);
            if ((p[0] & 1) == 0) continue;
            Sprite_Current = p;
        } else {
            if (g.actor_is_out(a) != 0) continue;
            if (B(at::kEventBattle) == 0x25) continue;
            Sprite_Current = EnemyObject(a - 3);
        }
        g.set_clut_stp();
    }
    if (B(at::kEventBattle) == 0) g.load_dat(static_cast<int>(Word(At(at::kBattleMusic)) + 0x15D));
    const unsigned char event = B(at::kEventBattle);
    for (unsigned i = 0; i < 4; ++i) SetLong(At(at::kB24 + 4 * i), 0);
    if (event != 0) reinterpret_cast<void (__cdecl*)(int)>(PtrAt(at::kEventHook))(6);
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x42E730 (no PSX twin paired): phase 0 step 1. Entry 0x904AA2 of
// a four-entry table the original builds on its own stack - the intro's
// steps below. The index is not checked; ours aborts past the table (as
// BattleStart_Dispatch).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleIntro_Dispatch(void) {
    const unsigned index = B(at::kSubStep);
    if (index >= 4) bof3::Fatal("BattleIntro_Dispatch: sub-step %u, past the four-entry table", index);
    g.intro_steps[index]();
}

// original 0x42E770 (PSX 0x801D1820): the intro's windows.
//  1. 0x93C2A0 / 0x93C2A1 zeroed; window 0x14 of kind 3 allocated, and the
//     record Window_Alloc's low byte names gets +2 = 7, +3 = 0, x 0x14,
//     y -22.
//  2. The initiative byte 0x904AE4 at 1 queues system message 0x16 at
//     (2, 0x30); at 2 (re-read), message 0x17.
//  3. Each member i in use allocates window 0xD + i of kind 3; that record
//     (whatever Window_Alloc answered) gets +2 = 5, +3 = 0, +8 = +9 = 0,
//     +0xA = i, and x, y from the member's +0x2E / +0x30 moved by the s8
//     pair of 0x64DF70 at 2 (+8 + 4 * +0x89), the x also by the s8 of
//     0x64E2BC at 2 ((0x904AAC ^ 2) >> 1), the y by 12.
//  4. The status window opened, the enemy names; the sub-step on.
//
// As the original has it: Window_Alloc's 0xFF is not tested - the first
// record is then 0xFF's, past the 64 (0x80551C..).
extern "C" void __cdecl BattleIntro_OpenWindows(void) {
    B(at::kC2A0) = 0;
    B(at::kC2A0 + 1) = 0;
    unsigned char* const first = Window(g.window_alloc(0x14, 3) & 0xFF);
    first[2] = 7;
    first[3] = 0;
    SetWord(first + 4, 0x14);
    SetWord(first + 6, 0xFFEA);
    if (B(at::kInitiative) == 1) g.queue_push(2, 0x30, static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(g.msg_system_ptr(0x16))));
    if (B(at::kInitiative) == 2) g.queue_push(2, 0x30, static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(g.msg_system_ptr(0x17))));
    for (unsigned m = 0; m < 3; ++m) {
        const unsigned char* const p = Member(m);
        if ((p[0] & 1) == 0) continue;
        g.window_alloc(0xD + m, 3);
        unsigned char* const w = Window(0xD + m);
        const unsigned pair = (p[8] + p[0x89] * 4u) * 2u;
        w[2] = 5;
        w[3] = 0;
        const unsigned facing = ((B(at::kFacing) ^ 2u) >> 1) * 2u;
        SetWord(w + 4, static_cast<unsigned>(S8(at::kFacingOffsets + facing) + S8(at::kStatusOffsets + pair) + Word(p + 0x2E)));
        SetWord(w + 6, static_cast<unsigned>(S8(at::kStatusOffsets + pair + 1) + Word(p + 0x30) + 0xC));
        w[0xA] = static_cast<unsigned char>(m);
        w[8] = 0;
        w[9] = 0;
    }
    g.open_status(0);
    g.open_enemy_names();
    B(at::kSubStep) = static_cast<unsigned char>(B(at::kSubStep) + 1);
}

// original 0x42E8C0 (PSX 0x801D1AAC): the sub-step on once window 1's +3 is
// 1.
extern "C" void __cdecl BattleIntro_WaitWindow(void) {
    if (Window(1)[3] == 1) B(at::kSubStep) = static_cast<unsigned char>(B(at::kSubStep) + 1);
}

// original 0x42E8D0 (PSX 0x801D1AE0): the first sub-window opened with 0;
// then, when the initiative byte 0x904AE4 is 2 or more, window 2's +3 = 2
// and the seven cross growth bytes 0x904ABC.. zeroed, else all seven 0x10;
// the sub-step on.
extern "C" void __cdecl BattleIntro_PrepareCross(void) {
    g.open_sub1(0);
    if (B(at::kInitiative) >= 2) {
        Window(2)[3] = 2;
        SetLong(At(at::kCrossGrow), 0);
        SetWord(At(at::kCrossGrow + 4), 0);
        B(at::kCrossGrow + 6) = 0;
    } else {
        SetLong(At(at::kCrossGrow), 0x10101010);
        SetWord(At(at::kCrossGrow + 4), 0x1010);
        B(at::kCrossGrow + 6) = 0x10;
    }
    B(at::kSubStep) = static_cast<unsigned char>(B(at::kSubStep) + 1);
}

// original 0x42E930 (PSX 0x801D1B8C): once the initiative byte is 2 or more
// or window 2's +3 is 1, no queued banner is pending and the files are
// loaded: the second sub-window opened with 2 (1 when the initiative byte,
// re-read, is below 2), the third with 2; phase on, step and sub-step 0.
extern "C" void __cdecl BattleIntro_Finish(void) {
    if (B(at::kInitiative) < 2 && Window(2)[3] != 1) return;
    if (g.queue_pending() != 0) return;
    if (g.file_load_done() == 0) return;
    g.open_sub2(B(at::kInitiative) >= 2 ? 2u : 1u);
    g.open_sub3(2);
    B(at::kPhase) = static_cast<unsigned char>(B(at::kPhase) + 1);
    B(at::kStep) = 0;
    B(at::kSubStep) = 0;
}

// ===========================================================================
// Phase 1: the command input
// ===========================================================================

// original 0x42E990 (no PSX twin paired): phase 1's entry. A tail jump
// through Battle_InputSteps 0x64AE28 by the step 0x904AA1: Battle_RoundStart,
// BattleInput_NextMember, BattleMenu_CommandSelect, 0x42ED90 (a sub-step
// dispatch through 0x64AE48, not this group's), BattleMenu_ConfirmDispatch.
extern "C" void __cdecl BattleInput_Dispatch(void) { JumpThrough(at::kInputSteps, B(at::kStep)); }

// original 0x42E9A0 (PSX Battle_RoundStart 0x801D1C88): the commands
// cleared, the entry order built, the menu index 0x904AC3 zeroed. With auto
// battle on (the round flags' bit 4, read after the two calls) the commands
// are filled (0x446720, PSX AutoBattle_FillCommands) and the phase set to 2;
// else the actor copies are spawned and the step goes on.
extern "C" void __cdecl Battle_RoundStart(void) {
    g.clear_commands();
    g.build_entry_order();
    const unsigned char flags = B(at::kRoundFlags);
    B(at::kMenuIndex) = 0;
    if ((flags & 0x10) != 0) {
        g.auto_fill_commands();
        B(at::kPhase) = 2;
        return;
    }
    g.spawn_actor_copies();
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
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
    if (g.file_load_done() == 0) return;
    signed char index = static_cast<signed char>(B(at::kMenuIndex));
    if (index == 0) SetWord(At(at::kInputHeld), 0);
    if (index < 3) {
        for (;;) {
            const unsigned c = B(static_cast<std::uint32_t>(static_cast<std::int32_t>(at::kEntryOrder) + index));
            if (c != 0xFF) {
                B(at::kMenuMember) = static_cast<unsigned char>(c);
                const unsigned char* const q = Member(c);
                if ((q[0x90] & 0x20) == 0 && (static_cast<std::uint32_t>(Long(q + 0x134)) & 0x14001u) == 0) {
                    unsigned char* const p = Member(c);
                    SetPtrAt(at::kMenuActor, p);
                    SetPtrAt(at::kMenuRecord, p + 0x124);
                    g.banner_show_name(p);
                    Window(4)[3] = 1;
                    Window(3)[3] = 1;
                    Window(2)[3] = 1;
                    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
                    return;
                }
            }
            index = static_cast<signed char>(index + 1);
            B(at::kMenuIndex) = static_cast<unsigned char>(index);
            if (index >= 3) break;
        }
    }
    const unsigned char phase = B(at::kPhase);
    B(at::kTapTimer) = 0;
    B(at::kTapCommand) = 0;
    SetWord(At(at::kWaitFrames), 0xE);
    B(at::kPhase) = static_cast<unsigned char>(phase + 1);
    B(at::kStep) = 0;
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
    if (B(at::kTapTimer) != 0) B(at::kTapTimer) = static_cast<unsigned char>(B(at::kTapTimer) - 1);
    unsigned pressed = Word(At(at::kInputPressed));
    if ((Word(At(at::kCancelButtons)) & pressed) != 0 && B(at::kMenuIndex) != 0) {
        g.play_effect(0x106);
        const unsigned char index = static_cast<unsigned char>(B(at::kMenuIndex) - 1);
        const unsigned char step = static_cast<unsigned char>(B(at::kStep) - 1);
        B(at::kMenuIndex) = index;
        B(at::kStep) = step;
        unsigned char* p = Member(B(static_cast<std::uint32_t>(static_cast<std::int32_t>(at::kEntryOrder) + static_cast<signed char>(index))));
        SetPtrAt(at::kMenuActor, p);
        SetLong(p + 0x134, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p + 0x134)) & ~4u));
        MenuActor()[1] = 2;
        p = MenuActor();
        if (p[0x125] == 5) {
            const std::uint32_t flags = static_cast<std::uint32_t>(Long(p + 0x130));
            if ((flags & 0x4000) == 0) g.return_item(p[0x12E], Word(p + 0x126));
            else SetLong(p + 0x130, static_cast<std::int32_t>(flags & ~0x4000u));
            p = MenuActor();
        }
        SetLong(p + 0x130, 0);
        MenuActor()[0x125] = 0;
        return;
    }
    if ((Word(At(at::kConfirmButtons)) & pressed) != 0) {
        const unsigned command = B(at::kCommand);
        if ((command == 2 || command == 3) && CommandsGreyed()) {
            g.play_effect(0x107);
            return;
        }
        g.play_effect(0x104);
        const unsigned char step = static_cast<unsigned char>(B(at::kStep) + 2);
        const unsigned char chosen = B(at::kCommand);
        Window(3)[3] = 2;
        Window(2)[3] = 2;
        B(at::kStep) = step;
        B(at::kSubStep) = chosen;
        return;
    }
    unsigned held = Word(At(at::kInputHeld));
    if ((held & 0x100) != 0) {
        g.play_effect(0x104);
        const unsigned y = Word(Window(2) + 6);
        const unsigned x = Word(Window(2) + 4);
        Window(2)[3] = 2;
        Window(1)[3] = 2;
        Window(3)[3] = 2;
        g.draw_command_cross(static_cast<int>(x), static_cast<int>(y));
        g.draw_command_label(B(at::kCommand));
        g.draw_party_status(static_cast<int>(Word(Window(1) + 4)), static_cast<int>(Word(Window(1) + 6)));
        B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
        return;
    }
    if ((held & 0xF00C) == 0) {
        if (B(at::kCommand) != 0) B(at::kCommand) = 0;
        return;
    }
    for (int k = 5; k >= 0; --k) {
        const unsigned mask = Word(At(at::kPadMasks + 2 * static_cast<unsigned>(k)));
        if ((mask & held) == 0) continue;
        if ((pressed & mask) != 0) {
            g.play_effect(0x101);
            if (B(at::kTapCommand) == static_cast<unsigned>(k + 1) && B(at::kTapTimer) != 0) {
                if ((k == 1 || k == 2) && CommandsGreyed()) {
                    B(at::kTapTimer) = 0;
                    B(at::kTapCommand) = 0;
                    return;
                }
                const unsigned char step = static_cast<unsigned char>(B(at::kStep) + 2);
                B(at::kCommand) = static_cast<unsigned char>(k + 1);
                B(at::kSubStep) = static_cast<unsigned char>(k + 1);
                Window(3)[3] = 2;
                Window(2)[3] = 2;
                B(at::kStep) = step;
                B(at::kTapTimer) = 0;
                B(at::kTapCommand) = 0;
                return;
            }
            pressed = Word(At(at::kInputPressed));
            B(at::kTapTimer) = 8;
            B(at::kTapCommand) = static_cast<unsigned char>(k + 1);
            held = Word(At(at::kInputHeld));
        }
        B(at::kCommand) = static_cast<unsigned char>(k + 1);
    }
}

// original 0x42EED0 (PSX BattleMenu_ConfirmDispatch 0x801D24CC): the menu
// pulse stepped, then a tail jump through Battle_MenuSteps 0x64AE54 by the
// chosen command 0x904AA2 (read after the pulse): 0x447110, 0x447430,
// 0x448180, 0x447FD0 (groups CH and CI), Cmd_ConfirmDefend, 0x42EF50 (the
// PSX's Cmd_AutoBattle_Begin, unowned), 0x44A000, 0x44FF00.
extern "C" void __cdecl BattleMenu_ConfirmDispatch(void) {
    g.pulse_step();
    JumpThrough(at::kMenuSteps, B(at::kSubStep));
}

// original 0x42EEF0 (PSX 0x801D2514, the Defend confirm the sibling's
// BATTLE_RAM.md places after BattleMenu_ConfirmDispatch): the command record
// gets command 2, the actor's +5 as its target, +0xC bit 1; the actor +1 =
// 2; the menu index on one, step 1, sub-steps 0. Both pointers re-read at
// each use, as the original.
extern "C" void __cdecl Cmd_ConfirmDefend(void) {
    MenuRecord()[1] = 2;
    const unsigned char target = MenuActor()[5];
    MenuRecord()[0] = target;
    unsigned char* const r = MenuRecord();
    SetLong(r + 0xC, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(r + 0xC)) | 2u));
    MenuActor()[1] = 2;
    const unsigned char index = static_cast<unsigned char>(B(at::kMenuIndex) + 1);
    B(at::kStep) = 1;
    B(at::kMenuIndex) = index;
    B(at::kSubStep) = 0;
    B(at::kSubStep2) = 0;
}

// ===========================================================================
// Phase 2: the commit
// ===========================================================================

// original 0x42F070 (no PSX twin paired): phase 2's entry. A tail jump
// through Battle_CommitSteps 0x64AE74 by the step 0x904AA1:
// Battle_CommitRound, BattleCommit_QueueMessages, BattleCommit_WaitLoad.
extern "C" void __cdecl BattleCommit_Dispatch(void) { JumpThrough(at::kCommitSteps, B(at::kStep)); }

// original 0x42F080 (PSX Battle_CommitRound 0x801D2774): the turn order
// built; window 4's +3 = 2 unless auto battle, windows 3 and 2 too;
// 0x93B8E0 zeroed. Each member not out with an item command (+0x125 = 5, an
// item word +0x126 with a high byte) gives the item back (0x446D90(+0x12E,
// +0x126)) unless +0x130 has bit 14, which is cleared instead. The round
// flags get bit 3; the enemies choose their actions; unless auto battle
// (re-read) DAT file 0xD1 is loaded; the step on.
extern "C" void __cdecl Battle_CommitRound(void) {
    g.build_turn_order();
    if ((B(at::kRoundFlags) & 0x10) == 0) Window(4)[3] = 2;
    Window(3)[3] = 2;
    Window(2)[3] = 2;
    B(at::kB8E0) = 0;
    for (unsigned m = 0; m <= 2; ++m) {
        if (g.actor_is_out(m) != 0) continue;
        unsigned char* const p = Member(m);
        if (p[0x125] != 5) continue;
        const unsigned item = Word(p + 0x126);
        if ((item & 0xFF00) == 0) continue;
        const std::uint32_t flags = static_cast<std::uint32_t>(Long(p + 0x130));
        if ((flags & 0x4000) == 0) g.return_item(p[0x12E], item);
        else SetLong(p + 0x130, static_cast<std::int32_t>(flags & ~0x4000u));
    }
    B(at::kRoundFlags) = static_cast<unsigned char>(B(at::kRoundFlags) | 8);
    g.choose_actions();
    if ((B(at::kRoundFlags) & 0x10) == 0) g.load_dat(0xD1);
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x42F130 (PSX 0x801D28E0): while 0x939F60 is 0, one queued
// enemy message a frame, the last first: the count 0x93C2A2 at 0 moves the
// step on; else Text_Records[0] is cleared, its first 12 bytes the named
// enemy's +0x80.. (the record 0x939FBC + 4 n: its byte the enemy, its word
// +2 the system message), and the message queued at (1, 0); the count
// (re-read) down one.
extern "C" void __cdecl BattleCommit_QueueMessages(void) {
    if (B(at::kMessageBusy) != 0) return;
    const unsigned count = B(at::kMessageCount);
    if (count == 0) {
        B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
        return;
    }
    unsigned char* const text = At(at::kTextRecord0);
    std::memset(text, 0, 0x20);
    const unsigned char* const record = At(at::kMessages + count * 4);
    const unsigned char* const name = EnemyObject(record[0]) + 0x80;
    SetLong(text, Long(name));
    SetLong(text + 4, Long(name + 4));
    const unsigned message = Word(record + 2);
    SetLong(text + 8, Long(name + 8));
    g.queue_push(1, 0, static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(g.msg_system_ptr(message))));
    B(at::kMessageCount) = static_cast<unsigned char>(B(at::kMessageCount) - 1);
}

// original 0x42F1D0 (PSX 0x801D2A1C): once the files are loaded - and,
// unless auto battle, once the wait 0x904B70 has counted down through 0
// (the word decremented on each frame it is tested) - the wait zeroed,
// phase on, step 0.
extern "C" void __cdecl BattleCommit_WaitLoad(void) {
    if (g.file_load_done() == 0) return;
    if ((B(at::kRoundFlags) & 0x10) == 0) {
        const unsigned wait = Word(At(at::kWaitFrames));
        SetWord(At(at::kWaitFrames), wait - 1);
        if (wait != 0) return;
    }
    const unsigned char phase = B(at::kPhase);
    SetWord(At(at::kWaitFrames), 0);
    B(at::kPhase) = static_cast<unsigned char>(phase + 1);
    B(at::kStep) = 0;
}
