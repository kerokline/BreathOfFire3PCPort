// Windows-only by design (docs/linux-round.md, LB1): a harness-only watcher over VirtualProtect and guard pages.
// BOF3X_WRITEWATCH - see write_watch.h and docs/write-watch.md.
#include "hook/write_watch.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/image_base.h"
#include "hook/cpu_context.h"
#include "hook/input_script.h"
#include "hook/log.h"

namespace write_watch {
namespace {

constexpr int kWords = 4;                 // DR0..DR3
constexpr DWORD kSetDr = 0xE0575757;      // a code of ours, raised to load the registers (gapwatch.cpp's pattern)
constexpr int kCallers = 6;               // return addresses into the DLL kept for a hit
constexpr int kWriters = 16;              // distinct writers remembered a word
constexpr unsigned kEvery = 256;          // a known writer is logged again every kEvery-th write

// Everything the handler touches is static: it runs on whatever stack took
// the trap, and game logic runs on 16 KB task stacks (calltrace.cpp).
struct Writer {
    std::uintptr_t ip;
    unsigned count;
};
struct Word {
    std::uint32_t place;   // where the word lies in this process (Img of its VA)
    std::uint32_t va;      // the image's VA, for the log
    std::uint32_t last;    // its value as last seen (before the next write)
    unsigned writes;
    Writer writers[kWriters];
    int nwriters;
};
Word g_word[kWords];
int g_words = 0;
DWORD g_thread = 0;
std::uintptr_t g_dll_lo = 0, g_dll_hi = 0;

std::uint32_t Read32(std::uint32_t place) {
    return *reinterpret_cast<const volatile std::uint32_t*>(static_cast<std::uintptr_t>(place));
}

bool InDll(std::uintptr_t v) { return v >= g_dll_lo + 6 && v < g_dll_hi; }

// A stack word that points into our DLL just after a call (crash.cpp's and
// gapwatch.cpp's filter: E8 rel32, or FF in the 2-, 3- and 6-byte forms).
bool AfterCall(std::uintptr_t v) {
    if (!InDll(v)) return false;
    const auto* b = reinterpret_cast<const std::uint8_t*>(v - 6);
    return b[1] == 0xE8 || b[4] == 0xFF || b[3] == 0xFF || b[0] == 0xFF;
}

int Callers(const CONTEXT* ctx, std::uintptr_t* out) {
    const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const std::uintptr_t top = reinterpret_cast<std::uintptr_t>(tib->StackBase);
    std::uintptr_t at = bof3::CtxSp(*ctx);
    int n = 0;
    if (at >= top || (at & 3)) return 0;
    const std::uintptr_t end = top - at > 0x1000 ? at + 0x1000 : top;
    for (; at + sizeof at <= end && n < kCallers; at += sizeof at) {
        const std::uintptr_t v = *reinterpret_cast<const std::uintptr_t*>(at);
        if (AfterCall(v)) out[n++] = v;
    }
    return n;
}

// An address as the log prints it: a DLL's as name+offset (our DLL's base is
// not fixed); the main module's, or code in no module (the exe host's Capcom
// image), as the image's VA - exe:XXXXXXXX.
void Name(std::uintptr_t ip, char* out, std::size_t size) {
    HMODULE m = nullptr;
    char path[MAX_PATH] = "";
    const bool module = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                           reinterpret_cast<LPCSTR>(ip), &m) != 0;
    if (module && m != GetModuleHandleA(nullptr) && GetModuleFileNameA(m, path, sizeof path)) {
        const char* base = std::strrchr(path, '\\');
        std::snprintf(out, size, "%s+0x%llX", base ? base + 1 : path,
                      static_cast<unsigned long long>(ip - reinterpret_cast<std::uintptr_t>(m)));
        return;
    }
    std::snprintf(out, size, "exe:%08X", static_cast<unsigned>(ip - bof3::ImageBase() + bof3::image::kBase));
}

// True when this writer should be logged now: new, or its kEvery-th write.
bool Note(Word& w, std::uintptr_t ip, unsigned* count) {
    for (int k = 0; k < w.nwriters; ++k)
        if (w.writers[k].ip == ip) {
            *count = ++w.writers[k].count;
            return *count % kEvery == 0;
        }
    if (w.nwriters < kWriters) w.writers[w.nwriters++] = {ip, 1};
    *count = 1;
    return true;
}

void Hit(Word& w, const CONTEXT* ctx) {
    const std::uint32_t now = Read32(w.place);   // a write-only register: this read does not trap
    const std::uint32_t was = w.last;
    w.last = now;
    ++w.writes;
    const std::uintptr_t ip = bof3::CtxIp(*ctx);   // the instruction after the write: the trap follows it
    unsigned count = 0;
    if (!Note(w, ip, &count)) return;
    char where[96], callers[kCallers * 72] = "";
    Name(ip, where, sizeof where);
    std::uintptr_t c[kCallers];
    const int n = Callers(ctx, c);
    std::size_t len = 0;
    for (int k = 0; k < n && len < sizeof callers; ++k) {
        char one[72];
        Name(c[k], one, sizeof one);
        len += static_cast<std::size_t>(std::snprintf(callers + len, sizeof callers - len, " %s", one));
    }
    const std::uint32_t frame = Read32(bof3::Img(Frame_Counter_addr));
    const unsigned area = *reinterpret_cast<const volatile std::uint16_t*>(static_cast<std::uintptr_t>(bof3::Img(Game_AreaNumber_addr)));
    bof3::Log("writewatch  0x%08X: %08X -> %08X by %s (that writer's write %u, the word's %u); frame %u, recipe frame %u, "
              "area %u; callers:%s",
              w.va, was, now, where, count, w.writes, frame, bof3::InputScript_Frame(), area, n ? callers : " none in the DLL");
}

// The words into a context: register k write-only (RW 01), four bytes
// (LEN 11), locally enabled.
void Load(CONTEXT* ctx) {
    decltype(ctx->Dr0)* const dr[kWords] = {&ctx->Dr0, &ctx->Dr1, &ctx->Dr2, &ctx->Dr3};
    DWORD dr7 = 0;
    for (int k = 0; k < kWords; ++k) {
        *dr[k] = k < g_words ? g_word[k].place : 0;
        if (k < g_words) dr7 |= (1u << (2 * k)) | (1u << (16 + 4 * k)) | (3u << (18 + 4 * k));
    }
    ctx->Dr6 = 0;
    ctx->Dr7 = dr7;
    ctx->ContextFlags |= CONTEXT_DEBUG_REGISTERS;
}

LONG CALLBACK OnException(EXCEPTION_POINTERS* info) {
    EXCEPTION_RECORD* rec = info->ExceptionRecord;
    CONTEXT* ctx = info->ContextRecord;
    if (rec->ExceptionCode == kSetDr) {
        Load(ctx);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (rec->ExceptionCode != EXCEPTION_SINGLE_STEP || GetCurrentThreadId() != g_thread) return EXCEPTION_CONTINUE_SEARCH;
    if ((ctx->ContextFlags & CONTEXT_DEBUG_REGISTERS) != CONTEXT_DEBUG_REGISTERS || !(ctx->Dr6 & 0xF))
        return EXCEPTION_CONTINUE_SEARCH;   // a trap-flag step: someone else's (the call trace)
    for (int k = 0; k < g_words; ++k)
        if (ctx->Dr6 & (1u << k)) Hit(g_word[k], ctx);
    ctx->Dr6 = 0;
    return EXCEPTION_CONTINUE_EXECUTION;
}

// Every word of [va, va + length) on a committed, writable page once placed:
// a VA outside the image is refused here, before a register holds it.
bool Committed(std::uint32_t va, std::uint32_t length) {
    for (std::uint32_t at = va & ~3u; at < va + length; at += 4) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(bof3::Img(at))), &mbi, sizeof mbi) ||
            mbi.State != MEM_COMMIT ||
            !(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            return false;
    }
    return true;
}

bool Add(std::uint32_t va, char* why, std::size_t size) {
    for (int k = 0; k < g_words; ++k)
        if (g_word[k].va == va) return true;
    if (g_words == kWords) {
        std::snprintf(why, size, "more than %d aligned words", kWords);
        return false;
    }
    Word& w = g_word[g_words++];
    std::memset(&w, 0, sizeof w);
    w.va = va;
    w.place = bof3::Img(va);
    w.last = Read32(w.place);
    return true;
}

}  // namespace

void Start() {
    char text[256];
    const DWORD n = GetEnvironmentVariableA("BOF3X_WRITEWATCH", text, sizeof text);
    if (n == 0) return;
    if (n >= sizeof text) bof3::Fatal("BOF3X_WRITEWATCH: longer than %u characters", static_cast<unsigned>(sizeof text - 1));
    if (GetEnvironmentVariableA("BOF3X_GAPWATCH", nullptr, 0) > 0)
        bof3::Fatal("BOF3X_WRITEWATCH: refused beside BOF3X_GAPWATCH, which uses the same debug registers");
    char why[96] = "";
    for (char* item = std::strtok(text, ","); item; item = std::strtok(nullptr, ",")) {
        char* end = nullptr;
        const unsigned long va = std::strtoul(item, &end, 16);
        unsigned long length = 4;
        if (*end == ':') length = std::strtoul(end + 1, &end, 0);
        if (*end != '\0' || length == 0 || length > 0x10 ||
            !Committed(static_cast<std::uint32_t>(va), static_cast<std::uint32_t>(length)))
            bof3::Fatal("BOF3X_WRITEWATCH=%s: want <hex VA>[:<length>],... on writable pages of the image (docs/write-watch.md)",
                        item);
        for (std::uint32_t w = static_cast<std::uint32_t>(va) & ~3u; w < va + length; w += 4)
            if (!Add(w, why, sizeof why)) bof3::Fatal("BOF3X_WRITEWATCH: %s (the CPU has four debug registers)", why);
    }
    HMODULE self = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&Start), &self))
        bof3::Fatal("BOF3X_WRITEWATCH: our own module not found");
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(self);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const std::uint8_t*>(self) + dos->e_lfanew);
    g_dll_lo = reinterpret_cast<std::uintptr_t>(self);
    g_dll_hi = g_dll_lo + nt->OptionalHeader.SizeOfImage;
    if (!AddVectoredExceptionHandler(1, OnException)) bof3::Fatal("BOF3X_WRITEWATCH: no vectored handler");
    g_thread = GetCurrentThreadId();
    RaiseException(kSetDr, 0, 0, nullptr);
    for (int k = 0; k < g_words; ++k)
        bof3::Log("writewatch  watching 0x%08X (place %08X, now %08X) for writes on thread %lu, debug register %d", g_word[k].va,
                  g_word[k].place, g_word[k].last, static_cast<unsigned long>(g_thread), k);
}

}  // namespace write_watch
