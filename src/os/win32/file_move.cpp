// bof3::os on Windows: a rename that keeps or replaces its target
// (os/file_move.h), by the two MoveFileExW calls snapshot.cpp made itself.
#include "os/file_move.h"

#include <windows.h>

namespace bof3::os {

namespace {

MoveResult Move(const std::filesystem::path& from, const std::filesystem::path& to, DWORD flags) noexcept {
    if (MoveFileExW(from.c_str(), to.c_str(), flags)) return MoveResult::Moved;
    switch (GetLastError()) {
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS: return MoveResult::TargetExists;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND: return MoveResult::NoSource;
    default: return MoveResult::Failed;
    }
}

}  // namespace

MoveResult MoveFileKeep(const std::filesystem::path& from, const std::filesystem::path& to) noexcept {
    return Move(from, to, 0);
}

MoveResult MoveFileReplace(const std::filesystem::path& from, const std::filesystem::path& to) noexcept {
    return Move(from, to, MOVEFILE_REPLACE_EXISTING);
}

int MoveFileKeepWay() noexcept { return 0; }
void MoveFileKeepStartAt(int) noexcept {}

}  // namespace bof3::os
