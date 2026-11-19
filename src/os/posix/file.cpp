// bof3::os narrow names on POSIX: the bytes themselves (UTF-8 by convention; the
// kernel does not look), with the game's backslash read as a separator. See
// os/file.h.

#include "os/file.h"

#include <algorithm>
#include <cerrno>
#include <system_error>

#include "os/case_check.h"

namespace bof3::os {

std::filesystem::path Path(const char* narrow) noexcept {
    try {
        std::string s(narrow ? narrow : "");
        std::replace(s.begin(), s.end(), '\\', '/');
        std::filesystem::path p(std::move(s));
        if (CaseCheckOn()) CaseCheck(p);
        return p;
    } catch (...) {
        return std::filesystem::path();
    }
}

// The case check's question (os/case_check.h): a walk of the folder - the
// exact name if the folder holds it, else the first entry equal to it but for
// ASCII case (what a case-insensitive mount opened). A root is not asked.
// errno is left as the call before it left it.
namespace {
bool StoredSpellingOf(const std::filesystem::path& existing, std::string* stored) noexcept {
    try {
        if (!existing.has_relative_path() || !existing.has_filename()) return false;
        const std::string want = existing.filename().native();
        std::filesystem::path dir = existing.parent_path();
        if (dir.empty()) dir = ".";
        std::error_code ec;
        std::filesystem::directory_iterator it(dir, ec), end;
        if (ec) return false;
        const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; };
        std::string folded;
        bool any = false;
        for (; it != end; it.increment(ec)) {
            if (ec) break;
            const std::string n = it->path().filename().native();
            if (n == want) {
                *stored = n;
                return true;
            }
            if (!any && n.size() == want.size() &&
                std::equal(n.begin(), n.end(), want.begin(), [&](char a, char b) { return lower(a) == lower(b); })) {
                folded = n;
                any = true;
            }
        }
        if (any) *stored = folded;
        return any;
    } catch (...) {
        return false;
    }
}
}  // namespace

bool StoredSpelling(const std::filesystem::path& existing, std::string* stored) noexcept {
    const int error = errno;
    const bool there = StoredSpellingOf(existing, stored);
    errno = error;
    return there;
}

namespace {
// Well-formed UTF-8 (RFC 3629): no overlong form, no surrogate, nothing past
// U+10FFFF - what Windows' MB_ERR_INVALID_CHARS refuses, refused here too.
bool WellFormedUtf8(const unsigned char* s) {
    while (*s) {
        const unsigned c = *s;
        int more;
        unsigned lo = 0x80, hi = 0xBF;
        if (c < 0x80) more = 0;
        else if (c >= 0xC2 && c <= 0xDF) more = 1;
        else if (c >= 0xE0 && c <= 0xEF) more = 2, lo = c == 0xE0 ? 0xA0 : 0x80, hi = c == 0xED ? 0x9F : 0xBF;
        else if (c >= 0xF0 && c <= 0xF4) more = 3, lo = c == 0xF0 ? 0x90 : 0x80, hi = c == 0xF4 ? 0x8F : 0xBF;
        else return false;
        ++s;
        for (int k = 0; k < more; ++k, ++s) {
            if (*s < (k == 0 ? lo : 0x80u) || *s > (k == 0 ? hi : 0xBFu)) return false;
        }
    }
    return true;
}
}  // namespace

// UTF-8 is the narrow encoding here: Path's road, once the bytes are checked.
std::filesystem::path PathOfUtf8(const char* utf8) noexcept {
    if (!utf8 || !*utf8 || !WellFormedUtf8(reinterpret_cast<const unsigned char*>(utf8))) return std::filesystem::path();
    return Path(utf8);
}

std::string Narrow(const std::filesystem::path& path) noexcept {
    try {
        return path.native();
    } catch (...) {
        return std::string();
    }
}

}  // namespace bof3::os
