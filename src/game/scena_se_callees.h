// The raw addresses scena_se.cpp reads or calls that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/scena_se.md.
#pragma once

#include <cstdint>

namespace scena_se::at {

// Callees nobody owns, called through the harness by address (SH_AT).
constexpr std::uint32_t kPartyPalettes = 0x533E00;    // void(): per member below Field_MemberCount, Sprite_Current = the
                                                      // member (ObjTrio + 0x14C i), Sprite_ReleaseTint and
                                                      // Sprite_LoadPalette(0x80D380 + 0x40 i, 0); nobody's

// Data.
constexpr std::uint32_t kScriptFlags2High = 0x905BA5;  // Field_ScriptFlags2's high byte: bit 4 is the word's bit 12, "an encounter"
constexpr std::uint32_t kEventBattle = 0x904AAA;       // u8: the event battle (0 none; docs/battle_actions.md)
constexpr std::uint32_t kBattleIntro = 0x904AE4;       // u8: the battle's opening kind (BattleWin_OpenSub* read it)
constexpr std::uint32_t kBattleFlags = 0x904AE5;       // u8: the event battle's flags byte (its record's +0)
constexpr std::uint32_t kLeader = 0x802D40;            // ObjTrio's first record: +1 its state, +2..+4 its sub-state bytes
constexpr std::uint32_t kPartyLists = 0x904062;        // two 3-byte lists of member ids (field-event.md section 2)
constexpr std::uint32_t kCount = 0x903850;             // s16: the next Sprite_Objects entry a placement fills (DamageScratch)
constexpr std::uint32_t kBank = 0x903852;              // u16: the animation bank a placement passes on
constexpr std::uint32_t kObjectStride = 0xA4;          // Sprite_Objects and CharacterRecords alike
constexpr unsigned kObjectCount = 30;                  // Sprite_Objects' entries; a count of 30 or more places nothing
constexpr unsigned kEffectStride = 0x80;               // Effect_Objects' records
constexpr unsigned char kCellFxKind = 0x34;            // the kind Effect_SpawnAtCell gives its object (+5)

}  // namespace scena_se::at
