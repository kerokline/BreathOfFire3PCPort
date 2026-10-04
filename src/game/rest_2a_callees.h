// Group R2A's raw addresses (round fourteen, wave two; docs/rest_2a.md): the
// cells and .data tables the group's code names by address. Every function
// the group calls is ours already (the field engine's, SX's Zenny_Add) or
// Capcom's CRT (Rand, Crt_sprintf), called by name, so no callee is listed by
// address here. The three tables named in symbols.toml ([[data]]) are given
// by name where the code reads them (Area_ObjectFallbacks, Mode11_ScreenBanks,
// LinkedObjectD_Amounts); these are the rest.
#pragma once

#include <cstdint>

namespace rest_2a {
namespace at {

// The sprite bank list Sprite_SetAnimationBank searches: a count byte (the
// dword's low byte), and a pointer to its 8-byte records (Title_LoadTask sets
// it to 0x8C3584, just after the count). +0 the bank (u16), +7 the byte a
// sprite keeps in its u16 +0x2C.
constexpr std::uint32_t kBankCount = 0x8C3580;
constexpr std::uint32_t kBankRecords = 0x7E0880;   // unsigned char * to the records
constexpr std::uint32_t kBankRecordsAt = 0x8C3584;  // where Title_LoadTask points it

// The leader (ObjTrio's first record): its direction +8 and its byte +0x30.
constexpr std::uint32_t kLeaderDirection = 0x802D48;
constexpr std::uint32_t kLeader30 = 0x802D70;

// Field_ScriptFlags' high byte (bit 0: the word's 0x100).
constexpr std::uint32_t kScriptFlagsHi = 0x9039A3;

// CharacterRecords' words +0x10 (state), +0x18 (HP), +0x1A (AP), +0x20 (max HP)
// by the record stride.
constexpr std::uint32_t kRecordStride = 0xA4;

// Effect_Objects record 255: where the originals read an effect record's +0
// when a sprite's +0xB holds Effect_FindFree's 0xFF ("none free"). Inside
// Gfx_PacketPools (0x7E1C00..), read in place by ours as by the original.
constexpr std::uint32_t kEffectNone = 0x7E9160;

}  // namespace at
}  // namespace rest_2a
