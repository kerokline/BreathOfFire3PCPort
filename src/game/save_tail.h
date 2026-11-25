// DIVERGENCE (the state-by-name round's group A4, ledger draft C,
// docs/state-by-name-tier0.md 3): the came-from place in a save file's tail.
//
// A save file is 0x12B0 bytes: the 0x10B0-byte game block and 0x200 bytes
// after it, which every save the PC port writes leaves zero (Save_BuildBlock
// clears the staging buffer past the block and Save_WriteFile writes the
// staging buffer as it is) and which the load reads into Save_Staging and
// neither sums nor copies (LoadMenu_Read sums and copies 0x10B0 bytes). Ours
// writes a tag there, then the three words Area_Enter records on every entry
// and a step onto an encounter field's edge (Field_LeaderStepLands) or a mode
// 8 area's leave (GameMode8_Leave) hands to Field_ChangeArea: the area the
// party came from (AreaW2c_LastArea 0x802290) and the position there (the
// dwords 0x7E091C, 0x7E0920, 16.16). A load that finds the tag puts the three
// words back once the loaded game has entered its area, so the edge leads where
// it led when the save was made, in place of DIV-0121's hold.
//
// The tail's layout, little-endian (a later use adds a record kind):
//
//   +0x00  8  magic "BOF3XTL\0"
//   +0x08  2  version, 1: the grammar below
//   +0x0A  2  the records' bytes that follow, at most 0x1F0
//   +0x0C  4  the FNV-1a of those bytes
//   +0x10     records, each: u16 kind, u16 bytes, then its bytes;
//             a reader skips a kind it does not know
//   the rest  zero
//
//   kind 1, the came-from place, 12 bytes: u16 area (0xFFFF: none), u16 0,
//   s32 x, s32 z.
//
// Switch: "SaveCameFrom", a PatchBytes of a flag of ours set after every
// module's self-test (the hosted fuzz compares Capcom's save callers and the
// title's load): off, no save writes the tail and no load reads it - DIV-0121's
// hold, as before.
#pragma once

#include <cstddef>
#include <cstdint>

namespace save_tail {

constexpr std::size_t kBlockBytes = 0x10B0;   // the tail starts here in the file and in Save_Staging
constexpr std::size_t kTailBytes = 0x200;
constexpr std::size_t kHeadBytes = 0x10;
constexpr std::uint16_t kVersion = 1;
constexpr std::uint16_t kKindCameFrom = 1;
constexpr std::uint16_t kNoArea = 0xFFFF;     // Area_Enter's record after GameMode_Start: none

struct CameFrom {
    std::uint16_t area = kNoArea;
    std::int32_t x = 0, z = 0;
};

// Pure: the tail's 0x200 bytes written whole, the came-from record its one record.
void Build(unsigned char* tail, const CameFrom& from);

// Pure: what a tail holds. NoTag - no magic (every save before this, a
// converted PlayStation save, a save of the original); Damaged - the magic and
// a fault (another version, a length past the tail, the FNV, a record past the
// records); Tagged - read, `has_from` whether a came-from record was among the
// records. `why` names a fault, a short text for the log.
enum class Read { NoTag, Damaged, Tagged };
Read Parse(const unsigned char* tail, CameFrom& from, bool& has_from, const char*& why);

// Pure: the put-back's decision at a frame point after a load armed it.
// `mode` is Game_Mode: 0 and 1 are the loaded game's start and entry
// (GameMode_Start, GameMode_Enter, whose Area_Enter writes the words), and the
// title's menu while the load is applied. Past them, the put-back is due once:
// PutBack when the party is still in the area the save holds and the came-from
// word is still Area_Enter's 0xFFFF; Drop otherwise (an entry event took the
// party on, or the game changed since); Wait in modes 0 and 1.
enum class Due { Wait, PutBack, Drop };
Due Decide(unsigned mode, std::uint16_t area_now, std::uint16_t area_saved, std::uint16_t last_area);

}  // namespace save_tail

namespace bof3 {

// Save_BuildBlock's callers, after the build and before Save_WriteFile (or the
// copy out, for a quicksave's save half): the tail of `tail` (0x200 bytes)
// written from the live words. Nothing while the switch is off.
void SaveTail_Write(unsigned char* tail);

// LoadMenu_Read, when the block summed: the staging buffer's tail read, and the
// put-back armed for a tagged tail whose area is not 0xFFFF. `block_area` is
// the block's area word (+0x24, the area the save resumes in). Nothing while
// the switch is off.
void SaveTail_Loaded(const unsigned char* staging, std::uint16_t block_area);

// The loop's frame point (QuickMenu_AfterPoint): an armed put-back, when due.
void SaveTail_Frame();

// TitleFlow_Begin: the title's menu begins again; a put-back armed and not yet
// due is dropped.
void SaveTail_Forget();

// QuickMenu_Inject: the pure part's self-test (BOF3X_SELFTEST_ONLY), then the
// switch armed.
void SaveTail_Inject(bool selftest);

}  // namespace bof3
