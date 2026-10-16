// BOF3X_EXEIMAGE (exe_image.h; docs/exe-import-engine.md section 4.4).
#include "hook/exe_image.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "common/sha256.h"
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
//   map, place, table, widen       copied (a disc's numbers may differ: logged)
//   exe                            copied; must equal the file's when it is there (only a pc-zh image has it)
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

// Bytes every disc build carries since an importer fix, which an image written
// before that fix left as a `gap` - zero where the engine now reads a value it
// cannot do without. Such an image is refused by name rather than mapped: a
// player's cache from an older release would otherwise run into the defect the
// fix removed (D305: a 2026-10-12 afternoon PSX-US cache crashing on entering
// area 16). A row per fix, when one changes what a disc build must hold.
struct Since {
    std::uint32_t lo, hi;
    const char* what;
};
std::vector<Since> FilledSince() {
    std::vector<Since> out;
    // tools/psp_exe.py wmrec (2026-10-12, docs/visual-findings-1012.md section 8):
    // WorldMap_Records 0x653910, eleven 0x1C-byte records, the world map's area
    // word at +0x18. Zero, WorldMap_RecordIndex finds no world map and answers
    // the "none" record, whose effect kind 1 calls MoveCmd_AttachOffset with
    // area 16's or 45's null vector table.
    for (std::uint32_t k = 0; k < 11; ++k) {
        const std::uint32_t at = 0x653910 + 0x1C * k + 0x18;
        out.push_back({at, at + 4, "WorldMap_Records' area words (the importer's wmrec rule of 2026-10-12)"});
    }
    return out;
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
    for (const Since& s : FilledSince())
        for (const Range& r : im.ranges)
            if (r.how == "gap" && r.lo < s.hi && s.lo < r.hi)
                Fatal("This install's game data was built by an older release and is missing %s, which this "
                      "release needs (it would crash on entering a world map area). Rebuild the install: in the "
                      "launcher's Sources page add the disc again, or run tools/importer.py build.\n\n"
                      "BOF3X_EXEIMAGE: %s (%s) leaves 0x%06X..0x%06X a gap (%s)",
                      s.what, exe_dir.c_str(), im.build.c_str(), (unsigned)r.lo, (unsigned)r.hi, r.from.c_str());
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

// --- DIV-0111: the exe tables layer ----------------------------------------------
namespace {

struct OptRange {
    std::uint32_t lo, hi;
    std::string what;
};
struct OptTables {
    std::string build, file, sha256;
    std::uint32_t bytes = 0;
    std::vector<OptRange> ranges;
};

// place.toml as tools/psp_layers.py build_exe writes it: `key = value` lines,
// then one `[0x.., 0x.., "what"],` line a range. Only that shape; false with
// `why` otherwise.
bool ParseOptTables(const std::string& text, OptTables* out, std::string* why) {
    *out = OptTables{};
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const char* p = line.c_str();
        while (*p == ' ') ++p;
        unsigned a = 0, b = 0;
        char s[128] = {};
        if (std::sscanf(p, "[0x%x, 0x%x, \"%127[^\"]\"]", &a, &b, s) == 3) out->ranges.push_back({a, b, s});
        else if (std::sscanf(p, "build = \"%127[^\"]\"", s) == 1) out->build = s;
        else if (std::sscanf(p, "file = \"%127[^\"]\"", s) == 1) out->file = s;
        else if (std::sscanf(p, "sha256 = \"%127[^\"]\"", s) == 1) out->sha256 = s;
        else if (std::sscanf(p, "bytes = %u", &a) == 1) out->bytes = a;
    }
    if (out->build.empty() || out->file != "place.bin" || out->sha256.size() != 64 || out->ranges.empty()) {
        *why = "not a place.toml tools/psp_layers.py wrote (build, file = \"place.bin\", sha256, ranges)";
        return false;
    }
    return true;
}

// The layer's ranges and bytes against .data [va, va + size), the mapped image
// (when one is: each range's bytes in a class that need not equal the PC's)
// and laid over `data` (va's byte at data[0]). Returns the bytes that changed,
// or -1 with `why`, laying nothing.
long LayOptTables(const OptTables& t, const std::vector<unsigned char>& bin, unsigned char* data, std::uint32_t va,
                  std::uint32_t size, const Image* mapped, std::string* why) {
    char msg[256];
    std::uint32_t total = 0, prev = va;
    for (const OptRange& r : t.ranges) {
        if (r.lo < prev || r.hi <= r.lo || r.hi > va + size) {
            std::snprintf(msg, sizeof msg, "range 0x%X..0x%X (%s) out of order or outside .data 0x%X..0x%X",
                          (unsigned)r.lo, (unsigned)r.hi, r.what.c_str(), (unsigned)va, (unsigned)(va + size));
            *why = msg;
            return -1;
        }
        prev = r.hi;
        total += r.hi - r.lo;
        if (!mapped) continue;
        for (const Range& m : mapped->ranges) {
            if (m.hi <= r.lo || m.lo >= r.hi) continue;
            if (ClassOf(m)->must_equal) {
                std::snprintf(msg, sizeof msg, "range 0x%X..0x%X (%s) lies on 0x%X..0x%X, class %s of the mapped %s "
                              "image, which must equal the PC's", (unsigned)r.lo, (unsigned)r.hi, r.what.c_str(),
                              (unsigned)m.lo, (unsigned)m.hi, m.how.c_str(), mapped->build.c_str());
                *why = msg;
                return -1;
            }
        }
    }
    if (total != bin.size() || total != t.bytes) {
        std::snprintf(msg, sizeof msg, "the ranges hold %u bytes, place.bin %u, place.toml says %u", (unsigned)total,
                      (unsigned)bin.size(), (unsigned)t.bytes);
        *why = msg;
        return -1;
    }
    std::string hex;
    if (!bof3x::Sha256HexBytes(bin.data(), bin.size(), hex) || hex != t.sha256) {
        *why = "place.bin is not the file place.toml's sha256 names";
        return -1;
    }
    long changed = 0;
    std::size_t off = 0;
    for (const OptRange& r : t.ranges)
        for (std::uint32_t a = r.lo; a < r.hi; ++a, ++off) {
            changed += data[a - va] != bin[off];
            data[a - va] = bin[off];
        }
    return changed;
}

bool ReadText(const std::string& path, std::string* out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out->clear();
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out->append(buf, n);
    std::fclose(f);
    return true;
}

}  // namespace

void ExeImage_LayOptTables(void* to, std::uint32_t va, std::uint32_t size) {
    char opt[16 * 24];
    const DWORD n = GetEnvironmentVariableA("BOF3X_OPT", opt, sizeof opt);
    if (n == 0 || n >= sizeof opt) return;   // none, or too long: ReadOptLayers refuses it at injection
    char data_mode[4];
    const DWORD m = GetEnvironmentVariableA("BOF3X_CACHE_DATA", data_mode, sizeof data_mode);
    if (m == 1 && data_mode[0] == '0') return;   // the cache for the music only (DIV-0089)
    char root[MAX_PATH];
    const DWORD r = GetEnvironmentVariableA("BOF3X_CACHE", root, sizeof root);
    if (r == 0 || r >= sizeof root) return;
    std::string base(root, r);
    while (base.size() > 1 && (base.back() == '\\' || base.back() == '/')) base.pop_back();
    std::string list(opt, n);
    std::size_t at = 0;
    while (at <= list.size()) {
        std::size_t end = list.find(',', at);
        if (end == std::string::npos) end = list.size();
        const std::string layer = list.substr(at, end - at);
        at = end + 1;
        // a malformed name is ReadOptLayers' to refuse; only a plain one is a directory here
        if (layer.empty() || layer.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") !=
                                 std::string::npos)
            continue;
        const std::string dir = base + "\\opt\\" + layer + "\\exe\\";
        std::string text;
        if (!ReadText(dir + "place.toml", &text)) continue;   // no exe part
        OptTables t;
        std::string why;
        if (!ParseOptTables(text, &t, &why)) Fatal("DIV-0111: %splace.toml: %s", dir.c_str(), why.c_str());
        FILE* f = std::fopen((dir + t.file).c_str(), "rb");
        if (!f) Fatal("DIV-0111: %s%s does not open", dir.c_str(), t.file.c_str());
        std::vector<unsigned char> bin;
        unsigned char buf[4096];
        std::size_t got;
        while ((got = std::fread(buf, 1, sizeof buf, f)) > 0) bin.insert(bin.end(), buf, buf + got);
        std::fclose(f);
        const long changed = LayOptTables(t, bin, static_cast<unsigned char*>(to), va, size,
                                          g_disc_mapped ? &g_mapped : nullptr, &why);
        if (changed < 0) Fatal("DIV-0111: layer %s (%s): %s", layer.c_str(), dir.c_str(), why.c_str());
        Log("DIV-0111    layer %s: %u ranges, %u bytes of the %s image's .data laid over .data (%s); %ld of them "
            "differ from what was there",
            layer.c_str(), (unsigned)t.ranges.size(), (unsigned)t.bytes, t.build.c_str(),
            g_disc_mapped ? g_mapped.build.c_str() : "the file's or a pc-zh image", changed);
    }
}

void ExeImage_OptSelfTest() {
    // A synthetic .data of 64 bytes at 0x1000 and a layer of two ranges.
    constexpr std::uint32_t kVa = 0x1000, kSize = 64;
    std::vector<unsigned char> bin = {1, 2, 3, 4, 5, 6};
    std::string hex;
    if (!bof3x::Sha256HexBytes(bin.data(), bin.size(), hex)) Fatal("DIV-0111 self-test: no SHA-256");
    const std::string good = "build = \"psp-xx\"\nimage_sha256 = \"x\"\nfile = \"place.bin\"\nsha256 = \"" + hex +
                             "\"\nbytes = 6\n\nranges = [\n  [0x001004, 0x001008, \"a\"],\n  [0x001010, 0x001012, \"b\"],\n]\n";
    OptTables t;
    std::string why;
    std::vector<unsigned char> data(kSize, 0xEE);
    data[0x10] = 5;   // one byte already the layer's
    const bool parsed = ParseOptTables(good, &t, &why) && t.ranges.size() == 2 && t.bytes == 6;
    const long changed = parsed ? LayOptTables(t, bin, data.data(), kVa, kSize, nullptr, &why) : -1;
    bool laid = changed == 5;
    for (std::uint32_t i = 0; i < kSize; ++i) {
        const int want = i >= 4 && i < 8 ? int(i - 3) : i == 0x10 ? 5 : i == 0x11 ? 6 : 0xEE;
        laid = laid && data[i] == want;
    }
    // The controls, each refused with nothing laid.
    unsigned refused = 0;
    const auto refuse = [&](std::string text, const std::vector<unsigned char>& b, const Image* mapped) {
        OptTables u;
        std::string w;
        std::vector<unsigned char> d(kSize, 0xEE);
        const bool no = !ParseOptTables(text, &u, &w) || LayOptTables(u, b, d.data(), kVa, kSize, mapped, &w) < 0;
        refused += no && d == std::vector<unsigned char>(kSize, 0xEE);
    };
    std::vector<unsigned char> other = bin;
    other[0] ^= 1;
    refuse(good, other, nullptr);                                                   // a wrong hash
    std::string outside = good;
    outside.replace(outside.find("0x001010, 0x001012"), 18, "0x001040, 0x001042");
    refuse(outside, bin, nullptr);                                                  // outside .data
    std::string order = good;
    order.replace(order.find("0x001010, 0x001012"), 18, "0x001002, 0x001004");
    refuse(order, bin, nullptr);                                                    // out of order
    std::string count = good;
    count.replace(count.find("bytes = 6"), 9, "bytes = 7");
    refuse(count, bin, nullptr);                                                    // a byte count that differs
    Image im;
    im.build = "psx-xx";
    im.ranges = {{0x1000, 0x1010, "map", ""}, {0x1010, 0x1040, "layout", ""}};
    refuse(good, bin, &im);                                                         // on a must-equal class
    im.ranges[1].how = "table";
    std::vector<unsigned char> d2(kSize, 0xEE);
    OptTables v;
    const bool over_disc = ParseOptTables(good, &v, &why) && LayOptTables(v, bin, d2.data(), kVa, kSize, &im, &why) == 6;
    if (!parsed || !laid || refused != 5 || !over_disc)
        Fatal("DIV-0111 self-test: parse %s, laid %s (%ld changed), over a disc image's table class %s, %u of 5 controls "
              "refused", parsed ? "ok" : "WRONG", laid ? "ok" : "WRONG", changed, over_disc ? "ok" : "WRONG", refused);
    Log("shadow      DIV-0111 self-test: a place.toml parsed, its two ranges laid (5 bytes changed, one already the "
        "layer's), and over a disc image's table class; 5 controls refused with nothing laid (a wrong hash, a range "
        "outside .data, ranges out of order, a byte count that differs, a range on a must-equal class)");
}

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
