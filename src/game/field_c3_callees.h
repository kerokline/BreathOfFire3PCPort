// The raw addresses field_c3.cpp calls or reads that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/field_c3.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"   // the constants below name their functions since 2026-10-01 (round twelve's debt 2): the same values, so the fuzz keys stand

namespace field_c3::at {

// Callees another group of round twelve's wave two owns (the cut table
// analysis/round12_cut.tsv), called through the harness by address (SH_AT)
// until they merge; the coordinator rebinds them after (docs/field_c3.md
// section 8). All FE2's.
constexpr ::bof3::image::CodeVa kEventObjectFrame{bof3::addr::Mode11_ObjectDraw};   // FE2: void(void); Sprite_Current = 0x905DA0, the event object's frame
constexpr ::bof3::image::CodeVa kUpFace{bof3::addr::Leader_Pose3C};             // FE2: void(void); animation 0x3C facing 7, else 0x3D
constexpr ::bof3::image::CodeVa kDownJumpSetUp{bof3::addr::Leader_HopStart};      // FE2: void(void); pace +0x70 + 2, Field_JumpSetUp, the frames doubled
constexpr ::bof3::image::CodeVa kDownWait1{bof3::addr::Leader_HopFlight};          // FE2: unsigned char(void); al 1 when +9 has run out
constexpr ::bof3::image::CodeVa kDownWait2{bof3::addr::Leader_HopPose};          // FE2: unsigned char(void)
constexpr ::bof3::image::CodeVa kDownWait3{bof3::addr::Leader_Pose3E};          // FE2: unsigned char(void)
constexpr ::bof3::image::CodeVa kDownWait4{bof3::addr::Leader_StepUp};          // FE2: unsigned char(void)
constexpr ::bof3::image::CodeVa kUpLanded{bof3::addr::Leader_Pose34};           // FE2: void(void); a CLUT word 0, animation 0x37 facing 7, else 0x34
constexpr ::bof3::image::CodeVa kUpWait5{bof3::addr::Leader_StepDown};            // FE2: unsigned char(void)
constexpr ::bof3::image::CodeVa kUpWait6{bof3::addr::Leader_HopStartAfterTick};            // FE2: unsigned char(void)
constexpr ::bof3::image::CodeVa kUpWait7{bof3::addr::Leader_HopFall};            // FE2: unsigned char(void); al 1 once the object is back in state 1
constexpr ::bof3::image::CodeVa kRecoilFace{bof3::addr::Field_FloorHurt};         // FE2: void(unsigned char), reads the byte (and eax, 0xFF)

// Callees nobody owns, in the harness's field-standard set by address.
constexpr ::bof3::image::CodeVa kPartyScreens{bof3::addr::Mode11_ListedSpriteScreens};   // R2A: void(void), mode 11's frame's sixth call
constexpr ::bof3::image::CodeVa kMenuDispatchA{bof3::addr::BattleExtra_Dispatch};      // void(void): jmp through 0x64ADAC by the menu byte 0x929F00
constexpr ::bof3::image::CodeVa kMenuDispatchB{bof3::addr::Shisu_ModeDispatch};      // void(void): jmp through 0x663DD0 by the menu byte 0x929F00

// Data.
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember (unsigned char *): its cell, a region of the fuzz's
// kExtraPositions: now Sprite_ObjectsExtra[0].x (symbols.toml). Sprite_ObjectsExtra +0x34: the attached object's x, z, y, +0x64.. (stride 0xA4)
// kLeaderX: now ObjTrio[0].x (symbols.toml). ObjTrio +0x34: the leader's x (16.16)
// kLeaderZ: now ObjTrio[0].z (symbols.toml). ObjTrio +0x38
// kLeaderPace: now ObjTrio[0].step_frames (symbols.toml). ObjTrio +9: the leader's frames left in a step
// kLeaderStepX: now ObjTrio[0].step_x (symbols.toml). ObjTrio +0xC: the leader's step a frame, x
// kLeaderStepZ: now ObjTrio[0].step_z (symbols.toml). ObjTrio +0x10
// kJumpSteps: now MoveCmd_F9Steps (symbols.toml). per direction (8 bytes): x, z steps, the jumps' table (Field_JumpSetUp's)
constexpr std::uint32_t kTargetOffsets = 0x66978C;      // s16 by Field_State +0x89: taken off the ground at (kTargetX, kTargetZ)
constexpr std::uint32_t kTargetX = 0x904EF4;            // s32 (16.16): the x the vertical moves test the ground at
constexpr std::uint32_t kTargetZ = 0x904EF8;            // s32: its z
inline auto& kSloped = SharedScratch.b_00;  // u8: AreaMap_Slope's "sloped" answer (SharedScratch's first byte)
constexpr std::uint32_t kSloped_addr = SharedScratch_addr; // kSloped's address, for the raw accessors that take one
constexpr std::uint32_t kScriptFlags2High = 0x905BA5;   // Field_ScriptFlags2's high byte: bit 1 set by the fall's area change
// kPalettes: now Gfx_ClutStripSource[0xf00] (symbols.toml). 0x40 bytes a CLUT slot: Sprite_LoadPalette's source by Sprite_Current +5

}  // namespace field_c3::at
