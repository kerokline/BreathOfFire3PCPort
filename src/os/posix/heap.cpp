// bof3::os::HeapExactSize on POSIX: no answer. glibc's malloc_usable_size
// gives the usable size, which is larger than the request, and nothing gives
// the request itself; the engine's heap here is the arena (os/heap.h,
// LowArena), which keeps it. See os/heap.h.
#include "os/heap.h"

namespace bof3::os {

bool HeapHasExactSize() noexcept { return false; }

bool HeapExactSize(const void*, std::size_t*) noexcept { return false; }

}  // namespace bof3::os
