// The POSIX backend's own: the range of the own stack this thread runs on
// (os/own_stack.h), set by posix/own_stack.cpp around a run and read by
// posix/thread.cpp's CurrentStack. Not part of the seam.
#pragma once

#include "os/stack.h"

namespace bof3::os {

// True, with the range, while this thread runs on an own stack.
bool SwitchedStack(StackRange* out) noexcept;

// True, with the range, while this thread runs on a stack the caller placed
// (os/placed.h RunOnPlacedStack; posix/placed.cpp). An own stack run from
// there answers first: it is the one the code is on.
bool PlacedStack(StackRange* out) noexcept;

}  // namespace bof3::os
