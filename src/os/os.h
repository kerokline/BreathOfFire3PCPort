// The operating-system seam (docs/linux-round.md, stage LB1): an umbrella
// over one header per service. Every operating-system service the ENGINE uses
// at run time goes through these; the only platform #if is inside the
// backends (src/os/win32, src/os/posix). The window, input, fibers and fault
// catching (units L2 and L3) get their headers here too.
#pragma once

#include "os/clock.h"
#include "os/dialog.h"
#include "os/env.h"
#include "os/file.h"
#include "os/hash.h"
#include "os/memory.h"
#include "os/text.h"
