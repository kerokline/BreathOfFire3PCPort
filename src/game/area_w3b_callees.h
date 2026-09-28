// Internal to area_w3b.cpp and area_w3b_fuzz.cpp: the cells and tables world
// 3's areas 120 and 121 touch that symbols.toml has no name for (the tables it
// names are listed too, by address, for the fuzz's regions), area 121's
// world-map table set, and the callees nobody owns this wave, by raw address.
// docs/area_w3b.md.
//
// Raw-address callees (the round's rebinding pass names them). All four lie
// in area 104's block (group AR2E's band 0x4146C0..0x4168E0 this wave): the
// linker folded identical code of areas 104 and 121 into one copy, and area
// 121's leader and effect code calls area 104's copy of these.
//   0x415640  (): Field_State +0x128 = 3; Sprite_Current +0xC, +0x10, +0x14 = 0;
//             MoveScript_F3Divisor = MoveScript_FAWord = 0 (read 2026-09-28,
//             0x415640..0x415675).
//   0x415680  (): Field_State +0x128 = 4 when the button word 0x903582 has a
//             bit of Input_Held and Sprite_Current +0xB is below 0x40, else 3
//             (read 2026-09-28, 0x415680..0x4156B9).
//   0x4156C0  (): Sprite_Current +0xA counted down, then a charge on +0xB by the
//             same button (read to 0x415730 on 2026-09-28: AR2E's to describe).
//   0x415940  (x, row): one POLY_FT4 of a gauge; both arguments bytes (read to
//             0x4159AD on 2026-09-28: AR2E's to describe).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w3b {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells --------------------------------------------------
constexpr U kStoryFlags = 0x904030;
constexpr U kTailKind = 0x9039F3;         // Field_ModeTailKinds' index (s8)
constexpr U kTailState = 0x9039F4;        // s8: the armed tail kind's (or the field hook's) state
constexpr U kTailArg = 0x9039F5;
constexpr U kMsgMode = kTailKind, kMsgState = kTailState, kMsgArg = kTailArg;
constexpr U kLeader = 0x802D40;           // ObjTrio + 0, the leader's record
constexpr U kLeaderDir = 0x802D48;        // the leader's +8, its facing
constexpr U kLeaderSteps = 0x802D49;      // +9
constexpr U kLeaderA = 0x802D4A;          // +0xA: effect kind 0x5C's gauge draws the blink while it is not 0
constexpr U kLeaderB = 0x802D4B;          // +0xB: the charge the gauge's second row shows (0x40 - it)
constexpr U kLeaderDirX = 0x802D4C;       // +0xC / +0x10: the step direction (dwords)
constexpr U kLeaderDirZ = 0x802D50;
constexpr U kLeaderX = 0x802D74;          // +0x34 / +0x38 / +0x3C, 16.16
constexpr U kLeaderZ = 0x802D78;
constexpr U kLeaderY = 0x802D7C;
constexpr U kLeaderCellWordX = 0x802D76;  // the high word of +0x34
constexpr U kLeaderCellWordZ = 0x802D7A;
constexpr U kLeaderHeight = 0x802D7E;     // the word +0x3E
constexpr U kPartyStride = 0x14C;
constexpr U kObjectStride = 0xA4;         // Sprite_Objects / Sprite_ObjectsExtra / Field_ActorStates records
constexpr U kEffectStride = 0x80;
constexpr U kScriptVar3 = 0x903848;       // MoveScript variables 3..6 (docs/area_w0a.md)
constexpr U kScriptVar4 = 0x903849;
constexpr U kScriptVar5 = 0x90384A;
constexpr U kScriptVar6 = 0x90384B;
constexpr U kScriptVar7 = 0x8034E4;       // MoveScript_Var7 (s8): the running scene's run
constexpr U kScriptStep = 0x8034E5;       // the run's step
constexpr U kChoice = 0x7DEE67;           // the choice box's answer (s8)
constexpr U kRowPointer = 0x903804;       // a record pointer whose word +0x8A a choice counts (docs/area_w1a.md)
constexpr U kButtonMap0 = 0x903580;       // the field's button map: word 0 (the dial's first legend)
constexpr U kButtonMap1 = 0x903582;       // word 1: the push / charge button
constexpr U kButtonMap3 = 0x903586;       // word 3: Field_Request 4's button
constexpr U kButtonMap6 = 0x90358C;       // word 6 (the dial's second legend)
constexpr U kPlace = 0x937F82;            // u16: the place the party stands on (world map)
constexpr U kMapMode = 0x9045FA;          // the world map's mode byte
constexpr U kLeaderCellX = 0x905E66;      // the high words of Field_Kind2X / Z
constexpr U kLeaderCellZ = 0x905E62;
constexpr U kMapHeight = 0x8CB581;        // AreaMap_Header's second byte
constexpr U kPartySet = 0x90412C;         // & 0x7F: the party set (0xC shows the dial's third legend)
constexpr U kAreaText = 0x803580;         // the area's text section; the label's offset is its first dword's low word
constexpr U kFlag3A79 = 0x903A79;         // the record +4 state: 9 releases the effect
constexpr U kChangeFlags = 0x905B88;      // byte: Field_ChangeArea's fourth argument on a 0xAF cell
constexpr U kPendingX = 0x903860;         // dword: Field_ChangeArea's x (the pending area cells, docs/area_harness.md)
constexpr U kPendingZ = 0x90384C;         // dword: its z
constexpr U k904EE0 = 0x904EE0;           // byte zeroed on the 0xAF cell's change of area
constexpr U k937F98 = 0x937F98;           // byte set to 0xC on it
constexpr U kFieldStateNow = 0x905D98;    // Field_State

// --- area 120's (its descriptor 0x623648: only a choice table) ------------------
constexpr U kA120Choices = 0x62363C;      // 3: its descriptor's +0x34

// --- area 121's own (its descriptor 0x6245D8) ------------------------------------
constexpr U kA121Handlers = 0x6245C8;     // 3: its descriptor's +0x3C
constexpr U kA121CameraStates = 0x62461C; // 2: handler 0's by Sprite_Current +4
constexpr U kLeaderStates = 0x624704;     // 2: leader state 12's (Area121_LeaderRun) by Sprite_Current +2
constexpr U kKind5CStates = 0x62470C;     // 5: effect kind 0x5C's (Area121_Kind5CRun) by Sprite_Current +1
constexpr U kBandHeights = 0x624720;      // bytes by Cond_ByteFE (1..2): the top band's bottom edge

// --- area 121's world-map tables (area 87's layout, docs/area_w2b.md section 4;
// the field hook's place list replaces area 87's place rows, cells and names) ---
struct WorldMapTables {
    const char* name;                 // "Area121": the messages' prefix
    U places, places_end;             // the field hook's u16 places (four), searched in bounds
    U plate_anims;                    // (u16 place, u8 animation, u8), searched with NO bound
    U cells;                          // (u8 x, u8 z, u8, u8): Record4MarkCell's by +0xB, unchecked
    U plate_states, hud_states, frame_states, box_states;
    U sprites, buttons;               // (w, h, u, v) x 22; (u16 mask, u8 sprite, u8) x 6, the second legend reads eight
    U record8_states, directions, record8_anims, record4_states;
    U label_offset;                   // the dword whose low word is the region label's offset in kAreaText
    unsigned plate_bank;              // PlateStart's Sprite_SetAnimationBank
    // the copy's own functions another of them calls directly
    U fn_frame_step, fn_frame_hold, fn_box_step, fn_draw_frame, fn_draw_sprite, fn_draw_hud;
};

// Area 121: WorldMap_Records record 8 (0x6539F0), WorldMap_FieldHooks entry 8
// (0x662E10); the tables in its data block after area 120's descriptor.
constexpr WorldMapTables kWm121 = {
    "Area121",
    0x624624, 0x62462C,   // 4 places
    0x623690,             // 4 plate animations
    0x6236A0,             // 1 cell record (then zeros)
    0x62462C, 0x624640, 0x624648, 0x624658,
    0x624668, 0x6246C0,
    0x6246D8, 0x6246E4, 0x6246F4, 0x6246FC,
    0x803580, 0x158,
    bof3::addr::Area121_FrameStep, bof3::addr::Area121_FrameHold, bof3::addr::Area121_BoxStep,
    bof3::addr::Area121_DrawFrame, bof3::addr::Area121_DrawSprite, bof3::addr::Area121_DrawHud,
};

}  // namespace at

// The unowned callees (above).
constexpr std::uint32_t kLeaderHalt104 = bof3::addr::Area104_StopMotion;
constexpr std::uint32_t kLeaderPace104 = bof3::addr::Area104_PoseByCharge;
constexpr std::uint32_t kLeaderCharge104 = bof3::addr::Area104_LeaderCharge;
constexpr std::uint32_t kGaugeRow104 = bof3::addr::Area104_DrawGauge;

// Area 121's own functions another of them calls directly (by the original
// address, so the fuzz can stand a recorder in).
constexpr std::uint32_t kPushObject = bof3::addr::Area121_PushObject;
constexpr std::uint32_t kStepAround = bof3::addr::Area121_StepAround;
constexpr std::uint32_t kDirectionTo = bof3::addr::Area121_DirectionTo;
constexpr std::uint32_t kStepOffObject = bof3::addr::Area121_StepOffObject;
constexpr std::uint32_t kLeaderControl = bof3::addr::Area121_LeaderControl;
constexpr std::uint32_t kMenuButton = bof3::addr::Area121_MenuButton;
constexpr std::uint32_t kRequest4Button = bof3::addr::Area121_Request4Button;
constexpr std::uint32_t kTurnInput = bof3::addr::Area121_TurnInput;
constexpr std::uint32_t kKind5CFollow = bof3::addr::Area121_Kind5CFollow;
constexpr std::uint32_t kGaugeSprite = bof3::addr::Area121_GaugeSprite;
constexpr std::uint32_t kKind5CFace = bof3::addr::Area121_Kind5CFace;
constexpr std::uint32_t kKind5CTurnStep = bof3::addr::Area121_Kind5CTurnStep;
constexpr std::uint32_t kKind5CTurn = bof3::addr::Area121_Kind5CTurn;
constexpr std::uint32_t kRingRise = bof3::addr::Area121_RingRise;

}  // namespace area_w3b
