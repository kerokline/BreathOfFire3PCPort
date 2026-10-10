// rdata_consts.h's copies against BOF3.exe's .rdata (docs/exe-import-engine.md section 2).
#include "game/rdata_consts.h"

#include <cstring>

#include "hook/log.h"

namespace rdata {

static_assert(sizeof kEntries / sizeof kEntries[0] == 44, "40 literals and 4 GUIDs: the doc's count");
static_assert(kLo == kEntries[0].pc, "kLo is the first entry");

consteval bool Sorted() {
    std::uint32_t end = 0;
    for (const Entry& e : kEntries) {
        if (e.pc < end || e.pc % 4 || (e.size == 8 && (e.pc - kLo) % 8) || e.pc + e.size > kHi) return false;
        end = e.pc + e.size;
    }
    return true;
}
static_assert(Sorted(), "entries sorted, aligned, not overlapping, inside [kLo, kHi)");

void Verify() {
    const auto* ours = reinterpret_cast<const unsigned char*>(kCopies.data());
    for (const Entry& e : kEntries) {
        const auto* theirs = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(e.pc));
        if (std::memcmp(ours + (e.pc - kLo), theirs, e.size) != 0)
            bof3::Fatal("rdata: 0x%06X (%s): the engine's copy is not BOF3.exe's .rdata", (unsigned)e.pc, e.what);
    }
    bof3::Log("rdata: %u constants (%s .. %s) held by the engine, each equal to BOF3.exe's .rdata",
              (unsigned)(sizeof kEntries / sizeof kEntries[0]), kEntries[0].what,
              kEntries[sizeof kEntries / sizeof kEntries[0] - 1].what);
}

}  // namespace rdata
