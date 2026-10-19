// Internal to area_w0a.cpp and area_w0a_fuzz.cpp: the cells world 0's first
// areas (0..5, 7, 8, 10, 12, 13, 15) touch that symbols.toml has no name for,
// and the areas' own .data tables, read in place. Every call they make is to
// a named function through the area harness (AH_CALL): Capcom's
// Crt_sprintf and Rand, ours the rest (MoveCmd_Move too since 2026-10-10).
// docs/area_w0a.md.
#pragma once

#include <cstdint>

namespace area_w0a {
namespace at {

// --- the field frame -------------------------------------------------------
//
// The choice box's cursor row (s8 where a handler indexes a table with it,
// u8 where it only tests it for 0), and the message word a choice handler
// leaves (0xFFFF: no new message; docs/item-use.md section 5).
// kCursor: now AreaW0c_ChoiceAnswer (symbols.toml).
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
// kTailKind: now AreaW0a_TailKind (symbols.toml).
// kTailArg: now AreaW0c_TailSub (symbols.toml).
// The story flags (Cond_Flags + 0xA0): Flags_Set's bits.
constexpr std::uint32_t kStoryFlags = 0x904030;
// The dword holding the flag bits the chapter's Flags_Test calls are given.
// kFlagBank: now AreaW0c_FlagRow (symbols.toml).
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
// kZoneLists: now Area_StateRecords (symbols.toml).

// --- Effect_Objects: 20 records of 0x80 ------------------------------------
// kEffects: now Effect_Objects (symbols.toml).
constexpr std::uint32_t kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;

// --- the areas' own tables (symbols.toml [[data]]) ---------------------------
// kArea05MessagesA: now Area05_MessagesA (symbols.toml). u16 x 4, by the s8 cursor
// kArea05MessagesB: now Area05_MessagesB (symbols.toml). u16 x 4
// kArea08ChoiceMessages: now Area08_ChoiceMessages (symbols.toml). u16 x 2
constexpr std::uint32_t kArea08MessageFormat = 0x5E10C0;    // a format string, passed by address
// kArea10MessagesA: now Area10_MessagesA (symbols.toml). u16 x 4
// kArea10MessagesB: now Area10_MessagesB (symbols.toml). u16 x 4
// kArea13ChoiceMessages: now Area13_ChoiceMessages (symbols.toml). u16 x 2
// Area 15's two state tables (by Sprite_Current[4]) and the pose pairs
// (an animation for Sprite_EnsureAnimation, a byte for +0x2A) by frame parity.
// kArea15StatesA: now Area15_StatesA (symbols.toml).
// kArea15PosesA: now Area15_PosesA (symbols.toml).
// kArea15StatesB: now Area15_StatesB (symbols.toml).
// kArea15PosesB: now Area15_PosesB (symbols.toml).
constexpr unsigned kArea15States = 2;

}  // namespace at
}  // namespace area_w0a
