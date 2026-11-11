// The save editor's format core (docs/save-editor-spec.md, docs/save-block.md):
// the 0x10B0-byte block, its checksum, the PC file around it, and the field
// table - the one source of every field's offset, width, signedness and class,
// which both the read and the write go through. No window code here: the
// launcher's page (save_dialog.cpp) and the host tests
// (tools/launcher_host/save_tests.cpp) use the same functions.
//
// Written from our own documents (the spec's section 2): no code and no table
// of another editor is in it. The offsets are block offsets; a character
// record's are the PC layout's (docs/save-block/range-b-characters.md).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bof3x::save {

constexpr std::size_t kBlockSize = 0x10B0;    // the game state
constexpr std::size_t kPcFileSize = 0x12B0;   // BISLPS0X.DAT: the block, then 0x200 zero bytes
constexpr int kSlots = 16;                    // BISLPS00..0F; 10..1F are quicksaves, never touched

using Block = std::array<std::uint8_t, kBlockSize>;

// The u16 at +0x70: the byte sum of the block with that word counted as zero.
std::uint16_t Checksum(const Block& block);
std::uint16_t StoredChecksum(const Block& block);

// The install's names, for the lists a field offers. Empty where the install
// has none the launcher can read (save_names.cpp).
struct Names {
    bool latin = false;                 // the language played writes a letter as its ASCII byte
    std::vector<std::string> item[4];   // by inventory category: items, weapons, armour, accessories
    std::vector<std::string> key;       // key items
    std::vector<std::string> ability;   // by ability id; [0] is empty
};

enum class Type {
    kUnsigned,   // size bytes, little-endian
    kSigned,
    kBit,        // bit `size` (0..7) of the byte at `at`
    kName,       // `size` bytes of text, NUL-filled
    kClock,      // four bytes: hours, minutes, seconds, frames
    kFixed,      // s32, 16.16
    kHex,        // `size` bytes, shown as hex
    kStack,      // an item id at `at` and its count at `at2`
};

enum class List { kNone, kItem0, kItem1, kItem2, kItem3, kKeyItem, kAbility, kMember };

// The spec's classes (section 7): E a normal field, A advanced, D derived (the
// game or the editor makes it; shown, not edited), P preserved (shown as hex).
struct Field {
    std::string label;
    Type type = Type::kUnsigned;
    std::uint32_t at = 0;
    std::uint32_t size = 1;
    std::int64_t lo = 0, hi = 0;   // the values the field takes; outside them an entry is refused
    List list = List::kNone;
    char cls = 'E';
    std::string note;
    std::uint32_t at2 = 0;
};

struct Section {
    const char* name;
    int records;          // 1, or the number of records the section's box offers
    bool advanced;        // shown only with the advanced box on
};

const std::vector<Section>& Sections();
// A section's fields for one of its records, advanced ones included.
std::vector<Field> SectionFields(int section, int record);
// The words for a record in the section's box: "Record 1 - Ryu", "Weapons".
std::string RecordLabel(const Block& block, int section, int record, const Names& names);

// A field's value as text, and text back into the field. FieldSet changes
// nothing and says why when the text is not a value the field takes.
std::string FieldText(const Block& block, const Field& field, const Names& names);
bool FieldSet(Block& block, const Field& field, const std::string& text, const Names& names, std::string& why);
// What the value box offers for the field: "12 - Dagger". Empty for a field with no list.
std::vector<std::string> FieldChoices(const Block& block, const Field& field, const Names& names);
bool FieldReadOnly(const Field& field);
// One line for a list of saves: "Ryu, level 12, 3:22:10".
std::string Describe(const Block& block, const Names& names);

// The party-combination table of the install (57 bytes at 0x669750 of
// BOF3.exe: 19 rows of three ids), or empty when the install has no file to
// read it from. With it a change of party members gets its row at +0x74C;
// without it the members cannot change (Check says so).
using PartyTable = std::vector<std::uint8_t>;

// The fields the editor sets itself (the spec's section 13, "Set by the
// editor"), each only where its source changed from `before`, so a block
// nobody edited is written byte for byte. Then the checksum.
void Finish(const Block& before, Block& now, const PartyTable& table);

struct Issue {
    bool error;         // true: the save is refused; false: a warning, written if the player says so
    std::string text;
};
// What is wrong with `now` that was not already wrong with `before`.
std::vector<Issue> Check(const Block& before, const Block& now, const PartyTable& table);

// The PC file. ReadPcFile refuses a file that is not 0x12B0 bytes; a wrong
// checksum is not a refusal, the caller asks StoredChecksum. WritePcFile
// writes beside the target, reads the bytes back, keeps the file it replaces
// in `backup_dir` (made if absent; `backup` names the copy) and then replaces.
bool ReadPcFile(const std::wstring& path, Block& block, std::string& why);
bool WritePcFile(const std::wstring& path, const Block& block, const std::wstring& backup_dir, std::wstring& backup,
                 std::string& why);

}  // namespace bof3x::save
