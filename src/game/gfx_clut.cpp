#include "game/gfx_clut.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// The converted-palette cache. The PSX game keeps its CLUTs as rows of cells in
// VRAM; the port converts a VRAM row to the display's pixel format on demand,
// one 1,024-cell buffer per row, and counts each conversion so that whatever
// was built from a palette can tell it has changed.

namespace {

struct ClutRow {
    std::uint32_t generation;   // +0: incremented on every conversion of the row
    void* pixels;               // +4: 0x1000 bytes, or null until first wanted
};
static_assert(sizeof(ClutRow) == 8);
static_assert(sizeof(ClutRow) * 512 == sizeof(unsigned long) * Gfx_ClutRows_count);

ClutRow* Rows() { return reinterpret_cast<ClutRow*>(Gfx_ClutRows); }

struct PixelFormat {
    std::uint8_t unread[3];
    std::uint8_t bytes_per_pixel;   // +3
    std::uint32_t r_shift, g_shift, b_shift;
    std::uint32_t r_mask, g_mask, b_mask;
    std::uint32_t opaque_bits;      // +0x1C
};
static_assert(sizeof(PixelFormat) == sizeof(unsigned long) * Gfx_PixelFormat_count);

const PixelFormat& Format() { return *reinterpret_cast<const PixelFormat*>(Gfx_PixelFormat); }

std::uint16_t* ShadowCell(int x, int y) { return Gfx_VramShadow + ((y << 10) + x); }

// x86 shr takes its count modulo 32; say so rather than leave it to the compiler.
std::uint32_t Shr(std::uint32_t v, std::uint32_t count) { return v >> (count & 31); }

}  // namespace

// original 0x59EBB0. Converts w cells of VRAM row y, from x, into the row's
// buffer. As the original has it: nothing is checked - not y against the 512
// rows, not x + w against the buffer's 1,024 cells; and the generation moves
// on even when w <= 0 converts nothing.
//
// PSX meaning carried over: cell 0 is transparent and stays 0, and any other
// cell that the display format would round to 0 is forced to 1, so that it
// stays opaque.
extern "C" void __cdecl Gfx_ConvertRow(int x, int y, int w) {
    ClutRow& row = Rows()[y];
    if (row.pixels == nullptr) row.pixels = Crt_malloc(0x1000);

    const PixelFormat& f = Format();
    auto* out = static_cast<std::uint8_t*>(row.pixels) + (x << (f.bytes_per_pixel >> 1));
    const std::uint16_t* src = ShadowCell(x, y);
    for (int n = 0; n < w; ++n) {
        const std::uint32_t v = src[n];
        std::uint32_t p = (Shr((v & 0x3E0) << 14, f.g_shift) & f.g_mask) |
                          (Shr((v & 0x1F) << 19, f.r_shift) & f.r_mask) |
                          (Shr((v & 0x7C00) << 9, f.b_shift) & f.b_mask);
        if (p != 0) p |= f.opaque_bits;
        else if (v != 0) p = 1 | f.opaque_bits;
        if (f.bytes_per_pixel == 2) {
            const auto p16 = static_cast<std::uint16_t>(p);
            std::memcpy(out, &p16, 2);
            out += 2;
        } else {
            std::memcpy(out, &p, 4);
            out += 4;
        }
    }
    ++row.generation;
}

// original 0x59EB00. Gfx_LoadImage's sibling for data that is mostly unchanged
// from frame to frame: a row is copied, and converted, only if it differs from
// what the shadow already holds. It does not invalidate textures.
//
// As the original has it: no bounds check at all, where Gfx_LoadImage has two;
// the row bound and the x and w handed on are read from the rect again on
// every row, the row length once.
extern "C" void __cdecl Gfx_LoadImageIfChanged(const short* rect, const void* pixels) {
    int y = rect[1];
    const std::uint32_t row_bytes = static_cast<std::uint32_t>(rect[2] << 1);
    auto* dst = reinterpret_cast<std::uint8_t*>(ShadowCell(rect[0], y));
    auto* src = static_cast<const std::uint8_t*>(pixels);
    for (; y < rect[3] + rect[1]; ++y) {
        if (std::memcmp(dst, src, row_bytes) != 0) {
            std::memcpy(dst, src, row_bytes);
            Gfx_ConvertRow(rect[0], y, rect[2]);
        }
        src += row_bytes;
        dst += 0x800;
    }
}

// original 0x5A04C0. The converted colours of a CLUT, by PSX CLUT id: x / 16 in
// the low six bits, y above. Converts the first 256 cells of the row if the row
// has never been wanted before. As the original has it, y is not checked, and
// an id can name a row up to 1,023.
extern "C" void* __cdecl Gfx_ClutPixels(unsigned clut_id) {
    clut_id &= 0xFFFF;
    const unsigned y = clut_id >> 6;
    const unsigned x = (clut_id & 0x3F) << 4;
    if (Rows()[y].pixels == nullptr) Gfx_ConvertRow(0, static_cast<int>(y), 0x100);
    return static_cast<std::uint8_t*>(Rows()[y].pixels) + (x << (Format().bytes_per_pixel >> 1));
}

// --- BOF3X_SHADOW: a start-up differential fuzz against clones -----------------
// (docs/SCAFFOLDING.md section 2). The DLL is injected before the game has run:
// the shadow, the row table and the pixel format are all still zero, and the
// game's heap does not exist yet - so every row used here is given a buffer of
// ours first, and the one path this cannot reach is the Crt_malloc of a row's
// first conversion. Everything is put back to zero afterwards.

namespace {

using ConvertRowFn = void (__cdecl*)(int, int, int);
using LoadFn = void (__cdecl*)(const short*, const void*);
using ClutFn = void* (__cdecl*)(unsigned);

constexpr int kRows = 8;            // rows a round may touch: y0 .. y0 + kRows - 1
std::uint8_t g_buffers[kRows][0x1000];
std::uint16_t g_shadow_in[kRows][1024], g_shadow_theirs[kRows][1024];
std::uint8_t g_buffers_in[kRows][0x1000], g_buffers_theirs[kRows][0x1000];
std::uint32_t g_gen_theirs[kRows];
std::uint16_t g_upload[kRows * 1024];

std::uint32_t g_rng = 0x9E3779B9u;
std::uint32_t Rng() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}
int RngIn(int lo, int hi) { return lo + static_cast<int>(Rng() % static_cast<unsigned>(hi - lo + 1)); }

void PutState(int y0) {
    for (int i = 0; i < kRows; ++i) {
        std::memcpy(ShadowCell(0, y0 + i), g_shadow_in[i], sizeof g_shadow_in[i]);
        std::memcpy(g_buffers[i], g_buffers_in[i], sizeof g_buffers[i]);
        Rows()[y0 + i] = {static_cast<std::uint32_t>(i * 7), g_buffers[i]};
    }
}

void SelfTest(ConvertRowFn their_convert, LoadFn their_load, ClutFn their_clut) {
    constexpr unsigned kRounds = 3000;
    // 565, 555 with an alpha bit, and 32-bit with alpha: what a display offers.
    static const PixelFormat formats[] = {
        {{0, 0, 0}, 2, 8, 13, 19, 0xF800, 0x07E0, 0x001F, 0},
        {{0, 0, 0}, 2, 9, 14, 19, 0x7C00, 0x03E0, 0x001F, 0x8000},
        {{0, 0, 0}, 4, 0, 8, 16, 0xFF0000, 0x00FF00, 0x0000FF, 0xFF000000},
    };
    auto* format = reinterpret_cast<PixelFormat*>(Gfx_PixelFormat);

    unsigned bad = 0, n_convert = 0, n_load = 0, n_clut = 0, rows_skipped = 0, rows_copied = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        *format = formats[round % 3];
        const int y0 = RngIn(0, 512 - kRows);
        for (int i = 0; i < kRows; ++i) {
            for (auto& c : g_shadow_in[i])   // a third zero cells: the transparent case
                c = Rng() % 3 == 0 ? 0 : static_cast<std::uint16_t>(Rng() % 7 == 0 ? Rng() & 0x8421 : Rng());
            for (auto& b : g_buffers_in[i]) b = static_cast<std::uint8_t>(Rng());
        }
        const int x = RngIn(0, 1023), w = Rng() % 8 == 0 ? RngIn(-2, 0) : RngIn(0, 1024 - x);
        const int y = y0 + RngIn(0, kRows - 1), h = RngIn(0, y0 + kRows - y);
        const short rect[4] = {static_cast<short>(x), static_cast<short>(y),
                               static_cast<short>(w < 0 ? 0 : w), static_cast<short>(h)};
        const int which = static_cast<int>(round % 3);
        if (which == 1) {   // an upload: each row either what the shadow holds, or not
            for (int r = 0; r < h; ++r) {
                std::uint16_t* up = g_upload + r * rect[2];
                std::memcpy(up, g_shadow_in[y - y0 + r] + x, sizeof(std::uint16_t) * static_cast<unsigned>(rect[2]));
                if (rect[2] > 0 && Rng() % 2) { up[RngIn(0, rect[2] - 1)] ^= 0x0101; ++rows_copied; }
                else ++rows_skipped;
            }
        }
        const unsigned clut_id = static_cast<unsigned>(y << 6) | (Rng() & 0x3F) | (Rng() % 2 ? 0x12340000u : 0);

        void* result[2] = {nullptr, nullptr};
        for (int pass = 0; pass < 2; ++pass) {
            PutState(y0);
            if (which == 0) { (pass ? &Gfx_ConvertRow : their_convert)(x, y, w); }
            else if (which == 1) { (pass ? &Gfx_LoadImageIfChanged : their_load)(rect, g_upload); }
            else { result[pass] = (pass ? &Gfx_ClutPixels : their_clut)(clut_id); }
            if (pass == 0) {
                for (int i = 0; i < kRows; ++i) {
                    std::memcpy(g_shadow_theirs[i], ShadowCell(0, y0 + i), sizeof g_shadow_theirs[i]);
                    std::memcpy(g_buffers_theirs[i], g_buffers[i], sizeof g_buffers_theirs[i]);
                    g_gen_theirs[i] = Rows()[y0 + i].generation;
                }
            }
        }
        ++(which == 0 ? n_convert : which == 1 ? n_load : n_clut);

        bool same = result[0] == result[1];
        for (int i = 0; i < kRows && same; ++i)
            same = std::memcmp(g_shadow_theirs[i], ShadowCell(0, y0 + i), sizeof g_shadow_theirs[i]) == 0 &&
                   std::memcmp(g_buffers_theirs[i], g_buffers[i], sizeof g_buffers_theirs[i]) == 0 &&
                   g_gen_theirs[i] == Rows()[y0 + i].generation && Rows()[y0 + i].pixels == g_buffers[i];
        if (!same && ++bad <= 8)
            bof3::Log("shadow      gfx_clut self-test MISMATCH round %u: function %d, format %u, "
                      "x %d y %d w %d h %d, clut id 0x%X", round, which, round % 3, x, y, w, h, clut_id);

        for (int i = 0; i < kRows; ++i) {
            std::memset(ShadowCell(0, y0 + i), 0, sizeof g_shadow_in[i]);
            Rows()[y0 + i] = {0, nullptr};
        }
    }
    std::memset(format, 0, sizeof *format);
    bof3::Log("shadow      gfx_clut self-test: %u rounds (%u Gfx_ConvertRow, %u Gfx_LoadImageIfChanged with "
              "%u rows copied and %u skipped, %u Gfx_ClutPixels), 3 pixel formats, %u MISMATCHES; "
              "shadow rows, row buffers, generations and results compared",
              kRounds, n_convert, n_load, rows_copied, rows_skipped, n_clut, bad);
    if (bad) bof3::Fatal("gfx_clut differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void GfxClut_Inject() {
    if (bof3::WantsShadow("gfx_clut")) {
        // Disasm 2026-09-19: in all three every jump is internal and there is
        // exactly one relative call. Clones call clones, never ours.
        const bof3::CloneCall to_malloc[] = {{0x16, nullptr}};
        auto* convert = bof3::CloneOriginal("Gfx_ConvertRow", bof3::addr::Gfx_ConvertRow, 0x123, to_malloc, 1);
        const bof3::CloneCall to_convert_a[] = {{0x64, convert}};
        auto* load = bof3::CloneOriginal("Gfx_LoadImageIfChanged", bof3::addr::Gfx_LoadImageIfChanged, 0xA1,
                                         to_convert_a, 1);
        const bof3::CloneCall to_convert_b[] = {{0x2B, convert}};
        auto* clut = bof3::CloneOriginal("Gfx_ClutPixels", bof3::addr::Gfx_ClutPixels, 0x4D, to_convert_b, 1);
        SelfTest(reinterpret_cast<ConvertRowFn>(convert), reinterpret_cast<LoadFn>(load),
                 reinterpret_cast<ClutFn>(clut));
    }
    BOF3_INJECT(Gfx_ConvertRow);
    BOF3_INJECT(Gfx_LoadImageIfChanged);
    BOF3_INJECT(Gfx_ClutPixels);
}
