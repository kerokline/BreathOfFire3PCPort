// Round thirteen group E3C (docs/effect_3c.md): the 50 functions of
// analysis/round13_cut.tsv's group E3C, 0x484050..0x485CA0, and one start the
// cut does not list (kind 0x75's shared tail 0x485C60, which two states jump
// to and which has its own frame and ret), each read with capstone to its last
// instruction (2026-10-03). Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says:
//
//   kind 0x6E   sixteen shards (0x28 bytes at EffectKind30_Shards) emitted
//               one every fourth frame from the record's point and risen,
//               shrinking and darkening, by E3B's shard quad; Field_ScriptFlags
//               bit 6 held while any is live
//   kind 0x6D   thirty-two flat triangles (0x28 bytes, at 0x92C780 + 0x500 *
//               +6) thrown from one of two fixed boxes and falling, each a
//               shape of eight by a random byte
//   kind 0x6F   a line of four segments along z at x 0x5C (+1 = 1) or along x
//               at z 0x44 (+1 = 2), each a LINE_F2 and two shaded quads, by
//               story flags 0x5C / 0x5D and Cond_ByteFD; party members on its
//               four cells are turned away and set state 2_8
//   kind 0x72   thirty-two debris triangles (0x2C bytes at EffectKind30_Shards),
//               kind 0x1F's twin
//   kind 0x73   thirty-two textured sparks (0x18 bytes at EffectKind30_Shards):
//               +1 = 0 emitted one every fourth frame and risen; +1 = 1 sixteen
//               at once, spread by their angle byte
//   kind 0x74   a column of textured quads turning round the record's point,
//               rising to eight units over 0x40 frames and fading
//   kind 0x75   a full-screen tile brightened over fifteen frames (E4D's
//               0x48CA90), the field objects at pose 6 redrawn over it
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, indexes a table of its own past its rows or ObjTrio past
// its three records, ours aborts with a message (docs/effect_3c.md section 6).
#include "game/effect_3c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3c::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;
using VoidFn = void (__cdecl*)();

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }
U AddressOf(const void* p) { return Low32(p); }
U Shards() { return AddressOf(EffectKind30_Shards); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul of two whole registers: the low 32 bits.
U Mul(U a, std::int32_t b) { return a * static_cast<U>(b); }

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32 (the original's 0x20-byte local).
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original jumps
// through the dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3c.md section 6)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 (or +2) up, Sprite_Current read afresh.
void Step(unsigned byte) {
    unsigned char* const s = S();
    s[byte] = static_cast<unsigned char>(s[byte] + 1);
}
// +9 down one; answers whether it is now 0 (Sprite_Current read for each access).
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
}

// The draw mode most of these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page) - its fifth argument is
// the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

}  // namespace

// ===========================================================================
// Kind 0x6E: Effect_KindHandlers[0x6E] (0x655508), EffectKind6E_States (four)
// ===========================================================================

// ===========================================================================
// Kind 0x6D: Effect_KindHandlers[0x6D] (0x655504), EffectKind6D_States (two)
// ===========================================================================

// ===========================================================================
// Kind 0x6F: Effect_KindHandlers[0x6F] (0x65550C), EffectKind6F_States (four)
// ===========================================================================

// ===========================================================================
// Kind 0x72: Effect_KindHandlers[0x72] (0x655518), EffectKind72_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x73: Effect_KindHandlers[0x73] (0x65551C), EffectKind73_States (two)
// by +1, each a sub-state dispatcher by +2
// ===========================================================================

// ===========================================================================
// Kind 0x74: Effect_KindHandlers[0x74] (0x655520), EffectKind74_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x75: Effect_KindHandlers[0x75] (0x655524), EffectKind75_States (three)
// ===========================================================================

void Effect3C_Inject() {
    if (bof3::WantsShadow("effect_3c")) effect_3c::SelfTest();
}
