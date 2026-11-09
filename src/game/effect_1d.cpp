// Effect kinds 0x21..0x27 - round thirteen, wave one, group E1D: the 30
// functions of the cut (analysis/round13_cut.tsv) in 0x46F2B0..0x4702F6, each
// read with capstone to its last instruction (docs/effect_1d.md section 1).
// Effect_RunObjects (ours) makes each live record of Effect_Objects (20 of 0x80
// bytes) Sprite_Current and calls Effect_KindHandlers[+5]; each kind here is a
// dispatcher by +1 through its state table (none bounded by a compare) and the
// states it names:
//
//   kind 0x21  an arm: four points round a centre built by 0x46F570 and drawn
//              by 0x46F690 each frame; it grows, lifts, holds, swirls and folds,
//              then marks itself done (+6) and is released;
//   kind 0x22  a sound, five kind-0x21 arms spawned at its point (their
//              records kept at +0xC..+0x1C), then a wait until all five are done:
//              the counter byte 0x903848 up and released;
//   kind 0x23  a sound, then 0x60 frames of kind-0x24 rays, one a frame in four
//              of every eight;
//   kind 0x24  a ray of random direction and colour from the leader (0x46FAE0
//              draws it), shrinking;
//   kind 0x25  a disc at the leader's screen point: grows, glows (a sound at one
//              frame), shrinks;
//   kind 0x26  a band of a dome at its point, widening over 0x50 frames;
//   kind 0x27  a kind-0x26 band every 16 frames for 0x1000 frames, on the ground
//              at its point (area 81's handler 0 spawns it, area_w2a.cpp).
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, or would write a record past the pool on an answer the
// callee never gives, ours aborts with a message (docs/effect_1d.md section 2).
#include "game/effect_1d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1d::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U AddressOf(const void* p) { return Low32(p); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_1d.md section 2)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// Effect_FindFree's record (answer & 0xFF, as the originals widen it): a Fatal
// on an answer past the twenty (the callee answers 0xFF or 0..19; the original
// would write past the pool).
unsigned char* NewRecord(const char* who, unsigned index) {
    if (index >= at::kEffects)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes past the pool "
                    "(docs/effect_1d.md section 2)",
                    who, index);
    return rv::Bytes(Effect_Objects) + index * at::kEffectStride;
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

}  // namespace

// ===========================================================================
// Kind 0x21: Effect_KindHandlers[0x21] (0x6553D4), EffectKind21_States (six)
// ===========================================================================

// ===========================================================================
// Kind 0x22: Effect_KindHandlers[0x22] (0x6553D8), EffectKind22_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x23: Effect_KindHandlers[0x23] (0x6553DC), EffectKind23_States (three,
// the third FC1's Effect_StateRelease)
// ===========================================================================

// ===========================================================================
// Kind 0x24: Effect_KindHandlers[0x24] (0x6553E0), EffectKind24_States (three,
// the third Effect_StateRelease)
// ===========================================================================

// ===========================================================================
// Kind 0x25: Effect_KindHandlers[0x25] (0x6553E4), EffectKind25_States (five,
// the fifth Effect_StateRelease)
// ===========================================================================

// ===========================================================================
// Kind 0x26: Effect_KindHandlers[0x26] (0x6553E8), EffectKind26_States (three,
// the third Effect_StateRelease)
// ===========================================================================

// ===========================================================================
// Kind 0x27: Effect_KindHandlers[0x27] (0x6553EC), EffectKind27_States (three,
// the third Effect_StateRelease)
// ===========================================================================

void Effect1D_Inject() {
    if (bof3::WantsShadow("effect_1d")) effect_1d::SelfTest();
}
