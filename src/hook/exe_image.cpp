// BOF3X_EXEIMAGE (exe_image.h; docs/exe-import-engine.md section 4.4).
#include "hook/exe_image.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "hook/exe_host.h"
#include "hook/exe_layout.h"
#include "hook/exe_own.h"
#include "hook/log.h"

namespace bof3 {
namespace {

struct Range {
    std::uint32_t lo, hi;
    std::string how, from;
};

struct Image {
    std::uint32_t va = 0, size = 0, bss_end = 0;
    std::string build, file, gaps;
    std::vector<Range> ranges;
};

// data.toml's classes (tools/exe_tables.py write_exe), what the engine does
// with each in a disc-built image, and whether the bytes must then equal the
// PC's .data (the comparison when BOF3.exe is there too):
//   exe, map, place, table, widen  copied (a disc's numbers may differ: logged)
//   rebuilt, layout                copied; the PC's pointer by construction (must equal)
//   own                            filled by ExeOwn_Fill after the copy (must equal)
//   keys                           Key_TableDefault, written by Shell_MapDefaultKeys (must equal)
//   none                           zero; recipes/exe-gaps.tsv says the PC's are zero too (must equal)
//   language                       zero; a language layer's handlers write it, or ours read around it
//   unread                         zero; nothing the exe host runs reads it
//   gap                            zero where the PC has a value no disc carries: logged, each range
//                                  once; BOF3X_EXEIMAGE_STRICT=1 makes it a Fatal
//   pointer                        a pointer word left unfilled: a Fatal (no disc build has one)
struct Class {
    const char* how;
    bool must_equal;
};
constexpr Class kClasses[] = {{"exe", true},     {"map", false},     {"place", false},   {"table", false},
                              {"widen", false},  {"rebuilt", true},  {"layout", true},   {"own", true},
                              {"keys", true},    {"none", true},     {"language", false}, {"unread", false},
                              {"gap", false},    {"pointer", true}};

const Class* ClassOf(const Range& r) {
    for (const Class& c : kClasses)
        if (r.how == c.how) return &c;
    Fatal("BOF3X_EXEIMAGE: data.toml range 0x%X has an unknown how \"%s\"", (unsigned)r.lo, r.how.c_str());
}

// The disc-built image ExeImage_Source mapped, kept for ExeImage_CompareWithFile.
Image g_mapped;
bool g_disc_mapped = false;

// data.toml as tools/exe_tables.py writes it: `key = value` lines in [image],
// then one `[0x.., 0x.., "how", "from"],` line a range. Only that shape.
Image ReadToml(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) Fatal("BOF3X_EXEIMAGE: cannot open %s", path.c_str());
    Image im;
    char line[1024];
    while (std::fgets(line, sizeof line, f)) {
        unsigned a = 0, b = 0;
        char s[64] = {0}, from[256] = {0};
        const char* p = line;
        while (*p == ' ') ++p;
        const int got = std::sscanf(p, "[0x%x, 0x%x, \"%63[^\"]\", \"%255[^\"]\"", &a, &b, s, from);
        if (got >= 3) {
            im.ranges.push_back({a, b, s, got == 4 ? from : ""});
        } else if (std::sscanf(p, "va = 0x%x", &a) == 1) {
            im.va = a;
        } else if (std::sscanf(p, "size = %u", &a) == 1) {
            im.size = a;
        } else if (std::sscanf(p, "bss_end = 0x%x", &a) == 1) {
            im.bss_end = a;
        } else if (std::sscanf(p, "build = \"%63[^\"]\"", s) == 1) {
            im.build = s;
        } else if (std::sscanf(p, "file = \"%63[^\"]\"", s) == 1) {
            im.file = s;
        } else if (std::sscanf(p, "gaps = \"%63[^\"]\"", s) == 1) {
            im.gaps = s;
        }
    }
    std::fclose(f);
    if (!im.va || !im.size || !im.bss_end || im.build.empty() || im.file.empty() || im.ranges.empty())
        Fatal("BOF3X_EXEIMAGE: %s is not a data.toml exe_tables.py wrote", path.c_str());
    return im;
}

std::vector<unsigned char> ReadBin(const std::string& path, std::uint32_t size) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) Fatal("BOF3X_EXEIMAGE: cannot open %s", path.c_str());
    std::vector<unsigned char> out(size + 1);
    const std::size_t n = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    if (n != size) Fatal("BOF3X_EXEIMAGE: %s holds %u bytes, data.toml says %u", path.c_str(), (unsigned)n, (unsigned)size);
    out.resize(size);
    return out;
}

// BOF3.exe's .data: (address, initialised size, virtual size) - its section
// table's values (exe_layout.h), where this read the running image's header
// before the exe host existed; the exe host checks them against the
// player's file before copying (exe_host.cpp).
void LiveData(std::uint32_t& va, std::uint32_t& raw, std::uint32_t& vsz) {
    va = exe_layout::kDataVa;
    raw = exe_layout::kDataBytes;
    vsz = exe_layout::kBssEnd - exe_layout::kDataVa;
}

}  // namespace

void ExeImage_Check() {
    char dir[MAX_PATH];
    const DWORD n = GetEnvironmentVariableA("BOF3X_EXEIMAGE", dir, sizeof dir);
    if (n == 0) return;
    if (n >= sizeof dir) Fatal("BOF3X_EXEIMAGE: the path is longer than %u", (unsigned)sizeof dir);
    const std::string exe_dir = std::string(dir) + "\\base\\exe\\";
    const Image im = ReadToml(exe_dir + "data.toml");
    std::uint32_t va = 0, raw = 0, vsz = 0;
    LiveData(va, raw, vsz);
    if (im.va != va || im.size != raw || im.bss_end != va + vsz)
        Fatal("BOF3X_EXEIMAGE: data.toml's image is 0x%X + 0x%X (zero to 0x%X), the running .data 0x%X + 0x%X (to 0x%X)",
              (unsigned)im.va, (unsigned)im.size, (unsigned)im.bss_end, (unsigned)va, (unsigned)raw, (unsigned)(va + vsz));
    const std::vector<unsigned char> bin = ReadBin(exe_dir + im.file, im.size);
    auto* live = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(va));

    // .bss: zero-initialised by the loader, as state 3's engine must do itself.
    for (std::uint32_t a = va + raw; a < im.bss_end; ++a)
        if (live[a - va] != 0) Fatal("BOF3X_EXEIMAGE: .bss is not zero at 0x%X before the game starts", (unsigned)a);

    // Every byte, counted by the range that produced it.
    std::uint32_t equal = 0, first_diff = 0;
    struct Count { const char* how; std::uint32_t equal, total; };
    Count by[] = {{"exe", 0, 0},     {"map", 0, 0},    {"place", 0, 0},    {"table", 0, 0},  {"widen", 0, 0},
                  {"rebuilt", 0, 0}, {"layout", 0, 0}, {"own", 0, 0},      {"keys", 0, 0},   {"none", 0, 0},
                  {"language", 0, 0}, {"unread", 0, 0}, {"gap", 0, 0},     {"pointer", 0, 0}};
    std::uint32_t covered = va;
    for (const Range& r : im.ranges) {
        if (r.lo != covered || r.hi <= r.lo || r.hi > va + raw)
            Fatal("BOF3X_EXEIMAGE: data.toml's ranges do not cover the image in order at 0x%X", (unsigned)r.lo);
        covered = r.hi;
        Count* c = nullptr;
        for (Count& k : by)
            if (r.how == k.how) c = &k;
        if (!c) Fatal("BOF3X_EXEIMAGE: data.toml range 0x%X has an unknown how \"%s\"", (unsigned)r.lo, r.how.c_str());
        for (std::uint32_t a = r.lo; a < r.hi; ++a) {
            const bool same = bin[a - va] == live[a - va];
            c->total += 1;
            c->equal += same;
            equal += same;
            if (!same && !first_diff) first_diff = a;
        }
    }
    if (covered != va + raw) Fatal("BOF3X_EXEIMAGE: data.toml's ranges end at 0x%X, not 0x%X", (unsigned)covered, (unsigned)(va + raw));
    for (const Count& c : by)
        if (c.total) Log("BOF3X_EXEIMAGE  %-8s %7u of %7u bytes equal to the running .data", c.how, (unsigned)c.equal, (unsigned)c.total);

    if (im.build == "pc-zh") {
        if (equal != im.size)
            Fatal("BOF3X_EXEIMAGE: base/exe/data.bin is built from BOF3.exe but %u of its %u bytes differ from the "
                  "running .data, the first at 0x%X", (unsigned)(im.size - equal), (unsigned)im.size, (unsigned)first_diff);
        std::memcpy(live, bin.data(), im.size);   // the same bytes: the map step, changing nothing
        Log("BOF3X_EXEIMAGE: base/exe/data.bin (pc-zh) laid over .data at 0x%X: %u bytes, byte-identical; "
            ".bss zero to 0x%X", (unsigned)va, (unsigned)im.size, (unsigned)im.bss_end);
    } else {
        Log("BOF3X_EXEIMAGE: base/exe/data.bin (%s) compared, not mapped: %u of %u bytes equal to the running .data "
            "(the hosted build lays only a pc-zh image over Capcom's; bof3x.exe maps a disc-built one)",
            im.build.c_str(), (unsigned)equal, (unsigned)im.size);
    }
}

bool ExeImage_Source(void* to, std::uint32_t va, std::uint32_t size, std::uint32_t bss_end) {
    char dir[MAX_PATH];
    const DWORD n = GetEnvironmentVariableA("BOF3X_EXEIMAGE", dir, sizeof dir);
    if (n == 0) return false;
    if (n >= sizeof dir) Fatal("BOF3X_EXEIMAGE: the path is longer than %u", (unsigned)sizeof dir);
    const std::string exe_dir = std::string(dir) + "\\base\\exe\\";
    const Image im = ReadToml(exe_dir + "data.toml");
    if (im.va != va || im.size != size || im.bss_end != bss_end)
        Fatal("BOF3X_EXEIMAGE: data.toml's image is 0x%X + 0x%X (zero to 0x%X), BOF3.exe's .data 0x%X + 0x%X (to 0x%X)",
              (unsigned)im.va, (unsigned)im.size, (unsigned)im.bss_end, (unsigned)va, (unsigned)size, (unsigned)bss_end);
    std::uint32_t covered = va;
    for (const Range& r : im.ranges) {
        if (r.lo != covered || r.hi <= r.lo || r.hi > va + size)
            Fatal("BOF3X_EXEIMAGE: data.toml's ranges do not cover the image in order at 0x%X", (unsigned)r.lo);
        covered = r.hi;
        ClassOf(r);
    }
    if (covered != va + size)
        Fatal("BOF3X_EXEIMAGE: data.toml's ranges end at 0x%X, not 0x%X", (unsigned)covered, (unsigned)(va + size));
    const std::vector<unsigned char> bin = ReadBin(exe_dir + im.file, im.size);

    if (im.build == "pc-zh") {
        std::memcpy(to, bin.data(), im.size);
        // The bytes no disc carries that are the engine's own (exe_own.h):
        // checked against the image before they are written, so that a pc-zh
        // image proves them; over a pc-zh image the fill changes nothing.
        ExeOwn_Verify(bin.data(), "base/exe/data.bin (pc-zh)");
        ExeOwn_Fill(static_cast<std::uint8_t*>(to));
        Log("BOF3X_EXEIMAGE: .data at 0x%X from base/exe/data.bin (pc-zh), %u bytes: the source in the exe host",
            (unsigned)va, (unsigned)im.size);
        return true;
    }

    // A disc-built image (docs/exe-import-engine.md section 4.4): every range
    // a class the engine accounts for. An image written before
    // recipes/exe-gaps.tsv named what no disc carries has `none` ranges that
    // hide the PC's values: refused, as the no-stub rule wants.
    if (im.gaps.empty())
        Fatal("BOF3X_EXEIMAGE: %s (%s) was built before recipes/exe-gaps.tsv classed the bytes no disc carries: its "
              "`none` ranges may stand where the PC has values. Rebuild the cache (tools/importer.py build).",
              exe_dir.c_str(), im.build.c_str());
    for (const Range& r : im.ranges)
        if (r.how == "pointer")
            Fatal("BOF3X_EXEIMAGE: %s (%s) leaves the pointer words 0x%X..0x%X unfilled; every pointer word of a disc "
                  "build is rebuilt or the layout's (recipes/exe-layout.tsv). Rebuild the cache.",
                  exe_dir.c_str(), im.build.c_str(), (unsigned)r.lo, (unsigned)r.hi);
    const bool strict = [] {
        char v[8] = {};
        return GetEnvironmentVariableA("BOF3X_EXEIMAGE_STRICT", v, sizeof v) > 0 && std::strcmp(v, "0") != 0;
    }();

    std::memcpy(to, bin.data(), im.size);
    ExeOwn_Fill(static_cast<std::uint8_t*>(to));   // the engine's own regions, over the image's zeros

    struct Count {
        const char* how;
        std::uint32_t ranges, bytes;
    } by[sizeof kClasses / sizeof kClasses[0]] = {};
    for (std::size_t i = 0; i < sizeof kClasses / sizeof kClasses[0]; ++i) by[i].how = kClasses[i].how;
    for (const Range& r : im.ranges) {
        const std::size_t i = static_cast<std::size_t>(ClassOf(r) - kClasses);
        by[i].ranges += 1;
        by[i].bytes += r.hi - r.lo;
        if (r.how != "gap") continue;
        if (strict)
            Fatal("BOF3X_EXEIMAGE_STRICT: 0x%06X..0x%06X (%u bytes, %s) is a gap - the PC has a value there that no "
                  "disc carries - and the strict switch refuses an image with one",
                  (unsigned)r.lo, (unsigned)r.hi, (unsigned)(r.hi - r.lo), r.from.c_str());
        Log("BOF3X_EXEIMAGE  gap 0x%06X..0x%06X %4u bytes  %s: zero (the PC has a value; no disc carries it)",
            (unsigned)r.lo, (unsigned)r.hi, (unsigned)(r.hi - r.lo), r.from.c_str());
    }
    for (const Count& c : by)
        if (c.ranges) Log("BOF3X_EXEIMAGE  %-8s %5u ranges %7u bytes", c.how, (unsigned)c.ranges, (unsigned)c.bytes);
    Log("BOF3X_EXEIMAGE: .data at 0x%X from base/exe/data.bin (%s, disc-built), %u bytes; the engine's own regions "
        "filled; the key table next (Shell_MapDefaultKeys); every gap zero, as logged (BOF3X_EXEIMAGE_STRICT=1 "
        "refuses them)",
        (unsigned)va, im.build.c_str(), (unsigned)im.size);
    g_mapped = im;
    g_disc_mapped = true;
    return true;
}

bool ExeImage_DiscMapped() { return g_disc_mapped; }

void ExeImage_CompareWithFile() {
    if (!g_disc_mapped) return;
    const Image& im = g_mapped;
    if (!ExeFile_Present()) {
        Log("BOF3X_EXEIMAGE: no BOF3.exe to compare the disc-built image with; recipes/exe-gaps.tsv is what says "
            "where it may differ from the PC's");
        return;
    }
    std::vector<unsigned char> pc(im.size);
    ExeFile_Read(im.va, pc.data(), im.size);
    const auto* live = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(im.va));
    struct Count {
        std::uint32_t ranges, bytes, differ_ranges, differ;
    } by[sizeof kClasses / sizeof kClasses[0]] = {};
    std::uint32_t equal = 0;
    for (const Range& r : im.ranges) {
        const Class* c = ClassOf(r);
        Count& k = by[c - kClasses];
        std::uint32_t differ = 0, first = 0;
        for (std::uint32_t a = r.lo; a < r.hi; ++a)
            if (live[a - im.va] != pc[a - im.va]) {
                if (!differ) first = a;
                ++differ;
            }
        k.ranges += 1;
        k.bytes += r.hi - r.lo;
        equal += (r.hi - r.lo) - differ;
        if (!differ) continue;
        if (c->must_equal)
            Fatal("BOF3X_EXEIMAGE: 0x%06X..0x%06X (%s%s%s) must equal BOF3.exe's .data after the fills, but %u of its "
                  "%u bytes differ, the first at 0x%06X (ours 0x%02X, the file's 0x%02X)",
                  (unsigned)r.lo, (unsigned)r.hi, r.how.c_str(), r.from.empty() ? "" : " ", r.from.c_str(),
                  (unsigned)differ, (unsigned)(r.hi - r.lo), (unsigned)first, (unsigned)live[first - im.va],
                  (unsigned)pc[first - im.va]);
        k.differ_ranges += 1;
        k.differ += differ;
        Log("BOF3X_EXEIMAGE  differs  %-8s 0x%06X..0x%06X %5u of %5u bytes, the first 0x%06X  %s", r.how.c_str(),
            (unsigned)r.lo, (unsigned)r.hi, (unsigned)differ, (unsigned)(r.hi - r.lo), (unsigned)first, r.from.c_str());
    }
    for (std::size_t i = 0; i < sizeof kClasses / sizeof kClasses[0]; ++i)
        if (by[i].ranges)
            Log("BOF3X_EXEIMAGE  against BOF3.exe  %-8s %5u ranges %7u bytes: %5u ranges, %6u bytes differ",
                kClasses[i].how, (unsigned)by[i].ranges, (unsigned)by[i].bytes, (unsigned)by[i].differ_ranges,
                (unsigned)by[i].differ);
    Log("BOF3X_EXEIMAGE: the mapped disc-built image (%s) after the fills against BOF3.exe's .data: %u of %u bytes "
        "equal; every class that must equal does",
        im.build.c_str(), (unsigned)equal, (unsigned)im.size);
}

}  // namespace bof3
