// Moved here by tools/move_funcs.py from effect_6d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_61.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_6d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

std::int32_t S16(U a) { return S16(At(a)); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

U S8U(unsigned char b) { return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(b))); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

const short* Vertex(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

// cdq; and edx, 0xFFF; add; sar 12 - a signed divide by 4096, toward zero.
std::int32_t Div4096(U v) { return I(v) / 4096; }

// The float bits of an int (fild; fstp dword).
U FloatBits(std::int32_t v) {
    const float f = static_cast<float>(v);
    U b;
    std::memcpy(&b, &f, sizeof b);
    return b;
}

// cdq; xor eax, edx; sub eax, edx: |d| as the original computes it -
// 0x80000000 stays itself, negative.
std::int32_t Abs(U d) {
    const U m = I(d) < 0 ? 0xFFFFFFFFu : 0u;
    return I((d ^ m) - m);
}

// A texture window of 8 bytes taken from the packet cursor (mov ecx,
// [Gfx_PacketNext]; add 8): x, y 0 and w, h - its address the draw mode's
// last argument.
U Window(U x, U wh) {
    unsigned char* const r = Gfx_PacketNext;
    Gfx_PacketNext = r + 8;
    SetWord(r, x);
    SetWord(r + 2, 0);
    SetWord(r + 6, wh);
    SetWord(r + 4, wh);
    return AddressOf(r);
}

}  // namespace

// original 0x515000 (hidden in 0x514DF0): call [EffectKind18Sub61_Steps +
// Cond_ByteFE * 4], unbounded; then the tracks, and a tail jump to the mist.
extern "C" void __cdecl EffectKind18Sub61_Run(void) {
    const unsigned step = Cond_ByteFE;
    if (step >= EffectKind18Sub61_Steps_count)
        bof3::Fatal("EffectKind18Sub61_Run: Cond_ByteFE is %u, past the %u steps of 0x%X - the original calls through "
                    "the dword after (docs/effect_6d.md section 7)",
                    step, (unsigned)EffectKind18Sub61_Steps_count, (unsigned)AddressOf(EffectKind18Sub61_Steps));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(AddressOf(EffectKind18Sub61_Steps) + 4 * step)))();
    SH_CALL(EffectKind18Sub61_DrawTracks)();
    SH_CALL(EffectKind18Sub61_DrawMist)();
}

// original 0x515020 (step 0): MapView_FocusX 0x16FF: +2 = 4 (the mist starts).
extern "C" void __cdecl EffectKind18Sub61_WaitFocus(void) {
    if (MapView_FocusX == 0x16FF) S()[2] = 4;
}

// original 0x515040 (step 1): +0x34 down 1 every fourth frame (Frame_Counter &
// 3 = 0); the overlay at y +0x34.
extern "C" void __cdecl EffectKind18Sub61_Slide(void) {
    const U by = (Frame_Counter & 3) == 0 ? 1u : 0u;
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    so->x = static_cast<std::uint32_t>(so->x) - by;
    SH_CALL(EffectKind18Sub61_DrawOverlay)(rv::Current<records::EffectObject>()->x);
}

// original 0x515070 (step 2): the overlay at y 0.
extern "C" void __cdecl EffectKind18Sub61_Hold(void) { SH_CALL(EffectKind18Sub61_DrawOverlay)(0); }

// original 0x515080 (step 3): by +0x38 (signed): above 0x100 the glow and the
// overlay at +0x38; above 0x80 the glow, +0x38 up 2, the overlay; above 0x60
// +0x38 up 2, the overlay; else up 1, the overlay.
extern "C" void __cdecl EffectKind18Sub61_Rise(void) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    const U v = static_cast<std::uint32_t>(so->z);
    if (I(v) > 0x100) {
        SH_CALL(EffectKind18Sub61_DrawGlow)();
    } else if (I(v) > 0x80) {
        SH_CALL(EffectKind18Sub61_DrawGlow)();
        rv::Current<records::EffectObject>()->z = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z) + 2;
    } else if (I(v) > 0x60) {
        so->z = v + 2;
    } else {
        so->z = v + 1;
    }
    SH_CALL(EffectKind18Sub61_DrawOverlay)(rv::Current<records::EffectObject>()->z);
}

// original 0x515100 (step 4): +0x38 0, +0x34 0x54, +0x3C 0; the ring's first
// entry (0x7E, 0x14, 1) and (0x54, 0x13, 0); Cond_ByteFE 0.
extern "C" void __cdecl EffectKind18Sub61_Reset(void) {
    rv::Current<records::EffectObject>()->z = 0;
    rv::Current<records::EffectObject>()->x = 0x54;
    rv::Current<records::EffectObject>()->height = 0;
    unsigned char* const ring = At(::bof3::Img(at::kTrackRing.v));
    ring[0] = 0x7E;
    ring[1] = 0x14;
    ring[2] = 1;
    ring[3] = 0x54;
    ring[4] = 0x13;
    ring[5] = 0;
    Cond_ByteFE = 0;
}

// original 0x515150 (step 5): the tracks cleared, then Cond_ByteFE 1.
extern "C" void __cdecl EffectKind18Sub61_Rearm(void) {
    SH_CALL(EffectKind18Sub61_ClearTracks)();
    Cond_ByteFE = 1;
}

// original 0x515160 (step 6, and step 5's call): both heads 0, every mark's
// cell x 0xFF (none drawn), the first entry's parities 1 and 0; Cond_ByteFE 0.
extern "C" void __cdecl EffectKind18Sub61_ClearTracks(void) {
    SetWord(At(::bof3::Img(at::kTrackHeads.v)), 0);
    for (U r = 0; r < 2; ++r)
        for (U k = 0; k < at::kTrackEntries; ++k) At(::bof3::Img(at::kTrackRing.v) + 3 * r + at::kTrackStride * k)[0] = 0xFF;
    At(::bof3::Img(at::kTrackRing.v))[2] = 1;
    At(::bof3::Img(at::kTrackRing.v))[5] = 0;
    Cond_ByteFE = 0;
}

// original 0x5151A0 (cdecl (y); the overlay, called by steps 1, 2 and 3):
// nothing unless Draw_PassFlags has bit 2. A draw mode (page 0x99, the window
// (16, 0) 16 x 16) committed at slot 7 (0xC); two textured quads, (0, 0)..(256,
// 256) and (256, 0)..(320, 256), CLUT 0x79C0, page 0x99, committed (0x48); a
// draw mode (page 0x99, the window 256 x 256) (0xC); the eight pieces of
// 0x65F470 at their (x, y + y) .. (x + w, y + h + y), their (u, v) .. (u + du,
// v + dv), their page byte's page and CLUT, committed (0x48).
//
// What it is (docs/ending-backdrop.md): the ending's backdrop in area 0xC7 -
// the two quads a plain tile over the frame, pieces 0 and 1 the cloud sky
// (256 + 64 wide, 48 tall, page 0x99's rows 16..63 and 64..111), pieces 2..7
// the far dunes (page 0x97: a 256-wide picture, its right 64 columns repeated
// down the screen's last 64).
//
// DIV-0127 (two changes, both off in every fuzz and under BOF3X_ORIGINAL):
// - the seam: pieces 0 and 1 are 48 tall with a dv of 48, so the PC's
//   texel-centre table (D3d_TexCoords, (i + 0.512) / 256) samples texel row 64
//   on their last pixel row - the row under the sky, index 0 of the page, a
//   lighter blue - a one-pixel line across the picture where the sky meets
//   the dunes; piece 1's du of 64 for 64 columns does the same down its last
//   column. Once the fills are armed (Widescreen_FillsArmed, after the
//   self-tests; wide or narrow) their du and dv are w - 1 and h - 1, the rule
//   piece 0's du, pieces 4 and 5 and the port's own fills follow.
// - the wide picture: with Widescreen_Fill() columns a side, the tile runs to
//   the bands, and every piece on the left edge (x 0) or the right edge (x + w
//   320) gets band pieces beside it: the same rows, mirrored about the edge
//   column (and mirrored again past the piece's width) - the texel at the
//   edge continues outward - so the sky and the dunes meet the band without
//   a seam. The pictures are 256 wide and already repeated by Capcom for the
//   last 64 columns; a wider painting is the art path.
extern "C" void __cdecl EffectKind18Sub61_DrawOverlay(int y) {
    if ((Draw_PassFlags & 4) == 0) return;
    const U wide = Widescreen_Fill();
    const bool armed = Widescreen_FillsArmed();
    {
        const U window = Window(0x10, 0x10);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x99, window);
    }
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
    const U f256 = 0x43800000u, f320 = 0x43A00000u;
    // A quad of the tile: x0..x1 by 0..256, u0..u1 by v 0..255, page 0x99,
    // CLUT 0x79C0 - the original's two, then the bands'.
    const auto Tile = [](U x0_bits, U x1_bits, unsigned char u0, unsigned char u1) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 1);
        SetUL(p + 8, x0_bits);
        SetUL(p + 0xC, 0);
        SetUL(p + 0x18, x1_bits);
        SetUL(p + 0x1C, 0);
        SetUL(p + 0x28, x0_bits);
        SetUL(p + 0x2C, 0x43800000u);
        SetUL(p + 0x38, x1_bits);
        SetUL(p + 0x3C, 0x43800000u);
        p[0x14] = u0;
        p[0x15] = 0;
        p[0x24] = u1;
        p[0x25] = 0;
        p[0x34] = u0;
        p[0x35] = 0xFF;
        p[0x44] = u1;
        p[0x45] = 0xFF;
        SetWord(p + 0x26, 0x99);
        SetWord(p + 0x16, 0x79C0);
        SH_CALL(Gfx_CommitPrim)(7, 0x48);
    };
    Tile(0, f256, 0, 0xFF);
    Tile(f256, f320, 0, 0x40);
    if (wide != 0) {
        // The tile repeats every 16 texels (the window), so any u a multiple
        // of 16 apart from the edge's joins it: 256 - wide .. 256 on the left,
        // 64 .. 64 + wide on the right.
        Tile(FloatBits(-I(wide)), 0, static_cast<unsigned char>(256u - wide), 0);
        Tile(f320, FloatBits(I(320u + wide)), 0x40, static_cast<unsigned char>(0x40u + wide));
    }
    {
        const U window = Window(0, 0x100);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x99, window);
    }
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
    const U dy = static_cast<U>(y);
    // One piece: x0..x1 by top..bottom (ints as the original's s16 sums),
    // texels u0..u1 by v0..v1, the page byte's page and CLUT.
    const auto Piece = [](U x0, U x1, U top, U bottom, unsigned char u0, unsigned char u1, unsigned char v0,
                          unsigned char v1, unsigned char page) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 1);
        SetUL(p + 8, FloatBits(I(x0)));
        SetUL(p + 0xC, FloatBits(I(top)));
        SetUL(p + 0x18, FloatBits(I(x1)));
        SetUL(p + 0x1C, FloatBits(I(top)));
        SetUL(p + 0x28, FloatBits(I(x0)));
        SetUL(p + 0x2C, FloatBits(I(bottom)));
        SetUL(p + 0x38, FloatBits(I(x1)));
        SetUL(p + 0x3C, FloatBits(I(bottom)));
        p[0x14] = u0;
        p[0x15] = v0;
        p[0x24] = u1;
        p[0x25] = v0;
        p[0x34] = u0;
        p[0x35] = v1;
        p[0x44] = u1;
        p[0x45] = v1;
        SetWord(p + 0x26, ((((static_cast<U>(page) << 7) + 0x140u) >> 6) & 0xFu) | 0x90u);
        SetWord(p + 0x16, (static_cast<U>(page) + 0x1E5u) << 6);
        SH_CALL(Gfx_CommitPrim)(7, 0x48);
    };
    for (U k = 0; k < at::kOverlayCount; ++k) {
        const unsigned char* const e = At(::bof3::Img(at::kOverlayPieces.v) + at::kOverlayStride * k);
        const U x = static_cast<U>(S16(e)), top = static_cast<U>(S16(e + 2)) + dy;
        const U w = static_cast<U>(S16(e + 4)), h = static_cast<U>(S16(e + 6));
        const U bottom = static_cast<U>(S16(e + 2)) + (dy + h);
        const unsigned char u = e[9], v = e[0xA];
        unsigned char du = e[0xB], dv = e[0xC];
        if (armed && k < 2) {   // DIV-0127: the sky's last row and column are its own
            du = static_cast<unsigned char>(w - 1);
            dv = static_cast<unsigned char>(h - 1);
        }
        Piece(x, x + w, top, bottom, u, static_cast<unsigned char>(du + u), v, static_cast<unsigned char>(dv + v),
              e[8]);
    }
    if (wide != 0) {
        // A band beside a piece on the picture's edge: the piece's texels
        // mirrored about the edge column, and, past the piece's width (the
        // 64-wide right pieces under 120 columns), mirrored again - so the
        // texel at the edge continues outward and no run shows a seam. Run
        // j covers columns j * w .. from the edge; the even runs mirror,
        // the odd ones read forward. Every span follows the w - 1 rule.
        for (U k = 0; k < at::kOverlayCount; ++k) {
            const unsigned char* const e = At(::bof3::Img(at::kOverlayPieces.v) + at::kOverlayStride * k);
            const U x = static_cast<U>(S16(e)), top = static_cast<U>(S16(e + 2)) + dy;
            const U w = static_cast<U>(S16(e + 4)), h = static_cast<U>(S16(e + 6));
            const U bottom = top + h;
            const unsigned char u = e[9], v = e[0xA];
            unsigned char dv = e[0xC];
            if (k < 2) dv = static_cast<unsigned char>(h - 1);
            const unsigned char v1 = static_cast<unsigned char>(dv + v);
            const bool left = x == 0, right = x + w == 320;
            if (!left && !right) continue;
            for (U from = 0; from < wide; from += w) {
                const U n = wide - from < w ? wide - from : w;
                const bool mirror = ((from / w) & 1u) == 0;
                if (left) {
                    const U x0 = 0u - (from + n), x1 = 0u - from;
                    if (mirror)
                        Piece(x0, x1, top, bottom, static_cast<unsigned char>(u + n - 1), u, v, v1, e[8]);
                    else
                        Piece(x0, x1, top, bottom, static_cast<unsigned char>(u + w - n),
                              static_cast<unsigned char>(u + w - 1), v, v1, e[8]);
                }
                if (right) {
                    const U x0 = 320u + from, x1 = x0 + n;
                    if (mirror)
                        Piece(x0, x1, top, bottom, static_cast<unsigned char>(u + w - 1),
                              static_cast<unsigned char>(u + w - n), v, v1, e[8]);
                    else
                        Piece(x0, x1, top, bottom, u, static_cast<unsigned char>(u + n - 1), v, v1, e[8]);
                }
            }
        }
    }
}

// original 0x515440 (the tracks; called by EffectKind18Sub61_Run every frame):
// for followers 1 and 2 (Sprite_Objects[1], [2]): when the last mark's cell x
// (the follower's ring head, 0x6BC704) is past the follower's (x + 0x4000) >>
// 16, a new mark at the next of 32 entries - (x cell, the z cell byte +0x3A,
// the parity flipped). Then every mark of the ring: the map item half at its
// cell, a POLY_FT4 copying the half's four corners, the texture the follower's
// first byte (0x65F4E0) + the parity | 0x25808000, linked at the cell; and the
// half one z step on (0x65F4E4), the second byte, linked there.
extern "C" void __cdecl EffectKind18Sub61_DrawTracks(void) {
    auto copy = [](unsigned char* p, const unsigned char* item) {
        for (U corner = 8; corner < 0x48; corner += 0x10) std::memcpy(p + corner, item + corner, 12);
    };
    for (U r = 0; r < at::kFollowerCount; ++r) {
        const unsigned char* const follower = At(::bof3::Img(Sprite_Objects_addr) + 0xd8 + at::kFollowerStride * r);
        const std::int32_t cell = I(UL(follower) + 0x4000u) >> 16;
        unsigned char* const head = At(::bof3::Img(at::kTrackHeads.v) + r);
        const U h = *head;
        if (h >= at::kTrackEntries)
            bof3::Fatal("EffectKind18Sub61_DrawTracks: follower %u's head is %u, past the ring's %u entries - the "
                        "original reads and writes past it (docs/effect_6d.md section 7)",
                        (unsigned)r, (unsigned)h, at::kTrackEntries);
        const unsigned char* const last = At(::bof3::Img(at::kTrackRing.v) + at::kTrackStride * h + 3 * r);
        if (static_cast<std::int32_t>(last[0]) > cell) {
            const U next = (h + 1) & 0x1F;
            *head = static_cast<unsigned char>(next);
            unsigned char* const mark = At(::bof3::Img(at::kTrackRing.v) + at::kTrackStride * next + 3 * r);
            mark[0] = static_cast<unsigned char>(cell);
            mark[1] = follower[6];
            mark[2] = static_cast<unsigned char>((last[2] - 1) & 1);
        }
        for (U k = 0; k < at::kTrackEntries; ++k) {
            const unsigned char* const t = At(::bof3::Img(at::kTrackRing.v) + 3 * r + at::kTrackStride * k);
            const unsigned char* item = SH_CALL(MapView_ItemHalfAt)(t[0], t[1]);
            if (item == nullptr) continue;
            {
                unsigned char* const p = Gfx_PacketNext;
                SH_CALL(Gpu_SetPolyFT4)(p);
                SH_CALL(Gpu_SetShadeTex)(p, 1);
                copy(p, item);
                SH_CALL(Prim_SetTexture)(At(::bof3::Img(at::kTrackTextures.v) + 2 * r)[0] + t[2] + 0x25808000u, p, 1);
                SH_CALL(MapView_LinkPrimAt)(static_cast<U>(t[0]) << 16, static_cast<U>(t[1]) << 16, 0, 0x48);
            }
            const U step = S8U(At(::bof3::Img(at::kTrackSteps.v) + r)[0]);
            item = SH_CALL(MapView_ItemHalfAt)(t[0], static_cast<long>(t[1] + step));
            if (item == nullptr) continue;
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 1);
            copy(p, item);
            SH_CALL(Prim_SetTexture)(At(::bof3::Img(at::kTrackTextures.v) + 2 * r + 1)[0] + t[2] + 0x25808000u, p, 1);
            const U step2 = S8U(At(::bof3::Img(at::kTrackSteps.v) + r)[0]);
            SH_CALL(MapView_LinkPrimAt)(static_cast<U>(t[0]) << 16, (t[1] + step2) << 16, 0, 0x48);
        }
    }
}

// original 0x5156B0 (the glow; called by step 3 while +0x38 is above 0x80): a
// draw mode (page 0xB5, additive) committed at slot 4 (0xC); +9 up to 0xFF;
// past 0x14 by t, a disc at (0x82, +9 + 10) of radius 20t + 0x20 (at most 500)
// and shade 12t + 0x80 (at most 0xFF); then four rings of width 0x40, 0x20, 8
// and 0x10 (shade 12 * +9, at most 0x30) about (0x82 + Math_Sin(0x100) * (0x1E
// - +9) * m / 4096, +9 + 10 + Math_Cos(0x100) * (0x1E - +9) * m / 4096) for m 9,
// 8, 6 and 4, +9 read again for each.
extern "C" void __cdecl EffectKind18Sub61_DrawGlow(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
    if (S()[9] < 0xFF) S()[9] = static_cast<unsigned char>(S()[9] + 1);
    {
        const U b = S()[9];
        const U t = b > 0x14 ? b - 0x14 : 0;
        U radius = t * 20 + 0x20;
        if (I(radius) > 0x1F4) radius = 0x1F4;
        U shade = t * 12 + 0x80;
        if (I(shade) > 0xFF) shade = 0xFF;
        SH_CALL(EffectKind18Sub61_DrawDisc)(0x82, I(b + 0xA), I(radius), shade);
    }
    U width = static_cast<U>(S()[9]) * 12;
    if (I(width) > 0x30) width = 0x30;
    struct Ring { U times; int reach; };
    static constexpr Ring kRings[] = {{9, 0x40}, {8, 0x20}, {6, 8}, {4, 0x10}};
    for (const Ring& ring : kRings) {
        const U b = S()[9];
        const U c = static_cast<U>(SH_CALL(Math_Cos)(0x100));
        const std::int32_t ry = Div4096(c * (0x1Eu - b) * ring.times) + I(b) + 0xA;
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(0x100));
        const U b2 = S()[9];
        const std::int32_t rx = Div4096(sn * (0x1Eu - b2) * ring.times) + 0x82;
        SH_CALL(EffectKind18Sub61_DrawRing)(rx, ry, ring.reach, width);
    }
}

// original 0x515950 (cdecl (x, y, radius, shade); called four times by the
// glow): 64 steps about (x, y), angles k * 0x40 and (k + 1) * 0x40, each two
// semi-transparent POLY_G4s: radius + 2 (black) in to radius (shade), and
// radius (shade) in to radius - 8 (black); committed at slot 4 (0x44).
extern "C" void __cdecl EffectKind18Sub61_DrawRing(int x, int y, int radius, unsigned shade) {
    const unsigned char c = static_cast<unsigned char>(shade);
    const U r = static_cast<U>(radius), outer = r + 2, inner = r - 8;
    // a corner: Math_Sin(angle) * reach / 4096 + x, then Math_Cos for y
    auto corner = [&](unsigned char* p, int angle, U reach) {
        U t = static_cast<U>(SH_CALL(Math_Sin)(angle));
        SetUL(p, FloatBits(Div4096(t * reach) + x));
        t = static_cast<U>(SH_CALL(Math_Cos)(angle));
        SetUL(p + 4, FloatBits(Div4096(t * reach) + y));
    };
    auto colours = [](unsigned char* p, unsigned char first, unsigned char second) {
        for (U k = 0; k < 3; ++k) {
            p[4 + k] = first;
            p[0x14 + k] = first;
            p[0x24 + k] = second;
            p[0x34 + k] = second;
        }
    };
    for (U a = 0; a < 0x40000u; a += 0x1000u) {
        const int a0 = static_cast<int>(a >> 6), a1 = static_cast<int>((a + 0x1000u) >> 6);
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            corner(p + 8, a0, outer);
            corner(p + 0x18, a1, outer);
            corner(p + 0x28, a0, r);
            corner(p + 0x38, a1, r);
            colours(p, 0, c);
            SH_CALL(Gfx_CommitPrim)(4, 0x44);
        }
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            corner(p + 8, a0, r);
            corner(p + 0x18, a1, r);
            corner(p + 0x28, a0, inner);
            corner(p + 0x38, a1, inner);
            colours(p, c, 0);
            SH_CALL(Gfx_CommitPrim)(4, 0x44);
        }
    }
}

// original 0x515CC0 (cdecl (x, y, radius, shade); called by the glow): 64
// semi-transparent POLY_G3s about (x, y), each from the centre (shade, grey) to
// two points of the rim at radius (black), angles k * 0x40 and (k + 1) * 0x40
// (Math_Sin for x, Math_Cos for y, * radius / 4096), committed at slot 4
// (0x34).
extern "C" void __cdecl EffectKind18Sub61_DrawDisc(int x, int y, int radius, unsigned shade) {
    const U fx = FloatBits(x), fy = FloatBits(y);
    const U r = static_cast<U>(radius);
    for (U a = 0; a < 0x40000u; a += 0x1000u) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, fx);
        SetUL(p + 0xC, fy);
        const int a0 = static_cast<int>(a >> 6);
        U t = static_cast<U>(SH_CALL(Math_Sin)(a0));
        SetUL(p + 0x18, FloatBits(Div4096(t * r) + x));
        t = static_cast<U>(SH_CALL(Math_Cos)(a0));
        SetUL(p + 0x1C, FloatBits(Div4096(t * r) + y));
        const int a1 = static_cast<int>((a + 0x1000u) >> 6);
        t = static_cast<U>(SH_CALL(Math_Sin)(a1));
        SetUL(p + 0x28, FloatBits(Div4096(t * r) + x));
        t = static_cast<U>(SH_CALL(Math_Cos)(a1));
        const std::int32_t y2 = Div4096(t * r) + y;
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x16] = 0;
        p[0x24] = 0;
        p[4] = static_cast<unsigned char>(shade);
        p[5] = static_cast<unsigned char>(shade);
        p[6] = static_cast<unsigned char>(shade);
        p[0x25] = 0;
        SetUL(p + 0x2C, FloatBits(y2));
        p[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(4, 0x34);
    }
}

// original 0x515E00 (the mist; a tail jump from EffectKind18Sub61_Run): nothing
// while +2 is 0. Else two layers, each a draw mode (page 0x97, dtd, the window
// 64 x 64) committed at slot 4 (0xC), two semi-transparent textured quads side
// by side 256 wide and 256 high from (-o, o - 0x20), CLUT 0x78C0, page 0x7B,
// committed (0x48), and a draw mode (page 0x97, the window 256 x 256) (0xC):
// the first layer's offset +2 & 0x3F and shade (0x80 - |0x80 - +2|) / 2, the
// second's (+2 & 0x1F) * 2 and shade 0x80 - |0x80 - +2| while that is under
// 0x40 off 0x80, else (Math_Cos((+2 - 0x40) << 5) / 128 + 0x60) / 2. Then +2 up
// 4.
extern "C" void __cdecl EffectKind18Sub61_DrawMist(void) {
    const U c = S()[2];
    if (c == 0) return;
    const std::int32_t off = Abs(0x80u - c);
    const std::int32_t e = 0x80 - off;
    U shades[2];
    shades[0] = static_cast<U>(e >> 1);
    if (off > 0x40) {
        shades[1] = static_cast<U>(e);
    } else {
        const std::int32_t t = SH_CALL(Math_Cos)(static_cast<int>((c - 0x40u) << 5));
        shades[1] = static_cast<U>(((t >> 7) + 0x60) >> 1);
    }
    U offsets[2];
    {
        const U c2 = S()[2];
        offsets[0] = c2 & 0x3F;
        offsets[1] = (c2 & 0x1F) << 1;
    }
    auto quad = [](unsigned char* p, U x0, U x1, U top, U bottom, unsigned char shade) {
        SetUL(p + 8, x0);
        SetUL(p + 0xC, top);
        SetUL(p + 0x18, x1);
        SetUL(p + 0x1C, top);
        SetUL(p + 0x28, x0);
        SetUL(p + 0x2C, bottom);
        SetUL(p + 0x38, x1);
        SetUL(p + 0x3C, bottom);
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x24] = 0xFF;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x35] = 0xFF;
        p[0x44] = 0xFF;
        p[0x45] = 0xFF;
        p[4] = shade;
        p[5] = shade;
        p[6] = shade;
        SetWord(p + 0x26, 0x7B);
        SetWord(p + 0x16, 0x78C0);
    };
    for (U layer = 0; layer < 2; ++layer) {
        const std::int32_t o = I(offsets[layer]);
        const unsigned char shade = static_cast<unsigned char>(shades[layer]);
        {
            const U window = Window(0, 0x40);
            SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x97, window);
        }
        SH_CALL(Gfx_CommitPrim)(4, 0xC);
        const U top = FloatBits(o - 0x20), bottom = FloatBits(o + 0xE0);
        const U left = FloatBits(-o), middle = FloatBits(0x100 - o), right = FloatBits(0x200 - o);
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 0);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            quad(p, left, middle, top, bottom, shade);
            SH_CALL(Gfx_CommitPrim)(4, 0x48);
        }
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 0);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            quad(p, middle, right, top, bottom, shade);
            SH_CALL(Gfx_CommitPrim)(4, 0x48);
        }
        {
            const U window = Window(0, 0x100);
            SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x97, window);
        }
        SH_CALL(Gfx_CommitPrim)(4, 0xC);
    }
    S()[2] = static_cast<unsigned char>(S()[2] + 4);
}

// original 0x516090 (EffectKind18_States[0x62], 0x6541F4; hidden in 0x515CC0):
// Camera_Distance 0x5DC; for 35 columns j from Field_Kind2X's cell - 0x14 and
// 35 rows from Field_Kind2Z's cell - 0x14 inside the map (AreaMap_Header's
// width and height), the corner bytes 0 and 2 of each cell up by Math_Sin's
// step at ((Frame_Counter + 3j) << 7) less the one 0x80 before (each >> 10),
// bytes 1 and 3 by the step at + 3 less + 2. Then Sprite_ObjectsExtra[0]'s
// height from the ground at its point, its +0x14 0, MapView_Redraw 2.
extern "C" void __cdecl EffectKind18Sub62_Ripple(void) {
    const std::int32_t row0 = S16(::bof3::Img(at::kKind2ZCell.v)) - 0x14;
    const std::int32_t col0 = S16(::bof3::Img(at::kKind2XCell.v)) - 0x14;
    Camera_Distance = 0x5DC;
    for (U j = 0; j < 35; ++j) {
        const U phase = 3 * j;
        const std::int32_t s1 = SH_CALL(Math_Sin)(static_cast<int>((((Frame_Counter + phase) << 7) - 0x80u) & 0xFFFu));
        const std::int32_t s2 = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + phase) << 7) & 0xFFFu));
        const U up02 = static_cast<U>((s2 >> 10) - (s1 >> 10));
        const std::int32_t s3 = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + phase + 3) << 7) & 0xFFFu));
        const std::int32_t s4 = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + phase + 2) << 7) & 0xFFFu));
        const U up13 = static_cast<U>((s3 >> 10) - (s4 >> 10));
        std::int32_t row = row0;
        for (U n = 0; n < 35; ++n, ++row) {
            if (row < 0) continue;
            if (row >= static_cast<std::int32_t>(AreaMap_Header[1])) continue;
            const std::int32_t col = static_cast<std::int32_t>(j) + col0;
            if (col < 0) continue;
            if (col >= static_cast<std::int32_t>(AreaMap_Header[0])) continue;
            unsigned char* const corners = reinterpret_cast<unsigned char*>(&AreaMap_Corners);
            const U at0 = 4 * static_cast<U>(row * static_cast<std::int32_t>(AreaMap_Header[0]) + col);
            corners[at0] = static_cast<unsigned char>(corners[at0] + up02);
            const U at2 = 4 * static_cast<U>(row * static_cast<std::int32_t>(AreaMap_Header[0]) + col) + 2;
            corners[at2] = static_cast<unsigned char>(corners[at2] + up02);
            const U at1 = 4 * static_cast<U>(row * static_cast<std::int32_t>(AreaMap_Header[0]) + col) + 1;
            corners[at1] = static_cast<unsigned char>(corners[at1] + up13);
            const U at3 = 4 * static_cast<U>(row * static_cast<std::int32_t>(AreaMap_Header[0]) + col) + 3;
            corners[at3] = static_cast<unsigned char>(corners[at3] + up13);
        }
    }
    const long h = SH_CALL(AreaMap_Elevation)(Sprite_ObjectsExtra[0].x, Sprite_ObjectsExtra[0].z);
    Sprite_ObjectsExtra[0].height_whole = static_cast<U>(h);
    Sprite_ObjectsExtra[0].step_y = 0;
    MapView_Redraw = 2;
}

// original 0x516280 (EffectKind18_States[0x63], 0x6541F8; hidden in 0x515CC0):
// Cond_ByteFE 1; a draw mode (page 0xB5, additive) committed at slot 4 (0xC);
// six semi-transparent POLY_G4s, quad n from the points k and k + 1 of
// 0x65F4E8 (k the n-th byte of 0x65F554) to the same two turned by
// Cond_AngleFB + 0x200 (x by Math_Sin, y by Math_Cos, times each point's reach
// of 0x65F530 / 4096, z the reach table's), projected, depths by
// Gte_PrimDepths4_10B, the two near corners the points' shades (grey), the far
// ones black, committed (0x44); a draw mode (page 0x95) (0xC).
extern "C" void __cdecl EffectKind18Sub63_Glow(void) {
    Cond_ByteFE = 1;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
    unsigned char* const v = Vertices();
    for (U n = 0; n < at::kGlowQuads; ++n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const U k = At(::bof3::Img(at::kGlowOrder.v) + n)[0];
            for (U i = 0; i < 2; ++i) {
                const unsigned char* const point = At(::bof3::Img(at::kGlowPoints.v) + 8 * (k + i));
                SetWord(v + 8 * i, Word(point));
                SetWord(v + 8 * i + 2, Word(point + 2));
                SetWord(v + 8 * i + 4, Word(point + 4));
            }
        }
        for (U i = 0; i < 2; ++i) {
            unsigned char* const out = v + 0x10 + 8 * i;
            const U k = At(::bof3::Img(at::kGlowOrder.v) + n)[0] + i;
            const U sn = static_cast<U>(SH_CALL(Math_Sin)(S16(AddressOf(&Cond_AngleFB)) + 0x200));
            const U x = static_cast<U>(Div4096(sn * static_cast<U>(S16(::bof3::Img(at::kGlowReach.v) + 4 * k)))) + Word(At(::bof3::Img(at::kGlowPoints.v) + 8 * k));
            SetWord(out, x);
            const U k2 = At(::bof3::Img(at::kGlowOrder.v) + n)[0] + i;
            const U cs = static_cast<U>(SH_CALL(Math_Cos)(S16(AddressOf(&Cond_AngleFB)) + 0x200));
            const U y = static_cast<U>(Div4096(cs * static_cast<U>(S16(::bof3::Img(at::kGlowReach.v) + 4 * k2)))) + Word(At(::bof3::Img(at::kGlowPoints.v) + 8 * k2 + 2));
            SetWord(out + 2, y);
            SetWord(out + 4, Word(At(::bof3::Img(at::kGlowReach.v) + 4 * k2 + 2)));
        }
        long depth;
        SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                                   reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                                   reinterpret_cast<float*>(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10B)(p);
        const unsigned char near0 = At(::bof3::Img(at::kGlowPoints.v) + 8 * At(::bof3::Img(at::kGlowOrder.v) + n)[0] + 6)[0];
        p[6] = near0;
        p[5] = near0;
        p[4] = near0;
        const unsigned char near1 = At(::bof3::Img(at::kGlowPoints.v) + 8 * At(::bof3::Img(at::kGlowOrder.v) + n)[0] + 0xE)[0];
        p[0x36] = 0;
        p[0x16] = near1;
        p[0x15] = near1;
        p[0x14] = near1;
        p[0x26] = 0;
        p[0x35] = 0;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x24] = 0;
        SH_CALL(Gfx_CommitPrim)(4, 0x44);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
}

void EffectKind18Sub61_Inject() {
    BOF3_INJECT(EffectKind18Sub61_Run);
    BOF3_INJECT(EffectKind18Sub61_WaitFocus);
    BOF3_INJECT(EffectKind18Sub61_Slide);
    BOF3_INJECT(EffectKind18Sub61_Hold);
    BOF3_INJECT(EffectKind18Sub61_Rise);
    BOF3_INJECT(EffectKind18Sub61_Reset);
    BOF3_INJECT(EffectKind18Sub61_Rearm);
    BOF3_INJECT(EffectKind18Sub61_ClearTracks);
    BOF3_INJECT(EffectKind18Sub61_DrawOverlay);
    BOF3_INJECT(EffectKind18Sub61_DrawTracks);
    BOF3_INJECT(EffectKind18Sub61_DrawGlow);
    BOF3_INJECT(EffectKind18Sub61_DrawRing);
    BOF3_INJECT(EffectKind18Sub61_DrawDisc);
    BOF3_INJECT(EffectKind18Sub61_DrawMist);
    BOF3_INJECT(EffectKind18Sub62_Ripple);
    BOF3_INJECT(EffectKind18Sub63_Glow);
}
