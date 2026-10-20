// Chapter 0's calls to functions nobody owns yet, by address through the
// scenario harness (scenario_harness.h), and the cells and tables it reads.
// docs/scena_sc0.md section 6. Round nine's rebinding pass turns the raw
// addresses into names once their owners take them.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
#include "game/scenario_harness.h"

namespace scena_sc0 {

// --- the chapter's tables (.data, read in place) ---------------------------
// kStates: now Scena00_States (symbols.toml). Scena00_States: 3 handlers by the s8 0x8034E2
// kRuns: now Scena00_Runs (symbols.toml). Scena00_Runs: 12 handlers by the s8 MoveScript_Var7
// kObjectHooks: now Scena00_ObjectHooks (symbols.toml). Scena00_ObjectHooks: 1 handler by the object's +0x86
constexpr std::uint32_t kShake = 0x660D20;        // Scena00_Shake: 16 s8 offsets by a 4-bit index
// kStartSteps: now Scena00_StartSteps (symbols.toml). run 6's first step by Cond_ByteFD
constexpr std::uint32_t kInputAnims = 0x660D38;   // Scena00_InputAnims: 16 pairs (animate?, animation) by input >> 12

// --- the cells ----------------------------------------------------------------
// kState: now MoveScript_PartyRecords[0x62] (symbols.toml).
// kRun: now MoveScript_Var7 (symbols.toml). MoveScript_Var7
// kStep: now MoveScript_PartyRecords[0x65] (symbols.toml).
constexpr std::uint32_t kTimer = 0x8034E6;        // u16
// kByteFD: now Cond_ByteFD (symbols.toml). Cond_ByteFD
// kFlagRow: now Chapter_FlagRow (symbols.toml).
constexpr std::uint32_t kCounter = 0x903848;      // dword; its low byte is waited on
constexpr std::uint32_t kCount2 = 0x903849;
constexpr std::uint32_t kByte4A = 0x90384A;
constexpr std::uint32_t kByte4B = 0x90384B;
constexpr std::uint32_t kSlot = 0x903850;         // the effect slot just taken
// kWait: now Transition_Busy (symbols.toml). Transition_Busy
// kRequest: now Field_Request (symbols.toml). Field_Request
// kArea: now Game_AreaNumber (symbols.toml). Game_AreaNumber
// kPassFlags: now Draw_PassFlags (symbols.toml). Draw_PassFlags
constexpr std::uint32_t kScriptFlags = 0x9039A2;  // Field_ScriptFlags (u16)
constexpr std::uint32_t kPending = 0x903A04;      // the pending area change: u16 area, +3 flags, +4 x, +8 z
// kAngles: now Camera_Angles (symbols.toml). Camera_Angles: three s16
// kDistance: now Camera_Distance (symbols.toml). Camera_Distance
// kEffects: now Effect_Objects (symbols.toml). Effect_Objects, 0x80 each
// kObjTrio: now ObjTrio (symbols.toml).
// kFocusZ: now MapView_FocusZ (symbols.toml). MapView_FocusZ
// kKind2Z: now Field_Kind2Z (symbols.toml). Field_Kind2Z
// kKind2X: now Field_Kind2X (symbols.toml). Field_Kind2X
constexpr std::uint32_t kInput = 0x7E1BEC;        // Input_Pressed
constexpr std::uint32_t kDescriptors = 0x667590;  // Area_Descriptors
constexpr std::uint32_t kCorner = 0x92A0C0;       // MapView_CornerPtr

// --- callees nobody owns, by address -----------------------------------------
// SE's (round ten): an event battle's set-up by index.
constexpr std::uint32_t kEventBattle = bof3::addr::Field_StartEventBattle;
// Nobody's: x, z and an index - an event battle's party placement (0x903780 / 84).
constexpr std::uint32_t kPlaceParty = bof3::addr::Party_PlaceForBattle;
// Nobody's: the camera turned toward an s16 angle at an s8 speed; al 1 while turning.
constexpr std::uint32_t kTurnCamera = bof3::addr::Camera_EaseAngleFB;
// Nobody's: the view shift after a focus test (PSX 0x80155154).
constexpr std::uint32_t kViewShift = bof3::addr::MapView_FillCells;
// Nobody's: Field_StatusBits |= 0x80.
constexpr std::uint32_t kStatus80 = bof3::addr::Field_SetStatus80;

inline void EventBattle(unsigned n) { SH_AT(void (__cdecl*)(unsigned), kEventBattle)(n); }
inline void PlaceParty(std::uint32_t x, std::uint32_t z, unsigned n) {
    SH_AT(void (__cdecl*)(std::uint32_t, std::uint32_t, unsigned), kPlaceParty)(x, z, n);
}
inline unsigned char TurnCamera(unsigned angle, unsigned speed) {
    return SH_AT(unsigned char (__cdecl*)(unsigned, unsigned), kTurnCamera)(angle, speed);
}
inline void ViewShift() { SH_AT(void (__cdecl*)(), kViewShift)(); }
inline void Status80() { SH_AT(void (__cdecl*)(), kStatus80)(); }

}  // namespace scena_sc0
