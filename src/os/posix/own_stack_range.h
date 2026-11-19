// The POSIX backend's own: the range of the own stack this thread runs on
// (os/own_stack.h), set by posix/own_stack.cpp around a run and read by
// posix/thread.cpp's CurrentStack. Not part of the seam.
#pragma once

#include "os/stack.h"

namespace bof3::os {

// True, with the range, while this thread runs on an own stack.
bool SwitchedStack(StackRange* out) noexcept;

}  // namespace bof3::os
