// bof3::os::CaseFold on Windows: the name as asked. Windows' filesystems
// look up a name without its case themselves, so there is nothing to fold
// and no walk is made; BOF3X_CASECHECK (os/case_check.h) is what finds the
// names that open here only because of that. See os/case_fold.h.
#include "os/case_fold.h"

namespace bof3::os {

std::filesystem::path CaseFold(const std::filesystem::path& asked) noexcept {
    try {
        return asked;
    } catch (...) {
        return std::filesystem::path();
    }
}

std::size_t CaseFoldWalks() noexcept { return 0; }
void CaseFoldForgetForTest() noexcept {}

}  // namespace bof3::os
