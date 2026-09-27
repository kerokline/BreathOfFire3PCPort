// Internal to area_w0a.cpp and area_w0a_fuzz.cpp: the cells world 0's first
// areas (0..5, 7, 8, 10, 12, 13, 15) touch that symbols.toml has no name for,
// and the areas' own .data tables, read in place. Every call they make is to
// a named function through the area harness (AH_CALL): Capcom's
// MoveCmd_Move, Crt_sprintf and Rand, ours the rest. docs/area_w0a.md.
#pragma once

#include <cstdint>

namespace area_w0a {
namespace at {

// --- the field frame -------------------------------------------------------
//
// The choice box's cursor row (s8 where a handler indexes a table with it,
// u8 where it only tests it for 0), and the message word a choice handler
// leaves (0xFFFF: no new message; docs/item-use.md section 5).
constexpr std::uint32_t kCursor = 0x7DEE67;
constexpr std::uint32_t kMessage = 0x7DEE48;
// The movement script's variables 3 and 6 (MoveScript_Variable's cases 3..6
// are the bytes 0x903848..0x90384B); variable 5 is the counter 0x90384A.
constexpr std::uint32_t kVar3 = 0x903848;
constexpr std::uint32_t kVar5 = 0x90384A;
constexpr std::uint32_t kVar6 = 0x90384B;
// A byte the effect handlers keep their Effect_FindFree slot in (read back
// as a dword and masked).
constexpr std::uint32_t kEffectSlot = 0x903850;
// The field mode's tail kind (Field_ModeTailKinds 0x662CE8 by this s8) and
// the byte beside it the tail phases read.
constexpr std::uint32_t kTailKind = 0x9039F3;
constexpr std::uint32_t kTailArg = 0x9039F5;
// The story flags (Cond_Flags + 0xA0): Flags_Set's bits.
constexpr std::uint32_t kStoryFlags = 0x904030;
// The dword holding the flag bits the chapter's Flags_Test calls are given.
constexpr std::uint32_t kFlagBank = 0x929ED0;
// A byte area 3's choice handlers set to 6 with the message 0x44.
constexpr std::uint32_t kByte9398CF = 0x9398CF;
// The scene's timer (u16, docs/field-modes.md).
constexpr std::uint32_t kTimer = 0x8034E6;
// Field object 6's x (Sprite_Objects + 6 * 0xA4 + 0x34), a dword.
constexpr std::uint32_t kObject6X = 0x7DF28C;
// Text_Records, where Crt_sprintf writes the message's parameter, and the
// format area 8's handlers 1 and 2 give it (Area08_MessageFormat).
constexpr std::uint32_t kTextRecords = 0x904CE0;
// The per-area zone record lists (8-byte records, the zone byte at +4;
// Area_ZoneAt 0x52FFD0's table, docs/field-event.md): a pointer per area.
constexpr std::uint32_t kZoneLists = 0x668D80;

// --- Effect_Objects: 20 records of 0x80 ------------------------------------
constexpr std::uint32_t kEffects = 0x7E11E0;
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;

// --- the areas' own tables (symbols.toml [[data]]) ---------------------------
constexpr std::uint32_t kArea05MessagesA = 0x5DF6E4;   // u16 x 4, by the s8 cursor
constexpr std::uint32_t kArea05MessagesB = 0x5DF6EC;   // u16 x 4
constexpr std::uint32_t kArea08ChoiceMessages = 0x5E10BC;   // u16 x 2
constexpr std::uint32_t kArea08MessageFormat = 0x5E10C0;    // a format string, passed by address
constexpr std::uint32_t kArea10MessagesA = 0x5E23EC;   // u16 x 4
constexpr std::uint32_t kArea10MessagesB = 0x5E23F4;   // u16 x 4
constexpr std::uint32_t kArea13ChoiceMessages = 0x5E3FFC;   // u16 x 2
// Area 15's two state tables (by Sprite_Current[4]) and the pose pairs
// (an animation for Sprite_EnsureAnimation, a byte for +0x2A) by frame parity.
constexpr std::uint32_t kArea15StatesA = 0x5E5BC4;
constexpr std::uint32_t kArea15PosesA = 0x5E5BCC;
constexpr std::uint32_t kArea15StatesB = 0x5E5BD0;
constexpr std::uint32_t kArea15PosesB = 0x5E5BD8;
constexpr unsigned kArea15States = 2;

}  // namespace at
}  // namespace area_w0a
