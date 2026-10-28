// Internal to area_w2b.cpp and area_w2b_fuzz.cpp: the cells and tables world
// 2's areas 85..88 touch that symbols.toml has no name for (the tables it
// names are listed too, by address, for the fuzz's regions), the world-map
// copies' table sets, and the callees nobody owns this wave, by raw address.
// docs/area_w2b.md.
//
// Raw-address callees (the round's rebinding pass names them):
//   0x454A80  (object): releases every Field_Slots record whose +0xC is the
//             object (0x454A50 each); engine code nobody owns (read
//             2026-09-28, 0x454A80..0x454AAA).
//   0x455290  (object, script): the first free Field_Slots record (+0 bit 0
//             clear) of eight gets +0 = 1, +0xC the object, +4 the script,
//             +2 = 0xFF, +3 = object +0x27; al its index, 0xFF none; engine
//             code nobody owns (read 2026-09-28, 0x455290..0x4552F5).
//   0x57C160  (bits, index): toggles bit (index & 7) of bits[(index & 0xFF)
//             >> 3] (docs/area_w1b.md); SX2's this wave.
//   0x469FE0  (v): Effect_FindFree; a free slot gets +0 = 1, kind +5 = 4, +9
//             = v, and story flag 0x1C is set (docs/area_w1c.md); SX2's this
//             wave.
#pragma once

#include <cstdint>

#include "common/image_base.h"

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace area_w2b {
namespace at {

using U = std::uint32_t;

// --- the field frame's cells ------------------------------------------------
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the flag bank every Flags_* call here is handed
// kTailKind: now AreaW0a_TailKind (symbols.toml). Field_ModeTailKinds' index (s8)
// kTailState: now AreaW0c_TailState (symbols.toml). s8: the armed tail kind's (or the place hook's) state
// kTailArg: now AreaW0c_TailSub (symbols.toml).
// kLeader: now ObjTrio (symbols.toml). ObjTrio + 0, the leader's record
// kLeaderDir: now ObjTrio[0].direction (symbols.toml). the leader's +8, its facing
// kLeaderX: now ObjTrio[0].x (symbols.toml). the leader's +0x34 / +0x38, 16.16
// kLeaderZ: now ObjTrio[0].z (symbols.toml).
constexpr U kPartyStride = 0x14C;
constexpr U kEffectStride = 0x80;
// kScriptVar7: now MoveScript_Var7 (symbols.toml). MoveScript_Var7 (s8): the running scene's run
// kScriptStep: now MoveScript_PartyRecords[0x65] (symbols.toml). the run's step (Scena* step machines)
// kPrevArea: now AreaW2c_LastArea (symbols.toml). u16: the area the party came from (Area_Enter, docs/mode-flow.md)
constexpr ::bof3::image::Va kSlots{0x9035C0};            // Field_Slots: 8 records of 0x10
// kClutRow6: now Gfx_ClutStripSource[0x600] (symbols.toml). Gfx_ClutStripSource + 0xC00: CLUT row 6 as loaded (0x100 words)
// kClutRow6Live: now Gfx_ClutStrip[0x600] (symbols.toml). Gfx_ClutStrip + 0xC00: the row the game uploads

// --- area 85's ------------------------------------------------------------------
constexpr ::bof3::image::Va kA85Handlers{0x61177C};      // 10: its descriptor 0x6117A8's +0x3C (entry 7 shared with area 198)
constexpr ::bof3::image::Va kA85SlotScript{0x611814};    // the movement script handler 6 hands 0x455290

// --- area 86's (its descriptor 0x611C38) ----------------------------------------
// kA86Switches: now Area86_Switches (symbols.toml). (x, z, facing, flag) x 3: the cell hook's
constexpr ::bof3::image::Va kA86SwitchesEnd{0x611C88};
// kA86Rects: now Area86_Rects (symbols.toml). (x0, z0, x1, z1) x 3 by the switch: the cells that must be clear
constexpr ::bof3::image::Va kA86RectsEnd{0x611C94};

// --- the world maps' cells (area 16's, docs/area_w0b.md) ----------------------
// kLeaderSteps: now ObjTrio[0].step_frames (symbols.toml). u8 step count
// kLeaderDirX: now ObjTrio[0].step_x (symbols.toml). dword direction
// kLeaderDirZ: now ObjTrio[0].step_z (symbols.toml).
// kLeaderCellWordX: now ObjTrio[0].x_whole (symbols.toml). the high word of +0x34
// kLeaderCellWordZ: now ObjTrio[0].z_whole (symbols.toml). the high word of +0x38
// kPlace: now AreaW0b_Place (symbols.toml). u16: the place the party stands on (world map)
constexpr ::bof3::image::Va kMapMode{0x9045FA};          // the world map's mode byte
constexpr ::bof3::image::Va kLeaderCellX{0x905E66};      // the high words of Field_Kind2X / Z
constexpr ::bof3::image::Va kLeaderCellZ{0x905E62};
// kMapHeight: now AreaMap_Header[1] (symbols.toml). AreaMap_Header's second byte
constexpr ::bof3::image::Va kButtonMap0{0x903580};       // the field's button map: word 0, the first legend's button
// kButtonMap6: now Gfx_UnpackScratch[0x1f00c] (symbols.toml). word 6, the second's
// kPartySet: now AreaW0b_PartySet (symbols.toml). & 0x7F: the party set (0xC shows the third legend)
constexpr ::bof3::image::Va kAreaText{0x803580};         // the area's text section; areas 87 and 88 read their label offset from its first dword
constexpr ::bof3::image::Va kItemsHeld{0x9040EC};        // a byte per item id: 0 not yet seen
// kItemNameKey: now AreaW0b_ItemNameKey (symbols.toml). the 16-byte name the hook copies for item 0x16
constexpr ::bof3::image::Va kTextRecords{0x904CE0};      // Text_Records: the place hook fills rows of 0x20 from here
constexpr ::bof3::image::Va kFlag3A79{0x903A79};         // the record +4 state: 9 releases the effect
// kMsgMode: now AreaW0a_TailKind (symbols.toml).
// kMsgState: now AreaW0c_TailState (symbols.toml).
// kMsgArg: now AreaW0c_TailSub (symbols.toml).

// --- a world-map copy's tables (area 45's layout, docs/area_w1b.md section 5;
// each area's in its data block after the previous area's descriptor) ----------
struct WorldMapTables {
    const char* name;                 // "Area87": the messages' prefix
    U plate_anims, plate_anims_end;   // (u16 place, u8 animation, u8), searched with NO bound
    U cells, cells_end;               // (u8 x, u8 z, u8, u8 id): the shipped records, searched with NO bound
    U place_messages, place_messages_end;   // rows of 0x20: u16 place, fifteen u16 messages
    U name_sets, name_sets_end;       // (u8 id, u8 item x set_items)
    unsigned set_stride;              // 5 (area 87), 12 (area 88)
    U text_rows_end;                  // the place hook's last Text_Records row (exclusive)
    U plate_states, hud_states, frame_states, box_states;
    U sprites, buttons;               // (w, h, u, v) x 22; (u16 mask, u8 sprite, u8) x 6, the second legend reads eight
    U record8_states, directions, record8_anims, record4_states;
    U drift_u, drift_v, drift_size;   // u8 by +0xB - 2 each
    U label_offset;                   // the dword whose low word is the region label's offset in kAreaText
    unsigned plate_bank;              // PlateStart's Sprite_SetAnimationBank
    // the copy's own functions another of them calls directly
    U fn_frame_step, fn_frame_hold, fn_box_step, fn_draw_frame, fn_draw_sprite, fn_draw_hud;
};

// Area 87: WorldMap_Records record 4 (0x653980), WorldMap_FieldHooks entry 4;
// its descriptor 0x612488 (only a choice table, +0x34 -> 0x41D270, and the
// init 0x40FC40, which area 65 shares).
constexpr WorldMapTables kWm87 = {
    "Area87",
    ::bof3::Img(0x611C98), ::bof3::Img(0x611CC0),   // 10 plate animations
    ::bof3::Img(0x611CC0), ::bof3::Img(0x611CCC),   // 3 cell records (then a zero record, then the descriptor's data)
    ::bof3::Img(0x6124CC), ::bof3::Img(0x61260C),   // 10 place rows
    ::bof3::Img(0x61260C), ::bof3::Img(0x61261B), 5, ::bof3::Img(0x904D60),   // 3 name sets of (id, 4 items); 4 text rows
    ::bof3::Img(0x61261C), ::bof3::Img(0x612630), ::bof3::Img(0x612638), ::bof3::Img(0x612648),
    ::bof3::Img(0x612658), ::bof3::Img(0x6126B0),
    ::bof3::Img(0x6126C8), ::bof3::Img(0x6126D4), ::bof3::Img(0x6126E4), ::bof3::Img(0x6126EC),
    ::bof3::Img(0x6126F4), ::bof3::Img(0x6126F8), ::bof3::Img(0x6126FC),
    ::bof3::Img(0x803580), 0x156,
    bof3::addr::Area87_FrameStep, bof3::addr::Area87_FrameHold, bof3::addr::Area87_BoxStep,
    bof3::addr::Area87_DrawFrame, bof3::addr::Area87_DrawSprite, bof3::addr::Area87_DrawHud,
};

// Area 88: record 5 (0x65399C), field hook entry 5; its descriptor 0x612E50
// (only a choice table, +0x34 -> 0x41D270).
constexpr WorldMapTables kWm88 = {
    "Area88",
    ::bof3::Img(0x612700), ::bof3::Img(0x61271C),   // 7 plate animations
    ::bof3::Img(0x61271C), ::bof3::Img(0x612728),   // 3 cell records (then the descriptor's data)
    ::bof3::Img(0x612E94), ::bof3::Img(0x612F74),   // 7 place rows
    ::bof3::Img(0x612F74), ::bof3::Img(0x612F98), 12, ::bof3::Img(0x904E40),   // 3 name sets of (id, 11 items); 11 text rows
    ::bof3::Img(0x612F98), ::bof3::Img(0x612FAC), ::bof3::Img(0x612FB4), ::bof3::Img(0x612FC4),
    ::bof3::Img(0x612FD4), ::bof3::Img(0x61302C),
    ::bof3::Img(0x613044), ::bof3::Img(0x613050), ::bof3::Img(0x613060), ::bof3::Img(0x613068),
    ::bof3::Img(0x613070), ::bof3::Img(0x613074), ::bof3::Img(0x613078),
    ::bof3::Img(0x803580), 0x157,
    bof3::addr::Area88_FrameStep, bof3::addr::Area88_FrameHold, bof3::addr::Area88_BoxStep,
    bof3::addr::Area88_DrawFrame, bof3::addr::Area88_DrawSprite, bof3::addr::Area88_DrawHud,
};

}  // namespace at

// The unowned callees (above).
constexpr std::uint32_t kSlotsReleaseFor = bof3::addr::Field_SlotsReleaseOwner;   // 0x454A80 (BE6, 2026-09-29)
constexpr std::uint32_t kSlotStart = bof3::addr::Field_SlotStart;                  // 0x455290 (BE6, 2026-09-29)
constexpr std::uint32_t kFlagsToggle = bof3::addr::Flags_Toggle;
constexpr std::uint32_t kSpawnKind4 = bof3::addr::Effect_HoldFlag1C;

}  // namespace area_w2b
