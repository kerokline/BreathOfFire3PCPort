// bof3::os, a file renamed with the target's fate chosen by the caller: kept
// (the rename refuses when the target is there) or replaced. For the
// quicksave's rotation and its newest file (hook/snapshot.cpp, F6), where a
// rename must never overwrite a quicksave that is there.
//
// std::filesystem::rename always replaces, and the standard has no refusing
// form; a test for the target before the rename leaves a gap between the two.
//
// Windows: MoveFileExW with no flags (refuses: ERROR_ALREADY_EXISTS) or with
// MOVEFILE_REPLACE_EXISTING - the two calls snapshot.cpp made before the
// seam. POSIX: renameat2 with RENAME_NOREPLACE (Linux 3.15 on, where the
// filesystem takes the flag); where the kernel or the filesystem refuses the
// flag itself (ENOSYS, EINVAL), link and then unlink, which refuse an
// existing target the same way (EEXIST) - for a moment both names are there,
// so a crash between the two leaves the file under both, never under neither.
// The replacing form is rename.
//
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <filesystem>

namespace bof3::os {

enum class MoveResult {
    Moved,          // `from` is now `to`
    TargetExists,   // refused: `to` is there (the refusing form)
    NoSource,       // `from` is not there
    Failed,         // any other refusal (LastError)
};

// `from` renamed to `to`, which must not be there: refused, nothing moved, if it is.
MoveResult MoveFileKeep(const std::filesystem::path& from, const std::filesystem::path& to) noexcept;
// `from` renamed to `to`, a file at `to` replaced.
MoveResult MoveFileReplace(const std::filesystem::path& from, const std::filesystem::path& to) noexcept;

// The way the refusing form went on POSIX at its last call on this thread:
// 0 renameat2, 1 link and unlink, -1 neither was reached. Windows: always 0
// (one call). For the tests, which say what the system answered.
int MoveFileKeepWay() noexcept;

}  // namespace bof3::os
