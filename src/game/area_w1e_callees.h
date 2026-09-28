// Internal to area_w1e.cpp and area_w1e_fuzz.cpp: the cells and tables world
// 1's areas 65 and 67 touch that symbols.toml has no name for (the tables it
// names are listed too, by address, for the fuzz's regions). docs/area_w1e.md.
//
// Raw-address callees: none. Every callee of the 49 is named - ours, or
// Capcom's with a signature (Effect_Spawn, Effect_SpawnAt).
#pragma once

#include <cstdint>

namespace area_w1e {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells ------------------------------------------------
constexpr U kChoice = 0x7DEE67;           // s8: the choice the message box committed
constexpr U kMessage = 0x7DEE48;          // u16: the message a choice handler opens, 0xFFFF none
constexpr U kTailKind = 0x9039F3;         // Field_ModeTailKinds' index (s8)
constexpr U kTailState = 0x9039F4;        // s8: the armed tail kind's state
constexpr U kTailArg = 0x9039F5;          // the tail's argument byte
constexpr U kLeader = 0x802D40;           // ObjTrio + 0, the leader's record
constexpr U kMember1 = 0x802E8C;          // ObjTrio + 0x14C
constexpr U kMember2 = 0x802FD8;          // ObjTrio + 0x298
constexpr U kPartyStride = 0x14C;
constexpr U kLeaderX = 0x802D74;          // the leader's +0x34 / +0x38, 16.16
constexpr U kLeaderZ = 0x802D78;
constexpr U kLeaderEffect = 0x802DC9;     // the leader's +0x89: MoveScript_EffectState's index
constexpr U kPartyList = 0x904062;        // three bytes: the members' character ids, in order
constexpr U kScratch48 = 0x903848;        // u8 scratch cells 0x903848..0x90384B (the script's)
constexpr U kVar7 = 0x8034E4;             // MoveScript_Var7
constexpr U kVar8 = 0x8034E5;             // the byte after it
constexpr U kObject0X = 0x7DEEB4;         // Sprite_Objects[0] +0x34
constexpr U kObject1X = 0x7DEF58;         // Sprite_Objects[1] +0x34
constexpr U kEffectStride = 0x80;

// --- area 65's tables (the world-map copy's, in its data block after area
// 66's descriptor 0x604B70... read only by area 65's code) ---------------------
constexpr U kA65PlateAnims = 0x6043C8;    // (u16 place, u8 animation, u8) x 11, searched with NO bound
constexpr U kA65Cells = 0x6043F4;         // (u8 x, u8 z, u8, u8 id) x 182, searched with NO bound
constexpr U kA65CellsEnd = 0x6046CC;      // then area 65's choice table (1) and its descriptor 0x6046D0..0x604713
constexpr U kA65Descriptor = 0x6046D0;
constexpr U kA65PlaceMessages = 0x604714; // 11 rows of 0x20: u16 place, fifteen u16 messages
constexpr U kA65PlaceMessagesEnd = 0x604874;
constexpr U kA65NameSets = 0x604874;      // 2 records of (u8 id, u8 item x 5) - area 45's are 3 of (id, 4 items)
constexpr U kA65NameSetsEnd = 0x604880;
constexpr U kA65NameSetStride = 6;
constexpr U kA65PlateStates = 0x604880;   // 5
constexpr U kA65HudStates = 0x604894;     // 2
constexpr U kA65FrameStates = 0x60489C;   // 4
constexpr U kA65BoxStates = 0x6048AC;     // 4
constexpr U kA65Sprites = 0x6048BC;       // (w, h, u, v) x 22, by index & 0xFF unchecked
constexpr U kA65Buttons = 0x604914;       // (u16 mask, u8 sprite, u8) x 6; the second legend reads EIGHT
constexpr U kA65Record8States = 0x60492C; // 3 (0x4253C0 another group's, Area65_Record8Place, Area65_Record8Move)
constexpr U kA65Directions = 0x604938;    // (s16 dx, s16 dz) x 4 by +8, unchecked
constexpr U kA65Record8Anims = 0x604948;  // (u8 animation, u8 +0x2A) x 4 by +8, unchecked
constexpr U kA65Record4States = 0x604950; // 2: Area65_Record4MarkCell, Area45_Record4Tick (shared)
constexpr U kA65DriftUBase = 0x604958;    // u8 by +0xB - 2; v base +4, cell size +8
constexpr U kA65DriftVBase = 0x60495C;
constexpr U kA65DriftSize = 0x604960;

// --- area 65's world-map cells (area 16's and 45's, docs/area_w0b.md) ---------
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
constexpr U kAreaTextOffset = 0x803584;   // the region label at +[this] & 0xFFFF (area 45's cell; area 16's is 0x803588)
constexpr U kItemsHeld = 0x9040EC;        // a byte per item id: 0 not yet seen
constexpr U kItemNameKey = 0x669CD8;      // the 16-byte name the hook copies for item 0x16
constexpr U kTextRecords = 0x904CE0;      // Text_Records: area 65's hook fills its first FIVE rows of 0x20
constexpr U kTextRecordsEnd = 0x904D80;
constexpr U kFlag3A79 = 0x903A79;         // the record +4 state: 9 releases the effect
// The place hook's names for the tail bytes (area 16's code, copied): the
// kind, the hook's state, its argument.
constexpr U kMsgMode = kTailKind, kMsgState = kTailState, kMsgArg = kTailArg;

// --- area 67's tables (after its descriptor 0x607000's arrays) ----------------
constexpr U kA67Handlers = 0x606F20;      // 19: its +0x3C array (then its movement scripts)
constexpr U kA67Choices = 0x606FF4;       // 3: its +0x34 array
constexpr U kA67MessagesA = 0x607044;     // u16 by the s8 choice, unchecked (choice 0)
constexpr U kA67MessagesB = 0x607050;     // ... (choice 1)
constexpr U kA67MessagesC = 0x60705C;     // ... (choice 2)
// Three byte tables by a member's character id (unchecked), in area 66's data
// block after its descriptor 0x604B70 (read only by area 67's handlers): the
// effect argument each character's effect spawns with.
constexpr U kA67SpawnAtArg = 0x604BB8;    // handlers 3..8 (kind 3)
constexpr U kA67SpawnArgB = 0x604BC0;     // handlers 9..11 (kind 1)
constexpr U kA67SpawnArgC = 0x604BC8;     // handler 16 (kind 4)

}  // namespace at
}  // namespace area_w1e
