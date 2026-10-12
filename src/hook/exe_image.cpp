// BOF3X_EXEIMAGE (exe_image.h; docs/exe-import-engine.md section 4.4).
#include "hook/exe_image.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "hook/exe_layout.h"
#include "hook/exe_own.h"
#include "hook/log.h"

namespace bof3 {
namespace {

struct Range {
    std::uint32_t lo, hi;
    std::string how;
};

struct Image {
    std::uint32_t va = 0, size = 0, bss_end = 0;
    std::string build, file;
    std::vector<Range> ranges;
};

// data.toml as tools/exe_tables.py writes it: `key = value` lines in [image],
// then one `[0x.., 0x.., "how", "from"],` line a range. Only that shape.
Image ReadToml(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) Fatal("BOF3X_EXEIMAGE: cannot open %s", path.c_str());
    Image im;
    char line[1024];
    while (std::fgets(line, sizeof line, f)) {
        unsigned a = 0, b = 0;
        char s[64] = {0};
        const char* p = line;
        while (*p == ' ') ++p;
        if (std::sscanf(p, "[0x%x, 0x%x, \"%63[^\"]\"", &a, &b, s) == 3) {
            im.ranges.push_back({a, b, s});
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
    Count by[] = {{"exe", 0, 0}, {"map", 0, 0}, {"place", 0, 0}, {"table", 0, 0}, {"widen", 0, 0},
                  {"rebuilt", 0, 0}, {"pointer", 0, 0}, {"none", 0, 0}};
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
            "(a disc-built image leaves pointer words and what no disc carries unfilled)",
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
    // A disc-built image leaves the code pointers and what no disc carries
    // unfilled (docs/exe-import-engine.md section 4.3): not runnable.
    if (im.build != "pc-zh")
        Fatal("BOF3X_EXEIMAGE: %s is built from %s, not from BOF3.exe (pc-zh): a disc-built image is not runnable",
              exe_dir.c_str(), im.build.c_str());
    std::uint32_t covered = va;
    for (const Range& r : im.ranges) {
        if (r.lo != covered || r.hi <= r.lo || r.hi > va + size)
            Fatal("BOF3X_EXEIMAGE: data.toml's ranges do not cover the image in order at 0x%X", (unsigned)r.lo);
        covered = r.hi;
    }
    if (covered != va + size)
        Fatal("BOF3X_EXEIMAGE: data.toml's ranges end at 0x%X, not 0x%X", (unsigned)covered, (unsigned)(va + size));
    const std::vector<unsigned char> bin = ReadBin(exe_dir + im.file, im.size);
    std::memcpy(to, bin.data(), im.size);
    // The bytes no disc carries that are the engine's own (exe_own.h): checked
    // against the image before they are written, so that a pc-zh image proves
    // them; over a pc-zh image the fill changes nothing.
    ExeOwn_Verify(bin.data(), "base/exe/data.bin (pc-zh)");
    ExeOwn_Fill(static_cast<std::uint8_t*>(to));
    Log("BOF3X_EXEIMAGE: .data at 0x%X from base/exe/data.bin (pc-zh), %u bytes: the source in the exe host",
        (unsigned)va, (unsigned)im.size);
    return true;
}

}  // namespace bof3
