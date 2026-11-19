// StrToL32 / StrToUL32: strtol and strtoul with the answer a 32-bit `long`
// gives, on every width.
//
// The game's code and its tools parse their settings with strtol / strtoul
// and keep the result in 32 bits. Where `long` is 32 bits (Windows, both
// widths) the C library saturates a number that does not fit - LONG_MAX,
// LONG_MIN or ULONG_MAX, errno ERANGE - and the range check that follows
// refuses it. Where `long` is 64 bits (x86-64 Linux) the library returns the
// whole number, and storing that in 32 bits keeps its low half: 4294967298
// reads as 2 and passes the check. These two parse into the library's own
// type and narrow as the 32-bit library would, so one text gives one answer.
//
// Where `long` is 32 bits each is the library's call and nothing else: the
// narrowing below is not compiled into the path taken. Narrow32 / NarrowU32
// are the 64-bit path's arithmetic, apart so that a 32-bit `long` build can
// hold them to its own strtol / strtoul (tools/os_host/os_tests.cpp).
//
// Header-only and free of Windows.
#pragma once

#include <cerrno>
#include <cstdint>
#include <cstdlib>

namespace bof3 {

// What a 32-bit strtol returns for a number whose true value is `wide`.
inline std::int32_t Narrow32(long long wide) {
    if (wide > INT32_MAX) {
        errno = ERANGE;
        return INT32_MAX;
    }
    if (wide < INT32_MIN) {
        errno = ERANGE;
        return INT32_MIN;
    }
    return static_cast<std::int32_t>(wide);
}

// What a 32-bit strtoul returns, given a wider strtoul's result for the same
// text, whether the text carried a minus sign, and whether the wider call
// itself overflowed. strtoul reads the digits as a magnitude, saturates when
// the magnitude does not fit, and otherwise negates in the unsigned type: so
// "-2" is 0xFFFFFFFE in 32 bits, and "-4294967298" saturates.
inline std::uint32_t NarrowU32(unsigned long long wide, bool minus, bool overflowed) {
    const unsigned long long magnitude = minus ? 0ull - wide : wide;
    if (overflowed || magnitude > UINT32_MAX) {
        errno = ERANGE;
        return UINT32_MAX;
    }
    return static_cast<std::uint32_t>(wide);   // the low half is the 32-bit negation
}

// Whether strtol / strtoul would read a minus sign at the head of `text`:
// after the white space the C locale's isspace skips.
inline bool StrToMinus(const char* text) {
    while (*text == ' ' || (*text >= '\t' && *text <= '\r')) ++text;
    return *text == '-';
}

inline std::int32_t StrToL32(const char* text, char** end, int base) {
    const long v = std::strtol(text, end, base);
    if (sizeof(long) == sizeof(std::int32_t)) return static_cast<std::int32_t>(v);
    return Narrow32(v);
}

inline std::uint32_t StrToUL32(const char* text, char** end, int base) {
    if (sizeof(unsigned long) == sizeof(std::uint32_t)) return static_cast<std::uint32_t>(std::strtoul(text, end, base));
    const int before = errno;
    errno = 0;
    const unsigned long v = std::strtoul(text, end, base);
    const bool overflowed = errno == ERANGE;
    errno = before;
    return NarrowU32(v, StrToMinus(text), overflowed);
}

}   // namespace bof3
