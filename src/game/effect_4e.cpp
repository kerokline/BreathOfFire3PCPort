// Round thirteen group E4E (docs/effect_4e.md): the 48 functions of
// analysis/round13_cut.tsv's group E4E, 0x48DF90..0x491C96, and kind 0xA3's
// dispatcher 0x4912F0 that no list held, each read with capstone to its last
// instruction. Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says (the spawners: area 100's tail and
// chapter 15's runs, area_w2d.cpp and scena_sc15.cpp):
//
//   kind 0x9B  a ring of 16 semi-transparent POLY_G4 round the record's
//              point, linked into the map, that rises (its height +0x14
//              accelerating) to 0x8000000 and stays
//   kind 0x9C  at a fixed place (0x308000, 0x740000): a glow and a trail
//              (EffectKind9C_DrawTrail) that grow and hold; eight panels
//              that flicker in; five beams (LINE_F2, two fans and two quads
//              each); the glow shrinking; the beams fading; released
//   kind 0x9E  a dispatcher here (its seven states are catalog part 6 rows)
//              and four quads those states draw round the record's point:
//              a white plate, a textured plate, a wall and a shaded plate
//   kind 0xA0  a glow at the leader's point that grows, holds and rises;
//              thirty-two shards and sixteen sparks of its own pools
//              (0x6762B0, 0x676430); then a trail flown from the glow to
//              (0x2F8000, 0x278000, 0xF000000) until the chapter's count
//              reaches 0x17; then a fade
//   kinds 0xA1, 0xA2, 0xA3, 0xA7, 0xA8, 0xA9  dispatchers only (their
//              states are catalog part 6 rows); 0xA7's and 0xA9's call
//              through their tables and then draw a glow / a disc while +1
//              is not 0
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Each is a
// faithful replacement; where the original jumps through a state table past
// its end, or its trail's dot loop would run until the coordinate wraps, ours
// aborts with a message (docs/effect_4e.md section 6). The x87 arithmetic is
// done in long double, which the compiler keeps on the x87 at the control
// word's precision, as the originals' fild / fadd / fsub / fstp chains are.
#include "game/effect_4e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4e_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4e::at;
using U = std::uint32_t;
using LD = long double;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t SL(const unsigned char* p) { return Long(p); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S16(U v) { return static_cast<std::int16_t>(v); }
U AddressOf(const void* p) { return Low32(p); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul r32, r32 then sar: the product's low 32 bits, shifted.
U MulSar(int a, U b, unsigned n) { return Sar(static_cast<U>(a) * b, n); }
// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, UL(from + i));
}

// --- x87 as the original has it: `fld dword` through inline assembly, so the
// optimizer cannot narrow an operation of two floats to SSE; every operation
// in long double on the x87 at the game's control word, `fstp dword` out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}
LD I(U v) { return static_cast<LD>(static_cast<std::int32_t>(v)); }
void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }
float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }
const unsigned char* Bytes(const long* p) { return reinterpret_cast<const unsigned char*>(p); }
unsigned char* Bytes(long* p) { return reinterpret_cast<unsigned char*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original
// jumps through the dword after - the next kind's table or data.
void DispatchEffect4e(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4e.md section 6)",
                    who, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// Gpu_GetTPage(0, abr, x, y), then Gpu_SetDrawMode(cursor, 0, dtd, the page's
// low word, 0 - the fifth word the leftover of GetTPage's five pushes).
void SetMode(U abr, int x, int y, int dtd) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
}
// The same, committed 0xC to `slot`.
void DrawMode(U abr, int x, int y, int dtd, U slot) {
    SetMode(abr, x, y, dtd);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}
// +9 down one; true when it reached 0 (Sprite_Current read for each access,
// as the originals).
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
}

}  // namespace

// ===========================================================================
// Kind 0x9B: Effect_KindHandlers[0x9B] (0x6555BC), EffectKind9B_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x9C: Effect_KindHandlers[0x9C] (0x6555C0), EffectKind9C_States (eight)
// ===========================================================================

// ===========================================================================
// Kind 0x9E: Effect_KindHandlers[0x9E] (0x6555C8), EffectKind9E_States
// (seven, catalog part 6 rows and Effect_StateRelease), and the four quads
// its states 1..4 draw round the record's point `a` (+0x34) with the
// half-extent `b` (+0xC, or the cells 0x6762A0): +6 bit 0 says which of x / z
// the second and third corners take a's sum on.
// ===========================================================================

// ===========================================================================
// Kind 0xA0: Effect_KindHandlers[0xA0] (0x6555D0), EffectKindA0_States (nine)
// ===========================================================================

// ===========================================================================
// Kinds 0xA1, 0xA2, 0xA3, 0xA7, 0xA8, 0xA9: dispatchers whose states are
// catalog part 6 rows (in no group of round thirteen)
// ===========================================================================

void Effect4E_Inject() {
    if (bof3::WantsShadow("effect_4e")) effect_4e::SelfTest();
}
