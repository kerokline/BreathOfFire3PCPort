// Moved here by tools/move_funcs.py from effect_2a.cpp (docs/source-layout-round.md).
#include "game/effect/effect_specks.h"

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

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// The pool's records.
unsigned char* Pool(U stride, unsigned i) { return At(::bof3::Img(EffectKind30_Shards_addr) + stride * i); }

}  // namespace

// original 0x471D10 (0xB7 bytes; kinds 0x1C and 0x1D's rise and hold call it
// too): the first free speck of the pool (+0 == 0, 0x80 of 0x14; none: return)
// gets +0 = 1, +1 = 0x40; a distance d = Rand % the word +0x2E (signed, idiv;
// read again after Rand) when +0x2E is not 0, else 0; an angle Rand & 0xFFF; +4 /
// +8 = (cos / sin(angle) * d (as s16) sar 4) + Sprite_Current's +0x34 / +0x38;
// +0xC = (the rise +0x30 << 16) + +0x3C; the word +2 = (Rand & 0xFFF) + 0x1000
// (Sprite_Current read after each call). Nothing a caller reads is answered.
extern "C" void __cdecl EffectSpecks_Spawn(void) {
    unsigned char* r = nullptr;
    for (unsigned i = 0; i < at::kPoolCount; ++i)
        if (Pool(at::kSpeckStride, i)[0] == 0) {
            r = Pool(at::kSpeckStride, i);
            break;
        }
    if (!r) return;
    r[0] = 1;
    r[1] = 0x40;
    std::int32_t d = 0;
    if (Word(S() + 0x2E) != 0) {
        const auto n = static_cast<std::int32_t>(SH_CALL(Rand)());
        const std::int32_t m = SW(S() + 0x2E);
        if (m == 0 || (m == -1 && n == INT32_MIN))
            bof3::Fatal("EffectSpecks_Spawn: %d / %d - the original's idiv faults (the word +0x2E read again after "
                        "Rand; docs/effect_2a.md section 2)",
                        n, m);
        d = n % m;
    }
    const U angle = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U dist = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(d)));
    const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
    SetUL(r + 4, Sar(c * dist, 4) + UL(S() + 0x34));
    const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
    const unsigned char* const s = S();
    SetUL(r + 8, Sar(n * dist, 4) + UL(s + 0x38));
    SetUL(r + 0xC, (static_cast<U>(SW(s + 0x30)) << 16) + UL(s + 0x3C));
    SetWord(r + 2, (static_cast<U>(SH_CALL(Rand)()) & 0xFFFu) + 0x1000u);
}

// original 0x471DD0 (0x4B bytes): each speck in use: its height +0xC down (the
// word +2, signed, << 8), drawn (EffectSpecks_Draw), freed (+0 = 0) once below
// Sprite_Current's +0x3C (signed); al 1 when any was in use, else 0.
extern "C" unsigned char __cdecl EffectSpecks_Move(void) {
    unsigned char any = 0;
    for (unsigned i = 0; i < at::kPoolCount; ++i) {
        unsigned char* const r = Pool(at::kSpeckStride, i);
        if (r[0] == 0) continue;
        rv::Store<std::uint32_t>(rv::As<records::EffectSpeck>(r)->height, static_cast<std::uint32_t>(rv::As<records::EffectSpeck>(r)->height) - (static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectSpeck>(r)->fall))) << 8));
        SH_CALL(EffectSpecks_Draw)(r);
        if (rv::As<records::EffectSpeck>(r)->height < Long(S() + 0x3C)) r[0] = 0;
        any = 1;
    }
    return any;
}

// original 0x471E20 (0x77 bytes): the speck's point (+4..+0xC) projected
// (EffectGte_ProjectPoint into a stack vector); a TILE_1 at the packet cursor
// (Gpu_SetTile1, opaque) at it, white on an odd Rand, else black; linked at the
// speck's x, z by MapView_LinkPrimAt(x, z, 0, 0x14).
extern "C" void __cdecl EffectSpecks_Draw(const unsigned char* speck) {
    float screen[3];
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(speck + 4), screen);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    std::memcpy(p + 8, screen, 12);
    const auto c = static_cast<unsigned char>((SH_CALL(Rand)() & 1) ? 0xFF : 0);
    p[6] = p[5] = p[4] = c;
    SH_CALL(MapView_LinkPrimAt)(UL(speck + 4), UL(speck + 8), 0, 0x14);
}

void EffectSpecks_Inject() {
    BOF3_INJECT(EffectSpecks_Spawn);
    BOF3_INJECT(EffectSpecks_Move);
    BOF3_INJECT(EffectSpecks_Draw);
}
