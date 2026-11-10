// Moved here by tools/move_funcs.py from draw_pass.cpp, field_frame.cpp, sprite_draw.cpp, sprite_order.cpp (docs/source-layout-round.md).
#include "game/field/sprite_2.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <type_traits>

#include "bof3/symbols.gen.h"
#include "game/draw_pass.h"
#include "game/draw_pool.h"
#include "game/field_frame.h"
#include "game/layering.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/sprite_draw.h"
#include "game/sprite_order.h"
#include "game/wide_staging.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"

namespace {

using namespace move_script;

}  // namespace

namespace {

std::uint16_t Key(const unsigned char* object) {
    std::uint16_t key;
    std::memcpy(&key, object + 0x32, sizeof key);
    return key;
}

std::int32_t Long(const unsigned char* object, unsigned offset) {
    std::int32_t v;
    std::memcpy(&v, object + offset, sizeof v);
    return v;
}

// A layer's 0x30 bytes are three (first, last) pairs per buffer; the pass
// appends the third, which Gfx_FrameNodes names.
unsigned long* FrameNode(unsigned layer) { return reinterpret_cast<unsigned long*>(&Gfx_FrameNodes) + (Gfx_BufferIndex + layer * 6u) * 2u; }

// Pass 1 of Sprite_DrawPass, below.
void SortDrawList() {
    unsigned char count = Sprite_DrawListCount;
    if (static_cast<int>(count) - 1 > 0) {
        unsigned char i = 0;
        do {
            const int lowest = i + 1;
            for (unsigned char j = static_cast<unsigned char>(count - 1); static_cast<int>(j) >= lowest; --j) {
                const unsigned char* const a = Sprite_DrawList[j - 1];
                const unsigned char* const b = Sprite_DrawList[j];
                bool exchange = false;
                if (Key(b) < Key(a)) {
                    exchange = true;
                } else if (Key(b) == Key(a)) {
                    const std::int32_t sum_a = static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(a, 0x38)) +
                                                                         static_cast<std::uint32_t>(Long(a, 0x34)));
                    const std::int32_t sum_b = static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(b, 0x38)) +
                                                                         static_cast<std::uint32_t>(Long(b, 0x34)));
                    exchange = sum_b < sum_a || (sum_b == sum_a && Long(b, 0x3C) < Long(a, 0x3C));
                }
                if (exchange) Sprite_DrawListSwap(j, static_cast<unsigned char>(j - 1));
            }
            count = Sprite_DrawListCount;
            ++i;
        } while (static_cast<int>(i) < static_cast<int>(count) - 1);
    }
}

// DIVERGENCE DIV-0041 (wide-ring, docs/wide-ring.md): the grown ring's two
// rows the original never draws - layer -1 before Capcom's first and Capcom's
// row 55 (layer 0x37) after its last - terrain and frame-node primitives only.
// No sprite is drawn at either (docs/wide-ring-read.md 3, owner's answer 1):
// Sprite_UpdateScreen keeps hiding a sprite outside 0..0x36, and a type 0x0A
// sprite Sprite_ProjectA leaves on the list at 0x37 stays undrawn, as on the
// original.
void LinkTableItem(unsigned index) {
    const auto at = Low32(draw_pool::Items() + (Gfx_BufferIndex + index * 2u) * 0x48u);
    Gpu_LinkPrim(Gfx_OtPointers[Draw_OtSlot], at);
    Gfx_OtPointers[Draw_OtSlot] = reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(at));
}

void GrownLayerAbove() {
    if (!(Draw_PassFlags & 4)) return;
    DrawLayer_Open(-1);
    for (unsigned i = 0; i < map_rim::ExtraTableCount(); ++i) LinkTableItem(map_rim::ExtraTableItem(i));
    DrawLayer_Close(-1);
}

void GrownLayerBelow(unsigned char item) {
    constexpr unsigned kLayer = 0x37;
    if (Draw_PassFlags & 4) DrawLayer_Open(static_cast<int>(kLayer));
    if (Draw_PassFlags & 0x10) {
        const unsigned long first = FrameNode(kLayer)[0];
        if (first != 0) {
            if (draw_order::g_on) draw_order::TagList(first, FrameNode(kLayer)[1], draw_order::kFrameNode, kLayer, Draw_OtSlot);
            Gpu_LinkPrim(Gfx_OtPointers[Draw_OtSlot], first);
            Gfx_OtPointers[Draw_OtSlot] = reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(FrameNode(kLayer)[1]));
        }
    }
    for (; item < DrawTable_Count && (DrawTable[item] >> 24) == kLayer; ++item)
        if (Draw_PassFlags & 4) LinkTableItem(DrawTable[item] & 0xFFFu);
    if (Draw_PassFlags & 4) DrawLayer_Close(static_cast<int>(kLayer));
}

}  // namespace
namespace field_frame { unsigned g_band_added; }
namespace {

// Just before the draw pass: each noted sprite still flagged off screen, still
// not a model and not already on the list, appended while the list has room.
void BandAppend() {
    field_frame::g_band_added = 0;
    for (unsigned i = 0; i < field_frame::g_band_count; ++i) {
        field_frame::BandSprite& b = field_frame::g_band[i];
        if (!(b.sprite[0] & 0x80) || (b.sprite[0] & 0x40) || (b.sprite[0x24] & 0x40)) continue;
        const unsigned char n = Sprite_DrawListCount;
        if (n >= 0x28) break;
        bool listed = false;
        for (unsigned k = 0; k < n; ++k) listed = listed || Sprite_DrawList[k] == b.sprite;
        if (listed) continue;
        std::memcpy(b.scale, b.sprite + 0x40, sizeof b.scale);
        Sprite_DrawList[n] = b.sprite;
        Sprite_DrawListCount = static_cast<unsigned char>(n + 1);
        field_frame::g_band[field_frame::g_band_added++] = b;
    }
    field_frame::g_band_count = 0;
}

void BandRestore() {
    for (unsigned i = 0; i < field_frame::g_band_added; ++i) std::memcpy(field_frame::g_band[i].sprite + 0x40, field_frame::g_band[i].scale, sizeof field_frame::g_band[i].scale);
    field_frame::g_band_added = 0;
}

// `fild dword` then `fstp dword` of a sign-extended s16: exact.
std::uint32_t FloatBits(std::int16_t v) {
    const float f = static_cast<float>(v);
    std::uint32_t bits;
    std::memcpy(&bits, &f, sizeof bits);
    return bits;
}

template <class T> T Get(const unsigned char* p, unsigned offset) {
    // A pointer-typed field is a 32-bit word of the image on every width.
    if constexpr (std::is_pointer_v<T>) {
        std::uint32_t w;
        std::memcpy(&w, p + offset, sizeof w);
        return reinterpret_cast<T>(static_cast<std::uintptr_t>(w));
    } else {
        T v;
        std::memcpy(&v, p + offset, sizeof v);
        return v;
    }
}

template <class T> void Put(unsigned char* p, unsigned offset, T v) {
    if constexpr (std::is_pointer_v<T>) {   // the field is a 32-bit word on every width
        const std::uint32_t w = Low32(v);
        std::memcpy(p + offset, &w, sizeof w);
    } else {
        std::memcpy(p + offset, &v, sizeof v);
    }
}

constexpr unsigned kPrimBytes = 0x20, kCellBytes = 8, kPartBytes = 5;

void Draw();

// The cull, and the off-screen flag it leaves in bit 7 of byte 0: true when
// the object is to be drawn. The original has one bound for both, x in
// [-0x40, 0x180] and y in [-0x40, 0x130]. Ours moves each x end out by its
// own number of columns: `draw` for the picture, `flag` for the bit.
//
// DIVERGENCE DIV-0041 (the off-screen flag split, ultrawide round, group UC2,
// docs/ultrawide-culls-fix.md section 1): the bit is game logic - nine
// functions read it to decide what an actor or effect does next
// (Member_FollowStep, Member_CatchUp, Scena08_Scene4, BossNina_WalkStep,
// EffectKind32_Arc / _Settle, EffectKind34_V4Move, Area65_Record8Move,
// Fish_Swim; docs/ultrawide-culls.md section 2) - so it keeps Capcom's bound
// at every width (Sprite_Draw passes flag 0), and only the picture's cull
// moves by the columns. From 2026-09-30 until this split the bit moved with
// the picture under BOF3X_WIDE=1. Narrow, both are 0 and this is the
// original's test. The self-test's control passes flag = draw to show the fuzz
// can see the difference.
bool Cull(unsigned char* object, short x, short y, int draw, int flag) {
    const bool y_out = y > 0x130 || y < -0x40;
    if (y_out || x > 0x180 + flag || x < -0x40 - flag) object[0] |= 0x80;
    else object[0] &= 0x7F;
    return !(y_out || x > 0x180 + draw || x < -0x40 - draw);
}

void Draw() {
    unsigned char* object = Sprite_Current;
    if (object[0] & 0x40) return;
    if (!(object[0x24] & 0x80) && object[0x48] == 1) {
        const long depth = Get<long>(object, 0x60);
        if (depth == 0) {
            Put<long>(object, 0x40, 0);
            Put<long>(object, 0x44, 0);
            return;
        }
        const long scale = (0x4650000L / depth) & ~0xFFL;
        Put<long>(object, 0x40, scale);
        Put<long>(object, 0x44, scale);
    }
    const short x = Get<short>(object, 0x2E), y = Get<short>(object, 0x30);
    const int draw = sprite_draw::g_draw_columns >= 0 ? sprite_draw::g_draw_columns : static_cast<int>(Widescreen_Fill());
    if (!Cull(object, x, y, draw, sprite_draw::g_flag_columns >= 0 ? sprite_draw::g_flag_columns : 0)) return;
    if (g_sprite_capture != 0 && (object[0x24] & 8)) {   // DIV-0142: the PSX's draw into VRAM, before any screen test
        sprite_draw::Capture(object);
        return;
    }
    if (wide_staging::Hide(object)) return;   // DIV-0124: 21:9, past the 16:9 view in a scene - draw-only, after bit 7
    const unsigned char* frame = Get<const unsigned char*>(object, 0x54) + Get<std::uint16_t>(object, 0x5A);
    const unsigned count = frame[0];
    if (count == 0) {
        if (sprite_draw::g_capture_logged < 64 && ++sprite_draw::g_capture_logged)
            bof3::Log("DIV-0142    capture %u: object at (%d, %d), a frame of no pieces (anim %u, frame +0x%X)", sprite_draw::g_capture_logged, x, y,
                      (unsigned)object[8], (unsigned)Get<std::uint16_t>(object, 0x5A));
        return;
    }

    unsigned char* const prim = Gfx_PacketNext;
    Gpu_SetCode84(prim);
    Sprite_ClutWord(reinterpret_cast<unsigned short*>(prim + 0x1C));
    float scale_x = 1.0f, scale_y = 1.0f;
    if (object[0x48] != 0) {
        scale_x = static_cast<float>(static_cast<double>(Get<long>(object, 0x40)) * (1.0 / 65536));
        scale_y = static_cast<float>(static_cast<double>(Get<long>(object, 0x44)) * (1.0 / 65536));
    }
    const unsigned wide = object[0x28] != 0 ? 1u : 0u;
    const unsigned char* piece = frame + 1;
    for (unsigned i = 0; i < count; ++i, piece += kPartBytes) {
        const unsigned size = (piece[0] & 0xFu) * 2;
        const unsigned flags = ((((piece[0] & 0x80u) | (wide << 5)) << 2) | object[0x25]);
        const unsigned long index = SpriteCell_Add(
            static_cast<unsigned>(static_cast<signed char>(piece[1])),
            static_cast<unsigned>(static_cast<signed char>(piece[2])), flags, piece[3],
            static_cast<unsigned>(object[0x26]) + piece[4], SpriteCell_Sizes[size], SpriteCell_Sizes[size + 1]);
        if (i == 0) Put<std::uint16_t>(prim, 0x18, static_cast<std::uint16_t>(index));
    }
    Put<std::uint16_t>(prim, 0x1A, static_cast<std::uint16_t>(count));
    unsigned mode = ((object[0x5C] | (wide << 2)) << 5) | object[0x25];
    if (object[0x2A] != 0) mode |= 0x400;
    Put<std::uint16_t>(prim, 0x1E, static_cast<std::uint16_t>(mode));
    Put<std::uint32_t>(prim, 8, Get<std::uint32_t>(object, 0x74));
    Put<std::uint32_t>(prim, 0xC, Get<std::uint32_t>(object, 0x78));
    Put<float>(prim, 0x10, scale_x);
    Put<float>(prim, 0x14, scale_y);
    prim[4] = static_cast<unsigned char>(object[0x5D] + 0x80);
    prim[5] = static_cast<unsigned char>(object[0x5E] + 0x80);
    prim[6] = static_cast<unsigned char>(object[0x5F] + 0x80);
    Gpu_SetSemiTrans(prim, (object[0] >> 5) & 1u);
    Gfx_CommitPrim(object[0x29], kPrimBytes);
}

struct {
    unsigned calls, drawn, cells, mismatches;
} g_counts;

struct LiveState {
    unsigned char object[0x80];
    unsigned char prim[kPrimBytes];
    unsigned char* packet_next;
    unsigned long count;
    unsigned char cells[256 * kCellBytes];
    unsigned long slot_word, tail_word;
};

struct LivePlace {
    unsigned char* object;
    unsigned char* prim;
    unsigned char* cells;
    unsigned long* slot;
    unsigned long* tail;
};

void LiveCapture(LiveState& s, const LivePlace& at) {
    std::memcpy(s.object, at.object, sizeof s.object);
    std::memcpy(s.prim, at.prim, sizeof s.prim);
    s.packet_next = Gfx_PacketNext;
    s.count = SpriteCell_Count;
    std::memcpy(s.cells, at.cells, sizeof s.cells);
    s.slot_word = *at.slot;
    s.tail_word = at.tail ? *at.tail : 0;
}

void LiveApply(const LiveState& s, const LivePlace& at) {
    std::memcpy(at.object, s.object, sizeof s.object);
    std::memcpy(at.prim, s.prim, sizeof s.prim);
    Gfx_PacketNext = s.packet_next;
    SpriteCell_Count = s.count;
    std::memcpy(at.cells, s.cells, sizeof s.cells);
    *at.slot = s.slot_word;
    if (at.tail) *at.tail = s.tail_word;
}

void LiveDraw() {
    static LiveState input, theirs, ours;
    unsigned char* object = Sprite_Current;
    if (g_sprite_capture != 0 && (object[0x24] & 8)) {   // DIV-0142's capture is not Capcom's: not compared
        Draw();
        return;
    }
    LivePlace at;
    at.object = object;
    at.prim = Gfx_PacketNext;
    at.cells = SpriteCell_Table + SpriteCell_Count * kCellBytes;
    at.slot = reinterpret_cast<unsigned long*>(&Gfx_OtPointers[object[0x29]]);
    at.tail = *at.slot ? reinterpret_cast<unsigned long*>(*at.slot) : nullptr;
    LiveCapture(input, at);
    sprite_draw::g_draw_clone();
    LiveCapture(theirs, at);
    LiveApply(input, at);
    Draw();
    LiveCapture(ours, at);
    ++g_counts.calls;
    if (ours.count != input.count) {
        ++g_counts.drawn;
        g_counts.cells += ours.count - input.count;
    }
    if (std::memcmp(&theirs, &ours, sizeof ours) != 0 && ++g_counts.mismatches <= 8) {
        const auto* a = reinterpret_cast<const unsigned char*>(&theirs);
        const auto* b = reinterpret_cast<const unsigned char*>(&ours);
        unsigned first = 0;
        while (a[first] == b[first]) ++first;
        bof3::Log("shadow      Sprite_Draw live MISMATCH: call %u, first difference at state +0x%X (%02X / %02X)",
                  g_counts.calls, first, a[first], b[first]);
    }
    if (g_counts.calls == 1 || g_counts.calls % 1024 == 0)
        bof3::Log("shadow      Sprite_Draw live: %u calls, %u drawn with %u cells, %u MISMATCHES", g_counts.calls,
                  g_counts.drawn, g_counts.cells, g_counts.mismatches);
}

constexpr unsigned kMaxCount = 0x200;                                  // SpriteCell_Count at most, going in

constexpr unsigned kCellWindow = (kMaxCount + 255) * kCellBytes;       // what one call can reach

constexpr unsigned kFrameBytes = 0x100 + 1 + 255 * kPartBytes;

constexpr unsigned kWindow = 0x40;

}  // namespace

namespace sprite_draw {

struct State {
    alignas(4) unsigned char object[0x80];
    alignas(4) unsigned char frame[kFrameBytes];
    unsigned char cells[kCellWindow];
    unsigned long cell_count;
    unsigned char* packet_next;
    unsigned long* ot[8];
    unsigned long tails[8];
    unsigned char buffer_index;
    unsigned char* current;
    unsigned char window[kWindow];   // the pool, from packet_next as it was on the way in
};

}  // namespace sprite_draw

namespace {

void Capture(sprite_draw::State& s, const unsigned char* window_at) {
    std::memcpy(s.object, sprite_draw::g_live.object, sizeof s.object);
    std::memcpy(s.frame, sprite_draw::g_live.frame, sizeof s.frame);
    std::memcpy(s.cells, SpriteCell_Table, kCellWindow);
    s.cell_count = SpriteCell_Count;
    s.packet_next = Gfx_PacketNext;
    std::memcpy(s.ot, Gfx_OtPointers, sizeof s.ot);
    std::memcpy(s.tails, sprite_draw::g_tails, sizeof s.tails);
    s.buffer_index = Gfx_BufferIndex;
    s.current = Sprite_Current;
    if (window_at) std::memcpy(s.window, window_at, kWindow);
}

}  // namespace

// original 0x592F00: the draw at the end of the field's frame (PSX
// FUN_8014B948), and both sprite lists emptied for the next.
extern "C" void __cdecl Field_DrawFrame(void) {
    field_frame::g.header_pass();
    field_frame::g.overlays();
    BandAppend();    // DIV-0041 (UC2): the band's type-0x0A sprites, in the slots left
    field_frame::g.draw_pass();
    BandRestore();
    Sprite_OverlayListCount = 0;
    Sprite_DrawListCount = 0;
    Widescreen_SweepClear();   // DIV-0041 (I38): the sweep marks go with the overlay list
}

// original 0x592F20: the overlay list, sorted by height (+0x3E, s16) and
// drawn, when Draw_PassFlags has bit 0.
//
// As the original has it: a bubble sort, the smallest height rising to the
// front, stable (only a strictly smaller neighbour moves); the list's count
// re-read after every exchange and every draw, so a callee that changes it
// changes the bounds; the screen floats +0x74 / +0x78 rewritten from the
// integer position +0x2E / +0x30 before each draw.
extern "C" void __cdecl Sprite_DrawOverlays(void) {
    if (!(Draw_PassFlags & 1)) return;
    unsigned char count = Sprite_OverlayListCount;
    if (static_cast<int>(count) - 1 > 0) {
        unsigned char i = 0;
        do {
            for (unsigned char j = static_cast<unsigned char>(count - 1); j >= i + 1u; --j) {
                const auto here = static_cast<std::int16_t>(Word(Sprite_OverlayList[j] + 0x3E));
                const auto before = static_cast<std::int16_t>(Word(Sprite_OverlayList[j - 1] + 0x3E));
                if (here < before) {
                    field_frame::g.swap(j, static_cast<unsigned char>(j - 1));
                    count = Sprite_OverlayListCount;
                }
            }
            ++i;
        } while (static_cast<int>(i) < static_cast<int>(count) - 1);
    }
    if (count == 0) return;
    unsigned char i = 0;
    do {
        unsigned char* const sprite = Sprite_OverlayList[i];
        Sprite_Current = sprite;
        SetLong(sprite + 0x74, static_cast<std::int32_t>(FloatBits(static_cast<std::int16_t>(Word(sprite + 0x2E)))));
        rv::Current<records::SpriteObject>()->screen_yf = static_cast<std::int32_t>(FloatBits(static_cast<std::int16_t>(static_cast<std::uint16_t>(rv::Current<records::SpriteObject>()->screen_y))));
        Widescreen_SweepBefore(sprite);   // DIV-0041 (I38): a marked sweep drawn at its mapped x
        field_frame::g.sprite_draw();
        Widescreen_SweepAfter(sprite);    // ... and Capcom's +0x74 put back
        ++i;
    } while (i < Sprite_OverlayListCount);
}

// original 0x593020: exchanges two overlay list entries. The caller pushes
// whole registers with stale upper bytes; the original masks them, so do we.
extern "C" void __cdecl Sprite_SwapOverlays(unsigned long a, unsigned long b) {
    unsigned char* const first = Sprite_OverlayList[a & 0xFF];
    unsigned char* const second = Sprite_OverlayList[b & 0xFF];
    Sprite_OverlayList[b & 0xFF] = first;
    Sprite_OverlayList[a & 0xFF] = second;
}

namespace {
// The three callees that are not ours, through pointers so that the start-up
// fuzz can stand recording functions in for them - for the original's copy
// and for ours alike.
using OpenFn = void (__cdecl*)(int);
using AddRecordsFn = unsigned (__cdecl*)(unsigned long*);
using DrawFn = void (__cdecl*)();
}  // namespace
namespace draw_pass { OpenFn g_open = DrawLayer_Open; }
namespace draw_pass { AddRecordsFn g_add_records = Sprite_AddDrawRecords; }
namespace draw_pass { DrawFn g_draw = Sprite_Draw; }

// original 0x593060: the field's draw-order pass, once a logic frame
// (docs/sprite-draw-order.md section 2).
//
// Pass 1 bubble-sorts Sprite_DrawList ascending on the u16 at +0x32 (layer,
// then order), then the sum of the dwords +0x34 and +0x38, then the dword
// +0x3C; equal neighbours stay put. Pass 2 walks 0x37 layers, merging the
// layer's sprites with the draw table's items, lower key first and the table
// first on a tie; sprites gathered into 12-byte records, sorted descending on
// the s16 at record +4, and drawn or linked.
//
// As the original has it: every index is a byte; the record count is a
// SIGNED byte; a slot byte other than 4 or 6, or a table out of order, never
// returns (by reading - not something a test can show); and globals are read again wherever the original reads them again,
// since three of the callees are free to change them.
extern "C" void __cdecl Sprite_DrawPass(void) {
    SortDrawList();
    // DIVERGENCE DIV-0071 (layering.h), off during every fuzz: a sprite drawn
    // a layer or more later while only floor lies under its feet there.
    if (layering::g_mode == layering::kFloor && layering::Defer()) SortDrawList();
    unsigned char count;

    Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, 0x95, 0);
    Gfx_CommitPrim(6, 0xC);

    unsigned char sprite = 0, item = 0;   // next in the draw list, next in the draw table
    const bool grown = map_rim::Drawn();   // DIV-0041 (wide-ring), above
    if (grown) GrownLayerAbove();
    for (unsigned layer = 0; layer < 0x37; ++layer) {
        if (Draw_PassFlags & 4) draw_pass::g_open(static_cast<int>(layer));
        if (Draw_PassFlags & 0x10) {
            const unsigned long first = FrameNode(layer)[0];
            if (first != 0) {
                if (draw_order::g_on)
                    draw_order::TagList(first, FrameNode(layer)[1], draw_order::kFrameNode, layer, Draw_OtSlot);
                Gpu_LinkPrim(Gfx_OtPointers[Draw_OtSlot], first);
                Gfx_OtPointers[Draw_OtSlot] = reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(FrameNode(layer)[1]));
            }
        }

        for (;;) {
            count = Sprite_DrawListCount;
            const bool sprite_here = sprite < count && (Key(Sprite_DrawList[sprite]) >> 8) == layer;
            if (!sprite_here && ((DrawTable[item] >> 24) != layer || item >= DrawTable_Count)) break;

            bool table_first = sprite == count;
            if (!table_first) {
                const unsigned long entry = DrawTable[item];
                table_first = (entry >> 24) == layer && item < DrawTable_Count &&
                              static_cast<std::uint16_t>(entry >> 16) <= Key(Sprite_DrawList[sprite]);
            }
            if (table_first) {
                if (Draw_PassFlags & 4) {
                    const auto at = [item] {
                        return Low32(draw_pool::Items() + (Gfx_BufferIndex + (DrawTable[item] & 0xFFFu) * 2u) * 0x48u);
                    };
                    if (draw_order::g_on)
                        draw_order::TagOne(at(), draw_order::kTable, layer, Draw_OtSlot, DrawTable[item] >> 16,
                                           DrawTable[item] & 0xFFFu);
                    Gpu_LinkPrim(Gfx_OtPointers[Draw_OtSlot], at());
                    Gfx_OtPointers[Draw_OtSlot] = reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(at()));
                    count = Sprite_DrawListCount;
                }
                ++item;
            }

            // Gather the run of sprites that comes before the table's next item.
            unsigned char records = 0;
            if (sprite < count) {
                const std::uint16_t first_key = Key(Sprite_DrawList[sprite]);
                do {
                    unsigned char* const object = Sprite_DrawList[sprite];
                    const std::uint16_t key = Key(object);
                    if (static_cast<std::uint16_t>(DrawTable[item] >> 16) <= key && item != DrawTable_Count) break;
                    if ((key >> 8) != layer) break;
                    const unsigned char slot = Draw_OtSlot;
                    if (slot != 6 && (slot != 4 || first_key != key)) break;
                    Sprite_Current = object;
                    unsigned long* const record = Sprite_DrawRecords + 3 * static_cast<int>(static_cast<signed char>(records));
                    if (object[0x24] & 0x40) {
                        const unsigned char added = static_cast<unsigned char>(draw_pass::g_add_records(record));
                        records = static_cast<unsigned char>(records + added);
                        // What the model drew, kept for the next pass's Defer (layering.h, issue 141).
                        if (layering::g_mode == layering::kFloor && added != 0) layering::AfterModel(object, record, added);
                    } else {
                        record[0] = Low32(object);
                        record[1] = static_cast<unsigned long>(Long(object, 0x60)) | 0x80000000u;
                        ++records;
                    }
                    ++sprite;
                } while (sprite < Sprite_DrawListCount);
            }

            // Bubble sort, descending on the s16 at +4.
            const int n = static_cast<signed char>(records);
            if (n - 1 > 0) {
                unsigned char round = 0;
                int k = 0;
                do {
                    const int lowest = k + 1;
                    unsigned char j = static_cast<unsigned char>(records - 1);
                    for (int jj = static_cast<signed char>(j); jj >= lowest; jj = static_cast<signed char>(--j)) {
                        unsigned long* const here = Sprite_DrawRecords + 3 * jj;
                        if (static_cast<std::int16_t>(here[1]) > static_cast<std::int16_t>(here[-2]))
                            Sprite_DrawRecordSwap(here, here - 3);
                    }
                    k = static_cast<signed char>(++round);
                } while (k < n - 1);
            }

            if (n <= 0) continue;
            unsigned long* record = Sprite_DrawRecords;
            for (int left = n; left != 0; --left, record += 3) {
                const unsigned char flags = Draw_PassFlags;
                if (record[1] & 0x80000000u) {
                    if (flags & 2) {
                        Sprite_Current = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(record[0]));
                        if (draw_order::g_on)
                            draw_order::Set(draw_order::kSprite, layer, Key(Sprite_Current),
                                            static_cast<unsigned>(record[0]));
                        if (layering::g_mode == layering::kFloor) layering::BeforeSprite();   // what it draws (layering.h)
                        draw_pass::g_draw();
                        if (draw_order::g_on) draw_order::Clear();
                        // What it drew, kept; the table faerie's sides again (layering.h, D300).
                        if (layering::g_mode == layering::kFloor) layering::AfterSprite(Sprite_Current);
                    }
                } else if (flags & 8) {
                    unsigned char* const owner = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(record[2]));
                    const unsigned long prim = record[0];
                    if (draw_order::g_on)
                        draw_order::TagOne(prim, draw_order::kRecord, layer, owner[0x29], static_cast<unsigned>(record[2]), 0);
                    Sprite_Current = owner;
                    Gpu_LinkPrim(Gfx_OtPointers[owner[0x29]], prim);
                    Gfx_OtPointers[Sprite_Current[0x29]] = reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(record[0]));
                }
            }
        }

        if (Draw_PassFlags & 4) DrawLayer_Close(static_cast<int>(layer));
    }
    if (grown) GrownLayerBelow(item);
    if (layering::g_mode == layering::kFloor) layering::Restore();
}

// original 0x593530. Exchanges two entries of the sprite draw list, for the
// bubble sort at the head of 0x593060.
//
// As the original has it: only the low byte of each index counts - the caller
// pushes whole registers of which it set one byte - and nothing holds that
// byte to the list's 40 entries.
extern "C" void __cdecl Sprite_DrawListSwap(unsigned i, unsigned j) {
    i &= 0xFF;
    j &= 0xFF;
    unsigned char* const at_i = Sprite_DrawList[i];
    unsigned char* const at_j = Sprite_DrawList[j];
    Sprite_DrawList[j] = at_i;
    Sprite_DrawList[i] = at_j;
}

// original 0x593570. Exchanges two 12-byte draw records, for the per-layer
// bubble sort in 0x593060 - the hottest unnamed logic function of the attract
// run, 421,058 calls.
//
// As the original has it, in its order: all of b is read; then a is copied to
// b a dword at a time; then what was read goes to a. The game passes
// neighbouring records, which do not overlap; for records that do, this order
// is the result.
extern "C" void __cdecl Sprite_DrawRecordSwap(unsigned long* a, unsigned long* b) {
    const unsigned long b0 = b[0], b1 = b[1], b2 = b[2];
    b[0] = a[0];
    b[1] = a[1];
    b[2] = a[2];
    a[0] = b0;
    a[1] = b1;
    a[2] = b2;
}

extern "C" void __cdecl Sprite_Draw() {
    if (sprite_draw::g_draw_clone) LiveDraw();
    else Draw();
}

// original 0x593860: the current object's CLUT word, to *out. Byte +0x27 is
// its palette and byte +0x28 how the palettes share the CLUT rows: 2^s to a
// row, s = 4, 0, 3, 2 for +0x28 = 0..3 and 1 for anything above - so column
// (palette mod 2^s) * 256 / 2^s texels, row 0x1E0 + palette / 2^s, or 0x1F0
// + with bit 2 of +0x24. (The original is a four-way jump table.)
extern "C" void __cdecl Sprite_ClutWord(unsigned short* out) {
    const unsigned char* object = Sprite_Current;
    static const unsigned char kShift[4] = {4, 0, 3, 2};
    const unsigned row = ((object[0x24] & 4u) | 0x78u) << 2;
    const unsigned shift = object[0x28] < 4 ? kShift[object[0x28]] : 1u;
    const unsigned palette = object[0x27];
    const unsigned column = ((palette & ((1u << shift) - 1)) << (8 - shift)) >> 4;
    *out = static_cast<unsigned short>(((row + (palette >> shift)) << 6) | (column & 0x3F));
}

void Sprite2_Inject() {
    BOF3_INJECT(Field_DrawFrame);
    BOF3_INJECT(Sprite_DrawOverlays);
    BOF3_INJECT(Sprite_SwapOverlays);
    BOF3_INJECT(Sprite_DrawPass);
    BOF3_INJECT(Sprite_DrawListSwap);
    BOF3_INJECT(Sprite_DrawRecordSwap);
    BOF3_INJECT(Sprite_Draw);
    BOF3_INJECT(Sprite_ClutWord);
}
