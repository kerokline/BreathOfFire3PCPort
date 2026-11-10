// Round thirteen group E2G (docs/effect_2g.md): the 56 functions of
// analysis/round13_cut.tsv's group E2G, 0x47DBE0..0x47FD92, and two starts the
// cut does not list (the sub-kind's shared draw 0x47E120, kind 0x5E's
// dispatcher 0x47F5D0), each read with capstone to its last instruction.
// Effect_RunObjects (ours) makes each live record of Effect_Objects (20 of 0x80
// bytes) Sprite_Current and calls Effect_KindHandlers[+5]; each kind here is a
// dispatcher by +1 through its state table (none bounded by a compare) and
// the states it names. What each kind is, as far as the code says:
//
//   kind 0x66   a window (area 135's choice spawns it, +6 the answer): it grows
//               over eight frames, stands with the message MsgBox_FrameTask
//               types and two or three textured pieces, and shrinks away
//   kind 0x18, sub-kind 0x20   a pair of textured strips that lower and rise
//               with story flag 0x2A (Cond_ByteFE the step), MoveCmd_TestFB
//               at each end
//   kind 0x15   the second party member's side of a timed exchange: it waits
//               on the cue byte 0x90384A, animates ObjTrio record 1, and raises
//               the cue after random waits (area 145, chapter 3)
//   kind 0x54   a field object's side of the same exchange (area 8, area 135):
//               the object +0xB names answers the cue, the count 0x90384B up on
//               a hit
//   kind 0x55   a thirty-second clock and the count, then the end (bit 7 of
//               the count; bit 7 of the cue too at a count of 16 or more) -
//               the chop of area 8 (Scena01_Scene06 steps 0x12..0x14, which
//               take the cue's bit 7 as the win): under BOF3X_NO_FAIL the cue
//               is raised at any count (DIVERGENCE DIV-0188), and under a
//               language overlay that has written DIV-0064's group 25 the
//               count's box, place and word are the disc's own
//   kind 0x57   a ring of sixteen shaded quads round the screen's centre (in
//               area 2 it stays)
//   kind 0x5A   a glowing cylinder placed on the ground at (10, 0x63)
//   kind 0x5B   area 75's two beams: a textured quad from a point to the ground
//               beside it, brightening and fading
//   kinds 0x5D, 0x5E, 0x5F   dispatchers only (their states are catalog part 6
//               rows, no group's this round)
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Two
// divergences, both kind 0x55's (DIV-0188 in EffectKind55_Finish, DIV-0064's
// group 25 in EffectKind55_DrawCount); every other function is a faithful
// replacement. Where the original jumps through a state table past its end or
// indexes a table or the object pool past its records, ours aborts with a
// message (docs/effect_2g.md section 6).
#include "game/effect_2g.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect_2g_callees.h"
#include "game/effect_gte.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2g::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U ULEffect2g(const unsigned char* p) { return static_cast<U>(Long(p)); }
U ULEffect2g(U a) { return ULEffect2g(At(a)); }
U W(U a) { return Word(At(a)); }
unsigned char& B(U a) { return *At(a); }
std::int32_t S16Effect2g(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }
U AddressOf(const void* p) { return Low32(p); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2g.md section 6)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect2g(table + 4 * state)))();
}

// ===========================================================================
// Kind 0x66: Effect_KindHandlers[0x66] (0x6554E8), EffectKind66_States (six)
// ===========================================================================

}  // namespace

// ===========================================================================
// Kind 0x18's sub-kind 0x20: EffectKind18_States[0x20] (0x6540EC),
// EffectKind18Sub20_States (five) by +2
// ===========================================================================

// ===========================================================================
// Kind 0x15: Effect_KindHandlers[0x15] (0x6553A4), EffectKind15_States (ten)
// ===========================================================================

// ===========================================================================
// Kind 0x54: Effect_KindHandlers[0x54] (0x6554A0), EffectKind54_States (15)
// ===========================================================================

// ===========================================================================
// Kind 0x55: Effect_KindHandlers[0x55] (0x6554A4), EffectKind55_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x57: Effect_KindHandlers[0x57] (0x6554AC), EffectKind57_States (two)
// ===========================================================================

// ===========================================================================
// Kind 0x5A: Effect_KindHandlers[0x5A] (0x6554B8), EffectKind5A_States (two)
// ===========================================================================

// ===========================================================================
// Kind 0x5B: Effect_KindHandlers[0x5B] (0x6554BC), EffectKind5B_States (three)
// ===========================================================================

// ===========================================================================
// Kinds 0x5D, 0x5E, 0x5F: dispatchers only
// ===========================================================================

void Effect2G_Inject() {
    if (bof3::WantsShadow("effect_2g")) effect_2g::SelfTest();
}
