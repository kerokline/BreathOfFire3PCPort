#include "launcher/exe_image.h"

#include "common/sha256.h"
#include "os/wide_file.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

// The one launcher file that sees symbols.toml: its names are macros, kept
// out of the rest of the launcher.
#include "bof3/symbols.gen.h"

namespace bof3x {

namespace {

// The PE headers by their offsets (the PE format's own, as winnt.h lays its
// structures out), so that this reader needs no system header: the DOS
// header's 64 bytes, IMAGE_NT_HEADERS32's 248 and a section header's 40 are
// what is read, as the structures were before.
constexpr std::uint32_t kDosSize = 64, kNtSize = 248, kSecSize = 40;
constexpr std::uint32_t kNtOptional = 24;   // offsetof(IMAGE_NT_HEADERS32, OptionalHeader)

std::uint16_t U16(const unsigned char* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
std::uint32_t U32(const unsigned char* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

struct Dos {
    std::uint16_t magic;   // e_magic: "MZ"
    std::int32_t lfanew;   // e_lfanew
};
Dos DosOf(const unsigned char* d) { return {U16(d), static_cast<std::int32_t>(U32(d + 0x3C))}; }
constexpr std::uint16_t kDosMagic = 0x5A4D;      // IMAGE_DOS_SIGNATURE

struct Nt {
    std::uint32_t signature;      // "PE\0\0"
    std::uint16_t sections;       // FileHeader.NumberOfSections
    std::uint16_t optional_size;  // FileHeader.SizeOfOptionalHeader
    std::uint16_t magic;          // OptionalHeader.Magic
    std::uint32_t image_base;     // OptionalHeader.ImageBase
};
Nt NtOf(const unsigned char* n) { return {U32(n), U16(n + 6), U16(n + 20), U16(n + 24), U32(n + 52)}; }
constexpr std::uint32_t kNtSignature = 0x00004550;   // IMAGE_NT_SIGNATURE
constexpr std::uint16_t kPe32Magic = 0x10B;          // IMAGE_NT_OPTIONAL_HDR32_MAGIC

struct Section {
    std::uint32_t virtual_address, raw_size, raw_pointer;   // VirtualAddress, SizeOfRawData, PointerToRawData
};
Section SectionOf(const unsigned char* s) { return {U32(s + 12), U32(s + 16), U32(s + 20)}; }

// A file whole, or false.
bool ReadWhole(const std::wstring& path, std::vector<unsigned char>& b) {
    const bof3::os::InFile file = bof3::os::InOpenW(path, false);
    if (file == bof3::os::kNoInFile) return false;
    std::uint64_t size = 0;
    bool ok = bof3::os::InSize(file, &size) && size < 0x80000000u;
    b.assign(ok ? static_cast<std::size_t>(size) : 0u, 0);
    std::uint32_t got = 0;
    ok = ok && (b.empty() || (bof3::os::InRead(file, b.data(), static_cast<std::uint32_t>(b.size()), &got) && got == b.size()));
    bof3::os::InClose(file);
    return ok;
}

std::string Narrow(const std::wstring& w) {
    std::string out;
    for (const wchar_t c : w) out += c < 0x80 ? static_cast<char>(c) : '?';
    return out;
}

// The classes of tools/exe_tables.py write_exe whose bytes in the image are
// the game's own values (src/hook/exe_image.cpp kClasses): the PC's (`exe`),
// a disc's (`map`, `place`, `table`, `widen`, and `file`, laid in), the PC's
// pointers (`rebuilt`, `layout`) and the PC's zeros (`none`). The others are
// zero in the image: `own` and `keys` the engine fills from its own tables,
// `language` a language layer writes, `unread`, `gap` and `pointer` no disc
// carries. A class this list does not name is refused by the reader.
bool HowHeld(const std::string& how, bool* known) {
    static const char* const kHeld[] = {"exe", "map", "place", "table", "widen", "file", "rebuilt", "layout", "none"};
    static const char* const kNot[] = {"own", "keys", "language", "unread", "gap", "pointer"};
    *known = true;
    for (const char* h : kHeld)
        if (how == h) return true;
    for (const char* h : kNot)
        if (how == h) return false;
    *known = false;
    return false;
}

// One `[0x.., 0x.., "how", "from"],` line of data.toml's ranges (four fields),
// or one `["key", 0x.., 0x.., count, stride, "sha256", ...]` row of
// tables.toml; the lines around them are `key = value` and comments.
struct TomlRange {
    std::uint32_t lo, hi;
    std::string how, from;
};
struct TomlRow {
    std::string key, sha;
    std::uint32_t lo, hi;
};

std::vector<std::string> Lines(const std::vector<unsigned char>& text) {
    std::vector<std::string> out;
    std::string line;
    for (const unsigned char c : text) {
        if (c == '\n') {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            out.push_back(line);
            line.clear();
        } else {
            line += static_cast<char>(c);
        }
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

}  // namespace

const ExeImage::Range* ExeImage::FirstNotHeld(std::uint32_t va, std::uint32_t size) const {
    // ranges are in order and cover the run (ExeImageFromCache checks it)
    std::size_t lo = 0, hi = ranges.size();
    while (lo < hi) {   // the first range whose end is past va
        const std::size_t mid = (lo + hi) / 2;
        if (ranges[mid].hi <= va) lo = mid + 1;
        else hi = mid;
    }
    for (std::size_t i = lo; i < ranges.size() && ranges[i].lo < va + size; ++i)
        if (!ranges[i].held) return &ranges[i];
    return nullptr;
}

const unsigned char* ExeImage::At(std::uint32_t va, std::uint32_t size) const {
    if (trace) trace->push_back({va, size});
    for (const Run& r : runs) {
        if (va < r.va || static_cast<std::uint64_t>(va) + size > static_cast<std::uint64_t>(r.va) + r.size) continue;
        if (const Range* n = FirstNotHeld(va, size)) {
            if (!not_held++) refused_how = n->how;
            return nullptr;
        }
        return b.data() + r.at + (va - r.va);
    }
    ++outside;
    return nullptr;
}

bool ExeImageFromCache(const std::wstring& cache, ExeImage& img, std::string& why) {
    img = ExeImage();
    const std::wstring dir = cache + L"\\base\\exe\\";
    std::vector<unsigned char> toml_bytes;
    if (cache.empty() || !ReadWhole(dir + L"data.toml", toml_bytes)) {
        why = "the cache has no image (base\\exe\\data.toml)";
        return false;
    }
    std::uint32_t va = 0, size = 0;
    std::string file, sha, image_sha;
    std::vector<TomlRange> ranges;
    bool in_image = false;
    for (const std::string& line : Lines(toml_bytes)) {
        if (!line.empty() && line[0] == '[' && line.compare(0, 2, "[0") != 0) {
            in_image = line == "[image]";
            continue;
        }
        unsigned a = 0, z = 0;
        char s[64] = {}, f[256] = {}, h[80] = {};
        const char* p = line.c_str();
        while (*p == ' ') ++p;
        if (std::sscanf(p, "[0x%x, 0x%x, \"%63[^\"]\", \"%255[^\"]\"]", &a, &z, s, f) == 4 ||
            std::sscanf(p, "[0x%x, 0x%x, \"%63[^\"]\", \"\"]", &a, &z, s) == 3) {
            ranges.push_back({a, z, s, f});
        } else if (!in_image) {
            continue;
        } else if (std::sscanf(p, "va = 0x%x", &a) == 1) {
            va = a;
        } else if (std::sscanf(p, "size = %u", &a) == 1) {
            size = a;
        } else if (std::sscanf(p, "file = \"%63[^\"]\"", s) == 1) {
            file = s;
        } else if (std::sscanf(p, "sha256 = \"%79[^\"]\"", h) == 1) {
            sha = h;
        } else if (std::sscanf(p, "image_sha256 = \"%79[^\"]\"", h) == 1) {
            image_sha = h;
        } else if (std::sscanf(p, "build = \"%63[^\"]\"", s) == 1) {
            img.build = s;
        }
    }
    if (!va || !size || file.empty() || sha.size() != 64 || img.build.empty() || ranges.empty() ||
        file.find_first_of("\\/:") != std::string::npos) {
        why = "base\\exe\\data.toml is not one tools/exe_tables.py wrote";
        return false;
    }
    // The ranges cover the image in order, each of a class the reader knows.
    std::uint32_t covered = va;
    for (const TomlRange& r : ranges) {
        bool known = false;
        const bool held = HowHeld(r.how, &known);
        if (!known || r.lo != covered || r.hi <= r.lo || r.hi - va > size) {
            char m[160];
            std::snprintf(m, sizeof m, "base\\exe\\data.toml's range at 0x%X (\"%s\") is not one this launcher reads",
                          (unsigned)r.lo, r.how.c_str());
            why = m;
            return false;
        }
        covered = r.hi;
        img.ranges.push_back({r.lo, r.hi, held, r.how});
    }
    if (covered != va + size) {
        why = "base\\exe\\data.toml's ranges do not cover the image";
        return false;
    }
    std::wstring wfile(file.begin(), file.end());
    std::string got;
    if (!ReadWhole(dir + wfile, img.b) || img.b.size() != size) {
        why = "base\\exe\\" + file + " is absent or not the size data.toml gives";
        return false;
    }
    if (!Sha256HexBytes(img.b.data(), img.b.size(), got) || got != sha) {
        why = "base\\exe\\" + file + " is not the file data.toml's sha256 names";
        return false;
    }
    // The table files, laid where data.bin leaves them out (an image written
    // before them has no `file` range: data.bin is the image whole).
    bool files = false;
    for (const TomlRange& r : ranges) files = files || r.how == "file";
    if (files) {
        std::vector<unsigned char> rows_bytes;
        if (!ReadWhole(dir + L"tables\\tables.toml", rows_bytes)) {
            why = "base\\exe\\tables\\tables.toml is absent";
            return false;
        }
        std::vector<TomlRow> rows;
        for (const std::string& line : Lines(rows_bytes)) {
            const char* p = line.c_str();
            while (*p == ' ') ++p;
            unsigned a = 0, z = 0, c = 0, s = 0;
            char key[128] = {}, h[128] = {};
            if (std::sscanf(p, "[\"%127[^\"]\", 0x%x, 0x%x, %u, %u, \"%127[^\"]\"", key, &a, &z, &c, &s, h) == 6)
                rows.push_back({key, h, a, z});
        }
        for (const TomlRange& r : ranges) {
            if (r.how != "file") continue;
            const TomlRow* row = nullptr;
            for (const TomlRow& t : rows)
                if (t.key == r.from) row = &t;
            if (!row || r.from.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos ||
                r.lo < row->lo || r.hi > row->hi) {
                why = "base\\exe\\tables\\tables.toml has no row for the table " + r.from;
                return false;
            }
            std::vector<unsigned char> body;
            const std::wstring wkey(r.from.begin(), r.from.end());
            if (!ReadWhole(dir + L"tables\\" + wkey + L".bin", body) || body.size() != row->hi - row->lo ||
                !Sha256HexBytes(body.data(), body.size(), got) || got != row->sha) {
                why = "base\\exe\\tables\\" + r.from + ".bin is absent or not the file tables.toml names";
                return false;
            }
            for (std::uint32_t a = r.lo; a < r.hi; ++a)
                if (img.b[a - va]) {
                    why = "base\\exe\\" + file + " is not zero where the table " + r.from + " is laid";
                    return false;
                }
            std::memcpy(img.b.data() + (r.lo - va), body.data() + (r.lo - row->lo), r.hi - r.lo);
        }
        if (!Sha256HexBytes(img.b.data(), img.b.size(), got) || got != image_sha) {
            why = "the image with its table files is not the image data.toml's image_sha256 names";
            return false;
        }
    }
    img.runs.push_back({va, 0, size});
    return true;
}

bool ExeImageFromFile(const std::wstring& exe, ExeImage& img, std::string& why) {
    img = ExeImage();
    std::vector<unsigned char> b;
    if (!ReadWhole(exe, b)) {
        why = "cannot open " + Narrow(exe);
        return false;
    }
    Dos dos{};
    Nt nt{};
    if (b.size() < kDosSize || (dos = DosOf(b.data())).magic != kDosMagic || dos.lfanew < 0 ||
        static_cast<std::size_t>(dos.lfanew) + kNtSize > b.size() ||
        (nt = NtOf(b.data() + dos.lfanew)).signature != kNtSignature || nt.magic != kPe32Magic) {
        why = "not a 32-bit PE";
        return false;
    }
    const std::size_t first = static_cast<std::size_t>(dos.lfanew) + kNtOptional + nt.optional_size;
    for (unsigned i = 0; i < nt.sections; ++i) {
        if (first + (i + 1) * kSecSize > b.size()) {
            why = "section table cut short";
            return false;
        }
        const Section sec = SectionOf(b.data() + first + i * kSecSize);
        // a section whose raw part runs past the file is held as far as the file goes, as the reader of the
        // whole file took it before
        if (sec.raw_pointer >= b.size()) continue;
        const std::size_t n = std::min<std::size_t>(sec.raw_size, b.size() - sec.raw_pointer);
        img.runs.push_back({nt.image_base + sec.virtual_address, sec.raw_pointer, n});
    }
    img.b = std::move(b);
    img.build = "BOF3.exe";
    return true;
}

bool ExeImageRead(const ExeImage& img, std::uint32_t va, std::uint32_t size, std::vector<unsigned char>& out,
                  std::string& why) {
    const ExeImage::Range* n = img.FirstNotHeld(va, size);
    const unsigned char* p = img.At(va, size);
    if (!p) {
        char m[160];
        if (n)
            std::snprintf(m, sizeof m, "0x%06X..0x%06X: this install's image does not hold it (class %s)", (unsigned)va,
                          (unsigned)(va + size), n->how.c_str());
        else
            std::snprintf(m, sizeof m, "0x%06X..0x%06X: not in this install's image", (unsigned)va, (unsigned)(va + size));
        why = m;
        return false;
    }
    out.assign(p, p + size);
    return true;
}

bool ExeDefaultKeys(const std::wstring& exe, std::vector<input::KeyBinding>& keys, std::string& why) {
    const std::uint32_t va = Key_TableDefault_addr;   // a VA, read from the file: the launcher places nothing
    const std::uint32_t size = Key_TableDefault_count * sizeof Key_TableDefault[0];
    ExeImage img;
    std::vector<unsigned char> table;
    if (!ExeImageFromFile(exe, img, why) || !ExeImageRead(img, va, size, table, why)) return false;
    keys.clear();
    if (!input::KeysFromTable(table.data(), table.size(), keys)) {
        why = "Key_TableDefault holds a key above 0xFF";
        return false;
    }
    return true;
}

bool ExeAreaLinks(const ExeImage& img, std::vector<AreaLink>& links, std::string& why) {
    links.clear();
    const auto at = [&img](std::uint32_t va, std::uint32_t size) { return img.At(va, size); };
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
        why = "Area_Descriptors is not in the image";
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

bool ExeAreaFinds(const ExeImage& img, std::vector<save::Find>& chests, std::vector<save::Find>& passages,
                  std::string& why) {
    chests.clear();
    passages.clear();
    const save::ImageAt at = [&img](std::uint32_t va, std::uint32_t size) { return img.At(va, size); };
    const auto u32 = [](const unsigned char* p) {
        return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    };
    constexpr unsigned kAreas = Area_Descriptors_count;
    const unsigned char* table = at(Area_Descriptors_addr, kAreas * 4);
    const unsigned char* lengths = at(EventScript_OpLengths_addr, EventScript_OpLengths_count);
    if (!table || !lengths) {
        why = "Area_Descriptors or EventScript_OpLengths is not in the image";
        return false;
    }
    constexpr std::uint32_t kOp9 = 13;   // EventOp_9x's record
    for (unsigned a = 0; a < kAreas; ++a) {
        const unsigned char* d = at(u32(table + 4 * a), 0x34);
        if (!d) continue;
        // The placement script (descriptor +0; Area_RunPlacement 0x579740), on every path.
        save::ScriptContentFinds(at, u32(d), lengths, a, chests);
        // The passage list (+0x2C, null in some areas: DIV-0197), +0x31 the last index.
        if (const std::uint32_t list = u32(d + 0x2C))
            for (unsigned i = 0; i <= d[0x31]; ++i) {
                save::Find f;
                if (const unsigned char* e = at(list + 8 * i, 8); e && save::PassageFind(e, a, f)) passages.push_back(f);
            }
    }
    // The 9x records that area code hands EventOp_9x itself: Area42_Init's prizes by
    // Area42_Rank (docs/area_w1b.md), Area135_SpawnCopy's by 0x9039F5 (docs/area_w3d.md).
    const struct {
        unsigned area;
        std::uint32_t va, count;
    } tables[2] = {{42, Area42_Placements_addr, Area42_Placements_count / kOp9},
                   {135, Area135_SpawnOps_addr, Area135_SpawnOps_count / kOp9}};
    for (const auto& t : tables)
        for (std::uint32_t i = 0; i < t.count; ++i) {
            save::Find f;
            const unsigned char* op = at(t.va + kOp9 * i, kOp9);
            if (op && op[0] >> 4 == 9 && save::ContentFind(op, t.area, f)) chests.push_back(f);
        }
    return true;
}

bool ExePartyTable(const ExeImage& img, std::vector<unsigned char>& rows, std::string& why) {
    return ExeImageRead(img, MoveScript_EffectArg_addr + 0xC, 19 * 3, rows, why);
}

bool ExeDefaultNameTables(const ExeImage& img, DefaultNameTables& out, std::string& why) {
    out.chars.clear();
    out.faeries.clear();
    std::vector<unsigned char> name;
    for (std::uint32_t k = 0; k < 8; ++k) {   // the seven records, then the whelp's (NewGame_InitCharacters' n = 7)
        if (!ExeImageRead(img, k < 7 ? Char_DefaultRecords_addr + 0xA4 * k : 0x64B80C, 9, name, why)) return false;
        out.chars.insert(out.chars.end(), name.begin(), name.end());
    }
    for (std::uint32_t r = 0; r < 60; ++r) {
        if (!ExeImageRead(img, 0x653200 + 20 * r, 5, name, why)) return false;
        out.faeries.insert(out.faeries.end(), name.begin(), name.end());
    }
    return true;
}

NameTableTags ExeNameTableTags() {
    // Ability_Records' names start 8 bytes into NameTable_Abilities: the first
    // name there is ability 1's (symbols.toml, NameTable_Abilities' note).
    namespace a = ::bof3::addr;
    return {a::NameTable_Consumables, a::NameTable_KeyItems,    a::NameTable_Weapons,
            a::NameTable_Armour,      a::NameTable_Accessories, a::NameTable_Abilities + 8};
}

}  // namespace bof3x
