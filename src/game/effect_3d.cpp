// Round thirteen group E3D (docs/effect_3d.md): the 63 functions of
// analysis/round13_cut.tsv's group E3D, 0x485CB0..0x48801A, and five starts the
// cut does not list (kind 0x82's state 2 0x487DE0, and kind 0x77's sub-state
// dispatcher 0x486280 and its three sub-states 0x4861C0, 0x4861E0, 0x4862A0,
// catalog rows no group of the round holds), each read with capstone to its
// last instruction. Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names, or (kinds
// 0x76, 0x79, 0x7A) a single draw. What each kind is, as far as the code says:
//
//   kinds 0x76, 0x79, 0x7A   a full-screen quad of colour 0 committed
//               semi-transparent (abr 2, 2, 0); kind 0x79 ends when the
//               chapter counter 0x903848 is 1
//   kinds 0x7B, 0x7C   the same quad with a red that pulses between two
//               shades every 17 frames; kind 0x7C ends at counter 4
//   kind 0x77   a count in a box (to 200, one every 30 or 40 frames, a sound
//               each when the chapter's flag 0x14 is set); at 200 it sets run
//               5 step 0x19 unless the chapter's flag 0x2E is set
//   kind 0x78   a spinning ring of 32 red-and-white triangles about
//               Sprite_Objects record 2, which it turns (area 130 spawns it)
//   kind 0x7D   area 170's three dials: a panel of three 7 x 9 grids the
//               player turns, confirm or cancel; confirm applies them to the
//               map (EffectKind7D_SetMap, which area 170's init also calls)
//   kind 0x7F   a widening cone of 32 shaded quads about the record's point
//   kind 0x80   a trail of 32 points that rises along x and fades
//   kind 0x81   drops from sixteen sources about the leader, pouring until
//               the counter reaches 12, then draining
//   kind 0x82   Sprite_Objects record 0 pushed a cell at a time (three, four
//               or five pushes by 0x903849) while it is short of the leader
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end or indexes a table past its entries, ours aborts with a
// message (docs/effect_3d.md section 7). The x87 sequences (fild, fadd, fsub,
// fld / fstp copies, _ftol) run as the original's instructions (inline asm), so
// the rounding and a signalling NaN's quieting are the original's.
#include "game/effect_3d.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_3d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3d::at;
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
U W(const unsigned char* p) { return Word(p); }
U W(U a) { return Word(At(a)); }
unsigned char& B(U a) { return *At(a); }
std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }
U AddressOf(const void* p) { return Low32(p); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
void Copy4(unsigned char* to, const unsigned char* from) { std::memcpy(to, from, 4); }   // mov reg, [from]; mov [to], reg
void SetBits(unsigned char* to, U bits) { std::memcpy(to, &bits, 4); }

// --- the x87 sequences, as the original's instructions --------------------------
// fild dword v; fstp dword [to]
void Fild(unsigned char* to, std::int32_t v) {
    __asm__ volatile("fildl %1\n\tfstps %0" : "=m"(*reinterpret_cast<float*>(to)) : "m"(v));
}

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, data or the next kind's states.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3d.md section 7)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

void NextState() { S()[1] = static_cast<unsigned char>(S()[1] + 1); }

// +9 down one; true when it reaches 0 (Sprite_Current read for each access).
bool CountDown() {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    return S()[9] == 0;
}

}  // namespace

// ===========================================================================
// Kinds 0x76, 0x79, 0x7A: Effect_KindHandlers[0x76] / [0x79] / [0x7A], a draw each
// ===========================================================================

// ===========================================================================
// Kinds 0x7B, 0x7C: EffectKind7B_States, EffectKind7C_States (two each)
// ===========================================================================

// ===========================================================================
// Kind 0x77: EffectKind77_States (three); states 0 and 1 dispatch by +2
// ===========================================================================

// ===========================================================================
// Kind 0x78: EffectKind78_States (four)
// ===========================================================================

// ===========================================================================
// Kind 0x7D: EffectKind7D_States (four) - area 170's dials
// ===========================================================================

// ===========================================================================
// Kind 0x7F: EffectKind7F_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x80: EffectKind80_States (nine; 4..7 are 0x492750, a catalog row no
// group of the round holds, and 8 Effect_StateRelease)
// ===========================================================================

// ===========================================================================
// Kind 0x81: EffectKind81_States (three) - drops from sixteen sources
// ===========================================================================

// ===========================================================================
// Kind 0x82: EffectKind82_States (24; 11..23 are E4A's and two of ours)
// ===========================================================================

void Effect3D_Inject() {
    if (bof3::WantsShadow("effect_3d")) effect_3d::SelfTest();
}
