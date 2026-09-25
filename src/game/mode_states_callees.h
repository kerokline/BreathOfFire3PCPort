// Internal to mode_states.cpp and mode_states_fuzz.cpp: every call the
// top-level mode handlers, the system choice, the 8 px UI draw, the field's
// per-frame entries, the field core's state steps, scenario chapter 1's
// hooks and the window cursor draw make, through pointers, so that the
// start-up fuzz can stand recording functions in for them - for the
// originals' copies and for ours alike. docs/mode_states.md.
//
// Five dispatches are not calls to a named function:
//   - GameMode_Shop 0x496290 jumps through GameMode_ShopSteps 0x656AAC on
//     Game_Step; FieldCore_State2 0x525370 through FieldCore_State2Steps
//     0x66011C on Sprite_Current +2; FieldCore_Fade 0x5258B0 through
//     FieldCore_FadeSteps 0x660158 on +3; Scena01_Frame 0x539AD0 through
//     Scena01_States 0x660D7C on the s8 0x8034E2. All four tables are .data,
//     read afresh and indexed unchecked; the fuzz swaps their words.
//   - MsgBox_SystemChoice 0x498A30 builds a sixteen-entry table on its own
//     stack (`mov [esp + k], imm32`); `choice` below, and the fuzz re-aims
//     the copy's immediates.
// Addresses of other groups' functions (the round's cross-group rule,
// docs/takeover-queue-round8.md) are raw here and never bound by name.
#pragma once

#include <cstdint>

namespace mode_states {

namespace at {

constexpr std::uint32_t kShopSteps = 0x656AAC;      // GameMode_ShopSteps, 3 entries (0x656AB8 is mode 8's)
constexpr std::uint32_t kState2Steps = 0x66011C;    // FieldCore_State2Steps, 9 entries (0x660140 is 0x525390's)
constexpr std::uint32_t kFadeSteps = 0x660158;      // FieldCore_FadeSteps, 2 entries (0x660160 is 0x525960's)
constexpr std::uint32_t kScena01States = 0x660D7C;  // Scena01_States, 3 entries (0x660D88 is state 2's run table)
constexpr std::uint32_t kScena01State = 0x8034E2;   // s8, chapter 1's state (docs/field-modes.md section 2)
constexpr std::uint32_t kShopObject = 0x929F0C;     // s8: the menu block 0x929F00's +0xC, the object the shop was opened on
constexpr std::uint32_t kPartyCombo = 0x90412C;     // u8: the party combination; bit 7 set by Shop_Close
constexpr std::uint32_t kLookTurn = 0x66C7D9;       // u8: task 0's private +9, which way Look_Return turns the pitch
constexpr std::uint32_t kLookButton = 0x903586;     // u16: the button map's word after Field_MenuButton
constexpr std::uint32_t kFlagBank = 0x929ED0;       // dword: the flag bits Flags_Test / Flags_Set are given
constexpr std::uint32_t kCounters = 0x903848;       // u8 x 4: the script counters (MoveScript_CounterOps)
constexpr std::uint32_t kEffectSlot = 0x903850;     // u8 (read back as a dword's low byte): the effect slot just taken
constexpr std::uint32_t kStep5 = 0x8034E5;          // u8: the scene's step
constexpr std::uint32_t kTimer = 0x8034E6;          // u16: the scene's timer
constexpr std::uint32_t kChoiceId = 0x7DEE64;       // u8: the message box's choice id
constexpr std::uint32_t kCursorSet = 0x7DEE65;      // u8: the list set 0x66AE2C is indexed by
constexpr std::uint32_t kCursorRow = 0x7DEE67;      // s8: the cursor's row
constexpr std::uint32_t kCursorRows = 0x66AE2C;     // 6-byte records: +2 the first row's y, +4 the row stride
constexpr std::uint32_t kWindow = 0x905B84;         // dword: the window record the cursor is drawn in (+4 x, +6 y, 12.4)
constexpr std::uint32_t kSmallUV = 0x65F5A8;        // u8 pairs by colour >> 4: the 8 px draw's texture u and v

}  // namespace at

// Callees with no name in symbols.gen.h: other groups' functions this round,
// and one unread function, called by address.
constexpr std::uint32_t kMenuStates = 0x589970;   // group DH's: jmp [0x6672B4 + 4 * u8 0x929F00], the field menu
constexpr std::uint32_t kShopStates = 0x57F500;   // group DF's: the shop overlay's first table
constexpr std::uint32_t kAfterShop = 0x4560D0;    // unread: called by Shop_Close after DAT 0x12A is loaded

using Handler = void (__cdecl*)();

struct Callees {
    // MsgBox_SystemChoice's stack-built table, as the original's immediates:
    // 0x498AD0, 0x498B00 .. 0x498BC0 (0x30 apart), 0x498BE0 .. 0x498D00
    // (0x20 apart) - MsgBox_SysChoice80..8F (item_use.cpp)
    Handler choice[16];
    // Where MsgBox_SystemChoice's caller resumes for choice id 0x90: the two
    // commits' tails 0x4981F1 and 0x4983F1 (the fuzz aims them at its copies)
    std::uint32_t tails[2];
    // ours, in this file
    void (__cdecl* look_return)();
    void (__cdecl* loading_frame)();
    void (__cdecl* menu_frame)();
    // other modules' and Capcom's
    void (__cdecl* field_frame)();
    void (__cdecl* load_dat)(int);
    int (__cdecl* load_done)();
    void (__cdecl* sleep)(int);
    void (__cdecl* clut_copy_row)(unsigned);
    void (__cdecl* face_direction)(unsigned char);
    void (__cdecl* bank_file)(unsigned);
    void (__cdecl* after_shop)();
    void (__cdecl* emit_glyph)(int, int, int, int, int, int, int);
    void (__cdecl* areamap_frame)();
    void (__cdecl* extra_screens)();
    void (__cdecl* update_screens)();
    void (__cdecl* objects_screen)();
    void (__cdecl* run_effects)();
    void (__cdecl* tint_frame)();
    void (__cdecl* draw_frame)();
    void (__cdecl* mode_dispatch)();
    void (__cdecl* menu_states)();
    void (__cdecl* task_records)();
    void (__cdecl* shop_states)();
    void (__cdecl* shade_begin)();
    unsigned char (__cdecl* shade_step)(unsigned);
    unsigned char (__cdecl* flags_test)(const unsigned char*, unsigned);
    void (__cdecl* flags_set)(unsigned char*, unsigned);
    void (__cdecl* set40)();
    int (__cdecl* music_load)(unsigned);
    void (__cdecl* music_stop)(int);
    void (__cdecl* sound)(unsigned short);
    unsigned char (__cdecl* effect_find)();
    void (__cdecl* call_a)(unsigned);
    void (__cdecl* setup_entries)();
    unsigned char (__cdecl* test_fb)(short, short);
    void (__cdecl* change_area)(unsigned, int, int, unsigned);
    void (__cdecl* draw_hand)(int, int, int);
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (mode_states_fuzz.cpp): clones every function of this
// file with each call out re-aimed at a recording stand-in, the system
// choice's stack table re-aimed in its copy, the chapter's jump tables
// relocated and the four .data tables swapped for recorders; runs ours
// against the clones from the same random state, and ends the process
// through bof3::Fatal on any difference.
void SelfTest();

}  // namespace mode_states
