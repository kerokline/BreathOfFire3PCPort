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
constexpr std::uint32_t kChoiceAnswer = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// The field-frame mode bytes: the tail kind Field_ModeTailRun runs by
// (Field_ModeTailKinds 0x662CE8), its state, a sub-kind, and a word timer.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailState = 0x9039F4;
constexpr std::uint32_t kTailSub = 0x9039F5;
constexpr std::uint32_t kTailTimer = 0x9039F6;
// The movement script's counters 0 and 3 (MoveScript_CounterOps, ops
// A0..AF), and the scratch cell after them (area 170's tail keeps an effect
// slot and the input word there, area 171's step hook a count).
constexpr std::uint32_t kCounter0 = 0x903848;
constexpr std::uint32_t kCounter3 = 0x90384B;
constexpr std::uint32_t kScratch = 0x903850;
// The story flags (Flags_Set / Flags_Test's bank), and a second bank area
// 172's choice 0 tests (Cond_Flags row 14: 0x903F90 + 8 * 14).
constexpr std::uint32_t kStoryFlags = 0x904030;
constexpr std::uint32_t kCondRow14 = 0x904000;
// The focus object: the object whose talk ran last (area_w1a_callees.h's
// kFocusObject). Area 168's choice 1 writes its dwords +0x18 and +0x1C.
constexpr std::uint32_t kFocusObject = 0x903804;
// The zone the field was entered from (area_w1a_callees.h's kEntryZone):
// area 170's init tests it against 2 and 3.
constexpr std::uint32_t kEntryZone = 0x905E68;
// A byte area 171's handler 0 compares with 0x4B (a cell of the 0x903Dxx
// block no other group has read; what it holds is not read here).
constexpr std::uint32_t kGateKey = 0x903DB6;
// The leader's record (ObjTrio record 0): x +0x34, z +0x38, the pose byte +8
// the hooks compare, +0x89 (MoveScript_EffectState's index, area_w1c), +0x137
// (area 171's tail waits for it to be 0; field_hidden_callees.h's
// kLeader137). Party records 1 and 2's words +0x12E (area 170's state 60
// counts both up).
constexpr std::uint32_t kLeader = 0x802D40;
constexpr std::uint32_t kLeaderPose = 0x802D48;
constexpr std::uint32_t kLeaderX = 0x802D74;
constexpr std::uint32_t kLeaderZ = 0x802D78;
constexpr std::uint32_t kLeader89 = 0x802DC9;
constexpr std::uint32_t kLeader137 = 0x802E77;
constexpr std::uint32_t kParty1Word12E = 0x802FBA;
constexpr std::uint32_t kParty2Word12E = 0x803106;
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
constexpr std::uint32_t kKind2Height = 0x7E097C;
// Camera_Angles + 2: the yaw word the effects of kind 0x13 take.
constexpr std::uint32_t kCameraYaw = 0x929ECA;
// Field_MoveSpeeds + 2 and + 3 (bytes): the glide divisor is one << 3.
constexpr std::uint32_t kMoveSpeed2 = 0x6697F2;
constexpr std::uint32_t kMoveSpeed3 = 0x6697F3;
// MoveScript_Var7 and the byte after it (area 172's choice 0 sets both; its
// effect kind 0xA5 counts the second up).
constexpr std::uint32_t kVar7 = 0x8034E4;
constexpr std::uint32_t kVar7b = 0x8034E5;
// Effect_Objects' stride and count (records of 0x80 bytes, 20 of them).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// A field object's size: the quotient areas 171 and 172 store in an effect's
// +0xB is the active member's distance from Sprite_Objects over it.
constexpr std::uint32_t kObjectStride = 0xA4;
// Named cells, by address for the fuzz's regions (symbols.gen.h spells each
// name as a macro of its value):
constexpr std::uint32_t kEffectObjects = 0x7E11E0;        // Effect_Objects
constexpr std::uint32_t kTintRecords = 0x7E0700;          // MoveScript_TintRecords
constexpr std::uint32_t kSpriteObjects = 0x7DEE80;        // Sprite_Objects
constexpr std::uint32_t kSpriteObjectsExtra = 0x802000;   // Sprite_ObjectsExtra
constexpr std::uint32_t kActiveMember = 0x9035A4;         // Field_ActiveMember
constexpr std::uint32_t kScriptObject = 0x929E80;         // MoveScript_Object
constexpr std::uint32_t kPacketNext = 0x7E0670;           // Gfx_PacketNext
constexpr std::uint32_t kInputHeld = 0x7E1BE8;            // Input_Held
constexpr std::uint32_t kWaitWord = 0x66C810;             // MoveScript_WaitWordDA
constexpr std::uint32_t kKind2Hold = 0x929F12;            // Field_Kind2Hold
constexpr std::uint32_t kCameraAngles = 0x929EC8;         // Camera_Angles
constexpr std::uint32_t kMoveSpeeds = 0x6697F0;           // Field_MoveSpeeds
constexpr std::uint32_t kKind2Z = 0x905E60;               // Field_Kind2Z
constexpr std::uint32_t kKind2X = 0x905E64;               // Field_Kind2X
constexpr std::uint32_t kF3Divisor = 0x937F8C;            // MoveScript_F3Divisor
constexpr std::uint32_t kCondByteFE = 0x905E20;           // Cond_ByteFE

// --- the areas' tables (symbols.toml [[data]]) ---

// Area 168: byte pairs its choice 1 reads by the signed answer (x 2), into
// the focus object's +0x18 and +0x1C.
constexpr std::uint32_t kArea168FocusPairs = 0x63C944;
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
constexpr std::uint32_t kArea170InputSwap = 0x63D63C;
// Area 172: two two-state tables laid back to back (the fall, then the
// slide), the sixteen signed drift steps, and effect kind 0xA5's three states.
constexpr std::uint32_t kArea172FallStates = 0x63EF14;    // Area172_FallStates, 2 (+ the slide's 2 after)
constexpr std::uint32_t kArea172SlideStates = 0x63EF1C;   // Area172_SlideStates, 2
constexpr unsigned kArea172FallReach = 4;                 // entries 0..3 are code (the slide's two run on)
constexpr unsigned kArea172SlideCount = 2;
constexpr std::uint32_t kArea172DriftSteps = 0x63EF24;    // Area172_DriftSteps, 16 bytes
constexpr std::uint32_t kArea172EffectStates = 0x63EF34;  // Area172_EffectStates, 3
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
