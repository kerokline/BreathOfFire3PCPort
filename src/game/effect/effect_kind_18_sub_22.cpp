// Moved here by tools/move_funcs.py from effect_5d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_22.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U ULEffect5d(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect5d(U a) { return ULEffect5d(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U W(const unsigned char* p) { return Word(p); }

U W(U a) { return Word(At(a)); }

unsigned char& B(U a) { return *At(a); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

std::int32_t S8(unsigned char v) { return static_cast<signed char>(v); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* Vertex() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

void SetV(unsigned offset, U v) { SetWord(Vertex() + offset, v); }

const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertex() + 8 * i); }

float* Fp(unsigned char* p) { return reinterpret_cast<float*>(p); }

using Handler = scenario_harness::Handler;

// A sub-kind's state table: its address and its own length (symbols.toml [[data]]).
#define E5D_TABLE(name) AddressOf(name), name##_count

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original jumps
// through the dword after - the next table or data.
void DispatchEffect5d(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5d.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect5d(table + 4 * state)))();
}

// An index into one of the image's tables by a record word or byte, checked:
// past it the original reads the data after (another table or code pointers).
U Index(const char* who, std::int32_t index, unsigned count, U table) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: index %d, past the %u entries of the table at 0x%X - the original reads the data after "
                    "(docs/effect_5d.md section 7)",
                    who, (int)index, count, (unsigned)table);
    return static_cast<U>(index);
}

// The record's x cell word (+0x36, signed), the index the 0x14 / 0x1E and 0x22
// sub-kinds' tables take.
U Place(const char* who, unsigned count, U table) { return Index(who, S16(W(S() + 0x36)), count, table); }

constexpr char kSub22[] = "EffectKind18Sub22";

U Sub22Place() { return Place(kSub22, at::kSub22Count, at::kSub22Rect.v); }

U Rect(U place, unsigned k) { return B(::bof3::Img(at::kSub22Rect.v) + 4 * place + k); }

// The record's story flag (+0x34's low byte, which sub-state 0 sets) tested.
bool Sub22Flag() { return SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), S()[0x34]) != 0; }

// Sub-states 2 and 4's turn: +9 0, +2 `next` (or up one), +0x3A the step;
// the map written; sounds 0x202 and 0x201.
void Sub22Turn() {
    SH_CALL(EffectKind18Sub22_SetMap)();
    SH_CALL(Sound_PlayEffect)(0x202);
    SH_CALL(Sound_PlayEffect)(0x201);
}

}  // namespace

// original 0x5064A0 (EffectKind18_States[34], hidden in 0x505E60):
// jmp [EffectKind18Sub22_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub22_Run(void) {
    DispatchEffect5d("EffectKind18Sub22_Run", E5D_TABLE(EffectKind18Sub22_States));
}

// original 0x5064C0 (sub-state 0): +0x34's low word the place's story flag
// (0x65E348 by +0x36); set: +0x3E the place's height (0x65E34C) less 0x80 and
// +2 2; clear: +0x3E the height and +2 4. The map written, then the draw (a
// tail jump).
extern "C" void __cdecl EffectKind18Sub22_Start(void) {
    U place = Place(kSub22, at::kSub22Count, at::kSub22Flags.v);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->x_frac, B(::bof3::Img(at::kSub22Flags.v) + place));
    const bool set = Sub22Flag();
    unsigned char* const s = S();
    place = Place(kSub22, at::kSub22Count, at::kSub22Heights.v);
    if (set) {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, W(::bof3::Img(at::kSub22Heights.v) + 2 * place) - 0x80);
        S()[2] = 2;
    } else {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, W(::bof3::Img(at::kSub22Heights.v) + 2 * place));
        S()[2] = 4;
    }
    SH_CALL(EffectKind18Sub22_SetMap)();
    SH_CALL(EffectKind18Sub22_Draw)();
}

// original 0x506540 (sub-state 4): the flag set: +9 0, +2 1, +0x3A -8, the map
// written, sounds 0x202 and 0x201. The draw (a tail jump).
extern "C" void __cdecl EffectKind18Sub22_WaitSet(void) {
    if (Sub22Flag()) {
        S()[9] = 0;
        S()[2] = 1;
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->z_whole, 0xFFF8);
        Sub22Turn();
    }
    SH_CALL(EffectKind18Sub22_Draw)();
}

// original 0x5065A0 (sub-states 1 and 3): +0x3E plus +0x3A (words); +9 up one;
// at 0x10 or past, +2 up and the map written. The draw (a tail jump).
extern "C" void __cdecl EffectKind18Sub22_Move(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole));
    s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] >= 0x10) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        SH_CALL(EffectKind18Sub22_SetMap)();
    }
    SH_CALL(EffectKind18Sub22_Draw)();
}

// original 0x5065E0 (sub-state 2): the flag clear: +9 0 (the al Flags_Test
// answered), +2 up, +0x3A 8, the map written, sounds 0x202 and 0x201. The draw
// (a tail jump).
extern "C" void __cdecl EffectKind18Sub22_WaitClear(void) {
    if (!Sub22Flag()) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->z_whole, 8);
        Sub22Turn();
    }
    SH_CALL(EffectKind18Sub22_Draw)();
}

// original 0x506640: the place's map cells (0x65E354 by +0x36: x from..to, z
// from..to) written - for place 0 their AreaMap_Bytes byte 0x11 (0 at
// sub-state 2), for the others their AreaMap_Corners dword four times a byte of
// the flag's row of 0x65E364 (a running count, eleven a row); then every
// cell's height byte (AreaMap_Header + AreaMap_HeightBase * 4 + the cell) to
// (-ground - 2 +0x3E) / 32. Indexes into the area block unchecked, as the
// original's: the rectangles are the area's own cells.
extern "C" void __cdecl EffectKind18Sub22_SetMap(void) {
    const U flag = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), S()[0x34]) & 0xFFu;
    U count = 0;
    if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->x_whole) == 0) {
        U off = 0;
        U x = B(::bof3::Img(at::kSub22Rect.v));
        if (x < B(::bof3::Img(at::kSub22Rect.v) + 2)) {
            do {
                U z = B(::bof3::Img(at::kSub22Rect.v) + 1 + off);
                if (z < B(::bof3::Img(at::kSub22Rect.v) + 3 + off)) {
                    do {
                        const unsigned char value = S()[2] != 2 ? 0x11 : 0;
                        const U width = AreaMap_Header[0];
                        const_cast<unsigned char*>(AreaMap_Bytes.Native())[width * z + x] = value;
                        ++z;
                    } while (z < Rect(Sub22Place(), 3));
                }
                off = 4 * Sub22Place();
                ++x;
            } while (x < B(::bof3::Img(at::kSub22Rect.v) + 2 + off));
        }
    } else {
        U place = Sub22Place();
        U x = Rect(place, 0);
        if (x < Rect(place, 2)) {
            do {
                U z = Rect(place, 1);
                if (z < Rect(place, 3)) {
                    const U row = ::bof3::Img(at::kSub22Corners.v) + at::kSub22CornerRow * flag;
                    do {
                        if (count >= at::kSub22CornerRow || flag > 1)
                            bof3::Fatal("EffectKind18Sub22_SetMap: corner byte %u of row %u, past the two rows of eleven at "
                                        "0x65E364 - the original reads the data after (docs/effect_5d.md section 7)",
                                        (unsigned)count, (unsigned)flag);
                        const U c = B(row + count);
                        const U width = AreaMap_Header[0];
                        SetUL(At(AddressOf(&AreaMap_Corners) + 4 * (width * z + x)), c * 0x01010101u);
                        ++count;
                        ++z;
                    } while (z < Rect(Sub22Place(), 3));
                }
                place = Sub22Place();
                ++x;
            } while (x < Rect(place, 2));
        }
    }
    U place = Sub22Place();
    U x = Rect(place, 0);
    if (x >= Rect(place, 2)) return;
    do {
        U z = Rect(place, 1);
        if (z < Rect(place, 3)) {
            do {
                const U e = static_cast<U>(SH_CALL(AreaMap_Elevation)(static_cast<long>(x << 16), static_cast<long>(z << 16)));
                unsigned char* const s = S();
                const std::int32_t v = -S16(e) - S16(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole)) * 2;
                const U base = AreaMap_HeightBase;
                const U width = AreaMap_Header[0];
                B(AddressOf(AreaMap_Header) + base * 4 + x + width * z) = static_cast<unsigned char>(v / 32);
                ++z;
            } while (z < Rect(Sub22Place(), 3));
        }
        place = Sub22Place();
        ++x;
    } while (x < Rect(place, 2));
}

// original 0x506860 (sub-states 0..4 tail-jump to it; inside 0x506640's span,
// its own frame and ret): the place's pieces (0x65E38C by +0x36: from..to of
// the 89 records at 0x65E3D0) - each a textured quad whose four corners are
// its shape's (0x65E394, by the piece's byte 1) offsets from the piece's cell
// (bytes 3, 2), height the offset over +0x3E; Gte_RotTransPers4,
// Gte_PrimDepths4_10, the piece's texture dword; linked on the piece's cell
// with the row offset 0x65E37C gives its kind (byte 0) at sub-state 2 or not -
// or, negative, committed to slot 3.
extern "C" void __cdecl EffectKind18Sub22_Draw(void) {
    static const char kWho[] = "EffectKind18Sub22_Draw";
    U place = Place(kWho, at::kSub22Count, at::kSub22Pieces.v);
    U k = B(::bof3::Img(at::kSub22Pieces.v) + 2 * place);
    if (k >= B(::bof3::Img(at::kSub22Pieces.v) + 2 * place + 1)) return;
    do {
        const U piece = ::bof3::Img(at::kSub22Piece.v) + 8 * Index(kWho, I(k), at::kSub22PieceCount, at::kSub22Piece.v);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        U d = ULEffect5d(piece);
        const U x = ((d >> 24) << 7) - 0x3FC0;
        const U z = (((d >> 16) & 0xFF) << 7) - 0x3FC0;
        const U shape = ::bof3::Img(at::kSub22Shapes.v) + 12 * Index(kWho, (d >> 8) & 0xFF, at::kSub22ShapeCount, at::kSub22Shapes.v);
        for (unsigned v = 0; v < 4; ++v) {
            const U o = shape + 3 * v;
            SetV(8 * v, x - (static_cast<U>(S8(B(o))) << 7));
            SetV(8 * v + 2, z - (static_cast<U>(S8(B(o + 1))) << 7));
            SetV(8 * v + 4, (static_cast<U>(S8(B(o + 2))) << 7) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole));
        }
        long depth[2];
        SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x18), Fp(p + 0x28), Fp(p + 0x38), depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Prim_SetTexture)(ULEffect5d(piece + 4), p, 1);
        d = ULEffect5d(piece);
        unsigned char* s = S();
        const U kind = Index(kWho, d & 0xFF, at::kSub22RowKinds, at::kSub22Rows.v);
        const std::int32_t row = S8(B(::bof3::Img(at::kSub22Rows.v) + (s[2] == 2 ? 1 : 0) + 2 * kind));
        if (row < 0)
            SH_CALL(Gfx_CommitPrim)(3, 0x48);
        else
            SH_CALL(MapView_LinkPrimAt)((d >> 8) & 0xFF0000, d & 0xFF0000, row, 0x48);
        s = S();
        ++k;
        place = Place(kWho, at::kSub22Count, at::kSub22Pieces.v);
    } while (k < B(::bof3::Img(at::kSub22Pieces.v) + 2 * place + 1));
}

void EffectKind18Sub22_Inject() {
    BOF3_INJECT(EffectKind18Sub22_Run);
    BOF3_INJECT(EffectKind18Sub22_Start);
    BOF3_INJECT(EffectKind18Sub22_WaitSet);
    BOF3_INJECT(EffectKind18Sub22_Move);
    BOF3_INJECT(EffectKind18Sub22_WaitClear);
    BOF3_INJECT(EffectKind18Sub22_SetMap);
    BOF3_INJECT(EffectKind18Sub22_Draw);
}
