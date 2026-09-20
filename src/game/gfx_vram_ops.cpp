#include "game/gfx_vram_ops.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// The PC counterparts of the PSX ClearImage and MoveImage, on the VRAM shadow.

// original 0x59E650. Fills rect with one BYTE, then drops the textures under
// it. As the original has it:
//   - the PSX call is ClearImage(rect, r, g, b); this one reads two colour
//     arguments and makes ((g & 0xF) << 2) | (r >> 3) of them - not a 15-bit
//     colour, and only its low byte is used, for both bytes of every cell. The
//     one caller found (0x461E46) passes 0, 0, where none of it matters.
//   - no bounds check of any kind; h is read from the rect again every row.
extern "C" void __cdecl Gfx_ClearImage(const short* rect, unsigned char red, unsigned char green) {
    const auto fill = static_cast<std::uint8_t>(((green & 0x0F) << 2) | (red >> 3));
    auto* dst = reinterpret_cast<std::uint8_t*>(Gfx_VramShadow) + ((rect[1] << 10) + rect[0]) * 2;
    const std::uint32_t row_bytes = static_cast<std::uint32_t>(rect[2] << 1);
    for (int row = 0; row < rect[3]; ++row) {
        std::memset(dst, fill, row_bytes);
        dst += 0x800;
    }
    Gfx_InvalidateTextures(rect, 0);
}

// original 0x5AA5D6, hand-written assembly. Copies rect, within the buffer at
// `base` (1,024 cells a row), to (to_x, to_y). As the original has it:
//   - x, y, w, h are read UNSIGNED;
//   - each row copies w / 2 dwords, so an odd w loses its last cell - and the
//     row step is computed from the full w, so every row after an odd one
//     starts two bytes further left;
//   - always forward, a dword at a time, whatever the overlap;
//   - the row counter is h in the low half of a register whose high half is
//     to_x's: it counts down to zero as 32 bits. h = 0 is 65,536 rows at best.
extern "C" void __cdecl Gfx_MoveCells(void* base, const short* rect, int to_x, int to_y) {
    const auto x = static_cast<std::uint16_t>(rect[0]), y = static_cast<std::uint16_t>(rect[1]);
    const auto w = static_cast<std::uint16_t>(rect[2]), h = static_cast<std::uint16_t>(rect[3]);
    auto* src = static_cast<std::uint8_t*>(base) + (static_cast<std::uint32_t>(y) << 11) + x * 2u;
    auto* dst = static_cast<std::uint8_t*>(base) + (to_y << 11) + to_x * 2;
    const std::uint32_t step = (0x400u - w) << 1;
    std::uint32_t rows = (static_cast<std::uint32_t>(to_x) & 0xFFFF0000u) | h;
    do {
        for (std::uint32_t n = w >> 1; n != 0; --n) {
            std::uint32_t cells;
            std::memcpy(&cells, src, 4);
            std::memcpy(dst, &cells, 4);
            src += 4;
            dst += 4;
        }
        src += step;
        dst += step;
    } while (--rows != 0);
}

// original 0x59E9A0. Clips rect to the shadow IN PLACE, moves it to
// (to_x, to_y), rewrites rect's x and y as the destination, and marks the
// textures under the destination stale (mode 1) rather than dropping them.
// As the original has it:
//   - the caller's rect is changed, as above;
//   - a source hanging off the left or top is clipped and the destination
//     shifted by the same (negative) amount; one off the right or bottom is
//     only shortened; the DESTINATION is never clipped at all;
//   - w == 0 or h == 0 returns at once, but clipping can produce either - or a
//     negative - afterwards, and Gfx_MoveCells takes them as they come.
extern "C" void __cdecl Gfx_MoveImage(short* rect, int to_x, int to_y) {
    if (rect[2] == 0 || rect[3] == 0) return;

    const short x = rect[0];
    if (x < 0) {
        rect[2] = static_cast<short>(rect[2] + x);
        rect[0] = 0;
        to_x += x;
    } else if (x + rect[2] > 0x400) {
        rect[2] = static_cast<short>(0x400 - x);
    }
    const short y = rect[1];
    if (y < 0) {
        rect[3] = static_cast<short>(rect[3] + y);
        rect[1] = 0;
        to_y += y;
    } else if (y + rect[3] > 0x200) {
        rect[3] = static_cast<short>(0x200 - y);
    }

    Gfx_MoveCells(Gfx_VramShadow, rect, to_x, to_y);
    rect[0] = static_cast<short>(to_x);
    rect[1] = static_cast<short>(to_y);
    Gfx_InvalidateTextures(rect, 1);
}

// --- BOF3X_SHADOW=gfx_vram_ops: start-up differential fuzz against clones -------
// (docs/SCAFFOLDING.md section 2.) Calls to Gfx_InvalidateTextures are re-aimed
// at its original address - ours by now - and the cloned Gfx_MoveImage calls
// the cloned Gfx_MoveCells.

namespace {

std::uint32_t g_rng = 0x7A3C9E15u;
std::uint32_t Rng() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}
int RngIn(int lo, int hi) { return lo + static_cast<int>(Rng() % static_cast<unsigned>(hi - lo + 1)); }

constexpr unsigned kShadowBytes = 0x100000;
std::uint8_t g_input[kShadowBytes], g_theirs[kShadowBytes];
std::uint8_t g_table_in[Gfx_TexCache_count], g_table_theirs[Gfx_TexCache_count];

// A texture-cache table with a built entry in every page, some reaching into
// the next: what mode 1 marks shows which rect Gfx_InvalidateTextures was
// given. No COM pointers, so mode 0 has nothing to Release.
void MakeTable() {
    std::memset(g_table_in, 0, sizeof g_table_in);
    for (int page = 0; page < 32; ++page)
        for (int slot = 0; slot < 3; ++slot) {
            std::uint8_t* e = g_table_in + (page * 32 + slot) * 0x18;
            e[0] = 1;
            e[1] = static_cast<std::uint8_t>(Rng() % 2);
            e[2] = static_cast<std::uint8_t>(Rng());
        }
}

template <class Theirs, class Ours>
bool Differs(Theirs theirs, Ours ours, short* rect, const short* rect_in) {
    auto* shadow = reinterpret_cast<std::uint8_t*>(Gfx_VramShadow);
    short rect_theirs[4];
    for (int pass = 0; pass < 2; ++pass) {
        std::memcpy(shadow, g_input, kShadowBytes);
        std::memcpy(Gfx_TexCache, g_table_in, sizeof g_table_in);
        std::memcpy(rect, rect_in, sizeof(short) * 4);
        if (pass) ours(); else theirs();
        if (pass == 0) {
            std::memcpy(g_theirs, shadow, kShadowBytes);
            std::memcpy(g_table_theirs, Gfx_TexCache, sizeof g_table_in);
            std::memcpy(rect_theirs, rect, sizeof rect_theirs);
        }
    }
    return std::memcmp(g_theirs, shadow, kShadowBytes) != 0 ||
           std::memcmp(g_table_theirs, Gfx_TexCache, sizeof g_table_in) != 0 ||
           std::memcmp(rect_theirs, rect, sizeof rect_theirs) != 0;
}

using ClearFn = void (__cdecl*)(const short*, unsigned char, unsigned char);
using CellsFn = void (__cdecl*)(void*, const short*, int, int);
using MoveFn = void (__cdecl*)(short*, int, int);

void SelfTest(ClearFn their_clear, CellsFn their_cells, MoveFn their_move) {
    constexpr unsigned kRounds = 900;
    unsigned bad = 0, clipped = 0, odd = 0, overlapping = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        if (round % 30 == 0) for (auto& b : g_input) b = static_cast<std::uint8_t>(Rng());
        MakeTable();
        short rect[4], in[4];
        bool differs = false;
        const int which = static_cast<int>(round % 3);
        if (which == 0) {
            in[2] = static_cast<short>(RngIn(0, 300)); in[3] = static_cast<short>(RngIn(-2, 200));
            in[0] = static_cast<short>(RngIn(0, 1024 - in[2])); in[1] = static_cast<short>(RngIn(0, 512 - 200));
            const auto r = static_cast<unsigned char>(Rng()), g = static_cast<unsigned char>(Rng());
            differs = Differs([&] { their_clear(rect, r, g); }, [&] { Gfx_ClearImage(rect, r, g); }, rect, in);
        } else {
            // Source and destination chosen so that nothing - the odd-width
            // drift included - leaves the shadow; for Gfx_MoveImage the source
            // may hang over any edge as long as what is left after clipping
            // is at least 1 x 1.
            int x, y, w, h, tx, ty;
            for (;;) {
                w = RngIn(1, 200); h = RngIn(1, 120);
                x = RngIn(which == 2 ? -40 : 0, which == 2 ? 1060 : 1024 - w);
                y = RngIn(which == 2 ? -40 : 0, which == 2 ? 540 : 512 - h);
                int cx = x, cy = y, cw = w, ch = h, shift_x = 0, shift_y = 0;
                if (which == 2) {
                    if (x < 0) { cw += x; cx = 0; shift_x = x; } else if (x + w > 0x400) cw = 0x400 - x;
                    if (y < 0) { ch += y; cy = 0; shift_y = y; } else if (y + h > 0x200) ch = 0x200 - y;
                }
                if (cw < 1 || ch < 1) continue;
                // half the time near the source, so the ranges overlap
                tx = Rng() % 2 ? cx + RngIn(-6, 6) : RngIn(0, 1023);
                ty = Rng() % 2 ? cy + RngIn(-3, 3) : RngIn(0, 511);
                const int drift = (cw & 1) ? ch : 0;   // cells lost to the left over the whole move
                if (tx < 0 || ty < 0 || tx + cw > 1024 || ty + ch > 512) continue;
                if (drift && (cy * 1024 + cx < drift || ty * 1024 + tx < drift)) continue;
                if (cw != w || ch != h) ++clipped;
                if (cw & 1) ++odd;
                if (tx < cx + cw && cx < tx + cw && ty < cy + ch && cy < ty + ch) ++overlapping;
                tx -= shift_x; ty -= shift_y;   // what the caller passes, before the clip shifts it
                break;
            }
            in[0] = static_cast<short>(x); in[1] = static_cast<short>(y);
            in[2] = static_cast<short>(w); in[3] = static_cast<short>(h);
            if (which == 1)
                differs = Differs([&] { their_cells(Gfx_VramShadow, rect, tx, ty); },
                                  [&] { Gfx_MoveCells(Gfx_VramShadow, rect, tx, ty); }, rect, in);
            else
                differs = Differs([&] { their_move(rect, tx, ty); }, [&] { Gfx_MoveImage(rect, tx, ty); }, rect, in);
        }
        if (differs && ++bad <= 8)
            bof3::Log("shadow      gfx_vram_ops self-test MISMATCH round %u: function %d, rect {%d,%d,%d,%d}",
                      round, which, in[0], in[1], in[2], in[3]);
    }
    std::memset(Gfx_VramShadow, 0, kShadowBytes);
    std::memset(Gfx_TexCache, 0, Gfx_TexCache_count);
    bof3::Log("shadow      gfx_vram_ops self-test: %u rounds over Gfx_ClearImage, Gfx_MoveCells and Gfx_MoveImage "
              "(moves: %u clipped, %u of odd width, %u overlapping), %u MISMATCHES; the whole shadow, the texture "
              "cache and the caller's rect compared", kRounds, clipped, odd, overlapping, bad);
    if (bad) bof3::Fatal("gfx_vram_ops differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void GfxVramOps_Inject() {
    if (bof3::WantsShadow("gfx_vram_ops")) {
        // Disasm 2026-09-19: every jump in all three is internal. Gfx_MoveCells
        // calls nothing; the other two call Gfx_InvalidateTextures, and
        // Gfx_MoveImage calls Gfx_MoveCells first.
        auto* cells = bof3::CloneOriginal("Gfx_MoveCells", bof3::addr::Gfx_MoveCells, 0x4E);
        const bof3::CloneCall clear_calls[] = {{0x9A, nullptr}};
        auto* clear = bof3::CloneOriginal("Gfx_ClearImage", bof3::addr::Gfx_ClearImage, 0xA5, clear_calls, 1);
        const bof3::CloneCall move_calls[] = {{0xAD, cells}, {0xBC, nullptr}};
        auto* move = bof3::CloneOriginal("Gfx_MoveImage", bof3::addr::Gfx_MoveImage, 0xC8, move_calls, 2);
        SelfTest(reinterpret_cast<ClearFn>(clear), reinterpret_cast<CellsFn>(cells), reinterpret_cast<MoveFn>(move));
    }
    BOF3_INJECT(Gfx_ClearImage);
    BOF3_INJECT(Gfx_MoveCells);
    BOF3_INJECT(Gfx_MoveImage);
}
