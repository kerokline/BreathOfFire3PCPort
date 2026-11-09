// Effect kinds 0x4F, 0x50, 0x51, 0x52, 0x53, 0x56 and kind 0x4E's disc - round
// thirteen, wave two, group E2F: the 52 functions of the cut
// (analysis/round13_cut.tsv) in 0x47B7D0..0x47DBD1, the ten more of the band
// that are states or callees of the same kinds (0x47C0F0, 0x47C190, 0x47C320,
// 0x47C4A0, 0x47CF20, 0x47CF40, 0x47CFC0, 0x47CFF0, 0x47D010, 0x47D8B0), each
// read with capstone to its last instruction (docs/effect_2f.md section 1).
// Effect_RunObjects (ours) makes each live record of Effect_Objects (20 of 0x80
// bytes) Sprite_Current and calls Effect_KindHandlers[+5]; each kind here is a
// dispatcher by +1 through its state table (none bounded by a compare) and the
// states it names:
//
//   kind 0x4E  (E2E's) draws the disc here, EffectKind4E_DrawDisc;
//   kind 0x4F  two sprites tinted and set apart (EffectKind3C_Start's twin with
//              its own event op), then FC1's EffectKind3C_Hold and E2D's state;
//   kind 0x50  specks thrown from the record's point in a turning direction,
//              64 of 0x28 bytes at EffectKind30_Shards, each a textured quad;
//   kind 0x51  a column of quads swaying on a history of 65 angles, which
//              grows, waits on the counter byte 0x903848, shrinks, then a ring
//              of sixteen quad pairs closing or opening;
//   kind 0x52  sparks (8 of 0x1C at EffectKind30_Shards) rising round the
//              record, then a trail of 32 points at 0x92C060 that the record
//              drags along two fixed paths, drawn as Gouraud quads with caps;
//   kind 0x53  a textured beam from the top of the screen down onto an extra
//              sprite, between two texture-window primitives;
//   kind 0x56  a column of red crosses from Sprite_ObjectsExtra[0]'s height
//              down to the ground, armed and stopped by the counter byte
//              0x90384B.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, indexes past a pool or a table, or loops for ever, ours
// aborts with a message (docs/effect_2f.md section 2). The x87 arithmetic is
// done in long double, which the compiler keeps on the x87 at the control
// word's precision, as the originals' fld / fadd / fsub / fstp chains are.
#include "game/effect_2f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2f_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2f::at;
using U = std::uint32_t;
using LD = long double;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U S16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v))); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
U AddressOf(const void* p) { return Low32(p); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul then sar 12: a trigonometric answer scaled by a length.
U Scale(int trig, U length) { return Sar(static_cast<U>(trig) * length, 12); }

// --- x87 as the original has it -------------------------------------------------
// `fld dword [p]`: the float widened (a signalling NaN comes out quiet, as fld
// makes it).
LD X(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return static_cast<LD>(f);
}
// `fild dword`: an int widened, exactly.
LD XI(U v) { return static_cast<LD>(static_cast<std::int32_t>(v)); }
// `fstp dword [p]`: rounded to a float at the control word's rounding.
void Fstp(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
// `fld dword [src]; fstp dword [dst]`: a copy through the FPU (a signalling NaN
// quietened), kept in assembly so the compiler cannot fold it into a move.
void FCopy(unsigned char* dst, const unsigned char* src) {
    __asm__ volatile("flds (%1)\n\tfstps (%0)" : : "r"(dst), "r"(src) : "st", "memory");
}
// The CRT's _ftol 0x5B9550 on st(0): truncation to 64 bits in a copy of the
// control word, the low dword kept; NaN and out-of-range answer the integer
// indefinite 0x8000000000000000, whose low dword is 0.
U Ftol(LD v) {
    if (!(v > -9223372036854775809.0L && v < 9223372036854775808.0L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// --- the tables and the pools ------------------------------------------------------

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Jump(const char* who, U table, unsigned entries, unsigned index) {
    if (index >= entries)
        bof3::Fatal("%s: state byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2f.md section 2)",
                    who, index, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * index))))();
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// Gpu_GetTPage(tp, abr, x, y), then the draw mode of that page at the packet
// cursor (dfe 0, dtd as given), committed to slot `slot` (0xC bytes).
void DrawMode(unsigned tp, unsigned abr, int x, int y, int dtd, unsigned slot) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(tp, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

}  // namespace

// ===========================================================================
// Kind 0x4E's disc (E2E owns the kind; its states 1..3 call this)
// ===========================================================================

// ===========================================================================
// Kind 0x4F: Effect_KindHandlers[0x4F] (0x65548C), EffectKind4F_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x50: Effect_KindHandlers[0x50] (0x655490), EffectKind50_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x51: Effect_KindHandlers[0x51] (0x655494), EffectKind51_States (twelve)
// ===========================================================================

// ===========================================================================
// Kind 0x52: Effect_KindHandlers[0x52] (0x655498), EffectKind52_States (two)
// ===========================================================================

// ===========================================================================
// Kind 0x53: Effect_KindHandlers[0x53] (0x65549C), EffectKind53_States (two)
// ===========================================================================

// ===========================================================================
// Kind 0x56: Effect_KindHandlers[0x56] (0x6554A8), EffectKind56_States (four)
// ===========================================================================

void Effect2F_Inject() {
    if (bof3::WantsShadow("effect_2f")) effect_2f::SelfTest();
}
