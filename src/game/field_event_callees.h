// Internal to field_event.cpp and field_event_fuzz.cpp: every call the 31
// functions make, through pointers, so that the start-up fuzz can stand
// recording functions in for them - for the original's copies and for ours
// alike. The callees this module implements are called through the same
// pointers, so each function is tested alone. docs/field-event.md.
#pragma once

#include <cstdint>

namespace field_event {

using Handler = void (__cdecl*)();

// The addresses this module reads or writes that symbols.toml does not name
// (docs/field-event.md, section 2). Kept here, not as [[data]], so that no
// other group of the parallel round can bind one twice.
namespace at {
constexpr std::uint32_t kLeaderTimer = 0x905B82;      // byte, counted down by Field_LeaderFrame
constexpr std::uint32_t kPending = 0x904EF0;          // byte: a pending jump; +1 its index
constexpr std::uint32_t kLeaderStates = 0x660918;     // 15 handlers, by Sprite_Current +1
constexpr std::uint32_t kControlStates = 0x660954;    // 3 handlers, by Sprite_Current +2
constexpr std::uint32_t kPendingJumps = 0x660B60;     // 4 handlers, by 0x904EF1
constexpr std::uint32_t kZoneCounter = 0x802E74;      // word, ObjTrio +0x134
constexpr std::uint32_t kLeaderXHigh = 0x802D76;      // ObjTrio +0x36, the integer x's low byte
constexpr std::uint32_t kLeaderZHigh = 0x802D7A;      // ObjTrio +0x3A
constexpr std::uint32_t kZoneBases = 0x660A24;        // 2 bytes a zone: base, most added
constexpr std::uint32_t kZoneLists = 0x668D80;        // an 8-byte record list per area
constexpr std::uint32_t kAnimationRemap = 0x6608D8;   // 2 bytes an animation
constexpr std::uint32_t kPartyLists = 0x904062;       // two 3-byte lists of member ids, 0xFF-ended
constexpr std::uint32_t kPartySetCurrent = 0x90412C;  // the loaded party set; bit 7 from PartySet_Select mode 0
constexpr std::uint32_t kPartySets = 0x669750;        // 19 sorted rows of 3 member ids
constexpr std::uint32_t kMemberFlags = 0x669738;      // a byte per member id, to sprite +0x2B
constexpr std::uint32_t kBankTable = 0x813580;        // dword offsets from itself, by column
constexpr std::uint32_t kPaletteBase = 0x80D380;      // 64 bytes a slot, inside Gfx_ClutStripSource
constexpr std::uint32_t kActorRecords = 0x903A70;     // 0xA4 bytes each; Field_ActorStates is +0x10 of the first
constexpr std::uint32_t kFindIds = 0x903850;          // PartySet_Find's three ids, then its swap byte at +4
constexpr std::uint32_t kJoinState = 0x9045FC;        // two dwords and a byte, Party_JoinReset's
constexpr std::uint32_t kFlagBits = 0x904030;         // the bit array Field_PartySetUp tests flag 0x77 in
constexpr std::uint32_t kMemberOffsets = 0x660B40;    // 8 bytes (dx, dz) per direction / 2
constexpr std::uint32_t kTintSprites = 0x7E0708;      // MoveScript_TintRecords +8: each record's sprite
constexpr std::uint32_t kObject905DA0 = 0x905DA0;     // the object MapView_CheckHeightScale singles out
constexpr std::uint32_t kClutMirror = 0x80F580;       // Gfx_ClutStrip
constexpr std::uint32_t kClutSource = 0x80B580;       // Gfx_ClutStripSource
}  // namespace at

struct Callees {
    // Field_LeaderFrame
    void (__cdecl* restore_clut)();
    void (__cdecl* copy_input)();
    const std::uint32_t* leader_states;
    // Field_LeaderStart
    void (__cdecl* clear_steps)();
    long (__cdecl* ground_at)(long, long);
    unsigned char (__cdecl* leader_animation)(unsigned);
    // Field_LeaderControl
    const std::uint32_t* control_states;
    unsigned char (__cdecl* script_tick)();
    // Field_LeaderStand
    unsigned char (__cdecl* test_535120)(unsigned);
    unsigned char (__cdecl* test_535240)(unsigned);
    unsigned char (__cdecl* test_5301f0)();
    unsigned char (__cdecl* tests[7])();   // 0x531950 0x530920 0x5302C0 0x530380 0x530800 0x5303E0 0x530430
    void (__cdecl* walk)();                // 0x530480
    // Field_ZoneCounterRoll
    const unsigned char* (__cdecl* zone_at)(unsigned, unsigned);
    int (__cdecl* rand)();
    int (__cdecl* party_count)(unsigned);
    unsigned char (__cdecl* has_item)(unsigned, unsigned, unsigned);   // 0x535310
    // Field_LeaderAnimation
    unsigned char (__cdecl* set_animation)(unsigned);   // 0x589330
    // Field_PartySetUp
    unsigned char (__cdecl* flags_test)(const unsigned char*, unsigned);
    void (__cdecl* set_bank)(unsigned short);
    void (__cdecl* member_sprite)(unsigned, unsigned);
    void (__cdecl* party_load)(unsigned);
    void (__cdecl* release_tint)(unsigned char*);
    void (__cdecl* load_palette)(unsigned short*, unsigned);
    void (__cdecl* position)(long, long, unsigned);
    void (__cdecl* position_alt)(long, long, unsigned);   // 0x533690
    // Field_PartyFirstFrame
    void (__cdecl* clear_active)();
    void (__cdecl* leader_frame)();
    void (__cdecl* member_frame)();
    // Field_PendingJump
    const std::uint32_t* pending_jumps;
    // Field_PartyLoad
    void (__cdecl* clear_all)();
    unsigned char (__cdecl* set_tint)(unsigned char*, unsigned char, unsigned char, unsigned char, unsigned char);
    void (__cdecl* member_timers)();
    // Party_Join
    void (__cdecl* join_reset)();
    void (__cdecl* clear_state)(unsigned);
    // PartySet_Load, _LoadFirst, _LoadSecond, _Find, _Select
    void (__cdecl* load_first)(unsigned, unsigned, unsigned);
    void (__cdecl* load_second)(unsigned, unsigned, unsigned, unsigned);
    int (__cdecl* load_done)();
    void (__cdecl* sleep)(int);
    unsigned char (__cdecl* find)(unsigned, unsigned, unsigned);
    void (__cdecl* select)(unsigned, unsigned);
    void (__cdecl* set_error)();   // 0x536A60
    void (__cdecl* load_dat)(int);
    // MapView_GroundAt
    void (__cdecl* height_check)();
    long (__cdecl* elevation)(long, long);
    // Sprite_ReleaseTint
    void (__cdecl* tint_release)(unsigned char);
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (field_event_fuzz.cpp): clones every function with each
// call re-aimed at a recording stand-in, runs ours against the clones, and
// ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace field_event
