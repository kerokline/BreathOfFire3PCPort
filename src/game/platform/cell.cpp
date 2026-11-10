// Moved here by tools/move_funcs.py from tex_cells.cpp (docs/source-layout-round.md).
#include "game/platform/cell.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/tex_cells_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace tex_cells;

namespace tex_cells {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Addr(const void* p) { return Low32(p); }

U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U Word(U address) { return Word(At(address)); }

void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}

void PutWord(unsigned char* p, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

// The cell unpackers' shared shape (0x5AA03E, 0x5AA1E0, 0x5AA2F6, 0x5AA4AE):
// h rows of w texels from src (rows 0x800 bytes apart) to dst (rows `pitch`
// apart), each texel looked up in the palette and written only if the entry
// is non-zero - 0 is transparent, the destination keeps what it had (tested
// on the 16-bit entry, or the whole 32-bit one). Four-bit texels low nibble
// first, eight-bit low byte first; the Flip ones start at the row's last
// dword (src + w / 2 - 4, unsigned, or src + w - 4) and walk back, each
// dword's texels high first - the row mirrored. The texel count steps 8 or 4
// and stops only at exactly 0, and the row count is a `dec` / `jne`: w must
// be a non-zero multiple of the step and h non-zero, or the original runs on
// through memory; ours does the same arithmetic. Byte 0x7DED63 == 4 (read once
// on entry) makes the destination 32-bit.
template <int Bits, bool Flip> void CellUnpack(void* dst, const void* src, const void* palette, int w, int h, int pitch) {
    constexpr U kPerLong = 32 / Bits;
    constexpr U kMask = (1u << Bits) - 1;
    const bool wide = At(::bof3::Img(Gfx_PixelFormat_addr))[3] == 4;
    const auto* table = static_cast<const unsigned char*>(palette);
    const U width = static_cast<U>(w);
    U s = Addr(src);
    if (Flip) s += (Bits == 4 ? width >> 1 : width) - 4;
    auto* d = static_cast<unsigned char*>(dst);
    U rows = static_cast<U>(h);
    do {
        U p = s;
        unsigned char* q = d;
        U n = width;
        do {
            const U v = Long(p);
            for (U k = 0; k < kPerLong; ++k) {
                const U index = (v >> (Bits * (Flip ? kPerLong - 1 - k : k))) & kMask;
                if (wide) {
                    const U c = Long(table + 4 * index);
                    if (c != 0) PutLong(q + 4 * k, c);
                } else {
                    const U c = Word(table + 2 * index);
                    if (c != 0) PutWord(q + 2 * k, c);
                }
            }
            p = Flip ? p - 4 : p + 4;
            q += (wide ? 4 : 2) * kPerLong;
            n -= kPerLong;
        } while (n != 0);
        s += 0x800;
        d += pitch;
    } while (--rows != 0);
}

}  // namespace

}  // namespace tex_cells

// 0x5AA03E, 0x5AA1E0, 0x5AA2F6, 0x5AA4AE: see CellUnpack.
void Cell_Unpack4(void* dst, const void* src, const void* palette, int w, int h, int pitch) {
    CellUnpack<4, false>(dst, src, palette, w, h, pitch);
}

void Cell_Unpack8(void* dst, const void* src, const void* palette, int w, int h, int pitch) {
    CellUnpack<8, false>(dst, src, palette, w, h, pitch);
}

void Cell_Unpack4Flip(void* dst, const void* src, const void* palette, int w, int h, int pitch) {
    CellUnpack<4, true>(dst, src, palette, w, h, pitch);
}

void Cell_Unpack8Flip(void* dst, const void* src, const void* palette, int w, int h, int pitch) {
    CellUnpack<8, true>(dst, src, palette, w, h, pitch);
}

void Cell_Inject() {
    BOF3_INJECT(Cell_Unpack4);
    BOF3_INJECT(Cell_Unpack8);
    BOF3_INJECT(Cell_Unpack4Flip);
    BOF3_INJECT(Cell_Unpack8Flip);
}
