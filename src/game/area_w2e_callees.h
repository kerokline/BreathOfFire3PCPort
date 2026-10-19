// Internal to area_w2e.cpp and area_w2e_fuzz.cpp: the cells and tables world
// 2's areas 104..106 touch that symbols.toml has no name for (the tables it
// names are listed too, by address, for the fuzz's regions), area 104's
// world-map table set, and the callees nobody owns this wave, by raw address.
// docs/area_w2e.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x41BE10  (x, z, object): area 121's code (group AR3B's band this wave):
//             answers the facing byte Area104_TurnToFree stores (read
//             2026-09-28 only as far as its call shape).
//   0x41C0A0  (): area 121's menu-button test - Field_MenuButton pressed and
//             Field_ScriptFlags bit 6 clear: Sound_PlayEffect(0x105),
//             Field_Request 1, Sprite_Current +2 = 0, al 1; else al 0 (AR3B's).
//   0x41C0E0  (): area 121's: Field_ScriptFlags bit 13 clear and the button
//             word 0x903586 held: Field_Request 4, Sprite_Current +2 = 0, al
//             1; else al 0 (AR3B's).
//   0x41C110  (): area 121's turn keys - Input_Held bit 14 turns
//             Sprite_Current +8 round (^ 4), bit 13 one way, bit 15 the
//             other, bit 12 as bit 14; al 1 when one did, else 0 (AR3B's).
//   0x41C350  (x, y, frame): area 121's gauge frame draw (AR3B's).
//   0x41C5B0  (): area 121's; kind 0x5C's state 4 (AR3B's).
//   0x5A7570  (prim): an engine GPU setter nobody owns - +7 = 0x20 (POLY_F3)
//             and the three vertices' z floats (+0x10, +0x1C, +0x28) =
//             0x3C23D70A (read 2026-09-28, 0x5A7570..0x5A7586).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w2e {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells -----------------------------------------------------
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the flag bank every Flags_* call here is handed
// kTailKind: now AreaW0a_TailKind (symbols.toml). Field_ModeTailKinds' index (s8)
// kTailState: now AreaW0c_TailState (symbols.toml). s8: the armed tail kind's (or a place hook's) state
// kTailArg: now AreaW0c_TailSub (symbols.toml).
constexpr U kScriptFlags = 0x9039A2;       // Field_ScriptFlags (u16; read as a dword where tested)
constexpr U kScriptFlags2 = 0x905BA4;      // Field_ScriptFlags2's low byte
// kInputHeld: now Input_Held (symbols.toml). Input_Held (the code reads its low 16 bits)
// kFieldInputHeld: now Field_InputHeld (symbols.toml). Field_InputHeld (u16)
// kFieldInputFlags: now Field_InputFlags (symbols.toml). Field_InputFlags
// kButtonMap0: now Gfx_UnpackScratch[0x1f000] (symbols.toml). the field's button words: 0, 1 (the charge button) ..
// kButtonCharge: now Gfx_UnpackScratch[0x1f002] (symbols.toml).
constexpr U kActorStates = 0x903A80;       // Field_ActorStates: records of 0xA4
constexpr U kActorStride = 0xA4;
// kEdgeBits: now Field_EdgeBits (symbols.toml). Field_EdgeBits (u16)
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). MoveScript_F3Divisor (u16)
// kFAWord: now MoveScript_FAWord (symbols.toml). MoveScript_FAWord (u16)
// kDirectionSteps: now Field_DirectionSteps (symbols.toml). Field_DirectionSteps: (dx, dz) dwords by facing
// kPendingArea: now AreaW0b_Place (symbols.toml). u16: the pending area / the place (world map)
constexpr U kPendingX = 0x903860;          // the pending area's cells (Field_ChangeArea's x, z)
// kPendingZ: now AreaEntry_PendingZ (symbols.toml).
// kPendingFlags: now AreaEntry_PendingFlags (symbols.toml).
// kCell904EE0: now ModeFlow_AreaTransition (symbols.toml). zeroed with 0x937F98 = 0xC by area 104's exit cell
// kCell937F98: now AreaW0c_PendingKind (symbols.toml).
constexpr U kFoundX = 0x903850;            // the scratch word the step hook of area 105 stores the facing in
// kStatusBits: now Field_StatusBits (symbols.toml). Field_StatusBits
// kCondFA: now Cond_ByteFA (symbols.toml). Cond_ByteFA (s8, the chapter)
// kCondFD: now Cond_ByteFD (symbols.toml). Cond_ByteFD
// kCondFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
// kPrevArea: now AreaW2c_LastArea (symbols.toml). u16: the area the party came from
constexpr U kLeader = 0x802D40;            // ObjTrio + 0, the leader's record
// kLeaderState: now ObjTrio[1] (symbols.toml). its +1 (leader state 12: area 104's controller)
// kLeaderDir: now ObjTrio[8] (symbols.toml). its +8, the facing
// kLeaderA: now ObjTrio[0xa] (symbols.toml). its +0xA (the charge's hold)
// kLeaderB: now ObjTrio[0xb] (symbols.toml). its +0xB (the charge, 0..0x40)
// kLeaderX: now ObjTrio[0x34] (symbols.toml). its +0x34 / +0x38 / +0x3C (16.16)
// kLeaderZ: now ObjTrio[0x38] (symbols.toml).
// kLeaderY: now ObjTrio[0x3c] (symbols.toml).
// kLeaderCellWordX: now ObjTrio[0x36] (symbols.toml). the high words of +0x34 / +0x38
// kLeaderCellWordZ: now ObjTrio[0x3a] (symbols.toml).
// kLeaderHeight: now ObjTrio[0x3e] (symbols.toml). the word +0x3E
// kLeader89: now ObjTrio[0x89] (symbols.toml). its +0x89
// kEffects: now Effect_Objects (symbols.toml). Effect_Objects: twenty records of 0x80
constexpr U kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// kObjects: now Sprite_Objects (symbols.toml). Sprite_Objects: thirty of 0xA4
// kObjectsExtra: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra: four more
constexpr U kObjectStride = 0xA4;
// kActiveMember: now Field_ActiveMember (symbols.toml). Field_ActiveMember (a pointer)
// kScriptObject: now MoveScript_Object (symbols.toml). MoveScript_Object (a pointer)
constexpr U kEffectStates = 0x66972C;      // MoveScript_EffectState: a byte per +0x89 value
// kDescriptors: now Area_Descriptors (symbols.toml). Area_Descriptors
constexpr U kPacketNext = 0x7E0670;        // Gfx_PacketNext
constexpr U kPassFlags = 0x7E0918;         // Draw_PassFlags
constexpr U kFrameCounter = 0x937F94;      // Frame_Counter
constexpr U kClutDirty = 0x937F90;         // Gfx_ClutStripDirty
// kMenuColour: now BattleDraw_Colour (symbols.toml). the byte area 104's countdown hands Menu_DrawBox
constexpr U kCountdownText = 0x904BA0;     // the countdown's text buffer (Crt_sprintf's)
constexpr U kCountdownFormat = 0x61BC74;   // its format (data, read in place)
// kPanelTris: now Area104_PanelTris (symbols.toml). four (x0, x1, x2, y0, y1, y2) s16 rows: the panel's triangles
constexpr U kPanelTrisEnd = 0x61BC74;
// The minimap's image, its upload and its CLUT row (area 104's init).
// kUnpack: now Gfx_UnpackScratch (symbols.toml). Gfx_UnpackScratch: two words (0x14, 0x44), then 69 rows of 40 bytes
constexpr U kUnpackBytes = 4 + 0x45 * 0x28;
constexpr U kUploadCount = 0x9035A0;       // Gfx_UploadQueueCount
// kUploadX: now Gfx_UploadQueueX (symbols.toml). Gfx_UploadQueueX: twenty words
// kUploadY: now Gfx_UploadQueueY (symbols.toml). Gfx_UploadQueueY: twenty words
// kUploadRecord: now Gfx_UploadQueueRecord (symbols.toml). Gfx_UploadQueueRecord: twenty pointers
constexpr unsigned kUploadSlots = 20;
// kClutWords: now Gfx_ClutStrip[0xf60] (symbols.toml). four words of the live CLUT strip (the minimap's)

// --- area 104's own tables (its descriptor 0x61BB38) --------------------------------
// kA104LeaderStates: now Area104_LeaderStates (symbols.toml). 2: Area104_LeaderIdle, Area104_LeaderStep (by the leader's +2)
constexpr unsigned kA104LeaderStateCount = 2;
// kA104Kind5CStates: now Area104_Kind5CStates (symbols.toml). 5: kind 0x5C's states by +1 (the last, 0x41C5B0, AR3B's)
constexpr unsigned kA104Kind5CStateCount = 5;
constexpr U kA104EntryPlus8 = 8;           // Sprite_InitFromEntry(descriptor[+8] + 8)
// kA106Cells: now Area106_Cells (symbols.toml). four (x, z) byte pairs: area 106's handler 2
constexpr U kA106CellsEnd = 0x61C6BC;

// The world-map copy's tables (area 45's layout, docs/area_w1b.md section 5):
// the place hook and plate start are not the copies' (area 104's hook is its
// own, its plate state 0 is 0x41ACD0, outside the band), and record 6 has no
// +4 / +8 / +0x14, so no cell, name, direction or drift table.
struct WorldMapTables {
    const char* name;
    U plate_anims;                         // (u16 place, u8 animation, u8), searched with NO bound
    U plate_states, hud_states, frame_states, box_states;
    U sprites, buttons;                    // (w, h, u, v) x 22; (u16 mask, u8 sprite, u8) x 6, the second legend reads eight
    U label_offset;                        // the dword whose low word is the region label's offset in kAreaText
    U fn_frame_step, fn_frame_hold, fn_box_step, fn_draw_frame, fn_draw_sprite, fn_draw_hud;
};
constexpr U kAreaText = 0x803580;
// kPlace: now AreaW0b_Place (symbols.toml). the place the party stands on (world map)
// kMapMode: now AreaW0b_MapMode (symbols.toml). the world map's mode byte
constexpr U kLeaderCellX = 0x905E66;       // the high words of Field_Kind2X / Z
constexpr U kLeaderCellZ = 0x905E62;
// kButtonMap6: now Gfx_UnpackScratch[0x1f00c] (symbols.toml). the field's button word 6
// kPartySet: now AreaW0b_PartySet (symbols.toml). & 0x7F: the party set (0xC shows the third legend)
// kLeaderSteps: now ObjTrio[9] (symbols.toml). the leader's +9
// kLeaderDirX: now ObjTrio[0xc] (symbols.toml). its +0xC / +0x10
// kLeaderDirZ: now ObjTrio[0x10] (symbols.toml).

// Area 104: WorldMap_Records record 6 (0x6539B8: +0 0x414760, +0xC 0x414A90,
// +0x10 0x41B730 - AR3B's), WorldMap_FieldHooks entry 6 (0x662E08).
constexpr WorldMapTables kWm104 = {
    "Area104",
    0x61B4F0,                              // one plate animation (place 0x65), then a zero record
    0x61BB7C, 0x61BB90, 0x61BB98, 0x61BBA8,
    0x61BBB8, 0x61BC10,
    0x803580,
    bof3::addr::Area104_FrameStep, bof3::addr::Area104_FrameHold, bof3::addr::Area104_BoxStep,
    bof3::addr::Area104_DrawFrame, bof3::addr::Area104_DrawSprite, bof3::addr::Area104_DrawHud,
};

}  // namespace at

// The unowned callees (above).
constexpr std::uint32_t kTurnToward121 = bof3::addr::Area121_DirectionTo;
constexpr std::uint32_t kMenuButton121 = bof3::addr::Area121_MenuButton;
constexpr std::uint32_t kHoldButton121 = bof3::addr::Area121_Request4Button;
constexpr std::uint32_t kTurnKeys121 = bof3::addr::Area121_TurnInput;
constexpr std::uint32_t kGaugeFrame121 = bof3::addr::Area121_GaugeSprite;
constexpr std::uint32_t kKind5CState4 = bof3::addr::Area121_RingRise;
constexpr std::uint32_t kSetPolyF3 = bof3::addr::Gpu_SetPolyF3;

}  // namespace area_w2e
