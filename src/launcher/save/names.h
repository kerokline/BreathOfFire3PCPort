// The save editor's lists of names, read from the player's install: nothing
// here is in the repository or in the launcher (docs/save-editor-spec.md
// section 10). A language layer's FIRST.DAT holds the item and ability names
// as name chunks (kind 5, DIV-0008: tools/loc_build.py writes them,
// src/game/text/name_tables.cpp reads them), each tagged with the address of
// the table it fills.
#pragma once

#include <cstdint>
#include <string>

#include "launcher/save/block.h"

namespace bof3x::save {

// The tags of the six tables' chunks (launcher/exe_image.h ExeNameTableTags).
struct NameTags {
    std::uint32_t items, key_items, weapons, armour, accessories, abilities;
};

// True for a language whose text the editor can show and write as letters: the
// Latin layers, where a letter is its ASCII byte.
bool LanguageIsLatin(const std::string& language);

// The names in `first_dat` (a language layer's FIRST.DAT). A file that is not
// there, or holds no name chunk, gives no names: the editor then shows numbers.
Names LoadNames(const std::wstring& first_dat, const NameTags& tags, bool latin);

}  // namespace bof3x::save
