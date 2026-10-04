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
constexpr std::uint32_t kEventObjectFrame = bof3::addr::Mode11_ObjectDraw;   // FE2: void(void); Sprite_Current = 0x905DA0, the event object's frame
constexpr std::uint32_t kUpFace = bof3::addr::Leader_Pose3C;             // FE2: void(void); animation 0x3C facing 7, else 0x3D
constexpr std::uint32_t kDownJumpSetUp = bof3::addr::Leader_HopStart;      // FE2: void(void); pace +0x70 + 2, Field_JumpSetUp, the frames doubled
constexpr std::uint32_t kDownWait1 = bof3::addr::Leader_HopFlight;          // FE2: unsigned char(void); al 1 when +9 has run out
constexpr std::uint32_t kDownWait2 = bof3::addr::Leader_HopPose;          // FE2: unsigned char(void)
constexpr std::uint32_t kDownWait3 = bof3::addr::Leader_Pose3E;          // FE2: unsigned char(void)
constexpr std::uint32_t kDownWait4 = bof3::addr::Leader_StepUp;          // FE2: unsigned char(void)
constexpr std::uint32_t kUpLanded = bof3::addr::Leader_Pose34;           // FE2: void(void); a CLUT word 0, animation 0x37 facing 7, else 0x34
constexpr std::uint32_t kUpWait5 = bof3::addr::Leader_StepDown;            // FE2: unsigned char(void)
constexpr std::uint32_t kUpWait6 = bof3::addr::Leader_HopStartAfterTick;            // FE2: unsigned char(void)
constexpr std::uint32_t kUpWait7 = bof3::addr::Leader_HopFall;            // FE2: unsigned char(void); al 1 once the object is back in state 1
constexpr std::uint32_t kRecoilFace = bof3::addr::Field_FloorHurt;         // FE2: void(unsigned char), reads the byte (and eax, 0xFF)

// Callees nobody owns, in the harness's field-standard set by address.
constexpr std::uint32_t kPartyScreens = bof3::addr::Mode11_ListedSpriteScreens;   // R2A: void(void), mode 11's frame's sixth call
constexpr std::uint32_t kMenuDispatchA = 0x42D710;      // void(void): jmp through 0x64ADAC by the menu byte 0x929F00
constexpr std::uint32_t kMenuDispatchB = bof3::addr::Shisu_ModeDispatch;      // void(void): jmp through 0x663DD0 by the menu byte 0x929F00

// Data.
constexpr std::uint32_t kActiveMember = 0x9035A4;       // Field_ActiveMember (unsigned char *): its cell, a region of the fuzz's
constexpr std::uint32_t kExtraPositions = 0x802034;     // Sprite_ObjectsExtra +0x34: the attached object's x, z, y, +0x64.. (stride 0xA4)
constexpr std::uint32_t kLeaderX = 0x802D74;            // ObjTrio +0x34: the leader's x (16.16)
constexpr std::uint32_t kLeaderZ = 0x802D78;            // ObjTrio +0x38
constexpr std::uint32_t kLeaderPace = 0x802D49;         // ObjTrio +9: the leader's frames left in a step
constexpr std::uint32_t kLeaderStepX = 0x802D4C;        // ObjTrio +0xC: the leader's step a frame, x
constexpr std::uint32_t kLeaderStepZ = 0x802D50;        // ObjTrio +0x10
constexpr std::uint32_t kJumpSteps = 0x6696DC;          // per direction (8 bytes): x, z steps, the jumps' table (Field_JumpSetUp's)
constexpr std::uint32_t kTargetOffsets = 0x66978C;      // s16 by Field_State +0x89: taken off the ground at (kTargetX, kTargetZ)
constexpr std::uint32_t kTargetX = 0x904EF4;            // s32 (16.16): the x the vertical moves test the ground at
constexpr std::uint32_t kTargetZ = 0x904EF8;            // s32: its z
constexpr std::uint32_t kSloped = 0x903850;             // u8: AreaMap_Slope's "sloped" answer (DamageScratch's first byte)
constexpr std::uint32_t kScriptFlags2High = 0x905BA5;   // Field_ScriptFlags2's high byte: bit 1 set by the fall's area change
constexpr std::uint32_t kPalettes = 0x80D380;           // 0x40 bytes a CLUT slot: Sprite_LoadPalette's source by Sprite_Current +5

}  // namespace field_c3::at
