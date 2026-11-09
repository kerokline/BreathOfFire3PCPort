// Round thirteen group E4A (docs/effect_4a.md): the 48 functions of
// analysis/round13_cut.tsv's group E4A and three starts the cut does not list
// (kind 0x83's state 2 0x4883D0, kind 0x85's shared draw tail 0x488B90 and kind
// 0x86's dispatcher 0x488BE0), each read with capstone to its last
// instruction. Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says:
//
//   kind 0x82   (states 11..23; 0..10 are E3D's) Sprite_Objects record 0, once
//               it has reached the leader, nudged in z (+0.5, -1, +0.5 cells
//               with eight-frame holds), then restarted or ended
//   kind 0x83   kind 0x82 again for record 1: pushed toward the leader by
//               0x903849's count, nudged, restarted or ended
//   kind 0x84   the count's driver: waits for one of three held-button words,
//               picks the next push count (EffectKind84_PushCounts by Rand),
//               counts the presses into 0x90384A (0x80 at 17); ends when the
//               third member's x passes 0x34 cells or 0x903849 is 0xFE
//   kind 0x85   a run of messages 0x33..0x36 under a full-screen tile that
//               brightens between them; then waits for the counter at 0x35
//   kind 0x86   two free sprites placed by EventOp_0x, one cell either side in
//               z, slid back over 32 frames, tinted brighter, then freed
//   kind 0x87   E4B's records in EffectKind30_Shards set up, run to their end,
//               the draw pass flags kept and put back
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end or indexes Sprite_Objects past its 30 records by a byte
// of the record, ours aborts with a message (docs/effect_4a.md section 7).
#include "game/effect_4a.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_4a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4a::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
unsigned char& B(U a) { return *At(a); }
U AddressOf(const void* p) { return Low32(p); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table, a null or data.
void DispatchEffect4a(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4a.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(P(table + 4 * state))))();
}

// Sprite_Current pointed at `object` for `count` adds of `step` to its dword
// `offset`, then +1 `next` (and +9 `wait` when given) on the effect record and
// Sprite_Current put back, in the original's order.
void Nudge(unsigned char* object, unsigned offset, int count, U step, unsigned next, int wait) {
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = object;
    for (int n = count; n != 0; --n) SetUL(Sprite_Current + offset, UL(Sprite_Current + offset) + step);
    saved[1] = static_cast<unsigned char>(next);
    Sprite_Current = saved;
    if (wait >= 0) saved[9] = static_cast<unsigned char>(wait);
}

// +9 down one; +1 `next` when it was 0.
void Hold(unsigned next) {
    unsigned char* const s = S();
    const unsigned char was = s[9];
    s[9] = static_cast<unsigned char>(was - 1);
    if (was == 0) S()[1] = static_cast<unsigned char>(next);
}

// The pushes have ended (0x903849 0xFF) or the presses have (0x90384A 0x80):
// +1 0x17, +2 0; else 0x903849 0 and +1, +2 0 (back to the start).
void Again() {
    if (B(::bof3::Img(effect_4a::at::kCounterB.v)) == 0xFF || B(::bof3::Img(effect_4a::at::kCounterC.v)) == 0x80) {
        S()[1] = 0x17;
        S()[2] = 0;
        return;
    }
    unsigned char* const s = S();
    B(::bof3::Img(effect_4a::at::kCounterB.v)) = 0;
    s[1] = 0;
    S()[2] = 0;
}

// Field_State the third member's object; its x at 0x34 cells or past it
// (signed): true, with 0x903849 0xFF, 0x90384A / B 0 and the step 0x14.
bool PastLine() {
    const std::int32_t x = ObjTrio[2].x;
    Field_State = P(::bof3::Img(ObjTrio_addr) + 0x298);
    if (x < at::kTrio2Line) return false;
    B(::bof3::Img(effect_4a::at::kCounterB.v)) = 0xFF;
    B(::bof3::Img(effect_4a::at::kCounterC.v)) = 0;
    B(::bof3::Img(at::kCounterD.v)) = 0;
    MoveScript_PartyRecords[0x65] = 0x14;
    return true;
}

}  // namespace

// ===========================================================================
// Kind 0x82, states 11..23 (EffectKind82_States, E3D's; 0..10 E3D's)
// ===========================================================================

// ===========================================================================
// Kind 0x83: EffectKind83_States (24)
// ===========================================================================


// ===========================================================================
// Kind 0x84: EffectKind84_States (5)
// ===========================================================================

// ===========================================================================
// Kind 0x85: EffectKind85_States (5)
// ===========================================================================

// ===========================================================================
// Kind 0x86: EffectKind86_States (4)
// ===========================================================================

// ===========================================================================
// Kind 0x87: EffectKind87_States (9)
// ===========================================================================

void Effect4A_Inject() {
    if (bof3::WantsShadow("effect_4a")) effect_4a::SelfTest();
}
