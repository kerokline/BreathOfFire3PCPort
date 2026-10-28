#include "hook/statehash.h"

#include <windows.h>

#include <cstdint>
#include <algorithm>
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
//   header   "BOF3SH4\0", u32 base, u32 pages, u32 page size, u32 skip ranges,
//            u32 skip list hash (FNV-1a over each range's address and length
//            in the list's order; 0 without a list), u32 flags (bit 0: our
//            pointers normalised, below; 0 under BOF3X_STATEHASH_RAW=1;
//            bit 1: the run-time slots below are normalised too, 2026-10-11),
//            u32 image base (where the game's image lay: 0x400000 in every
//            host until the relocatable image), u32 fold base (the
//            BOF3X_STATEHASH_FOLD address, 0 without one)
//   record   u32 tick, u32 Frame_Counter, u32 recipe frame, u32 n,
//            then n times (u32 page index, u32 hash)
//
// A record lists only the pages whose hash differs from the record before;
// the first lists every page. A tick is one logic frame seen by the latch,
// counted from the first. "BOF3SH1\0" (until 2026-10-05) had no skip list
// hash: two runs under different lists of one length compared without a
// word (round fourteen's review, item 3). "BOF3SH2\0" (until 2026-10-10) had
// no flags and never normalised. "BOF3SH3\0" (until 2026-10-28) had no
// bases: every run had the image at 0x400000 and folded nothing, which is
// what a BOF3SH4 run with base 0x400000 and no fold hashes, bit for bit. The
// tool reads all four.
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
//
// The image's own pointers (docs/state-hash.md section 8, 2026-10-28). Once
// the image can lie elsewhere than 0x400000 (IDEAS I44 stage 3), every
// pointer into it - the 18,831 words of recipes/exe-pointers.tsv the loader
// rebases, and the ones the game stores at run time in the arena and its
// records (895 to 3,144 aligned words outside the load-time slots in the raw
// dumps measured that day) - holds base + offset. Each such word is hashed as
// 0x400000 + offset: a value in a folded range [F, F + 0x53F000) is replaced
// by 0x400000 + (value - F), on every aligned word of the page, slot or not.
// The folded ranges are the image's own base when it is not 0x400000 and the
// BOF3X_STATEHASH_FOLD address. Game data holds values in any such range (no
// 5 MB window below 0x80000000 is free of them across the 219 raw dumps of
// analysis/statehash), so a run at 0x400000 folds the moved run's range too,
// with BOF3X_STATEHASH_FOLD set to it: then a data word in that range is
// folded alike on both sides, and a pointer reads 0x400000 + offset on both.
// No index is mixed in for a folded word, so that a pointer folded on one
// side hashes as the same pointer stored at 0x400000 on the other; and with
// no folded range - every host today, BOF3X_STATEHASH_FOLD unset - the hash
// is BOF3SH3's, bit for bit.

namespace bof3 {
namespace {

// BOF3.exe's .data, from its section header: 0x5DA000, virtual size 0x3636EC,
// which the loader maps to the end of its last page. Addresses as at the
// image's own base 0x400000 (the skip list's, the file's base, the run-time
// slots'); g_data is where .data is read.
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
constexpr std::uint32_t kImageSize = kExeHi - kExeLo;

// Where the image lies (StateHash_SetImageBase), where its .data is read, and
// the folded ranges: the image's base when it is not kExeLo, and the
// BOF3X_STATEHASH_FOLD address (the file's comment). At most two.
std::uint32_t g_image = kExeLo;
std::uint32_t g_data = kBase;
std::uint32_t g_fold_env = 0;
std::uint32_t g_folds[2];
unsigned g_nfolds = 0;

// What a normalised word is hashed as when its target has no content to name
// it by: code. Which of our functions a table names has no build-independent
// identity without symbols; the same-build comparison against Capcom's side
// still sees the entry (his address against ours).
constexpr std::uint32_t kTagCode = 0x80000000u;
constexpr std::uint32_t kContentMax = 256;

// Words Capcom's code fills at run time with Msg_SystemPtr's result, zero
// when the census runs and so not slots of his: the battle message ring
// 0x93C2C4 (16 entries of 8 bytes, +0 the text pointer; battle_actor_copies.cpp
// queues the item under the cursor's description there) and the battle
// result's current message 0x93B8E4 (battle_result.cpp). Under a language
// overlay the pointer names MsgPool_Relocate's static pool in our DLL
// (DIV-0007), at the same offset in every build and a different address:
// two builds of ours on the combat route differed on those two pages from
// the first battle message on (docs/state-hash.md section 7, measured 3,
// 2026-10-11), so they join the slots. Without an overlay they hold a
// pointer into MessagePools in BOF3.exe, hashed raw as before.
constexpr std::uint32_t kMsgRing = 0x93C2C4;
constexpr std::uint32_t kMsgRingEntries = 16;
constexpr std::uint32_t kMsgRingStride = 8;
constexpr std::uint32_t kMsgCurrent = 0x93B8E4;

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

// A value in a folded range, as it would read with the image at 0x400000.
inline bool Fold(const std::uint32_t* folds, unsigned n, std::uint32_t v, std::uint32_t* out) {
    for (unsigned i = 0; i < n; ++i)
        if (v - folds[i] < kImageSize) {
            *out = kExeLo + (v - folds[i]);
            return true;
        }
    return false;
}

// A Capcom slot is a word whose value at the census lies in the image: at
// 0x400000, or in a folded range (where a moved image's rebased words lie; a
// run at 0x400000 folding that range lists the same words, so both sides of
// a moved pair test the same slots).
inline bool InImage(std::uint32_t v) {
    std::uint32_t c;
    return (v >= kExeLo && v < kExeHi) || Fold(g_folds, g_nfolds, v, &c);
}

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
        const std::uint32_t w = WordAt(g_data + off);
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
            (unsigned long)WordAt(g_data + listed[i]));
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

// One page as the file records it: the skip ranges zeroed; Capcom's slots
// that hold a pointer of ours hashed by what they name (with `dll`); every
// aligned word in a folded range as at 0x400000; then the hash, with each
// named slot's index mixed in after. `sites` are the slots' byte offsets
// within the page, `scratch` a page of room; `normalised` counts the slots
// that named ours. Shared by the tick and the self-test.
std::uint32_t HashNormalised(const unsigned char* page, const std::vector<Skip>& skips, const std::uint32_t* sites,
                             std::uint32_t nsites, bool dll, const std::uint32_t* folds, unsigned nfolds,
                             unsigned char* scratch, std::uint32_t* normalised) {
    const unsigned char* p = page;
    if (!skips.empty()) {
        std::memcpy(scratch, p, kPage);
        for (const Skip& s : skips) std::memset(scratch + s.at, 0, s.len);
        p = scratch;
    }
    // Capcom's pointer slots on this page that hold a pointer of ours, after
    // the skip list (a masked slot is 0, never ours): each hashed by what it
    // names, and its index mixed in after.
    std::uint32_t mix = 0;
    std::uint32_t mixes[kPage / 4];
    if (dll) {
        for (std::uint32_t k = 0; k < nsites; ++k) {
            const std::uint32_t at = sites[k];
            std::uint32_t w;
            std::memcpy(&w, p + at, 4);
            const Section* named = Named(w);
            if (!named) continue;
            if (p == page) {
                std::memcpy(scratch, page, kPage);
                p = scratch;
            }
            const std::uint32_t id = Identity(*named, w);
            std::memcpy(scratch + at, &id, 4);
            mixes[mix++] = at / 4;
        }
        *normalised += mix;
    }
    // The image's pointers, as at 0x400000. An identity has bit 31 set and a
    // folded range ends at or below 0x80000000, so no identity is folded; our
    // image lies in no folded range (StateHash_BeforeInjects), so no pointer
    // of ours is either.
    if (nfolds) {
        for (std::uint32_t at = 0; at < kPage; at += 4) {
            std::uint32_t w, c;
            std::memcpy(&w, p + at, 4);
            if (!Fold(folds, nfolds, w, &c)) continue;
            if (p == page) {
                std::memcpy(scratch, page, kPage);
                p = scratch;
            }
            std::memcpy(scratch + at, &c, 4);
        }
    }
    std::uint32_t h = HashPage(p);
    for (std::uint32_t m = 0; m < mix; ++m) h = (h ^ (0x9E3779B9u + mixes[m])) * 16777619u;
    return h;
}

// A base for the image or a folded range: 64 KiB aligned, its whole extent
// in the low 2 GB (an identity has bit 31 set), and clear of the image's own
// range at 0x400000 unless it is that base.
void CheckImageBase(const char* what, unsigned long long b) {
    if ((b & 0xFFFF) || b == 0 || b + kImageSize > 0x80000000ull)
        Fatal("%s: 0x%llX is not a 64 KiB-aligned base whose 0x%lX-byte image ends at or below 0x80000000", what, b,
              (unsigned long)kImageSize);
    if (b != kExeLo && b < kExeHi && b + kImageSize > kExeLo)
        Fatal("%s: 0x%llX..0x%llX overlaps the image's own range 0x%lX..0x%lX", what, b, b + kImageSize,
              (unsigned long)kExeLo, (unsigned long)kExeHi);
}

// The hash's own test, on synthetic pages (BOF3X_SELFTEST_ONLY): a page as
// the image at 0x400000 leaves it, and the same page with the image at F -
// every pointer into the image, in a slot or not, moved by F - 0x400000; the
// game's data unmoved, among it values in F's range and numbers that read as
// addresses in the image. Folding F, the two hash the same; without the fold
// they do not; a byte of data changed is seen through the fold; and with no
// fold a page hashes as its raw bytes (BOF3SH3's hash, bit for bit).
void SelfTest() {
    auto* mem = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4 * kPage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!mem) Fatal("statehash self-test: VirtualAlloc of the scratch pages failed");
    unsigned char* const at0 = mem;               // the image at 0x400000
    unsigned char* const atf = mem + kPage;       // the image at F
    unsigned char* const scratch = mem + 2 * kPage;
    unsigned char* const raw = mem + 3 * kPage;
    constexpr std::uint32_t F = 0x70000000u;
    CheckImageBase("statehash self-test", F);
    constexpr std::uint32_t delta = F - kExeLo;
    std::uint32_t seed = 0x2545F491u;
    auto next = [&] { return seed = seed * 1664525u + 1013904223u; };
    std::vector<std::uint32_t> sites;
    std::uint32_t pointers = 0, data_in_fold = 0, numbers = 0;
    for (std::uint32_t at = 0; at < kPage; at += 4) {
        const std::uint32_t kind = (next() >> 24) % 8;
        std::uint32_t w0 = next(), wf = w0;
        if (kind < 2) {   // a pointer into the image: a slot (0) the loader rebases, or one stored at run time (1)
            w0 = kExeLo + ((next() % kImageSize) & ~3u);
            wf = w0 + delta;
            if (kind == 0) sites.push_back(at);
            ++pointers;
        } else if (kind == 2) {   // data whose value lies where the image moved: unmoved
            w0 = wf = F + next() % kImageSize;
            ++data_in_fold;
        } else if (kind == 3) {   // a number that reads as an address in the image: unmoved
            w0 = wf = kExeLo + next() % kImageSize;
            ++numbers;
        }
        std::memcpy(at0 + at, &w0, 4);
        std::memcpy(atf + at, &wf, 4);
    }
    // A pointer the skip list masks, on both sides, and a range across words.
    const std::uint32_t masked0 = kExeLo + 0x1DA100, maskedf = masked0 + delta;
    std::memcpy(at0 + 0x40, &masked0, 4);
    std::memcpy(atf + 0x40, &maskedf, 4);
    const std::vector<Skip> skips = {{0x40, 4}, {0x7F2, 6}};
    const std::vector<Skip> none;
    const std::uint32_t folds[1] = {F};
    std::uint32_t n = 0;
    auto hash = [&](const unsigned char* page, const std::vector<Skip>& sk, unsigned nf) {
        return HashNormalised(page, sk, sites.data(), static_cast<std::uint32_t>(sites.size()), false, folds, nf,
                              scratch, &n);
    };
    const std::uint32_t h0 = hash(at0, skips, 1), hf = hash(atf, skips, 1);
    if (h0 != hf)
        Fatal("statehash self-test: the page with the image at 0x%08lX hashes 0x%08lX, at 0x400000 0x%08lX, both "
              "folding 0x%08lX", (unsigned long)F, (unsigned long)hf, (unsigned long)h0, (unsigned long)F);
    if (hash(at0, skips, 0) == hash(atf, skips, 0))
        Fatal("statehash self-test: without the fold the moved page hashes as the unmoved one");
    std::memcpy(raw, atf, kPage);
    raw[0x200] ^= 1;
    if (hash(raw, skips, 1) == hf) Fatal("statehash self-test: a changed byte hashes as before through the fold");
    std::memcpy(raw, at0, kPage);
    for (const Skip& s : skips) std::memset(raw + s.at, 0, s.len);
    if (hash(at0, skips, 0) != HashPage(raw))
        Fatal("statehash self-test: with no folded range the page does not hash as its masked bytes (BOF3SH3)");
    if (hash(at0, none, 0) != HashPage(at0))
        Fatal("statehash self-test: with no skip and no fold the page does not hash as its raw bytes");
    if (pointers == 0 || sites.empty() || data_in_fold == 0 || numbers == 0)
        Fatal("statehash self-test: the synthetic page lacks a kind of word (%lu pointers, %lu slots, %lu data, %lu "
              "numbers)", (unsigned long)pointers, (unsigned long)sites.size(), (unsigned long)data_in_fold,
              (unsigned long)numbers);
    VirtualFree(mem, 0, MEM_RELEASE);
    Log("statehash   self-test: a page with %lu image pointers (%lu of them slots) moved by 0x%08lX, %lu data words "
        "in the moved range and %lu image-shaped numbers unmoved, hashes the same folding 0x%08lX; differs without "
        "the fold and with a byte changed; with no fold hashes as its raw bytes",
        (unsigned long)pointers, (unsigned long)sites.size(), (unsigned long)delta, (unsigned long)data_in_fold,
        (unsigned long)numbers, (unsigned long)F);
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
    std::fwrite(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(g_data)), 1, kEnd - kBase, f);
    std::fclose(f);
    Log("statehash   tick %lu (Frame_Counter %lu): .data dumped to %s", static_cast<unsigned long>(g_tick),
        static_cast<unsigned long>(Frame_Counter), path);
}

}  // namespace

void StateHash_SetImageBase(unsigned long base) {
    if (g_census) Fatal("statehash: the image's base set to 0x%08lX after the census", base);
    CheckImageBase("statehash: the image's base", base);
    g_image = static_cast<std::uint32_t>(base);
    g_data = g_image + (kBase - kExeLo);
}

void StateHash_BeforeInjects(void* dll_module) {
    if (GetEnvironmentVariableA("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0) SelfTest();
    if (GetEnvironmentVariableA("BOF3X_STATEHASH", nullptr, 0) == 0) return;
    char raw[8];
    const DWORD rn = GetEnvironmentVariableA("BOF3X_STATEHASH_RAW", raw, sizeof raw);
    if (rn >= sizeof raw || (rn > 0 && std::strcmp(raw, "0") != 0 && std::strcmp(raw, "1") != 0))
        Fatal("BOF3X_STATEHASH_RAW: wants 1 (hash our pointers raw, as BOF3SH2 did) or 0");
    g_normalise = !(rn > 0 && raw[0] == '1');
    ReadDllImage(dll_module);
    // The folded ranges (the file's comment): the image's own base when it
    // moved, and BOF3X_STATEHASH_FOLD - on a run at 0x400000, the moved run's
    // base, so both sides fold the same range.
    char fold[24];
    const DWORD fn = GetEnvironmentVariableA("BOF3X_STATEHASH_FOLD", fold, sizeof fold);
    if (fn >= sizeof fold) Fatal("BOF3X_STATEHASH_FOLD: wants an address, as 0x70000000");
    if (fn > 0) {
        char* end = nullptr;
        const unsigned long long f = std::strtoull(fold, &end, 0);
        if (end == fold || *end) Fatal("BOF3X_STATEHASH_FOLD: wants an address, as 0x70000000; got '%s'", fold);
        CheckImageBase("BOF3X_STATEHASH_FOLD", f);
        g_fold_env = static_cast<std::uint32_t>(f);
    }
    if (g_image != kExeLo) g_folds[g_nfolds++] = g_image;
    if (g_fold_env && g_fold_env != kExeLo && g_fold_env != g_image) {
        if (g_nfolds && g_fold_env < g_image + kImageSize && g_fold_env + kImageSize > g_image)
            Fatal("BOF3X_STATEHASH_FOLD: 0x%08lX overlaps the image at 0x%08lX without being its base",
                  (unsigned long)g_fold_env, (unsigned long)g_image);
        g_folds[g_nfolds++] = g_fold_env;
    }
    // A folded range names the image: our DLL must not lie in one, or a
    // pointer of ours would read as one of the image's.
    for (unsigned i = 0; i < g_nfolds; ++i)
        if (g_folds[i] < g_dll_hi && g_folds[i] + kImageSize > g_dll_lo)
            Fatal("statehash: the folded range 0x%08lX..0x%08lX overlaps our image 0x%08lX..0x%08lX; set "
                  "BOF3X_STATEHASH_FOLD (and the image's base) elsewhere", (unsigned long)g_folds[i],
                  (unsigned long)(g_folds[i] + kImageSize), (unsigned long)g_dll_lo, (unsigned long)g_dll_hi);
    for (unsigned i = 0; i < g_nfolds; ++i)
        Log("statehash   the image at 0x%08lX: a word whose value lies in 0x%08lX..0x%08lX is hashed as at 0x400000",
            (unsigned long)g_image, (unsigned long)g_folds[i], (unsigned long)(g_folds[i] + kImageSize));
    // The identities have bit 31 set. Neither image reaches it here, so no
    // identity equals a pointer into either; game data can hold any value,
    // which is why the word's index is mixed into the page hash as well.
    Log("statehash   our image 0x%08lX..0x%08lX, %u sections a pointer can name in 0x%08lX..0x%08lX; BOF3.exe's "
        "0x%06lX..0x%06lX; identities 0x80000000.. %s both",
        (unsigned long)g_dll_lo, (unsigned long)g_dll_hi, (unsigned)g_sections.size(), (unsigned long)g_named_lo,
        (unsigned long)g_named_hi, (unsigned long)g_image, (unsigned long)(g_image + kImageSize),
        g_dll_hi <= 0x80000000u ? "lie above" : "OVERLAP our image - the index mix still keeps them apart -");
    g_page_sites.assign(kPages + 1, 0);
    for (std::uint32_t off = 0; off < kEnd - kBase; off += 4) {
        const std::uint32_t w = WordAt(g_data + off);
        if (InImage(w)) g_sites.push_back(off);
    }
    // The run-time slots (above) join the list, kept ascending; one that the
    // scan already listed (a word not zero at the census) is not doubled.
    std::uint32_t runtime_added = 0;
    auto add_site = [&](std::uint32_t va) {
        if (va < kBase || va + 4 > kEnd || (va & 3))
            Fatal("statehash: run-time slot 0x%08lX is not an aligned word of .data", (unsigned long)va);
        const std::uint32_t off = va - kBase;
        const auto it = std::lower_bound(g_sites.begin(), g_sites.end(), off);
        if (it != g_sites.end() && *it == off) return;
        g_sites.insert(it, off);
        ++runtime_added;
    };
    for (std::uint32_t e = 0; e < kMsgRingEntries; ++e) add_site(kMsgRing + e * kMsgRingStride);
    add_site(kMsgCurrent);
    std::size_t k = 0;
    for (std::uint32_t i = 0; i <= kPages; ++i) {
        while (k < g_sites.size() && g_sites[k] < i * kPage) ++k;
        g_page_sites[i] = static_cast<std::uint32_t>(k);
    }
    g_census = true;
    Log("statehash   %lu aligned words of .data hold a pointer into BOF3.exe's image as mapped: Capcom's pointer "
        "slots, and %lu run-time slots the battle's messages fill (%lu in all); %s",
        (unsigned long)(g_sites.size() - runtime_added), (unsigned long)runtime_added, (unsigned long)g_sites.size(),
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
    std::fwrite("BOF3SH4", 1, 8, g_out);
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
    Put(g_normalise ? 3u : 0u);   // bit 0 the slots, bit 1 the run-time slots with them
    Put(g_image);
    Put(g_fold_env);
    g_last.assign(kPages, 0);
    Log("statehash   %lu pages of .data from 0x%lX, a record a logic frame to %s; %u skip ranges, %u dump ticks, "
        "our pointers %s; the image at 0x%08lX, %u folded ranges",
        static_cast<unsigned long>(kPages), static_cast<unsigned long>(kBase), path, (unsigned)g_skips.size(),
        (unsigned)g_dump_ticks.size(), g_normalise ? "normalised" : "raw (BOF3X_STATEHASH_RAW=1)",
        static_cast<unsigned long>(g_image), g_nfolds);
    return true;
}

void StateHash_Tick() {
    if (!g_out) return;
    const std::uint32_t frame = Frame_Counter;
    if (g_seen && frame == g_last_frame) return;
    g_seen = true;
    g_last_frame = frame;

    static std::vector<std::uint32_t> changed;
    static std::vector<std::uint32_t> sites;
    changed.clear();
    unsigned char masked[kPage];
    std::uint32_t normalised = 0;
    for (std::uint32_t i = 0; i < kPages; ++i) {
        const unsigned char* const page = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(g_data + i * kPage));
        sites.clear();
        for (std::uint32_t k = g_page_sites[i]; k < g_page_sites[i + 1]; ++k) sites.push_back(g_sites[k] - i * kPage);
        const std::uint32_t h = HashNormalised(page, g_page_skips[i], sites.data(), static_cast<std::uint32_t>(sites.size()),
                                               g_normalise, g_folds, g_nfolds, masked, &normalised);
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
