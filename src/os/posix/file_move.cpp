#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// bof3::os on POSIX: a rename that keeps or replaces its target
// (os/file_move.h): renameat2 with RENAME_NOREPLACE, else link and unlink;
// rename for the replacing form.
#include "os/file_move.h"

#include <cerrno>
#include <cstdio>

#include <fcntl.h>
#include <unistd.h>

namespace bof3::os {

namespace {

thread_local int t_way = -1;

MoveResult Of(int error) noexcept {
    switch (error) {
    case EEXIST:
    case ENOTEMPTY: return MoveResult::TargetExists;
    case ENOENT: return MoveResult::NoSource;
    default: return MoveResult::Failed;
    }
}

}  // namespace

MoveResult MoveFileKeep(const std::filesystem::path& from, const std::filesystem::path& to) noexcept {
    t_way = -1;
#ifdef RENAME_NOREPLACE
    t_way = 0;
    if (renameat2(AT_FDCWD, from.c_str(), AT_FDCWD, to.c_str(), RENAME_NOREPLACE) == 0) return MoveResult::Moved;
    if (errno != ENOSYS && errno != EINVAL) return Of(errno);
#endif
    // The kernel or the filesystem does not take the flag: a hard link refuses
    // an existing name as the flag would, and the old name goes after it.
    t_way = 1;
    if (link(from.c_str(), to.c_str()) != 0) return Of(errno);
    if (unlink(from.c_str()) != 0) {
        const int error = errno;
        unlink(to.c_str());   // the new name taken back: the file stays where it was
        errno = error;
        return MoveResult::Failed;
    }
    return MoveResult::Moved;
}

MoveResult MoveFileReplace(const std::filesystem::path& from, const std::filesystem::path& to) noexcept {
    if (std::rename(from.c_str(), to.c_str()) == 0) return MoveResult::Moved;
    return Of(errno);
}

int MoveFileKeepWay() noexcept { return t_way; }

}  // namespace bof3::os
