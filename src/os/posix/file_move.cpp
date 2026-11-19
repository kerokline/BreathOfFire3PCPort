#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// bof3::os on POSIX: a rename that keeps or replaces its target
// (os/file_move.h): renameat2 with RENAME_NOREPLACE, else link and unlink,
// else an exclusive create and a rename over it; rename for the replacing form.
#include "os/file_move.h"

#include <cerrno>
#include <cstdio>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bof3::os {

namespace {

thread_local int t_way = -1;
thread_local int t_start = 0;   // the tests' (MoveFileKeepStartAt)

// link's answers that say "no hard links here", not "this link is refused".
// EPERM is also what a protected or immutable source answers; the rename of
// the last way then gives the refusal itself.
bool NoHardLinks(int error) noexcept {
    return error == EPERM || error == EOPNOTSUPP || error == ENOTSUP || error == ENOSYS || error == EMLINK;
}

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
    if (t_start <= 0) {
        t_way = 0;
        if (renameat2(AT_FDCWD, from.c_str(), AT_FDCWD, to.c_str(), RENAME_NOREPLACE) == 0) return MoveResult::Moved;
        if (errno != ENOSYS && errno != EINVAL) return Of(errno);
    }
#endif
    if (t_start <= 1) {
        // The kernel or the filesystem does not take the flag: a hard link refuses
        // an existing name as the flag would, and the old name goes after it.
        t_way = 1;
        if (link(from.c_str(), to.c_str()) == 0) {
            if (unlink(from.c_str()) != 0) {
                const int error = errno;
                unlink(to.c_str());   // the new name taken back: the file stays where it was
                errno = error;
                return MoveResult::Failed;
            }
            return MoveResult::Moved;
        }
        if (!NoHardLinks(errno)) return Of(errno);
    }
    // No hard links either. The name is claimed by an exclusive create, which
    // refuses a file that is there as the two ways above do, and the source is
    // renamed over the empty file the create left. Between the two calls the
    // target is that empty file: another process can see it, and one that
    // removes it and puts its own there loses that file to the rename. A file
    // that was there before this call is never replaced. (The source is looked
    // for first, so that a missing one leaves no empty file to take back; one
    // that goes missing after the look is caught by the rename.)
    t_way = 2;
    if (access(from.c_str(), F_OK) != 0) return Of(errno);
    const int claim = open(to.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (claim < 0) return Of(errno);
    close(claim);
    if (std::rename(from.c_str(), to.c_str()) != 0) {
        const int error = errno;
        // The claim taken back - if it is still the empty file we made, not a
        // file someone put in its place meanwhile.
        struct stat st {};
        if (stat(to.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size == 0) unlink(to.c_str());
        errno = error;
        return Of(error);
    }
    return MoveResult::Moved;
}

MoveResult MoveFileReplace(const std::filesystem::path& from, const std::filesystem::path& to) noexcept {
    if (std::rename(from.c_str(), to.c_str()) == 0) return MoveResult::Moved;
    return Of(errno);
}

int MoveFileKeepWay() noexcept { return t_way; }
void MoveFileKeepStartAt(int way) noexcept { t_start = way; }

}  // namespace bof3::os
