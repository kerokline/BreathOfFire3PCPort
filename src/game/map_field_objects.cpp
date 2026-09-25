// Group DD of the eighth takeover round (docs/map_field_objects.md): sixteen
// functions the shop and world-map routes reach, each read to its last
// instruction with capstone against bof3/BOF3.exe on 2026-09-25, and every
// site that holds its address read for what the caller does with the call.
// Faithful: no divergence. The start-up fuzz is map_field_objects_fuzz.cpp.
//
//   MapCell_DrawUprights       0x570210..0x570520 (0x311)  MapCell_Handlers 1..3, 5..0xF, 0x31..0x33, 0x35..0x3E
//   MapCell_DrawDiagonalWall   0x570530..0x570654 (0x125)  MapCell_Handlers 4, 0x34
//   MapCell_DrawFlatFaces      0x570A00..0x570AA5 (0xA6)   MapCell_Handlers 0x11..0x17, 0x19..0x1F
//   MapCell_PatchThenStep      0x571090..0x5710C3 (0x34)   MapCell_Handlers 0x24
//   MapCell_PatchThenStop      0x5710D0..0x571103 (0x34)   MapCell_Handlers 0x25
//   MapCell_DrawAnimated       0x5712E0..0x5714F7 (0x218)  MapCell_Handlers 0x27 (PSX 0x80158464, gap4)
//   AreaMap_SetupTint          0x571880..0x5718EE (0x6F)   AreaMap_SetupHandlers 0 (PSX 0x80158C70, gap4)
//   AreaMap_SetupFlatColours   0x571A30..0x571A6E (0x3F)   AreaMap_SetupHandlers 1 (PSX 0x80158E78, gap4)
//   AreaMap_SetupTexture       0x571A70..0x571AE0 (0x71)   AreaMap_SetupHandlers 2 (PSX 0x80158EE4, gap4)
//   Menu_DrawMemberStatus      0x573560..0x573891 (0x332)  5 E8 callers (PSX 0x801D8270, call-anchored)
//   Menu_DrawCell8             0x5744B0..0x574525 (0x76)   8 E8 callers (PSX 0x801DA0D4, gap)
//   Menu_DrawPlayTime          0x5746C0..0x5747C7 (0x108)  1 E8 caller, 0x599DE1
//   EventScript_SkipSwitch     0x579CF0..0x579D3F (0x50) + its 11-entry jump table 0x579D40 (PSX 0x801A584C)
//   AreaMap_SetByte            0x579F00..0x579F26 (0x27)   ~420 E8 callers (PSX 0x801A5BC8)
//   EventOp_8x                 0x57A3A0..0x57A5D8 (0x239)  EventScript_Op's table (PSX 0x801A6408)
//   ScriptFlags_Clear40        0x57C7A0..0x57C7B5 (0x16)   ~390 E8 callers, two tail jmps (PSX 0x8015CA48)
//
// The nine map-layer handlers are called only through their tables, by
// DrawLayer_Open and AreaMap_SetupEntries (both ours, both calls typed void),
// and so return nothing. The menu draws hand their last callee's eax on, as
// the originals do (0x59BEE9 and 0x599C17 return it to their own callers).
#include "game/map_field_objects.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/map_field_objects_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace map_field_objects {

namespace {
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
}  // namespace

const Callees kOriginals = {
    As<long (__cdecl*)(long, long)>(AreaMap_Elevation),
    As<long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*)>(Gte_RotTransPers3),
    As<void (__cdecl*)(float*, float*, float*)>(Gte_StoreDepthF3),
    As<void (__cdecl*)(unsigned char*)>(Gpu_SetPolyFT4),
    As<void (__cdecl*)(unsigned char*, unsigned)>(Gpu_SetShadeTex),
    As<U (__cdecl*)(unsigned, unsigned)>(Gfx_CommitPrim),
    As<long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*, long*)>(
        Gte_RotTransPers4),
    As<void (__cdecl*)(float*, float*, float*, float*)>(Gte_StoreDepthF4),
    As<void (__cdecl*)(void*)>(Gte_PrimDepths4_10),
    As<void (__cdecl*)(unsigned long, unsigned char*, int)>(Prim_SetTexture),
    As<void (__cdecl*)(int, unsigned)>(MapCell_FlatOverlay),
    As<void (__cdecl*)(const unsigned char*)>(AreaMap_ApplyPatch),
    As<unsigned char (__cdecl*)(unsigned long)>(Area_TestCondition),
    As<long (__cdecl*)(const short*, unsigned long*, long*)>(Gte_RotTransPers),
    As<void (__cdecl*)(void*)>(Gte_PrimDepthFlat4_10),
    As<long (__cdecl*)(int, int, int, int, int)>(Gfx_ClutAdjust),
    As<void (__cdecl*)(int, int, int, int, int, int)>(Menu_DrawBox),
    As<void (__cdecl*)(int, int, int, int)>(Menu_DrawItemIcon),
    As<const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*)>(Text_DrawAt),
    As<int (__cdecl*)(char*, const char*, unsigned)>(Crt_sprintf),
    As<void (__cdecl*)(int, int, int, const unsigned char*)>(Text_DrawFont8),
    reinterpret_cast<const unsigned char* (__cdecl*)(int, int, unsigned, unsigned, const unsigned char*)>(
        static_cast<std::uintptr_t>(kTinyFont)),
    As<U (__cdecl*)(int, int, const unsigned char*, int)>(Menu_DrawPieces),
    As<U (__cdecl*)(int, int, unsigned, unsigned, unsigned)>(Menu_DrawExpBar),
    As<void (__cdecl*)(unsigned char*)>(Gpu_SetSprt8),
    As<void (__cdecl*)(unsigned char*, unsigned)>(Gpu_SetSemiTrans),
    As<void (__cdecl*)(int, int, int, const unsigned char*)>(Text_DrawFont12),
    As<const unsigned char* (__cdecl*)(const unsigned char*)>(EventScript_SkipControl),
    As<void (__cdecl*)()>(EventObj_Reset),
    As<U (__cdecl*)(U)>(Sprite_SetAnimationBank),
    As<void (__cdecl*)(const unsigned char*)>(EventObj_SetFlags),
    As<void (__cdecl*)(U)>(Sprite_SetAnimation),
    As<U (__cdecl*)(U, U, U)>(AreaMap_SetByte),
};
Callees g = kOriginals;

}  // namespace map_field_objects

namespace {

using namespace map_field_objects;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Byte(U address) { return At(address)[0]; }
U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
U Dword(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void SetWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void SetDword(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
float Float(U address) {
    float f;
    std::memcpy(&f, At(address), sizeof f);
    return f;
}
void SetFloat(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }
U S8(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(v))); }
U S16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v))); }
short* Vertex(U address) { return reinterpret_cast<short*>(At(address)); }
float* FloatAt(U address) { return reinterpret_cast<float*>(At(address)); }

unsigned char* PacketNext() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(Dword(At(at::PacketNextAt())))); }
U OtSlot() { return Byte(at::OtSlotAt()); }
U SpriteCurrent() { return Dword(At(kSpriteCurrent)); }
unsigned char* Sprite() { return At(SpriteCurrent()); }

// The slot a cell handler commits to: 6 when the record's top nibble is set
// (kinds 0x10 and up), else Draw_OtSlot - zero-extended from its byte.
U HandlerSlot(U record) { return (record & 0xF0000000u) ? 6u : OtSlot(); }

}  // namespace

// --- the map-cell handlers (MapCell_Handlers, called by DrawLayer_Open) -----------------
// Each gets (record, b1, b0): the cell's record and the run head's bytes 1 and
// 0, the cell's map x and y (DrawLayer_Open).

// original 0x570210: kinds 1..3, 5..0xF and 0x31..0x33, 0x35..0x3E - upright
// quads standing on the cell. With e = (kind & 0xF) - 1 (32-bit, as the
// original's dec):
//   - the cell's origin ((b1 - 0x81) << 7, (b0 - 0x80) << 7) goes to the
//     scratch dwords kScratchX / kScratchY, and h = -(s16
//     AreaMap_Elevation((b1 << 16) - 0x10000, b0 << 16) / 2) (a C divide, toward
//     zero) to kScratchH; the scratch dwords are read again after the call;
//   - three points through Gte_RotTransPers3 into MapView_ScreenXY: the origin
//     at h - MapCell_UprightHeights[e] (Prim_VertexScratch +0x10), the origin at
//     h (+0x18), and +0 at that top height with whatever x and y it holds
//     (stale - its screen point is never read); then Gte_StoreDepthF3;
//   - culled unless x0 >= -80 and x0 <= 400 and y1 >= -20 and y0 <= 260 (x87
//     compares: a NaN fails the first three, passes the fourth);
//   - MapCell_UprightCounts[e] times (the byte read again after each): two
//     POLY_FT4s at Gfx_PacketNext (read once for the pair), each
//     Gpu_SetPolyFT4, Gpu_SetShadeTex(0), vertices 2 and 3 the top and the base
//     (their screen dwords and depths copied), u / v / CLUT / tpage from the
//     tables, colour 0x80, Gfx_CommitPrim(slot, 0x48); then the four outer
//     vertices - entry 2 (i + 3e) and the next of MapCell_UprightOffsetsX / Y,
//     about the origin, top and base - through Gte_RotTransPers4 into the pair's
//     vertices 0 and 1, and Gte_StoreDepthF4 into their depths.
// As the original has it: every table is indexed by e unchecked, so kinds with
// a low nibble of 0xA..0xF read past the nine the tables hold (counts 0, 0, 0,
// then bytes of the next table - 9, 0xFF, 0xBE); the counts' loop counter and
// both depths live in the argument slots, which the callers do not read.
extern "C" void __cdecl MapCell_DrawUprights(const unsigned char* record, unsigned b1, unsigned b0) {
    const U rec = Dword(record);
    const U slot = HandlerSlot(rec);
    const U e = ((rec >> 24) & 0xFu) - 1u;
    SetDword(At(kScratchX), (b1 - 0x81u) << 7);
    SetDword(At(kScratchY), (b0 - 0x80u) << 7);
    const U elevation = static_cast<U>(g.elevation(static_cast<long>((b1 << 16) - 0x10000u), static_cast<long>(b0 << 16)));
    const U h = static_cast<U>(-(static_cast<std::int32_t>(S16(elevation)) / 2));
    const U x = Dword(At(kScratchX));
    SetWord(At(kVertex3), x);
    SetWord(At(kVertex2), x);
    const U y = Dword(At(kScratchY));
    SetWord(At(kVertex3 + 2), y);
    SetWord(At(kVertex2 + 2), y);
    SetDword(At(kScratchH), h);
    const U top = h - Word(At(at::UprightHeightsAt() + e * 2u));
    SetWord(At(kVertex3 + 4), h);
    SetWord(At(kVertex1 + 4), h);
    SetWord(At(kVertex2 + 4), top);
    SetWord(At(kVertex0 + 4), top);
    long p = 0;
    g.rtp3(Vertex(kVertex2), Vertex(kVertex3), Vertex(kVertex0), FloatAt(kScreen0), FloatAt(kScreen1),
           FloatAt(kScreen2), &p);
    float depth0 = 0, depth1 = 0, depth2 = 0;
    g.depth_f3(&depth0, &depth1, &depth2);
    if (!(Float(kScreen0) >= -80.0f)) return;       // fcomp [0x5C4264]; test ah, 1
    if (Float(kScreen0) > 400.0f) return;           // fcomp [0x5C4260]; test ah, 0x41
    if (!(Float(kScreen1 + 4) >= -20.0f)) return;   // fcomp [0x5C422C]; test ah, 1
    if (Float(kScreen0 + 4) > 260.0f) return;       // fcomp [0x5C425C]; test ah, 0x41
    if (Byte(at::UprightCountsAt() + e) == 0) return;
    U d0, d1;
    std::memcpy(&d0, &depth0, 4);
    std::memcpy(&d1, &depth1, 4);
    const U e2 = e * 2u;
    for (U i = 0;;) {
        unsigned char* const packet = PacketNext();
        for (U j = 0; j < 2; ++j) {
            unsigned char* const prim = packet + j * 0x48u;
            g.set_poly_ft4(prim);
            g.set_shade_tex(prim, 0);
            SetDword(prim + 0x28, Dword(At(kScreen0)));
            SetDword(prim + 0x2C, Dword(At(kScreen0 + 4)));
            SetDword(prim + 0x30, d0);
            SetDword(prim + 0x38, Dword(At(kScreen1)));
            SetDword(prim + 0x3C, Dword(At(kScreen1 + 4)));
            SetDword(prim + 0x40, d1);
            const auto u02 = static_cast<unsigned char>(Byte(at::UprightU02At() + e));
            prim[0x34] = u02;
            prim[0x14] = u02;
            const auto u13 = static_cast<unsigned char>(Byte(at::UprightU13At() + e));
            prim[0x44] = u13;
            prim[0x24] = u13;
            const auto v01 = static_cast<unsigned char>(Byte(at::UprightV01At() + e2 + j));
            prim[0x25] = v01;
            prim[0x15] = v01;
            const auto v23 = static_cast<unsigned char>(Byte(at::UprightV23At() + e2 + j));
            prim[0x45] = v23;
            prim[0x35] = v23;
            prim[6] = prim[5] = prim[4] = 0x80;
            SetWord(prim + 0x16, (Byte(at::UprightClutAt() + e) >> 4) | 0x78C0u);
            SetWord(prim + 0x26, 0x1B);
            g.commit(slot, 0x48);
        }
        const U base = (i + e * 3u) * 2u;
        const U vy = Dword(At(kScratchY));
        for (U v = 0; v < 4; ++v) {
            const U k = ((v >> 1) + base) * 2u;
            SetWord(At(kVertex0 + v * 8u), Word(At(at::UprightOffsetsXAt() + k)) + Dword(At(kScratchX)));
            SetWord(At(kVertex0 + v * 8u + 2), Word(At(at::UprightOffsetsYAt() + k)) + vy);
        }
        g.rtp4(Vertex(kVertex0), Vertex(kVertex1), Vertex(kVertex2), Vertex(kVertex3),
               reinterpret_cast<float*>(packet + 8), reinterpret_cast<float*>(packet + 0x18),
               reinterpret_cast<float*>(packet + 0x50), reinterpret_cast<float*>(packet + 0x60), &p);
        g.depth_f4(reinterpret_cast<float*>(packet + 0x10), reinterpret_cast<float*>(packet + 0x20),
                   reinterpret_cast<float*>(packet + 0x58), reinterpret_cast<float*>(packet + 0x68));
        ++i;
        if (static_cast<std::int32_t>(i) >= static_cast<std::int32_t>(Byte(at::UprightCountsAt() + e))) return;
    }
}

// original 0x570530: kinds 4 and 0x34 - a wall 0x80 high along the cell's
// diagonal, from its corner (0, 1) to its corner (1, 0), at the heights of
// the cell's corner bytes 2 and 1 (unsigned, times -16): one POLY_FT4 at
// Gfx_PacketNext through Gpu_SetPolyFT4, Gpu_SetShadeTex(0),
// Gte_RotTransPers4, Gte_PrimDepths4_10, Prim_SetTexture(0xB1800110, 1) and
// Gfx_CommitPrim(slot, 0x48). The corner dword is AreaMap_Corners + (width *
// b0 + b1) * 4, 32-bit and unchecked. The vertices' fourth words are never
// written - Gte_RotTransPers4 reads three.
extern "C" void __cdecl MapCell_DrawDiagonalWall(const unsigned char* record, unsigned b1, unsigned b0) {
    const U slot = HandlerSlot(Dword(record));
    const U width = Dword(At(kHeader)) & 0xFFu;
    const unsigned char* const corner = At(kCorners + (width * b0 + b1) * 4u);
    const U y = (b0 << 7) - 0x4040u, x = (b1 << 7) - 0x4040u;
    const U z2 = (0u - corner[2]) << 4, z1 = (0u - corner[1]) << 4;
    short v[4][4] = {};
    v[0][0] = v[2][0] = static_cast<short>(x);
    v[0][1] = v[2][1] = static_cast<short>(y + 0x80u);
    v[1][0] = v[3][0] = static_cast<short>(x + 0x80u);
    v[1][1] = v[3][1] = static_cast<short>(y);
    v[0][2] = static_cast<short>(z2 - 0x80u);
    v[2][2] = static_cast<short>(z2);
    v[1][2] = static_cast<short>(z1 - 0x80u);
    v[3][2] = static_cast<short>(z1);
    unsigned char* const prim = PacketNext();
    g.set_poly_ft4(prim);
    g.set_shade_tex(prim, 0);
    long p = 0;
    g.rtp4(v[0], v[1], v[2], v[3], reinterpret_cast<float*>(prim + 8), reinterpret_cast<float*>(prim + 0x18),
           reinterpret_cast<float*>(prim + 0x28), reinterpret_cast<float*>(prim + 0x38), &p);
    g.depths4(prim);
    g.set_texture(0xB1800110u, prim, 1);
    g.commit(slot, 0x48);
}

// original 0x570A00: kinds 0x11..0x17 and 0x19..0x1F (0x18 is a bare ret) -
// the cell's flat faces. The cell's draw item by the view ring's diagonal
// lookup (MapView_ItemAt's, inline here, docs/field-misc.md): (x', z') = (b1,
// b0) - MapView_Origin, both 32-bit; row (x' + z' + MapView_Row + 1) and column
// ((x' - z') / 2 + MapView_Column + 1), each wrapped by one subtraction, when 0
// <= x' + z' < 0x38 and 0 <= x' - z' < 0x38; the item the low 12 bits of the
// ring word +2. A non-zero one goes to MapCell_FlatOverlay(kind - 0x10, item).
extern "C" void __cdecl MapCell_DrawFlatFaces(const unsigned char* record, unsigned b1, unsigned b0) {
    const U ox = S16(Word(At(at::OriginAt()))), oz = S16(Word(At(at::OriginAt() + 2)));
    const U dx = b1 - ox, dz = b0 - oz;
    const auto sum = static_cast<std::int32_t>(dx + dz);
    if (sum >= 0x38 || sum < 0) return;
    auto row = static_cast<std::int32_t>(static_cast<U>(sum) + S16(Word(At(at::RowAt()))) + 1u);
    if (row >= 0x38) row -= 0x38;
    const auto diff = static_cast<std::int32_t>(dx - dz);
    if (diff >= 0x38 || diff < 0) return;
    auto column = static_cast<std::int32_t>(static_cast<U>(diff / 2) + S16(Word(At(at::ColumnAt()))) + 1u);
    if (column >= 0x1C) column -= 0x1C;
    const U index = static_cast<U>(column) + static_cast<U>(row) * 28u;
    const U item = Word(At(kCellItemWords + index * 4u)) & 0xFFFu;
    if (item == 0) return;
    g.flat_overlay(static_cast<int>((Dword(record) >> 24) - 0x10u), item);
}

// original 0x571090 / 0x5710D0: kinds 0x24 and 0x25 - the record's kind byte
// becomes 0x25 (then 0x23, a bare ret) before the patch list entry at dword
// (record & 0xFFFF) + AreaMap_PatchBase of the area block goes to
// AreaMap_ApplyPatch. So an 0x24 record applies its patch on two draws of the
// cell, and then never again. The original pops the argument slot
// AreaMap_ApplyPatch overwrote into ecx; DrawLayer_Open reloads ecx.
namespace {
void PatchThen(unsigned char* record, U kind) {
    const U r = (Dword(record) & 0x00FFFFFFu) | kind;
    SetDword(record, r);
    const U index = (r & 0xFFFFu) + (Dword(At(kPatchBase)) & 0xFFFFu);
    g.apply_patch(At(kHeader + index * 4u));
}
}  // namespace
extern "C" void __cdecl MapCell_PatchThenStep(unsigned char* record, unsigned, unsigned) { PatchThen(record, 0x25000000u); }
extern "C" void __cdecl MapCell_PatchThenStop(unsigned char* record, unsigned, unsigned) { PatchThen(record, 0x23000000u); }

// original 0x5712E0 (PSX 0x80158464, gap4 pair): kind 0x27 - an animated
// sprite standing on the cell. Unless Area_TestCondition(word +0) says no: one
// point, (s8 byte 7 + (b1 - 0x80) * 64) * 2, (s8 byte 6 + (b0 - 0x80) * 64) * 2,
// word +4 (Prim_VertexScratch +0), through Gte_RotTransPers into
// MapView_ScreenXY; culled unless -60 <= x <= 380 and -150 <= y <= 300 (a NaN
// fails the lower bounds) and the depth p it returns is non-zero. The frame:
// with period byte +8 and n = (byte +9 + 5) >> 2, t = Frame_Counter mod
// period (unsigned), k the first index with t < byte +0xA + k (signed); dword
// +(n + 2k + 2) * 4 holds four s8 corner offsets and the next dword the
// texture. The POLY_FT4 at Gfx_PacketNext: Gpu_SetPolyFT4, Gpu_SetShadeTex(0),
// Gte_PrimDepthFlat4_10; its corners x - o0 * 1125 / p, then + o1 * 1125 / p
// (vertices 0 / 2 and 1 / 3), y - o2 * 1125 / p, then + o3 * 1125 / p (0 / 1
// and 2 / 3) - x87 at the game's double precision, the running value kept in
// the register and each corner stored as a float; Prim_SetTexture(texture
// without bit 15, 1); then, by bit 30 of the texture dword read again,
// Gfx_CommitPrim(6 or Draw_OtSlot, 0x48).
// As the original has it: a period of 0 divides by zero and the threshold scan
// has no bound (both read, not run); the depth is stored over the argument
// slot and then the corner offsets over it again.
extern "C" void __cdecl MapCell_DrawAnimated(const unsigned char* record, unsigned b1, unsigned b0) {
    if (g.test(Word(record)) == 0) return;
    SetWord(At(kVertex0), (S8(Dword(record + 4) >> 24) + ((b1 - 0x80u) << 6)) << 1);
    SetWord(At(kVertex0 + 2), (S8(Dword(record + 4) >> 16) + ((b0 - 0x80u) << 6)) << 1);
    SetWord(At(kVertex0 + 4), Word(record + 4));
    long depth = 0;
    const auto p = static_cast<std::int32_t>(
        g.rtp(Vertex(kVertex0), reinterpret_cast<unsigned long*>(At(kScreen0)), &depth));
    if (!(Float(kScreen0) >= -60.0f)) return;      // fcomp [0x5C4210]; test ah, 1
    if (Float(kScreen0) > 380.0f) return;          // fcomp [0x5C420C]; test ah, 0x41
    if (!(Float(kScreen0 + 4) >= -150.0f)) return; // fcomp [0x5C4200]; test ah, 1
    if (Float(kScreen0 + 4) > 300.0f) return;      // fcomp [0x5C41FC]; test ah, 0x41
    if (p == 0) return;
    const U timing = Dword(record + 8);
    const U t = Dword(At(at::FrameCounterAt())) % (timing & 0xFFu);
    U k = 0;
    while (static_cast<std::int32_t>(t) >= static_cast<std::int32_t>(record[0xA + k])) ++k;
    unsigned char* const prim = PacketNext();
    const U n = (((timing >> 8) & 0xFFu) + 5u) >> 2;
    g.set_poly_ft4(prim);
    g.set_shade_tex(prim, 0);
    g.depth_flat4(prim);
    const unsigned char* const offsets = record + (n + k * 2u + 2u) * 4u;
    const unsigned char* const texture = record + (n + k * 2u) * 4u + 0xC;
    const double dp = static_cast<double>(p);
    const double x = static_cast<double>(Float(kScreen0));
    const double a = x - static_cast<double>(static_cast<std::int32_t>(S8(Dword(offsets)))) * 1125.0 / dp;
    SetFloat(prim + 0x28, static_cast<float>(a));
    SetFloat(prim + 8, static_cast<float>(a));
    const double b = static_cast<double>(static_cast<std::int32_t>(S8(Dword(offsets) >> 8))) * 1125.0 / dp + a;
    SetFloat(prim + 0x38, static_cast<float>(b));
    SetFloat(prim + 0x18, static_cast<float>(b));
    const double y = static_cast<double>(Float(kScreen0 + 4));
    const double c = y - static_cast<double>(static_cast<std::int32_t>(S8(Dword(offsets) >> 16))) * 1125.0 / dp;
    SetFloat(prim + 0x1C, static_cast<float>(c));
    SetFloat(prim + 0xC, static_cast<float>(c));
    const double d = static_cast<double>(static_cast<std::int32_t>(S8(Dword(offsets) >> 24))) * 1125.0 / dp + c;
    SetFloat(prim + 0x3C, static_cast<float>(d));
    SetFloat(prim + 0x2C, static_cast<float>(d));
    g.set_texture(Dword(texture) & 0xFFFF7FFFu, prim, 1);
    if (Dword(texture) & 0x40000000u) g.commit(6, 0x48);
    else g.commit(OtSlot(), 0x48);
}

// --- the area set-up list's handlers (AreaMap_SetupHandlers, AreaMap_SetupEntries) --------

// original 0x571880 (PSX 0x80158C70, gap4): entry 0, the area's palette tint.
// Gfx_ClutAdjust(word +4, word +6, then - when Area_TestCondition(word +0),
// its al signed, is non-zero - the s8 bytes +8, +9, +10 as red, green and
// blue, else 0, 0, 0). The words are read after the condition.
extern "C" void __cdecl AreaMap_SetupTint(const unsigned char* entry) {
    const auto yes = static_cast<signed char>(g.test(Word(entry)));
    const U d = Dword(entry + 4);
    const int columns = static_cast<int>(d & 0xFFFFu), rows = static_cast<int>(d >> 16);
    if (yes == 0) {
        g.clut_adjust(columns, rows, 0, 0, 0);
        return;
    }
    const U rgb = Dword(entry + 8);
    g.clut_adjust(columns, rows, static_cast<int>(S8(rgb)), static_cast<int>(S8(rgb >> 8)),
                  static_cast<int>(S8(rgb >> 16)));
}

// original 0x571A30 (PSX 0x80158E78, gap4): entry 1, MapCell_FlatOverlay's two
// colours (area block +0x1A and +0x1C): the high words of dwords +4 and +8
// when Cond_ByteFF (the story bit AreaMap_SetupEntries copies in) is set,
// else their low words.
extern "C" void __cdecl AreaMap_SetupFlatColours(const unsigned char* entry) {
    if (Byte(at::CondByteFFAt()) != 0) {
        SetWord(At(kFlatColour0), Dword(entry + 4) >> 16);
        SetWord(At(kFlatColour1), Dword(entry + 8) >> 16);
    } else {
        SetWord(At(kFlatColour0), Word(entry + 4));
        SetWord(At(kFlatColour1), Word(entry + 8));
    }
}

// original 0x571A70 (PSX 0x80158EE4, gap4): entry 2, a texture of the area's
// run replaced - dword +4 when Cond_ByteFF is set, else +8 - at (height *
// width + 1) / 2 + offset + tile of the block, tile the word of map (x = byte
// 1, y = byte 0) as MapView_CellTextures reads it (word y * width + x + offset
// * 2). All unchecked.
extern "C" void __cdecl AreaMap_SetupTexture(const unsigned char* entry) {
    const U flag = Byte(at::CondByteFFAt());
    const U width = Dword(At(kHeader)) & 0xFFu;
    const U d = Dword(entry);
    const U offset = Word(At(kHeaderOffset));
    const U texture = Dword(entry + 4 + (flag == 0 ? 4 : 0));
    const U tile = Word(At(kHeader + ((d & 0xFFu) * width + ((d >> 8) & 0xFFu) + offset * 2u) * 2u));
    const auto half = static_cast<std::int32_t>(Byte(kHeader + 1) * width + 1u) / 2;
    SetDword(At(kHeader + (static_cast<U>(half) + offset + tile) * 4u), texture);
}

// --- the menu draws -------------------------------------------------------------------------

// original 0x573560 (PSX 0x801D8270, call-anchored): a party
// member's status entry in the menus' lists (menu_frame.cpp: the PSX
// 0x801EA99C's entry body). Its box Menu_DrawBox(x + 3, y + 3, 0x7D, 0x30,
// highlight, the window colour); the record CharacterRecords + (member & 0xFF) *
// 0xA4; its portrait Menu_DrawItemIcon(x + 0x56, y + 2, byte +9, shade 1 when
// highlighted, else 2 when +0x10 bit 7, else 0); the name Text_DrawAt(x + 0x14,
// y + 3, colour, 5, record); the level "%3d" of byte +0xA at (x + 0x16, y + 0x17)
// in the 8 px font; a status word through 0x516E70 at (x + 0x2E, y + 0x17) -
// 0x66A0E8 with +0x10 bit 7, 0x66A0F0 with bit 5, alternating on Frame_Counter
// bit 5 with both; HP "%3d/   " and "   /%3d" (+0x18, +0x20) at y + 0x1F and AP
// (+0x1A, +0x22) at y + 0x27; the frame pieces 0x6632C8; and the EXP bar
// Menu_DrawExpBar(x + 0x14, y + 0x12, member, level, dword +0xC). +0x10 bit 13
// is set when HP is below a quarter of the maximum, cleared otherwise.
// Colours: 7 for everything when highlighted; otherwise 0, the HP 4 below a
// quarter (by the bit just stored) and 2 at 1 or less, the maximum HP 4 when
// byte +0x1E is set, the AP 4 below a quarter and 2 at 0, the maximum AP 0,
// and the status word 1.
// As the original has it: the colours go out in a dword whose upper bytes are
// stale stack (Text_DrawString and Text_DrawFont8 read the low byte, 6 bits);
// the window colour with the entry's ecx above it (Menu_DrawBox reads the
// byte); the level to Menu_DrawExpBar with Menu_DrawPieces' eax above it, which
// ours hands on as it is. Every field is read after the call before it.
extern "C" unsigned long __cdecl Menu_DrawMemberStatus(int x, int y, unsigned member, unsigned highlight) {
    const bool lit = (highlight & 0xFFu) != 0;
    const U bl = lit ? 7u : 0u;
    U colour = bl;
    g.draw_box(x + 3, y + 3, 0x7D, 0x30, static_cast<int>(highlight), static_cast<int>(Byte(kWindowColour)));
    unsigned char* const record = At(kCharRecords + (member & 0xFFu) * kCharStride);
    const U shade = lit ? 1u : ((record[0x10] >> 6) & 2u);
    g.item_icon(x + 0x56, y + 2, static_cast<int>(record[9]), static_cast<int>(shade));
    g.text_at(x + 0x14, y + 3, static_cast<int>(colour), 5, record);
    char* const buffer = reinterpret_cast<char*>(At(kPrintBuf));
    const auto* const text = reinterpret_cast<const unsigned char*>(buffer);
    g.sprintf(buffer, reinterpret_cast<const char*>(At(kFormat3d)), record[0xA]);
    g.font8(x + 0x16, y + 0x17, static_cast<int>(colour), text);
    const U flags = Word(record + 0x10);
    U status = 0;
    if (flags & 0x80u) {
        if (flags & 0x20u) status = (Byte(at::FrameCounterAt()) & 0x20u) ? kStatusText0 : kStatusText1;
        else status = kStatusText0;
    } else if (flags & 0x20u) {
        status = kStatusText1;
    }
    if (status != 0) g.tiny_font(x + 0x2E, y + 0x17, colour != 0 ? colour : 1u, 0x10, At(status));
    const U hp = Word(record + 0x18);
    if (hp < (Word(record + 0x20) >> 2)) SetWord(record + 0x10, Word(record + 0x10) | 0x2000u);
    else SetWord(record + 0x10, Word(record + 0x10) & 0xDFFFu);
    if (!lit) {
        if (Word(record + 0x10) & 0x2000u) colour = 4;
        if (hp <= 1) colour = 2;
    } else {
        colour = 7;
    }
    g.sprintf(buffer, reinterpret_cast<const char*>(At(kFormatLeft)), hp);
    g.font8(x + 0x16, y + 0x1F, static_cast<int>(colour), text);
    colour = lit ? 7u : (record[0x1E] != 0 ? 4u : 0u);
    g.sprintf(buffer, reinterpret_cast<const char*>(At(kFormatRight)), Word(record + 0x20));
    g.font8(x + 0x16, y + 0x1F, static_cast<int>(colour), text);
    colour = bl;
    if (!lit) {
        const U ap = Word(record + 0x1A);
        if (ap < (Word(record + 0x22) >> 2)) colour = 4;
        if (ap == 0) colour = 2;
    } else {
        colour = 7;
    }
    g.sprintf(buffer, reinterpret_cast<const char*>(At(kFormatLeft)), Word(record + 0x1A));
    g.font8(x + 0x16, y + 0x27, static_cast<int>(colour), text);
    g.sprintf(buffer, reinterpret_cast<const char*>(At(kFormatRight)), Word(record + 0x22));
    g.font8(x + 0x16, y + 0x27, static_cast<int>(bl), text);
    const U eax = g.pieces(x, y, At(kMemberPieces), 0);
    const U exp = Dword(record + 0xC);
    const U level = (eax & 0xFFFFFF00u) | record[0xA];
    return g.exp_bar(x + 0x14, y + 0x12, member, level, exp);
}

// original 0x5744B0 (PSX 0x801DA0D4, gap pair): one 8 x 8 cell - a SPRT_8 at
// Gfx_PacketNext through Gpu_SetSprt8, at (x, y) (their low words, unsigned,
// as floats), u = u * 8 and v = v * 8 (bytes), CLUT word `clut`, colour the
// byte `shade` in all three channels; Gpu_SetSemiTrans(0) and
// Gfx_CommitPrim(1, 0x18), whose eax is returned. 8 E8 callers (0x461DB1,
// 0x461DD7, 0x461DFD, 0x57437E, 0x5750B7, 0x599C17 among them).
extern "C" unsigned long __cdecl Menu_DrawCell8(unsigned x, unsigned y, unsigned u, unsigned v, unsigned clut,
                                                unsigned shade) {
    unsigned char* const prim = PacketNext();
    g.set_sprt8(prim);
    SetFloat(prim + 8, static_cast<float>(static_cast<std::int32_t>(x & 0xFFFFu)));
    prim[0x15] = static_cast<unsigned char>(v << 3);
    SetFloat(prim + 0xC, static_cast<float>(static_cast<std::int32_t>(y & 0xFFFFu)));
    prim[0x14] = static_cast<unsigned char>(u << 3);
    SetWord(prim + 0x16, clut);
    prim[6] = prim[5] = prim[4] = static_cast<unsigned char>(shade);
    g.set_semi(prim, 0);
    return g.commit(1, 0x18);
}

// original 0x5746C0: the play-time box - Menu_DrawBox(x + 3, y + 3, 0x55, 0x10,
// 0, the window colour); the hours and minutes (0x9040C8, 0x9040C9) "%02d" in
// the 12 px font at (x + 0x1C, y + 4) and (x + 0x3C, y + 4); the colon, two
// dots through 0x516E70 at (x + 0x34, y + 5) and (x + 0x34, y + 9), for the
// first 15 of the clock's 30 frames; then Menu_DrawPieces of 0x6633CC at (x, y),
// 0x6633B4 six times, and 0x6633C0 at x - 0x10, whose eax is returned.
// As the original has it: the six middle pieces' x is x + 0x18 + 8 * i with i
// in a register whose upper half is the last Menu_DrawPieces' eax (xor ax, ax;
// mov al, i) - ours takes the callee's eax as the original does, so i * 8 is
// off by eax's high word << 19, which Menu_DrawPieces' 16-bit piece draws do
// not see.
extern "C" unsigned long __cdecl Menu_DrawPlayTime(int x, int y) {
    g.draw_box(x + 3, y + 3, 0x55, 0x10, 0, static_cast<int>(Byte(kWindowColour)));
    char* const buffer = reinterpret_cast<char*>(At(kPrintBuf));
    const auto* const text = reinterpret_cast<const unsigned char*>(buffer);
    g.sprintf(buffer, reinterpret_cast<const char*>(At(kFormat02d)), Byte(kClockHours));
    g.font12(x + 0x1C, y + 4, 0, text);
    g.sprintf(buffer, reinterpret_cast<const char*>(At(kFormat02d)), Byte(kClockMinutes));
    g.font12(x + 0x3C, y + 4, 0, text);
    if (Byte(kClockFrames) < 0xF) {
        g.tiny_font(x + 0x34, y + 5, 0, 0xFF, At(kColonText));
        g.tiny_font(x + 0x34, y + 9, 0, 0xFF, At(kColonText));
    }
    U eax = g.pieces(x, y, At(kClockPieces), 0);
    for (U i = 0; i < 6; ++i) {
        const U index = (eax & 0xFFFF0000u) | i;
        eax = g.pieces(static_cast<int>(static_cast<U>(x) + index * 8u + 0x18u), y, At(kClockMiddle), 0);
    }
    return g.pieces(x - 0x10, y, At(kClockLeft), 0);
}

// --- the event script ------------------------------------------------------------------------

// original 0x579CF0 (PSX 0x801A584C): `at` is ON the F4 of a switch being
// skipped; from two on, to past its F5 - an op below F0 by
// EventScript_OpLengths[op >> 4], F6 two bytes, F7 and F8 one, F0 F1 F4 F9 FA
// through EventScript_SkipControl (whose pointer is taken as the new
// position), F5 the end, returned one past.
// As the original has it: F2, F3 and FB..FF match no case of its jump table
// and the loop reads the same byte for ever (read, not run: the fuzz keeps to
// the others); byte +1 is loaded and never used.
extern "C" const unsigned char* __cdecl EventScript_SkipSwitch(const unsigned char* at) {
    const volatile unsigned char* p = at + 2;
    for (;;) {
        const unsigned char c = *p;
        if (c < 0xF0) {
            p += Byte(at::OpLengthsAt() + (c >> 4));
            continue;
        }
        switch (c) {
        case 0xF0:
        case 0xF1:
        case 0xF4:
        case 0xF9:
        case 0xFA: p = g.skip_control(const_cast<const unsigned char*>(p)); break;
        case 0xF5: return const_cast<const unsigned char*>(p) + 1;
        case 0xF6: p += 2; break;
        case 0xF7:
        case 0xF8: p += 1; break;
        default: break;   // F2, F3, FB..FF: the same byte again
        }
    }
}

// original 0x579F00 (PSX 0x801A5BC8): AreaMap_Bytes[s16 z * width + s16 x] =
// value (the width the area block's byte 0). eax on the way out is the row
// offset with the value in its low byte (imul; mov al), returned whole.
extern "C" unsigned long __cdecl AreaMap_SetByte(unsigned long x, unsigned long z, unsigned value) {
    const U row = (Dword(At(kHeader)) & 0xFFu) * S16(static_cast<U>(z));
    At(S16(static_cast<U>(x)) + Dword(At(at::BytesAt())) + row)[0] = static_cast<unsigned char>(value);
    return (row & 0xFFFFFF00u) | (value & 0xFFu);
}

// original 0x57A3A0 (PSX 0x801A6408): event op 8x (12 bytes) - an object
// placed. While the next object index (s16 kScratch +0) is below 0x1E: the
// object 0x7DEE80 + 0xA4 n becomes Sprite_Current and Field_ActiveMember; the
// animation bank (op[1] << 8 | op[2]) to the scratch word +2, read back after
// EventObj_Reset for Sprite_SetAnimationBank; byte +0 = 1; its position x =
// op[5] << 16 (+ 0x8000 when op[4]), z = op[7] << 16 (+ 0x8000 when op[6]), each
// copied to the object n's +0x8C / +0x90 (n read again each time), +0x94 = 0;
// +0x3E the AreaMap_Elevation there; +1 op[8], +2 op[10], +8 op[0] & 0xF, +6
// op[0] >> 4; n's +0x88 = 0xFFFF, +0x84 = op[9], +0xA0 = 0x7F; EventObj_SetFlags(op
// + 11); +0 |= 0x20 when op[3] bit 7; +0x5D, +0x5F, +0x5E = 0; +0x5C = op[3] &
// 0xF; +0x2A = bit 4 of +7; Sprite_SetAnimation(+8); then 0x10 stamped into
// AreaMap_Bytes at the object's cell (words +0x36, +0x3A), and the cells beside
// it that its half-cell position overlaps (+1 in x when word +0x34, in z when
// +0x38, both when both); the index + 1. Sprite_Current is read again for
// every access, as the original reads [0x937F88].
extern "C" void __cdecl EventOp_8x(const unsigned char* op) {
    const U first = Word(At(kScratchX));
    if (static_cast<std::int16_t>(first) >= 0x1E) return;
    const U object = kObjects + S16(first) * kObjectStride;
    SetDword(At(kSpriteCurrent), object);
    SetDword(At(kActiveMember), object);
    SetWord(At(kScratchBank), (static_cast<U>(op[1]) << 8) | op[2]);
    g.obj_reset();
    g.set_bank(Word(At(kScratchBank)));
    Sprite()[0] = 1;
    SetDword(Sprite() + 0x34, (static_cast<U>(op[5]) << 16) | (op[4] != 0 ? 0x8000u : 0u));
    SetDword(At(kObjX8C + S16(Word(At(kScratchX))) * kObjectStride), Dword(Sprite() + 0x34));
    SetDword(Sprite() + 0x38, (static_cast<U>(op[7]) << 16) | (op[6] != 0 ? 0x8000u : 0u));
    {
        unsigned char* const s = Sprite();
        const U n = S16(Word(At(kScratchX))) * kObjectStride;
        SetDword(At(kObjZ90 + n), Dword(s + 0x38));
        SetDword(At(kObj94 + n), 0);
        const long elevation = g.elevation(static_cast<long>(Dword(s + 0x34)), static_cast<long>(Dword(s + 0x38)));
        SetWord(Sprite() + 0x3E, static_cast<U>(elevation));
    }
    Sprite()[1] = op[8];
    Sprite()[2] = op[0xA];
    Sprite()[8] = static_cast<unsigned char>(op[0] & 0xF);
    Sprite()[6] = static_cast<unsigned char>(op[0] >> 4);
    {
        const U n = S16(Word(At(kScratchX))) * kObjectStride;
        SetWord(At(kObjWord88 + n), 0xFFFF);
        At(kObjFlag84 + n)[0] = op[9];
        At(kObjByteA0 + n)[0] = 0x7F;
    }
    g.set_flags(op + 0xB);
    if (op[3] & 0x80) Sprite()[0] = static_cast<unsigned char>(Sprite()[0] | 0x20);
    Sprite()[0x5D] = 0;
    Sprite()[0x5F] = 0;
    Sprite()[0x5E] = 0;
    Sprite()[0x5C] = static_cast<unsigned char>(op[3] & 0xF);
    {
        unsigned char* const s = Sprite();
        s[0x2A] = static_cast<unsigned char>((s[7] >> 4) & 1);
    }
    g.set_animation(Sprite()[8]);
    {
        unsigned char* const s = Sprite();
        g.set_byte(Word(s + 0x36), Word(s + 0x3A), 0x10);
    }
    unsigned char* s = Sprite();
    if (Word(s + 0x34) != 0) {
        g.set_byte((Word(s + 0x36) + 1u) & 0xFFFFu, Word(s + 0x3A), 0x10);
        s = Sprite();
    }
    if (Word(s + 0x38) != 0) {
        g.set_byte(Word(s + 0x36), (Word(s + 0x3A) + 1u) & 0xFFFFu, 0x10);
        s = Sprite();
    }
    if (Word(s + 0x34) != 0 && Word(s + 0x38) != 0)
        g.set_byte((Word(s + 0x36) + 1u) & 0xFFFFu, (Word(s + 0x3A) + 1u) & 0xFFFFu, 0x10);
    SetWord(At(kScratchX), Word(At(kScratchX)) + 1u);
}

// original 0x57C7A0 (PSX 0x8015CA48): Field_StatusBits &= ~0x40 and
// Field_ScriptFlags &= ~0x100 (the blocking flags, docs/menu-screens.md). The
// byte is read first and stored last. eax on the way out is the caller's own
// with the new byte in al - two callers tail-jump here (0x417D49, 0x543A5D), so
// the entry passes eax to the body and the body returns it so.
extern "C" unsigned long __cdecl MapFieldObjects_Clear40Body(unsigned long eax) {
    const U bits = Byte(at::StatusBitsAt());
    SetWord(At(at::ScriptFlagsAt()), Word(At(at::ScriptFlagsAt())) & 0xFEFFu);
    const auto cleared = static_cast<unsigned char>(bits & 0xBFu);
    At(at::StatusBitsAt())[0] = cleared;
    return (static_cast<U>(eax) & 0xFFFFFF00u) | cleared;
}
extern "C" __attribute__((naked)) void __cdecl ScriptFlags_Clear40(void) {
    asm("pushl %eax\n\t"
        "call _MapFieldObjects_Clear40Body\n\t"
        "addl $4, %esp\n\t"
        "ret");
}

void MapFieldObjects_Inject() {
    if (bof3::WantsShadow("map_field_objects")) map_field_objects::SelfTest();
    BOF3_INJECT(MapCell_DrawUprights);
    BOF3_INJECT(MapCell_DrawDiagonalWall);
    BOF3_INJECT(MapCell_DrawFlatFaces);
    BOF3_INJECT(MapCell_PatchThenStep);
    BOF3_INJECT(MapCell_PatchThenStop);
    BOF3_INJECT(MapCell_DrawAnimated);
    BOF3_INJECT(AreaMap_SetupTint);
    BOF3_INJECT(AreaMap_SetupFlatColours);
    BOF3_INJECT(AreaMap_SetupTexture);
    BOF3_INJECT(Menu_DrawMemberStatus);
    BOF3_INJECT(Menu_DrawCell8);
    BOF3_INJECT(Menu_DrawPlayTime);
    BOF3_INJECT(EventScript_SkipSwitch);
    BOF3_INJECT(AreaMap_SetByte);
    BOF3_INJECT(EventOp_8x);
    BOF3_INJECT(ScriptFlags_Clear40);
}
