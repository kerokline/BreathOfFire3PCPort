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

// Key_TableDefault, as input::KeysFromTable reads it.
bool ExeDefaultKeys(const std::wstring& exe, std::vector<input::KeyBinding>& keys, std::string& why);

}  // namespace bof3x
