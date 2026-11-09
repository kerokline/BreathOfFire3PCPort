// Round thirteen group E6C (docs/effect_6c.md): the cut's 48 rows of group E6C,
// 0x510C90..0x51426B, less one start that is a case of its host's switch
// (0x5124C0, inside EffectKind18Sub51_Wait), plus three its band holds that no
// group's list did - sub-kind 0x51's third state 0x512510 and the dispatchers
// of sub-kinds 0x59 and 0x5B (0x512B20, 0x513CD0): 50 functions, each read with
// capstone to its last instruction (2026-10-03). Effect_RunObjects (ours) makes
// each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current and
// calls Effect_KindHandlers[+5]; kind 0x18's EffectKind18_Run jumps through
// EffectKind18_States by +1 (the sub-kind). What each is, as far as the code
// says (what the game shows is the owner's to say):
//
//   sub-kind 0x44   (E6B's dispatcher 0x510C20, whose state 1 calls the first
//                   and jumps to the second) the record kept on the leader's
//                   animation and projected at Field_Kind2's point, put on the
//                   sprite list; on Cond_ByteFE 1 its colour cells set, on 2 a
//                   red additive fill over the frame - DIV-0041's (below). Then
//                   a screen-space sky: bands and two rows of three textured
//                   quads tinted by area 189's frame word (0x90405C, 0..0x3BF:
//                   Gfx_ClutAdjust by its quarter), stars, two glows placed by
//                   the camera's angle and distance from two map points
//   sub-kind 0x45   a ring of 32 textured quads round a fixed point, opened
//                   outward 8 a frame; then eight rings spreading one after
//                   another; at the end Cond_ByteFE 2 and the record released
//   sub-kind 0x51   a rectangle of map cells (one of five, by +0x36) whose
//                   records' bits 19..23 are cleared, then - once a flag or
//                   Cond_ByteFE says - stepped up to 0x10 and the record released
//   sub-kind 0x53   sixteen rings of spinning dots near the record while the
//                   camera's point is within 20 cells of it
//   sub-kind 0x55   a field of 66 textured squares over cells (3..13, 0x71..
//                   0x76) on a checkerboard, shading out once flag 0xE is set
//   sub-kind 0x59   a sixteen-sided ring of two-quad columns and glows, its
//                   shades faded in a cascade through twelve .data bytes; a
//                   texture-page move (DR_MOVE) each frame
//   sub-kind 0x5A   map cells rewritten in a sweep, then two texture moves
//                   cycling through four rows
//   sub-kind 0x5B   three groups of the 49 textured pieces of 0x65EF98 slid
//                   apart and lifted, story flag 0x81 set at the end
//   sub-kind 0x66   two layers of a 2 x 2 grid of 256-pixel textured quads,
//                   scrolled and faded by +9
//   0x511C10        the map's height at (x, z) from the corner table - called
//                   by area 189's code (ours), not effect code
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. Sprite_Current is read
// again wherever the original reads [0x937F88] again after a call. The draw-item
// array is read through draw_pool::Items() (DIV-0062: the original's immediates
// in this band are re-aimed at inject). Sub-kind 0x44's red fill is DIV-0041's
// (section 3c): its tile is (Widescreen_FillX(), 0) Widescreen_FillWidth() x
// 240, the original's (0, 0) 320 x 240 until Widescreen_ArmFills has run and
// whenever the picture is narrow. Otherwise no divergence: each is a faithful
// replacement. Where the original jumps through a state table past its end,
// indexes a table of the image past its room, divides by zero or indexes past
// the draw-item pool, ours aborts with a message (docs/effect_6c.md section 7).
//
// x87: the game runs its FPU at 53 bits; every fadd / fsub / fmul here is
// written in double (the same rounding) and stored to a float once, as the
// original's fstp does; a float the original only loads and stores goes
// through CopyQuiet (x87 turns a signalling NaN quiet on the load).
#include "game/effect_6c.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_6c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6c::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U ULEffect6c(const unsigned char* p) { return static_cast<U>(Long(p)); }
U ULEffect6c(U a) { return ULEffect6c(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
void SetUL(U a, U v) { SetUL(At(a), v); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S16(U a) { return S16(At(a)); }
std::int32_t S8(U a) { return static_cast<signed char>(*At(a)); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
U AddressOf(const void* p) { return Low32(p); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// cdq; and edx, 2^n - 1; add eax, edx; sar eax, n: a signed divide by 2^n,
// toward zero.
U DivP2(U v, unsigned n) { return Sar(v + (I(v) < 0 ? (1u << n) - 1u : 0u), n); }
// cdq; xor eax, edx; sub eax, edx.
U Abs(U v) { return I(v) < 0 ? 0u - v : v; }

float F(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return f;
}
float F(U a) { return F(At(a)); }
double D(U a) { return static_cast<double>(F(a)); }
void SetF(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }
void SetD(unsigned char* p, double d) { SetF(p, static_cast<float>(d)); }
void SetD(U a, double d) { SetD(At(a), d); }
// fild dword: a whole number, exact in double.
double Fi(U v) { return static_cast<double>(I(v)); }
// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000; the callers keep eax's low word.
U Ftol(double v) {
    if (!(v > -9.2233720368547758e18 && v < 9.2233720368547758e18)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
void SetV(unsigned offset, U v) { SetWord(Vertices() + offset, v); }
const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }
float* Fp(unsigned char* p) { return reinterpret_cast<float*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6c.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect6c(At(table + 4 * sub))))();
}
#define E6C_TABLE(name) AddressOf(name), name##_count

// A draw item by index: the pool's (DIV-0062), a Fatal past its count where the
// original reads past the array.
unsigned char* Item(const char* who, U item) {
    if (item >= draw_pool::Count())
        bof3::Fatal("%s: draw item %u, past the pool's %u - the original reads past DrawItems (docs/effect_6c.md "
                    "section 7)",
                    who, (unsigned)item, draw_pool::Count());
    return draw_pool::Items() + item * at::kDrawItemStride;
}

// The dword record of the map cell (x, z) at `run` records into its run, as
// the original indexes it: the cell's word at AreaMap_Header + (x + width * z
// + 2 * base) * 2, plus base, plus (height * width + 1) / 2 - every byte and
// word re-read where the original re-reads them.
unsigned char* CellRecord(U x, U z, U run) {
    const U header = ULEffect6c(::bof3::Img(at::kAreaHeader.v));
    const U width = header & 0xFF;
    const U base = Word(At(::bof3::Img(AreaMap_Header_addr) + 0x2));
    const U word = Word(At(::bof3::Img(AreaMap_Header_addr) + 2 * (x + width * z + 2 * base)));
    const U height = (header >> 8) & 0xFF;
    const U half = (height * width + 1) / 2;   // cdq; sub eax, edx; sar 1 - never negative here
    return At(::bof3::Img(AreaMap_Header_addr) + 4 * (base + word + half + run));
}

}  // namespace

// ===========================================================================
// 0x511C10: the map's height at a point (area 189's, not effect code)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x44 (E6B's dispatcher 0x510C20; its state 1, 0x510C80, calls the
// first and tail-jumps to the second): the record on the leader, and the sky
// ===========================================================================

namespace {

// A rect of four words allocated at the cursor (the cursor up 8), as the sky
// and sub-kind 0x66 do before a draw mode that names it.
unsigned char* Rect(U x, U y, U w, U h) {
    unsigned char* const r = Gfx_PacketNext;
    Gfx_PacketNext = r + 8;
    SetWord(r, x);
    SetWord(r + 2, y);
    SetWord(r + 6, h);
    SetWord(r + 4, w);
    return r;
}

}  // namespace

// ===========================================================================
// Sub-kind 0x45: EffectKind18_States[0x45] (0x654180), EffectKind18Sub45_States
// (three) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x51: EffectKind18_States[0x51] (0x6541B0), EffectKind18Sub51_States
// (three) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x53: EffectKind18_States[0x53] (0x6541B8) - one state
// ===========================================================================

// ===========================================================================
// Sub-kind 0x55: EffectKind18_States[0x55] (0x6541C0), EffectKind18Sub55_States
// (three: WeretigerFx_Next, then these two) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x59: EffectKind18_States[0x59] (0x6541D0), EffectKind18Sub59_States
// (six) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x66: EffectKind18_States[0x66] (0x654204) - one state
// ===========================================================================

// ===========================================================================
// Sub-kind 0x5A: EffectKind18_States[0x5A] (0x6541D4), EffectKind18Sub5A_States
// (six) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x5B: EffectKind18_States[0x5B] (0x6541D8), EffectKind18Sub5B_States
// (eight) by +2
// ===========================================================================

void Effect6C_Inject() {
    if (bof3::WantsShadow("effect_6c")) effect_6c::SelfTest();
}
