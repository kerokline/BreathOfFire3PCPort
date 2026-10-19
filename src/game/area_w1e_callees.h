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
// kChoice: now AreaW0c_ChoiceAnswer (symbols.toml). s8: the choice the message box committed
constexpr U kMessage = 0x7DEE48;          // u16: the message a choice handler opens, 0xFFFF none
// kTailKind: now AreaW0a_TailKind (symbols.toml). Field_ModeTailKinds' index (s8)
// kTailState: now AreaW0c_TailState (symbols.toml). s8: the armed tail kind's state
// kTailArg: now AreaW0c_TailSub (symbols.toml). the tail's argument byte
// kLeader: now ObjTrio (symbols.toml). ObjTrio + 0, the leader's record
constexpr U kMember1 = 0x802E8C;          // ObjTrio + 0x14C
constexpr U kMember2 = 0x802FD8;          // ObjTrio + 0x298
constexpr U kPartyStride = 0x14C;
// kLeaderX: now ObjTrio[0x34] (symbols.toml). the leader's +0x34 / +0x38, 16.16
// kLeaderZ: now ObjTrio[0x38] (symbols.toml).
// kLeaderEffect: now ObjTrio[0x89] (symbols.toml). the leader's +0x89: MoveScript_EffectState's index
constexpr U kPartyList = 0x904062;        // three bytes: the members' character ids, in order
constexpr U kScratch48 = 0x903848;        // u8 scratch cells 0x903848..0x90384B (the script's)
// kVar7: now MoveScript_Var7 (symbols.toml). MoveScript_Var7
// kVar8: now MoveScript_PartyRecords[0x65] (symbols.toml). the byte after it
// kObject0X: now Sprite_Objects[0x34] (symbols.toml). Sprite_Objects[0] +0x34
// kObject1X: now Sprite_Objects[0xd8] (symbols.toml). Sprite_Objects[1] +0x34
constexpr U kEffectStride = 0x80;

// --- area 65's tables (the world-map copy's, in its own data block
// 0x6043C8..0x604963 around its descriptor; read only by area 65's code) ------
// kA65PlateAnims: now Area65_PlateAnims (symbols.toml). (u16 place, u8 animation, u8) x 11, searched with NO bound
// kA65Cells: now Area65_Cells (symbols.toml). (u8 x, u8 z, u8, u8 id) x 182, searched with NO bound
constexpr U kA65CellsEnd = 0x6046CC;      // then area 65's choice table (1) and its descriptor 0x6046D0..0x604713
constexpr U kA65Descriptor = 0x6046D0;
// kA65PlaceMessages: now Area65_PlaceMessages (symbols.toml). 11 rows of 0x20: u16 place, fifteen u16 messages
constexpr U kA65PlaceMessagesEnd = 0x604874;
// kA65NameSets: now Area65_NameSets (symbols.toml). 2 records of (u8 id, u8 item x 5) - area 45's are 3 of (id, 4 items)
constexpr U kA65NameSetsEnd = 0x604880;
constexpr U kA65NameSetStride = 6;
// kA65PlateStates: now Area65_PlateStates (symbols.toml). 5
// kA65HudStates: now Area65_HudStates (symbols.toml). 2
// kA65FrameStates: now Area65_FrameStates (symbols.toml). 4
// kA65BoxStates: now Area65_BoxStates (symbols.toml). 4
// kA65Sprites: now Area65_Sprites (symbols.toml). (w, h, u, v) x 22, by index & 0xFF unchecked
// kA65Buttons: now Area65_Buttons (symbols.toml). (u16 mask, u8 sprite, u8) x 6; the second legend reads EIGHT
// kA65Record8States: now Area65_Record8States (symbols.toml). 3 (0x4253C0 another group's, Area65_Record8Place, Area65_Record8Move)
// kA65Directions: now Area65_Directions (symbols.toml). (s16 dx, s16 dz) x 4 by +8, unchecked
// kA65Record8Anims: now Area65_Record8Anims (symbols.toml). (u8 animation, u8 +0x2A) x 4 by +8, unchecked
// kA65Record4States: now Area65_Record4States (symbols.toml). 2: Area65_Record4MarkCell, Area45_Record4Tick (shared)
// kA65DriftUBase: now Area65_DriftUV (symbols.toml). u8 by +0xB - 2; v base +4, cell size +8
// kA65DriftVBase: now Area65_DriftUV[4] (symbols.toml).
// kA65DriftSize: now Area65_DriftUV[8] (symbols.toml).

// --- area 65's world-map cells (area 16's and 45's, docs/area_w0b.md) ---------
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
constexpr U kAreaTextOffset = 0x803584;   // the region label at +[this] & 0xFFFF (area 45's cell; area 16's is 0x803588)
constexpr U kItemsHeld = 0x9040EC;        // a byte per item id: 0 not yet seen
// kItemNameKey: now AreaW0b_ItemNameKey (symbols.toml). the 16-byte name the hook copies for item 0x16
constexpr U kTextRecords = 0x904CE0;      // Text_Records: area 65's hook fills its first FIVE rows of 0x20
constexpr U kTextRecordsEnd = 0x904D80;
constexpr U kFlag3A79 = 0x903A79;         // the record +4 state: 9 releases the effect
// The place hook's names for the tail bytes (area 16's code, copied): the
// kind, the hook's state, its argument.
// kMsgMode: now AreaW0a_TailKind (symbols.toml).
// kMsgState: now AreaW0c_TailState (symbols.toml).
// kMsgArg: now AreaW0c_TailSub (symbols.toml).

// --- area 67's tables (after its descriptor 0x607000's arrays) ----------------
constexpr U kA67Handlers = 0x606F20;      // 19: its +0x3C array (then its movement scripts)
constexpr U kA67Choices = 0x606FF4;       // 3: its +0x34 array
// kA67MessagesA: now Area67_MessagesA (symbols.toml). u16 by the s8 choice, unchecked (choice 0)
// kA67MessagesB: now Area67_MessagesB (symbols.toml). ... (choice 1)
// kA67MessagesC: now Area67_MessagesC (symbols.toml). ... (choice 2)
// Three byte tables by a member's character id (unchecked), in area 66's data
// block after its descriptor 0x604B70 (read only by area 67's handlers): the
// effect argument each character's effect spawns with.
constexpr U kA67SpawnAtArg = 0x604BB8;    // handlers 3..8 (kind 3)
// kA67SpawnArgB: now Area67_SpawnArgB (symbols.toml). handlers 9..11 (kind 1)
// kA67SpawnArgC: now Area67_SpawnArgC (symbols.toml). handler 16 (kind 4)

}  // namespace at
}  // namespace area_w1e
