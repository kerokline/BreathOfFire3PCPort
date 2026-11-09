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
unsigned char* MemberBattlePhases(unsigned index) { return battle_phases::At(::bof3::Img(ObjTrio_addr) + index * battle_phases::at::kMemberSize); }
unsigned char* EnemyObject(unsigned index) { return battle_phases::At(::bof3::Img(Battle_Enemies_addr) + index * battle_phases::at::kEnemySize); }
unsigned char* Window(unsigned index) { return battle_phases::At(::bof3::Img(WindowRecords_addr) + index * battle_phases::at::kWindowSize); }
int S8(std::uint32_t address) { return static_cast<signed char>(BBattlePhases(address)); }

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
    if ((BBattlePhases(::bof3::Img(at::kRoundFlags.v)) & 2) == 0) {
        const unsigned phase = BBattlePhases(::bof3::Img(at::kPhase.v));
        const bool window_held = WindowRecords[0].in_use != 0 && phase != 5;
        const bool flag_held = (BattleMisc_BannerKinds & 2) != 0 && phase != 5;
        if (!window_held && !flag_held) battle_phases::g.phase_dispatch();
    }
    battle_phases::g.party_run_states();
    battle_phases::g.enemy_run_all();
    battle_phases::g.banner_dispatch();
    battle_phases::g.area_map_frame();
    battle_phases::g.task_run_all();
    battle_phases::g.party_update_screens();
    battle_phases::g.enemy_update_screens();
    if (EventBattle_Id != 0) battle_phases::g.objects_screen();
    else battle_phases::g.update_object_screens();
    battle_phases::g.party_extra_screens();
    battle_phases::g.effect_run_objects();
    battle_phases::g.field_run_slots();
    battle_phases::g.tint_frame();
    battle_phases::g.run_task_records();
    battle_phases::g.draw_frame();
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
    const unsigned index = BattleActions_Step;
    if (index >= 2) bof3::Fatal("BattleStart_Dispatch: step %u, past the two-entry table", index);
    battle_phases::g.start_steps[index]();
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

extern "C" void __cdecl Battle_Init(void) {
    battle_phases::g.banner_clear_all();
    const unsigned count = BBattlePhases(::bof3::Img(at::kActorCount.v));
    Battle_PartyStanding = 0;
    if (count != 0) {
        unsigned i = 0;
        do {
            if (battle_phases::g.actor_is_out(i) == 0) Battle_PartyStanding = static_cast<unsigned char>(Battle_PartyStanding + 1);
            ++i;
        } while (i < BBattlePhases(::bof3::Img(at::kActorCount.v)));
    }
    BattleE4_MenuActor = reinterpret_cast<records::PartyMember*>(MemberBattlePhases(0));
    BossSb_Countdown = 0;
    BattleE5_GeneRow = 0;
    BattleE1_Roster = 0;
    BBattlePhases(::bof3::Img(at::kCommand.v)) = 0;
    BBattlePhases(::bof3::Img(at::kCrossGrow.v)) = 8;
    battle_phases::SetWord(battle_phases::At(::bof3::Img(at::kRoundFlags.v)), 0);
    Battle_EndFlags = 0;
    battle_phases::SetLong(battle_phases::At(::bof3::Img(at::kTurnCounter.v)), 1);
    BattleFlow_ZennyTotal = 0;
    Battle_ExpTotal = 0;
    BattlePhases_AE6 = 0;
    BossSe_Script = 0;
    BattlePhases_B7A = 0;
    battle_phases::SetWord(battle_phases::At(::bof3::Img(at::kB98.v)), 0);
    Battle_DropCount = 0;
    BattleActions_Pending = 0;
    BattlePhases_B8B = 0;
    BattleFlow_ActorAt = 0;
    BattleFlow_AnimGate = 0;
    BattleDamage_AiByteB97 = 0;
    BattlePhases_B89 = 0;
    MoveScript_FAWord = 0;
    if (battle_phases::g_init_pick_clear) BattleE4_Picking = 0;
    battle_phases::g.init_encounter_kind();
    battle_phases::g.task_clear_all();
    battle_phases::g.init_actor_contexts();
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = MemberBattlePhases(m);
        auto* const member_rec = rv::As<records::PartyMember>(p);
        if ((member_rec->flags & 1) == 0) continue;
        std::memcpy(p + 0xC0, p + 0xA0, 0x20);
        const unsigned quarter = static_cast<unsigned>(member_rec->max_hp) >> 2;
        if (member_rec->hp < quarter) p[0x91] = static_cast<unsigned char>(p[0x91] | 0x20);
        else member_rec->status = member_rec->status & 0xDFFFu;
        member_rec->flags_130 = 0;
        member_rec->flags_134 = 0;
        p[0x125] = 0;
        p[0x12D] = 0;
        member_rec->count_4000 = 0;
        member_rec->count_800 = 0;
        member_rec->count_40 = 0;
        member_rec->count_80 = 0;
        battle_phases::SetLong(p + 0x138, 0);
        battle_phases::SetLong(p + 0x13C, 0);
    }
    battle_phases::g.apply_stat_mods();
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = MemberBattlePhases(m);
        auto* const member_rec = rv::As<records::PartyMember>(p);
        if ((member_rec->flags & 1) != 0) std::memcpy(p + 0xA0, p + 0xC0, 0x20);
    }
    for (unsigned i = 0; i < 16; ++i) battle_phases::SetWord(battle_phases::At(::bof3::Img(Gfx_ClutStrip_addr) + 0x1fe0 + 2 * i), battle_phases::Word(battle_phases::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1fe0 + 2 * i)));
    battle_phases::g.clut_strip_copy_row(0x1A);
    battle_phases::g.clut_strip_copy_row(0x1B);
    for (unsigned a = 0; a <= 10; ++a) {
        if (a <= 2) {
            unsigned char* const p = MemberBattlePhases(a);
            auto* const member_rec = rv::As<records::PartyMember>(p);
            if ((member_rec->flags & 1) == 0) continue;
            Sprite_Current = p;
        } else {
            if (battle_phases::g.actor_is_out(a) != 0) continue;
            if (EventBattle_Id == 0x25) continue;
            Sprite_Current = EnemyObject(a - 3);
        }
        battle_phases::g.set_clut_stp();
    }
    if (EventBattle_Id == 0) battle_phases::g.load_dat(static_cast<int>(Game_AreaNumber + 0x15D));
    const unsigned char event = EventBattle_Id;
    for (unsigned i = 0; i < 4; ++i) battle_phases::SetLong(battle_phases::At(::bof3::Img(BattlePhases_B24_addr) + 4 * i), 0);
    if (event != 0) reinterpret_cast<void (__cdecl*)(int)>(PtrAtBattlePhases(::bof3::Img(at::kEventHook.v)))(6);
    BattleActions_Step = static_cast<unsigned char>(BattleActions_Step + 1);
}

// original 0x42E730 (no PSX twin paired): phase 0 step 1. Entry 0x904AA2 of
// a four-entry table the original builds on its own stack - the intro's
// steps below. The index is not checked; ours aborts past the table (as
// BattleStart_Dispatch).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleIntro_Dispatch(void) {
    const unsigned index = BattleE4_Step2;
    if (index >= 4) bof3::Fatal("BattleIntro_Dispatch: sub-step %u, past the four-entry table", index);
    battle_phases::g.intro_steps[index]();
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
    BattleWindows_MsgHead = 0;
    BBattlePhases(::bof3::Img(at::kC2A0.v) + 1) = 0;
    auto* const window_first = rv::As<records::WindowRecord>(Window(battle_phases::g.window_alloc(0x14, 3) & 0xFF));
    window_first->kind = 7;
    window_first->state = 0;
    window_first->x = 0x14;
    window_first->y = 0xFFEA;
    if (BattlePhases_Initiative == 1) battle_phases::g.queue_push(2, 0x30, Low32(battle_phases::g.msg_system_ptr(0x16)));
    if (BattlePhases_Initiative == 2) battle_phases::g.queue_push(2, 0x30, Low32(battle_phases::g.msg_system_ptr(0x17)));
    for (unsigned m = 0; m < 3; ++m) {
        auto* const member_rec = rv::As<records::PartyMember>(MemberBattlePhases(m));
        if ((member_rec->flags & 1) == 0) continue;
        battle_phases::g.window_alloc(0xD + m, 3);
        unsigned char* const w = Window(0xD + m);
        auto* const window_rec = rv::As<records::WindowRecord>(w);
        const unsigned pair = (member_rec->direction + member_rec->char_id * 4u) * 2u;
        window_rec->kind = 5;
        window_rec->state = 0;
        const unsigned facing = ((BBattlePhases(::bof3::Img(at::kFacing.v)) ^ 2u) >> 1) * 2u;
        window_rec->x = static_cast<unsigned>(S8(::bof3::Img(at::kFacingOffsets.v) + facing) + S8(::bof3::Img(at::kStatusOffsets.v) + pair) + static_cast<std::uint16_t>(member_rec->screen_x));
        window_rec->y = static_cast<unsigned>(S8(::bof3::Img(at::kStatusOffsets.v) + pair + 1) + static_cast<std::uint16_t>(member_rec->screen_y) + 0xC);
        w[0xA] = static_cast<unsigned char>(m);
        w[8] = 0;
        w[9] = 0;
    }
    battle_phases::g.open_status(0);
    battle_phases::g.open_enemy_names();
    BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// original 0x42E8C0 (PSX 0x801D1AAC): the sub-step on once window 1's +3 is
// 1.
extern "C" void __cdecl BattleIntro_WaitWindow(void) {
    if (rv::As<records::WindowRecord>(Window(1))->state == 1) BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// original 0x42E8D0 (PSX 0x801D1AE0): the first sub-window opened with 0;
// then, when the initiative byte 0x904AE4 is 2 or more, window 2's +3 = 2
// and the seven cross growth bytes 0x904ABC.. zeroed, else all seven 0x10;
// the sub-step on.
extern "C" void __cdecl BattleIntro_PrepareCross(void) {
    battle_phases::g.open_sub1(0);
    if (BattlePhases_Initiative >= 2) {
        rv::As<records::WindowRecord>(Window(2))->state = 2;
        battle_phases::SetLong(battle_phases::At(::bof3::Img(at::kCrossGrow.v)), 0);
        battle_phases::SetWord(battle_phases::At(::bof3::Img(at::kCrossGrow.v) + 4), 0);
        BBattlePhases(::bof3::Img(at::kCrossGrow.v) + 6) = 0;
    } else {
        battle_phases::SetLong(battle_phases::At(::bof3::Img(at::kCrossGrow.v)), 0x10101010);
        battle_phases::SetWord(battle_phases::At(::bof3::Img(at::kCrossGrow.v) + 4), 0x1010);
        BBattlePhases(::bof3::Img(at::kCrossGrow.v) + 6) = 0x10;
    }
    BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// original 0x42E930 (PSX 0x801D1B8C): once the initiative byte is 2 or more
// or window 2's +3 is 1, no queued banner is pending and the files are
// loaded: the second sub-window opened with 2 (1 when the initiative byte,
// re-read, is below 2), the third with 2; phase on, step and sub-step 0.
extern "C" void __cdecl BattleIntro_Finish(void) {
    if (BattlePhases_Initiative < 2 && rv::As<records::WindowRecord>(Window(2))->state != 1) return;
    if (battle_phases::g.queue_pending() != 0) return;
    if (battle_phases::g.file_load_done() == 0) return;
    battle_phases::g.open_sub2(BattlePhases_Initiative >= 2 ? 2u : 1u);
    battle_phases::g.open_sub3(2);
    BBattlePhases(::bof3::Img(at::kPhase.v)) = static_cast<unsigned char>(BBattlePhases(::bof3::Img(at::kPhase.v)) + 1);
    BattleActions_Step = 0;
    BattleE4_Step2 = 0;
}

// ===========================================================================
// Phase 1: the command input
// ===========================================================================

// ===========================================================================
// Phase 2: the commit
// ===========================================================================

// The module's inject, in the live file as every module's is (the 64-bit
// DLL leaves the fuzz files out, docs/x64-build-round.md 2); the self-test
// is battle_phases_fuzz.cpp's, hosted only.
void BattlePhases_Inject() {
    if (bof3::WantsShadow("battle_phases")) battle_phases::SelfTest();
    BattlePhases_ArmPickClear();  // DIV-0163: after the self-test, which compared Capcom's resets
    BOF3_INJECT(Battle_Frame);
    BOF3_INJECT(BattleStart_Dispatch);
    BOF3_INJECT(Battle_Init);
    BOF3_INJECT(BattleIntro_Dispatch);
    BOF3_INJECT(BattleIntro_OpenWindows);
    BOF3_INJECT(BattleIntro_WaitWindow);
    BOF3_INJECT(BattleIntro_PrepareCross);
    BOF3_INJECT(BattleIntro_Finish);
}
