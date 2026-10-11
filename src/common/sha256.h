// The SHA-256 of a file, through Windows' CNG: the check that the player's
// BOF3.exe is the build every address of ours describes. One routine for the
// launcher (before it starts anything) and the engine (before the exe host
// copies a byte of the file, docs/state3-design.md section 1.4).
#pragma once

#include <string>

namespace bof3x {

// fixtures.toml build "pc-zh" / symbols.toml [meta].sha256.
inline constexpr char kExpectedSha256[] = "15984158e0badb4b2f9471c1e9003bb18b2ab11aff47af2376f9ebcd31dfb8cb";

// The file's SHA-256 as lowercase hex into `hex`. False, with `why` set, on
// any I/O or CNG failure.
bool Sha256Hex(const wchar_t* path, std::string& hex, std::string& why);

}  // namespace bof3x
