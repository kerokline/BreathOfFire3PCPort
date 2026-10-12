#include "common/icon_stamp.h"

#include <windows.h>

#include <cstring>

namespace bof3x {
namespace {

constexpr char kHeader[] = "bof3x icon stamp 1";

bool IsSha(const std::string& s) {
    if (s.size() != 64) return false;
    for (char c : s)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

}  // namespace

bool IconStampRead(const std::wstring& path, IconStamp& stamp) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    char buf[512];
    DWORD got = 0;
    const BOOL ok = ReadFile(f, buf, sizeof buf - 1, &got, nullptr);
    CloseHandle(f);
    if (!ok) return false;
    buf[got] = '\0';
    // Four lines: the header, then "sheet ", "from ", "to " each with a hash.
    std::string text(buf);
    std::string lines[4];
    std::size_t at = 0;
    for (std::string& line : lines) {
        const std::size_t end = text.find('\n', at);
        if (end == std::string::npos) return false;
        line = text.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        at = end + 1;
    }
    if (lines[0] != kHeader || lines[1].compare(0, 6, "sheet ") != 0 || lines[2].compare(0, 5, "from ") != 0 ||
        lines[3].compare(0, 3, "to ") != 0)
        return false;
    IconStamp s{lines[1].substr(6), lines[2].substr(5), lines[3].substr(3)};
    if (!IsSha(s.sheet) || !IsSha(s.from) || !IsSha(s.to)) return false;
    stamp = s;
    return true;
}

bool IconStampWrite(const std::wstring& path, const IconStamp& stamp, std::string& why) {
    const std::string text = std::string(kHeader) + "\r\nsheet " + stamp.sheet + "\r\nfrom " + stamp.from + "\r\nto " +
                             stamp.to + "\r\n";
    const std::wstring tmp = path + L".tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        why = "cannot create the stamp (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    DWORD wrote = 0;
    const BOOL ok = WriteFile(f, text.data(), static_cast<DWORD>(text.size()), &wrote, nullptr) && wrote == text.size();
    const DWORD error = GetLastError();
    CloseHandle(f);
    if (!ok) {
        DeleteFileW(tmp.c_str());
        why = "cannot write the stamp (error " + std::to_string(error) + ")";
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        why = "cannot move the stamp into place (error " + std::to_string(GetLastError()) + ")";
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

}  // namespace bof3x
