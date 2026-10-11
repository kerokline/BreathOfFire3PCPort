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
//   header   "BOF3SH3\0", u32 base, u32 pages, u32 page size, u32 skip ranges,
//            u32 skip list hash (FNV-1a over each range's address and length
//            in the list's order; 0 without a list), u32 flags (bit 0: our
//            pointers normalised, below; 0 under BOF3X_STATEHASH_RAW=1)
//   record   u32 tick, u32 Frame_Counter, u32 recipe frame, u32 n,
//            then n times (u32 page index, u32 hash)
//
// A record lists only the pages whose hash differs from the record before;
// the first lists every page. A tick is one logic frame seen by the latch,
// counted from the first. "BOF3SH1\0" (until 2026-10-05) had no skip list
// hash: two runs under different lists of one length compared without a
// word (round fourteen's review, item 3). "BOF3SH2\0" (until 2026-10-10) had
// no flags and never normalised. The tool reads all three.
//
// The normalisation (docs/state-hash.md section 7). A language overlay
// re-aims Capcom's pointer tables at text in our DLL, whose addresses move
// with every build: its base (the DLL is ASLR'd) and its layout both. So a
// word is hashed by what it names rather than where: an aligned word of .data
// that held a pointer into BOF3.exe's image when the loader mapped it - one
// of Capcom's pointer slots, recorded before InjectAll - and now holds a
// value inside one of our non-discardable sections is hashed as 0x80000000 |
// the FNV-1a of the bytes it points at, up to the NUL (at most 256, within
// the section), or as kTagCode for a target in code, and the word's index is
// mixed into the page's hash after, so the substitute cannot be mistaken for
// the same value stored raw. Any other word is hashed raw: the game's data is
// full of values that fall inside our 94 MB image (thousands of words a
// frame, measured), so a scan of every word by range would rename game data.
// A page with no normalised word hashes exactly as BOF3SH2 did. The raw dumps
// keep the raw bytes.

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

// BOF3.exe's image from its PE header: ImageBase 0x400000, SizeOfImage
// 0x53F000 (VerifyImage has checked the timestamp before any of this runs).
// A word of .data holding a value in it when the loader has mapped the image
// is a pointer slot of Capcom's: a table entry, a record's string pointer.
constexpr std::uint32_t kExeLo = 0x400000;
constexpr std::uint32_t kExeHi = 0x93F000;

// What a normalised word is hashed as when its target has no content to name
// it by: code. Which of our functions a table names has no build-independent
// identity without symbols; the same-build comparison against Capcom's side
// still sees the entry (his address against ours).
constexpr std::uint32_t kTagCode = 0x80000000u;
constexpr std::uint32_t kContentMax = 256;

// A section of ours a pointer can name: every one the loader keeps. The
// discardable ones (.reloc and the DWARF sections, 41 of the image's 94 MB in
// the build of 2026-10-10) are mapped but nothing points into them, and a
// language overlay's text written in place over one of Capcom's slots can
// look like an address there (docs/state-hash.md section 7: "Para" at
// 0x65D1D0), so a value inside one is hashed raw.
struct Section {
    std::uint32_t lo, hi;
    bool content;   // readable and not executable: hashed by its bytes
};

bool g_census = false;           // BeforeInjects ran (BOF3X_STATEHASH was set)
bool g_normalise = false;        // and BOF3X_STATEHASH_RAW was not 1
std::uint32_t g_dll_lo = 0, g_dll_hi = 0;      // the whole image, for the census
std::uint32_t g_named_lo = 0, g_named_hi = 0;  // the span of g_sections, a quick reject
std::vector<Section> g_sections;
std::vector<std::uint32_t> g_sites;        // byte offsets from kBase of Capcom's pointer slots, ascending
std::vector<std::uint32_t> g_page_sites;   // kPages + 1: page i's slots are [g_page_sites[i], g_page_sites[i + 1])
std::uint32_t g_last_normalised = 0xFFFFFFFFu;
unsigned g_count_lines = 0;

inline std::uint32_t WordAt(std::uint32_t va) {
    std::uint32_t w;
    std::memcpy(&w, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(va)), 4);
    return w;
}

inline bool InDll(std::uint32_t v) { return v >= g_dll_lo && v < g_dll_hi; }

// The section of ours a value names, or null.
const Section* Named(std::uint32_t v) {
    if (v < g_named_lo || v >= g_named_hi) return nullptr;
    for (const Section& s : g_sections)
        if (v >= s.lo && v < s.hi) return &s;
    return nullptr;
}

// What a word holding a pointer of ours is hashed as (the file's comment):
// the target's bytes up to its NUL, so the same text in two builds hashes
// alike wherever the linker put it.
std::uint32_t Identity(const Section& s, std::uint32_t target) {
    if (!s.content) return kTagCode;
    std::uint32_t h = 2166136261u;
    const auto* p = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(target));
    const std::uint32_t room = s.hi - target < kContentMax ? s.hi - target : kContentMax;
    for (std::uint32_t i = 0; i < room; ++i) {
        h = (h ^ p[i]) * 16777619u;
        if (p[i] == 0) break;
    }
    h &= 0x7FFFFFFFu;
    return 0x80000000u | (h == 0 ? 1 : h);   // never the code tag
}

void ReadDllImage(void* module) {
    const auto base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
    const auto* dos = static_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) Fatal("statehash: our module at 0x%08lX has no MZ header", (unsigned long)base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(static_cast<std::uintptr_t>(base + dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE) Fatal("statehash: our module at 0x%08lX has no PE header", (unsigned long)base);
    g_dll_lo = base;
    g_dll_hi = base + nt->OptionalHeader.SizeOfImage;
    g_named_lo = g_dll_hi;
    g_named_hi = g_dll_lo;
    const IMAGE_SECTION_HEADER* s = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++s) {
        if (s->Characteristics & IMAGE_SCN_MEM_DISCARDABLE) continue;
        const std::uint32_t size = s->Misc.VirtualSize ? s->Misc.VirtualSize : s->SizeOfRawData;
        const bool content = (s->Characteristics & IMAGE_SCN_MEM_READ) && !(s->Characteristics & IMAGE_SCN_MEM_EXECUTE);
        const std::uint32_t lo = base + s->VirtualAddress, hi = lo + size;
        g_sections.push_back({lo, hi, content});
        if (lo < g_named_lo) g_named_lo = lo;
        if (hi > g_named_hi) g_named_hi = hi;
    }
    if (g_sections.empty())
        Fatal("statehash: our module at 0x%08lX has no section a pointer can name", (unsigned long)base);
}

// The census: every aligned word of .data whose value lies in our image; of
// those, the ones in a section a pointer can name; of those, the ones in
// Capcom's pointer slots - what the hash normalises - each named.
void Census(const char* when) {
    std::uint32_t all = 0, named = 0;
    std::vector<std::uint32_t> listed;
    std::size_t k = 0;
    for (std::uint32_t off = 0; off < kEnd - kBase; off += 4) {
        while (k < g_sites.size() && g_sites[k] < off) ++k;
        const std::uint32_t w = WordAt(kBase + off);
        if (!InDll(w)) continue;
        ++all;
        if (!Named(w)) continue;
        ++named;
        if (k < g_sites.size() && g_sites[k] == off) listed.push_back(off);
    }
    Log("statehash   census %s: %lu aligned words of .data lie in our image 0x%08lX..0x%08lX, %lu of them in a "
        "section a pointer can name 0x%08lX..0x%08lX, %lu of those in Capcom's pointer slots (normalised)",
        when, (unsigned long)all, (unsigned long)g_dll_lo, (unsigned long)g_dll_hi, (unsigned long)named,
        (unsigned long)g_named_lo, (unsigned long)g_named_hi, (unsigned long)listed.size());
    for (std::size_t i = 0; i < listed.size() && i < 32; ++i)
        Log("statehash     slot 0x%06lX = 0x%08lX", (unsigned long)(kBase + listed[i]),
            (unsigned long)WordAt(kBase + listed[i]));
    if (listed.size() > 32) Log("statehash     and %lu more slots", (unsigned long)(listed.size() - 32));
}

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

constexpr const char* kSpace = " \t\r\n";

void LoadSkips(const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) Fatal("BOF3X_STATEHASH_SKIP: cannot open %s", path);
    char line[512];
    int n = 0;
    while (std::fgets(line, sizeof line, f)) {
        ++n;
        if (char* hash = std::strchr(line, '#')) *hash = 0;
        if (line[std::strspn(line, kSpace)] == 0) continue;   // blank or comment
        char* end = nullptr;
        const unsigned long at = std::strtoul(line, &end, 0);
        char* end2 = nullptr;
        const unsigned long len = end == line ? 0 : std::strtoul(end, &end2, 0);
        if (end == line || end2 == end || len == 0 || end2[std::strspn(end2, kSpace)] != 0)
            Fatal("BOF3X_STATEHASH_SKIP: %s line %d wants ADDRESS LENGTH", path, n);
        if (at < kBase || at >= kEnd || len > kEnd - at)   // the difference: a huge length cannot wrap
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

void StateHash_BeforeInjects(void* dll_module) {
    if (GetEnvironmentVariableA("BOF3X_STATEHASH", nullptr, 0) == 0) return;
    char raw[8];
    const DWORD rn = GetEnvironmentVariableA("BOF3X_STATEHASH_RAW", raw, sizeof raw);
    if (rn >= sizeof raw || (rn > 0 && std::strcmp(raw, "0") != 0 && std::strcmp(raw, "1") != 0))
        Fatal("BOF3X_STATEHASH_RAW: wants 1 (hash our pointers raw, as BOF3SH2 did) or 0");
    g_normalise = !(rn > 0 && raw[0] == '1');
    ReadDllImage(dll_module);
    // The identities have bit 31 set. Neither image reaches it here, so no
    // identity equals a pointer into either; game data can hold any value,
    // which is why the word's index is mixed into the page hash as well.
    Log("statehash   our image 0x%08lX..0x%08lX, %u sections a pointer can name in 0x%08lX..0x%08lX; BOF3.exe's "
        "0x%06lX..0x%06lX; identities 0x80000000.. %s both",
        (unsigned long)g_dll_lo, (unsigned long)g_dll_hi, (unsigned)g_sections.size(), (unsigned long)g_named_lo,
        (unsigned long)g_named_hi, (unsigned long)kExeLo, (unsigned long)kExeHi,
        g_dll_hi <= 0x80000000u ? "lie above" : "OVERLAP our image - the index mix still keeps them apart -");
    g_page_sites.assign(kPages + 1, 0);
    for (std::uint32_t off = 0; off < kEnd - kBase; off += 4) {
        const std::uint32_t w = WordAt(kBase + off);
        if (w >= kExeLo && w < kExeHi) g_sites.push_back(off);
    }
    std::size_t k = 0;
    for (std::uint32_t i = 0; i <= kPages; ++i) {
        while (k < g_sites.size() && g_sites[k] < i * kPage) ++k;
        g_page_sites[i] = static_cast<std::uint32_t>(k);
    }
    g_census = true;
    Log("statehash   %lu aligned words of .data hold a pointer into BOF3.exe's image as mapped: Capcom's pointer "
        "slots; %s",
        (unsigned long)g_sites.size(),
        g_normalise ? "a slot holding a pointer into our image is hashed by what it names"
                    : "BOF3X_STATEHASH_RAW=1: hashed raw, as BOF3SH2 did");
    Census("before the injects");
}

void StateHash_AfterInjects() {
    if (!g_census) return;
    Census("after the injects");
}

bool StateHash_Start() {
    char path[MAX_PATH];
    const DWORD n = GetEnvironmentVariableA("BOF3X_STATEHASH", path, sizeof path);
    if (n == 0) return false;
    if (n >= sizeof path) Fatal("BOF3X_STATEHASH: the path is %lu characters, over MAX_PATH", (unsigned long)n);
    g_path = path;
    if (!g_census) Fatal("BOF3X_STATEHASH: the pointer slots were not recorded before the injects (DllMain)");
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
    std::fwrite("BOF3SH3", 1, 8, g_out);
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
    Put(g_normalise ? 1u : 0u);
    g_last.assign(kPages, 0);
    Log("statehash   %lu pages of .data from 0x%lX, a record a logic frame to %s; %u skip ranges, %u dump ticks, "
        "our pointers %s",
        static_cast<unsigned long>(kPages), static_cast<unsigned long>(kBase), path, (unsigned)g_skips.size(),
        (unsigned)g_dump_ticks.size(), g_normalise ? "normalised" : "raw (BOF3X_STATEHASH_RAW=1)");
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
    std::uint32_t normalised = 0;
    for (std::uint32_t i = 0; i < kPages; ++i) {
        const unsigned char* const page = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(kBase + i * kPage));
        const unsigned char* p = page;
        if (!g_page_skips[i].empty()) {
            std::memcpy(masked, p, kPage);
            for (const Skip& s : g_page_skips[i]) std::memset(masked + s.at, 0, s.len);
            p = masked;
        }
        // Capcom's pointer slots on this page that hold a pointer of ours,
        // after the skip list (a masked slot is 0, never ours): each hashed
        // by what it names, and its index mixed in after.
        std::uint32_t mix = 0;
        std::uint32_t mixes[kPage / 4];
        if (g_normalise) {
            for (std::uint32_t k = g_page_sites[i]; k < g_page_sites[i + 1]; ++k) {
                const std::uint32_t at = g_sites[k] - i * kPage;
                std::uint32_t w;
                std::memcpy(&w, p + at, 4);
                const Section* named = Named(w);
                if (!named) continue;
                if (p == page) {
                    std::memcpy(masked, page, kPage);
                    p = masked;
                }
                const std::uint32_t id = Identity(*named, w);
                std::memcpy(masked + at, &id, 4);
                mixes[mix++] = at / 4;
            }
            normalised += mix;
        }
        std::uint32_t h = HashPage(p);
        for (std::uint32_t m = 0; m < mix; ++m) h = (h ^ (0x9E3779B9u + mixes[m])) * 16777619u;
        if (g_first || h != g_last[i]) {
            g_last[i] = h;
            changed.push_back(i);
        }
    }
    g_first = false;
    // How many words were normalised: a line when the number changes, the
    // first 64 times, so a run's log says when the overlay's tables arrived.
    if (g_normalise && normalised != g_last_normalised && g_count_lines < 64) {
        ++g_count_lines;
        Log("statehash   tick %lu: %lu of Capcom's pointer slots hold a pointer of ours, hashed by what they name%s",
            static_cast<unsigned long>(g_tick), static_cast<unsigned long>(normalised),
            g_count_lines == 64 ? " (the last such line)" : "");
    }
    g_last_normalised = normalised;
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

void StateHash_Flush() {
    if (g_out) std::fflush(g_out);
}

}  // namespace bof3
