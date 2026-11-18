// bof3::os, a key posted to a window on POSIX: there is no window message to
// post, so the call refuses. See os/window_key.h.
#include "os/window_key.h"

#include <errno.h>

namespace bof3::os {

bool PostWindowKey(void*, unsigned, bool) noexcept {
    errno = ENOSYS;
    return false;
}

}  // namespace bof3::os
