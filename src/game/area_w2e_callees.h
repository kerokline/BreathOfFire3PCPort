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

namespace area_w2e {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells -----------------------------------------------------
constexpr U kStoryFlags = 0x904030;        // the flag bank every Flags_* call here is handed
constexpr U kTailKind = 0x9039F3;          // Field_ModeTailKinds' index (s8)
constexpr U kTailState = 0x9039F4;         // s8: the armed tail kind's (or a place hook's) state
constexpr U kTailArg = 0x9039F5;
constexpr U kScriptFlags = 0x9039A2;       // Field_ScriptFlags (u16; read as a dword where tested)
constexpr U kScriptFlags2 = 0x905BA4;      // Field_ScriptFlags2's low byte
constexpr U kInputHeld = 0x7E1BE8;         // Input_Held (the code reads its low 16 bits)
constexpr U kFieldInputHeld = 0x905BA6;    // Field_InputHeld (u16)
constexpr U kFieldInputFlags = 0x905BA2;   // Field_InputFlags
constexpr U kButtonMap0 = 0x903580;        // the field's button words: 0, 1 (the charge button) ..
constexpr U kButtonCharge = 0x903582;
constexpr U kActorStates = 0x903A80;       // Field_ActorStates: records of 0xA4
constexpr U kActorStride = 0xA4;
constexpr U kEdgeBits = 0x905B80;          // Field_EdgeBits (u16)
constexpr U kF3Divisor = 0x937F8C;         // MoveScript_F3Divisor (u16)
constexpr U kFAWord = 0x904EFE;            // MoveScript_FAWord (u16)
constexpr U kDirectionSteps = 0x6697B0;    // Field_DirectionSteps: (dx, dz) dwords by facing
constexpr U kPendingArea = 0x937F82;       // u16: the pending area / the place (world map)
constexpr U kPendingX = 0x903860;          // the pending area's cells (Field_ChangeArea's x, z)
constexpr U kPendingZ = 0x90384C;
constexpr U kPendingFlags = 0x905B88;
constexpr U kCell904EE0 = 0x904EE0;        // zeroed with 0x937F98 = 0xC by area 104's exit cell
constexpr U kCell937F98 = 0x937F98;
constexpr U kFoundX = 0x903850;            // the scratch word the step hook of area 105 stores the facing in
constexpr U kStatusBits = 0x8034E1;        // Field_StatusBits
constexpr U kCondFA = 0x8034E0;            // Cond_ByteFA (s8, the chapter)
constexpr U kCondFD = 0x8034F1;            // Cond_ByteFD
constexpr U kCondFE = 0x905E20;            // Cond_ByteFE
constexpr U kPrevArea = 0x802290;          // u16: the area the party came from
constexpr U kLeader = 0x802D40;            // ObjTrio + 0, the leader's record
constexpr U kLeaderState = 0x802D41;       // its +1 (leader state 12: area 104's controller)
constexpr U kLeaderDir = 0x802D48;         // its +8, the facing
constexpr U kLeaderA = 0x802D4A;           // its +0xA (the charge's hold)
constexpr U kLeaderB = 0x802D4B;           // its +0xB (the charge, 0..0x40)
constexpr U kLeaderX = 0x802D74;           // its +0x34 / +0x38 / +0x3C (16.16)
constexpr U kLeaderZ = 0x802D78;
constexpr U kLeaderY = 0x802D7C;
constexpr U kLeaderCellWordX = 0x802D76;   // the high words of +0x34 / +0x38
constexpr U kLeaderCellWordZ = 0x802D7A;
constexpr U kLeaderHeight = 0x802D7E;      // the word +0x3E
constexpr U kLeader89 = 0x802DC9;          // its +0x89
constexpr U kEffects = 0x7E11E0;           // Effect_Objects: twenty records of 0x80
constexpr U kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
constexpr U kObjects = 0x7DEE80;           // Sprite_Objects: thirty of 0xA4
constexpr U kObjectsExtra = 0x802000;      // Sprite_ObjectsExtra: four more
constexpr U kObjectStride = 0xA4;
constexpr U kActiveMember = 0x9035A4;      // Field_ActiveMember (a pointer)
constexpr U kScriptObject = 0x929E80;      // MoveScript_Object (a pointer)
constexpr U kEffectStates = 0x66972C;      // MoveScript_EffectState: a byte per +0x89 value
constexpr U kDescriptors = 0x667590;       // Area_Descriptors
constexpr U kPacketNext = 0x7E0670;        // Gfx_PacketNext
constexpr U kPassFlags = 0x7E0918;         // Draw_PassFlags
constexpr U kFrameCounter = 0x937F94;      // Frame_Counter
constexpr U kClutDirty = 0x937F90;         // Gfx_ClutStripDirty
constexpr U kMenuColour = 0x903A5A;        // the byte area 104's countdown hands Menu_DrawBox
constexpr U kCountdownText = 0x904BA0;     // the countdown's text buffer (Crt_sprintf's)
constexpr U kCountdownFormat = 0x61BC74;   // its format (data, read in place)
constexpr U kPanelTris = 0x61BC44;         // four (x0, x1, x2, y0, y1, y2) s16 rows: the panel's triangles
constexpr U kPanelTrisEnd = 0x61BC74;
// The minimap's image, its upload and its CLUT row (area 104's init).
constexpr U kUnpack = 0x8E4580;            // Gfx_UnpackScratch: two words (0x14, 0x44), then 69 rows of 40 bytes
constexpr U kUnpackBytes = 4 + 0x45 * 0x28;
constexpr U kUploadCount = 0x9035A0;       // Gfx_UploadQueueCount
constexpr U kUploadX = 0x903680;           // Gfx_UploadQueueX: twenty words
constexpr U kUploadY = 0x9036A8;           // Gfx_UploadQueueY: twenty words
constexpr U kUploadRecord = 0x92BF20;      // Gfx_UploadQueueRecord: twenty pointers
constexpr unsigned kUploadSlots = 20;
constexpr U kClutWords = 0x811440;         // four words of the live CLUT strip (the minimap's)

// --- area 104's own tables (its descriptor 0x61BB38) --------------------------------
constexpr U kA104LeaderStates = 0x61BC28;  // 2: Area104_LeaderIdle, Area104_LeaderStep (by the leader's +2)
constexpr unsigned kA104LeaderStateCount = 2;
constexpr U kA104Kind5CStates = 0x61BC30;  // 5: kind 0x5C's states by +1 (the last, 0x41C5B0, AR3B's)
constexpr unsigned kA104Kind5CStateCount = 5;
constexpr U kA104EntryPlus8 = 8;           // Sprite_InitFromEntry(descriptor[+8] + 8)
constexpr U kA106Cells = 0x61C6B4;         // four (x, z) byte pairs: area 106's handler 2
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
constexpr U kPlace = kPendingArea;         // the place the party stands on (world map)
constexpr U kMapMode = 0x9045FA;           // the world map's mode byte
constexpr U kLeaderCellX = 0x905E66;       // the high words of Field_Kind2X / Z
constexpr U kLeaderCellZ = 0x905E62;
constexpr U kButtonMap6 = 0x90358C;        // the field's button word 6
constexpr U kPartySet = 0x90412C;          // & 0x7F: the party set (0xC shows the third legend)
constexpr U kLeaderSteps = 0x802D49;       // the leader's +9
constexpr U kLeaderDirX = 0x802D4C;        // its +0xC / +0x10
constexpr U kLeaderDirZ = 0x802D50;

// Area 104: WorldMap_Records record 6 (0x6539B8: +0 0x414760, +0xC 0x414A90,
// +0x10 0x41B730 - AR3B's), WorldMap_FieldHooks entry 6 (0x662E08).
constexpr WorldMapTables kWm104 = {
    "Area104",
    0x61B4F0,                              // one plate animation (place 0x65), then a zero record
    0x61BB7C, 0x61BB90, 0x61BB98, 0x61BBA8,
    0x61BBB8, 0x61BC10,
    0x803580,
    0x414AC0, 0x414B10, 0x414B90, 0x414D30, 0x414F00, 0x414FC0,
};

}  // namespace at

// The unowned callees (above).
constexpr std::uint32_t kTurnToward121 = 0x41BE10;
constexpr std::uint32_t kMenuButton121 = 0x41C0A0;
constexpr std::uint32_t kHoldButton121 = 0x41C0E0;
constexpr std::uint32_t kTurnKeys121 = 0x41C110;
constexpr std::uint32_t kGaugeFrame121 = 0x41C350;
constexpr std::uint32_t kKind5CState4 = 0x41C5B0;
constexpr std::uint32_t kSetPolyF3 = 0x5A7570;

}  // namespace area_w2e
