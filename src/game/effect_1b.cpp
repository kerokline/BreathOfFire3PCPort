// Round thirteen group E1B: the effect engine's resident code 0x4672F0..0x46A5F2
// (analysis/round13_cut.tsv, group E1B), taken with the scenario harness in
// effect mode (scenario_harness.h, docs/scenario_harness.md section 8).
// docs/effect_1b.md has each function, every caller, the tables and the fuzz.
//
// Effect kind 0xF (Effect_KindHandlers[0xF], its dispatcher 0x466080 and states
// 0..25 are E1A's): states 26..28 open, show and close a message list with a
// toggle row; state 29 is a child effect run by sub-kind (+2) while record 6
// holds its state 0xE; states 30..35 are the six sub-kinds' step dispatchers
// themselves (by +3); 36..40 are sub-kind 0's steps.
//
//   EffectKind0F_ListOpen        0x4672F0  state 26: window, toggles, the list sliding in by +9; at 0 a kind-0x1A child
//   EffectKind0F_ListShow        0x4673B0  state 27: the same at rest
//   EffectKind0F_ListClose       0x467400  state 28: the list sliding out by +9; at 4 the next state by +0xB / +6
//   EffectKind0F_Child           0x4674F0  state 29: EffectKind0F_Children[+2] while record 6 is in state 0xE, else released
//   EffectKind0F_Child0 .. 5     0x467520, 0x4677F0, 0x467A60, 0x467D60, 0x468020, 0x468320: the sub-kinds' +3 dispatch
//   EffectKind0F_Child0Start     0x467540  two kind-0x1A children, the sprite placed
//   EffectKind0F_Child0Animate   0x467630  the animation script 0x653CCC stepped
//   EffectKind0F_Child0Second    0x4676A0  bank 0x21, placed, a counter's step set
//   EffectKind0F_Child0Tick      0x467750  the sprite script ticked, queued
//   EffectKind0F_Child0Back      0x467760  bank 0x47 again, one step back
//   EffectKind0F_Child1Start     0x467810  placed
//   EffectKind0F_Child1Meter     0x4678C0  the script 0x653CEC; a value bounced 0..0x60 or counted; a bar and a number
//   EffectKind0F_Child2Start     0x467A80  placed
//   EffectKind0F_Child2Gauge     0x467B10  the script 0x653D08; a gauge 0..0x18; a kind-0x1A child at a marked step
//   EffectKind0F_Child2Fade      0x467CA0  bank 0x2B, 0x40 frames
//   EffectKind0F_Child2Blink     0x467D30  counted down, drawn every other 8 frames, released at 0
//   EffectKind0F_Child3Start     0x467D80  placed
//   EffectKind0F_Child3Grid      0x467E10  a 4-row grid of marks (0x653D34), the lit cell stepped by +9
//   EffectKind0F_Child4Start     0x468040  placed, the cursor at (0x6A, 0x6A)
//   EffectKind0F_Child4Move      0x4680F0  the script 0x653D5C moves the cursor; the panel drawn
//   EffectKind0F_DrawCursorPanel 0x468210  (down): the cursor clamped and drawn with its range
//   EffectKind0F_Child5Start     0x468340  as Child4Start, +0xB = 0x37
//   EffectKind0F_Child5Move      0x468350  the script 0x653DAC; a count to 0x3C printed
//   EffectKind0F_DrawMessageList 0x468560  (x, y): a window of script-pool lines scrolled by +8 / +0x14
//   EffectKind0F_DrawListFrame   0x468840  (x, y): the list's sprite frame
//   Panel_DrawEdgeQuad           0x468950  (x, y, height, which): a textured quad of the frames' edges (FE1 calls it too)
//   EffectKind0F_DrawCountHeader 0x468A40  a sprite, a label, +0x3E + 1 printed
//   EffectKind0F_DrawToggles     0x468AC0  (x, y, bits): three toggle boxes and their labels by the bits
//   EffectKind0F_DrawToggle      0x468BB0  (x, y, lit): one toggle box
//   EffectKind0F_DrawItemsB      0x468C50  (x, y): the accessories of category 0xB, the chosen one lit
//   EffectKind0F_DrawScrollMark  0x468E50  (x, y, height): a textured quad
//   EffectKind0F_DrawItemsA      0x468F00  (x, y): the accessories of category 0xA, scrolled
//   EffectKind0F_DrawEquipped    0x469210  (x, y): two accessories and their icons, a message
//   EffectKind0F_DrawTwinFrame   0x469490  (x, y): two windows and their sprite frames
//   EffectKind0F_DrawItemFrame   0x469630  (x, y): a window and its sprite frame
//   Panel_DrawWindow             0x469750  (x, y, w, h, colour): the bevel and the edges (FE2's trade screen too)
//   Panel_DrawWindowBevel        0x469790  (x, y, w, h): two semi-transparent quads and a tile in the style's colour
//   Panel_DrawWindowEdges        0x469960  (x, y, w, h, colour): two three-point lines
//   EffectKind0F_DrawGlyph       0x469AD0  (clut, text, width, x): one glyph primitive at record 3's y
//   EffectKind92_Follow          0x46A3E0  kind 0x92: a sprite object moved by the record's step; released with it
//   EffectKind11_DrawShade       0x46A450  kind 0x11: a screen-wide black semi-transparent quad
//   EffectKind12_DrawGradient    0x46A500  kind 0x12: a screen-top gradient
//   EffectKind14_Run             0x46A5E0  kind 0x14: EffectKind14_States[+1]
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Each is a
// faithful replacement; the one divergence, DIV-0069 (the toggles' labels and the
// accessories' names under a Latin overlay, src/game/fishing_text.cpp), is armed
// only after every self-test, so the fuzz compares Capcom's. Sprite_Current is read afresh at each use, as
// the originals read [0x937F88] after every call (where one keeps it in a
// register across code with no call, the two are the same). Where an original
// indexes past one of its tables - a state table, an animation script, the
// accessories, the sprite objects, a record Effect_FindFree did not give - ours
// aborts with a message (docs/effect_1b.md section 7).
#include "game/effect_1b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1b_callees.h"
#include "game/fishing_text.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1b::at;
using move_script::At;
using move_script::Long;
using U = std::uint32_t;

unsigned char* Sc() { return Sprite_Current; }
// A float stored as the originals' fild / fstp store it: every value here is a
// whole number well inside a float's 24 bits, so exact.
void F(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// A .data state table read in place: the index a byte of the record, unchecked
// in the original, which jumps through whatever follows the table past its
// length; ours aborts there. While the fuzz runs its entries are recorders.
using Handler = void (__cdecl*)();
Handler StepAt(U table, unsigned index, unsigned count, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: index %u past the %u entries of 0x%X (the original jumps through what follows)", who, index,
                    count, (unsigned)::bof3::VaOf(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))));
}

}  // namespace

// --- kind 0xF: the list states -------------------------------------------------------

// --- sub-kind 0 (EffectKind0F_Child0Steps) --------------------------------------------

// --- sub-kind 1 --------------------------------------------------------------------

// --- sub-kind 2 --------------------------------------------------------------------

// --- sub-kind 3 --------------------------------------------------------------------

// --- sub-kinds 4 and 5 -------------------------------------------------------------

// --- the panels ---------------------------------------------------------------------

// --- the window (FE2's trade screen and the kinds' panels) --------------------------

namespace effect_1b { unsigned g_glyph_calls = 0; }   // DIV-0144's self-test (effect_1a.cpp) counts the calls that reach the draw

unsigned EffectKind0F_GlyphCalls() { return effect_1b::g_glyph_calls; }

// --- kinds 0x92, 0x11, 0x12, 0x14 ---------------------------------------------------

void Effect1B_Inject() {
    if (bof3::WantsShadow("effect_1b")) effect_1b::SelfTest();
}
