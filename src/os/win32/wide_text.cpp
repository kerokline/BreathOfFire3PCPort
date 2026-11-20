// bof3::os, wide text on Windows: MultiByteToWideChar and WideCharToMultiByte
// with the arguments the launcher's model used before the seam. See
// os/wide_text.h.

#include "os/wide_text.h"

#include <windows.h>

#include <cwchar>

namespace bof3::os {

namespace {
std::wstring Widen(UINT page, const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(page, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(page, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
}  // namespace

std::wstring WideOfUtf8(const std::string& utf8) { return Widen(CP_UTF8, utf8); }
std::wstring WideOfAnsi(const std::string& ansi) { return Widen(CP_ACP, ansi); }

std::string Utf8OfWide(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

bool IsUtf8(const std::string& bytes) {
    return !bytes.empty() &&
           MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0) > 0;
}

std::wstring WideOfMessage(const std::string& narrow) {
    std::wstring w;
    w.reserve(narrow.size());
    for (char c : narrow) w += static_cast<wchar_t>(static_cast<unsigned char>(c));
    return w;
}

void PrintW(std::FILE* stream, const std::wstring& text) { std::fwprintf(stream, L"%ls", text.c_str()); }

}  // namespace bof3::os
