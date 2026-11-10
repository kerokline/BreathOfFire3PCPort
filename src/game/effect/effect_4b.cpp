// Round thirteen group E4B (docs/effect_4b.md): the 61 functions of
// analysis/round13_cut.tsv's group E4B, 0x489030..0x48B1FC, and five starts the
// cut does not list - kind 0x87's pane draw 0x489390 (the cut ran 0x489220 over
// it) and its midpoint helper 0x489630, kind 0xA4's state 2 0x48A550, and the
// two shared tails with their own ret that kinds 0x9F and 0xA4 jump to,
// 0x48A480 and 0x48A560 - each read with capstone to its last instruction
// (2026-10-03). Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says:
//
//   kind 0x87   (E4A's dispatcher and states) six shaded panes of four
//               POLY_G3s over fourteen projected points, stepped through four
//               cases - the helpers here
//   kind 0x88   thirty-two particles (0x2C bytes at EffectKind30_Shards)
//               emitted every eighth frame from the record's point toward
//               +0xC.., each a ring of sixteen POLY_G3s, until extra object
//               0's x reaches 0x3C8000 / 0x408000
//   kind 0x89   a full-screen tile (EffectKind89_DrawTint) at 0x80 grey, the
//               party at pose 3, faded out by +1 = 2
//   kind 0x9D   a textured quad of a field object's (+0xB) at its projected
//               point, pulsing (+6 = 0) or growing and shrinking
//   kind 0x9F   the tile brightened to 0x7C, held, faded, the party tinted
//               against it
//   kind 0xA4   the tile brightened to white, the field objects of record
//               words 0x261 / 0x308 redrawn over it
//   kind 0x8A   three lines (kind 0x6F's segment and ribbon, E3C's, at the
//               record's +0xC) along x 0x10, the six cells under them blocked
//               while it lives
//   kind 0x8B   the leader's two poses 0x6F / 0x6E by the counter 0x903849
//   kind 0x8C   a count of pad presses in two timed windows, a steered step
//               of the leader between them, judged against a kept count
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again. The full-screen
// fill of 0x489CD0 (EffectKind89_DrawTint, the site 0x489D47) is DIV-0041's
// (section 3c): it draws at (Widescreen_FillX(), 0) Widescreen_FillWidth() x
// 240, which is the original's (0, 0) 320 x 240 until Widescreen_ArmFills has
// run and whenever the picture is narrow. Kind 0x9D's start clears the
// rectangle its quad reads before the capture lands there - DIV-0142's
// amendment for issue #191, armed after this module's self-test (the switch
// SpriteCapture). Kind 0x8C's losing state 0x16 (EffectKind8C_Fail) ends
// as its passing state 0x15 does while BOF3X_NO_FAIL is on (DIVERGENCE
// DIV-0188, the well). Otherwise no divergence: each is a
// faithful replacement. Where the original jumps through a state
// table past its end, indexes a table of its own past its rows, or ObjTrio or
// Sprite_Objects past their records, ours aborts with a message
// (docs/effect_4b.md section 6).
#include "game/effect_4b.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect_4b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/sprite_draw.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_4b::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
}  namespace {
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }
U AddressOf(const void* p) { return Low32(p); }
U Shards() { return AddressOf(EffectKind30_Shards); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul of two whole registers: the low 32 bits.
U Mul(U a, std::int32_t b) { return a * static_cast<U>(b); }
unsigned char& Byte(U cell) { return At(cell)[0]; }

// `fld dword [a]; fsub dword [c]; fstp dword [o]`
void FldSub(const void* a, const void* c, void* o) {
    __asm__ volatile("flds (%0)\n\tfsubs (%1)\n\tfstps (%2)" : : "r"(a), "r"(c), "r"(o) : "st", "memory");
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the
// dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4b.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// The draw mode most of these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page) - its fifth argument is
// the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

// ObjTrio record m; past the three, ours aborts (the original writes what
// follows).
unsigned char* Member(unsigned m, const char* who) {
    if (m >= at::kObjTrioCount)
        bof3::Fatal("%s: party member %u, past ObjTrio's three records - the original writes what follows "
                    "(docs/effect_4b.md section 6)",
                    who, m);
    return rv::Bytes(&ObjTrio[m]);
}

}  // namespace

// ===========================================================================
// Kind 0x87's helpers (its dispatcher 0x488F60 and states are E4A's)
// ===========================================================================

// ===========================================================================
// Kind 0x88: Effect_KindHandlers[0x88] (0x655570), EffectKind88_States (four)
// ===========================================================================

// ===========================================================================
// Kind 0x89: Effect_KindHandlers[0x89] (0x655574), EffectKind89_States (four;
// entry 3 BareRet) - and the screen tile kinds 0x89, 0x9F and 0xA4 draw
// ===========================================================================

// ===========================================================================
// Kind 0x9D: Effect_KindHandlers[0x9D] (0x6555C4), EffectKind9D_States (four)
// ===========================================================================

// ===========================================================================
// Kind 0x9F: Effect_KindHandlers[0x9F] (0x6555CC), EffectKind9F_States (four);
// kind 0xA4: Effect_KindHandlers[0xA4] (0x6555E0), EffectKindA4_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x8A: Effect_KindHandlers[0x8A] (0x655578), EffectKind8A_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x8B: Effect_KindHandlers[0x8B] (0x65557C), EffectKind8B_States (four)
// ===========================================================================

// ===========================================================================
// Kind 0x8C: Effect_KindHandlers[0x8C] (0x655580), EffectKind8C_States (23;
// entries 2, 5..9, 14 and 19 BareRet)
// ===========================================================================

void Effect4B_Inject() {
    if (bof3::WantsShadow("effect_4b")) effect_4b::SelfTest();
}
