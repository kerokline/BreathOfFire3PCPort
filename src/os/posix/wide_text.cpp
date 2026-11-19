// bof3::os, wide text on POSIX: UTF-8 to UTF-32 wchar_t and back, by our own
// code (no locale), with Windows' rule for what does not convert. See
// os/wide_text.h.

#include "os/wide_text.h"

#include <cstdint>

namespace bof3::os {

namespace {

constexpr wchar_t kReplacement = 0xFFFD;

// One code point from `s` at `i`, `i` moved past it. An ill-formed sequence is
// U+FFFD for its longest start that could still have been well formed (the
// Unicode standard's "maximal subpart", which Windows also follows), so that
// the next byte is read again as the start of its own sequence.
wchar_t Next(const std::string& s, std::size_t& i) {
    const unsigned c = static_cast<unsigned char>(s[i++]);
    if (c < 0x80) return static_cast<wchar_t>(c);
    int more;
    unsigned lo = 0x80, hi = 0xBF, cp;
    if (c >= 0xC2 && c <= 0xDF) more = 1, cp = c & 0x1F;
    else if (c >= 0xE0 && c <= 0xEF) more = 2, cp = c & 0x0F, lo = c == 0xE0 ? 0xA0 : 0x80, hi = c == 0xED ? 0x9F : 0xBF;
    else if (c >= 0xF0 && c <= 0xF4) more = 3, cp = c & 0x07, lo = c == 0xF0 ? 0x90 : 0x80, hi = c == 0xF4 ? 0x8F : 0xBF;
    else return kReplacement;
    for (int k = 0; k < more; ++k) {
        if (i >= s.size()) return kReplacement;
        const unsigned b = static_cast<unsigned char>(s[i]);
        if (b < (k == 0 ? lo : 0x80u) || b > (k == 0 ? hi : 0xBFu)) return kReplacement;
        cp = (cp << 6) | (b & 0x3F);
        ++i;
    }
    return static_cast<wchar_t>(cp);
}

void Put(std::string& out, std::uint32_t cp) {
    if ((cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) cp = kReplacement;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

}  // namespace

std::wstring WideOfUtf8(const std::string& utf8) {
    std::wstring out;
    out.reserve(utf8.size());
    for (std::size_t i = 0; i < utf8.size();) out += Next(utf8, i);
    return out;
}

std::string Utf8OfWide(const std::wstring& wide) {
    std::string out;
    out.reserve(wide.size());
    for (wchar_t c : wide) Put(out, static_cast<std::uint32_t>(c));
    return out;
}

bool IsUtf8(const std::string& bytes) {
    if (bytes.empty()) return false;
    for (std::size_t i = 0; i < bytes.size();) {
        const std::size_t at = i;
        const wchar_t c = Next(bytes, i);
        // U+FFFD written out (EF BF BD) is a character, not a refusal
        if (c == kReplacement && !(i - at == 3 && bytes.compare(at, 3, "\xEF\xBF\xBD") == 0)) return false;
    }
    return true;
}

std::wstring WideOfAnsi(const std::string& ansi) {
    std::wstring out;
    out.reserve(ansi.size());
    for (char c : ansi) out += static_cast<wchar_t>(static_cast<unsigned char>(c));
    return out;
}

std::wstring WideOfMessage(const std::string& narrow) { return WideOfUtf8(narrow); }

void PrintW(std::FILE* stream, const std::wstring& text) { std::fputs(Utf8OfWide(text).c_str(), stream); }

}  // namespace bof3::os
