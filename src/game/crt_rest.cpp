// The C runtime's entry points the game calls: rand and atoi reimplemented
// exactly, sprintf and sscanf over the conversions the game's formats use,
// and the string, search, file and heap entries bound to our toolchain's
// runtime through thin named functions (docs/crt-rest.md;
// docs/platform-layers-plan.md section 2.3).
//
// Microsoft's runtime behind the entries is not decompiled and not ours to
// publish; what is ours is the contract each entry keeps with its callers,
// read from the disassembly and fuzzed against Capcom's copy
// (crt_rest_fuzz.cpp). Every caller of these entries is ours (an E8 / E9 scan
// of BOF3.exe, docs/crt-rest.md section 1) except where section 1 says so,
// and the injects below move the remaining Capcom callers - the CRT's own
// strncpy users and _fcloseall - with them.
#include "game/crt_rest.h"

#include <io.h>
#include <windows.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/host.h"
#include "hook/log.h"

namespace crt_rest {
namespace {

// --- Rand ---------------------------------------------------------------------
//
// MSVC 6's holdrand lives in the per-thread data at +0x14 and starts at 1
// (_initptd 0x5BAD51: `mov dword [eax + 0x14], 1`, called by _mtinit 0x5BACFD
// for the main thread and by _getptd 0x5BAD64 for any other); the binary has
// no srand. Only the main thread ever calls Rand: all 745 of BOF3.exe's call
// sites are game code (ours), the executable imports no thread-creating
// function, and every traced run entered Rand on its main thread
// (docs/crt-rest.md section 2.1). So one static seed is the main thread's.
std::uint32_t g_seed = 1;

bool g_counting = false;
std::uint32_t g_count = 0;

// With Rand left original under the randlog: a byte-copy of Capcom's (its
// Crt_GetPtd call kept), counted at its entry - an instrument, as the randlog
// was before Rand was ours.
int (__cdecl* g_original_copy)() = nullptr;

int __cdecl CountingOriginal() {
    ++g_count;
    return g_original_copy();
}

// --- Crt_sprintf ------------------------------------------------------------------
//
// The conversions the game's formats use (docs/crt-rest.md section 2.2, the
// scan of every format the 188 call sites of BOF3.exe and ours push): %d and
// %X with an optional 0 flag and a width up to 8, and a bare %s. MSVC 6's
// _output 0x5BA4F3 for those: a negative %d is its magnitude as unsigned
// with '-' in front; the width counts the sign; without the 0 flag the pad is
// spaces before the sign, with it zeros after the sign; a null %s prints
// "(null)". Anything else is not a format the game has, and ends the process
// (rule 4).
constexpr unsigned kMaxWidth = 8;

// Crt_sprintf's caller, for the refusal: a return into the image as its VA
// (symbols.toml's frame), else into our DLL as bof3x.dll+offset (__ImageBase,
// the DLL's own base).
extern "C" char __ImageBase;
const char* CallerText(const void* ret, char* out, std::size_t cap) {
    const auto at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ret));
    const auto dll = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&__ImageBase));
    if (at - ::bof3::ImageBase() < 0x58F000u)
        std::snprintf(out, cap, "VA 0x%08X", static_cast<unsigned>(::bof3::VaOf(at)));
    else
        std::snprintf(out, cap, "bof3x.dll+0x%X", static_cast<unsigned>(at - dll));
    return out;
}

[[noreturn]] void RefuseFormat(const char* fmt, const char* spec, std::size_t len, const void* caller) {
    char from[40];
    bof3::Fatal("Crt_sprintf: \"%s\" (at 0x%08X) has the conversion \"%.*s\", which no format of the game uses and "
                "ours does not take (docs/crt-rest.md section 2.2); called from %s",
                fmt, static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(fmt)), static_cast<int>(len), spec,
                CallerText(caller, from, sizeof from));
}

char* PutPadded(char* out, const char* prefix, const char* text, std::size_t len, unsigned width, bool zero) {
    const std::size_t plen = std::strlen(prefix);
    std::size_t pad = width > plen + len ? width - plen - len : 0;
    if (!zero)
        while (pad) *out++ = ' ', --pad;
    while (*prefix) *out++ = *prefix++;
    while (pad) *out++ = '0', --pad;
    std::memcpy(out, text, len);
    return out + len;
}

}  // namespace

std::uint32_t& RandSeed() { return g_seed; }

// Asked once, in game (the per-thread data is there by then): IsEnabled walks
// every inject, and the map_cells live check asks on every call it shadows.
std::uint32_t* RandSeedCell() {
    static std::uint32_t* const cell = bof3::IsEnabled(bof3::addr::Rand)
                                           ? &g_seed
                                           : reinterpret_cast<std::uint32_t*>(Crt_GetPtd() + 0x14);
    return cell;
}

void RandCount_Start() {
    if (bof3::IsEnabled(bof3::addr::Rand)) {
        g_counting = true;
        bof3::Log("input       randlog: Rand counted by ours, one line a frame");
        return;
    }
    if (!g_original_copy)
        bof3::Fatal("randlog: Rand is left original and no copy of it was made at its inject (crt_rest.cpp)");
    // Rand left to Capcom: ours jumps to 0x5B93D2, whose entry is intact.
    // The counter goes there as an instrument; a local constant, not
    // bof3::addr::Rand, so the ownership ledger does not read a second
    // takeover (tools/ledger_check.py).
    constexpr ::bof3::image::CodeVa kRand{bof3::addr::Rand};
    bof3::Inject("Rand", kRand.v, reinterpret_cast<void*>(&CountingOriginal), true);
    g_counting = true;
    bof3::Log("input       randlog: Rand left original, counted at its entry, one line a frame");
}

bool RandCounting() { return g_counting; }
std::uint32_t RandCount() { return g_count; }

}  // namespace crt_rest

using namespace crt_rest;

// original 0x5B93D2: `call Crt_GetPtd; ecx = [eax + 0x14]; ecx = ecx *
// 0x343FD + 0x269EC3; [eax + 0x14] = ecx; eax = (ecx >> 16) & 0x7FFF` - the
// MSVC 6 rand(), its seed per thread (ours: one static, section 2.1).
extern "C" int __cdecl Rand(void) {
    if (g_counting) ++g_count;
    g_seed = g_seed * 0x343FDu + 0x269EC3u;
    return static_cast<int>((g_seed >> 16) & 0x7FFFu);
}

// original 0x5B9380: sprintf over MSVC 6's _output, for the conversions above.
// Returns the characters written, the NUL not counted.
extern "C" int __cdecl Crt_sprintf(char* dst, const char* fmt, ...) {
    const void* const caller = __builtin_return_address(0);   // named by a refusal
    va_list ap;
    va_start(ap, fmt);
    char* out = dst;
    for (const char* p = fmt; *p;) {
        if (*p != '%') {
            *out++ = *p++;
            continue;
        }
        const char* const spec = p++;
        bool zero = false;
        if (*p == '0') zero = true, ++p;
        unsigned width = 0;
        bool has_width = false;
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + static_cast<unsigned>(*p++ - '0');
            has_width = true;
            if (width > kMaxWidth) RefuseFormat(fmt, spec, static_cast<std::size_t>(p - spec), caller);
        }
        const char conv = *p;
        if (conv == '\0') RefuseFormat(fmt, spec, static_cast<std::size_t>(p - spec), caller);
        ++p;
        const std::size_t speclen = static_cast<std::size_t>(p - spec);
        char digits[12];
        std::size_t n = 0;
        if (conv == 'd') {
            const int v = va_arg(ap, int);
            std::uint32_t mag = v < 0 ? 0u - static_cast<std::uint32_t>(v) : static_cast<std::uint32_t>(v);
            char rev[12];
            do rev[n++] = static_cast<char>('0' + mag % 10), mag /= 10;
            while (mag);
            for (std::size_t i = 0; i < n; ++i) digits[i] = rev[n - 1 - i];
            out = PutPadded(out, v < 0 ? "-" : "", digits, n, width, zero);
        } else if (conv == 'X') {
            std::uint32_t v = va_arg(ap, unsigned);
            char rev[12];
            do rev[n++] = "0123456789ABCDEF"[v & 0xF], v >>= 4;
            while (v);
            for (std::size_t i = 0; i < n; ++i) digits[i] = rev[n - 1 - i];
            out = PutPadded(out, "", digits, n, width, zero);
        } else if (conv == 's' && !zero && !has_width) {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            while (*s) *out++ = *s++;
        } else {
            RefuseFormat(fmt, spec, speclen, caller);
        }
    }
    *out = '\0';
    va_end(ap);
    return static_cast<int>(out - dst);
}

// original 0x5B9450: MSVC's strncpy.asm - at most n bytes of src, stopping
// after its NUL, the rest of the n zero-filled; dst returned. The toolchain's
// is the same contract (C90 7.11.2.4).
extern "C" char* __cdecl Crt_strncpy(char* dst, const char* src, unsigned n) {
    return std::strncpy(dst, src, n);
}

// original 0x5B9A9B: atoi, MSVC 6's wrapper over atol 0x5B9A10 (push the
// argument, call, pop). atol's body, read to its ret 0x5B9A9A:
//   - skip every byte whose _pctype entry has _SPACE (8): __mb_cur_max
//     0x675064 is 1 and _pctype 0x674E58 points at the "C" table 0x674E62,
//     and nothing in the executable writes either word or the table (the
//     _isctype path 0x5BCEB9 is never taken) - so 0x09..0x0D and 0x20, and no
//     byte at or above 0x80;
//   - one optional '-' or '+';
//   - while the byte is a digit by _pctype's _DIGIT (4) - '0'..'9' only -
//     `ebx = ebx * 10 + c - '0'` (two lea), a 32-bit register: the total
//     wraps modulo 2^32, with no overflow test;
//   - the total negated (neg) if the sign was '-'.
// So the answer is the digits' value modulo 2^32, negated for '-', read as
// an int. The toolchain's atoi saturates or is undefined past INT_MAX; ours
// keeps the wrap (docs/crt-rest.md section 2.7). Callers Cfg_Load 0x4FD094 /
// 0x4FD0A8 (BOF3.CFG's two lines).
extern "C" int __cdecl Crt_atoi(const char* s) {
    const auto* p = reinterpret_cast<const unsigned char*>(s);
    while (*p == ' ' || (*p >= 0x09 && *p <= 0x0D)) ++p;
    const unsigned char sign = *p;
    if (sign == '-' || sign == '+') ++p;
    std::uint32_t total = 0;
    while (*p >= '0' && *p <= '9') total = total * 10u + static_cast<std::uint32_t>(*p++ - '0');
    if (sign == '-') total = 0u - total;
    return static_cast<int>(total);
}

// original 0x5C2B40: _stricmp. Under the "C" locale (the game never sets one:
// __lc_handle[LC_CTYPE] 0x7DEC18 stays 0) both bytes are folded A..Z to a..z
// and compared unsigned; the answer is -1, 0 or 1 (`sbb al, al; sbb al,
// 0xFF; movsx eax, al`). The toolchain's folds the same way and answers the
// difference: its sign is the original's answer.
extern "C" int __cdecl Crt_stricmp(const char* a, const char* b) {
    const int r = _stricmp(a, b);
    return (r > 0) - (r < 0);
}

// original 0x5B979A / 0x5B9867: _findfirst / _findnext into MSVC 6's
// _finddata_t - attrib u32 +0, time_create / time_access / time_write 32-bit
// +4 / +8 / +0xC, size u32 +0x10, name[260] +0x14, 0x118 bytes. The
// toolchain's _finddata32_t is that layout (checked below); its _finddata_t is
// not (64-bit times). The handle goes only between the two (Save_ListFiles,
// which never closes it: the original's leak, kept there).
static_assert(offsetof(_finddata32_t, attrib) == 0x0 && sizeof(_finddata32_t::attrib) == 4, "_finddata32_t.attrib");
static_assert(offsetof(_finddata32_t, time_create) == 0x4 && sizeof(_finddata32_t::time_create) == 4,
              "_finddata32_t.time_create");
static_assert(offsetof(_finddata32_t, time_access) == 0x8 && sizeof(_finddata32_t::time_access) == 4,
              "_finddata32_t.time_access");
static_assert(offsetof(_finddata32_t, time_write) == 0xC && sizeof(_finddata32_t::time_write) == 4,
              "_finddata32_t.time_write");
static_assert(offsetof(_finddata32_t, size) == 0x10 && sizeof(_finddata32_t::size) == 4, "_finddata32_t.size");
static_assert(offsetof(_finddata32_t, name) == 0x14 && sizeof(_finddata32_t::name) == 260, "_finddata32_t.name");
static_assert(sizeof(_finddata32_t) == 0x118, "_finddata32_t is MSVC 6's _finddata_t");

extern "C" long __cdecl Crt_findfirst(const char* pattern, void* found) {
    return static_cast<long>(_findfirst32(pattern, static_cast<_finddata32_t*>(found)));
}

extern "C" int __cdecl Crt_findnext(long handle, void* found) {
    return _findnext32(static_cast<intptr_t>(handle), static_cast<_finddata32_t*>(found));
}

// The file layer: every stream is opened, used and closed by ours - File_Open
// / File_OpenWrite / File_Read / File_Write / File_Size / File_Seek /
// File_Close (file_io.cpp) and Cfg_Load (shell.cpp) - so the FILE behind it
// is the toolchain's. The eight go together: a stream one runtime opened must
// not reach the other's (docs/crt-rest.md section 2.4, the A/B note).

// original 0x5B9B6D: fopen, MSVC 6's over _fsopen with _SH_DENYNO; the
// toolchain's opens shared alike. Modes "rb", "wb", "rt".
extern "C" void* __cdecl Crt_fopen(const char* path, const char* mode) {
    return std::fopen(path, mode);
}

// original 0x5B9993.
extern "C" int __cdecl Crt_fclose(void* stream) {
    return std::fclose(static_cast<std::FILE*>(stream));
}

// original 0x5B9D4E.
extern "C" unsigned __cdecl Crt_fread(void* dst, unsigned size, unsigned count, void* stream) {
    return static_cast<unsigned>(std::fread(dst, size, count, static_cast<std::FILE*>(stream)));
}

// original 0x5B9E65.
extern "C" unsigned __cdecl Crt_fwrite(const void* src, unsigned size, unsigned count, void* stream) {
    return static_cast<unsigned>(std::fwrite(src, size, count, static_cast<std::FILE*>(stream)));
}

// original 0x5B9F9E: 0, or -1.
extern "C" int __cdecl Crt_fseek(void* stream, int offset, int whence) {
    return std::fseek(static_cast<std::FILE*>(stream), offset, whence);
}

// original 0x5C3660: the stream's descriptor (MSVC 6's FILE._file).
extern "C" int __cdecl Crt_fileno(void* stream) {
    return _fileno(static_cast<std::FILE*>(stream));
}

// original 0x5C35D6: the descriptor's length, -1 for a bad one.
extern "C" int __cdecl Crt_filelength(int fd) {
    return static_cast<int>(_filelength(fd));
}

// original 0x5B9ADA: fgets - Cfg_Load's two lines of BOF3.CFG, a text stream.
extern "C" char* __cdecl Crt_fgets(char* buf, int n, void* stream) {
    return std::fgets(buf, n, static_cast<std::FILE*>(stream));
}

// original 0x5B9660 / 0x5BA221 / 0x5B9577: malloc, calloc and free of MSVC
// 6's heap - on every Windows the game runs on today HeapAlloc / HeapFree on
// the runtime's own heap, the request rounded up to 16 bytes, a request of 0
// asking 16, one above 0xFFFFFFE0 refused (docs/crt-startup.md section 3;
// calloc multiplies its two arguments in 32 bits and zero-fills). The
// toolchain's three, which keep the same contract for every caller: a block
// to use and to free, or null. Bound as a set, because a block is freed by
// whichever free its allocator pairs with - the three move together, and the
// callers that are not ours (the MP3 decoder's 19 mallocs, 6 callocs and 41
// frees under BOF3X_MP3=original; the runtime's own start-up, which runs
// after the inject) pair inside the set too. The runtime's realloc 0x5C2F8E
// and its direct HeapAlloc / HeapReAlloc paths are not moved: nothing reaches
// them (docs/crt-rest.md section 2.6).
extern "C" void* __cdecl Crt_malloc(unsigned size) { return std::malloc(size); }
extern "C" void* __cdecl Crt_calloc(unsigned count, unsigned size) { return std::calloc(count, size); }
extern "C" void __cdecl Crt_free(void* ptr) { std::free(ptr); }

// original 0x5B9AA6: sscanf, MSVC 6's _input 0x5BCF64 over a string FILE.
// Two formats in the executable reach it: Cfg_Load's "%d %d" 0x65DAD0, and
// the MP3 decoder's "%lx@%lx" (0x672854, 0x6728C8 - Mp3_MemoryIo's open
// reading the size@address name back), which Capcom's decoder still uses
// under BOF3X_MP3=original and in DIV-0091's comparison. Any other format is
// fatal (rule 4). The engine's conversions, read from the disassembly's shape
// and fuzzed against Capcom's (crt_rest_fuzz.cpp):
//   - "%d": the input's whitespace (the "C" table's 0x09..0x0D and 0x20)
//     skipped; one optional '-' or '+'; digits '0'..'9' accumulated in 32
//     bits with no overflow test, so the value wraps modulo 2^32, negated for
//     '-'; nothing written and the scan stopped when no digit follows;
//   - "%lx": the same whitespace and sign; an optional "0x" / "0X" after a
//     leading '0'; hex digits of either case, the same wrap and negation;
//   - a space in the format skips the input's whitespace; the '@' must be
//     the very next byte (no skipping), else the scan stops.
// Each value is written as four bytes at the argument's pointer (Cfg_Load's
// are byte pointers one apart). The answer is the number of values written,
// or -1 when the string ended before the first was read.
namespace {

inline bool Space(unsigned char c) { return c == ' ' || (c >= 0x09 && c <= 0x0D); }
inline int HexDigit(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// One %d (hex false) or %lx (hex true) conversion at p; p is left after it.
bool ScanNumber(const unsigned char*& p, bool hex, std::uint32_t* out, bool* ended) {
    while (Space(*p)) ++p;
    if (*p == 0) {
        *ended = true;
        return false;
    }
    const unsigned char sign = *p;
    if (sign == '-' || sign == '+') ++p;
    std::uint32_t total = 0;
    if (!hex) {
        if (!(*p >= '0' && *p <= '9')) {
            *ended = *p == 0;
            return false;
        }
        while (*p >= '0' && *p <= '9') total = total * 10u + static_cast<std::uint32_t>(*p++ - '0');
    } else {
        if (HexDigit(*p) < 0) {
            *ended = *p == 0;
            return false;
        }
        if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
            if (HexDigit(*p) < 0) {   // "0x" with no digit after it: the conversion fails (fuzzed: "0x@1" answers 0)
                *ended = *p == 0;
                return false;
            }
        }
        for (int d; (d = HexDigit(*p)) >= 0; ++p) total = total * 16u + static_cast<std::uint32_t>(d);
    }
    *out = sign == '-' ? 0u - total : total;
    return true;
}

}  // namespace

extern "C" int __cdecl Crt_sscanf(const char* s, const char* fmt, ...) {
    const bool decimal = std::strcmp(fmt, "%d %d") == 0;
    if (!decimal && std::strcmp(fmt, "%lx@%lx") != 0)
        bof3::Fatal("Crt_sscanf: the format \"%s\" is neither Cfg_Load's \"%%d %%d\" nor the decoder's \"%%lx@%%lx\"", fmt);
    std::va_list args;
    va_start(args, fmt);
    void* const a = va_arg(args, void*);
    void* const b = va_arg(args, void*);
    va_end(args);
    const auto* p = reinterpret_cast<const unsigned char*>(s);
    std::uint32_t v;
    bool ended = false;
    if (!ScanNumber(p, !decimal, &v, &ended)) return ended ? -1 : 0;
    std::memcpy(a, &v, 4);
    if (!decimal) {
        if (*p != '@') return 1;
        ++p;
    }
    if (!ScanNumber(p, !decimal, &v, &ended)) return 1;
    std::memcpy(b, &v, 4);
    return 2;
}

void CrtRest_Inject() {
    if (bof3::WantsShadow("crt_rest")) crt_rest::SelfTest();
    // The randlog's copy of Capcom's Rand, made only when a recipe or a
    // recording may ask for the count and the tracer does not hold Rand's
    // entry (input_script.cpp RandCountStart) - before the inject, which
    // CloneOriginal refuses to follow. Used only if BOF3X_ORIGINAL leaves
    // Rand to Capcom; its one call, Crt_GetPtd at its entry, is kept. Not in
    // the exe host: no code of Capcom's is mapped to copy, and BOF3X_ORIGINAL
    // is refused there, so Rand is always ours and RandCount_Start never
    // reads the copy (without this every recipe run of bof3x.exe stopped
    // here, 2026-10-11).
    if ((GetEnvironmentVariableA("BOF3X_INPUT", nullptr, 0) > 0 ||
         GetEnvironmentVariableA("BOF3X_RECORD", nullptr, 0) > 0) &&
        GetEnvironmentVariableA("BOF3X_CALLTRACE", nullptr, 0) == 0 && !bof3::IsExeHost()) {
        constexpr std::uint32_t kRandSize = 0x22;
        const auto get_ptd = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Crt_GetPtd));   // still Capcom's
        const bof3::CloneCall calls[] = {{0, nullptr, get_ptd}};
        g_original_copy = reinterpret_cast<int (__cdecl*)()>(
            bof3::CloneOriginal("Rand", bof3::addr::Rand, kRandSize, calls, 1));
    }
    BOF3_INJECT(Rand);
    BOF3_INJECT(Crt_sprintf);
    BOF3_INJECT(Crt_strncpy);
    BOF3_INJECT(Crt_atoi);
    BOF3_INJECT(Crt_sscanf);
    BOF3_INJECT(Crt_malloc);
    BOF3_INJECT(Crt_calloc);
    BOF3_INJECT(Crt_free);
    BOF3_INJECT(Crt_stricmp);
    BOF3_INJECT(Crt_findfirst);
    BOF3_INJECT(Crt_findnext);
    BOF3_INJECT(Crt_fopen);
    BOF3_INJECT(Crt_fclose);
    BOF3_INJECT(Crt_fread);
    BOF3_INJECT(Crt_fwrite);
    BOF3_INJECT(Crt_fseek);
    BOF3_INJECT(Crt_fileno);
    BOF3_INJECT(Crt_filelength);
    BOF3_INJECT(Crt_fgets);
    // Every module's start-up fuzz ran before this, and any of ours it ran may
    // have drawn ours: the game's sequence starts from the CRT's 1, uncounted.
    g_seed = 1;
    g_count = 0;
}
