// The raw addresses scena_sx2.cpp reads or calls that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/scena_sx2.md.
#pragma once

#include <cstdint>

namespace scena_sx2::at {

// Callees nobody owns, called through the harness by address (SH_AT).
constexpr std::uint32_t kSndBufVolume = bof3::addr::SndBuf_SetVolume;     // (buffer, level): a DirectSound buffer's SetVolume (vtable +0x3C)
                                                      // of (level x two constants - a constant) through __ftol; nothing
                                                      // for a null buffer. The sound module's; nobody's

// Data.
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the story flags (Cond_Flags' row 0x14): flag 0x1C held by
                                                      // Effect_HoldFlag1C, cleared by effect kind 4 (0x469FB0 EffectKind04_HoldTick)
// kLeaderSlot: now Cond_Flags[0xd0] (symbols.toml). u8: the formation group Party_PlaceInFormation adds to
constexpr std::uint32_t kFormation = 0x904AAC;        // u8: the event battle's formation (EventBattle_Records[n] +1)
constexpr std::uint32_t kKeyItems = 0x904554;         // 32 bytes: the key-item ids (Inventory_IdLists[4])
constexpr std::uint32_t kWorkingRecords = 0x802DC0;   // ObjTrio + 0x80: each member's working copy of its record
constexpr std::uint32_t kAbilityType = 0x65C4D9;      // NameTable_Abilities + 1, stride 24: an id's type byte (& 3)
// kLightAngle0: now Light_Angles (symbols.toml). Light_Angles, read as a dword
// kLightAngle2: now Light_Angles[2] (symbols.toml). Light_Angles + 4, read as a dword
// kLightCopy0: now Light_AnglesCopy (symbols.toml). Light_AnglesCopy word 0
// kLightCopy2: now Light_AnglesCopy[2] (symbols.toml). Light_AnglesCopy word 2
// kAngle0: now Camera_Angles (symbols.toml). Camera_Angles word 0 (read as a dword, masked 0xFFF)
// kAngleFB: now Cond_AngleFB (symbols.toml). Cond_AngleFB (Camera_Angles word 2), the same
constexpr std::uint32_t kChannelsEnd = 0x6BC924;      // one past Sound_Channels' 23 dwords
constexpr std::uint32_t kBankStride = 0x384;          // Sound_Banks' records, banks 1..6
constexpr std::uint32_t kBankVoices = 0x184;          // a bank's voice entries (8 bytes: sample, buffer) +4
constexpr std::uint32_t kObjStride = 0x14C;           // ObjTrio's records
constexpr std::uint32_t kRecordStride = 0xA4;         // CharacterRecords'
constexpr std::uint32_t kDepth = 0x3C23D70Au;         // 0.01f, the port's primitive depth (Gpu_SetSprt8's too)

}  // namespace scena_sx2::at
