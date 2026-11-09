// Effect kinds 0x3E, 0x3F, 0x40, 0x42, 0x43, 0x44 and 0x6B - round thirteen,
// wave two, group E2C: the 51 functions of the cut (analysis/round13_cut.tsv)
// in 0x474F40..0x4771A5 and the two its span holds that no list has (0x476560,
// a callee of kind 0x43's; 0x476680, kind 0x44's dispatcher), each read with
// capstone to its last instruction (docs/effect_2c.md section 1).
// Effect_RunObjects (ours) makes each live record of Effect_Objects (20 of 0x80
// bytes) Sprite_Current and calls Effect_KindHandlers[+5]; each kind here is a
// dispatcher by +1 through its state table (none bounded by a compare) and the
// states it names:
//
//   kind 0x3E  four lightning bolts between fixed cells, a sound while
//              Field_Request is 5 (Area145_Tail20 spawns it);
//   kind 0x3F  a beam between two world points for one frame, then
//              Effect_StateRelease (Area145_SpawnTrail);
//   kind 0x40  a disc growing round its point with 32 shards falling inside it
//              until the counter byte 0x903848 is 0x19 (Area28_SpawnEffect40);
//   kind 0x42  a glow cylinder at its point (Area146_DrawGlowCylinder);
//   kind 0x43  256 sparks round a fixed cell: they rise, circle and fly out,
//              phase by phase (+6), until none is left (chapter 10's run 13);
//   kind 0x44  a ring of light on sprite 1 fading in, widening, following it
//              with a cone and trailing sparks, fading out (Area78_SpawnEffect44);
//   kind 0x6B  sprite 2 drawn off screen through a borrowed record, read back
//              from the VRAM shadow and crumbled into falling pixels (chapter
//              10's run 13).
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. One divergence,
// DIV-0186: kind 0x6B's particles live in a room of ours
// (EffectKind6B_Particles below); otherwise each is a faithful replacement. Every x87 operation is done in x87 (long
// double, loads through `flds`) as the original does it, so rounding and NaNs
// come out alike. Where the original jumps through a state table past its end,
// divides by 0, indexes kind 0x6B's column heights past their 80 or loops
// forever, ours aborts with a message (docs/effect_2c.md section 6).
#include "game/effect_2c.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2c::at;
using U = std::uint32_t;
using LD = long double;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U ULEffect2c(const unsigned char* p) { return static_cast<U>(Long(p)); }
U ULEffect2c(U a) { return ULEffect2c(At(a)); }
void SetULEffect2c(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
void SetULEffect2c(U a, U v) { SetULEffect2c(At(a), v); }
std::int32_t SWEffect2c(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t SWEffect2c(U a) { return SWEffect2c(At(a)); }
unsigned char& B(U a) { return At(a)[0]; }
U AddressOfEffect2c(const void* p) { return Low32(p); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul r32, r32 then sar: the product's low 32 bits, shifted.
U MulSarEffect2c(std::int32_t a, std::int32_t b, unsigned n) { return Sar(static_cast<U>(a) * static_cast<U>(b), n); }
// cdq; and edx, 2^n - 1; add eax, edx; sar eax, n: a signed divide toward zero.
std::int32_t DivPow2(U v, unsigned n) { return static_cast<std::int32_t>(v) / (1 << n); }
// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetULEffect2c(to + i, ULEffect2c(from + i));
}

// --- x87 as the original has it: `fld dword` through inline assembly, so the
// optimizer cannot narrow an operation of two floats to SSE (whose NaN rule
// differs); every operation in long double on the x87 at the game's control
// word, `fstp dword` on the way out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}
LD FAt(U a) { return F(At(a)); }
LD I(std::int32_t v) { return static_cast<LD>(v); }
void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
// The CRT's _ftol 0x5B9550: truncation to 64 bits, the low dword; NaN and
// values out of range give the integer indefinite 0x8000000000000000, whose
// low dword is 0.
U Ftol(LD v) {
    if (!(v > -9223372036854775808.0L && v < 9223372036854775808.0L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + k]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, const void* table, unsigned entries, unsigned state) {
    if (state >= entries)
        bof3::Fatal("%s: state byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2c.md section 6)",
                    who, state, entries, (unsigned)AddressOfEffect2c(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(ULEffect2c(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// The draw-mode packet every draw here opens with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawMode(U abr, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

}  // namespace
// DIVERGENCE DIV-0186 (D217, issue #163): kind 0x6B's particles. The original
// writes one 0x14-byte particle per opaque pixel of the 80 x 72 read-back at
// 0x92EC80 with no bound; the room before Gfx_CurrentEnv (0x937F84) holds
// 1,881, and the 1,882nd overwrites Sprite_Current - the crash of issue #163,
// measured 2026-10-29 on the owner's gaistFullRoute: the Gaist's collapsed
// body in record 2, 2,201 pixels. Ours: once g_ash_room is armed (after the
// self-test, PatchBytes "AshRoom") the particles go to a room of ours that
// holds the rectangle's every pixel (5,760), so the whole sprite crumbles;
// the count, the order, the fields and the draw are the original's. Nothing
// but these three states names the particles (a raw scan of the image: 0x92EC80
// only at 0x475F76, 0x4760E7 and, as +1, 0x476145), and the frame 0x6544A8
// that sizes them is read and never written. BOF3X_ORIGINAL=AshRoom (or
// BOF3X_OFF) leaves them at 0x92EC80, the overrun with them.
namespace effect_2c { unsigned char g_ash_room = 0; }  namespace {

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }
const long* Point(const unsigned char* p) { return reinterpret_cast<const long*>(p); }
float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

}  // namespace

// ===========================================================================
// Kind 0x3E: Effect_KindHandlers[0x3E] (0x655448), EffectKind3E_States (two)
// and its sub-state table EffectKind3E_SubStates (two)
// ===========================================================================

// ===========================================================================
// Kind 0x3F: Effect_KindHandlers[0x3F] (0x65544C), EffectKind3F_States (two,
// the second Effect_StateRelease)
// ===========================================================================

// ===========================================================================
// Kind 0x40: Effect_KindHandlers[0x40] (0x655450), EffectKind40_States (three,
// the third Effect_StateRelease)
// ===========================================================================

// ===========================================================================
// Kind 0x42: Effect_KindHandlers[0x42] (0x655458), EffectKind42_States (two:
// 0x47EEC0, group E2G's, then ours)
// ===========================================================================

// ===========================================================================
// Kind 0x43: Effect_KindHandlers[0x43] (0x65545C), EffectKind43_States (four)
// ===========================================================================

// ===========================================================================
// Kind 0x6B: Effect_KindHandlers[0x6B] (0x6554FC), EffectKind6B_States (five)
// ===========================================================================

// ===========================================================================
// Kind 0x44: Effect_KindHandlers[0x44] (0x655460), EffectKind44_States (six)
// ===========================================================================

void Effect2C_Inject() {
    if (bof3::WantsShadow("effect_2c")) effect_2c::SelfTest();
    // DIV-0186: armed after the self-test, which compares ours with Capcom's at
    // 0x92EC80.
    static const std::uint8_t ash_was = 0, ash_is = 1;
    bof3::PatchBytes("AshRoom", AddressOfEffect2c(&effect_2c::g_ash_room), &ash_was, &ash_is, 1);
    bof3::Log("DIV-0186    kind 0x6B's particles in a room of %u (the original's holds 1,881): %s", at::kPixelRoom,
              effect_2c::g_ash_room ? "on" : "OFF (BOF3X_ORIGINAL=AshRoom)");
}
