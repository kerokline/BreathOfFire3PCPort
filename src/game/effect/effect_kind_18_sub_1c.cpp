// Moved here by tools/move_funcs.py from effect_5d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_1c.h"

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

U Abs(std::int32_t v) { return static_cast<U>(v < 0 ? -v : v); }   // cdq; xor; sub (small values here)

U AddressOf(const void* p) { return Low32(p); }

U FC() { return Frame_Counter; }

float Fl(U a) {
    float f;
    std::memcpy(&f, At(a), sizeof f);
    return f;
}

long double SX() { return Fl(AddressOf(MapView_ScreenXY)); }   // fld / fadd dword [0x903820]

long double SY() { return MapView_ScreenXY[1]; }

void StoreFloat(unsigned char* p, long double v) {
    const float f = static_cast<float>(v);   // fstp dword
    std::memcpy(p, &f, sizeof f);
}

unsigned char* Vertex() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

void SetV(unsigned offset, U v) { SetWord(Vertex() + offset, v); }

const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertex() + 8 * i); }

unsigned long* ScreenXY() { return reinterpret_cast<unsigned long*>(MapView_ScreenXY); }

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

// An effect record by Effect_FindFree's answer (not 0xFF).
unsigned char* Record(const char* who, U index) {
    if (index >= at::kEffectCount)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes past Effect_Objects "
                    "(docs/effect_5d.md section 7)",
                    who, (unsigned)index);
    return rv::Bytes(Effect_Objects) + at::kEffectStride * index;
}

// The x87's quad corners about a screen point: the first float less the half
// size, the second that plus the whole size (no rounding between, as the
// original keeps both on the FPU stack).
long double Less(long double centre, long double half) { return centre - half; }

// Sub-kinds 0x1C / 0x1D's sub-state 1: Field_Request 0: +9 down one; at 0, +2 up
// and, the record on screen, Sound_PlayEffect(0x200).
void WaitThenSound() {
    if (Field_Request != 0) return;
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) return;
    s[2] = static_cast<unsigned char>(s[2] + 1);
    if (SH_CALL(EffectKind18Sub1C_OnScreen)() != 0) SH_CALL(Sound_PlayEffect)(0x200);
}

// Sub-kinds 0x1C / 0x1D's column (0x5052D0 / 0x5057D0, cdecl `rise`): eight
// semi-transparent squares from the record's cell, each a screen point
// (Gte_RotTransPers) with Gte_PrimDepthFlat4_10, half size (level or 4 past the
// fourth) * rise / 2 + (Frame_Counter & 1) * 4 rise / 16 + 4, texture
// 0xBA009124 | ((0x1F80000 - level 0x400000 - (Frame_Counter & 1) << 21) / 4 /
// (8 - rise)) & 0xF80000; the vertex `place` sets. The original divides by
// 8 - rise unchecked: 8 faults it, ours aborts.
template <typename Place>
void Column(const char* who, U rise, U row, Place place) {
    unsigned char* s = S();
    const U frame = FC();
    const U x0 = static_cast<U>(S16(W(s + 0x36)) - 0x80) << 7;
    const U z0 = static_cast<U>(S16(W(s + 0x3A)) - 0x80) << 7;
    const U y0 = static_cast<U>(-(S16(W(s + 0x3E)) / 2));
    const U odd = (frame & 1) << 2;
    const U spread = static_cast<U>(I(odd * rise) / 16);
    const std::int32_t divisor = static_cast<std::int32_t>(8 - rise);
    const U page = odd << 19;
    U level = 0;
    std::int32_t c = 0x1F80000;
    do {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        place(level, x0, z0, y0, odd);
        long depth[2];
        SH_CALL(Gte_RotTransPers)(Vx(0), ScreenXY(), depth);
        SH_CALL(Gte_PrimDepthFlat4_10)(p);
        const U m = c < 0x1380000 ? 4 : level;
        const U half = static_cast<U>(I(m * rise) / 2) + spread + 4;
        if (divisor == 0)
            bof3::Fatal("%s: the column's divisor 8 - rise is 0 (rise 8) - the original divides by it unchecked and faults "
                        "(docs/effect_5d.md section 7)",
                        who);
        const std::int32_t step = I(static_cast<U>(c) - page) / 4;
        const U texture = static_cast<U>(step / divisor);
        const long double h = static_cast<long double>(I(half));
        const long double sx = SX();
        StoreFloat(p + 0x28, sx - h);
        StoreFloat(p + 8, sx - h);
        StoreFloat(p + 0x38, h + SX());
        StoreFloat(p + 0x18, h + SX());
        const long double sy = SY();
        StoreFloat(p + 0x1C, sy - h);
        StoreFloat(p + 0xC, sy - h);
        StoreFloat(p + 0x3C, h + SY());
        StoreFloat(p + 0x2C, h + SY());
        SH_CALL(Prim_SetTexture)((texture & 0xF80000) | 0xBA009124u, p, 1);
        if (row == 3) p[6] = 0;
        s = S();
        SH_CALL(MapView_LinkPrimAt)(ULEffect5d(s + 0x34), ULEffect5d(s + 0x38), static_cast<int>(row), 0x48);
        c -= 0x400000;
        ++level;
    } while (c > static_cast<std::int32_t>(0xFFF80000u));
}

}  // namespace

// original 0x505100 (EffectKind18_States[28], hidden in 0x505000; a catalog row
// of part 2 that no group of the round holds): jmp [EffectKind18Sub1C_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub1C_Run(void) {
    DispatchEffect5d("EffectKind18Sub1C_Run", E5D_TABLE(EffectKind18Sub1C_States));
}

// original 0x505120 (sub-state 0; sub-kind 0x43's 4): +0x3E the ground's height
// at the point; +0xA the wait 0x65E2DC gives the cell's parity (bit 0 of z's
// and x's cell), +9 (Rand & 0x2F) + it; the cell's map byte 0; +2 up.
extern "C" void __cdecl EffectKind18Sub1C_Start(void) {
    unsigned char* s = S();
    const U e = static_cast<U>(SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, e);
    s = S();
    s[0xA] = B(::bof3::Img(at::kSub1CWaits.v) + (((s[0x3A] & 1u) << 1) | (s[0x36] & 1u)));
    const U r = static_cast<U>(SH_CALL(Rand)());
    s = S();
    s[9] = static_cast<unsigned char>((r & 0x2F) + s[0xA]);
    s = S();
    SH_CALL(AreaMap_SetByte)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole), 0);
    s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x5051A0 (sub-state 1): WaitThenSound.
extern "C" void __cdecl EffectKind18Sub1C_Wait(void) { WaitThenSound(); }

// original 0x5051E0 (sub-state 2): the column at +9; +9 up one; past 7 the
// cell's map byte 0x8A, +9 0, +2 up.
extern "C" void __cdecl EffectKind18Sub1C_Rise(void) {
    SH_CALL(EffectKind18Sub1C_Draw)(S()[9]);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 7) {
        SH_CALL(AreaMap_SetByte)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole), 0x8A);
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x505240 (sub-state 3): the column at 7; +9 up one; past 0x1E the
// cell's map byte 0, +9 0x1F, +2 up.
extern "C" void __cdecl EffectKind18Sub1C_Hold(void) {
    SH_CALL(EffectKind18Sub1C_Draw)(7);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 0x1E) {
        SH_CALL(AreaMap_SetByte)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole), 0);
        S()[9] = 0x1F;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x505290 (sub-state 4): the column at +9 >> 2; +9 down one; below 8,
// +9 the wait +0xA and +2 1.
extern "C" void __cdecl EffectKind18Sub1C_Fall(void) {
    SH_CALL(EffectKind18Sub1C_Draw)(S()[9] >> 2);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] < 8) {
        s[9] = s[0xA];
        S()[2] = 1;
    }
}

// original 0x5052D0 (cdecl): Column - the square's point the record's cell, y
// rising (odd + 8 level) * rise above half the stored height; linked on the
// record's row (dy 1).
extern "C" void __cdecl EffectKind18Sub1C_Draw(U rise) {
    Column("EffectKind18Sub1C_Draw", rise, 1, [rise](U level, U x0, U z0, U y0, U odd) {
        SetV(0, x0);
        SetV(2, z0);
        SetV(4, y0 - (odd + level * 8) * rise);
    });
}

// original 0x505480 (sub-kinds 0x1C / 0x1D's sub-state 1): the record's cell at
// half its stored height projected (Gte_RotTransPers to MapView_ScreenXY);
// eax 1 when both screen floats lie in [-20, 340] (0x5C422C, 0x5C4228; x87
// compares, a NaN outside), else 0. DIV-0041's "unnamed 0x5054E3" cull - left
// at 320's bounds as that entry says.
extern "C" U __cdecl EffectKind18Sub1C_OnScreen(void) {
    unsigned char* const s = S();
    SetV(4, static_cast<U>(-(S16(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole)) / 2)));
    SetV(2, static_cast<U>(S16(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole)) - 0x80) << 7);
    SetV(0, (static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole) - 0x80) << 7);
    long depth[2];
    SH_CALL(Gte_RotTransPers)(Vx(0), ScreenXY(), depth);
    const float low = Fl(at::kScreenLow), high = Fl(at::kScreenHigh);
    const float sx = Fl(AddressOf(MapView_ScreenXY)), sy = Fl(::bof3::Img(at::kScreenY.v));
    if (!(sx >= low)) return 0;
    if (sx > high) return 0;
    if (!(sy >= low)) return 0;
    if (sy > high) return 0;
    return 1;
}

// original 0x505540 (EffectKind18_States[29], hidden in 0x505480; a catalog row
// of part 2 that no group of the round holds): jmp [EffectKind18Sub1D_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub1D_Run(void) {
    DispatchEffect5d("EffectKind18Sub1D_Run", E5D_TABLE(EffectKind18Sub1D_States));
}

// original 0x505560 (sub-state 0): +0xA the wait 0x65E2F4 gives the cell's
// parity, +9 it; +0x3E the ground's height at the point; the direction (+0xC,
// +0x10) (0, 1) when the ground half a cell along (+x, -z) is higher, else
// (1, 0); +2 up.
extern "C" void __cdecl EffectKind18Sub1D_Start(void) {
    unsigned char* s = S();
    s[0xA] = B(::bof3::Img(at::kSub1DWaits.v) + (((s[0x3A] & 1u) << 1) | (s[0x36] & 1u)));
    s = S();
    s[9] = s[0xA];
    s = S();
    const U e = static_cast<U>(SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, e);
    s = S();
    const U e2 = static_cast<U>(
        SH_CALL(AreaMap_Elevation)(static_cast<long>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x) + 0x8000), static_cast<long>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z) - 0x8000)));
    s = S();
    if (S16(e2) > S16(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole))) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, 0);
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 1);
    } else {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, 1);
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0);
    }
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x505610 (sub-state 1): WaitThenSound (0x5051A0's twin).
extern "C" void __cdecl EffectKind18Sub1D_Wait(void) { WaitThenSound(); }

// original 0x505650 (sub-state 2): the column at +9; +9 up one; past 7, +9 0 and
// +2 up.
extern "C" void __cdecl EffectKind18Sub1D_Rise(void) {
    SH_CALL(EffectKind18Sub1D_Draw)(S()[9]);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 7) {
        s[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x505690 (sub-state 3): for the cells one and two along the direction
// (+0xC, +0x10) from the point: Party_MemberAt(x, z, 0); a member there
// (al not negative): Field_State and Sprite_Current that member's ObjTrio
// record; Actor_EquipCount(its +0x148, 3, 8) answering 0: unless bit 5 of its
// Field_ActorStates byte, Sprite_FlashClut(2); Field_FloorHurt(5); then
// Sprite_Current put back (Field_State left). Then the column at 7; +9 up one;
// past 0x1E, +9 0x1F and +2 up.
extern "C" void __cdecl EffectKind18Sub1D_Hurt(void) {
    static const char kWho[] = "EffectKind18Sub1D_Hurt";
    unsigned char* s = S();
    for (U step = 1; step < 3; ++step) {
        const U x = ((ULEffect5d(s + 0xC) * step) << 16) + ULEffect5d(s + 0x34);
        const U z = ((ULEffect5d(s + 0x10) * step) << 16) + ULEffect5d(s + 0x38);
        const unsigned char who = SH_CALL(Party_MemberAt)(static_cast<long>(x), static_cast<long>(z), 0);
        if (S8(who) < 0) {
            s = S();
            continue;
        }
        const U member = Index(kWho, S8(who), at::kMemberCount, AddressOf(rv::Bytes(ObjTrio)));
        unsigned char* const saved = S();
        unsigned char* const object = rv::Bytes(&ObjTrio[member]);
        Field_State = object;
        Sprite_Current = object;
        if (SH_CALL(Actor_EquipCount)(object[0x148], 3, 8) == 0) {
            if ((Field_ActorStates[Field_State[0x148] * at::kActorStride] & 0x20) == 0) SH_CALL(Sprite_FlashClut)(2);
            SH_CALL(Field_FloorHurt)(5);
        }
        Sprite_Current = saved;
        s = saved;
    }
    SH_CALL(EffectKind18Sub1D_Draw)(7);
    s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 0x1E) {
        s[9] = 0x1F;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x505790 (sub-state 4): 0x505290's twin with sub-kind 0x1D's column.
extern "C" void __cdecl EffectKind18Sub1D_Fall(void) {
    SH_CALL(EffectKind18Sub1D_Draw)(S()[9] >> 2);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] < 8) {
        s[9] = s[0xA];
        S()[2] = 1;
    }
}

// original 0x5057D0 (cdecl): Column - the square's point leaning along the
// direction (+0xC, +0x10, low words) by (odd + 8 level) * rise, y half the
// stored height; blue 0, linked on the record's row (dy 3).
extern "C" void __cdecl EffectKind18Sub1D_Draw(U rise) {
    Column("EffectKind18Sub1D_Draw", rise, 3, [rise](U level, U x0, U z0, U y0, U odd) {
        const U lean = odd + level * 8;
        unsigned char* const s = S();
        SetV(0, ((W(s + 0xC) * lean) & 0xFFFF) * rise + x0);
        SetV(2, ((W(s + 0x10) * lean) & 0xFFFF) * rise + z0);
        SetV(4, y0);
    });
}

// original 0x5059A0 (EffectKind18_States[20], hidden in 0x5057D0):
// jmp [EffectKind18Sub14_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub14_Run(void) {
    DispatchEffect5d("EffectKind18Sub14_Run", E5D_TABLE(EffectKind18Sub14_States));
}

// original 0x5059C0 (sub-state 0): area flag 0x3E (0x903FD8) set: the strip at
// half (0x40) and Effect_Release; +2 up either way.
extern "C" void __cdecl EffectKind18Sub14_Start(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x48), 0x3E)) {
        SH_CALL(EffectKind18Sub14_ScaleClut)(0x40);
        SH_CALL(Effect_Release)();
    }
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x5059F0 (sub-state 1): once area flag 0x3E is set: +9 up one, the
// strip at 0x80 - (+9 >> 1); at 0x80 or past, a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind18Sub14_Dim(void) {
    if (!SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x48), 0x3E)) return;
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    SH_CALL(EffectKind18Sub14_ScaleClut)(0x80u - (S()[9] >> 1));
    if (S()[9] >= 0x80) SH_CALL(Effect_Release)();
}

// original 0x505A40 (cdecl): the sixteen 15-bit colours at 0x80BCC0 written to
// 0x80FCC0 with red and blue scaled by scale / 128 (green and bit 15 kept), and
// Gfx_ClutStripDirty set.
extern "C" void __cdecl EffectKind18Sub14_ScaleClut(U scale) {
    for (U k = 0; k < at::kClutStripWords; ++k) {
        const U w = W(::bof3::Img(at::kClutStrip.v) + 2 * k);
        const U blue = static_cast<U>(I(((w >> 10) & 0x1F) * scale) / 128);
        const U red = static_cast<U>(I((w & 0x1F) * scale) / 128);
        SetWord(At(::bof3::Img(Gfx_ClutStrip_addr) + 0x740 + 2 * k), (blue << 10) | red | (w & 0x83E0));
    }
    Gfx_ClutStripDirty = 1;
}

// original 0x505AB0 (EffectKind18_States[30], hidden in 0x505A40):
// jmp [EffectKind18Sub1E_States + +2 * 4], unbounded (0x65E300, sub-kind 0x14's
// table from its entry 2).
extern "C" void __cdecl EffectKind18Sub1E_Run(void) {
    DispatchEffect5d("EffectKind18Sub1E_Run", E5D_TABLE(EffectKind18Sub1E_States));
}

// original 0x505AD0 (sub-kind 0x14's sub-state 3, 0x1E's 1): the place's flag
// (0x65E31C by +0x36) set in 0x903FD8: Sound_PlayEffect(0x20A); a free record
// made kind 0x6E at the place's point (0x65E30C; height 0x7800000), another
// made kind 0x6D with +6 the place; +9 0, +2 up. The tiles at full shade either
// way.
extern "C" void __cdecl EffectKind18Sub14_WaitFlag(void) {
    static const char kWho[] = "EffectKind18Sub14_WaitFlag";
    const U place = Place(kWho, at::kSub14Count, at::kSub14Flags.v);
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x48), B(::bof3::Img(at::kSub14Flags.v) + place))) {
        SH_CALL(Sound_PlayEffect)(0x20A);
        const U first = SH_CALL(Effect_FindFree)() & 0xFFu;
        if (first != 0xFF) {
            unsigned char* const s = S();
            unsigned char* const e = Record(kWho, first);
            e[0] = 1;
            e[5] = 0x6E;
            U k = Index(kWho, S16(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole)), at::kSub14Count, at::kSub14Spawn.v);
            SetUL(e + 0x34, ULEffect5d(::bof3::Img(at::kSub14Spawn.v) + 8 * k));
            k = Index(kWho, S16(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole)), at::kSub14Count, at::kSub14Spawn.v);
            SetUL(e + 0x38, ULEffect5d(::bof3::Img(at::kSub14Spawn.v) + 4 + 8 * k));
            SetUL(e + 0x3C, 0x7800000);
        }
        const U second = SH_CALL(Effect_FindFree)() & 0xFFu;
        unsigned char* const s = S();
        if (second != 0xFF) {
            unsigned char* const e = Record(kWho, second);
            e[0] = 1;
            e[5] = 0x6D;
            e[6] = s[0x36];
        }
        s[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub14_DrawTiles)(0x80, 0);
}

// original 0x505BB0 (sub-kind 0x14's sub-state 4, 0x1E's 2): +9 up one on even
// frames; past 0x7F Effect_Release (a call: the tiles still drawn); the tiles
// at 0x80 - +9.
extern "C" void __cdecl EffectKind18Sub14_Fade(void) {
    if ((FC() & 1) == 0) S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] > 0x7F) SH_CALL(Effect_Release)();
    SH_CALL(EffectKind18Sub14_DrawTiles)(0x80u - S()[9], 0);
}

// original 0x505BF0 (cdecl; the second argument unread): over the place's
// rectangle (0x65E320 by +0x36: columns from..to, rows from..to), every cell
// whose column + row is odd a semi-transparent textured square on the ground
// plane (z -0x340) projected (Gte_RotTransPers), half size |3 - ((Frame_Counter
// / 3 + column + row) & 7)| + 0x18, shaded (shade / 2, shade, shade / 2),
// linked on the cell's row (dy 1).
extern "C" void __cdecl EffectKind18Sub14_DrawTiles(U shade, U unused) {
    (void)unused;
    static const char kWho[] = "EffectKind18Sub14_DrawTiles";
    U place = Place(kWho, at::kSub14Count, at::kSub14Rect.v);
    U row = B(::bof3::Img(at::kSub14Rect.v) + 4 * place + 1);
    if (row >= B(::bof3::Img(at::kSub14Rect.v) + 4 * place + 3)) return;
    do {
        U column = B(::bof3::Img(at::kSub14Rect.v) + 4 * place);
        if (column < B(::bof3::Img(at::kSub14Rect.v) + 4 * place + 2)) {
            U parity = column + row;
            do {
                if (parity & 1) {
                    SetV(0, (column << 7) - 0x3FC0);
                    SetV(2, (row << 7) - 0x3FC0);
                    SetV(4, 0xFCC0);
                    long depth[2];
                    SH_CALL(Gte_RotTransPers)(Vx(0), ScreenXY(), depth);
                    const U swing = ((FC() / 3) + column + row) & 7;
                    const U half = Abs(3 - static_cast<std::int32_t>(swing)) + 0x18;
                    unsigned char* const p = Gfx_PacketNext;
                    SH_CALL(Gpu_SetPolyFT4)(p);
                    SH_CALL(Gpu_SetShadeTex)(p, 0);
                    const long double h = static_cast<long double>(I(half));
                    const long double whole = static_cast<long double>(I(half * 2));
                    StoreFloat(p + 8, Less(SX(), h));
                    StoreFloat(p + 0xC, Less(SY(), h));
                    StoreFloat(p + 0x18, Less(SX(), h) + whole);
                    StoreFloat(p + 0x1C, Less(SY(), h));
                    StoreFloat(p + 0x28, Less(SX(), h));
                    StoreFloat(p + 0x2C, Less(SY(), h) + whole);
                    StoreFloat(p + 0x38, Less(SX(), h) + whole);
                    StoreFloat(p + 0x3C, Less(SY(), h) + whole);
                    SH_CALL(Gte_PrimDepthFlat4_10)(p);
                    SH_CALL(Gpu_SetSemiTrans)(p, 1);
                    const unsigned char half_shade = static_cast<unsigned char>(I(shade << 6) / 128);
                    const unsigned char full_shade = static_cast<unsigned char>(I(shade << 7) / 128);
                    p[0x14] = 0xE0;
                    p[0x15] = 0x30;
                    p[0x24] = 0xFF;
                    p[0x25] = 0x30;
                    p[0x34] = 0xE0;
                    p[0x35] = 0x4F;
                    p[0x44] = 0xFF;
                    p[0x45] = 0x4F;
                    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, 0x3B);
                    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, 0x78C5);
                    p[4] = half_shade;
                    p[5] = full_shade;
                    p[6] = half_shade;
                    SH_CALL(MapView_LinkPrimAt)(column << 16, row << 16, 1, 0x48);
                }
                ++parity;
                ++column;
                place = Place(kWho, at::kSub14Count, at::kSub14Rect.v);
            } while (column < B(::bof3::Img(at::kSub14Rect.v) + 4 * place + 2));
        }
        place = Place(kWho, at::kSub14Count, at::kSub14Rect.v);
        ++row;
    } while (row < B(::bof3::Img(at::kSub14Rect.v) + 4 * place + 3));
}

void EffectKind18Sub1c_Inject() {
    BOF3_INJECT(EffectKind18Sub1C_Run);
    BOF3_INJECT(EffectKind18Sub1C_Start);
    BOF3_INJECT(EffectKind18Sub1C_Wait);
    BOF3_INJECT(EffectKind18Sub1C_Rise);
    BOF3_INJECT(EffectKind18Sub1C_Hold);
    BOF3_INJECT(EffectKind18Sub1C_Fall);
    BOF3_INJECT(EffectKind18Sub1C_Draw);
    BOF3_INJECT(EffectKind18Sub1C_OnScreen);
    BOF3_INJECT(EffectKind18Sub1D_Run);
    BOF3_INJECT(EffectKind18Sub1D_Start);
    BOF3_INJECT(EffectKind18Sub1D_Wait);
    BOF3_INJECT(EffectKind18Sub1D_Rise);
    BOF3_INJECT(EffectKind18Sub1D_Hurt);
    BOF3_INJECT(EffectKind18Sub1D_Fall);
    BOF3_INJECT(EffectKind18Sub1D_Draw);
    BOF3_INJECT(EffectKind18Sub14_Run);
    BOF3_INJECT(EffectKind18Sub14_Start);
    BOF3_INJECT(EffectKind18Sub14_Dim);
    BOF3_INJECT(EffectKind18Sub14_ScaleClut);
    BOF3_INJECT(EffectKind18Sub1E_Run);
    BOF3_INJECT(EffectKind18Sub14_WaitFlag);
    BOF3_INJECT(EffectKind18Sub14_Fade);
    BOF3_INJECT(EffectKind18Sub14_DrawTiles);
}
