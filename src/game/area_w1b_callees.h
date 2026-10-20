// Internal to area_w1b.cpp and area_w1b_fuzz.cpp: the cells and tables world
// 1's areas 42..47 touch that symbols.toml has no name for (the tables it
// names are listed too, by address, for the fuzz's regions), and the callees
// nobody owns this wave, by raw address. docs/area_w1b.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x40E750  (x, y, w, h, flag): a window frame drawn into the packet buffer
//             (Gpu_SetDrawMode, Gfx_CommitPrim, ...); area 75's block
//             (group AR1F, not this wave). Area 42's timer draws it.
//   0x57CD90  (): a free Sprite_Objects index 0..0x1D in al, 0xFF none
//             (docs/scena_sc3.md); group SX's this wave.
//   0x57C160  (bits, index): toggles bit (index & 7) of bits[(index & 0xFF)
//             >> 3] - Flags_Set's and Flags_Clear's sibling (read 2026-09-27,
//             0x57C160..0x57C17E); engine code nobody owns.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w1b {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells ------------------------------------------------
// kChoice: now AreaW0c_ChoiceAnswer (symbols.toml). s8: the choice the message box committed
constexpr U kMessage = 0x7DEE48;          // u16: the message a choice handler opens, 0xFFFF none
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the flag bank every Flags_* call here is handed
constexpr U kFlagsCC = 0x9040CC;          // a second bank: area 42 tests its bits 0x5C..0x5E
// kTailKind: now AreaW0a_TailKind (symbols.toml). Field_ModeTailKinds' index (s8)
// kTailState: now AreaW0c_TailState (symbols.toml). s8: the armed tail kind's state
// kTailArg: now AreaW0c_TailSub (symbols.toml). area 42: a frame count, then an effect slot
constexpr U kTailTimer = 0x9039F6;        // u16: area 42's countdown in frames (0x384 from its handler 0)
// kLeader: now ObjTrio (symbols.toml). ObjTrio + 0, the leader's record
// kLeaderX: now ObjTrio[0x34] (symbols.toml). the leader's +0x34 / +0x38, 16.16
// kLeaderZ: now ObjTrio[0x38] (symbols.toml).
// kLeaderEffect: now ObjTrio[0x89] (symbols.toml). the leader's +0x89: Character_IndexById's index
// kLeaderDir: now ObjTrio[8] (symbols.toml). the leader's +8, its facing
// kFoundSlot: now SharedScratch (symbols.toml). a found slot (word or byte, as each writer stores it)
// kCameraAngle1: now Camera_Angles[1] (symbols.toml). Camera_Angles[1]
// kCameraAngle2: now Cond_AngleFB (symbols.toml). Camera_Angles[2] (Cond_AngleFB's low half)
constexpr U kTextBuffer = 0x904BA0;       // area 42's time string, by Crt_sprintf
constexpr U kRank = 0x675A00;             // u8: area 42's result, 0..2 (its init and its check write it)
constexpr U kCells44 = 0x90384B;          // u8: area 44's two gates, bits 0..1 and 4..5
constexpr U kCount44 = 0x90384A;          // u8: area 44's init zeroes it with kCells44
constexpr U kDrop46 = 0x903849;           // u8: area 46's party-drop state (1 armed, 2 / 0 done)
// kExtra0: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra[0]
// kExtra1: now Sprite_ObjectsExtra[0xa4] (symbols.toml). Sprite_ObjectsExtra[1]
constexpr U kExtraStride = 0xA4;
constexpr U kPartyStride = 0x14C;
constexpr U kEffectStride = 0x80;

// --- area 42's tables (after its descriptor 0x5F6290's handler array) ------
constexpr U kA42Handlers = 0x5F6274;      // 7: its +0x3C array; +0x34 is its tail (entry 6)
// kA42Messages: now Area42_Messages (symbols.toml). u16 by the s8 choice, unchecked
// kA42Placements: now Area42_Placements (symbols.toml). 3 records of 13 bytes, EventOp_9x's operand, by kRank
constexpr U kA42Format = 0x5F6308;        // the time's two-number format (read-only)

// --- area 43's ----------------------------------------------------------------
constexpr U kA43Handlers = 0x5F6A18;      // 3
// kA43States: now Area43_States (symbols.toml). 2: its handler 0's state table, by Sprite_Current[4]

// --- area 44's ----------------------------------------------------------------
// kA44Gate0: now Area44_Gate0 (symbols.toml). (u8 x, u8 z) x 4 by kCells44 & 3: extra object 0's cell
// kA44Gate1: now Area44_Gate1 (symbols.toml). (u8 x, u8 z) x 4 by (kCells44 >> 4) & 3: extra object 1's
// kA44Switches: now Area44_Switches (symbols.toml). (x, z, facing, flag, tail kind) x 4: the cell hook's
constexpr U kA44SwitchesEnd = 0x5F7096;

// --- area 45's (the world-map copy's, in its data block after area 46's
// descriptor; tools/area_rows.py moves them to area 45) ------------------------
// kA45PlateAnims: now Area45_PlateAnims (symbols.toml). (u16 place, u8 animation, u8) x 10, searched with NO bound
// kA45Cells: now Area45_Cells (symbols.toml). (u8 x, u8 z, u8, u8 id) x 265, searched with NO bound
constexpr U kA45CellsEnd = 0x5F74E4;
// kA45PlaceMessages: now Area45_PlaceMessages (symbols.toml). 11 rows of 0x20: u16 place, fifteen u16 messages
constexpr U kA45PlaceMessagesEnd = 0x5F7644;
// kA45NameSets: now Area45_NameSets (symbols.toml). 3 records of (u8 id, u8 item x 4)
constexpr U kA45NameSetsEnd = 0x5F7653;
// kA45PlateStates: now Area45_PlateStates (symbols.toml). 5
// kA45HudStates: now Area45_HudStates (symbols.toml). 2
// kA45FrameStates: now Area45_FrameStates (symbols.toml). 4
// kA45BoxStates: now Area45_BoxStates (symbols.toml). 4
// kA45Sprites: now Area45_Sprites (symbols.toml). (w, h, u, v) x 22, by index & 0xFF unchecked
// kA45Buttons: now Area45_Buttons (symbols.toml). (u16 mask, u8 sprite, u8) x 6; the second legend reads EIGHT
// kA45Record8States: now Area45_Record8States (symbols.toml). 3 (0x4253C0 and 0x40C490 other areas' shared ones)
// kA45Directions: now Area45_Directions (symbols.toml). (s16 dx, s16 dz) x 4 by +8, unchecked
// kA45Record8Anims: now Area45_Record8Anims (symbols.toml). (u8 animation, u8 +0x2A) x 4 by +8, unchecked
// kA45Record4States: now Area45_Record4States (symbols.toml). 2: Area45_Record4MarkCell, Area45_Record4Tick (shared)
// kA45DriftUBase: now Area45_DriftUV (symbols.toml). u8 by +0xB - 2; v base +4, cell size +8
// kA45DriftVBase: now Area45_DriftUV[4] (symbols.toml).
// kA45DriftSize: now Area45_DriftUV[8] (symbols.toml).

// --- area 45's world-map cells (area 16's, docs/area_w0b.md) -------------------
// kLeaderSteps: now ObjTrio[9] (symbols.toml). u8 step count
// kLeaderDirX: now ObjTrio[0xc] (symbols.toml). dword direction
// kLeaderDirZ: now ObjTrio[0x10] (symbols.toml).
// kLeaderCellWordX: now ObjTrio[0x36] (symbols.toml). the high word of +0x34
// kLeaderCellWordZ: now ObjTrio[0x3a] (symbols.toml). the high word of +0x38
// kPlace: now AreaW0b_Place (symbols.toml). u16: the place the party stands on (world map)
// kMapMode: now AreaW0b_MapMode (symbols.toml). the world map's mode byte
constexpr U kLeaderCellX = 0x905E66;      // the high words of Field_Kind2X / Z
constexpr U kLeaderCellZ = 0x905E62;
// kMapHeight: now AreaMap_Header[1] (symbols.toml). AreaMap_Header's second byte
// kButtonMap0: now Gfx_UnpackScratch[0x1f000] (symbols.toml). the field's button map: word 0, the first legend's button
// kButtonMap6: now Gfx_UnpackScratch[0x1f00c] (symbols.toml). word 6, the second's
// kPartySet: now AreaW0b_PartySet (symbols.toml). & 0x7F: the party set (0xC shows the third legend)
constexpr U kAreaText = 0x803580;         // the area's text section
constexpr U kAreaTextOffset45 = 0x803584; // area 45's region label at +[this] & 0xFFFF (area 16's is 0x803588)
constexpr U kItemsHeld = 0x9040EC;        // a byte per item id: 0 not yet seen
// kItemNameKey: now AreaW0b_ItemNameKey (symbols.toml). the 16-byte name the hook copies for item 0x16
constexpr U kTextRecords = 0x904CE0;      // Text_Records: the hook fills its first four rows of 0x20
constexpr U kTextRecordsEnd = 0x904D60;
constexpr U kFlag3A79 = 0x903A79;         // the record +4 state: 9 releases the effect
// The place hook's names for the tail bytes (area 16's code, copied): the
// kind, the hook's state, its argument.
// kMsgMode: now AreaW0a_TailKind (symbols.toml).
// kMsgState: now AreaW0c_TailState (symbols.toml).
// kMsgArg: now AreaW0c_TailSub (symbols.toml).

}  // namespace at

// The unowned callees (above).
constexpr std::uint32_t kDrawWindow = bof3::addr::Area75_DrawWindow;
constexpr std::uint32_t kFreeObject = bof3::addr::Sprite_FindFree;
constexpr std::uint32_t kFlagsToggle = bof3::addr::Flags_Toggle;

}  // namespace area_w1b
