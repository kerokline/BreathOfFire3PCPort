// Byte-level access shared by the movement-script files: the script context's
// position, unaligned words and dwords, the PSX scratchpad's words, and the
// jump-table relocation their start-up fuzzes need. docs/movement-script.md.
#pragma once

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/log.h"

namespace move_script {

// The script position: the u16 at +0xA of the script context. Every store is
// 16 bits, so arithmetic on it wraps at 0x10000 as the original's does.
inline std::uint16_t Pos(const unsigned char* object) {
    std::uint16_t v;
    std::memcpy(&v, object + 0xA, sizeof v);
    return v;
}
inline void SetPos(unsigned char* object, unsigned v) {
    const auto p = static_cast<std::uint16_t>(v);
    std::memcpy(object + 0xA, &p, sizeof p);
}
inline std::uint16_t Word(const unsigned char* at) {
    std::uint16_t v;
    std::memcpy(&v, at, sizeof v);
    return v;
}
inline void SetWord(unsigned char* at, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(at, &w, sizeof w);
}
inline std::int32_t Long(const unsigned char* at) {
    std::int32_t v;
    std::memcpy(&v, at, sizeof v);
    return v;
}
inline void SetLong(unsigned char* at, std::int32_t v) { std::memcpy(at, &v, sizeof v); }
inline unsigned char* At(std::uint32_t address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

// The PSX scratchpad's first words (0x1F800000), where the PC keeps them; the
// A ops' counters sit in the 8 bytes below.
inline unsigned char* Scratch() { return At(bof3::addr::DamageScratch); }

// A clone's jump table holds absolute addresses into the original: move the
// entries and the `jmp [reg*4 + table]` operand into the copy.
struct Table { std::uint32_t jmp_disp, table, entries; };
inline void Relocate(void* copy, std::uint32_t base, std::uint32_t size, const Table& t) {
    auto* code = static_cast<std::uint8_t*>(copy);
    const std::uint32_t moved = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code)) - base;
    for (std::uint32_t i = 0; i < t.entries; ++i) {
        std::uint32_t target;
        std::memcpy(&target, code + t.table + 4 * i, sizeof target);
        if (target < base || target >= base + size)
            bof3::Fatal("move_script: jump table +0x%X entry %u is 0x%X", (unsigned)t.table, (unsigned)i, (unsigned)target);
        target += moved;
        std::memcpy(code + t.table + 4 * i, &target, sizeof target);
    }
    std::uint32_t disp;
    std::memcpy(&disp, code + t.jmp_disp, sizeof disp);
    if (disp != base + t.table) bof3::Fatal("move_script: no jump table operand at +0x%X", (unsigned)t.jmp_disp);
    disp += moved;
    std::memcpy(code + t.jmp_disp, &disp, sizeof disp);
}

}  // namespace move_script
