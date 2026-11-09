// Round thirteen group E4F (docs/effect_4f.md): the 45 functions of
// analysis/round13_cut.tsv's group E4F, the one its band holds that no list has
// (0x492AF0) and three unplaced rows of the band that serve its kinds (0x492530,
// 0x492750, 0x492CF0) - 49, 0x491D70..0x494026, each read with capstone to its
// last instruction. Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare). What each kind is, as far as the code
// says (spawners of ours: kind 0xAC area 198's Area198_Shake, 0xB9 area 188's
// Area188_SpawnEffectB9, 0xBA area 121's Area121_SpawnEffectBA; the others'
// spawners are not ours):
//
//   kind 0xAA  a dispatcher only here (its three states are catalog part 6)
//   kind 0xAB  a copy of kind 0x81 (effect_3d) with eight sources: drops from
//              the cells EffectKindAB_SourceCells gives, each a red or grey
//              dot and a 3 by 3 tile linked into the map
//   kind 0xAC  a dispatcher only here
//   kind 0xAD  a dispatcher; its state 1 (part 6) draws an arc of 16 POLY_G4
//              between two points (EffectKindAD_DrawArc); Effect_StateNext
//   kind 0xAE  an ellipse of 16 POLY_G3 (Effect_DrawEllipse, which kind 0x20
//              draws too) that sinks, widens, narrows and shrinks away
//   kind 0xAF  a red full-screen tile that brightens and fades, again while
//              Draw_PassFlags is set
//   kind 0xB0  sixteen bars (catalog part 6 draws them) started one by one
//   kind 0xB1  the camera swayed: Camera_Angles[1] up and down by 4 a frame
//              between -0xF0 and 0xF0, the view's elevation with it
//   kind 0xB9  a copy of kind 0x64 (effect_3a): a glow at the leader's point
//              that grows, holds and rises (E4E's glow draw); sixteen shards,
//              eight sparks and a trail - kind 0x64 calls this group's spark
//              and shard helpers too
//   kind 0xBA  a dispatcher, its start, and the line its state 1 (part 6) draws
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Each is a
// faithful replacement but kind 0xAF's full-screen tile (EffectKindAF_DrawScreen,
// the site 0x493308), which is DIV-0041's fill (section 3c) and the original's
// 320 x 240 while the fills are unarmed or the picture narrow; where the
// original jumps through a state table past its
// end ours aborts with a message (docs/effect_4f.md section 6). The x87
// arithmetic is the originals' instructions (inline assembly) or long double,
// which the compiler keeps on the x87 at the control word's precision, as the
// originals' fild / fadd / fsub / fstp chains are.
#include "game/effect_4f.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4f_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4f::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U ULEffect4f(const unsigned char* p) { return static_cast<U>(Long(p)); }
U ULEffect4f(U a) { return ULEffect4f(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U SW(const unsigned char* p) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Word(p)))); }
unsigned char& B(U a) { return At(a)[0]; }
U AddressOf(const void* p) { return Low32(p); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, ULEffect4f(from + i));
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }
float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }
const unsigned char* Bytes(const long* p) { return reinterpret_cast<const unsigned char*>(p); }

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
// The dispatchers whose states are all another part's (kinds 0xAA, 0xAC)
// ===========================================================================

// ===========================================================================
// Kind 0xAB: Effect_KindHandlers[0xAB] (0x6555FC), EffectKindAB_States (three):
// a copy of kind 0x81's drops (effect_3d.cpp) with eight sources
// ===========================================================================

// ===========================================================================
// Kind 0xAD: Effect_KindHandlers[0xAD] (0x655604), EffectKindAD_States (five)
// ===========================================================================

// ===========================================================================
// Kind 0xAE: Effect_KindHandlers[0xAE] (0x655608), EffectKindAE_States (five)
// ===========================================================================

// ===========================================================================
// Kind 0xAF: Effect_KindHandlers[0xAF] (0x65560C), EffectKindAF_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0xB0: Effect_KindHandlers[0xB0] (0x655610), EffectKindB0_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0xB1: Effect_KindHandlers[0xB1] (0x655614), EffectKindB1_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0xB9: Effect_KindHandlers[0xB9] (0x655634), EffectKindB9_States (nine):
// a copy of kind 0x64 (effect_3a.cpp)
// ===========================================================================

// ===========================================================================
// Kind 0xBA: Effect_KindHandlers[0xBA] (0x655638), EffectKindBA_States (three)
// ===========================================================================

void Effect4F_Inject() {
    if (bof3::WantsShadow("effect_4f")) effect_4f::SelfTest();
}
