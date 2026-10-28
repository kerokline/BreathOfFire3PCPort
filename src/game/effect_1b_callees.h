// The raw addresses effect_1b.cpp calls or reads that symbols.toml does not
// name, or that another group of round thirteen owns - each a load-bearing
// constant (CLAUDE.md rule 3). docs/effect_1b.md sections 2 and 8.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round thirteen's rebinding (docs/round-13-cleanup.md): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_1b::at {

// --- callees round thirteen's E1F owns (raw until it merges) ----------------------
constexpr std::uint32_t kDrawMode = bof3::addr::UiSprite_SetMode;     // (id, slot): a draw-mode primitive from the records 0x660394, committed
constexpr std::uint32_t kDrawSprite = bof3::addr::UiSprite_Draw;   // (id, slot, x, y) -> the primitive: a sprite from the records 0x660438
                                                  // at (x, y) (s16), committed

// --- callees round thirteen's E1A owns (raw until it merges), each read for the
// widths E1B's calls hand it (docs/effect_1b.md section 5) ---
constexpr std::uint32_t kE1aBarG4 = bof3::addr::EffectHud_TwoBars;     // (x, y, value, slot): x, y, value read as s16 words; G4 quads
constexpr std::uint32_t kE1aGaugeA = bof3::addr::EffectHud_Bar;    // (x, y, slot): G4 quads
constexpr std::uint32_t kE1aGaugeB = bof3::addr::EffectHud_Marker;    // (x, y, slot): G4 quads
constexpr std::uint32_t kE1aGaugeMark = bof3::addr::EffectHud_Sprite8; // (x, y, slot): G4 quads
constexpr std::uint32_t kE1aDigits = bof3::addr::EffectHud_DrawCount;    // (x, y, flag, value byte): sprites
constexpr std::uint32_t kE1aMarker = bof3::addr::EffectHud_DrawArrow;    // (x, y, flag, slot): x, y s16, flag a byte; a textured quad
constexpr std::uint32_t kE1aRange = bof3::addr::EffectHud_DrawMark;     // six words: a draw mode and a quad

// --- a callee round thirteen's E1G owns (raw until it merges) ---
constexpr std::uint32_t kE1gItemIcon = bof3::addr::Item_DrawIcon;  // (x, y, item byte, category, flag): nothing for item 0; else
                                                  // Item_IconKind and Menu_DrawIcon8

// --- data (the image's; read in place, never written) ---
// Kind 0xF's sub-kind dispatch (EffectKind0F_Child, by Sprite_Current +2) and
// each sub-kind's step table (by +3): the run of code pointers 0x653CA0..0x653DA8
// named in symbols.toml, each to its own length.
// kChildren: now EffectKind0F_Children (symbols.toml). 6: EffectKind0F_Children
// kChild0Steps: now EffectKind0F_Child0Steps (symbols.toml). 5
// kChild1Steps: now EffectKind0F_Child1Steps (symbols.toml). 2
// kChild2Steps: now EffectKind0F_Child2Steps (symbols.toml). 4
// kChild3Steps: now EffectKind0F_Child3Steps (symbols.toml). 2
// kChild4Steps: now EffectKind0F_Child4Steps (symbols.toml). 2
// kChild5Steps: now EffectKind0F_Child5Steps (symbols.toml). 2
constexpr unsigned kChildrenCount = 6, kChild0Count = 5, kChild1Count = 2, kChild2Count = 4, kChild3Count = 2,
                   kChild4Count = 2, kChild5Count = 2;
// kKind14States: now EffectKind14_States (symbols.toml). EffectKind14_States (named, round twelve; its name is a
constexpr unsigned kKind14Count = 3;                // pointer macro in symbols.gen.h, so the address is spelled here)
// The animation scripts the steps walk (byte records; the count is the bytes
// to the next table, a marker record inside it loops).
constexpr ::bof3::image::Va kChild0Script{0x653CCC};   // 11 records of 2: (animation, frames); animation 0xFF: go to frames
constexpr unsigned kChild0ScriptBytes = 22;
constexpr ::bof3::image::Va kChild1Script{0x653CEC};   // 4 records of 3: (animation, frames, mode); 0xFF: go to frames
constexpr unsigned kChild1ScriptBytes = 12;
constexpr ::bof3::image::Va kChild2Script{0x653D08};   // 12 records of 3: (animation, frames, step); animation 0: go to frames
constexpr unsigned kChild2ScriptBytes = 36;
constexpr ::bof3::image::Va kChild3Grid{0x653D34};     // 4 rows of 8 bytes, each row to an 0xFF
constexpr unsigned kChild3GridBytes = 32;
constexpr ::bof3::image::Va kChild4Script{0x653D5C};   // 18 records of 4: (animation | 0x80 no tick, frames, dx, dy)
constexpr ::bof3::image::Va kChild5Script{0x653DAC};   // 18 records of 4, the same shape
constexpr unsigned kMoveScriptBytes = 72;
// kTitleIds: now EffectKind0F_RuleTitleIds (symbols.toml). 6 u16: a message id by Sprite_Current word +0x3C
constexpr unsigned kTitleCount = 6;
// kLineIds: now EffectKind0F_RuleLineIds (symbols.toml). 0x36 u16: a message id per line, 0xFFFF none
constexpr unsigned kLineCount = 0x36;
constexpr ::bof3::image::Va kEdgeQuads{0x653E6C};      // 5 records of 10: u byte, v byte, w word, h byte, a CLUT word at +8
constexpr unsigned kEdgeQuadCount = 5;
// kItemsBLabel: now EffectKind0F_PanelTitles (symbols.toml). the strings the item panels print (image text, not copied here)
// kItemsALabel: now EffectKind0F_PanelTitles[4] (symbols.toml).
// kEquipLabel: now EffectKind0F_PanelTitles[8] (symbols.toml).
// kEquipLabel2: now EffectKind0F_PanelTitles[0xd] (symbols.toml).
// kPageFormat: now EffectKind0F_PageFormat (symbols.toml). a printf format of two numbers' shape, one used
// kCountLabel: now EffectKind0F_CountLabel (symbols.toml).
constexpr ::bof3::image::Va kCountFormat{0x653EC0};    // the item count's format
// kToggleText: now Effect1b_ToggleText (symbols.toml). 3 pointers to two-byte-glyph strings
constexpr ::bof3::image::Va kGridText{0x66A32C};       // 3 pointers to strings (index * 4, and * 8 on the lit cell)
// kStyleColours: now Gfx_ClutStripSource[0x114] (symbols.toml). the window styles' colour words, 0x40 bytes a style

// --- cells ---
// kStyle: now Config_WindowColour (symbols.toml). the window style byte (Menu_DrawBox's colour)
// kLeader3: now ObjTrio[0].state3 (symbols.toml). ObjTrio (the leader's record) +3
// kLeader4: now ObjTrio[0].state4 (symbols.toml). ObjTrio +4
// kRecord6State: now Effect_Objects[6].state (symbols.toml). Effect_Objects record 6: +1
// kRecord6Hold: now Effect_Objects_addr + 0x308 (symbols.toml). record 6: +8
// kRecord3Y: now Effect_Objects_addr + 0x1b0 (symbols.toml). record 3: dword +0x30 (the glyph row's y)
constexpr ::bof3::image::Va kItemIds{0x9042D4};        // the accessories' inventory ids, 0x80 bytes (docs/menu-windows.md)
constexpr ::bof3::image::Va kItemCounts{0x9044D4};     // their counts, 0x80 bytes
constexpr ::bof3::image::Va kEquipA{0x904130};         // two accessory ids the equipped panel prints
// kEquipB: now CharStats_ExtraItem (symbols.toml).
constexpr ::bof3::image::Va kPrint{0x904BA0};          // the print buffer
constexpr ::bof3::image::Va kPools{0x803580};          // MessagePools: u16 offsets from its own start
constexpr ::bof3::image::Va kAccessoryCategory{0x658462};   // NameTable_Accessories + 0x12, 24 bytes a record
constexpr unsigned kAccessoryStride = 24, kAccessoryCount = 52;
constexpr unsigned kEffectStride = 0x80, kEffectCount = 20;
constexpr unsigned kSpriteStride = 0xA4, kSpriteCount = 30;

}  // namespace effect_1b::at
