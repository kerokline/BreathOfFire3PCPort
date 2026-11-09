#include "game/gfx_vram_ops.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// The PC counterparts of the PSX ClearImage and MoveImage, on the VRAM shadow.

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
}
