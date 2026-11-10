// Round thirteen group E3A (docs/effect_3a.md): the 48 functions of
// analysis/round13_cut.tsv's group E3A, 0x4801F0..0x4823C2, each read with
// capstone to its last instruction. Effect_RunObjects (ours) makes each live
// record of Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says (the spawners are chapter 8's run 11 and
// chapter 10's run 2, scena_sc7.cpp and scena_sc9b.cpp):
//
//   kind 0x60  a dispatcher only here (its states EffectKind60_Start..EffectKind60_SlideEnd are
//              catalog part 6 rows in no group), and the grey LINE_G2 its
//              states 1 and 2 draw between two projected points
//   kind 0x61  a party member (ObjTrio record +7) drawn off screen into VRAM
//              (0x340, 0x100) through a borrowed record of kind 0x3A, read
//              back, every opaque pixel made a particle, the particles flown
//              in (+6 = 0) or out (+6 not 0) as flickering TILE_1s
//   kind 0x62  24 orange rays shot from the record's point, each a LINE_G2
//              that runs out and draws in, until message 0x12 at the
//              chapter's step 0xC; then a widening orange ring (32 POLY_G4)
//   kind 0x64  a glow (32 POLY_G3) at the leader's point that grows, holds and
//              rises; sixteen shards (E4F's draw) and eight sparks; a trail
//              of two POLY_G4 and eight TILE_1 dots flown from it until the
//              chapter's count reaches 0x18; then a fade
//   kind 0x68  sixteen motes (16 POLY_G3 discs each) rising round the
//              record's point through a three-state table of their own; then
//              a wall of four POLY_F4 linked into the map that brightens
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. One
// divergence, forced: EffectKind64_DrawGlow's rim vertices take their depth
// from a stack word the original never writes; ours gives them the centre's
// (DIV-0068). Otherwise each is a faithful replacement; where the original
// jumps through a state table past its end, indexes ObjTrio, kind 0x61's
// frames or the effect records past their end, or loops for ever, ours aborts
// with a message (docs/effect_3a.md section 6). The x87 arithmetic is done in
// long double, which the compiler keeps on the x87 at the control word's
// precision, as the originals' fild / fadd / fsubr / fiadd / fstp chains are.
#include "game/effect_3a.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3a_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3a::at;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U AddressOf(const void* p) { return Low32(p); }
// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, UL(from + i));
}

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table.
void DispatchEffect3a(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3a.md section 6)",
                    who, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// The draw-mode packet the draws open with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawMode(U abr, int x, int y, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

}
// ===========================================================================
// Kind 0x60: Effect_KindHandlers[0x60] (0x6554D0), EffectKind60_States (six)
// ===========================================================================

// ===========================================================================
// Kind 0x61: Effect_KindHandlers[0x61] (0x6554D4), EffectKind61_States (five)
// ===========================================================================

// ===========================================================================
// Kind 0x62: Effect_KindHandlers[0x62] (0x6554D8), EffectKind62_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x64: Effect_KindHandlers[0x64] (0x6554E0), EffectKind64_States (nine)
// ===========================================================================

// ===========================================================================
// Kind 0x68: Effect_KindHandlers[0x68] (0x6554F0), EffectKind68_States (four)
// and the motes' EffectKind68_MoteStates (three, called with the mote)
// ===========================================================================

void Effect3A_Inject() {
    if (bof3::WantsShadow("effect_3a")) effect_3a::SelfTest();
}
