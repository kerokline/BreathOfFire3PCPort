// The raw addresses scena_sx2.cpp reads or calls that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/scena_sx2.md.
#pragma once

#include <cstdint>

namespace scena_sx2::at {

// Callees nobody owns, called through the harness by address (SH_AT).
constexpr std::uint32_t kSndBufVolume = 0x5A6C60;     // (buffer, level): a DirectSound buffer's SetVolume (vtable +0x3C)
                                                      // of (level x two constants - a constant) through __ftol; nothing
                                                      // for a null buffer. The sound module's; nobody's

// Data.
constexpr std::uint32_t kStoryFlags = 0x904030;       // the story flags (Cond_Flags' row 0x14): flag 0x1C held by
                                                      // Effect_HoldFlag1C, cleared by effect kind 4 (0x469FB0 EffectKind04_HoldTick)
constexpr std::uint32_t kLeaderSlot = 0x904060;       // u8: the formation group Party_PlaceInFormation adds to
constexpr std::uint32_t kFormation = 0x904AAC;        // u8: the event battle's formation (EventBattle_Records[n] +1)
constexpr std::uint32_t kKeyItems = 0x904554;         // 32 bytes: the key-item ids (Inventory_IdLists[4])
constexpr std::uint32_t kWorkingRecords = 0x802DC0;   // ObjTrio + 0x80: each member's working copy of its record
constexpr std::uint32_t kAbilityType = 0x65C4D9;      // NameTable_Abilities + 1, stride 24: an id's type byte (& 3)
constexpr std::uint32_t kLightAngle0 = 0x903598;      // Light_Angles, read as a dword
constexpr std::uint32_t kLightAngle2 = 0x90359C;      // Light_Angles + 4, read as a dword
constexpr std::uint32_t kLightCopy0 = 0x7E0680;       // Light_AnglesCopy word 0
constexpr std::uint32_t kLightCopy2 = 0x7E0684;       // Light_AnglesCopy word 2
constexpr std::uint32_t kAngle0 = 0x929EC8;           // Camera_Angles word 0 (read as a dword, masked 0xFFF)
constexpr std::uint32_t kAngleFB = 0x929ECC;          // Cond_AngleFB (Camera_Angles word 2), the same
constexpr std::uint32_t kChannelsEnd = 0x6BC924;      // one past Sound_Channels' 23 dwords
constexpr std::uint32_t kBankStride = 0x384;          // Sound_Banks' records, banks 1..6
constexpr std::uint32_t kBankVoices = 0x184;          // a bank's voice entries (8 bytes: sample, buffer) +4
constexpr std::uint32_t kObjStride = 0x14C;           // ObjTrio's records
constexpr std::uint32_t kRecordStride = 0xA4;         // CharacterRecords'
constexpr std::uint32_t kDepth = 0x3C23D70Au;         // 0.01f, the port's primitive depth (Gpu_SetSprt8's too)

}  // namespace scena_sx2::at
