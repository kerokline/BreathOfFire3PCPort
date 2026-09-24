#include "game/area_backdrop.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_backdrop_callees.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

// The three handlers of AreaMap_EntryHandlers 0x66329C that map_layers.cpp
// left (entries 1, 2 and 3 - entry 0 is AreaMap_ClutCycle there), and the
// world map's sprite pin - see docs/area-backdrop.md for each function, its
// PSX twin and the fuzz. The point of the group is the backdrop: under a wide
// picture (DIV-0041) the sky gradient covers the whole 426 x 240 view.

namespace area_backdrop {

const Callees kOriginals = {
    Gpu_SetDrawMode, Gfx_CommitPrim, Gpu_SetPolyG4, Gpu_SetSemiTrans,
    Area_TestCondition, Gpu_SetDrawMove,
};
Callees g = kOriginals;

}  // namespace area_backdrop

namespace {

using area_backdrop::g;

std::uint32_t Dword(const unsigned char* p) {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
unsigned Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void SetWord(unsigned char* p, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void SetDword(unsigned char* p, std::uint32_t v) { std::memcpy(p, &v, sizeof v); }
void SetFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }
unsigned char* At(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

// A 5-bit colour channel of a packed entry colour, as the PSX's 15-bit
// (r, g, b) unpacked to 8 bits: the original's `and al, 0x1F; shl al, 3`.
unsigned char Channel(std::uint32_t colour, unsigned shift) {
    return static_cast<unsigned char>(((colour >> shift) & 0x1Fu) << 3);
}

// The party's three field objects: ObjTrio 0x802D40, 0x14C bytes each (the
// PSX's 0x140). Read here: the position dwords +0x34 / +0x38, the slot byte
// +0x29 (what Sprite_Draw commits the sprite's primitives to) and bit 0 of
// the flag byte +0x138 (what MapView_CheckHeightScale tests).
constexpr std::uint32_t kTrioStride = 0x14C, kTrioPosition = 0x34, kTrioSlot = 0x29, kTrioFlags = 0x138;
// A fourth object record at 0x905DA0, unnamed in symbols.toml: the one
// GameMode_Field's request 9 sets up and MapView_CheckHeightScale tests bit 0
// of its byte +0xB for, as it tests ObjTrio's +0x138. AreaMap_SlotZones
// reads its position +0x34 / +0x38 and sets or clears that bit; it has no
// slot byte in the zone's sense (the PSX twin does the same for its record
// at 0x8014629C / byte 0x80146273).
constexpr std::uint32_t kFourthObject = 0x905DA0, kFourthFlags = 0xB;

// The rounded cell of a 16.16 position: the high word of (p + 0x8000), signed.
int Cell(const unsigned char* position) {
    return static_cast<short>((Dword(position) + 0x8000u) >> 16);
}

}  // namespace

// original 0x571BE0: AreaMap_EntryHandlers 1 (PSX FUN_80159108) - the area's
// backdrop, one Gouraud quad over the whole view, drawn while the view shows
// cells off the map (MapView_BuildFlags non-zero) and the focus lies within
// the entry's bounds: dword +4 holds four bytes x0, z0, x1, z1 against the
// focus in cells, (0x7FFF - MapView_FocusX) >> 8 and (0x8000 - MapView_FocusZ)
// >> 8. A draw mode of tpage 0x95 with dithering (slot 7, 0xC bytes), then a
// POLY_G4 (slot 7, 0x44) whose top colour is the low word and bottom colour
// the high word of the entry's dword +8 - or +12 when Cond_ByteFF is set -
// each a 15-bit r, g, b unpacked to 8.
//
// DIV-0041: under a wide picture the quad spans the wide view, x from -53 to
// 373 in the game's 320-wide units, so the sky reaches both edges of the
// 426 x 240 picture; otherwise the original's 0..320. The rows are the same.
//
// As the original has it: the four bound tests are signed 32-bit compares of
// a byte against the 24-bit shifted focus; the packet cursor is read again
// after the first commit for the quad; the PC dropped the PSX's video-mode
// test that picks tpage 0x225 over 0x95, and passes 0x95 always.
extern "C" void __cdecl AreaMap_DrawBackdrop(const unsigned char* entry) {
    if (MapView_BuildFlags == 0) return;
    const auto fz = static_cast<std::int32_t>((0x8000u - static_cast<std::uint32_t>(MapView_FocusZ)) >> 8);
    const auto fx = static_cast<std::int32_t>((0x7FFFu - static_cast<std::uint32_t>(MapView_FocusX)) >> 8);
    const std::uint32_t bounds = Dword(entry + 4);
    if (static_cast<std::int32_t>(bounds & 0xFFu) > fx) return;
    if (static_cast<std::int32_t>((bounds >> 8) & 0xFFu) > fz) return;
    if (static_cast<std::int32_t>((bounds >> 16) & 0xFFu) < fx) return;
    if (static_cast<std::int32_t>(bounds >> 24) < fz) return;

    g.draw_mode(Gfx_PacketNext, 0, 1, 0x95, 0);
    g.commit(7, 0xC);
    unsigned char* const quad = Gfx_PacketNext;   // read again: the commit moved it
    g.set_poly_g4(quad);
    g.set_semi(quad, 0);
    // DIV-0041: (-53, 0)..(373, 240) under a wide picture, (0, 0)..(320, 240) otherwise.
    const float wide = static_cast<float>(Widescreen_Live());
    const float left = 0.0f - wide;   // not -wide, which is -0.0f when wide is 0
    const float right = 320.0f + wide;
    SetFloat(quad + 0x8, left);
    SetDword(quad + 0xC, 0);
    SetFloat(quad + 0x18, right);
    SetDword(quad + 0x1C, 0);
    SetFloat(quad + 0x28, left);
    SetFloat(quad + 0x2C, 240.0f);
    SetFloat(quad + 0x38, right);
    SetFloat(quad + 0x3C, 240.0f);
    const std::uint32_t colour = Dword(entry + (Cond_ByteFF != 0 ? 12 : 8));
    for (unsigned shift = 0; shift < 3; ++shift) {
        const unsigned char top = Channel(colour, shift * 5), bottom = Channel(colour, 16 + shift * 5);
        quad[0x4 + shift] = top;
        quad[0x14 + shift] = top;
        quad[0x24 + shift] = bottom;
        quad[0x34 + shift] = bottom;
    }
    g.commit(7, 0x44);
}

// original 0x571D30: AreaMap_EntryHandlers 2 (PSX FUN_801592EC) - a texture
// cycle: a VRAM rectangle copied over another on a frame schedule. Byte 0 of
// the entry plus one is the period; word +4 a condition; then pairs of
// dwords (A, B) from +8 whose A's top byte is a frame threshold, in order.
// With f = Frame_Counter mod period, the first pair whose threshold is at or
// above f is taken: the source rect is x = A bits 16..19 << 7 | B bits 9..15,
// + 0x140, y = B byte 0 + 0x100, w = B's top byte, h = B byte 2 + 1; the
// destination x = A bits 20..23 << 7 | A bits 9..15, + 0x140, y = A byte 0 +
// 0x100. Gpu_SetDrawMove, then Gfx_CommitPrim(6, 0x18).
//
// As the original has it: the condition is pushed with stale bits above the
// word (Area_TestCondition reads 16); the scan stops only at a threshold at
// or above f, so a list without one runs on (read, not run: the fuzz ends
// every list with 0xFF).
extern "C" void __cdecl AreaMap_TextureCycle(const unsigned char* entry) {
    if (g.test(Word(entry + 4)) == 0) return;
    const unsigned frame = Frame_Counter % (entry[0] + 1u);
    const unsigned char* pair = entry + 8;
    while (static_cast<int>(frame) > static_cast<int>(pair[3])) pair += 8;
    const std::uint32_t a = Dword(pair), b = Dword(pair + 4);
    unsigned char rect[8];
    SetWord(rect, (((a >> 16) & 0xFu) << 7) + ((b >> 9) & 0x7Fu) + 0x140u);
    SetWord(rect + 2, (b & 0xFFu) + 0x100u);
    SetWord(rect + 4, pair[7]);
    SetWord(rect + 6, pair[6] + 1u);
    const std::uint32_t x = (((a >> 20) & 0xFu) << 7) + ((a >> 9) & 0x7Fu) + 0x140u;
    const std::uint32_t y = (a & 0xFFu) + 0x100u;
    g.set_draw_move(Gfx_PacketNext, rect, x, y);
    g.commit(6, 0x18);
}

// original 0x571E20: AreaMap_EntryHandlers 3 (PSX FUN_801593FC) - draw-slot
// zones. Nothing while Draw_OtSlot is 4. Else the entry's byte +2 less one
// is a count of zone dwords from +4: x0 = the top byte, z0 = byte 2, a width
// of bits 9..15 and a depth of bits 2..8, and bit 0 the sense. For each of
// the party's three objects whose rounded cell lies in [x0, x0 + width) x
// [z0, z0 + depth): bit 0 set - flag bit 0 set and the slot byte 4; clear -
// flag bit 0 cleared and the slot byte Draw_OtSlot. The fourth object at
// 0x905DA0 gets only its flag bit, by the same test.
//
// As the original has it: the cell is the signed high word of the position
// plus 0x8000, compared as 16 bits against the byte bounds; a count byte of
// 0 or 1 does nothing; Draw_OtSlot is read again for every object.
extern "C" void __cdecl AreaMap_SlotZones(const unsigned char* entry) {
    if (Draw_OtSlot == 4) return;
    int count = static_cast<int>(entry[2]) - 1;
    if (count <= 0) return;
    for (const unsigned char* zone = entry + 4; count > 0; --count, zone += 4) {
        const std::uint32_t z = Dword(zone);
        const int x0 = static_cast<int>(z >> 24), width = static_cast<int>((z >> 9) & 0x7Fu);
        const int z0 = static_cast<int>((z >> 16) & 0xFFu), depth = static_cast<int>((z >> 2) & 0x7Fu);
        const auto inside = [&](const unsigned char* position) {
            const int cx = Cell(position), cz = Cell(position + 4);
            return cx >= x0 && cx < x0 + width && cz >= z0 && cz < z0 + depth;
        };
        for (unsigned k = 0; k < 3; ++k) {
            unsigned char* const object = ObjTrio + k * kTrioStride;
            if (!inside(object + kTrioPosition)) continue;
            if (zone[0] & 1) {
                object[kTrioFlags] |= 1;
                object[kTrioSlot] = 4;
            } else {
                object[kTrioFlags] &= 0xFE;
                object[kTrioSlot] = Draw_OtSlot;
            }
        }
        unsigned char* const fourth = At(kFourthObject);
        if (!inside(fourth + kTrioPosition)) continue;
        if (zone[0] & 1) fourth[kFourthFlags] |= 1;
        else fourth[kFourthFlags] &= 0xFE;
    }
}

// original 0x4112A0 (0x18 bytes, no calls): Sprite_Current's screen point
// +0x2E / +0x30 = (160, 80). Called first thing by 33 small state handlers
// at 0x401F80..0x424E00 - eleven triads, one per world-map region overlay
// compiled into the exe - each of which then adjusts the sprite's scale,
// countdown and state and tail-jumps to Sprite_UpdateScreen. On the
// world-map route (259 calls) the callers are 0x404030 and 0x404080. The
// point is centred on the 320-wide view, so under the wide picture's
// primitive shift it stays centred: nothing to widen.
extern "C" void __cdecl WorldMap_PinSprite(void) {
    SetWord(Sprite_Current + 0x2E, 0xA0);
    SetWord(Sprite_Current + 0x30, 0x50);
}

void AreaBackdrop_Inject() {
    if (bof3::WantsShadow("area_backdrop")) area_backdrop::SelfTest();
    BOF3_INJECT(AreaMap_DrawBackdrop);
    BOF3_INJECT(AreaMap_TextureCycle);
    BOF3_INJECT(AreaMap_SlotZones);
    BOF3_INJECT(WorldMap_PinSprite);
}
