// Round thirteen group E4C (docs/effect_4c.md): the 49 functions of
// analysis/round13_cut.tsv's group E4C, 0x48B200..0x48C985, and one start the
// cut does not list (kind 0x8E's dispatcher 0x48B300, Effect_KindHandlers
// [0x8E], hidden between 0x48B2F0 and 0x48B320), each read with capstone to its
// last instruction (2026-10-03). Effect_RunObjects (ours) makes each live
// record of Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says:
//
//   kind 0x8D   a camera pull: Camera_Distance set to 0x1C80 and the three
//               party records' byte +0x48 set; a full-screen tint (E4D's
//               0x48CA90) of +0x5D..+0x5F held at 0x7F while +1 is 1 (nothing
//               here moves it on); at +1 = 2 the distance drawn in by 0x2A a
//               frame for +9 frames as the tint fades, then 0x400; +1 = 3
//               steps the byte after MoveScript_Var7 and releases the record
//   kind 0x8E   two pools of 128 falling particles (flat triangles of 0x28
//               from EffectKind30_Shards, single-pixel tiles of 0x24 from
//               0x92D380) set up in three boxes and thrown, bouncing off a
//               floor, until all are spent
//   kind 0x8F   sixteen shards (0x28 at EffectKind30_Shards) spawned at fixed
//               offsets from two .data tables - three, then two every fifteen
//               frames, then in five stages by a frame word - each brightening,
//               then fading and drifting, drawn as a sized textured quad on the
//               ground's height
//   kind 0x90   thirty-two shards (0x28) emitted one every fourth frame at
//               the record's point, each tagged with the record's +6 (a
//               counter 0x676294 hands out) so several records share the pool
//   kind 0x93   kind 0x74's column (E3C's EffectKind74_*) at a lighter grey,
//               the shard pool cleared first, sound 0x208
//   kind 0x99   a disc on the ground one unit back from the record's point,
//               sixteen triangles in four fans, its radius growing for ten
//               frames, then sound 0x20F
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, indexes a table or pool of its own past its rows, reads
// its own frame past a local array or divides by zero, ours aborts with a
// message (docs/effect_4c.md section 6).
#include "game/effect_4c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4c::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;
using Handler = scenario_harness::Handler;
using RecordFn = void (__cdecl*)(unsigned char*);

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }
U AddressOf(const void* p) { return Low32(p); }
U Shards() { return AddressOf(EffectKind30_Shards); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through
// the dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4c.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 up, Sprite_Current read afresh.
void Step() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// The draw mode these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page) - its fifth argument is
// the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

}  // namespace

// ===========================================================================
// Kind 0x8D: Effect_KindHandlers[0x8D] (0x655584), EffectKind8D_States (four)
// ===========================================================================

// ===========================================================================
// Kind 0x8E: Effect_KindHandlers[0x8E] (0x655588), EffectKind8E_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x8F: Effect_KindHandlers[0x8F] (0x65558C), EffectKind8F_States (nine)
// ===========================================================================

// ===========================================================================
// Kind 0x90: Effect_KindHandlers[0x90] (0x655590), EffectKind90_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x93: Effect_KindHandlers[0x93] (0x65559C), EffectKind93_States (four)
// ===========================================================================

// ===========================================================================
// Kind 0x99: Effect_KindHandlers[0x99] (0x6555B4), EffectKind99_States (four)
// ===========================================================================

void Effect4C_Inject() {
    if (bof3::WantsShadow("effect_4c")) effect_4c::SelfTest();
}
