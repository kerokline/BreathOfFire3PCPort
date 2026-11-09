// The event script's shop and scene ops (group V1 of the sixth round) -
// docs/event-ops.md for each function, its PSX twin, the fuzz and what the
// shop route reaches.
//
//   - EventOp_5x / 9x / Dx, the three placement ops of the event script that
//     round five left Capcom's (docs/event-script.md section 1).
//   - The party leader's walk: Field_LeaderWalk 0x52DB90 is sub-state 1 of
//     Field_LeaderControl (the table 0x660954, docs/field-event.md), with the
//     step's target and cell, the pace, the encounter test, the six button
//     tests Field_LeaderStand and Field_LeaderWalk make first, and what they
//     call: the object and cell the leader faces, the passage list, the talk.
//   - The chapter's and the area's step hooks the step's target consults.
//
// Terms. "SC" is Sprite_Current and "FS" Field_State, read afresh at every
// use as the original reads [0x937F88] / [0x905D98] (a callee may move
// either). The leader is ObjTrio's member 0, which Field_State points at.
// Every call goes through event_ops::g (event_ops_callees.h), so that the
// start-up fuzz can stand recorders in for them.
#include "game/event_ops.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_ops_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace event_ops {

using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const std::uint32_t kStepHandlers[kStepCases] = {
    bof3::addr::Area36_StepHook, bof3::addr::Area42_StepHook, bof3::addr::Area46_StepHook, bof3::addr::Area49_StepHook, bof3::addr::Area59_StepHook, bof3::addr::Area75_StepHook, bof3::addr::Area76_StepDisarmTail5, bof3::addr::Area97_StepHook, bof3::addr::Area100_StepHook, bof3::addr::Area105_StepHook,
    bof3::addr::Area106_StepHook, bof3::addr::Area112_StepHook, bof3::addr::Area116_StepHook, bof3::addr::Area135_StepHook, bof3::addr::Area140_StepHook, bof3::addr::Area143_StepHook, bof3::addr::Area145_StepHook, bof3::addr::Area146_StepHook, bof3::addr::Area150_StepHook, bof3::addr::Area170_StepHook,
    bof3::addr::Area171_StepHook, bof3::addr::Area172_StepHook, bof3::addr::Scenario_NoHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook,
    bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area191_StepHook, bof3::addr::Area192_StepHook, bof3::addr::Area193_StepHook, bof3::addr::Area197_StepHook,
};
const std::uint32_t kArriveHandlers[kArriveCases] = {
    bof3::addr::Area40_ArriveHook, bof3::addr::Area48_ArriveHook, bof3::addr::Area111_ArriveHook, bof3::addr::Area148_ArriveHook, bof3::addr::Area167_ArriveHook, bof3::addr::Area169_ArriveHook, bof3::addr::Area171_ArriveHook, bof3::addr::Area173_ArriveHook,
};

const Callees kOriginals = {
    Field_LeaderTalkTest, Field_LeaderSwapTest, Field_LeaderMenuTest, Field_LeaderCheckTest,
    Field_LeaderRequest4Test, Field_LeaderRequest9Test,
    Field_LeaderDirection, Field_LeaderStepTarget, Field_LeaderPushObjects, Field_LeaderPushCount,
    Field_LeaderSetPace, Field_EncounterDue, Field_LeaderStepTick, Field_LeaderEffectTest, Field_LeaderStepCell,
    Party_CanSwap, Field_EffectAhead, Area_PassageAhead, Field_FacingObject, Field_ObjectInPath, Field_CellAround,
    Field_CellAroundNear, Field_LeaderTalkTo, Scenario_StepHook, Scenario_ArriveHook, Area_StepHook,
    Area_ArriveHook, Area_ReturnGate,
    reinterpret_cast<const ::bof3::Slot32<const ChapterHooks>*>(static_cast<std::uintptr_t>(::bof3::Img(Scenario_Hooks_addr))),
    Field_LeaderAnimation, Sprite_ClearSteps, Sprite_EnsureAnimation, AreaMap_ByteAt, Field_EncounterThresholdRoll,
    Party_Count, Area_ZoneAt, Rand, Sound_PlayEffect, Area_LinkAt, MapView_GroundAt, Sprite_ScriptTick,
    Sprite_ObjectAt, Sprite_PointInReach, Field_ChangeArea, Flags_Test, Flags_Clear, EventObj_Reset,
    Sprite_SetAnimationBank, AreaMap_Elevation, EventObj_SetFlags, Sprite_SetAnimation,
    Fn<void (__cdecl*)()>(::bof3::Code(fn::kFaceObject.v)),
    Fn<void (__cdecl*)()>(::bof3::Code(fn::kEncounterArea.v)),
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(fn::kExitGateway.v)),
    Fn<void (__cdecl*)()>(::bof3::Code(fn::kExitFromCell.v)),
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(fn::kCellAroundLarge.v)),
    Fn<int (__cdecl*)(unsigned, unsigned)>(::bof3::Code(fn::kCellHook.v)),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(fn::kSetCell.v)),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(fn::kPartyVisible.v)),
    Fn<unsigned char (__cdecl*)(long, long, unsigned, unsigned, unsigned)>(::bof3::Code(fn::kMemberFits.v)),
    Fn<void (__cdecl*)()>(::bof3::Code(fn::kLeaderMove.v)),
    Fn<void (__cdecl*)()>(::bof3::Code(fn::kLeaderFollow.v)),
    Fn<void (__cdecl*)()>(::bof3::Code(fn::kLeaderGround.v)),
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(fn::kStepCode.v)),
    Fn<long (__cdecl*)(long, long, std::uint32_t)>(::bof3::Code(fn::kSlopeAt.v)),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(fn::kTestFB.v)),
    {Fn<Hook>(::bof3::CodePlace(kStepHandlers[0])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[1])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[2])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[3])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[4])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[5])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[6])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[7])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[8])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[9])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[10])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[11])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[12])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[13])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[14])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[15])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[16])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[17])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[18])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[19])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[20])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[21])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[22])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[23])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[24])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[25])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[26])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[27])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[28])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[29])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[30])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[31])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[32])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[33])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[34])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[35])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[36])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[37]))},
    {Fn<Hook>(::bof3::CodePlace(kArriveHandlers[0])), Fn<Hook>(::bof3::CodePlace(kArriveHandlers[1])), Fn<Hook>(::bof3::CodePlace(kArriveHandlers[2])),
     Fn<Hook>(::bof3::CodePlace(kArriveHandlers[3])), Fn<Hook>(::bof3::CodePlace(kArriveHandlers[4])), Fn<Hook>(::bof3::CodePlace(kArriveHandlers[5])),
     Fn<Hook>(::bof3::CodePlace(kArriveHandlers[6])), Fn<Hook>(::bof3::CodePlace(kArriveHandlers[7]))},
};
Callees g = kOriginals;

}  // namespace event_ops

using namespace event_ops;

namespace {


unsigned char* Object(int n) { return rv::Bytes(&Sprite_Objects[n]); }
unsigned char* Extra(int n) { return rv::Bytes(&Sprite_ObjectsExtra[n]); }

// The record views (game/record_views.h): rv::Current<T>() reads
// Sprite_Current afresh at each use, as the original does.
// rv::As<records::SpriteObject>(p) views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record: Object(n) is a Sprite_Objects record.


// The whole part of a 16.16 position, as the original pushes it: the high
// half of the dword, which the callee reads as a short.
short Whole(std::uint32_t v) { return static_cast<short>(v >> 16); }

}  // namespace

// --- the event script's placements ----------------------------------------------

// --- the leader's walk ---------------------------------------------------------

// --- the tests Field_LeaderStand and Field_LeaderWalk make ---------------------

// --- the chapter's and the area's hooks ------------------------------------------

void EventOps_Inject() {
}
