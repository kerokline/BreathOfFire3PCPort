// Round thirteen group E1G (docs/effect_1g.md): the fourteen functions of
// analysis/round13_cut.tsv's group E1G, 0x594060..0x594D8A - the item-trade
// screen's rest and the item icon. The cut filed them under the effect engine
// (unit T66A470, "Sprite_ClutWord"); read, they are the trade screen round
// twelve's FE2 took the first seven states of (field_e2.cpp), none of them an
// effect kind's state:
//
//   the states    ItemTrade_FullMessage (ItemTrade_RunSteps[3]),
//                 ItemTrade_Leave (ItemTrade_States[2]) and its two steps
//                 ItemTrade_LeaveAsk / ItemTrade_LeaveWait (ItemTrade_LeaveSteps)
//   the draws     ItemTrade_DrawBackground, ItemTrade_DrawList and its frame,
//                 ItemTrade_DrawNeeds and its frame, ItemTrade_DrawCount and
//                 its frame
//   the tests     ItemTrade_Lacks (an ingredient short), ItemTrade_RowCount
//   the icon      Item_DrawIcon (also called by E1B's 0x469210)
//
// Every call goes through the scenario harness (SH_CALL / SH_AT, a table's word
// read in place), so the start-up fuzz (effect_1g_fuzz.cpp) stands recorders in
// for ours as for the originals' copies. Faithful but for DIV-0027's leave
// prompt under a Latin overlay (LeavePrompt, amended 2026-10-03) and the
// backdrop under a wide picture (DIV-0041, ItemTrade_DrawBackground; off in
// the fuzz). Where an
// original indexes past its table, ours aborts with a message
// (docs/effect_1g.md section 6).
#include "game/effect_1g.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_1g_callees.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1g::at;

}  // namespace

// DIVERGENCE DIV-0027 (amended 2026-10-03, group YN): Manillo's "Will that be
// all?  Yes No". The original's hand stops (0xE0 and 0x104) were fitted to the
// Chinese line; under the English overlay the hand on Yes is a word short of
// it and the hand on No covers Yes. With the flag on (Effect1G_Inject, a Latin
// overlay, after the self-test, which compares the original's draw) the line is
// re-spaced and the hand stops two units left of each answer - the load / save
// screen's spacing (yes_no_layout.cpp's YesNoLayout_Tail).
unsigned char g_trade_leave_layout = 0;

// DIV-0041 (effect_1g.h): every quad's u starts at its left x mod 32 and runs
// one texel a column, so the texel under column x is u x mod 32 as in the
// original (whose halves start at 0 and 0xA0, both 0 mod 32). The page texture
// is the 32 x 32 window tiled over 256 x 256 (Tex_Convert4 / Tex_Convert8), so
// any u below 256 repeats the pattern; a quad wider than that cannot be drawn
// in one, which is why a wider picture takes four quads (the original's two
// halves and a band quad each side) instead of two grown halves.
namespace {
effect_1g::BackdropSpan Span(int x0, int x1, unsigned columns) {
    const unsigned u0 = static_cast<unsigned>(x0) & 31u;   // x0 mod 32, for a negative x0 too
    const unsigned u1 = u0 + static_cast<unsigned>(x1 - x0);
    if (u1 > 0xFFu) bof3::Fatal("ItemTrade_BackdropSpan: %u columns put a u at %u (x %d..%d), past the byte", columns, u1, x0, x1);
    effect_1g::BackdropSpan s;
    s.x0 = 0.0f + static_cast<float>(x0);   // not a bare cast of -columns: 0.0f + keeps -0 out
    s.x1 = static_cast<float>(x1);
    s.u0 = static_cast<unsigned char>(u0);
    s.u1 = static_cast<unsigned char>(u1);
    return s;
}
// Whether the two grown halves would each fit the byte: the left's u runs to
// (-columns mod 32) + 0xA0 + columns, the right's to 0xA0 + columns.
bool HalvesFit(unsigned columns) {
    return ((32u - (columns & 31u)) & 31u) + 0xA0u + columns <= 0xFFu && 0xA0u + columns <= 0xFFu;
}
}  // namespace

unsigned effect_1g::ItemTrade_BackdropQuads(unsigned columns) { return HalvesFit(columns) ? 2u : 4u; }

effect_1g::BackdropSpan effect_1g::ItemTrade_BackdropSpan(unsigned quad, unsigned columns) {
    const int c = static_cast<int>(columns);
    if (HalvesFit(columns)) {
        if (quad == 0) return Span(-c, 0xA0, columns);
        if (quad == 1) return Span(0xA0, 0x140 + c, columns);
    } else {
        if (quad == 0) return Span(0, 0xA0, columns);
        if (quad == 1) return Span(0xA0, 0x140, columns);
        if (quad == 2) return Span(-c, 0, columns);
        if (quad == 3) return Span(0x140, 0x140 + c, columns);
    }
    bof3::Fatal("ItemTrade_BackdropSpan: quad %u of %u for %u columns", quad, ItemTrade_BackdropQuads(columns), columns);
}

// ============================================================================
// The states
// ============================================================================

// ============================================================================
// The draws
// ============================================================================

// ============================================================================
// The icon
// ============================================================================

// ============================================================================

void Effect1G_Inject() {
    if (bof3::WantsShadow("effect_1g")) effect_1g::SelfTest();
    // DIVERGENCE DIV-0027 (amended 2026-10-03): after the self-test, which
    // compares the original's prompt; a Latin overlay only (DIV-0056).
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("TradeLeaveLayout", Low32(&g_trade_leave_layout),
                         &was, &is, 1);
        bof3::Log("DIV-0027    Manillo's \"Will that be all?\": answers re-spaced, the hand two units left of each");
    }
}
