#include "hook/statehash.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "hook/input_script.h"
#include "hook/log.h"

// The file (little-endian, read by tools/statehash.py):
//
//   header   "BOF3SH2\0", u32 base, u32 pages, u32 page size, u32 skip ranges,
//            u32 skip list hash (FNV-1a over each range's address and length
//            in the list's order; 0 without a list)
//   record   u32 tick, u32 Frame_Counter, u32 recipe frame, u32 n,
//            then n times (u32 page index, u32 hash)
//
// A record lists only the pages whose hash differs from the record before;
// the first lists every page. A tick is one logic frame seen by the latch,
// counted from the first. "BOF3SH1\0" (until 2026-10-05) had no skip list
// hash: two runs under different lists of one length compared without a
// word (round fourteen's review, item 3). The tool reads both.

namespace bof3 {
namespace {

// BOF3.exe's .data, from its section header: 0x5DA000, virtual size 0x3636EC,
// which the loader maps to the end of its last page.
constexpr std::uint32_t kBase = 0x5DA000;
constexpr std::uint32_t kEnd = 0x93E000;
constexpr std::uint32_t kPage = 0x1000;
constexpr std::uint32_t kPages = (kEnd - kBase) / kPage;

struct Skip {
    std::uint32_t at, len;
};

FILE* g_out = nullptr;
std::string g_path;
std::vector<Skip> g_skips;
std::vector<std::vector<Skip>> g_page_skips;   // per page, offsets within it
std::vector<std::uint32_t> g_last;
std::vector<std::uint32_t> g_dump_ticks;
bool g_first = true;
bool g_seen = false;
std::uint32_t g_last_frame = 0;
std::uint32_t g_tick = 0;

std::uint32_t HashPage(const unsigned char* p) {
    std::uint64_t h = 0x9E3779B97F4A7C15ull;
    for (std::uint32_t i = 0; i < kPage; i += 8) {
        std::uint64_t w;
        std::memcpy(&w, p + i, 8);
        h = (h ^ w) * 0x9E3779B97F4A7C15ull;
        h ^= h >> 29;
    }
    return static_cast<std::uint32_t>(h ^ (h >> 32));
}

void Put(std::uint32_t v) { std::fwrite(&v, 4, 1, g_out); }

void LoadSkips(const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) Fatal("BOF3X_STATEHASH_SKIP: cannot open %s", path);
    char line[512];
    int n = 0;
    while (std::fgets(line, sizeof line, f)) {
        ++n;
        if (char* hash = std::strchr(line, '#')) *hash = 0;
        char* end = nullptr;
        const unsigned long at = std::strtoul(line, &end, 0);
        if (end == line) continue;   // blank or comment
        char* end2 = nullptr;
        const unsigned long len = std::strtoul(end, &end2, 0);
        if (end2 == end || len == 0) Fatal("BOF3X_STATEHASH_SKIP: %s line %d wants ADDRESS LENGTH", path, n);
        if (at < kBase || at + len > kEnd)
            Fatal("BOF3X_STATEHASH_SKIP: %s line %d: 0x%lX + 0x%lX is outside .data", path, n, at, len);
        g_skips.push_back({static_cast<std::uint32_t>(at), static_cast<std::uint32_t>(len)});
    }
    std::fclose(f);
    g_page_skips.assign(kPages, {});
    for (const Skip& s : g_skips) {
        for (std::uint32_t a = s.at; a < s.at + s.len;) {
            const std::uint32_t page = (a - kBase) / kPage;
            const std::uint32_t page_end = kBase + (page + 1) * kPage;
            const std::uint32_t stop = s.at + s.len < page_end ? s.at + s.len : page_end;
            g_page_skips[page].push_back({a - (kBase + page * kPage), stop - a});
            a = stop;
        }
    }
}

void Dump() {
    char path[MAX_PATH + 32];
    std::snprintf(path, sizeof path, "%s.%lu.bin", g_path.c_str(), static_cast<unsigned long>(g_tick));
    FILE* f = std::fopen(path, "wb");
    if (!f) Fatal("BOF3X_STATEHASH_DUMP: cannot open %s for writing", path);
    std::fwrite(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(kBase)), 1, kEnd - kBase, f);
    std::fclose(f);
    Log("statehash   tick %lu (Frame_Counter %lu): .data dumped to %s", static_cast<unsigned long>(g_tick),
        static_cast<unsigned long>(Frame_Counter), path);
}

}  // namespace

bool StateHash_Start() {
    char path[MAX_PATH];
    const DWORD n = GetEnvironmentVariableA("BOF3X_STATEHASH", path, sizeof path);
    if (n == 0) return false;
    if (n >= sizeof path) Fatal("BOF3X_STATEHASH: the path is %lu characters, over MAX_PATH", (unsigned long)n);
    g_path = path;
    char skip[MAX_PATH];
    const DWORD sn = GetEnvironmentVariableA("BOF3X_STATEHASH_SKIP", skip, sizeof skip);
    if (sn >= sizeof skip) Fatal("BOF3X_STATEHASH_SKIP: the path is over MAX_PATH");
    if (sn > 0) LoadSkips(skip);
    if (g_page_skips.empty()) g_page_skips.assign(kPages, {});
    char dump[256];
    const DWORD dn = GetEnvironmentVariableA("BOF3X_STATEHASH_DUMP", dump, sizeof dump);
    if (dn >= sizeof dump) Fatal("BOF3X_STATEHASH_DUMP: the list is over %u characters", (unsigned)sizeof dump);
    if (dn > 0) {
        for (char* p = dump; *p;) {
            char* end = nullptr;
            const unsigned long t = std::strtoul(p, &end, 0);
            if (end == p) Fatal("BOF3X_STATEHASH_DUMP: wants ticks separated by commas, got '%s'", dump);
            g_dump_ticks.push_back(static_cast<std::uint32_t>(t));
            p = *end == ',' ? end + 1 : end;
            if (*end && *end != ',') Fatal("BOF3X_STATEHASH_DUMP: wants ticks separated by commas, got '%s'", dump);
        }
    }
    g_out = std::fopen(path, "wb");
    if (!g_out) Fatal("BOF3X_STATEHASH: cannot open %s for writing", path);
    std::setvbuf(g_out, nullptr, _IOFBF, 1 << 20);
    std::fwrite("BOF3SH2", 1, 8, g_out);
    Put(kBase);
    Put(kPages);
    Put(kPage);
    Put(static_cast<std::uint32_t>(g_skips.size()));
    std::uint32_t skip_hash = 0;
    if (!g_skips.empty()) {
        skip_hash = 2166136261u;
        for (const Skip& s : g_skips)
            for (const std::uint32_t w : {s.at, s.len})
                for (int b = 0; b < 4; ++b) skip_hash = (skip_hash ^ ((w >> (8 * b)) & 0xFF)) * 16777619u;
    }
    Put(skip_hash);
    g_last.assign(kPages, 0);
    Log("statehash   %lu pages of .data from 0x%lX, a record a logic frame to %s; %u skip ranges, %u dump ticks",
        static_cast<unsigned long>(kPages), static_cast<unsigned long>(kBase), path, (unsigned)g_skips.size(),
        (unsigned)g_dump_ticks.size());
    return true;
}

void StateHash_Tick() {
    if (!g_out) return;
    const std::uint32_t frame = Frame_Counter;
    if (g_seen && frame == g_last_frame) return;
    g_seen = true;
    g_last_frame = frame;

    static std::vector<std::uint32_t> changed;
    changed.clear();
    unsigned char masked[kPage];
    for (std::uint32_t i = 0; i < kPages; ++i) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(kBase + i * kPage));
        if (!g_page_skips[i].empty()) {
            std::memcpy(masked, p, kPage);
            for (const Skip& s : g_page_skips[i]) std::memset(masked + s.at, 0, s.len);
            p = masked;
        }
        const std::uint32_t h = HashPage(p);
        if (g_first || h != g_last[i]) {
            g_last[i] = h;
            changed.push_back(i);
        }
    }
    g_first = false;
    Put(g_tick);
    Put(frame);
    Put(InputScript_Frame());
    Put(static_cast<std::uint32_t>(changed.size()));
    for (std::uint32_t i : changed) {
        Put(i);
        Put(g_last[i]);
    }
    for (std::uint32_t t : g_dump_ticks)
        if (t == g_tick) Dump();
    // The runners end the game with taskkill: nothing flushes at exit.
    if ((g_tick & 0x3F) == 0) std::fflush(g_out);
    ++g_tick;
}

}  // namespace bof3
