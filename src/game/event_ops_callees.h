// Internal to event_ops.cpp and event_ops_fuzz.cpp: the addresses the leader's
// walk, its button tests and the event script's three placements touch that
// symbols.toml does not name, and every call they make - through pointers, so
// that the start-up fuzz can stand recording functions in for them, for the
// originals' copies and for ours alike. Calls into other groups' functions of
// the sixth round (V2, M, Z) and into functions nobody owns go through raw
// addresses here (docs/takeover-queue-round6.md, "The rule for calls across
// groups"). docs/event-ops.md.
//
// Two dispatches are not calls to a named function:
//   - Scenario_StepHook / Scenario_ArriveHook call through the chapter's
//     record, at::kChapterHooks indexed by Cond_ByteFA sign-extended; ours
//     reads the table through g.chapter_hooks, the fuzz points it (and the
//     copies' operand) at records of its own.
//   - Area_StepHook / Area_ArriveHook pick one of 38 / 8 per-area handlers by
//     Game_AreaNumber (the original through a byte index table and a jump
//     table in its own body; ours a switch); the handlers are g.area_step[] /
//     g.area_arrive[].
#pragma once

#include <cstdint>

namespace event_ops {

namespace at {

// The leader and the field.
constexpr std::uint32_t kLeader = 0x802D40;          // ObjTrio: member 0, which Field_State points at
constexpr std::uint32_t kMemberStride = 0x14C;
constexpr std::uint32_t kZoneCounter = 0x802E74;     // u16, ObjTrio +0x134 (field-event.md section 4)
constexpr std::uint32_t kLeaderPace = 0x802E68;      // u8, ObjTrio +0x128 - Field_State +0x128 read by address
constexpr std::uint32_t kLeaderFlags = 0x802E78;     // u8, ObjTrio +0x138
constexpr std::uint32_t kLeaderZone = 0x802D76;      // u16 pair: ObjTrio +0x36 / +0x3A (whole x, z)
constexpr std::uint32_t kActorRecords = 0x903A70;    // stride 0xA4 (item_use_callees.h)
constexpr std::uint32_t kActorStride = 0xA4;
constexpr std::uint32_t kPartyLists = 0x904062;      // two 3-byte lists of member ids (field-event.md section 2)
constexpr std::uint32_t kMemberRecord = 0x66972C;    // u8 per member id: its actor record (MoveScript_EffectState)
constexpr std::uint32_t kMemberSlot = 0x669738;      // u8 per member id (field-event.md section 2)
constexpr std::uint32_t kMemberPositions = 0x7E06E0; // x, z dwords per member, 8 bytes apart
constexpr std::uint32_t kRunOption = 0x903A5E;       // u8 compared with "the run button is held"
constexpr std::uint32_t kBlockByte = 0x905B82;       // u8, Field_LeaderFrame counts it down (field-event.md)
constexpr std::uint32_t kExitKind = 0x905B88;        // u8, the fourth argument of Field_ChangeArea
constexpr std::uint32_t kExitArea = 0x937F82;        // u16, the first
constexpr std::uint32_t kExitX = 0x903860;           // dword, the second
constexpr std::uint32_t kExitZ = 0x90384C;           // dword, the third
constexpr std::uint32_t kTalkMode = 0x9039F3;        // u8, set to 0x37 / 0x1B by Field_LeaderCellEvent
constexpr std::uint32_t kCellCycle = 0x9045FA;       // u8, 0..2
constexpr std::uint32_t kReturnPoint = 0x904148;     // x, z dwords, the area word at +8, a counter byte at +0xA
constexpr std::uint32_t kStoryFlags = 0x904030;      // Cond_Flags + 0xA0 (item_use_callees.h)
constexpr std::uint32_t kPlacedFlags = 0x9040CC;     // the bits EventOp_9x tests by sprite +5 (PSX 0x80144FC0)
constexpr std::uint32_t kScreenCounter = 0x929EE0;   // s32 tested against 0xF0 by Field_EncounterDue
constexpr std::uint32_t kButtonConfirm = 0x903580;   // u16 each: the field's buttons (input-script.md section 4)
constexpr std::uint32_t kButtonRun = 0x903582;
constexpr std::uint32_t kButtonHeld = 0x903586;
constexpr std::uint32_t kButtonSwap = 0x903588;
constexpr std::uint32_t kButtonCheck = 0x90358C;
constexpr std::uint32_t kWalkDelta = 0x6696DC;       // s32 x, z per direction, 8 bytes apart (the step's unit)
constexpr std::uint32_t kCellDelta = 0x66971C;       // s8 x, z per direction, 2 bytes apart
constexpr std::uint32_t kStepSounds = 0x660960;      // u16 per byte 0x903851
constexpr std::uint32_t kSwapKinds = 0x660A34;       // three bytes Party_CanSwap looks a member's +2 up in
constexpr std::uint32_t kFacingPose = 0x660A38;      // u8 per Field_State +0x89
constexpr std::uint32_t kChapterHooks = 0x662C80;    // a record pointer per chapter (Cond_ByteFA): +8 step, +0xC arrive
// The scratch words (DamageScratch 0x903850 and after, PSX 0x1F800000).
constexpr std::uint32_t kScratch = 0x903850;         // u16 / u8: the count, a found cell's x, the slope flag
constexpr std::uint32_t kScratch2 = 0x903852;        // u16: a found cell's z, the animation bank
constexpr std::uint32_t kCellX = 0x903854;           // u16 each: the step's target cell
constexpr std::uint32_t kCellZ = 0x903856;
constexpr std::uint32_t kTargetX = 0x903858;         // s32 each: the step's target position
constexpr std::uint32_t kTargetZ = 0x90385C;         // Scratch_Swap

}  // namespace at

// Functions nobody owns, and other groups' of this round - raw addresses.
namespace fn {
constexpr std::uint32_t kEncounterArea = 0x5317F0;   // void(): 0x937F82 from the area table 0x660A90
constexpr std::uint32_t kExitGateway = 0x531820;     // u8(): an exit from 0x660AB8 / 0x660B08
constexpr std::uint32_t kExitFromCell = 0x531AF0;    // void(): an exit from the cell's list
constexpr std::uint32_t kCellAroundLarge = 0x531120; // u8(u8): Field_CellAround for a sprite with +0x70
constexpr std::uint32_t kCellHook = 0x56D7A0;        // int(x, z): the chapter's +0x10, then 0x56E670
constexpr std::uint32_t kSetCell = 0x579F00;         // void(short x, short z, u8): AreaMap byte store
constexpr std::uint32_t kPartyVisible = 0x591F30;    // u8(u8, u8): moves Sprite_Current
constexpr std::uint32_t kMemberFits = 0x535C50;      // u8(x, z, slot, 0x10, 1) (group V2)
constexpr std::uint32_t kLeaderMove = 0x536670;      // void(): +0x34 / +0x38 / +0x3E by the step (group V2)
constexpr std::uint32_t kLeaderFollow = 0x5345E0;    // void() (group V2)
constexpr std::uint32_t kLeaderGround = 0x535F50;    // void() (group V2)
constexpr std::uint32_t kStepCode = 0x526DB0;        // u8(): what lies ahead (group Z)
constexpr std::uint32_t kSlopeAt = 0x5725C0;         // long(x, z, direction dword): AreaMap_Slope (group M)
constexpr std::uint32_t kTestFB = 0x572650;          // MoveCmd_TestFB, u8(short, short) (group M)
constexpr std::uint32_t kFaceObject = 0x579D70;      // EventObj_Face, Capcom's
}  // namespace fn

// The per-area handlers, by case (docs/event-ops.md section 6).
constexpr unsigned kStepCases = 38, kArriveCases = 8;
extern const std::uint32_t kStepHandlers[kStepCases];
extern const std::uint32_t kArriveHandlers[kArriveCases];
constexpr unsigned kNoArgsCase = 6;   // Area_StepHook's case 6 calls 0x40EB90 without arguments

using Test = unsigned char (__cdecl*)();
using Hook = unsigned char (__cdecl*)(long, long);
struct ChapterHooks {   // the record at::kChapterHooks points at, as these two read it
    void* unused[2];
    Hook step;      // +8
    Hook arrive;    // +0xC
};

struct Callees {
    // this file's functions, each through here so each is tested alone
    Test talk_test, swap_test, menu_test, check_test, request4_test, request9_test;  // the six in Field_LeaderWalk's order
    unsigned char (__cdecl* direction)();
    unsigned char (__cdecl* step_target)();
    unsigned char (__cdecl* push_objects)();
    unsigned char (__cdecl* push_count)();
    void (__cdecl* set_pace)();
    unsigned char (__cdecl* encounter_due)();
    unsigned char (__cdecl* step_tick)();
    unsigned char (__cdecl* effect_test)(unsigned);
    void (__cdecl* step_cell)();
    unsigned char (__cdecl* can_swap)();
    unsigned char (__cdecl* effect_ahead)();
    const unsigned char* (__cdecl* passage_ahead)();
    unsigned char (__cdecl* facing_object)();
    unsigned char (__cdecl* object_ahead)(long, long, unsigned);
    unsigned char (__cdecl* cell_around)(unsigned);
    unsigned char (__cdecl* cell_around_near)(unsigned);
    void (__cdecl* talk_to)(unsigned);
    int (__cdecl* step_hook)(long, long);
    int (__cdecl* arrive_hook)(long, long);
    int (__cdecl* area_step)(long, long);
    int (__cdecl* area_arrive)(long, long);
    unsigned char (__cdecl* return_gate)(long, long);
    const ChapterHooks* const* chapter_hooks;   // entry 0 here; indexed -128..127
    // other files' functions
    unsigned char (__cdecl* leader_animation)(unsigned);           // Field_LeaderAnimation
    void (__cdecl* clear_steps)();                                 // Sprite_ClearSteps
    unsigned char (__cdecl* ensure_animation)(unsigned char);      // Sprite_EnsureAnimation
    unsigned char (__cdecl* byte_at)(short, short);                // AreaMap_ByteAt
    void (__cdecl* zone_roll)(unsigned);                           // Field_ZoneCounterRoll
    int (__cdecl* party_count)(unsigned);                          // Party_Count
    const unsigned char* (__cdecl* zone_at)(unsigned, unsigned);   // Area_ZoneAt
    int (__cdecl* rand)();                                         // Rand, the CRT's
    void (__cdecl* play_effect)(unsigned short);                   // Sound_PlayEffect
    unsigned char (__cdecl* link_at)(unsigned, unsigned);          // Area_LinkAt
    long (__cdecl* ground_at)(long, long);                         // MapView_GroundAt
    unsigned char (__cdecl* script_tick)();                        // Sprite_ScriptTick
    unsigned char (__cdecl* object_at)(long, long, unsigned);      // Sprite_ObjectAt
    unsigned char (__cdecl* point_in_reach)(int, int, short, int, const unsigned char*);  // Sprite_PointInReach
    void (__cdecl* change_area)(unsigned, int, int, unsigned);     // Field_ChangeArea
    unsigned char (__cdecl* flags_test)(const unsigned char*, unsigned);  // Flags_Test
    void (__cdecl* flags_clear)(unsigned char*, unsigned);         // Flags_Clear
    void (__cdecl* reset)();                                       // EventObj_Reset
    unsigned char (__cdecl* set_bank)(unsigned short);             // Sprite_SetAnimationBank
    long (__cdecl* elevation)(long, long);                         // AreaMap_Elevation
    void (__cdecl* set_flags)(const unsigned char*);               // EventObj_SetFlags
    void (__cdecl* set_animation)(unsigned char);                  // Sprite_SetAnimation
    void (__cdecl* face)();                                        // EventObj_Face (Capcom's)
    // raw addresses (namespace fn)
    void (__cdecl* encounter_area)();
    unsigned char (__cdecl* exit_gateway)();
    void (__cdecl* exit_from_cell)();
    unsigned char (__cdecl* cell_around_large)(unsigned);
    int (__cdecl* cell_hook)(unsigned, unsigned);
    void (__cdecl* set_cell)(unsigned, unsigned, unsigned);
    unsigned char (__cdecl* party_visible)(unsigned, unsigned);
    unsigned char (__cdecl* member_fits)(long, long, unsigned, unsigned, unsigned);
    void (__cdecl* leader_move)();
    void (__cdecl* leader_follow)();
    void (__cdecl* leader_ground)();
    unsigned char (__cdecl* step_code)();
    long (__cdecl* slope_at)(long, long, std::uint32_t);
    unsigned char (__cdecl* test_fb)(unsigned, unsigned);
    // the per-area handlers; step[kNoArgsCase] is called without arguments
    Hook area_step_handlers[kStepCases];
    Hook area_arrive_handlers[kArriveCases];
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=event_ops: the start-up fuzz, event_ops_fuzz.cpp. Clones every
// original before EventOps_Inject patches it.
void SelfTest();

}  // namespace event_ops
