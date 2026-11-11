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
    const std::uint32_t va = Key_TableDefault_addr;   // a VA, read from the file: the launcher places nothing
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

bool ExeAreaLinks(const std::wstring& exe, std::vector<AreaLink>& links, std::string& why) {
    links.clear();
    // The whole file once: some two thousand small reads follow.
    HANDLE file = CreateFileW(exe.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        why = "cannot open the file";
        return false;
    }
    std::vector<unsigned char> b(GetFileSize(file, nullptr));
    DWORD got = 0;
    const bool read = !b.empty() && ReadFile(file, b.data(), static_cast<DWORD>(b.size()), &got, nullptr) && got == b.size();
    CloseHandle(file);
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS32 nt;
    if (!read || b.size() < sizeof dos || (std::memcpy(&dos, b.data(), sizeof dos), dos.e_magic != IMAGE_DOS_SIGNATURE) ||
        static_cast<std::size_t>(dos.e_lfanew) + sizeof nt > b.size() ||
        (std::memcpy(&nt, b.data() + dos.e_lfanew, sizeof nt), nt.Signature != IMAGE_NT_SIGNATURE) ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        why = "not a 32-bit PE";
        return false;
    }
    const std::size_t first = dos.e_lfanew + offsetof(IMAGE_NT_HEADERS32, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    // `size` bytes at `va` in a section's initialised part, or null.
    const auto at = [&](std::uint32_t va, std::uint32_t size) -> const unsigned char* {
        if (va < nt.OptionalHeader.ImageBase) return nullptr;
        const std::uint32_t rva = va - nt.OptionalHeader.ImageBase;
        for (unsigned i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
            IMAGE_SECTION_HEADER sec;
            if (first + (i + 1) * sizeof sec > b.size()) return nullptr;
            std::memcpy(&sec, b.data() + first + i * sizeof sec, sizeof sec);
            if (rva < sec.VirtualAddress || rva + size > sec.VirtualAddress + sec.SizeOfRawData) continue;
            const std::size_t off = sec.PointerToRawData + (rva - sec.VirtualAddress);
            return off + size <= b.size() ? b.data() + off : nullptr;
        }
        return nullptr;
    };
    const auto u16 = [](const unsigned char* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); };
    const auto u32 = [](const unsigned char* p) {
        return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    };
    // Area_LinkAt's arithmetic: the link's word is cell * 256 + a fraction, kept to the half cell.
    const auto fixed = [](std::uint16_t w) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(w & 0xFF80u) << 8); };

    constexpr unsigned kAreas = Area_Descriptors_count;
    const unsigned char* table = at(Area_Descriptors_addr, kAreas * 4);
    if (!table) {
        why = "Area_Descriptors is not in the file";
        return false;
    }
    // A link list that several areas share is a placeholder (one list of a
    // single link stands in for eleven areas): its link is no door of theirs.
    std::vector<std::uint32_t> lists(kAreas, 0);
    for (unsigned a = 0; a < kAreas; ++a)
        if (const unsigned char* d = at(u32(table + 4 * a), 0x44)) lists[a] = u32(d + 0x20);
    for (unsigned a = 0; a < kAreas; ++a) {
        const unsigned char* d = at(u32(table + 4 * a), 0x44);
        if (!d || lists[a] == 0) continue;
        unsigned shared = 0;
        for (unsigned o = 0; o < kAreas; ++o) shared += lists[o] == lists[a];
        if (shared > 1) continue;
        const unsigned last = d[0x30];
        for (unsigned i = 0; i <= last; ++i) {
            const unsigned char* l = at(lists[a] + 12 * i, 12);
            if (!l) break;
            const auto add = [&](std::uint16_t to, std::uint16_t x, std::uint16_t z, std::uint8_t flags) {
                if (to >= kAreas || (fixed(x) == 0 && fixed(z) == 0)) return;   // a link of cell 0, 0 leads nowhere
                links.push_back({static_cast<std::uint16_t>(a), to, fixed(x), fixed(z), static_cast<std::uint8_t>(flags & 0x8F)});
            };
            if (u16(l + 2) != 0xFFFF) {
                add(u16(l + 2), u16(l + 4), u16(l + 6), l[0xA]);
                continue;
            }
            // An indirect link: alternatives chosen by a story flag, the one after the last the default.
            const unsigned index = u16(l + 4);
            const unsigned char* counts = at(u32(d + 0x28) + index, 1);
            const unsigned char* ptr = at(u32(d + 0x24) + 4 * index, 4);
            if (!counts || !ptr) continue;
            for (unsigned k = 0; k <= counts[0]; ++k)
                if (const unsigned char* alt = at(u32(ptr) + 10 * k, 10)) add(u16(alt), u16(alt + 2), u16(alt + 4), alt[8]);
        }
    }
    return true;
}

bool ExePartyTable(const std::wstring& exe, std::vector<unsigned char>& rows, std::string& why) {
    return ExeImageRead(exe, MoveScript_EffectArg_addr + 0xC, 19 * 3, rows, why);
}

NameTableTags ExeNameTableTags() {
    // Ability_Records' names start 8 bytes into NameTable_Abilities: the first
    // name there is ability 1's (symbols.toml, NameTable_Abilities' note).
    namespace a = ::bof3::addr;
    return {a::NameTable_Consumables, a::NameTable_KeyItems,    a::NameTable_Weapons,
            a::NameTable_Armour,      a::NameTable_Accessories, a::NameTable_Abilities + 8};
}

}  // namespace bof3x
