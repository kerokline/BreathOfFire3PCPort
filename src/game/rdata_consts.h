// The constants of BOF3.exe's .rdata that our code reads, held by the engine.
//
// docs/exe-import-engine.md section 2. `src/` names 65 addresses in .rdata;
// 40 of them are the compiler's float and double literals (1.0f, 0.5f,
// 340.0f, the double 1/32, ...) and four are COM / DirectInput interface
// identifiers. Those are the 2001 source's literals and the SDK's public
// GUIDs, not game data, so the engine holds them here and needs no copy of
// the PC's .rdata for them (state 3 of docs/platform-layers-plan.md section 3
// maps no .rdata). The other 21 - the import address table, the DirectInput
// keyboard data format, four floats only Capcom's code reads (widescreen.cpp
// checks the operands that name them), three literals that are no .rdata
// read - are listed in the doc with why they stay.
//
// Each constant keeps its PC address, a load-bearing constant (CLAUDE.md
// rule 3): it names the copy it replaces. A `Const` converts to the address
// of the engine's copy, so a reader written `At(kQuadEight)` or
// `F(at::kZero)` reads ours without changing shape; the constructor is
// consteval and refuses, at compile time, an address this table does not
// hold. While the game is hosted, Verify() (InjectAll's first step) compares
// every copy with BOF3.exe's own bytes and stops with a Fatal on any
// difference, so holding them is a verification, not a divergence.
#pragma once

#include <array>
#include <bit>
#include <cstdint>

namespace rdata {

struct Entry {
    std::uint32_t pc;      // the address in BOF3.exe's .rdata
    std::uint32_t size;    // 4 (a float), 8 (a double) or 16 (a GUID)
    std::uint64_t lo, hi;  // its bytes, little-endian
    const char* what;
};

constexpr Entry F(std::uint32_t pc, float v, const char* what) {
    return {pc, 4, std::bit_cast<std::uint32_t>(v), 0, what};
}
constexpr Entry D(std::uint32_t pc, double v, const char* what) {
    return {pc, 8, std::bit_cast<std::uint64_t>(v), 0, what};
}
// A GUID {d1-d2-d3-d4[0..1]-d4[2..7]} as the SDK writes it, laid out as in memory.
constexpr Entry G(std::uint32_t pc, std::uint32_t d1, std::uint16_t d2, std::uint16_t d3, std::uint64_t d4,
                  const char* what) {
    std::uint64_t be = 0;   // Data4 is a byte array: its first byte lowest in memory
    for (int i = 0; i < 8; ++i) be |= ((d4 >> (8 * (7 - i))) & 0xFF) << (8 * i);
    return {pc, 16, d1 | std::uint64_t(d2) << 32 | std::uint64_t(d3) << 48, be, what};
}

// Sorted by address. The readers are listed in docs/exe-import-engine.md section 2.
inline constexpr Entry kEntries[] = {
    F(0x5C41B8, 1.0f, "1.0f"),
    F(0x5C41BC, 3.0f, "3.0f"),
    F(0x5C41C0, 2.0f, "2.0f"),
    F(0x5C41C8, 4.0f, "4.0f"),
    F(0x5C41CC, 8.0f, "8.0f"),
    F(0x5C41D0, 16.0f, "16.0f"),
    F(0x5C41D4, 32.0f, "32.0f"),
    F(0x5C41D8, 0.5f, "0.5f"),
    F(0x5C41DC, 0.0f, "0.0f"),
    F(0x5C41E0, 0.0625f, "0.0625f"),
    F(0x5C41E4, 64.0f, "64.0f"),
    F(0x5C41EC, 48.0f, "48.0f"),
    F(0x5C41F0, 24.0f, "24.0f"),
    F(0x5C41F4, 37.0f, "37.0f"),
    F(0x5C41F8, 12.0f, "12.0f"),
    F(0x5C41FC, 300.0f, "300.0f"),
    F(0x5C4200, -150.0f, "-150.0f"),
    F(0x5C4204, 420.0f, "420.0f"),
    F(0x5C4208, -100.0f, "-100.0f"),
    F(0x5C420C, 380.0f, "380.0f"),
    F(0x5C4210, -60.0f, "-60.0f"),
    F(0x5C4228, 340.0f, "340.0f"),
    F(0x5C422C, -20.0f, "-20.0f"),
    F(0x5C4248, 89.0f, "89.0f"),
    F(0x5C424C, 90.0f, "90.0f"),
    F(0x5C4250, 512.0f, "512.0f"),
    F(0x5C4254, 13.0f, "13.0f"),
    F(0x5C4274, 40.0f, "40.0f"),
    F(0x5C4278, 88.0f, "88.0f"),
    F(0x5C427C, 127.0f, "127.0f"),
    G(0x5C4488, 0x93281502, 0x8CF8, 0x11D0, 0x89AB00A0C9054129ull, "IID_IDirect3DTexture2"),
    G(0x5C45B8, 0xB0210783, 0x89CD, 0x11D0, 0xAF0800A0C925CD16ull, "IID_IDirectSoundNotify"),
    F(0x5C4608, 65536.0f, "65536.0f"),
    F(0x5C460C, 1.0f / 320.0f, "1 / 320.0f"),
    F(0x5C4610, 0.1f, "0.1f"),
    F(0x5C4614, 1.0f / 128.0f, "1 / 128.0f"),
    D(0x5C4618, 1.0 / 32.0, "the double 1 / 32"),
    F(0x5C4620, 1.0f / 255.0f, "1 / 255.0f"),
    F(0x5C4648, 10000.0f, "10000.0f"),
    F(0x5C464C, 1.0f / 127.0f, "1 / 127.0f"),
    D(0x5C4650, 1.0 / 3.14, "the double 1 / 3.14"),
    D(0x5C4658, 2048.0, "the double 2048.0"),
    G(0x5C4718, 0x5944E682, 0xC92E, 0x11CF, 0xBFC7444553540000ull, "IID_IDirectInputDevice2A"),
    G(0x5C4828, 0x6F1D2B61, 0xD5A0, 0x11CF, 0xBFC7444553540000ull, "GUID_SysKeyboard (DInput_KeyboardGuid)"),
};

inline constexpr std::uint32_t kLo = 0x5C41B8;   // the first entry; the copies are kept at their offsets from it
inline constexpr std::uint32_t kHi = 0x5C4838;   // past the last

consteval bool Held(std::uint32_t pc) {
    for (const Entry& e : kEntries)
        if (e.pc == pc) return true;
    return false;
}

consteval std::array<std::uint32_t, (kHi - kLo) / 4> Build() {
    std::array<std::uint32_t, (kHi - kLo) / 4> m{};
    for (const Entry& e : kEntries) {
        const std::uint32_t w = (e.pc - kLo) / 4;
        m[w] = static_cast<std::uint32_t>(e.lo);
        if (e.size >= 8) m[w + 1] = static_cast<std::uint32_t>(e.lo >> 32);
        if (e.size == 16) {
            m[w + 2] = static_cast<std::uint32_t>(e.hi);
            m[w + 3] = static_cast<std::uint32_t>(e.hi >> 32);
        }
    }
    return m;
}

// The engine's copies, at the same offsets from kLo as BOF3.exe's (so a
// double is 8-aligned where the PC's is).
alignas(16) inline constexpr std::array<std::uint32_t, (kHi - kLo) / 4> kCopies = Build();

// A .rdata address the engine holds; converts to the address of its copy.
struct Const {
    std::uint32_t pc;
    consteval Const(std::uint32_t a) : pc(a) {
        if (!Held(a)) throw "rdata::Const: not a constant rdata_consts.h holds";
    }
    operator std::uint32_t() const {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&kCopies[(pc - kLo) / 4]));
    }
};

// Every copy against BOF3.exe's .rdata in the running image; a Fatal on the
// first difference. InjectAll's first step, before any fuzz reads one.
void Verify();

}  // namespace rdata
