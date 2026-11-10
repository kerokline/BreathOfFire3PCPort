// Moved here by tools/move_funcs.py from effect_2a.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_2b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2a.h"
#include "game/effect_2a_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

void AddWord(unsigned char* p, U d) { SetWord(p, Word(p) + d); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original jumps
// through the dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2a.md section 2)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// A draw mode for the page (x, y) with transparency `abr`, and `dtd` its
// dither flag: Gpu_GetTPage(0, abr, x, y), Gpu_SetDrawMode(the packet cursor,
// 0, dtd, the page's low word, 0).
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
}

void Shade3(unsigned char* v, unsigned char value) { v[0] = v[1] = v[2] = value; }

}  // namespace

// original 0x471930 (0x25 bytes): call [EffectKind2B_States + +1 * 4]
// (unbounded), then, when +1 (Sprite_Current read again) is not 0, a tail jmp
// to EffectKind2B_Draw.
extern "C" void __cdecl EffectKind2B_Run(void) {
    Dispatch("EffectKind2B_Run", AddressOf(EffectKind2B_States), EffectKind2B_States_count);
    if (S()[1] != 0) SH_CALL(EffectKind2B_Draw)();
}

// original 0x471960 (state 0): the words +0x2E (the radius) = 0x100, +0x30 (the
// rise) = 0, +0x32 (a count) = 0x18; +1 up.
extern "C" void __cdecl EffectKind2B_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0x100);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32, 0x18);
    NextState();
}

// original 0x471990 (state 1): the rise +0x30 up 0x80, the count +0x32 down; at
// 0: EffectShards_Clear (the specks freed), +0x32 = 0x12C, +1 up,
// Sound_PlayEffect(0x209).
extern "C" void __cdecl EffectKind2B_Rise(void) {
    AddWord(S() + 0x30, 0x80);
    AddWord(S() + 0x32, 0xFFFFu);
    if (rv::As<records::EffectWork>(S())->work_32 != 0) return;
    SH_CALL(EffectShards_Clear)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32, 0x12C);
    NextState();
    SH_CALL(Sound_PlayEffect)(0x209);
}

// original 0x4719E0 (state 2): on odd frames a speck spawned
// (EffectSpecks_Spawn); the specks moved (EffectSpecks_Move); when the counter
// byte 0x903848 is 0x29, +1 up.
extern "C" void __cdecl EffectKind2B_Specks(void) {
    if (Frame_Counter & 1u) SH_CALL(EffectSpecks_Spawn)();
    SH_CALL(EffectSpecks_Move)();
    if (At(::bof3::Img(at::kCounter0.v))[0] == 0x29) NextState();
}

// original 0x471A10 (state 3): the specks moved; once none is left, +0x32 =
// 0x10 and +1 up.
extern "C" void __cdecl EffectKind2B_Settle(void) {
    if (SH_CALL(EffectSpecks_Move)() != 0) return;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32, 0x10);
    NextState();
}

// original 0x471A30 (state 4): the radius +0x2E down 0x10, the count +0x32
// down; at 0 a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind2B_Shrink(void) {
    AddWord(S() + 0x2E, 0xFFF0u);
    AddWord(S() + 0x32, 0xFFFFu);
    if (rv::As<records::EffectWork>(S())->work_32 != 0) return;
    SH_CALL(Effect_Release)();
}

// original 0x471A60 (0x2AB bytes, no list held it; reached only by the
// dispatcher's tail jmp): the ring. EffectGte_LoadMapCamera; the point at angle
// u on the ring is (x + (cos(u) * r sar 4), z + (sin(u) * r sar 4)) with r the
// word +0x2E (signed) and (x, z) the record's +0x34 / +0x38 (Sprite_Current read
// after each Math_* call), projected at the ground height +0x3C and at the top
// +0x3C + (+0x30 << 16). For u = 0x100, 0x200, .. 0x1000 (each masked to its low
// word for Math_*), sixteen semi-transparent Gouraud quads at the packet cursor
// between the last angle's two projections and this one's, each preceded by a
// draw mode (0x3C0, 0, transparency 1, dithered) and followed by one (not
// dithered), all three linked at this angle's (x, z) by MapView_LinkPrimAt (0xC,
// 0x44, 0xC). The shade starts at (8 + the frame's bit) * 4; the first two
// vertices take it, then it moves by 4 - down for quads 4..11, up for the rest -
// and the last two take that (byte arithmetic, kept from quad to quad).
extern "C" void __cdecl EffectKind2B_Draw(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    long point[3];
    float ground[3], top[3];
    auto Project = [&](U u) {
        {
            const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(u)));
            const unsigned char* const s = S();
            point[0] = static_cast<long>(Sar(c * static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e))), 4) + static_cast<std::uint32_t>(rv::AsConst<records::EffectObject>(s)->x));
        }
        {
            const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(u)));
            const unsigned char* const s = S();
            point[1] = static_cast<long>(Sar(n * static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e))), 4) + static_cast<std::uint32_t>(rv::AsConst<records::EffectObject>(s)->z));
            point[2] = rv::AsConst<records::EffectObject>(s)->height;
        }
        SH_CALL(EffectGte_ProjectPoint)(point, ground);
        const unsigned char* const s = S();
        point[2] = static_cast<long>((static_cast<U>(SW(s + 0x30)) << 16) + static_cast<std::uint32_t>(rv::AsConst<records::EffectObject>(s)->height));
        SH_CALL(EffectGte_ProjectPoint)(point, top);
    };
    Project(0);
    auto shade = static_cast<unsigned char>(static_cast<unsigned char>((Frame_Counter & 1u) + 8u) << 2);
    U u = 0;
    for (unsigned k = 0; k < 16; ++k) {
        float last_ground[3], last_top[3];
        std::memcpy(last_ground, ground, sizeof ground);
        std::memcpy(last_top, top, sizeof top);
        u += 0x100;
        Project(u & 0xFFFFu);
        DrawMode(1, 0x3C0, 0, 1);
        SH_CALL(MapView_LinkPrimAt)(static_cast<U>(point[0]), static_cast<U>(point[1]), 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        std::memcpy(p + 8, last_ground, 12);
        std::memcpy(p + 0x18, last_top, 12);
        std::memcpy(p + 0x28, ground, 12);
        std::memcpy(p + 0x38, top, 12);
        Shade3(p + 4, shade);
        Shade3(p + 0x14, shade);
        shade = static_cast<unsigned char>(k >= 4 && k < 0xC ? shade - 4 : shade + 4);
        Shade3(p + 0x24, shade);
        Shade3(p + 0x34, shade);
        SH_CALL(MapView_LinkPrimAt)(static_cast<U>(point[0]), static_cast<U>(point[1]), 0, 0x44);
        DrawMode(1, 0x3C0, 0, 0);
        SH_CALL(MapView_LinkPrimAt)(static_cast<U>(point[0]), static_cast<U>(point[1]), 0, 0xC);
    }
}

void EffectKind2b_Inject() {
    BOF3_INJECT(EffectKind2B_Run);
    BOF3_INJECT(EffectKind2B_Start);
    BOF3_INJECT(EffectKind2B_Rise);
    BOF3_INJECT(EffectKind2B_Specks);
    BOF3_INJECT(EffectKind2B_Settle);
    BOF3_INJECT(EffectKind2B_Shrink);
    BOF3_INJECT(EffectKind2B_Draw);
}
