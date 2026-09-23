// Internal to member_sprites.cpp and member_sprites_fuzz.cpp: every call the
// 21 functions make, through pointers, so that the start-up fuzz can stand
// recording functions in for them - for the original's copies and for ours
// alike. The callees this module implements are called through the same
// pointers, so each function is tested alone. docs/member-sprites.md.
//
// Calls into other groups' functions (round six: V1, V2, M) and into
// functions nobody owns go through raw addresses here, never through a
// symbols.toml name (docs/takeover-queue-round6.md, "The rule for calls
// across groups").
#pragma once

#include <cstdint>

namespace member_sprites {

using Handler = void (__cdecl*)();

// The addresses this module reads or writes that symbols.toml does not name
// (docs/member-sprites.md, section 2).
namespace at {
constexpr std::uint32_t kMemberStates = 0x65F960;    // 9 handlers, by Sprite_Current +1
constexpr std::uint32_t kControlStates = 0x65F99C;   // 2 handlers, by Sprite_Current +2
constexpr std::uint32_t kCells = 0x903850;           // bytes: 0 the object-ahead flag, 1..5 the cell kinds, 8..10 the classes
constexpr std::uint32_t kAimX = 0x903858;            // long: where a member's step would end (Member_StepAhead)
constexpr std::uint32_t kAimZ = 0x90385C;            // long: the same, z (Scratch_Swap's address)
constexpr std::uint32_t kDirUnits = 0x6696DC;        // two longs a direction, a step's unit
constexpr std::uint32_t kCellOffsets = 0x66971C;     // two signed bytes a direction, the cell ahead
constexpr std::uint32_t kDirSteps = 0x6697B0;        // Field_DirectionSteps, read past its 8 rows unchecked
constexpr std::uint32_t kMoveSpeeds = 0x6697F0;      // Field_MoveSpeeds, read past its 6 entries unchecked
constexpr std::uint32_t kObject905DA0 = 0x905DA0;    // the object Field_CellKind singles out
constexpr std::uint32_t kActorRecords = 0x903A70;    // 0xA4 bytes each; Field_ActorStates is +0x10 of the first
}  // namespace at

struct Callees {
    // Field_MemberFrame
    void (__cdecl* restore_clut)();
    const std::uint32_t* member_states;
    // Member_Start
    void (__cdecl* clear_steps)();
    long (__cdecl* ground_at)(long, long);
    void (__cdecl* set_animation)(unsigned char);
    // Member_Control
    const std::uint32_t* control_states;
    unsigned char (__cdecl* script_tick)();
    // Member_Follow
    unsigned char (__cdecl* test_535120)(unsigned);   // V2
    unsigned char (__cdecl* test_535240)(unsigned);   // V2
    unsigned char (__cdecl* ensure_animation)(unsigned char);
    unsigned char (__cdecl* idle)();                  // Member_Idle
    unsigned char (__cdecl* follow_step)();           // Member_FollowStep
    unsigned char (__cdecl* test_531df0)();           // V1
    void (__cdecl* call_534610)();                    // V2
    void (__cdecl* call_535f50)();                    // V2
    void (__cdecl* call_52e140)();                    // V1
    // Member_Walk
    void (__cdecl* walk_end)();                       // Member_WalkEnd
    // Member_WalkEnd
    void (__cdecl* call_535270)();                    // V2
    void (__cdecl* call_5350c0)();                    // V2
    void (__cdecl* call_534f10)();                    // V2
    void (__cdecl* call_534a00)();                    // V2
    unsigned char (__cdecl* test_534920)();           // V2
    void (__cdecl* follow)();                         // Member_Follow
    // Member_FollowStep
    void (__cdecl* catch_up)(long, long);             // Member_CatchUp
    void (__cdecl* direction_to)(long, long, unsigned char*);   // Field_DirectionTo
    unsigned char (__cdecl* step_ahead)();            // Member_StepAhead
    unsigned char (__cdecl* blocked_at)(long, long, unsigned, long);   // 0x535610, nobody's
    // Member_StepAhead
    unsigned char (__cdecl* cell_ahead)();            // Field_CellAhead
    long (__cdecl* object_at)(long, long, unsigned);  // 0x5725C0, M
    // Member_CatchUp
    unsigned char (__cdecl* map_cell)(unsigned, unsigned);   // 0x592890, nobody's
    // Member_Idle
    int (__cdecl* rand)();
    // Field_CellAhead
    unsigned char (__cdecl* cell_ahead_raised)();     // 0x527640, nobody's
    unsigned char (__cdecl* cell_ahead_flat)();       // Field_CellAheadFlat
    // Field_CellAheadFlat
    void (__cdecl* read_cells)(unsigned, unsigned);   // Field_ReadCells
    unsigned char (__cdecl* cell_class)(unsigned, unsigned, unsigned);   // Field_CellClass
    void (__cdecl* turn_unless)(unsigned, unsigned, unsigned);           // Field_TurnUnless
    unsigned char (__cdecl* cell_slope)(unsigned);    // Field_CellSlope
    unsigned char (__cdecl* cell_pair_turn)(unsigned, unsigned, unsigned, unsigned);   // Field_CellPairTurn
    // Field_ReadCells
    unsigned char (__cdecl* cell_kind)(unsigned, unsigned, unsigned, unsigned);   // Field_CellKind
    // Field_CellKind
    unsigned char (__cdecl* byte_at)(short, short);   // AreaMap_ByteAt
    unsigned char (__cdecl* cell_facing)(unsigned);   // Field_CellFacing
    unsigned char (__cdecl* object_ahead)(long, long);   // Field_ObjectAhead
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (member_sprites_fuzz.cpp): clones every function with each
// call re-aimed at a recording stand-in, runs ours against the clones, and
// ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace member_sprites
