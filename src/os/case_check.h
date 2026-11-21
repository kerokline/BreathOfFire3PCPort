// bof3::os, the case check (docs/linux-round.md, unit LB8): a developer switch
// that finds, on Windows, every name that opens only because Windows does not
// look at case. A Linux filesystem does: "DAT\\AREA000.EMI" does not open
// "dat/area000.emi" there. The owner's rule (2026-11-19, call 3 of the LC
// staging): a name is fixed at its source where the engine builds it, and case
// is folded on POSIX only for names the player's own files bring.
//
// BOF3X_CASECHECK=1 turns it on; it is off by default, and off it costs one
// test of a word read once per process. On, a name that is there but under
// another spelling gives one line, once per pair of names:
//   CASECHECK asked <the name as given> found <the name as the disk holds it>
//   CASECHECK walk <the pattern> found <an entry it matched only without case>
// Where the checks sit: Path and PathOfUtf8 (every narrow or UTF-8 name passes
// one of them on its way to a library open, a stat or a walk), the wide
// IsFile / IsDirectory, the walks (WalkDirectory, and FindFirst / FindNext on
// Windows, which call the C runtime's search), the game's own fopen
// (crt_rest.cpp's Crt_fopen: OpenNarrow, os/narrow_open.h), and on Windows after
// OpenPath's and OutCreate's opens (on POSIX a create there makes the name as
// asked, so there is nothing to find after it).
//
// How the disk's spelling is found (StoredSpelling, each backend's): each
// part of the name in turn, asked of the folder that holds it. Windows:
// FindFirstFileExW on the exact name, which hands back the entry as the
// folder stores it without opening it - not GetFinalPathNameByHandleW, which
// needs an open handle (refused for a file another process holds without
// sharing, and an open the game never made), resolves junctions, links and
// SUBST drives into names the game never gave, and so cannot be lined up with
// the name asked part by part. POSIX: a walk of the folder, the exact name
// first and then a name equal but for ASCII case - which a case-sensitive
// filesystem never has for a name that opened, and a case-insensitive mount
// (FAT, ntfs3, a WSL /mnt/c folder, ext4 with casefold) does. So on a
// case-sensitive filesystem a line names a name that did NOT open for its
// case alone - the Linux failure itself. A part the folder answers under a
// name that differs by more than ASCII case (a Windows short alias, "RUNNER~1"
// for "runneradmin") is not another spelling: it stays as asked, with no line.
//
// The lines go to the sink the log sets (hook/log.cpp's LogOpen); those made
// before it are kept, up to 1,000, and handed when it is set.
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header.
#pragma once

#include <filesystem>

namespace bof3::os {

// The switch, read once (BOF3X_CASECHECK=1).
bool CaseCheckOn() noexcept;

// Where the lines go: one line each, no line end. Null keeps them.
using CaseCheckSinkFn = void (*)(const char* line);
void CaseCheckSink(CaseCheckSinkFn sink) noexcept;

// Every part of `asked` that is there under another spelling: one line. A
// part that is not there ends the check (nothing opened, nothing to say); a
// part with '*' or '?', ".", ".." and the root are taken as they are. Does
// nothing when the switch is off.
void CaseCheck(const std::filesystem::path& asked) noexcept;
// The same of a narrow name (os/file.h), for a name opened by the C runtime.
void CaseCheckNarrow(const char* narrow) noexcept;
// A walk handed `found` for the leaf pattern `pattern` ('*' and '?'): a line
// when it matches only with case ignored. `where` is the pattern as the
// caller gave it, for the line. Does nothing when the switch is off.
void CaseCheckMatch(const char* where, const std::filesystem::path::string_type& pattern,
                    const std::filesystem::path::string_type& found) noexcept;

// A line of the case fold's (os/case_fold.h) on this channel: once each, kept
// until the sink is set. Does nothing when the switch is off.
void CaseCheckSay(const char* line) noexcept;

// The backend's: the last part of `existing` as its folder stores it. False
// when it is not there, or for a root. For the check and its tests.
bool StoredSpelling(const std::filesystem::path& existing, std::filesystem::path::string_type* stored) noexcept;

// For the tests: the switch set by hand (it is otherwise read once from the
// environment), and the record of lines already given cleared.
void CaseCheckForTest(bool on) noexcept;

}  // namespace bof3::os
