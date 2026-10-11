#include "hook/code_bytes.h"

#include <windows.h>

#include <cstring>
#include <vector>

#include "hook/exe_layout.h"
#include "hook/host.h"
#include "hook/log.h"

namespace bof3 {
namespace {

using exe_layout::kStubSize;
using exe_layout::kStubVa;

HANDLE g_file = INVALID_HANDLE_VALUE;
std::uint32_t g_text_offset = 0, g_text_raw = 0;
std::uint8_t* g_store = nullptr;          // kStubSize reserved; pages committed as loaded
std::vector<std::uint8_t> g_loaded;       // a bit a byte of .stub: held in the store
std::uint32_t g_ranges = 0, g_bytes = 0;  // loads, and bytes loaded

CRITICAL_SECTION g_lock;   // initialised by CodeBytes_Open, before any read can reach Held

// Every range asked for, once loaded: a read inside one of them needs no lock
// and no walk of the bits (the draw dispatch reads its byte table per
// primitive). Append-only: an entry is written before the count that
// publishes it.
struct Span {
    std::uint32_t lo, hi;
};
constexpr unsigned kMaxSpans = 512;
Span g_spans[kMaxSpans];
volatile LONG g_span_count = 0;

bool Spanned(std::uint32_t off, std::uint32_t n) {
    const LONG count = g_span_count;
    for (LONG i = 0; i < count; ++i)
        if (off >= g_spans[i].lo && off + n <= g_spans[i].hi) return true;
    return false;
}

bool Loaded(std::uint32_t off) { return (g_loaded[off >> 3] >> (off & 7)) & 1; }
void MarkLoaded(std::uint32_t off) { g_loaded[off >> 3] = static_cast<std::uint8_t>(g_loaded[off >> 3] | 1u << (off & 7)); }

// The exe host's bytes of [va, va + n), loaded from the file where not yet held.
std::uint8_t* Held(std::uint32_t va, std::uint32_t n) {
    if (g_file == INVALID_HANDLE_VALUE)
        Fatal("codebytes: 0x%08X read before the player's BOF3.exe was opened (Engine_Main's order)", (unsigned)va);
    if (n == 0 || va < kStubVa || va + n > kStubVa + kStubSize || va + n < va)
        Fatal("codebytes: 0x%08X + %u is not inside Capcom's code range", (unsigned)va, (unsigned)n);
    const std::uint32_t off = va - kStubVa;
    if (Spanned(off, n)) return g_store + off;
    EnterCriticalSection(&g_lock);
    std::uint32_t missing = 0;
    for (std::uint32_t i = 0; i < n; ++i) missing += !Loaded(off + i);
    if (missing) {
        const std::uint32_t page_lo = off & ~0xFFFu, page_hi = (off + n + 0xFFF) & ~0xFFFu;
        if (!VirtualAlloc(g_store + page_lo, page_hi - page_lo, MEM_COMMIT, PAGE_READWRITE))
            Fatal("codebytes: cannot commit the store at +0x%X, error %lu", (unsigned)page_lo, GetLastError());
        if (off + n > g_text_raw)
            Fatal("codebytes: 0x%08X + %u lies past .text's file bytes (0x%X)", (unsigned)va, (unsigned)n, (unsigned)g_text_raw);
        std::vector<std::uint8_t> file(n);
        LARGE_INTEGER at;
        at.QuadPart = static_cast<LONGLONG>(g_text_offset) + off;
        DWORD got = 0;
        if (!SetFilePointerEx(g_file, at, nullptr, FILE_BEGIN) || !ReadFile(g_file, file.data(), n, &got, nullptr) ||
            got != n)
            Fatal("codebytes: cannot read 0x%08X + %u from BOF3.exe, error %lu", (unsigned)va, (unsigned)n, GetLastError());
        // Only the bytes not held yet: a byte a patch wrote keeps the patch.
        for (std::uint32_t i = 0; i < n; ++i) {
            if (Loaded(off + i)) continue;
            g_store[off + i] = file[i];
            MarkLoaded(off + i);
        }
        ++g_ranges;
        g_bytes += missing;
        Log("codebytes   0x%08X + %u read from BOF3.exe (%u new bytes; the store holds %u in %u reads)", (unsigned)va,
            (unsigned)n, (unsigned)missing, (unsigned)g_bytes, (unsigned)g_ranges);
    }
    if (!Spanned(off, n)) {
        if (g_span_count == static_cast<LONG>(kMaxSpans))
            Fatal("codebytes: more than %u distinct ranges read; raise kMaxSpans", kMaxSpans);
        g_spans[g_span_count] = {off, off + n};
        InterlockedIncrement(&g_span_count);
    }
    LeaveCriticalSection(&g_lock);
    return g_store + off;
}

}  // namespace

void CodeBytes_Open(void* file, std::uint32_t text_file_offset, std::uint32_t text_raw_size) {
    if (g_file != INVALID_HANDLE_VALUE) Fatal("codebytes: opened twice");
    InitializeCriticalSection(&g_lock);
    g_file = static_cast<HANDLE>(file);
    g_text_offset = text_file_offset;
    g_text_raw = text_raw_size;
    g_store = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, kStubSize, MEM_RESERVE, PAGE_READWRITE));
    if (!g_store) Fatal("codebytes: cannot reserve the store (0x%X bytes), error %lu", (unsigned)kStubSize, GetLastError());
    g_loaded.assign(kStubSize / 8, 0);
    Log("codebytes   the store for Capcom's code bytes our code reads: %p (BOF3.exe's .text at file offset 0x%X, "
        "0x%X bytes)", static_cast<void*>(g_store), (unsigned)text_file_offset, (unsigned)text_raw_size);
}

const std::uint8_t* CodeBytes(std::uint32_t va, std::uint32_t n) {
    if (!IsExeHost() || va < kStubVa || va >= kStubVa + kStubSize)
        return reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(va));
    return Held(va, n);
}

std::uint8_t* CodeBytesForWrite(std::uint32_t va, std::uint32_t n) {
    if (!IsExeHost()) Fatal("codebytes: a write to the store at 0x%08X in the hosted build", (unsigned)va);
    return Held(va, n);
}

void CodeBytes_Report(const char* when) {
    if (!IsExeHost()) return;
    Log("codebytes   %s: %u reads, %u bytes of BOF3.exe's .text held as data (none of it at its address, none "
        "executable)", when, (unsigned)g_ranges, (unsigned)g_bytes);
}

std::uint16_t CodeWord(std::uint32_t va) {
    std::uint16_t v;
    std::memcpy(&v, CodeBytes(va, 2), 2);
    return v;
}

std::uint32_t CodeLong(std::uint32_t va) {
    std::uint32_t v;
    std::memcpy(&v, CodeBytes(va, 4), 4);
    return v;
}

std::uint32_t CodeCallTarget(std::uint32_t site) {
    const std::uint8_t* p = CodeBytes(site, 5);
    std::uint32_t rel;
    std::memcpy(&rel, p + 1, 4);
    return site + 5 + rel;
}

}  // namespace bof3
