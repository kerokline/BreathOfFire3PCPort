// BOF3X_STPCENSUS - see stp_census.h and docs/stp-census.md.
#include "hook/stp_census.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <tuple>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/records_gpu.h"
#include "hook/input_script.h"
#include "hook/log.h"

namespace stp_census {

bool g_on = false;

namespace {

char g_path[MAX_PATH];
unsigned g_frames = 0;           // Direct3D frames walked
unsigned g_frames_changed = 0;   // of them, frames with a draw holding a bit-15-clear texel
bool g_frame_changed = false;

const unsigned char* At(std::uint32_t a) { return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a)); }
unsigned Cell(unsigned x, unsigned y) { return Gfx_VramShadow[((y & 511u) << 10) + (x & 1023u)]; }

unsigned Now() {
    const unsigned f = bof3::InputScript_Frame();
    return f ? f : static_cast<unsigned>(Frame_Counter);
}

struct Key {
    unsigned area, code, tpage, clut, window_x, window_y, window_w, window_h;
    bool operator<(const Key& o) const {
        return std::tie(area, code, tpage, clut, window_x, window_y, window_w, window_h) <
               std::tie(o.area, o.code, o.tpage, o.clut, o.window_x, o.window_y, o.window_w, o.window_h);
    }
};
struct Row {
    unsigned draws = 0, frames = 0, first = 0, last = 0, last_walk = ~0u;
    unsigned long long zero = 0, stp = 0, opaque = 0;
    unsigned only_stp = 0, only_opaque = 0, mixed = 0, clear = 0;
    unsigned u0 = 255, v0 = 255, u1 = 0, v1 = 0;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;   // the draws' first corners, in the primitives' 320 x 240
};
std::map<Key, Row> g_rows;

struct Count {
    unsigned zero = 0, stp = 0, opaque = 0;
    void Add(unsigned colour) {
        if (colour == 0) ++zero;
        else if (colour & 0x8000) ++stp;
        else ++opaque;
    }
};

// A texel of a page at (px, py) cells, colour mode `mode` (0 4-bit, 1 8-bit,
// 2 direct), at texel (x, y) of the page, through the CLUT at (cx, cy).
unsigned Texel(unsigned px, unsigned py, unsigned mode, unsigned cx, unsigned cy, unsigned x, unsigned y) {
    if (mode == 0) return Cell(cx + ((Cell(px + (x >> 2), py + y) >> ((x & 3) * 4)) & 0xF), cy);
    if (mode == 1) return Cell(cx + ((Cell(px + (x >> 1), py + y) >> ((x & 1) * 8)) & 0xFF), cy);
    return Cell(px + x, py + y);
}

// The box of (u, v) a page primitive samples, [u0, u1) x [v0, v1), through
// the texture window D3d_BuildPageTexture tiles across the 256 x 256 texture.
void SamplePage(unsigned tpage, unsigned clut, unsigned u0, unsigned v0, unsigned u1, unsigned v1, int kx, int ky,
                int kw, int kh, Count* c) {
    const unsigned page = tpage & 0x1F, mode = (tpage >> 7) & 3;
    const unsigned px = (page & 0xF) * 64, py = (page & 0x10) * 16;
    const unsigned cx = (clut & 0x3F) << 4, cy = (clut >> 6) & 511;
    for (unsigned v = v0; v < v1; ++v)
        for (unsigned u = u0; u < u1; ++u) {
            const unsigned tu = u & 0xFF, tv = v & 0xFF;
            const unsigned x = static_cast<unsigned>(kx) + (kw > 0 ? tu % static_cast<unsigned>(kw) : tu);
            const unsigned y = static_cast<unsigned>(ky) + (kh > 0 ? tv % static_cast<unsigned>(kh) : tv);
            c->Add(Texel(px, py, mode == 3 ? 2 : mode, cx, cy, x, y));
        }
}

// The cell sprite's records, as D3d_BuildCellTexture reads them
// (tex_cells.cpp DrawCells): each its own tpage, u, v and w x h, one CLUT.
void SampleCells(unsigned first, unsigned count, unsigned clut, Count* c) {
    const unsigned cx = (clut & 0x3F) << 4, cy = (clut >> 6) & 511;
    for (unsigned n = 0; n < count && n < 4096; ++n) {
        const auto* r = rv::As<records::SpriteCell>(SpriteCell_Table + (first + n) * 8);
        const unsigned tpage = r->tpage;
        const unsigned mode = (tpage & 0x180) ? 1 : 0;
        const unsigned px = (tpage & 0xF) * 64, py = (tpage & 0x10) * 16;
        const unsigned u = r->u, v = r->v;
        const unsigned w = (r->size & 0xFu) << 3, h = (static_cast<unsigned>(r->size) >> 1) & 0x78u;
        for (unsigned y = 0; y < h; ++y)
            for (unsigned x = 0; x < w; ++x) c->Add(Texel(px, py, mode, cx, cy, u + x, v + y));
    }
}

void Write() {
    FILE* f = std::fopen(g_path, "w");
    if (!f) return;
    std::fprintf(f, "# frames %u, frames with a bit-15-clear texel blended %u, rows %u\n", g_frames,
                 g_frames_changed, static_cast<unsigned>(g_rows.size()));
    std::fprintf(f, "area\tcode\ttpage\tpage\tmode\tblend\tclut\twindow\tdraws\tframes\tfirst\tlast\t"
                    "tex_zero\ttex_stp\ttex_opaque\tdraws_stp_only\tdraws_opaque_only\tdraws_mixed\tdraws_clear\tuv_box\txy_box\n");
    for (const auto& [k, r] : g_rows)
        std::fprintf(f, "%u\t%02X\t%04X\t%u\t%u\t%u\t%04X\t%d,%d,%d,%d\t%u\t%u\t%u\t%u\t%llu\t%llu\t%llu\t%u\t%u\t%u\t%u\t%u,%u-%u,%u\t%.0f,%.0f-%.0f,%.0f\n",
                     k.area, k.code, k.tpage, k.tpage & 0x1F, (k.tpage >> 7) & 3, (k.tpage >> 5) & 3, k.clut,
                     static_cast<short>(k.window_x), static_cast<short>(k.window_y), static_cast<short>(k.window_w),
                     static_cast<short>(k.window_h), r.draws, r.frames, r.first, r.last, r.zero, r.stp, r.opaque,
                     r.only_stp, r.only_opaque, r.mixed, r.clear, r.u0, r.v0, r.u1, r.v1, r.x0, r.y0, r.x1, r.y1);
    std::fclose(f);
}

}  // namespace

void Start() {
    const DWORD n = GetEnvironmentVariableA("BOF3X_STPCENSUS", g_path, sizeof g_path);
    if (n == 0 || n >= sizeof g_path) return;
    g_on = true;
    bof3::Log("stpcensus   on: %s, rewritten every 300 Direct3D frames", g_path);
}

void Frame() {
    if (g_frame_changed) ++g_frames_changed;
    g_frame_changed = false;
    if (++g_frames % 300 == 0) Write();
}

void Note(std::uint32_t p) {
    const unsigned char* const prim = At(p);
    const unsigned code = rv::As<records::GpuPrimHead>(prim)->code;
    if ((code & 6) != 6) return;   // semi-transparent and textured only
    const unsigned kind = code & 0xFC;
    unsigned tpage = 0, clut = 0, u0 = 255, v0 = 255, u1 = 0, v1 = 0;
    int kx = 0, ky = 0, kw = 0, kh = 0;
    Count c;
    // The box of n corners' (u, v), [u0, u1) x [v0, v1).
    auto corners = [&](unsigned n, const std::uint8_t* const* us, const std::uint8_t* const* vs) {
        for (unsigned i = 0; i < n; ++i) {
            const unsigned u = *us[i], v = *vs[i];
            if (u < u0) u0 = u;
            if (u > u1) u1 = u;
            if (v < v0) v0 = v;
            if (v > v1) v1 = v;
        }
        if (u1 == u0) ++u1;   // a one-texel edge still samples
        if (v1 == v0) ++v1;
    };
    switch (kind) {
    case 0x24: case 0x2C: {   // FT3, FT4 (an FT3's three corners lie as an FT4's first three: D3d_DrawPolyFT3)
        const auto* q = rv::As<records::GpuPolyFT4>(prim);
        const std::uint8_t* us[4] = {&q->u0, &q->u1, &q->u2, &q->u3};
        const std::uint8_t* vs[4] = {&q->v0, &q->v1, &q->v2, &q->v3};
        corners(kind == 0x24 ? 3 : 4, us, vs);
        clut = q->clut;
        tpage = q->tpage;
        break;
    }
    case 0x34: case 0x3C: {   // GT3, GT4 (likewise: D3d_DrawPolyGT3)
        const auto* q = rv::As<records::GpuPolyGT4>(prim);
        const std::uint8_t* us[4] = {&q->u0, &q->u1, &q->u2, &q->u3};
        const std::uint8_t* vs[4] = {&q->v0, &q->v1, &q->v2, &q->v3};
        corners(kind == 0x34 ? 3 : 4, us, vs);
        clut = q->clut;
        tpage = q->tpage;
        break;
    }
    case 0x64: case 0x74: case 0x7C: {   // SPRT, SPRT_8, SPRT_16: the draw mode's tpage
        const auto* q = rv::As<records::GpuSprt>(prim);
        const unsigned w = kind == 0x64 ? q->w : kind == 0x74 ? 8 : 16;
        const unsigned h = kind == 0x64 ? q->h : w;
        u0 = q->u;
        v0 = q->v;
        u1 = u0 + (w > 256 ? 256 : w);
        v1 = v0 + (h > 256 ? 256 : h);
        clut = q->clut;
        tpage = Gfx_DrawTpage;
        break;
    }
    case 0x84: {   // the port's cell sprite: its records, one CLUT, the flags word as the tpage
        const auto* q = rv::As<records::GpuCellSprite>(prim);
        clut = q->clut;
        tpage = q->flags;
        SampleCells(q->first, q->count, clut, &c);
        break;
    }
    default:   // the glyph 0x6C draws unblended; nothing else is textured
        return;
    }
    if (kind != 0x84) {
        // Gfx_TexCacheKey: s16 x, y, w, h in two dwords (d3d_list.cpp DrawMode).
        kx = static_cast<std::int16_t>(Gfx_TexCacheKey[0] & 0xFFFF);
        ky = static_cast<std::int16_t>(Gfx_TexCacheKey[0] >> 16);
        kw = static_cast<std::int16_t>(Gfx_TexCacheKey[1] & 0xFFFF);
        kh = static_cast<std::int16_t>(Gfx_TexCacheKey[1] >> 16);
        SamplePage(tpage, clut, u0, v0, u1, v1, kx, ky, kw, kh, &c);
    }
    if (((tpage >> 7) & 3) >= 2 && kind != 0x84) clut = 0;   // direct colour: no CLUT

    const Key key{Game_AreaNumber, code, tpage, clut, static_cast<unsigned>(kx) & 0xFFFF,
                  static_cast<unsigned>(ky) & 0xFFFF, static_cast<unsigned>(kw) & 0xFFFF, static_cast<unsigned>(kh) & 0xFFFF};
    Row& r = g_rows[key];
    const unsigned now = Now();
    if (r.draws == 0) r.first = now;
    r.last = now;
    if (r.last_walk != g_frames) {
        r.last_walk = g_frames;
        ++r.frames;
    }
    ++r.draws;
    r.zero += c.zero;
    r.stp += c.stp;
    r.opaque += c.opaque;
    if (c.opaque && c.stp) ++r.mixed;
    else if (c.opaque) ++r.only_opaque;
    else if (c.stp) ++r.only_stp;
    else ++r.clear;
    if (c.opaque) g_frame_changed = true;
    // Every textured kind has its first corner's float x, y at +8 and +0xC.
    const auto* corner = rv::As<records::GpuSprt>(prim);   // x +8, y +0xC, as every kind above lays them
    const float x = corner->x, y = corner->y;
    if (x < r.x0) r.x0 = x;
    if (y < r.y0) r.y0 = y;
    if (x > r.x1) r.x1 = x;
    if (y > r.y1) r.y1 = y;
    if (kind != 0x84) {
        if (u0 < r.u0) r.u0 = u0;
        if (v0 < r.v0) r.v0 = v0;
        if (u1 > r.u1) r.u1 = u1;
        if (v1 > r.v1) r.v1 = v1;
    }
}

}  // namespace stp_census
