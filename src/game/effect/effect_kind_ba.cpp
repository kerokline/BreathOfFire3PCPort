// Moved here by tools/move_funcs.py from effect_4f.cpp, rest_3g.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_ba.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/draw_pool.h"
#include "game/effect/effect_kind_ba_callees.h"
#include "game/effect_4f.h"
#include "game/effect_4f_callees.h"
#include "game/effect_gte.h"
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

namespace at = effect_kind_ba::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

unsigned char* S() { return Sprite_Current; }

U ULEffect4f(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect4f(U a) { return ULEffect4f(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U AddressOf(const void* p) { return Low32(p); }

// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, ULEffect4f(from + i));
}

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

using Handler = scenario_harness::Handler;

// The entry `state` of a kind's table (read in place: the fuzz swaps the cells
// for recorders); a Fatal past the table's own length, where the original jumps
// (or calls) through the dword after - the next kind's table.
Handler Entry(const char* who, U table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4f.md section 6)",
                    who, state, entries, (unsigned)table);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect4f(table + 4 * state)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]
void DispatchEffect4f(const char* who, U table, unsigned entries) { Entry(who, table, entries)(); }

// The draw-mode packet the draws open with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawMode(U abr, int x, int y, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

using move_script::Word;

void SetULRest3g(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetULRest3g(U a, U v) { SetULRest3g(At(a), v); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

// sar on the 32 bits.
U SarRest3g(U v, unsigned n) { return static_cast<U>(I(v) >> n); }

}  // namespace

// original 0x493E10 (hidden in 0x493C60): jmp [EffectKindBA_States + +1 * 4],
// unbounded. States: _Start, 0x493E50 (part 6: the line drawn from the leader's
// ring), Effect_StateRelease.
extern "C" void __cdecl EffectKindBA_Run(void) {
    DispatchEffect4f("EffectKindBA_Run", AddressOf(EffectKindBA_States), EffectKindBA_States_count);
}

// original 0x493E30 (state 0): the word +0x2E = 0; +1 up; Sound_PlayEffect(0x215).
extern "C" void __cdecl EffectKindBA_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0);
    NextState();
    SH_CALL(Sound_PlayEffect)(0x215);
}

// original 0x493E50 (state 1; hidden in EffectKindB9_DrawShard 0x493C60): from
// the angle +0x6C, d1 = ((3 sin - cos) * 3 << 13) sar 12 and d2 = ((-sin - 3
// cos) * 3 << 13) sar 12 (four Math calls, each with +0x6C read again); +0x34
// / +0x38 / +0x3C extra sprite 1's x + d1, z + d2, y + 0x1000000; +0xC its x
// - (s16 +0x2E << 15) + d1, +0x10 / +0x14 as +0x38 / +0x3C;
// EffectKindBA_DrawLine(+0x34, +0xC, 0x40); the word +0x2E up; at 0x14A sound
// 0x216 and +1 up.
extern "C" void __cdecl EffectKindBA_Line(void) {
    const int sin1 = SH_CALL(Math_Sin)(static_cast<std::int32_t>(rv::As<records::EffectObject>(S())->angle[2]));
    const U three_sin = static_cast<U>(sin1) * 3u;
    const int cos1 = SH_CALL(Math_Cos)(static_cast<std::int32_t>(rv::As<records::EffectObject>(S())->angle[2]));
    const U d1 = SarRest3g(((three_sin - static_cast<U>(cos1)) * 3u) << 13, 12);
    const int cos2 = SH_CALL(Math_Cos)(static_cast<std::int32_t>(rv::As<records::EffectObject>(S())->angle[2]));
    const U three_cos = static_cast<U>(cos2) * 3u;
    const int sin2 = SH_CALL(Math_Sin)(static_cast<std::int32_t>(rv::As<records::EffectObject>(S())->angle[2]));
    const U d2 = SarRest3g(((0u - static_cast<U>(sin2) - three_cos) * 3u) << 13, 12);
    {
        unsigned char* const s = S();
        rv::As<records::EffectObject>(s)->x = static_cast<std::uint32_t>(Sprite_ObjectsExtra[1].x) + d1;
        rv::As<records::EffectObject>(s)->z = static_cast<std::uint32_t>(Sprite_ObjectsExtra[1].z) + d2;
        rv::As<records::EffectObject>(s)->height = static_cast<std::uint32_t>(Sprite_ObjectsExtra[1].height) + 0x1000000u;
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, static_cast<std::uint32_t>(Sprite_ObjectsExtra[1].x) - (static_cast<U>(S16(s + 0x2E)) << 15) + d1);
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, static_cast<std::uint32_t>(Sprite_ObjectsExtra[1].z) + d2);
        SetULRest3g(s + 0x14, static_cast<std::uint32_t>(Sprite_ObjectsExtra[1].height) + 0x1000000u);
        SH_CALL(EffectKindBA_DrawLine)(reinterpret_cast<const long*>(s + 0x34), reinterpret_cast<const long*>(s + 0xC),
                                       0x40);
    }
    rv::As<records::EffectObject>(S())->screen_x = static_cast<std::uint16_t>(rv::As<records::EffectObject>(S())->screen_x) + 1u;
    if (static_cast<std::uint16_t>(rv::As<records::EffectObject>(S())->screen_x) != 0x14A) return;
    SH_CALL(Sound_PlayEffect)(0x216);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x493F70 (cdecl; kind 0xBA's state 1 0x493E50 draws it with the
// record's +0x34 and +0xC and 0x40): a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// dtd 1, committed to slot 1); the map camera; a semi-transparent LINE_F2 from
// `from` projected to `to` projected, (shade, shade, 0), committed 0x20 to slot 1.
extern "C" void __cdecl EffectKindBA_DrawLine(const long* from, const long* to, unsigned shade) {
    DrawMode(1, 0x3C0, 0, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    Copy12(p + 8, o);
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    Copy12(p + 0x14, o);
    p[4] = static_cast<unsigned char>(shade);
    p[5] = static_cast<unsigned char>(shade);
    p[6] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
}

void EffectKindBa_InjectFromEffect4f() {
    BOF3_INJECT(EffectKindBA_Run);
    BOF3_INJECT(EffectKindBA_Start);
    BOF3_INJECT(EffectKindBA_DrawLine);
}

void EffectKindBa_InjectFromRest3g() {
    BOF3_INJECT(EffectKindBA_Line);
}
