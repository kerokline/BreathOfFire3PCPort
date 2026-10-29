// Round thirteen group E1A: the addresses its functions read and the callees
// not yet ours, called raw through the scenario harness (SH_AT) until their
// owners merge. docs/effect_1a.md section 5 lists who owns each.
#pragma once

#include <cstdint>

#include "game/rdata_consts.h"
#include "bof3/symbols.gen.h"  // round thirteen's rebinding (docs/round-13-cleanup.md): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_1a::at {

// --- callees nobody of this group owns ---------------------------------------------
// E1F's (round thirteen): a draw mode of 0xC from 0x660394's records (id byte,
// slot); a sprite primitive of 0x1C from 0x660438's records (id byte, slot,
// s16 x, s16 y), eax the primitive; the depth pair +0x40 / +0x44 of
// Sprite_Current from 0x4650000 / (+0x60 - 2 * s16 +0x3E) after a
// Sprite_UpdateScreen with +0x3C zeroed around it.
constexpr ::bof3::image::CodeVa kDrawModeRecord{bof3::addr::UiSprite_SetMode};
constexpr ::bof3::image::CodeVa kDrawSpriteRecord{bof3::addr::UiSprite_Draw};
constexpr ::bof3::image::CodeVa kDepthPair{bof3::addr::Sprite_UpdateScreenScaled};
// E1B's (round thirteen): a window frame and fill (x, y, w, h, colour), each
// read as 16 bits (0x469790 / 0x469960 `and 0xFFFF`), the colour's byte; the
// three option boxes and their labels (x, y, bits byte); a message line (pen
// byte, text, width byte, x word); the member rows (x, y); the two item
// lists (x, y); the panel's title and count.
constexpr ::bof3::image::CodeVa kWindowBox{bof3::addr::Panel_DrawWindow};
constexpr ::bof3::image::CodeVa kOptionBoxes{bof3::addr::EffectKind0F_DrawToggles};
constexpr ::bof3::image::CodeVa kMessageLine{bof3::addr::EffectKind0F_DrawGlyph};
constexpr ::bof3::image::CodeVa kMemberRows{bof3::addr::EffectKind0F_DrawEquipped};
constexpr ::bof3::image::CodeVa kItemListA{bof3::addr::EffectKind0F_DrawItemsB};
constexpr ::bof3::image::CodeVa kItemListB{bof3::addr::EffectKind0F_DrawItemsA};
constexpr ::bof3::image::CodeVa kPanelTitle{bof3::addr::EffectKind0F_DrawCountHeader};
// Ours since round fourteen (R4F), by address until the round's rebinding: a
// sprite primitive by the word it is handed.
constexpr ::bof3::image::CodeVa kKind07Sprite{bof3::addr::EffectKind07_DrawSprite};
// Capcom's, unnamed (kEffectStd's rows): a string's characters counted (a byte
// above 0x7F takes two); the library layer's primitive from a RECT (prim, rect),
// 12 bytes written.
constexpr ::bof3::image::CodeVa kStringCount{bof3::addr::EffectKind0F_CharCount};
constexpr ::bof3::image::CodeVa kPrimFromRect{bof3::addr::Gpu_SetTexWindow};

// --- the image's tables, read in place (never copied) ------------------------------
// kRecordSlot4: now WorldMap_Records[1] (symbols.toml). WorldMap_Records' +4 (kind 0xE's handler)
// kRecordSlot8: now WorldMap_Records[2] (symbols.toml). WorldMap_Records' +8 (kind 0x16's handler)
constexpr std::uint32_t kRecordSize = 0x1C;
constexpr unsigned kRecordCount = 12;               // the eleven records and index 11, "none" (WorldMap_RecordIndex)
constexpr ::bof3::image::Va kMarkerShades{0x653AF8};   // four bytes: EffectHud_Marker's u by (Frame_Counter >> 3) & 3
constexpr ::bof3::image::Va kKind05Rise{0x653B24};     // four records of 8: the rise +0x14 and its step +0x20 by +6
constexpr ::bof3::image::Va kKind05Bounce{0x653B44};   // four records of 8: the height offset and the step on reaching it
constexpr unsigned kKind05Rows = 4;
constexpr ::bof3::image::Va kKind0DSounds{0x653B8C};   // six words: kind 0xD's sound by +6 (0xFFFF none)
constexpr unsigned kKind0DSoundCount = 6;
constexpr ::bof3::image::Va kKind0FTexts{0x653B98};    // thirteen records of 8: a string, a line byte +4, a pause byte +5
constexpr unsigned kKind0FTextCount = 13;
constexpr ::bof3::image::Va kKind0FLabels{0x653C00};   // bytes by a text's line byte: the label's pen
constexpr ::bof3::image::Va kKind0FRows{0x653C04};     // nine rows of four bytes by +6: three text indexes and a spawn byte
constexpr unsigned kKind0FRowBytes = 0x24;
constexpr ::bof3::image::Va kLabelTexts{0x66A2FC};     // dwords by a text's line byte: the label strings
constexpr rdata::Const kQuadHalf{0x5C41C8};       // 4.0f
constexpr rdata::Const kQuadEight{0x5C41CC};      // 8.0f
constexpr rdata::Const kQuadSixteen{0x5C41D0};    // 16.0f
constexpr rdata::Const kQuadThirtyTwo{0x5C41D4};  // 32.0f
// kCountFormat: now Boss26Fx_CountFormat (symbols.toml). Boss26Fx_CountFormat
constexpr std::uint32_t kAccessoryNames = bof3::addr::NameTable_Accessories; // NameTable_Accessories, 0x18 bytes a name

// The effect kinds' state tables (symbols.toml [[data]], this group's names).
// kKind01States: now EffectKind01_States (symbols.toml).
// kKind07States: now EffectKind07_States (symbols.toml).
// kKind08States: now EffectKind08_States (symbols.toml).
// kKind09States: now EffectKind09_States (symbols.toml).
// kKind0BStates: now EffectKind0B_States (symbols.toml).
// kKind02States: now EffectKind02_States (symbols.toml).
// kKind03States: now EffectKind03_States (symbols.toml).
// kKind05States: now EffectKind05_States (symbols.toml).
// kKind0AStates: now EffectKind0A_States (symbols.toml).
// kKind0CStates: now EffectKind0C_States (symbols.toml).
// kKind0DStates: now EffectKind0D_States (symbols.toml).
// kKind0FStates: now EffectKind0F_States (symbols.toml).
// kKind1AStates: now EffectKind1A_States (symbols.toml).

// --- cells ----------------------------------------------------------------------------
constexpr ::bof3::image::Va kRecord0{0x7E11E0};        // Effect_Objects record 0 (the kinds' HUD reads it)
constexpr ::bof3::image::Va kRecord2{0x7E12E0};        // record 2 (+0xC scales kind 5's height)
constexpr ::bof3::image::Va kRecord3{0x7E1360};        // record 3 (kind 0xF runs only there, or while its +1 is above 3;
                                                    // its +0x30 is the window's y the other kind-0xF records draw at)
// kRecord5: now Effect_Objects[5] (symbols.toml). record 5 (+0x10's sign picks kind 5's follow animation)
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
// kExtras: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra, four of 0xA4
constexpr unsigned kExtraCount = 4;
// kObjects: now Sprite_Objects (symbols.toml). Sprite_Objects, thirty of 0xA4
constexpr unsigned kObjectCount = 30;
constexpr std::uint32_t kObjectStride = 0xA4;
// kLeader: now ObjTrio[0] (symbols.toml). ObjTrio 0
constexpr ::bof3::image::Va kTextBuffer{0x904BA0};     // the text scratch sprintf writes
constexpr ::bof3::image::Va kCounterByte{0x903848};
constexpr ::bof3::image::Va kAccessoryA{0x904130};     // the two accessory bytes kind 3 names (the first read as a dword)
// kAccessoryB: now CharStats_ExtraItem (symbols.toml).
constexpr ::bof3::image::Va kKindCounts{0x9040EC};     // 32 bytes kind 0x1A walks by +0x3E
// kPanelObject: now Effect1a_PanelObject (symbols.toml). a Sprite_Objects index byte (kinds 5 and 0xC)
constexpr ::bof3::image::Va kPanelScript{0x939A20};    // a pointer: kind 3's and 5's parameters
constexpr ::bof3::image::Va kPanelSet{0x939A24};       // a pointer: kind 3's and 5's second block

}  // namespace effect_1a::at
