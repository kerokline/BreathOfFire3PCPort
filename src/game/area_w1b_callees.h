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
constexpr U kChoice = 0x7DEE67;           // s8: the choice the message box committed
constexpr U kMessage = 0x7DEE48;          // u16: the message a choice handler opens, 0xFFFF none
constexpr U kStoryFlags = 0x904030;       // the flag bank every Flags_* call here is handed
constexpr U kFlagsCC = 0x9040CC;          // a second bank: area 42 tests its bits 0x5C..0x5E
constexpr U kTailKind = 0x9039F3;         // Field_ModeTailKinds' index (s8)
constexpr U kTailState = 0x9039F4;        // s8: the armed tail kind's state
constexpr U kTailArg = 0x9039F5;          // area 42: a frame count, then an effect slot
constexpr U kTailTimer = 0x9039F6;        // u16: area 42's countdown in frames (0x384 from its handler 0)
constexpr U kLeader = 0x802D40;           // ObjTrio + 0, the leader's record
constexpr U kLeaderX = 0x802D74;          // the leader's +0x34 / +0x38, 16.16
constexpr U kLeaderZ = 0x802D78;
constexpr U kLeaderEffect = 0x802DC9;     // the leader's +0x89: MoveScript_EffectState's index
constexpr U kLeaderDir = 0x802D48;        // the leader's +8, its facing
constexpr U kFoundSlot = 0x903850;        // a found slot (word or byte, as each writer stores it)
constexpr U kCameraAngle1 = 0x929ECA;     // Camera_Angles[1]
constexpr U kCameraAngle2 = 0x929ECC;     // Camera_Angles[2] (Cond_AngleFB's low half)
constexpr U kTextBuffer = 0x904BA0;       // area 42's time string, by Crt_sprintf
constexpr U kRank = 0x675A00;             // u8: area 42's result, 0..2 (its init and its check write it)
constexpr U kCells44 = 0x90384B;          // u8: area 44's two gates, bits 0..1 and 4..5
constexpr U kCount44 = 0x90384A;          // u8: area 44's init zeroes it with kCells44
constexpr U kDrop46 = 0x903849;           // u8: area 46's party-drop state (1 armed, 2 / 0 done)
constexpr U kExtra0 = 0x802000;           // Sprite_ObjectsExtra[0]
constexpr U kExtra1 = 0x8020A4;           // Sprite_ObjectsExtra[1]
constexpr U kExtraStride = 0xA4;
constexpr U kPartyStride = 0x14C;
constexpr U kEffectStride = 0x80;

// --- area 42's tables (after its descriptor 0x5F6290's handler array) ------
constexpr U kA42Handlers = 0x5F6274;      // 7: its +0x3C array; +0x34 is its tail (entry 6)
constexpr U kA42Messages = 0x5F62D4;      // u16 by the s8 choice, unchecked
constexpr U kA42Placements = 0x5F62E0;    // 3 records of 13 bytes, EventOp_9x's operand, by kRank
constexpr U kA42Format = 0x5F6308;        // the time's two-number format (read-only)

// --- area 43's ----------------------------------------------------------------
constexpr U kA43Handlers = 0x5F6A18;      // 3
constexpr U kA43States = 0x5F6A6C;        // 2: its handler 0's state table, by Sprite_Current[4]

// --- area 44's ----------------------------------------------------------------
constexpr U kA44Gate0 = 0x5F7074;         // (u8 x, u8 z) x 4 by kCells44 & 3: extra object 0's cell
constexpr U kA44Gate1 = 0x5F707C;         // (u8 x, u8 z) x 4 by (kCells44 >> 4) & 3: extra object 1's
constexpr U kA44Switches = 0x5F7082;      // (x, z, facing, flag, tail kind) x 4: the cell hook's
constexpr U kA44SwitchesEnd = 0x5F7096;

// --- area 45's (the world-map copy's, in its data block after area 46's
// descriptor; tools/area_rows.py moves them to area 45) ------------------------
constexpr U kA45PlateAnims = 0x5F7098;    // (u16 place, u8 animation, u8) x 10, searched with NO bound
constexpr U kA45Cells = 0x5F70C0;         // (u8 x, u8 z, u8, u8 id) x 265, searched with NO bound
constexpr U kA45CellsEnd = 0x5F74E4;
constexpr U kA45PlaceMessages = 0x5F74E4; // 11 rows of 0x20: u16 place, fifteen u16 messages
constexpr U kA45PlaceMessagesEnd = 0x5F7644;
constexpr U kA45NameSets = 0x5F7644;      // 3 records of (u8 id, u8 item x 4)
constexpr U kA45NameSetsEnd = 0x5F7653;
constexpr U kA45PlateStates = 0x5F7654;   // 5
constexpr U kA45HudStates = 0x5F7668;     // 2
constexpr U kA45FrameStates = 0x5F7670;   // 4
constexpr U kA45BoxStates = 0x5F7680;     // 4
constexpr U kA45Sprites = 0x5F7690;       // (w, h, u, v) x 22, by index & 0xFF unchecked
constexpr U kA45Buttons = 0x5F76E8;       // (u16 mask, u8 sprite, u8) x 6; the second legend reads EIGHT
constexpr U kA45Record8States = 0x5F7700; // 3 (0x4253C0 and 0x40C490 other areas' shared ones)
constexpr U kA45Directions = 0x5F770C;    // (s16 dx, s16 dz) x 4 by +8, unchecked
constexpr U kA45Record8Anims = 0x5F771C;  // (u8 animation, u8 +0x2A) x 4 by +8, unchecked
constexpr U kA45Record4States = 0x5F7724; // 2: Area45_Record4MarkCell, Area45_Record4Tick (shared)
constexpr U kA45DriftUBase = 0x5F772C;    // u8 by +0xB - 2; v base +4, cell size +8
constexpr U kA45DriftVBase = 0x5F7730;
constexpr U kA45DriftSize = 0x5F7734;

// --- area 45's world-map cells (area 16's, docs/area_w0b.md) -------------------
constexpr U kLeaderSteps = 0x802D49;      // u8 step count
constexpr U kLeaderDirX = 0x802D4C;       // dword direction
constexpr U kLeaderDirZ = 0x802D50;
constexpr U kLeaderCellWordX = 0x802D76;  // the high word of +0x34
constexpr U kLeaderCellWordZ = 0x802D7A;  // the high word of +0x38
constexpr U kPlace = 0x937F82;            // u16: the place the party stands on (world map)
constexpr U kMapMode = 0x9045FA;          // the world map's mode byte
constexpr U kLeaderCellX = 0x905E66;      // the high words of Field_Kind2X / Z
constexpr U kLeaderCellZ = 0x905E62;
constexpr U kMapHeight = 0x8CB581;        // AreaMap_Header's second byte
constexpr U kButtonMap0 = 0x903580;       // the field's button map: word 0, the first legend's button
constexpr U kButtonMap6 = 0x90358C;       // word 6, the second's
constexpr U kPartySet = 0x90412C;         // & 0x7F: the party set (0xC shows the third legend)
constexpr U kAreaText = 0x803580;         // the area's text section
constexpr U kAreaTextOffset45 = 0x803584; // area 45's region label at +[this] & 0xFFFF (area 16's is 0x803588)
constexpr U kItemsHeld = 0x9040EC;        // a byte per item id: 0 not yet seen
constexpr U kItemNameKey = 0x669CD8;      // the 16-byte name the hook copies for item 0x16
constexpr U kTextRecords = 0x904CE0;      // Text_Records: the hook fills its first four rows of 0x20
constexpr U kTextRecordsEnd = 0x904D60;
constexpr U kFlag3A79 = 0x903A79;         // the record +4 state: 9 releases the effect
// The place hook's names for the tail bytes (area 16's code, copied): the
// kind, the hook's state, its argument.
constexpr U kMsgMode = kTailKind, kMsgState = kTailState, kMsgArg = kTailArg;

}  // namespace at

// The unowned callees (above).
constexpr std::uint32_t kDrawWindow = bof3::addr::Area75_DrawWindow;
constexpr std::uint32_t kFreeObject = bof3::addr::Sprite_FindFree;
constexpr std::uint32_t kFlagsToggle = bof3::addr::Flags_Toggle;

}  // namespace area_w1b
