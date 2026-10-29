// The raw addresses scena_se.cpp reads or calls that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/scena_se.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_se::at {

// Callees nobody owns, called through the harness by address (SH_AT).
constexpr ::bof3::image::CodeVa kPartyPalettes{bof3::addr::Party_ReloadPalettes};    // void(): per member below Field_MemberCount, Sprite_Current = the
                                                      // member (ObjTrio + 0x14C i), Sprite_ReleaseTint and
                                                      // Sprite_LoadPalette(0x80D380 + 0x40 i, 0); nobody's

// Data.
constexpr std::uint32_t kScriptFlags2High = 0x905BA5;  // Field_ScriptFlags2's high byte: bit 4 is the word's bit 12, "an encounter"
// kEventBattle: now EventBattle_Id (symbols.toml). u8: the event battle (0 none; docs/battle_actions.md)
// kBattleIntro: now BattlePhases_Initiative (symbols.toml). u8: the battle's opening kind (BattleWin_OpenSub* read it)
// kBattleFlags: now EventBattle_Flags (symbols.toml). u8: the event battle's flags byte (its record's +0)
// kLeader: now ObjTrio (symbols.toml). ObjTrio's first record: +1 its state, +2..+4 its sub-state bytes
// kPartyLists: now Cond_Flags[0xd2] (symbols.toml). two 3-byte lists of member ids (field-event.md section 2)
inline auto& kCount = SharedScratch.w_00;             // s16: the next Sprite_Objects entry a placement fills (SharedScratch)
constexpr std::uint32_t kCount_addr = SharedScratch_addr; // kCount's address, for the raw accessors that take one
inline auto& kBank = SharedScratch.w_02;              // u16: the animation bank a placement passes on
constexpr std::uint32_t kBank_addr = SharedScratch_addr + 0x2; // kBank's address, for the raw accessors that take one
constexpr std::uint32_t kObjectStride = 0xA4;          // Sprite_Objects and CharacterRecords alike
constexpr unsigned kObjectCount = 30;                  // Sprite_Objects' entries; a count of 30 or more places nothing
constexpr unsigned kEffectStride = 0x80;               // Effect_Objects' records
constexpr unsigned char kCellFxKind = 0x34;            // the kind Effect_SpawnAtCell gives its object (+5)

}  // namespace scena_se::at
