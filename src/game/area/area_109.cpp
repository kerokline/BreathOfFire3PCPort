// Moved here by tools/move_funcs.py from effect_5a.cpp, rest_3g.cpp (docs/source-layout-round.md).
#include "game/area/area_109.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area/area_109_callees.h"
#include "game/cheats.h"
#include "game/draw_pool.h"
#include "game/effect_5a.h"
#include "game/effect_5a_callees.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/psp_code.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3g.h"
#include "game/rest_3g_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_109::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

unsigned char T(U a) { return *At(a); }

// An index into one of the image's tables by a byte or word of the record,
// unchecked by the original (it reads whatever follows).
std::int32_t Index(const char* who, std::int32_t i, unsigned n, U table) {
    if (i < 0 || static_cast<unsigned>(i) >= n)
        bof3::Fatal("%s: index %d into 0x%X, past its %u entries - the original reads what follows "
                    "(docs/effect_5a.md section 7)",
                    who, (int)i, (unsigned)table, n);
    return i;
}

}  // namespace

// original 0x4FEDD0: the flags' pattern (0x4FEE70) changed: +2 it, +0x3E its
// height, the map's bytes (0x42..0x44, 3..6) 0x10, or 0 at -0x80; then
// Cond_ByteFE by +2.
extern "C" void __cdecl EffectKind18_09_Pattern(void) {
    static const char kWho[] = "EffectKind18_09_Pattern";
    using Fn = U(__cdecl*)();
    const U pattern = SH_AT(Fn, at::kPattern.v)();
    if (static_cast<U>(S()[2]) != pattern) {
        const U again = SH_AT(Fn, at::kPattern.v)();
        S()[2] = static_cast<unsigned char>(again);
        unsigned char* const s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, Word(At(::bof3::Img(at::kHeights9.v) + 2 * static_cast<U>(Index(kWho, s[2], at::kPatterns, at::kHeights9.v)))));
        const unsigned char value = static_cast<std::uint16_t>(rv::As<records::EffectObject>(S())->height_whole) != 0xFF80 ? 0x10 : 0;
        for (U z = 3; z < 7; ++z)
            for (U x = 0x42; x < 0x45; ++x) {
                const U w = AreaMap_Header[0];
                unsigned char* const bytes = const_cast<unsigned char*>(AreaMap_Bytes.Native());
                bytes[w * z + x] = value;
            }
    }
    Cond_ByteFE = T(::bof3::Img(at::kFe9.v) + static_cast<U>(Index(kWho, S()[2], at::kPatterns, at::kFe9.v)));
}

// original 0x4FEE70 (cdecl; EffectKind18_09_Pattern twice, Area109_SwitchHook):
// the three story flags 0x65DE60 lists as bits 0..2, plus 1 - eax 1..8.
extern "C" int __cdecl Area109_SwitchPattern(void) {
    U bits = 0;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char set = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), At(::bof3::Img(at::kPatternFlags.v) + i)[0]);
        if (set != 0) bits |= 1u << i;
    }
    return static_cast<int>(bits + 1);
}

// original 0x4FEEB0 (Area_CellHook's hook for area 0x6D, (x, z) cell bytes,
// al): at cell (0x1C, 6) with the leader facing 3 and story flag 0x1C clear, the
// pattern (Area109_SwitchPattern) mod 6 (idiv) set as the three flags anew -
// each cleared, then set by its bit; Effect_HoldFlag1C(0xF); sounds 0x206,
// 0x202; al 1. Otherwise al 0 (the rest of eax the caller's).
extern "C" unsigned char __cdecl Area109_SwitchHook(long x, long z) {
    if ((static_cast<U>(x) & 0xFF) != 0x1C || (static_cast<U>(z) & 0xFF) != 6 || ObjTrio[0].direction != 3) return 0;
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1C) != 0) return 0;
    const int pattern = SH_CALL(Area109_SwitchPattern)();
    int bits = pattern % 6;
    for (unsigned i = 0; i < 3; ++i) {
        SH_CALL(Flags_Clear)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), At(::bof3::Img(at::kPatternFlags.v) + i)[0]);
        if (bits & 1) SH_CALL(Flags_Set)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), At(::bof3::Img(at::kPatternFlags.v) + i)[0]);
        bits >>= 1;
    }
    SH_CALL(Effect_HoldFlag1C)(0xF);
    SH_CALL(Sound_PlayEffect)(0x206);
    SH_CALL(Sound_PlayEffect)(0x202);
    return 1;
}

void Area109_Inject() {
    BOF3_INJECT(EffectKind18_09_Pattern);
    BOF3_INJECT(Area109_SwitchPattern);
    BOF3_INJECT(Area109_SwitchHook);
}
