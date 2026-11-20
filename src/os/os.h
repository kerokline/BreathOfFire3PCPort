// The operating-system seam (docs/linux-round.md, stage LB1): an umbrella
// over one header per service. Every operating-system service the ENGINE uses
// at run time goes through these; the only platform #if is inside the
// backends (src/os/win32, src/os/posix). The window, input, fibers and fault
// catching (units L2 and L3) get their headers here too.
#pragma once

#include "os/case_check.h"
#include "os/clock.h"
#include "os/dialog.h"
#include "os/env.h"
#include "os/event.h"
#include "os/file.h"
#include "os/file_move.h"
#include "os/find.h"
#include "os/hash.h"
#include "os/heap.h"
#include "os/local_time.h"
#include "os/memory.h"
#include "os/module.h"
#include "os/named_event.h"
#include "os/narrow_open.h"
#include "os/own_stack.h"
#include "os/path_file.h"
#include "os/process.h"
#include "os/running.h"
#include "os/sampler.h"
#include "os/stack.h"
#include "os/out_file.h"
#include "os/stream.h"
#include "os/text.h"
#include "os/thread.h"
#include "os/wide_file.h"
#include "os/wide_text.h"
#include "os/window_key.h"
