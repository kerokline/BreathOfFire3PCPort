// The raw addresses scena_calls.cpp reads or calls that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/scena_calls.md.
#pragma once

#include <cstdint>

namespace scena_calls::at {

// Callees nobody owns, called through the harness by address (SH_AT).
constexpr std::uint32_t kLeave = 0x534030;      // void(unsigned id): member `id` (its byte) leaves - Party_JoinReset, the id
                                                // taken out of both party lists 0x904065.. / 0x904062.. (the later ones
                                                // moved up, 0xFF behind), the field objects after it moved down one
                                                // (ObjTrio, 0x14C each), Member_ClearState, Field_MemberCount - 1; nobody's
constexpr std::uint32_t kPalettes = 0x533E00;   // void(): per member below Field_MemberCount, Sprite_Current = the member
                                                // (ObjTrio + 0x14C i), Sprite_ReleaseTint and Sprite_LoadPalette(0x80D380 +
                                                // 0x40 i, 0); nobody's (scena_se_callees.h kPartyPalettes)

// Data.
constexpr std::uint32_t kSaved = 0x903A10;      // u8[6]: a saved party - three member ids, then (0x903A13) a copy of the
                                                // second party list; written and read only by this block's entries
                                                // (after the pending area change 0x903A04..0x903A0F)
constexpr std::uint32_t kPartyLists = 0x904062; // two 3-byte lists of member ids (field-event.md section 2), the second at +3
constexpr std::uint32_t kMemberIds = 0x802DC9;  // ObjTrio + 0x89: a field member's character id (event-objs.md)
constexpr std::uint32_t kMemberStride = 0x14C;  // ObjTrio's records
constexpr std::uint32_t kRecordStride = 0xA4;   // CharacterRecords' records: +9 a byte these set, +0xB bit 0 "in the party"
                                                // (Party_Join sets it; char-stats.md section 2 counts it)

}  // namespace scena_calls::at
