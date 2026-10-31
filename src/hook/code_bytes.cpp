#include "hook/code_bytes.h"

#include <cstring>

#include "hook/detour.h"
#include "hook/host.h"
#include "hook/log.h"

namespace bof3 {
namespace {

enum class Kind : std::uint8_t { Bytes, Call, Rule };

struct Entry {
    const char* site;
    std::uint32_t va, n;
    Kind kind;
    const std::uint8_t* ours;     // Bytes: n bytes; Call: a std::uint32_t target
    std::uint8_t held[64];        // OperandExpect's copy
    bool (*same)();               // Rule
    bool under_jmp;               // under a jmp of ours: compared before it went in (or never, if registered after)
    bool compared;                // compared at least once
};

// The inventory: about forty sites, each registered once (the chunk paths
// register again on a second load and are ignored).
constexpr unsigned kMax = 256;
Entry g_entries[kMax];
unsigned g_count = 0;
bool g_late = false;   // InjectAll is done: a new entry is checked as it comes

Entry* Add(const char* site, std::uint32_t va, std::uint32_t n, Kind kind) {
    if (n == 0 || n > 0x1000) Fatal("operands: %s at 0x%08X: %u bytes", site, (unsigned)va, (unsigned)n);
    for (unsigned i = 0; i < g_count; ++i)
        if (g_entries[i].va == va && g_entries[i].n == n && std::strcmp(g_entries[i].site, site) == 0) return nullptr;
    if (g_count == kMax) Fatal("operands: more than %u values of ours; raise kMax", kMax);
    Entry& e = g_entries[g_count++];
    e.site = site;
    e.va = va;
    e.n = n;
    e.kind = kind;
    return &e;
}

// Whether [va, va + n) overlaps the five bytes of a jmp of ours at a function start.
bool UnderJmp(std::uint32_t va, std::uint32_t n) {
    for (int i = 0; i < OwnedCount(); ++i) {
        const std::uint32_t a = OwnedAddress(i);
        if (a < va + n && a + 5 > va && IsEnabled(a)) return true;
    }
    return false;
}

const std::uint8_t* Code(std::uint32_t va) { return reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(va)); }

void CheckOne(Entry& e) {
    e.compared = true;
    if (!InCode(e.va) || !InCode(e.va + e.n - 1))
        Fatal("operands: %s names 0x%08X + %u, outside Capcom's code", e.site, (unsigned)e.va, (unsigned)e.n);
    switch (e.kind) {
    case Kind::Bytes:
        for (std::uint32_t k = 0; k < e.n; ++k)
            if (Code(e.va)[k] != e.ours[k])
                Fatal("operands: %s - the code at 0x%08X holds 0x%02X, our value 0x%02X (%u bytes from 0x%08X)", e.site,
                      (unsigned)(e.va + k), Code(e.va)[k], e.ours[k], (unsigned)e.n, (unsigned)e.va);
        return;
    case Kind::Call: {
        std::uint32_t target, rel;
        std::memcpy(&target, e.ours, 4);
        std::memcpy(&rel, Code(e.va) + 1, 4);
        if (Code(e.va)[0] != 0xE8 || e.va + 5 + rel != target)
            Fatal("operands: %s - 0x%08X is not a call to 0x%08X, our value (0x%02X, reaching 0x%08X)", e.site,
                  (unsigned)e.va, (unsigned)target, Code(e.va)[0], (unsigned)(e.va + 5 + rel));
        return;
    }
    case Kind::Rule:
        if (!e.same()) Fatal("operands: %s - the code at 0x%08X + %u is not what our rule holds", e.site, (unsigned)e.va, (unsigned)e.n);
        return;
    }
}

void Late(Entry* e) {
    if (!e || IsExeHost()) return;
    if (UnderJmp(e->va, e->n)) {
        e->under_jmp = true;   // its bytes are our jmp's already: nothing to compare
        // Named now, not only at the next Operands_Check: an entry registered
        // after the last check would otherwise go unlogged (operand-store.md 1).
        Log("operands    %s at 0x%08X + %u: registered after a jmp of ours covered it, never compared", e->site,
            (unsigned)e->va, (unsigned)e->n);
        return;
    }
    if (g_late) CheckOne(*e);
}

}  // namespace

void Operand(const char* site, std::uint32_t va, const void* ours, std::uint32_t n) {
    Entry* e = Add(site, va, n, Kind::Bytes);
    if (e) e->ours = static_cast<const std::uint8_t*>(ours);
    Late(e);
}

void OperandExpect(const char* site, std::uint32_t va, const void* bytes, std::uint32_t n) {
    if (n > sizeof g_entries[0].held) Fatal("operands: %s at 0x%08X: %u bytes, above the %u held", site, (unsigned)va, (unsigned)n, (unsigned)sizeof g_entries[0].held);
    Entry* e = Add(site, va, n, Kind::Bytes);
    if (e) {
        std::memcpy(e->held, bytes, n);
        e->ours = e->held;
    }
    Late(e);
}

void OperandCall(const char* site, std::uint32_t va, const std::uint32_t* target) {
    Entry* e = Add(site, va, 5, Kind::Call);
    if (e) e->ours = reinterpret_cast<const std::uint8_t*>(target);
    Late(e);
}

void OperandRule(const char* site, std::uint32_t va, std::uint32_t n, bool (*same)()) {
    Entry* e = Add(site, va, n, Kind::Rule);
    if (e) e->same = same;
    Late(e);
}

void Operands_BeforeJmp(std::uint32_t at) {
    if (IsExeHost()) return;
    for (unsigned i = 0; i < g_count; ++i) {
        Entry& e = g_entries[i];
        if (e.under_jmp || !(at < e.va + e.n && at + 5 > e.va)) continue;
        CheckOne(e);
        e.under_jmp = true;
    }
}

void Operands_Check(const char* when) {
    std::uint32_t bytes = 0;
    for (unsigned i = 0; i < g_count; ++i) bytes += g_entries[i].n;
    if (IsExeHost()) {
        if (!g_late)
            Log("operands    %s: %u values of ours stand for %u bytes of Capcom's code (docs/operand-store.md); "
                "none read, there is no code to compare in this host", when, g_count, (unsigned)bytes);
        g_late = true;
        return;
    }
    unsigned now = 0, before = 0, never = 0;
    for (unsigned i = 0; i < g_count; ++i) {
        Entry& e = g_entries[i];
        if (!e.under_jmp) {
            CheckOne(e);
            ++now;
        } else if (e.compared) {
            ++before;
        } else {
            ++never;
            Log("operands    %s at 0x%08X + %u: registered under a jmp of ours, never compared", e.site, (unsigned)e.va, (unsigned)e.n);
        }
    }
    Log("operands    %s: %u values of ours (%u bytes) - %u equal to the patched code at their addresses now, %u "
        "before a jmp of ours covered them, %u never compared", when, g_count, (unsigned)bytes, now, before, never);
    g_late = true;
}

}  // namespace bof3
