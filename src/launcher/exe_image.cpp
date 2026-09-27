#include "launcher/exe_image.h"

#include <windows.h>

#include <cstring>

// The one launcher file that sees symbols.toml: its names are macros, kept
// out of the rest of the launcher.
#include "bof3/symbols.gen.h"

namespace bof3x {

bool ExeImageRead(const std::wstring& exe, std::uint32_t va, std::uint32_t size, std::vector<unsigned char>& out,
                  std::string& why) {
    HANDLE file = CreateFileW(exe.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        why = "cannot open the file";
        return false;
    }
    auto read_at = [&](DWORD offset, void* to, DWORD n) {
        DWORD got = 0;
        return SetFilePointer(file, static_cast<LONG>(offset), nullptr, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
               ReadFile(file, to, n, &got, nullptr) && got == n;
    };
    bool ok = false;
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS32 nt;
    if (!read_at(0, &dos, sizeof dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || !read_at(dos.e_lfanew, &nt, sizeof nt) ||
        nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        why = "not a 32-bit PE";
    } else if (va < nt.OptionalHeader.ImageBase) {
        why = "address below the image";
    } else {
        const std::uint32_t rva = va - nt.OptionalHeader.ImageBase;
        const DWORD first = dos.e_lfanew + offsetof(IMAGE_NT_HEADERS32, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        why = "address in no section's initialised data";
        for (unsigned i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
            IMAGE_SECTION_HEADER sec;
            if (!read_at(first + i * sizeof sec, &sec, sizeof sec)) {
                why = "section table cut short";
                break;
            }
            if (rva < sec.VirtualAddress || rva + size > sec.VirtualAddress + sec.SizeOfRawData) continue;
            out.resize(size);
            ok = read_at(sec.PointerToRawData + (rva - sec.VirtualAddress), out.data(), size);
            if (!ok) why = "read error";
            break;
        }
    }
    CloseHandle(file);
    return ok;
}

bool ExeDefaultKeys(const std::wstring& exe, std::vector<input::KeyBinding>& keys, std::string& why) {
    const auto va = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Key_TableDefault));
    const std::uint32_t size = Key_TableDefault_count * sizeof Key_TableDefault[0];
    std::vector<unsigned char> table;
    if (!ExeImageRead(exe, va, size, table, why)) return false;
    keys.clear();
    if (!input::KeysFromTable(table.data(), table.size(), keys)) {
        why = "Key_TableDefault holds a key above 0xFF";
        return false;
    }
    return true;
}

}  // namespace bof3x
