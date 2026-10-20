// Internal to mode_flow.cpp and mode_flow_fuzz.cpp: every call the top-level
// task flow makes, through pointers, so that the start-up fuzz can stand
// recording functions in for them - for the originals' copies and for ours
// alike. Most callees are other modules' or Capcom's; the ones in this file
// are called through here all the same, so that each function is tested
// alone. docs/mode-flow.md.
//
// Two dispatches are not calls to a named function:
//   - Transition_Task builds its 21-entry table on its own stack out of
//     `mov [esp + k], imm32` and calls `[esp + eax * 4]`; the table is
//     `transitions` below, and the fuzz re-aims the copy's immediates.
//   - Field_Task calls through GameMode_Handlers 0x656A44, a table in .data;
//     ours reads it from memory as the original does, and the fuzz swaps its
//     twelve entries for recorders.
//   - Area_Enter calls through the area descriptor's +0x40 (`call eax`); the
//     fuzz points Area_Descriptors at descriptors of its own.
#pragma once

#include <cstdint>

namespace mode_flow {

namespace at {

// kTransitionKind: now ModeFlow_TransitionKind (symbols.toml). u16: Transition_Start's kind, Transition_Task's index (PSX 0x80143C90)
constexpr std::uint32_t kGameModeHandlers = 0x656A44; // 12 code pointers, Field_Task's table (PSX 0x801C8B0C, FUN_80198068's s0)
// The six bytes a fade leaves behind: 0 after a fade to black, 0xFF after one
// to white (0x495620, 0x4956A0, 0x4955A0). Three at 0x90393D and three at
// 0x9038AD; what reads them is unread.
// kFadeHoldA: now ModeFlow_FadeHoldA (symbols.toml).
// kFadeHoldB: now ModeFlow_FadeHoldB (symbols.toml).
// The play clock: hours, minutes, seconds, frames (PSX 0x80144FBC..BF).
constexpr std::uint32_t kClock = 0x9040C8;
// Two countdowns of the same shape, hours / minutes / seconds / frames, each
// counting down to 0:00:00 (0x90465C and 0x904660; readers unread).
// kCountdownA: now ModeFlow_CountdownA (symbols.toml).
// kCountdownB: now Rest3c_Byte660 (symbols.toml).
// The area-change cells Field_ChangeArea stashes (window_task_callees.h).
// kPendingArea: now AreaW0b_Place (symbols.toml).
constexpr std::uint32_t kPendingX = 0x903860;
// kPendingZ: now AreaEntry_PendingZ (symbols.toml).
// kPendingFlags: now AreaEntry_PendingFlags (symbols.toml).
// kPendingKind: now Area_EnterTransition (symbols.toml).
// kAreaTrack: now AreaW1f_MusicCurrent (symbols.toml).
// kMusicTrack: now Music_Track (symbols.toml).
// kAreaTransition: now ModeFlow_AreaTransition (symbols.toml). GameMode_Field's Transition_Start kind for the area change
// The saved position a new or loaded game starts from (GameMode_Start):
// area u16 at +0, flags byte at +3, x and z dwords at +4 and +8.
// kStartArea: now FieldModes_Pending (symbols.toml).
// The entry walk (Area_EntryWalk): on, direction, a zero, and x / z.
// kWalk: now EventLeader_PendingFlag (symbols.toml).
// kWalkX: now EventLeader_PendingX (symbols.toml).
// kWalkZ: now EventLeader_PendingZ (symbols.toml).
constexpr std::uint32_t kWalkPoses = 0x66ADBC;       // a byte per direction
constexpr std::uint32_t kMemberWalk = 0x802E77;      // two bytes in each 0x14C-byte member record
// Area_Enter's copies of the entry state.
constexpr std::uint32_t kLastArea = 0x802290;        // u16 (PSX unread)
constexpr std::uint32_t kLastZone = 0x905E68;        // u8, Cond_ByteFD's value on entry
// kLastKind2X: now MoveScript_TintRecords[0x21c] (symbols.toml). s32
// kLastKind2Z: now MoveScript_TintRecords[0x220] (symbols.toml). s32
constexpr std::uint32_t kKind2XHigh = 0x905E66;      // Field_Kind2X's integer part
constexpr std::uint32_t kKind2ZHigh = 0x905E62;      // Field_Kind2Z's integer part
constexpr std::uint32_t kDefaultColour = 0x6BE090;   // the colour matrix an area without its own gets
// kDefaultLight: now ModeFlow_DefaultLight (symbols.toml). two dwords for Light_Angles
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). Cond_Flags + 0xA0, the story flags (title-states.md section 3)
// Boot_Task's cells.
constexpr std::uint32_t kBootClear = 0x9039E0;       // 0x42C dwords zeroed
constexpr std::uint32_t kBootClearBytes = 0x42C * 4;
// kButtons: now Gfx_UnpackScratch[0x1f000] (symbols.toml). the field's button map (input-script.md section 4)
// kButtonDefaults: now ModeFlow_ButtonDefaults (symbols.toml). its 18 bytes of defaults
// kPartyCombination: now AreaW0b_PartySet (symbols.toml).
// kConfigBytes: now ModeFlow_ConfigBytes (symbols.toml). 1, then six zeros; +2 is the window colour 0x903A5A
// kPoolA: now BattleE2_AnimSet (symbols.toml). dword, 0x8B3580
// kPoolB: now MoveScript_TintRecords[0x180] (symbols.toml). dword, 0x8C3584
// The tint and slot resets.
constexpr std::uint32_t kTintMarks = 0x7E06A0;       // 16 dwords set to -1 by MoveScript_TintReset
// kClutStripZero: now Gfx_ClutStripSource[0x1c00] (symbols.toml). the source's last 0x800 bytes, zeroed before the copy
// Window_ResetAll's range.
constexpr std::uint32_t kWindowRecords = 0x803160;
// kWindowPass: now BossSa_WindowPass (symbols.toml).

}  // namespace at

// Entries of task records a Task_Create names (the original's addresses: a
// task started at one of these reaches ours through its detour).
constexpr std::uint32_t kTransitionEntry = bof3::addr::Transition_Task;
constexpr std::uint32_t kTitleLoadEntry = bof3::addr::Title_LoadTask;
// Callees with no name in symbols.gen.h, called by address.
constexpr std::uint32_t kEntryPoint = bof3::addr::Area_LinkAt;      // GameMode_Enter's, on input bit 0: the area's entry list; unread
constexpr std::uint32_t kPlaceParty = bof3::addr::Party_DropIn;      // Area_Enter's, on flag 0x80: the party placement (field_modes' kPlaceParty)

using Handler = void (__cdecl*)();

struct Callees {
    // the scheduler, Capcom's (hand-written esp swaps)
    void (__cdecl* sleep)(int);
    void (__cdecl* task_exit)();
    void (__cdecl* task_create)(int, void*);
    void (__cdecl* clear_private)();
    // ours, in this file
    void (__cdecl* fade_sub)(int, unsigned, unsigned);
    void (__cdecl* fade_add)(int, unsigned, unsigned);
    unsigned char (__cdecl* draw_tile)(short*, int, unsigned, unsigned, unsigned);
    void (__cdecl* window_reset)();
    void (__cdecl* clock_tick)();
    void (__cdecl* transition)(unsigned char);
    void (__cdecl* wait_transition)(unsigned char);
    void (__cdecl* area_enter)(unsigned, int, int, unsigned);
    void (__cdecl* entry_walk)(unsigned, int, int);
    void (__cdecl* tint_reset)();
    void (__cdecl* slot_release)(unsigned);
    void (__cdecl* slots_release)();
    void (__cdecl* clut_restore)();
    int (__cdecl* load_done)();
    void (__cdecl* clear_rect)(int, int, int, int);
    // other modules' and Capcom's
    unsigned (__cdecl* get_tpage)(unsigned, unsigned, int, int);
    void (__cdecl* draw_mode)(unsigned char*, int, int, unsigned, unsigned long);
    void (__cdecl* commit)(unsigned, unsigned);
    void (__cdecl* set_tile)(unsigned char*);
    void (__cdecl* set_semi)(unsigned char*, unsigned);
    void (__cdecl* party_load)(unsigned);
    void (__cdecl* change_area)(unsigned, int, int, unsigned);
    void (__cdecl* entry_point)(unsigned, unsigned);
    void (__cdecl* zone_roll)(unsigned);
    void (__cdecl* field_frame)();
    void (__cdecl* effect_clear)();
    void (__cdecl* music_play)(unsigned, int);
    void (__cdecl* load_dat)(int);
    void (__cdecl* party_set_up)(long, long, unsigned);
    unsigned char (__cdecl* zone_id)(unsigned, unsigned);
    void (__cdecl* view_reset)();
    void (__cdecl* colour_matrix)(const unsigned long*);
    void (__cdecl* scenario_start)(int);
    void (__cdecl* run_placement)(const unsigned char*);
    void (__cdecl* flags_clear)(unsigned char*, unsigned);
    void (__cdecl* mode_dispatch)();
    void (__cdecl* first_frame)();
    unsigned char (__cdecl* test_fb)(short, short);
    unsigned char (__cdecl* place_party)(unsigned);
    void (__cdecl* members_frame)();
    void (__cdecl* dropped)(unsigned char);
    void (__cdecl* title_task)();
    void (__cdecl* clear_image)(const short*, unsigned char, unsigned char);
    // Transition_Task's stack-built table, as the original's immediates
    Handler transitions[21];
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (mode_flow_fuzz.cpp): clones every function of this file
// with each call out re-aimed at a recording stand-in and the stack-built
// table's immediates re-aimed in the copy, runs ours against the clones from
// the same random state, and ends the process through bof3::Fatal on any
// difference.
void SelfTest();

}  // namespace mode_flow
