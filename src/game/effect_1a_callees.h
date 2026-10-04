// Round thirteen group E1A: the addresses its functions read and the callees
// not yet ours, called raw through the scenario harness (SH_AT) until their
// owners merge. docs/effect_1a.md section 5 lists who owns each.
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round thirteen's rebinding (docs/round-13-cleanup.md): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_1a::at {

// --- callees nobody of this group owns ---------------------------------------------
// E1F's (round thirteen): a draw mode of 0xC from 0x660394's records (id byte,
// slot); a sprite primitive of 0x1C from 0x660438's records (id byte, slot,
// s16 x, s16 y), eax the primitive; the depth pair +0x40 / +0x44 of
// Sprite_Current from 0x4650000 / (+0x60 - 2 * s16 +0x3E) after a
// Sprite_UpdateScreen with +0x3C zeroed around it.
constexpr std::uint32_t kDrawModeRecord = bof3::addr::UiSprite_SetMode;
constexpr std::uint32_t kDrawSpriteRecord = bof3::addr::UiSprite_Draw;
constexpr std::uint32_t kDepthPair = bof3::addr::Sprite_UpdateScreenScaled;
// E1B's (round thirteen): a window frame and fill (x, y, w, h, colour), each
// read as 16 bits (0x469790 / 0x469960 `and 0xFFFF`), the colour's byte; the
// three option boxes and their labels (x, y, bits byte); a message line (pen
// byte, text, width byte, x word); the member rows (x, y); the two item
// lists (x, y); the panel's title and count.
constexpr std::uint32_t kWindowBox = bof3::addr::Panel_DrawWindow;
constexpr std::uint32_t kOptionBoxes = bof3::addr::EffectKind0F_DrawToggles;
constexpr std::uint32_t kMessageLine = bof3::addr::EffectKind0F_DrawGlyph;
constexpr std::uint32_t kMemberRows = bof3::addr::EffectKind0F_DrawEquipped;
constexpr std::uint32_t kItemListA = bof3::addr::EffectKind0F_DrawItemsB;
constexpr std::uint32_t kItemListB = bof3::addr::EffectKind0F_DrawItemsA;
constexpr std::uint32_t kPanelTitle = bof3::addr::EffectKind0F_DrawCountHeader;
// Capcom's, in no group of this round (catalog part 6, kEffectStd's row): a
// sprite primitive by the word it is handed.
constexpr std::uint32_t kKind07Sprite = 0x462F10;
// Capcom's, unnamed (kEffectStd's rows): a string's characters counted (a byte
// above 0x7F takes two); the library layer's primitive from a RECT (prim, rect),
// 12 bytes written.
constexpr std::uint32_t kStringCount = bof3::addr::EffectKind0F_CharCount;
constexpr std::uint32_t kPrimFromRect = 0x5A7840;

// --- the image's tables, read in place (never copied) ------------------------------
constexpr std::uint32_t kRecordSlot4 = 0x653914;    // WorldMap_Records' +4 (kind 0xE's handler)
constexpr std::uint32_t kRecordSlot8 = 0x653918;    // WorldMap_Records' +8 (kind 0x16's handler)
constexpr std::uint32_t kRecordSize = 0x1C;
constexpr unsigned kRecordCount = 12;               // the eleven records and index 11, "none" (WorldMap_RecordIndex)
constexpr std::uint32_t kMarkerShades = 0x653AF8;   // four bytes: EffectHud_Marker's u by (Frame_Counter >> 3) & 3
constexpr std::uint32_t kKind05Rise = 0x653B24;     // four records of 8: the rise +0x14 and its step +0x20 by +6
constexpr std::uint32_t kKind05Bounce = 0x653B44;   // four records of 8: the height offset and the step on reaching it
constexpr unsigned kKind05Rows = 4;
constexpr std::uint32_t kKind0DSounds = 0x653B8C;   // six words: kind 0xD's sound by +6 (0xFFFF none)
constexpr unsigned kKind0DSoundCount = 6;
constexpr std::uint32_t kKind0FTexts = 0x653B98;    // thirteen records of 8: a string, a line byte +4, a pause byte +5
constexpr unsigned kKind0FTextCount = 13;
constexpr std::uint32_t kKind0FLabels = 0x653C00;   // bytes by a text's line byte: the label's pen
constexpr std::uint32_t kKind0FRows = 0x653C04;     // nine rows of four bytes by +6: three text indexes and a spawn byte
constexpr unsigned kKind0FRowBytes = 0x24;
constexpr std::uint32_t kLabelTexts = 0x66A2FC;     // dwords by a text's line byte: the label strings
constexpr std::uint32_t kQuadHalf = 0x5C41C8;       // 4.0f
constexpr std::uint32_t kQuadEight = 0x5C41CC;      // 8.0f
constexpr std::uint32_t kQuadSixteen = 0x5C41D0;    // 16.0f
constexpr std::uint32_t kQuadThirtyTwo = 0x5C41D4;  // 32.0f
constexpr std::uint32_t kCountFormat = 0x64D3EC;    // Boss26Fx_CountFormat
constexpr std::uint32_t kAccessoryNames = bof3::addr::NameTable_Accessories; // NameTable_Accessories, 0x18 bytes a name

// The effect kinds' state tables (symbols.toml [[data]], this group's names).
constexpr std::uint32_t kKind01States = 0x653A44;
constexpr std::uint32_t kKind07States = 0x653A4C;
constexpr std::uint32_t kKind08States = 0x653A60;
constexpr std::uint32_t kKind09States = 0x653A70;
constexpr std::uint32_t kKind0BStates = 0x653A98;
constexpr std::uint32_t kKind02States = 0x653AC0;
constexpr std::uint32_t kKind03States = 0x653AFC;
constexpr std::uint32_t kKind05States = 0x653B0C;
constexpr std::uint32_t kKind0AStates = 0x653B64;
constexpr std::uint32_t kKind0CStates = 0x653B6C;
constexpr std::uint32_t kKind0DStates = 0x653B78;
constexpr std::uint32_t kKind0FStates = 0x653C28;
constexpr std::uint32_t kKind1AStates = 0x653C5C;

// --- cells ----------------------------------------------------------------------------
constexpr std::uint32_t kRecord0 = 0x7E11E0;        // Effect_Objects record 0 (the kinds' HUD reads it)
constexpr std::uint32_t kRecord2 = 0x7E12E0;        // record 2 (+0xC scales kind 5's height)
constexpr std::uint32_t kRecord3 = 0x7E1360;        // record 3 (kind 0xF runs only there, or while its +1 is above 3;
                                                    // its +0x30 is the window's y the other kind-0xF records draw at)
constexpr std::uint32_t kRecord5 = 0x7E1460;        // record 5 (+0x10's sign picks kind 5's follow animation)
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
constexpr std::uint32_t kExtras = 0x802000;         // Sprite_ObjectsExtra, four of 0xA4
constexpr unsigned kExtraCount = 4;
constexpr std::uint32_t kObjects = 0x7DEE80;        // Sprite_Objects, thirty of 0xA4
constexpr unsigned kObjectCount = 30;
constexpr std::uint32_t kObjectStride = 0xA4;
constexpr std::uint32_t kLeader = 0x802D40;         // ObjTrio 0
constexpr std::uint32_t kTextBuffer = 0x904BA0;     // the text scratch sprintf writes
constexpr std::uint32_t kCounterByte = 0x903848;
constexpr std::uint32_t kAccessoryA = 0x904130;     // the two accessory bytes kind 3 names (the first read as a dword)
constexpr std::uint32_t kAccessoryB = 0x90412E;
constexpr std::uint32_t kKindCounts = 0x9040EC;     // 32 bytes kind 0x1A walks by +0x3E
constexpr std::uint32_t kPanelObject = 0x939A1C;    // a Sprite_Objects index byte (kinds 5 and 0xC)
constexpr std::uint32_t kPanelScript = 0x939A20;    // a pointer: kind 3's and 5's parameters
constexpr std::uint32_t kPanelSet = 0x939A24;       // a pointer: kind 3's and 5's second block

}  // namespace effect_1a::at
