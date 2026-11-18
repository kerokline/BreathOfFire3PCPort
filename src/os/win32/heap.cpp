// bof3::os::HeapExactSize on Windows: the C runtime's _msize, the exact size
// the quicksave asked of a block before the seam (sound_ours.cpp). See
// os/heap.h.
#include "os/heap.h"

#include <malloc.h>

namespace bof3::os {

bool HeapHasExactSize() noexcept { return true; }

bool HeapExactSize(const void* block, std::size_t* bytes) noexcept {
    *bytes = _msize(const_cast<void*>(block));
    return true;
}

}  // namespace bof3::os
