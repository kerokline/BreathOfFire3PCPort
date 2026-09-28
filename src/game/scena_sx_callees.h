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
constexpr std::uint32_t kFormationPlace = bof3::addr::Party_PlaceInFormation;   // (x, z, slot): Sprite_Current placed at the event battle's
                                                      // formation offset for the slot, its ground from
                                                      // MapView_GroundAt; nobody's (Party_PlaceForBattle's one callee)
constexpr std::uint32_t kAbilityListOf = bof3::addr::AbilityList_ForType;    // (member, id, which) -> unsigned char *: one of four 10-byte
                                                      // lists of the member's record (+0x60 / +0x6A / +0x74 / +0x7E)
                                                      // by the id's class; nobody's (a twin of Char_AbilityList)
constexpr std::uint32_t kCameraTurnYaw = bof3::addr::Camera_TurnStep;    // (angle, step): Camera_Angles[0] toward the angle, al 1 while
                                                      // turning; nobody's (Camera_TurnToDegrees' one callee)

// Data.
constexpr std::uint32_t kBattleX = 0x903780;          // s32: the event battle's party x (16.16), Party_PlaceForBattle's
constexpr std::uint32_t kBattleZ = 0x903784;          // s32: its z
constexpr std::uint32_t kFormation = 0x904AAC;        // u8: the event battle's formation (EventBattle_Records[n] +1)
constexpr std::uint32_t kSlotPositions = 0x7E06E0;    // 4 x (x, z) dwords: a party slot's placed position
constexpr std::uint32_t kPartyLists = 0x904062;       // two 3-byte lists of member ids (field-event.md section 2)
constexpr std::uint32_t kSecondList = 0x904065;       // the second list
constexpr std::uint32_t kLeaderSlot = 0x904060;       // u8: Party_Remove clears it at three members (>= 3) or two
constexpr std::uint32_t kPalettes = 0x80D380;         // 0x40 bytes a member slot: Party_ReloadPalettes' destinations
constexpr std::uint32_t kRecordStride = 0xA4;         // CharacterRecords and Sprite_Objects alike
constexpr std::uint32_t kObjStride = 0x14C;           // ObjTrio's records
constexpr unsigned kSpriteCount = 30;                 // Sprite_Objects' entries
constexpr std::uint32_t kKind2Speed = 0x7E09C4;       // Sprite_Kind2 +0x84: the index into Field_MoveSpeeds
constexpr std::uint32_t kKind2Mode = 0x7E0948;        // Sprite_Kind2 +8: 2 or 6 halve the ease's frames
constexpr std::uint32_t kEaseStep = 0x905B78;         // s32: Camera_EaseAngleFB's 16.16 step a frame
constexpr std::uint32_t kEaseAcc = 0x905B98;          // s32: its 16.16 accumulator
constexpr std::uint32_t kEaseLeft = 0x905B9C;         // s32: its frames left (0: idle)
constexpr std::uint32_t kAngleFB = 0x929ECC;          // Cond_AngleFB, read and written as the s16 it is
constexpr std::uint32_t kLeaderFacing = 0x802D48;     // ObjTrio's first record +8: the leader's facing
constexpr std::uint32_t kZennyTally = 0x904138;       // u32: Zenny_Add adds here too when its flag byte is 0
constexpr std::uint32_t kKeyItems = 0x904554;         // 32 bytes: the key-item ids (Inventory_IdLists[4])
constexpr std::uint32_t kAbilityShared = 0x904574;    // 128 bytes: the list AbilityList_Add fills when `shared`
constexpr std::uint32_t kZennyCap = 9999999;          // Zenny_Add's cap

}  // namespace scena_sx::at
