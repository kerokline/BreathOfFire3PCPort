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

#include "common/image_base.h"

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w3b {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells --------------------------------------------------
constexpr ::bof3::image::Va kStoryFlags{0x904030};
// kTailKind: now AreaW0a_TailKind (symbols.toml). Field_ModeTailKinds' index (s8)
// kTailState: now AreaW0c_TailState (symbols.toml). s8: the armed tail kind's (or the field hook's) state
// kTailArg: now AreaW0c_TailSub (symbols.toml).
// kMsgMode: now AreaW0a_TailKind (symbols.toml).
// kMsgState: now AreaW0c_TailState (symbols.toml).
// kMsgArg: now AreaW0c_TailSub (symbols.toml).
constexpr ::bof3::image::Va kLeader{0x802D40};           // ObjTrio + 0, the leader's record
// kLeaderDir: now ObjTrio[0].direction (symbols.toml). the leader's +8, its facing
// kLeaderSteps: now ObjTrio[0].step_frames (symbols.toml). +9
// kLeaderA: now ObjTrio_addr + 0xa (symbols.toml). +0xA: effect kind 0x5C's gauge draws the blink while it is not 0
// kLeaderB: now ObjTrio_addr + 0xb (symbols.toml). +0xB: the charge the gauge's second row shows (0x40 - it)
// kLeaderDirX: now ObjTrio[0].step_x (symbols.toml). +0xC / +0x10: the step direction (dwords)
// kLeaderDirZ: now ObjTrio[0].step_z (symbols.toml).
// kLeaderX: now ObjTrio[0].x (symbols.toml). +0x34 / +0x38 / +0x3C, 16.16
// kLeaderZ: now ObjTrio[0].z (symbols.toml).
// kLeaderY: now ObjTrio[0].height (symbols.toml).
constexpr ::bof3::image::Va kLeaderCellWordX{0x802D76};  // the high word of +0x34
constexpr ::bof3::image::Va kLeaderCellWordZ{0x802D7A};
// kLeaderHeight: now ObjTrio[0].height_whole (symbols.toml). the word +0x3E
constexpr U kPartyStride = 0x14C;
constexpr U kObjectStride = 0xA4;         // Sprite_Objects / Sprite_ObjectsExtra / Field_ActorStates records
constexpr U kEffectStride = 0x80;
constexpr ::bof3::image::Va kScriptVar3{0x903848};       // MoveScript variables 3..6 (docs/area_w0a.md)
constexpr ::bof3::image::Va kScriptVar4{0x903849};
constexpr ::bof3::image::Va kScriptVar5{0x90384A};
constexpr ::bof3::image::Va kScriptVar6{0x90384B};
// kScriptVar7: now MoveScript_Var7 (symbols.toml). MoveScript_Var7 (s8): the running scene's run
// kScriptStep: now MoveScript_PartyRecords[0x65] (symbols.toml). the run's step
// kChoice: now AreaW0c_ChoiceAnswer (symbols.toml). the choice box's answer (s8)
// kRowPointer: now AreaW1a_FocusObject (symbols.toml). a record pointer whose word +0x8A a choice counts (docs/area_w1a.md)
// kButtonMap0: now Gfx_UnpackScratch[0x1f000] (symbols.toml). the field's button map: word 0 (the dial's first legend)
// kButtonMap1: now Gfx_UnpackScratch[0x1f002] (symbols.toml). word 1: the push / charge button
// kButtonMap3: now Gfx_UnpackScratch[0x1f006] (symbols.toml). word 3: Field_Request 4's button
// kButtonMap6: now Gfx_UnpackScratch[0x1f00c] (symbols.toml). word 6 (the dial's second legend)
constexpr ::bof3::image::Va kPlace{0x937F82};            // u16: the place the party stands on (world map)
// kMapMode: now AreaW0b_MapMode (symbols.toml). the world map's mode byte
constexpr ::bof3::image::Va kLeaderCellX{0x905E66};      // the high words of Field_Kind2X / Z
constexpr ::bof3::image::Va kLeaderCellZ{0x905E62};
// kMapHeight: now AreaMap_Header[1] (symbols.toml). AreaMap_Header's second byte
// kPartySet: now AreaW0b_PartySet (symbols.toml). & 0x7F: the party set (0xC shows the dial's third legend)
constexpr ::bof3::image::Va kAreaText{0x803580};         // the area's text section; the label's offset is its first dword's low word
constexpr ::bof3::image::Va kFlag3A79{0x903A79};         // the record +4 state: 9 releases the effect
// kChangeFlags: now AreaEntry_PendingFlags (symbols.toml). byte: Field_ChangeArea's fourth argument on a 0xAF cell
inline auto& kPendingX = SharedScratch.d_10;         // dword: Field_ChangeArea's x (the pending area cells, docs/area_harness.md)
// kPendingZ: now AreaEntry_PendingZ (symbols.toml). dword: its z
// k904EE0: now ModeFlow_AreaTransition (symbols.toml). byte zeroed on the 0xAF cell's change of area
// k937F98: now Area_EnterTransition (symbols.toml). byte set to 0xC on it
constexpr ::bof3::image::Va kFieldStateNow{0x905D98};    // Field_State

// --- area 120's (its descriptor 0x623648: only a choice table) ------------------
constexpr ::bof3::image::Va kA120Choices{0x62363C};      // 3: its descriptor's +0x34

// --- area 121's own (its descriptor 0x6245D8) ------------------------------------
constexpr ::bof3::image::Va kA121Handlers{0x6245C8};     // 3: its descriptor's +0x3C
// kA121CameraStates: now Area121_CameraStates (symbols.toml). 2: handler 0's by Sprite_Current +4
// kLeaderStates: now Area121_LeaderStates (symbols.toml). 2: leader state 12's (Area121_LeaderRun) by Sprite_Current +2
// kKind5CStates: now Area121_Kind5CStates (symbols.toml). 5: effect kind 0x5C's (Area121_Kind5CRun) by Sprite_Current +1
// kBandHeights: now Area121_BandHeights (symbols.toml). bytes by Cond_ByteFE (1..2): the top band's bottom edge

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
    ::bof3::Img(0x624624), ::bof3::Img(0x62462C),   // 4 places
    ::bof3::Img(0x623690),             // 4 plate animations
    ::bof3::Img(0x6236A0),             // 1 cell record (then zeros)
    ::bof3::Img(0x62462C), ::bof3::Img(0x624640), ::bof3::Img(0x624648), ::bof3::Img(0x624658),
    ::bof3::Img(0x624668), ::bof3::Img(0x6246C0),
    ::bof3::Img(0x6246D8), ::bof3::Img(0x6246E4), ::bof3::Img(0x6246F4), ::bof3::Img(0x6246FC),
    ::bof3::Img(0x803580), 0x158,
    bof3::addr::Area121_FrameStep, bof3::addr::Area121_FrameHold, bof3::addr::Area121_BoxStep,
    bof3::addr::Area121_DrawFrame, bof3::addr::Area121_DrawSprite, bof3::addr::Area121_DrawHud,
};

}  // namespace at

// The unowned callees (above).
constexpr ::bof3::image::CodeVa kLeaderHalt104{bof3::addr::Area104_StopMotion};
constexpr ::bof3::image::CodeVa kLeaderPace104{bof3::addr::Area104_PoseByCharge};
constexpr ::bof3::image::CodeVa kLeaderCharge104{bof3::addr::Area104_LeaderCharge};
constexpr ::bof3::image::CodeVa kGaugeRow104{bof3::addr::Area104_DrawGauge};

// Area 121's own functions another of them calls directly (by the original
// address, so the fuzz can stand a recorder in).
constexpr ::bof3::image::CodeVa kPushObject{bof3::addr::Area121_PushObject};
constexpr ::bof3::image::CodeVa kStepAround{bof3::addr::Area121_StepAround};
constexpr ::bof3::image::CodeVa kDirectionTo{bof3::addr::Area121_DirectionTo};
constexpr ::bof3::image::CodeVa kStepOffObject{bof3::addr::Area121_StepOffObject};
constexpr ::bof3::image::CodeVa kLeaderControl{bof3::addr::Area121_LeaderControl};
constexpr ::bof3::image::CodeVa kMenuButton{bof3::addr::Area121_MenuButton};
constexpr ::bof3::image::CodeVa kRequest4Button{bof3::addr::Area121_Request4Button};
constexpr ::bof3::image::CodeVa kTurnInput{bof3::addr::Area121_TurnInput};
constexpr ::bof3::image::CodeVa kKind5CFollow{bof3::addr::Area121_Kind5CFollow};
constexpr ::bof3::image::CodeVa kGaugeSprite{bof3::addr::Area121_GaugeSprite};
constexpr ::bof3::image::CodeVa kKind5CFace{bof3::addr::Area121_Kind5CFace};
constexpr ::bof3::image::CodeVa kKind5CTurnStep{bof3::addr::Area121_Kind5CTurnStep};
constexpr ::bof3::image::CodeVa kKind5CTurn{bof3::addr::Area121_Kind5CTurn};
constexpr ::bof3::image::CodeVa kRingRise{bof3::addr::Area121_RingRise};

}  // namespace area_w3b
