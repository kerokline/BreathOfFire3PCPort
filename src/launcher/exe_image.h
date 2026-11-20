// The game's tables the launcher needs before any game process exists, read
// by the address the image gives them, so that none of them is copied into our
// source (docs/exe-table-audit.md, docs/ASSET_SOURCES.md section 5).
//
// Their source is the install's own image of .data (docs/exe-retire-round.md
// X4): the cache's base\exe\data.bin with its table files laid in, as the
// engine maps it (src/hook/exe_image.cpp ComposeBase; tools/exe_tables.py
// read_image), on every install kind - one made from the PC's BOF3.exe and one
// a disc built. A player's launcher opens no BOF3.exe for them. The file form
// (ExeImageFromFile) is the developer's: BOF3X_EXEFILE=1, and the oracle that
// holds the two forms' answers side by side (tools/launcher_host/save_tests.cpp).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "input/bindings.h"
#include "launcher/save/finds.h"

namespace bof3x {

// The bytes the readers below take, by virtual address. From the cache: one
// run, .data, and data.toml's ranges, each with its class; a byte of a class
// that is not the game's value in the image (`own`, `keys`, `language`,
// `unread`, `gap`, `pointer`: zero there, which the engine fills or leaves
// out) is not held, and a read that touches one gets nothing. From a file:
// every section's initialised part, all held, as the reader of the file read
// it before.
struct ExeImage {
    struct Run {
        std::uint32_t va = 0;
        std::size_t at = 0, size = 0;   // its bytes, b[at, at + size)
    };
    struct Range {
        std::uint32_t lo = 0, hi = 0;
        bool held = true;
        std::string how;
    };
    std::vector<unsigned char> b;
    std::vector<Run> runs;
    std::vector<Range> ranges;   // empty: every byte of every run held
    std::string build;           // data.toml's build ("pc-zh", a disc's); "BOF3.exe" for a file
    // What At refused: reads in no run, and reads in a run that touch a byte
    // not held (the first such range's class in `refused_how`).
    mutable std::uint32_t outside = 0, not_held = 0;
    mutable std::string refused_how;
    // When set, every read At is asked: (va, size), for the measurement of
    // what each reader touches (save_tests' oracle).
    mutable std::vector<std::pair<std::uint32_t, std::uint32_t>>* trace = nullptr;

    // `size` bytes at `va`, or null when not all of them are held.
    const unsigned char* At(std::uint32_t va, std::uint32_t size) const;
    // The first range at or after `va`'s that touches [va, va + size) and is
    // not held, or null (a file's image holds every byte).
    const Range* FirstNotHeld(std::uint32_t va, std::uint32_t size) const;
};

// The cache's image: `cache`\base\exe\data.toml, data.bin and tables\, each
// file checked against the hash data.toml or tables.toml records for it, and
// the image with its table files laid in against data.toml's image_sha256
// when it has table files. False, with `why` a sentence, when the cache has
// no image or a file is absent, damaged or not of the shape exe_tables.py
// writes.
bool ExeImageFromCache(const std::wstring& cache, ExeImage& img, std::string& why);

// A 32-bit PE's sections (the developer's: BOF3X_EXEFILE=1, and the oracle).
bool ExeImageFromFile(const std::wstring& exe, ExeImage& img, std::string& why);

// `size` bytes at virtual address `va`. False, with `why` set, if the image
// does not hold all of them.
bool ExeImageRead(const ExeImage& img, std::uint32_t va, std::uint32_t size, std::vector<unsigned char>& out,
                  std::string& why);

// Key_TableDefault, as input::KeysFromTable reads it, from the file: what the
// developer's start (start.cpp, BOF3X_EXEFILE=1 or hosted) compares the
// engine's rule (input::DefaultKeys) with. The file alone: a disc-built image
// does not hold it (class `keys`, the engine's rule writes it).
bool ExeDefaultKeys(const std::wstring& exe, std::vector<input::KeyBinding>& keys, std::string& why);

// The party-combination table, for the save editor (docs/save-editor-spec.md
// section 13): 19 rows of three character ids, sorted, which PartySet_Find
// (src/game/field/mode_11.cpp) reads at MoveScript_EffectArg + 0xC.
bool ExePartyTable(const ExeImage& img, std::vector<unsigned char>& rows, std::string& why);

// The save editor's default names (save/namemap.h NameSetFromTables), the
// name fields alone: the 9-byte names of the seven default records New Game
// copies (Char_DefaultRecords 0x64B390, 0xA4 apart, the name first) and of
// the whelp's (0x64B80C), then the 5-byte names of the sixty faerie trait
// records (0x653200, 20 bytes each, the name first) -
// docs/save-block/range-b-characters.md, range-e-tail.md. A disc-built image
// does not hold them (a `gap` and `language` ranges there): false.
struct DefaultNameTables {
    std::vector<unsigned char> chars, faeries;   // 8 * 9 and 60 * 5 bytes
};
bool ExeDefaultNameTables(const ExeImage& img, DefaultNameTables& out, std::string& why);

// Where the six name tables' first name fields are in the image: the tags of
// a language layer's name chunks (kind 5, src/game/text/name_tables.cpp),
// which the save editor reads for its lists. Addresses only, no bytes.
struct NameTableTags {
    std::uint32_t items, key_items, weapons, armour, accessories, abilities;
};
NameTableTags ExeNameTableTags();

// The ways into each area, for the save editor's entrance list: every link of
// every area's link list (Area_Descriptors -> a descriptor's +0x20, 12 bytes a
// link; Area_LinkAt 0x5951D0 reads them when the leader steps on one;
// docs/area-entry.md section 1), as the position Area_LinkAt would hand
// Field_ChangeArea - which is what a save's area, position and entry flags
// are. Read from the image, never copied into our source.
struct AreaLink {
    std::uint16_t from, to;   // the area the link is in, and the area it leads to
    std::int32_t x, z;        // 16.16, the save's +0x28 / +0x2C
    std::uint8_t flags;       // the save's +0x27
};
bool ExeAreaLinks(const ExeImage& img, std::vector<AreaLink>& links, std::string& why);

// The finds the save's banks +0x6EC and +0x72C record, for the save editor's
// rows (launcher/save/finds.h): every op 9x of every area's placement script
// and of the two 9x tables area code places from (Area42_Placements,
// Area135_SpawnOps), and every passage entry the take step reads. Read from
// the image, never copied into our source.
bool ExeAreaFinds(const ExeImage& img, std::vector<save::Find>& chests, std::vector<save::Find>& passages,
                  std::string& why);

}  // namespace bof3x
