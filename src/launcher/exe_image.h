// Bytes of the player's BOF3.exe, read from the file by the address the
// image gives them - for the few tables the launcher needs before any game
// process exists, so that none of them is copied into our source
// (docs/exe-table-audit.md, docs/ASSET_SOURCES.md section 5).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "input/bindings.h"

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

}  // namespace bof3x
