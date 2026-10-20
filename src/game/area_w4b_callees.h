// Internal to area_w4b.cpp and area_w4b_fuzz.cpp: the cells world 4's areas
// 168..172 touch that symbols.toml has no name for, the areas' own .data
// tables, and the one callee nobody owns (by its raw address, as round ten's
// rule for a function no group has taken). Every other call is to a named
// function through the area harness (AH_CALL). docs/area_w4b.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w4b {
namespace at {

// --- the field frame's cells (docs/area_harness.md section 4) ---

// The message box's answer to a choice (the cursor's row) and the message
// word a choice handler leaves (0xFFFF: no new message).
// kChoiceAnswer: now AreaW0c_ChoiceAnswer (symbols.toml).
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub-kind, and a word timer.
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailState: now AreaW0c_TailState (symbols.toml).
// kTailSub: now AreaW0c_TailSub (symbols.toml).
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's counters 0 and 3 (MoveScript_CounterOps, ops
// A0..AF), and the scratch cell after them (area 170's tail keeps an effect
// slot and the input word there, area 171's step hook a count).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter3 = 0x90384B;
constexpr std::uint32_t kScratch = SharedScratch_addr;
// The story flags (Flags_Set / Flags_Test's bank), and a second bank area
// 172's choice 0 tests (Cond_Flags row 14: 0x903F90 + 8 * 14).
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml).
// kCondRow14: now Cond_Flags[0x70] (symbols.toml).
// The focus object: the object whose talk ran last (area_w1a_callees.h's
// kFocusObject). Area 168's choice 1 writes its dwords +0x18 and +0x1C.
constexpr std::uint32_t kFocusObject = 0x903804;
// The zone the field was entered from (area_w1a_callees.h's kEntryZone):
// area 170's init tests it against 2 and 3.
constexpr std::uint32_t kEntryZone = 0x905E68;
// A byte area 171's handler 0 compares with 0x4B (a cell of the 0x903Dxx
// block no other group has read; what it holds is not read here).
// kGateKey: now AreaW4b_GateKey (symbols.toml).
// The leader's record (ObjTrio record 0): x +0x34, z +0x38, the pose byte +8
// the hooks compare, +0x89 (Character_IndexById's index, area_w1c), +0x137
// (area 171's tail waits for it to be 0; field_hidden_callees.h's
// kLeader137). Party records 1 and 2's words +0x12E (area 170's state 60
// counts both up).
// kLeader: now ObjTrio (symbols.toml).
// kLeaderPose: now ObjTrio[0].direction (symbols.toml).
// kLeaderX: now ObjTrio[0].x (symbols.toml).
// kLeaderZ: now ObjTrio[0].z (symbols.toml).
// kLeader89: now ObjTrio[0].char_id (symbols.toml).
constexpr std::uint32_t kLeader137 = 0x802E77;
// kParty1Word12E: now ObjTrio (symbols.toml). record 1 +0x12E (Field_Members[0x132] until stage C4).
// kParty2Word12E: now ObjTrio_addr + 0x3c6 (symbols.toml).
constexpr std::uint32_t kPartyStride = 0x14C;
// The field record's byte +0x128 the member frames set to 2, and the byte
// +0x149 area 172's fall indexes MoveScript_TintRecords by.
constexpr std::uint32_t kFieldStateMember = 0x128;
constexpr std::uint32_t kFieldStateTint = 0x149;
// MoveScript_TintRecords: records of 12 bytes by a byte index (area 172's
// fall darkens bytes +2..+4 of one).
constexpr std::uint32_t kTintStride = 12;
// Sprite_Kind2 + 0x3C: the camera object's height dword (scena_sc0.cpp
// writes it beside MapView_SetElevation the same way).
// kKind2Height: now Sprite_Kind2.height (symbols.toml).
// Camera_Angles + 2: the yaw word the effects of kind 0x13 take.
// kCameraYaw: now Camera_Angles[1] (symbols.toml).
// Field_MoveSpeeds + 2 and + 3 (bytes): the glide divisor is one << 3.
// kMoveSpeed2: now Field_MoveSpeeds[2] (symbols.toml).
// kMoveSpeed3: now Field_MoveSpeeds[3] (symbols.toml).
// MoveScript_Var7 and the byte after it (area 172's choice 0 sets both; its
// effect kind 0xA5 counts the second up).
// kVar7: now MoveScript_Var7 (symbols.toml).
// kVar7b: now MoveScript_PartyRecords[0x65] (symbols.toml).
// Effect_Objects' stride and count (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// A field object's size: the quotient areas 171 and 172 store in an effect's
// +0xB is the active member's distance from Sprite_Objects over it.
constexpr std::uint32_t kObjectStride = 0xA4;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
// kEffectObjects: now Effect_Objects (symbols.toml). Effect_Objects
// kTintRecords: now MoveScript_TintRecords (symbols.toml). MoveScript_TintRecords
// kSpriteObjects: now Sprite_Objects (symbols.toml). Sprite_Objects
// kSpriteObjectsExtra: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object
// kPacketNext: now Gfx_PacketNext (symbols.toml). Gfx_PacketNext
// kInputHeld: now Input_Held (symbols.toml). Input_Held
// kWaitWord: now Transition_Busy (symbols.toml). Transition_Busy
// kKind2Hold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
// kCameraAngles: now Camera_Angles (symbols.toml). Camera_Angles
// kMoveSpeeds: now Field_MoveSpeeds (symbols.toml). Field_MoveSpeeds
// kKind2Z: now Field_Kind2Z (symbols.toml). Field_Kind2Z
// kKind2X: now Field_Kind2X (symbols.toml). Field_Kind2X
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). MoveScript_F3Divisor
// kCondByteFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 168: byte pairs its choice 1 reads by the signed answer (x 2), into
// the focus object's +0x18 and +0x1C.
// kArea168FocusPairs: now Area168_FocusPairs (symbols.toml).
// Areas 169 and 171: two rectangles of five bytes each (x0, z0, x1, z1, the
// facing) the member frames search; the search's loop runs from the third
// byte of the first to the table's end.
struct Rects {
    const char* who;
    std::uint32_t rects;   // the first rectangle
    unsigned count;
    std::uint32_t fn_rect; // the copy's own rectangle search (called by address)
};
constexpr unsigned kRectStride = 5;
constexpr Rects kRects169 = {"Area169", 0x63CA3C, 2, bof3::addr::Area169_MemberRect};
constexpr Rects kRects171 = {"Area171", 0x63D84C, 2, bof3::addr::Area171_MemberRect};
// Area 170: the four input directions its tail's state 53 swaps, by the held
// word's top nibble (16 bytes).
// kArea170InputSwap: now Area170_InputSwap (symbols.toml).
// Area 172: two two-state tables laid back to back (the fall, then the
// slide), the sixteen signed drift steps, and effect kind 0xA5's three states.
// kArea172FallStates: now Area172_FallStates (symbols.toml). Area172_FallStates, 2 (+ the slide's 2 after)
// kArea172SlideStates: now Area172_SlideStates (symbols.toml). Area172_SlideStates, 2
constexpr unsigned kArea172FallReach = 4;                 // entries 0..3 are code (the slide's two run on)
constexpr unsigned kArea172SlideCount = 2;
// kArea172DriftSteps: now Area172_DriftSteps (symbols.toml). Area172_DriftSteps, 16 bytes
// kArea172EffectStates: now Area172_EffectStates (symbols.toml). Area172_EffectStates, 3
constexpr unsigned kArea172EffectCount = 3;

// --- the callee nobody owns (raw address) ---

// 0x486D60: void (void), engine code (called only by area 170's init): story
// flag 0x7E, then map bytes (AreaMap_SetByte) and map items
// (MapView_ItemAt, Prim_SetTexture) over tables of the 0x63CAxx block. Not
// read further here. Ours since round thirteen: EffectKind7D_SetMap (E3D,
// docs/effect_3d.md), which kind 0x7D's state 2 calls too.
constexpr std::uint32_t kArea170MapSetUp = bof3::addr::EffectKind7D_SetMap;

}  // namespace at
}  // namespace area_w4b
