// The raw addresses scena_sx.cpp reads or calls that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/scena_sx.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
namespace scena_sx::at {

// Callees nobody owns, called through the harness by address (SH_AT).
constexpr ::bof3::image::CodeVa kFormationPlace{bof3::addr::Party_PlaceInFormation};   // (x, z, slot): Sprite_Current placed at the event battle's
                                                      // formation offset for the slot, its ground from
                                                      // MapView_GroundAt; nobody's (Party_PlaceForBattle's one callee)
constexpr ::bof3::image::CodeVa kAbilityListOf{bof3::addr::AbilityList_ForType};    // (member, id, which) -> unsigned char *: one of four 10-byte
                                                      // lists of the member's record (+0x60 / +0x6A / +0x74 / +0x7E)
                                                      // by the id's class; nobody's (a twin of Char_AbilityList)
constexpr ::bof3::image::CodeVa kCameraTurnYaw{bof3::addr::Camera_TurnStep};    // (angle, step): Camera_Angles[0] toward the angle, al 1 while
                                                      // turning; nobody's (Camera_TurnToDegrees' one callee)

// Data.
// kBattleX: now BossSi_CentreX (symbols.toml). s32: the event battle's party x (16.16), Party_PlaceForBattle's
// kBattleZ: now BossSi_CentreZ (symbols.toml). s32: its z
constexpr std::uint32_t kFormation = 0x904AAC;        // u8: the event battle's formation (EventBattle_Records[n] +1)
constexpr std::uint32_t kSlotPositions = 0x7E06E0;    // 4 x (x, z) dwords: a party slot's placed position
// kPartyLists: now Cond_Flags[0xd2] (symbols.toml). two 3-byte lists of member ids (field-event.md section 2)
// kSecondList: now Cond_Flags[0xd5] (symbols.toml). the second list
// kLeaderSlot: now Cond_Flags[0xd0] (symbols.toml). u8: Party_Remove clears it at three members (>= 3) or two
constexpr std::uint32_t kPalettes = 0x80D380;         // 0x40 bytes a member slot: Party_ReloadPalettes' destinations
constexpr std::uint32_t kRecordStride = 0xA4;         // CharacterRecords and Sprite_Objects alike
constexpr std::uint32_t kObjStride = 0x14C;           // ObjTrio's records
constexpr unsigned kSpriteCount = 30;                 // Sprite_Objects' entries
// kKind2Speed: now Sprite_Kind2.speed (symbols.toml). Sprite_Kind2 +0x84: the index into Field_MoveSpeeds
// kKind2Mode: now Sprite_Kind2.direction (symbols.toml). Sprite_Kind2 +8: 2 or 6 halve the ease's frames
// kEaseStep: now CameraTurn_Steps[2] (symbols.toml). s32: Camera_EaseAngleFB's 16.16 step a frame
// kEaseAcc: now CameraTurn_Angles[2] (symbols.toml). s32: its 16.16 accumulator
// kEaseLeft: now ScenaSx_EaseLeft (symbols.toml). s32: its frames left (0: idle)
// kAngleFB: now Cond_AngleFB (symbols.toml). Cond_AngleFB, read and written as the s16 it is
// kLeaderFacing: now ObjTrio[0].direction (symbols.toml). ObjTrio's first record +8: the leader's facing
// kZennyTally: now Rest4a_CountC (symbols.toml). u32: Zenny_Add adds here too when its flag byte is 0
constexpr std::uint32_t kKeyItems = 0x904554;         // 32 bytes: the key-item ids (Inventory_IdLists[4])
constexpr std::uint32_t kAbilityShared = 0x904574;    // 128 bytes: the list AbilityList_Add fills when `shared`
constexpr std::uint32_t kZennyCap = 9999999;          // Zenny_Add's cap

}  // namespace scena_sx::at
