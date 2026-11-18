// bof3::os, the wall clock's local time of day, to the millisecond, for a log
// line's stamp (hook/stallwatch.cpp). Windows: GetLocalTime. POSIX:
// clock_gettime(CLOCK_REALTIME) and localtime_r. Not std::chrono's zoned
// time: the time-zone database is not in every C++ library the build uses.
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

namespace bof3::os {

struct LocalTime {
    unsigned hour = 0, minute = 0, second = 0, millisecond = 0;
};
LocalTime LocalNow() noexcept;

}  // namespace bof3::os
