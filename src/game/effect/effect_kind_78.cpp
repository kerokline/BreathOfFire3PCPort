// Moved here by tools/move_funcs.py from effect_3d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_78.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_3d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

U W(const unsigned char* p) { return Word(p); }

U W(U a) { return Word(At(a)); }

std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

U AddressOf(const void* p) { return Low32(p); }

void Copy4(unsigned char* to, const unsigned char* from) { std::memcpy(to, from, 4); }   // mov reg, [from]; mov [to], reg

// --- the x87 sequences, as the original's instructions --------------------------
// fild dword v; fstp dword [to]
void Fild(unsigned char* to, std::int32_t v) {
    __asm__ volatile("fildl %1\n\tfstps %0" : "=m"(*reinterpret_cast<float*>(to)) : "m"(v));
}

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, data or the next kind's states.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3d.md section 7)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

void NextState() { S()[1] = static_cast<unsigned char>(S()[1] + 1); }

// +9 down one; true when it reaches 0 (Sprite_Current read for each access).
bool CountDown() {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    return S()[9] == 0;
}

// The ring with the record's words: centre +0x2E, +0x30, radii +0xC, +0x10.
// The original pushes the dwords +0xC, +0x10 whole and the centre's words over
// the upper halves of those registers; the ring reads each as its low word.
void DrawRingOfRecord() {
    unsigned char* const s = S();
    const U rx = rv::As<records::EffectWork>(s)->work_0c, ry = rv::As<records::EffectWork>(s)->work_10;
    SH_CALL(EffectKind78_DrawRing)((rx & 0xFFFF0000u) | static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e), (ry & 0xFFFF0000u) | static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30), rx, ry);
}

}  // namespace

// original 0x486360 (Effect_KindHandlers[0x78], hidden in 0x486310): jmp
// [EffectKind78_States + +1 * 4].
extern "C" void __cdecl EffectKind78_Run(void) {
    Dispatch("EffectKind78_Run", AddressOf(EffectKind78_States), EffectKind78_States_count, 1);
}

// original 0x486380 (state 0): +0x32 the turn word of Sprite_Objects record 2
// (0x7DEFFA), +9 0x14, +0xC and +0x10 0, +0x2E / +0x30 its screen words
// (0x7DEFF6 / 0x7DEFF8); +1 up.
extern "C" void __cdecl EffectKind78_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32, W(::bof3::Img(at::kObject2Turn.v)));
    S()[9] = 0x14;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, static_cast<std::uint32_t>(static_cast<std::uint16_t>(Sprite_Objects[2].screen_x)));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, static_cast<std::uint32_t>(static_cast<std::uint16_t>(Sprite_Objects[2].screen_y)));
    NextState();
}

// original 0x4863E0 (state 1): record 2 turned 0xF00; the radii grow (+0xC by
// 0xF, +0x10 by 0xA); the ring; +9 down, at 0 +9 0x3C and +1 up.
extern "C" void __cdecl EffectKind78_Grow(void) {
    SetWord(At(::bof3::Img(at::kObject2Turn.v)), W(::bof3::Img(at::kObject2Turn.v)) + 0xF00u);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, rv::As<records::EffectWork>(S())->work_0c + 0xF);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, rv::As<records::EffectWork>(S())->work_10 + 0xA);
    DrawRingOfRecord();
    if (CountDown()) {
        S()[9] = 0x3C;
        NextState();
    }
}

// original 0x486450 (state 2): record 2 turned 0xF00, the ring; +9 down, at 0
// +1 up.
extern "C" void __cdecl EffectKind78_Spin(void) {
    SetWord(At(::bof3::Img(at::kObject2Turn.v)), W(::bof3::Img(at::kObject2Turn.v)) + 0xF00u);
    unsigned char* const s = S();
    // here the upper halves are the caller's (the dispatcher's ecx, Sprite_Current); the ring reads the low words
    SH_CALL(EffectKind78_DrawRing)(static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e), static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30), W(s + 0xC), W(s + 0x10));
    if (CountDown()) NextState();
}

// original 0x4864A0 (state 3): record 2's turn put back from +0x32; a tail jump
// to Effect_Release.
extern "C" void __cdecl EffectKind78_End(void) {
    SetWord(At(::bof3::Img(at::kObject2Turn.v)), static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32));
    SH_CALL(Effect_Release)();
}

// original 0x4864C0 (0x173 bytes, cdecl): 32 flat triangles about (cx, cy) - each
// the centre, the previous rim point and the next at angles 0x80 k, the rim
// (cx + (cos a * rx >> 12), cy + (sin a * ry >> 12)) each term's low word sign-
// extended; the first previous point at angle 0. Red (0xFF, 0, 0) or white by
// Frame_Counter bit 2 and k's parity; linked at Sprite_Objects record 2's x, z
// (MapView_LinkPrimAt, dy 0xC, 0x2C). Each argument read as its low word.
extern "C" void __cdecl EffectKind78_DrawRing(unsigned cx, unsigned cy, unsigned rx, unsigned ry) {
    const std::int32_t rxs = S16(rx);
    const U c0 = static_cast<U>(SH_CALL(Math_Cos)(0));
    const U x0 = static_cast<U>(static_cast<std::int32_t>(c0 * static_cast<U>(rxs)) >> 12);
    const std::int32_t rys = S16(ry);
    const U s0 = static_cast<U>(SH_CALL(Math_Sin)(0));
    const U y0 = static_cast<U>(static_cast<std::int32_t>(s0 * static_cast<U>(rys)) >> 12);
    const std::int32_t cxs = S16(cx), cys = S16(cy);
    unsigned char centre_x[4], centre_y[4], prev_x[4], prev_y[4];
    Fild(centre_x, cxs);
    Fild(centre_y, cys);
    Fild(prev_x, static_cast<std::int32_t>(static_cast<U>(S16(x0)) + static_cast<U>(cxs)));
    Fild(prev_y, static_cast<std::int32_t>(static_cast<U>(S16(y0)) + static_cast<U>(cys)));
    U angle = 0;
    for (U k = 0; k < 0x20; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        SH_AT(void (__cdecl*)(unsigned char*), at::kPolyF3.v)(p);
        angle += 0x80;
        const U a = angle & 0xFFFFu;
        Copy4(p + 8, centre_x);
        Copy4(p + 0xC, centre_y);
        Copy4(p + 0x14, prev_x);
        Copy4(p + 0x18, prev_y);
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
        const U xr = static_cast<U>(static_cast<std::int32_t>(c * static_cast<U>(S16(rx))) >> 12);
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
        const U yr = static_cast<U>(static_cast<std::int32_t>(sn * static_cast<U>(S16(ry))) >> 12);
        const U x = static_cast<U>(S16(xr)) + static_cast<U>(S16(cx));
        Fild(prev_x, static_cast<std::int32_t>(x));
        Copy4(p + 0x20, prev_x);
        const U y = static_cast<U>(S16(yr)) + static_cast<U>(S16(cy));
        Fild(prev_y, static_cast<std::int32_t>(y));
        Copy4(p + 0x24, prev_y);
        p[4] = 0xFF;
        if ((((Frame_Counter >> 2) & 1u) ^ (k & 1u)) == 0) {
            p[5] = 0xFF;
            p[6] = 0xFF;
        } else {
            p[5] = 0;
            p[6] = 0;
        }
        SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(Sprite_Objects[2].x), static_cast<std::uint32_t>(Sprite_Objects[2].z), 0xC, 0x2C);
    }
}

void EffectKind78_Inject() {
    BOF3_INJECT(EffectKind78_Run);
    BOF3_INJECT(EffectKind78_Start);
    BOF3_INJECT(EffectKind78_Grow);
    BOF3_INJECT(EffectKind78_Spin);
    BOF3_INJECT(EffectKind78_End);
    BOF3_INJECT(EffectKind78_DrawRing);
}
