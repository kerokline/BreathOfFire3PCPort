// bof3::os, the case fold: a name the player's own files bring, opened on a
// filesystem that looks at case (docs/linux-round.md, "B2c as run").
//
// The owner's call 3 of 2026-11-19: a name the ENGINE builds is fixed at its
// source (wave A's measure found none left to fix); case is folded in the
// POSIX backend only for relative names inside the game folder - the names in
// Capcom's data against the files of the player's own disc, which wave A
// listed: "SND\\OYASUMI.DAT" against Oyasumi.DAT, nine kind-0 SND streams,
// "BGM\\000.DAT" against 000.dat, CAPCOM.AVI and LOGOS.AVI against capcom.avi
// and logos.avi. Never for an absolute name: the cache, the home, a BOF3X_
// path and the temporary folder are spelt by us or by the player in full.
//
// The game folder is the process's working folder (bof3x-launcher starts the
// game in it; exe_host_posix.cpp step 11 says so), so a relative name is a
// name inside it - unless a part of it is "..", which leaves it: such a name
// is not folded either.
//
// The rule, in order:
//   1. The exact name first. When it is there (lstat: a dangling link is
//      there too), it is the name, whatever else the folder holds.
//   2. Otherwise one directory walk per part, from the game folder down: a
//      part the folder holds exactly is kept; else the folder's entry equal
//      to it but for ASCII case is taken; a part with none ends the fold, and
//      it and the parts after it stay as asked (so a file made in a folder of
//      another case lands in that folder). A part with '*' or '?' ends it
//      too: it is a walk's pattern, which WalkDirectory matches without case
//      itself. "." is kept.
//   3. Two entries that differ in case alone (Oyasumi.DAT and OYASUMI.DAT
//      beside each other, which no Windows folder can hold): the first in
//      byte order, which is the one WalkDirectory hands first (os/file.h: the
//      upper-cased order, ties by the bytes) - so "the first match" is the
//      same entry for an open and for a walk, on every run whatever order the
//      filesystem lists in. In ASCII an upper-case letter sorts before its
//      lower case: OYASUMI.DAT before Oyasumi.DAT.
//   4. Each folder's listing is cached, and asked again when the folder's
//      identity or its change time is not what it was (a file made or
//      renamed in it since: a quicksave), so a walk is made once per folder
//      and change, not once per open.
//
// Windows folds case itself, as every Windows filesystem the game runs from
// does: the name is the name as asked there (win32/case_fold.cpp).
//
// What the fold says: when the name opens under another spelling, one line
// "CASEFOLD asked <name> opened <name as the disk holds it>", once per name,
// through the case check's channel (os/case_check.h) while BOF3X_CASECHECK=1
// is set (hook/log.cpp hands that channel to the log only then); with the
// switch off the fold says nothing and costs one lstat of a relative name.
//
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstddef>
#include <filesystem>

namespace bof3::os {

// The name as the folder holds it (above), or `asked` itself: an absolute
// name, a name with "..", a name that is there exactly, a name that is not
// there in any case. Never throws; errno is left as the call before it left
// it. os/file.h's Path calls it on POSIX for every narrow name it converts.
std::filesystem::path CaseFold(const std::filesystem::path& asked) noexcept;

// For the tests: how many directory walks the fold has made since the
// process started (the cache's measure), and the cache emptied. Windows: 0.
std::size_t CaseFoldWalks() noexcept;
void CaseFoldForgetForTest() noexcept;

}  // namespace bof3::os
