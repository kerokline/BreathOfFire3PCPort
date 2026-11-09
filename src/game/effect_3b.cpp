// Round thirteen group E3B (docs/effect_3b.md): the 49 functions of
// analysis/round13_cut.tsv's group E3B, 0x4823D0..0x48404A, each read with
// capstone to its last instruction. Effect_RunObjects (ours) makes each live
// record of Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says:
//
//   kind 0x63   a disc of 32 shaded triangles on the ground at a fixed point
//               (0x24, 0x41): it grows to 0x190, holds, shrinks while a
//               full-screen tint is drawn and every live sprite and member is
//               refreshed, raises the counter 0x903848, waits for it to reach
//               0x2B, fades the tint and marks the members (+0x29 = 6)
//   kind 0x65   a field wait: on Field_Request 3, unless the save block's dword
//               0x904134 is a multiple of five, a held button toggles story
//               flag 0x4F and shakes the camera for 60 frames, then opens
//               script message 1 and waits for the request to leave 2
//   kind 0x67   spawns a kind-0x13 record every frame of its state 1 (state 0
//               is EffectKind54_Start, state 2 BareRet)
//   kind 0x69   nine parts spawned by one record (kept in 0x676268): +2 picks
//               a part's role - one column at the parent's point, four orbiting
//               at radius 0x20, four at 0x18 / 0x10 - each with its own steps by
//               +3, a matrix pushed at its point, a flickering column of quads
//               and a glow of eight triangles; the parent ends when all nine
//               have counted into its +0xB
//   kind 0x6C   sixteen sparks at (0xB8000, 0x548000, 0x1000000) scattered over
//               EffectKind30_Shards, flying and fading as textured quads; the
//               record ends when none is live
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, indexes a pool past its records or writes a spawned
// record past the pool (Effect_FindFree's 0xFF unchecked), ours aborts with a
// message (docs/effect_3b.md section 7).
#include "game/effect_3b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3b::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
void SetUL(U a, U v) { SetUL(At(a), v); }
unsigned char& B(U a) { return *At(a); }
U S16(U v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v & 0xFFFFu))); }
U AddressOf(const void* p) { return Low32(p); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul (32 bits kept)
U Mul(int a, U b) { return static_cast<U>(a) * b; }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + byte]; jmp / call
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original goes
// through the dword after - the next table.
void Through(const char* who, U table, unsigned entries, unsigned index, unsigned byte) {
    if (index >= entries)
        bof3::Fatal("%s: +%u is %u, past the %u entries of 0x%X - the original goes through the dword after "
                    "(docs/effect_3b.md section 7)",
                    who, byte, index, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * index)))();
}
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    Through(who, table, entries, Sprite_Current[byte], byte);
}

// Effect_FindFree's record (the answer's low byte, as the originals widen it):
// a Fatal past the twenty - the original writes record (answer * 0x80) past the
// pool, 0xFF included where it does not test for none.
unsigned char* Spawned(const char* who, U answer) {
    const unsigned k = answer & 0xFF;
    if (k >= at::kEffects)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes record %u past "
                    "the pool (docs/effect_3b.md section 7)",
                    who, k, k);
    return rv::Bytes(&Effect_Objects[k]);
}

}  // namespace

// ===========================================================================
// Kind 0x63: Effect_KindHandlers[0x63] (0x6554DC), EffectKind63_States (seven)
// ===========================================================================

// ===========================================================================
// Kind 0x65: Effect_KindHandlers[0x65] (0x6554E4), EffectKind65_States (five)
// ===========================================================================

// ===========================================================================
// Kind 0x67: Effect_KindHandlers[0x67] (0x6554EC), EffectKind67_States (three:
// EffectKind54_Start, this one, BareRet)
// ===========================================================================

// ===========================================================================
// Kind 0x69: Effect_KindHandlers[0x69] (0x6554F4) - a stack table of three by
// +1; state 2 by +2 through EffectKind69_Parts (three), each part by +3 through
// its own steps (four each)
// ===========================================================================

// ===========================================================================
// Kind 0x6C: Effect_KindHandlers[0x6C] (0x655500), EffectKind6C_States (two);
// its sparks by their +1 through EffectKind6C_SparkStates (two)
// ===========================================================================

void Effect3B_Inject() {
    if (bof3::WantsShadow("effect_3b")) effect_3b::SelfTest();
}
