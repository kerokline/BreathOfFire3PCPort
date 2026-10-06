// The C runtime's entry points the game calls: rand reimplemented exactly,
// sprintf over the conversions the game's formats use, and the string, search
// and file entries bound to our toolchain's runtime through thin named
// functions (docs/crt-rest.md; docs/platform-layers-plan.md section 2.3).
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
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
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

[[noreturn]] void RefuseFormat(const char* fmt, const char* spec, std::size_t len) {
    bof3::Fatal("Crt_sprintf: \"%s\" has the conversion \"%.*s\", which no format of the game uses and ours does not "
                "take (docs/crt-rest.md section 2.2)",
                fmt, static_cast<int>(len), spec);
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
    constexpr std::uint32_t kRand = bof3::addr::Rand;
    bof3::Inject("Rand", kRand, reinterpret_cast<void*>(&CountingOriginal), true);
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
            if (width > kMaxWidth) RefuseFormat(fmt, spec, static_cast<std::size_t>(p - spec));
        }
        const char conv = *p;
        if (conv == '\0') RefuseFormat(fmt, spec, static_cast<std::size_t>(p - spec));
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
            RefuseFormat(fmt, spec, speclen);
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

void CrtRest_Inject() {
    if (bof3::WantsShadow("crt_rest")) crt_rest::SelfTest();
    // The randlog's copy of Capcom's Rand, made only when a recipe or a
    // recording may ask for the count and the tracer does not hold Rand's
    // entry (input_script.cpp RandCountStart) - before the inject, which
    // CloneOriginal refuses to follow. Used only if BOF3X_ORIGINAL leaves
    // Rand to Capcom; its one call, Crt_GetPtd at its entry, is kept.
    if ((GetEnvironmentVariableA("BOF3X_INPUT", nullptr, 0) > 0 ||
         GetEnvironmentVariableA("BOF3X_RECORD", nullptr, 0) > 0) &&
        GetEnvironmentVariableA("BOF3X_CALLTRACE", nullptr, 0) == 0) {
        constexpr std::uint32_t kRandSize = 0x22;
        const auto get_ptd = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Crt_GetPtd));   // still Capcom's
        const bof3::CloneCall calls[] = {{0, nullptr, get_ptd}};
        g_original_copy = reinterpret_cast<int (__cdecl*)()>(
            bof3::CloneOriginal("Rand", bof3::addr::Rand, kRandSize, calls, 1));
    }
    BOF3_INJECT(Rand);
    BOF3_INJECT(Crt_sprintf);
    BOF3_INJECT(Crt_strncpy);
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
