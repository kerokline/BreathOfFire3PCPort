// Bytes of the player's BOF3.exe, read from the file by the address the
// image gives them - for the few tables the launcher needs before any game
// process exists, so that none of them is copied into our source
// (docs/exe-table-audit.md, docs/ASSET_SOURCES.md section 5).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "input/bindings.h"
#include "launcher/save/finds.h"

namespace bof3x {

// `size` bytes at virtual address `va`, from a section's initialised part.
// False, with `why` set, if the file is not a 32-bit PE with that range in it.
bool ExeImageRead(const std::wstring& exe, std::uint32_t va, std::uint32_t size, std::vector<unsigned char>& out,
                  std::string& why);

// Key_TableDefault, as input::KeysFromTable reads it: what the launcher
// compares the engine's rule (input::DefaultKeys) with when the file is there.
bool ExeDefaultKeys(const std::wstring& exe, std::vector<input::KeyBinding>& keys, std::string& why);

// The party-combination table, for the save editor (docs/save-editor-spec.md
// section 13): 19 rows of three character ids, sorted, which PartySet_Find
// (src/game/field/mode_11.cpp) reads at MoveScript_EffectArg + 0xC.
bool ExePartyTable(const std::wstring& exe, std::vector<unsigned char>& rows, std::string& why);

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
// are. Read from the file, never copied into our source.
struct AreaLink {
    std::uint16_t from, to;   // the area the link is in, and the area it leads to
    std::int32_t x, z;        // 16.16, the save's +0x28 / +0x2C
    std::uint8_t flags;       // the save's +0x27
};
bool ExeAreaLinks(const std::wstring& exe, std::vector<AreaLink>& links, std::string& why);

// The finds the save's banks +0x6EC and +0x72C record, for the save editor's
// rows (launcher/save/finds.h): every op 9x of every area's placement script
// and of the two 9x tables area code places from (Area42_Placements,
// Area135_SpawnOps), and every passage entry the take step reads. Read from
// the file, never copied into our source.
bool ExeAreaFinds(const std::wstring& exe, std::vector<save::Find>& chests, std::vector<save::Find>& passages,
                  std::string& why);

}  // namespace bof3x
