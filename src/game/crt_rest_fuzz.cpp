// BOF3X_SHADOW=crt_rest: the C runtime entries of crt_rest.cpp against
// Capcom's, once at start-up, before CrtRest_Inject patches them
// (docs/crt-rest.md section 4).
//
// This runs before BOF3.exe's C runtime has started (docs/HANDOFF.md, Traps):
// no per-thread data, no locks, no heap. So
//   - Rand runs as a byte-copy whose one call, Crt_GetPtd at its entry, is
//     re-aimed at a stand-in answering a per-thread block of our own, its
//     seed word (+0x14) set alike with ours;
//   - sprintf, strncpy and _stricmp are leaves of the runtime that touch none
//     of that (sprintf's string FILE is on its own stack; _stricmp's locale
//     word 0x7DEC18 is 0 until a setlocale, and the game has none) and run as
//     Capcom's own, called at their addresses while the entries are intact;
//   - _findfirst / _findnext and the file layer reach the runtime's locks and
//     heap and cannot run here: ours are checked against Windows' own
//     FindFirstFileA / FindNextFileA and against the bytes written, and the
//     layouts against MSVC 6's at compile time (crt_rest.cpp).
// Each comparison has its negative controls: the same rounds against a
// planted bug of the kind the comparison exists to see, each of which must be
// refused at least once, or the fuzz is blind and the process ends.
#include <io.h>
#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/crt_rest.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace crt_rest {
namespace {

using U = std::uint32_t;

// --- Random numbers of our own ----------------------------------------------
U g_rand = 0x5B93D2A5u;
U Next() {
    U x = g_rand;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g_rand = x;
}

unsigned g_bad = 0;
void Mismatch(const char* what, unsigned round, U theirs, U ours) {
    if (++g_bad <= 12)
        bof3::Log("shadow      crt_rest self-test MISMATCH: %s, round %u: original %08X, ours %08X", what, round,
                  (unsigned)theirs, (unsigned)ours);
}

void ControlRefused(const char* name, unsigned refused) {
    if (refused == 0) bof3::Fatal("crt_rest self-test: the control \"%s\" was not refused - the fuzz cannot see it", name);
    bof3::Log("shadow      crt_rest control %-34s refused in %u rounds", name, refused);
}

// =============================================================================
// Rand
// =============================================================================

// The per-thread block the copy's Crt_GetPtd answers: 0x74 bytes, as _mtinit
// allocates (calloc(1, 0x74) at 0x5BAD13).
alignas(4) unsigned char g_ptd[0x74];
unsigned char* __cdecl StandInGetPtd() { return g_ptd; }

U PtdSeed() {
    U v;
    std::memcpy(&v, g_ptd + 0x14, 4);
    return v;
}
void SetPtdSeed(U v) { std::memcpy(g_ptd + 0x14, &v, 4); }

// The planted bugs: the multiplier off by one, the shift 15.
int __cdecl PlantedMultiplier() {
    U& s = RandSeed();
    s = s * 0x343FEu + 0x269EC3u;
    return static_cast<int>((s >> 16) & 0x7FFF);
}
int __cdecl PlantedShift() {
    U& s = RandSeed();
    s = s * 0x343FDu + 0x269EC3u;
    return static_cast<int>((s >> 15) & 0x7FFF);
}

U SeedFor(unsigned round) {
    static const U kEdges[] = {0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000u, 0xFFFFFFFFu,
                               0xFFFE5BC3u, 0x269EC3, 0x343FD};
    if (round < sizeof kEdges / sizeof kEdges[0]) return kEdges[round];
    return Next();
}

// One comparison: `ours` against the copy from the same seed, the answer and
// the seed after. Returns the rounds that differ.
unsigned CompareRand(int (__cdecl* theirs)(), int (__cdecl* ours)(), unsigned rounds, bool log) {
    unsigned differ = 0;
    const U saved = g_rand;
    for (unsigned round = 0; round < rounds; ++round) {
        const U seed = SeedFor(round);
        SetPtdSeed(seed);
        RandSeed() = seed;
        const int a = theirs();
        const int b = ours();
        if (a != b || PtdSeed() != RandSeed()) {
            ++differ;
            if (log) {
                Mismatch("Rand's answer", round, static_cast<U>(a), static_cast<U>(b));
                Mismatch("Rand's seed after", round, PtdSeed(), RandSeed());
            }
        }
    }
    g_rand = saved;
    return differ;
}

void FuzzRand() {
    constexpr U kRandSize = 0x22;   // 0x5B93D2..0x5B93F3, the ret; nothing but the entry's call leaves it
    const auto get_ptd = static_cast<U>(reinterpret_cast<std::uintptr_t>(Crt_GetPtd));   // still Capcom's
    const bof3::CloneCall calls[] = {{0, reinterpret_cast<const void*>(&StandInGetPtd), get_ptd}};
    auto* copy = reinterpret_cast<int (__cdecl*)()>(
        bof3::CloneOriginal("Rand", bof3::addr::Rand, kRandSize, calls, 1));
    constexpr unsigned kRounds = 100000;
    const unsigned before = g_bad;
    CompareRand(copy, &::Rand, kRounds, true);
    // A run of the sequence from the CRT's start, 1, as the game draws it.
    SetPtdSeed(1);
    RandSeed() = 1;
    for (unsigned i = 0; i < 20000; ++i) {
        const int a = copy();
        const int b = ::Rand();
        if (a != b) Mismatch("Rand's sequence from 1", i, static_cast<U>(a), static_cast<U>(b));
    }
    bof3::Log("shadow      crt_rest self-test: Rand %u rounds and a sequence of 20000 from 1, %u MISMATCHES", kRounds,
              g_bad - before);
    ControlRefused("Rand, multiplier 0x343FE", CompareRand(copy, &PlantedMultiplier, kRounds, false));
    ControlRefused("Rand, shift 15", CompareRand(copy, &PlantedShift, kRounds, false));
}

// =============================================================================
// sprintf
// =============================================================================

// Every format the game's sprintf calls pass (docs/crt-rest.md section 2.2):
// the image's, by address, read in place, and ours' own literals.
const U kImageFormats[] = {
    0x5E10C0, 0x5F6308, 0x60ABF0, 0x61BC74, 0x64ADD8, 0x64ADDC, 0x64D3EC, 0x64E324, 0x652894, 0x65306C,
    0x653074, 0x65307C, 0x653088, 0x6531F4, 0x653EB4, 0x653EC0, 0x654830, 0x654900, 0x65AB08, 0x65DA5C,
    0x660C7C, 0x6639A8, 0x6639B0, 0x6639C4, 0x6639C8, 0x6639D4, 0x664068, 0x6641DC, 0x666F9C, 0x666FA8,
    0x666FB8, 0x66AF34, 0x66AF8C, 0x66B4A0, 0x66B4A8, 0x66B62C, 0x66B654, 0x66B674, 0x66BC24, 0x66BC30,
    // the six sites that copy a format with no conversion
    0x64E31C, 0x64E320, 0x653090, 0x653094,
};
const char* const kOurFormats[] = {"DAT\\%s", "DAT\\%s.%s", "%s%s"};

const char* const kStrings[] = {
    nullptr, "", "a", "BISLPS00.DAT", "C:", "SYS", "\xB5\xDA\xD2\xBB\xD5\xC2", "%d", "0123456789abcdefABCDEF",
    "en", "DAT\\ROOM", "a much longer string that runs past any width the formats ask for",
};

int IntFor(unsigned k) {
    static const int kEdges[] = {0, 1, -1, 9, 10, -9, -10, 99, 100, 999, 1000, 9999, 10000, 99999, 1000000,
                                 9999999, 10000000, 99999999, 100000000, 0x7FFFFFFF, static_cast<int>(0x80000000u),
                                 -99, -100, -999, 0xF, 0x10, 0xFF, 0x100, 0xFFFF, static_cast<int>(0xFFFFFFFFu)};
    switch (k % 4) {
    case 0: return kEdges[Next() % (sizeof kEdges / sizeof kEdges[0])];
    case 1: return static_cast<int>(Next() % 2000) - 1000;
    case 2: return static_cast<int>(Next() % 200000) - 100000;
    default: return static_cast<int>(Next());
    }
}

// The kinds of the conversions in a listed format, in order: 'i' an int, 's'
// a string. The formats are the list's, so the parse is the list's.
unsigned Kinds(const char* fmt, char* kinds) {
    unsigned n = 0;
    for (const char* p = fmt; *p; ++p) {
        if (*p != '%') continue;
        ++p;
        while (*p >= '0' && *p <= '9') ++p;
        kinds[n++] = *p == 's' ? 's' : 'i';
    }
    return n;
}

using SprintfFn = int (__cdecl*)(char*, const char*, ...);

// Up to three arguments, as the formats have.
int Call(SprintfFn f, char* out, const char* fmt, const U* args) { return f(out, fmt, args[0], args[1], args[2]); }

// The controls: ours' output altered as a bug of each kind would alter it.
using Plant = void (*)(char* out, int& n);
void PlantSignAfterZeros(char* out, int&) {   // zeros before the sign
    for (char* p = out; *p; ++p)
        if (*p == '0' && p[1] == '-') { p[0] = '-'; p[1] = '0'; return; }
        else if (*p == '-' && p[1] == '0') { p[0] = '0'; p[1] = '-'; return; }
}
void PlantNullEmpty(char* out, int& n) {   // "(null)" printed as nothing
    char* p = std::strstr(out, "(null)");
    if (!p) return;
    std::memmove(p, p + 6, std::strlen(p + 6) + 1);
    n -= 6;
}
void PlantLowerHex(char* out, int&) {   // %x for %X
    for (char* p = out; *p; ++p)
        if (*p >= 'A' && *p <= 'F') { *p = static_cast<char>(*p + 0x20); return; }
}
void PlantShortPad(char* out, int& n) {   // one pad too few
    if (out[0] == ' ' || (out[0] == '0' && out[1] >= '0' && out[1] <= '9')) {
        std::memmove(out, out + 1, std::strlen(out + 1) + 1);
        --n;
    }
}
void PlantCount(char*, int& n) { ++n; }   // the NUL counted

void FuzzSprintf() {
    const char* formats[64];
    unsigned nf = 0;
    for (U a : kImageFormats) formats[nf++] = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(a));
    for (const char* f : kOurFormats) formats[nf++] = f;
    const SprintfFn theirs = bof3::orig::Crt_sprintf;
    const Plant plants[] = {PlantSignAfterZeros, PlantNullEmpty, PlantLowerHex, PlantShortPad, PlantCount};
    const char* const plant_names[] = {"sprintf, zeros before the sign", "sprintf, (null) as nothing",
                                       "sprintf, %X in lower case", "sprintf, one pad short",
                                       "sprintf, the NUL counted"};
    unsigned refused[5] = {};
    constexpr unsigned kPerFormat = 3000;
    const unsigned before = g_bad;
    unsigned rounds = 0;
    for (unsigned f = 0; f < nf; ++f) {
        const char* fmt = formats[f];
        char kinds[8];
        const unsigned nk = Kinds(fmt, kinds);
        if (nk > 3) bof3::Fatal("crt_rest self-test: format %u has %u conversions, the fuzz passes 3", f, nk);
        for (unsigned r = 0; r < kPerFormat; ++r, ++rounds) {
            U args[3] = {};
            for (unsigned k = 0; k < nk; ++k)
                args[k] = kinds[k] == 's'
                              ? static_cast<U>(reinterpret_cast<std::uintptr_t>(
                                    kStrings[Next() % (sizeof kStrings / sizeof kStrings[0])]))
                              : static_cast<U>(IntFor(r + k));
            char a[0x100], b[0x100];
            std::memset(a, 0xCC, sizeof a);
            std::memset(b, 0xCC, sizeof b);
            const int na = Call(theirs, a, fmt, args);
            const int nb = Call(&::Crt_sprintf, b, fmt, args);
            if (na != nb || std::memcmp(a, b, sizeof a) != 0) {
                unsigned i = 0;
                while (i < sizeof a && a[i] == b[i]) ++i;
                Mismatch(fmt, r, static_cast<U>(na) << 16 | (i < sizeof a ? static_cast<unsigned char>(a[i]) : 0),
                         static_cast<U>(nb) << 16 | (i < sizeof a ? static_cast<unsigned char>(b[i]) : 0));
            }
            for (unsigned p = 0; p < 5; ++p) {
                char c[0x100];
                std::memcpy(c, b, sizeof c);
                int nc = nb;
                plants[p](c, nc);
                if (nc != na || std::memcmp(a, c, sizeof a) != 0) ++refused[p];
            }
        }
    }
    bof3::Log("shadow      crt_rest self-test: sprintf %u formats, %u rounds, %u MISMATCHES", nf, rounds, g_bad - before);
    for (unsigned p = 0; p < 5; ++p) ControlRefused(plant_names[p], refused[p]);
}

// =============================================================================
// strncpy and _stricmp
// =============================================================================

char* __cdecl PlantedNoFill(char* dst, const char* src, unsigned n) {   // stops at the NUL, no zero fill
    unsigned i = 0;
    for (; i < n && src[i]; ++i) dst[i] = src[i];
    if (i < n) dst[i] = 0;
    return dst;
}
char* __cdecl PlantedOneShort(char* dst, const char* src, unsigned n) {   // n - 1 bytes
    return ::Crt_strncpy(dst, src, n ? n - 1 : 0);
}

using StrncpyFn = char* (__cdecl*)(char*, const char*, unsigned);
unsigned CompareStrncpy(StrncpyFn ours, unsigned rounds, bool log) {
    const StrncpyFn theirs = bof3::orig::Crt_strncpy;
    unsigned differ = 0;
    const U saved = g_rand;
    for (unsigned round = 0; round < rounds; ++round) {
        unsigned char src[0x60], a[0x80], b[0x80];
        for (auto& x : src) x = static_cast<unsigned char>(Next() % 4 == 0 ? 0 : Next());
        for (auto& x : a) x = static_cast<unsigned char>(Next());
        std::memcpy(b, a, sizeof b);
        const unsigned so = Next() % 4, dofs = Next() % 4;   // every alignment of either: the dword path
        const unsigned n = Next() % 3 == 0 ? Next() % 8 : Next() % 0x50;
        const char* const s = reinterpret_cast<const char*>(src + so);
        char* const ra = theirs(reinterpret_cast<char*>(a + dofs), s, n);
        char* const rb = ours(reinterpret_cast<char*>(b + dofs), s, n);
        const bool same_ret = ra - reinterpret_cast<char*>(a) == rb - reinterpret_cast<char*>(b);
        if (!same_ret || std::memcmp(a, b, sizeof a) != 0) {
            ++differ;
            if (log) {
                unsigned i = 0;
                while (i < sizeof a && a[i] == b[i]) ++i;
                Mismatch("strncpy", round, n << 8 | (i < sizeof a ? a[i] : 0), i << 8 | (i < sizeof b ? b[i] : 0));
            }
        }
    }
    g_rand = saved;
    return differ;
}

int __cdecl PlantedCaseSensitive(const char* a, const char* b) {
    const int r = std::strcmp(a, b);
    return (r > 0) - (r < 0);
}
int __cdecl PlantedRawDifference(const char* a, const char* b) { return _stricmp(a, b); }
int __cdecl PlantedSigned(const char* a, const char* b) {   // folded, but compared as signed bytes
    for (;; ++a, ++b) {
        signed char x = static_cast<signed char>(*a), y = static_cast<signed char>(*b);
        if (x >= 'A' && x <= 'Z') x = static_cast<signed char>(x + 0x20);
        if (y >= 'A' && y <= 'Z') y = static_cast<signed char>(y + 0x20);
        if (x != y) return x < y ? -1 : 1;
        if (!x) return 0;
    }
}

using StricmpFn = int (__cdecl*)(const char*, const char*);
unsigned CompareStricmp(StricmpFn ours, unsigned rounds, bool log) {
    const StricmpFn theirs = reinterpret_cast<StricmpFn>(static_cast<std::uintptr_t>(bof3::addr::Crt_stricmp));
    static const char kAlphabet[] = "aAbBzZ@[`{_09\x80\xC1\xE1\xFF";
    unsigned differ = 0;
    const U saved = g_rand;
    for (unsigned round = 0; round < rounds; ++round) {
        char x[0x18], y[0x18];
        const unsigned lx = Next() % 0x14, ly = Next() % 3 == 0 ? lx : Next() % 0x14;
        for (unsigned i = 0; i < lx; ++i) x[i] = kAlphabet[Next() % (sizeof kAlphabet - 1)];
        x[lx] = 0;
        // often y is x with its cases changed and perhaps one byte different
        if (Next() % 2) {
            for (unsigned i = 0; i <= lx; ++i) {
                char c = x[i];
                if (Next() % 2 && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) c = static_cast<char>(c ^ 0x20);
                y[i] = c;
            }
            if (lx && Next() % 3 == 0) y[Next() % lx] = kAlphabet[Next() % (sizeof kAlphabet - 1)];
        } else {
            for (unsigned i = 0; i < ly; ++i) y[i] = kAlphabet[Next() % (sizeof kAlphabet - 1)];
            y[ly] = 0;
        }
        const int a = theirs(x, y), b = ours(x, y);
        if (a != b) {
            ++differ;
            if (log) Mismatch("_stricmp", round, static_cast<U>(a), static_cast<U>(b));
        }
    }
    g_rand = saved;
    return differ;
}

void FuzzStrings() {
    constexpr unsigned kRounds = 60000;
    const unsigned before = g_bad;
    CompareStrncpy(&::Crt_strncpy, kRounds, true);
    CompareStricmp(&::Crt_stricmp, kRounds, true);
    bof3::Log("shadow      crt_rest self-test: strncpy and _stricmp %u rounds each, %u MISMATCHES", kRounds, g_bad - before);
    ControlRefused("strncpy, no zero fill", CompareStrncpy(&PlantedNoFill, kRounds, false));
    ControlRefused("strncpy, one byte short", CompareStrncpy(&PlantedOneShort, kRounds, false));
    ControlRefused("_stricmp, case-sensitive", CompareStricmp(&PlantedCaseSensitive, kRounds, false));
    ControlRefused("_stricmp, the raw difference", CompareStricmp(&PlantedRawDifference, kRounds, false));
    ControlRefused("_stricmp, signed bytes", CompareStricmp(&PlantedSigned, kRounds, false));
}

// =============================================================================
// _findfirst / _findnext, against Windows'
// =============================================================================

// Ours over the patterns the game and the check use, the matches as
// FindFirstFileA / FindNextFileA list them: the same names in the same order,
// each with its size at +0x10 and its attributes at +0. The game's pattern
// is BISLPS??.DAT (Save_ListFiles), relative to the game's directory.
void FuzzFind() {
    constexpr U kListPattern = 0x65289C;   // "BISLPS??.DAT", pushed by Save_ListFiles at 0x4548D3
    const char* const patterns[] = {reinterpret_cast<const char*>(static_cast<std::uintptr_t>(kListPattern)), "*",
                                    "DAT\\*.DAT", "*.EXE", "NO_SUCH_FILE_*.XYZ"};
    unsigned matches = 0;
    const unsigned before = g_bad;
    for (unsigned k = 0; k < sizeof patterns / sizeof patterns[0]; ++k) {
        const char* pattern = patterns[k];
        WIN32_FIND_DATAA w;
        HANDLE h = FindFirstFileA(pattern, &w);
        unsigned char found[0x118];
        std::memset(found, 0xCC, sizeof found);
        const long handle = ::Crt_findfirst(pattern, found);
        if ((h == INVALID_HANDLE_VALUE) != (handle == -1)) {
            Mismatch(pattern, 0, h == INVALID_HANDLE_VALUE, handle == -1);
            if (h != INVALID_HANDLE_VALUE) FindClose(h);
            continue;
        }
        if (h == INVALID_HANDLE_VALUE) continue;
        for (unsigned i = 0;; ++i) {
            U attrib, size;
            std::memcpy(&attrib, found, 4);
            std::memcpy(&size, found + 0x10, 4);
            const char* name = reinterpret_cast<const char*>(found + 0x14);
            ++matches;
            if (std::strcmp(name, w.cFileName) != 0) Mismatch(pattern, i, 0, 1);
            if (size != w.nFileSizeLow) Mismatch(pattern, i, w.nFileSizeLow, size);
            if ((attrib & 0x37) != (w.dwFileAttributes & 0x37)) Mismatch(pattern, i, w.dwFileAttributes, attrib);
            const BOOL more = FindNextFileA(h, &w);
            const int next = ::Crt_findnext(handle, found);
            if ((more != 0) != (next == 0)) {
                Mismatch(pattern, i, more, static_cast<U>(next));
                break;
            }
            if (!more) break;
        }
        FindClose(h);
        // Save_ListFiles never closes its search; the check closes its own.
        _findclose(handle);
    }
    if (matches == 0) bof3::Fatal("crt_rest self-test: _findfirst matched nothing - run from the game's directory");
    bof3::Log("shadow      crt_rest self-test: _findfirst / _findnext %u matches over 5 patterns, %u MISMATCHES", matches,
              g_bad - before);
}

// =============================================================================
// The file layer, against the bytes written
// =============================================================================

// Capcom's cannot run before its runtime (its locks); ours is checked for the
// contract the game's callers hold it to: a file written "wb" reads back the
// same through "rb" at every offset, its length is what was written, a failed
// open is null, and fgets on a "rt" stream gives the lines with CRLF as LF.
void FuzzFiles() {
    char dir[MAX_PATH], path[MAX_PATH];
    if (!GetTempPathA(sizeof dir, dir) || !GetTempFileNameA(dir, "b3c", 0, path))
        bof3::Fatal("crt_rest self-test: no temporary file (error %lu)", GetLastError());
    const unsigned before = g_bad;
    static unsigned char data[0x5000], back[0x5000];
    for (auto& x : data) x = static_cast<unsigned char>(Next());
    // CR, LF and ^Z bytes in a binary file are data
    data[10] = '\r', data[11] = '\n', data[12] = 0x1A;
    void* w = ::Crt_fopen(path, "wb");
    if (!w) bof3::Fatal("crt_rest self-test: Crt_fopen(\"%s\", \"wb\") failed", path);
    if (::Crt_fwrite(data, 1, 0x3000, w) != 0x3000) Mismatch("fwrite count", 0, 0x3000, 0);
    if (::Crt_fwrite(data + 0x3000, 0x100, 0x20, w) != 0x20) Mismatch("fwrite count", 1, 0x20, 0);
    if (::Crt_fclose(w) != 0) Mismatch("fclose", 0, 0, 1);
    void* r = ::Crt_fopen(path, "rb");
    if (!r) bof3::Fatal("crt_rest self-test: Crt_fopen(\"%s\", \"rb\") failed", path);
    const int len = ::Crt_filelength(::Crt_fileno(r));
    if (len != 0x5000) Mismatch("filelength", 0, 0x5000, static_cast<U>(len));
    unsigned rounds = 0;
    for (; rounds < 400; ++rounds) {
        const unsigned at = Next() % 0x5000, want = Next() % 0x1800;
        if (::Crt_fseek(r, static_cast<int>(at), 0) != 0) Mismatch("fseek", rounds, 0, 1);
        const unsigned got = ::Crt_fread(back, 1, want, r);
        const unsigned expect = at + want > 0x5000 ? 0x5000 - at : want;
        if (got != expect) Mismatch("fread count", rounds, expect, got);
        else if (std::memcmp(back, data + at, got) != 0) Mismatch("fread bytes", rounds, at, want);
    }
    ::Crt_fclose(r);
    // fgets on a text stream: Cfg_Load's shape, (buf, 0x14, stream)
    static const char kText[] = "12 34\r\n5 6\r\nlonger than twenty bytes here\r\nlast";
    w = ::Crt_fopen(path, "wb");
    ::Crt_fwrite(kText, 1, sizeof kText - 1, w);
    ::Crt_fclose(w);
    r = ::Crt_fopen(path, "rt");
    static const char* const kLines[] = {"12 34\n", "5 6\n", "longer than twenty ", "bytes here\n", "last"};
    char line[0x14];
    for (unsigned i = 0; i < 5; ++i) {
        const char* got = ::Crt_fgets(line, 0x14, r);
        if (got != line || std::strcmp(line, kLines[i]) != 0) Mismatch("fgets line", i, 0, 1);
    }
    if (::Crt_fgets(line, 0x14, r) != nullptr) Mismatch("fgets at the end", 0, 0, 1);
    ::Crt_fclose(r);
    DeleteFileA(path);
    if (::Crt_fopen("NO_SUCH_DIRECTORY_B3\\NO_SUCH_FILE.DAT", "rb") != nullptr) Mismatch("fopen of nothing", 0, 0, 1);
    bof3::Log("shadow      crt_rest self-test: the file layer, %u reads at random offsets and 5 text lines, %u MISMATCHES",
              rounds, g_bad - before);
}

}  // namespace

void SelfTest() {
    FuzzRand();
    FuzzSprintf();
    FuzzStrings();
    FuzzFind();
    FuzzFiles();
    bof3::Log("shadow      crt_rest self-test: %u MISMATCHES in all", g_bad);
    if (g_bad) bof3::Fatal("the C runtime's entries differ from the originals in %u self-test comparisons", g_bad);
}

}  // namespace crt_rest
