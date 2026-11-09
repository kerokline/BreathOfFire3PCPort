// Round thirteen group E1A: the band 0x462B00..0x4672F0 of the cut
// (analysis/round13_cut.tsv), taken with the scenario harness in effect mode
// (scenario_harness.h, docs/scenario_harness.md section 8). Every state
// handler runs with Sprite_Current an Effect_Objects record, as
// Effect_RunObjects calls it through Effect_KindHandlers[+5] and the kind's
// dispatcher through its state table by +1. What each kind is, as far as the
// code says, is in docs/effect_1a.md section 2:
//
//   kinds 0xE, 0x16  a world map's record handler (WorldMap_Records +4 / +8)
//   kind 0x5C        area 104's or area 121's own handler
//   kind 1           a textured quad under an extra sprite (+0x18), flickering
//   kind 7           a colour ramp (+0x5D..+0x5F up to 0x80) with EffectKind07_DrawSprite
//   kinds 2, 8, 9, 0xB  dispatchers only (their states are R4F's, rest_4f.cpp)
//   kind 0x10        a textured quad between two extra sprites (+0xC, +0x18)
//   kind 3           a fading bar panel and an accessory's name with a count
//   kind 5           a sprite that rises to a height scaled by a parameter,
//                    bounces, spawns kind 0xA and then follows an object
//   kind 0xA         a copy of record 0's sprite following it (a shadow)
//   kind 0xC         an aim bar chasing an object's +0xA
//   kind 0xD         a sprite sliding in and out across the screen
//   kind 0xF         a message window: opens, titles, types lines, closes
//   kind 0x1A        the kind-count panel: option boxes, member rows, item
//                    lists, a panel sliding by +9
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Each is a
// faithful replacement; the one divergence, DIV-0069 (kind 0xF's lines and kind
// 3's name under a Latin overlay, src/game/fishing_text.cpp), is armed only after
// every self-test, so the fuzz compares Capcom's. Where the original indexes a table past its
// end, an object list past its records or the effect pool past its twenty,
// ours aborts with a message (docs/effect_1a.md section 6).
#include "game/effect_1a.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_1a_callees.h"
#include "game/fishing_text.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1a::at;
using U = std::uint32_t;
using move_script::At;

unsigned char* S() { return Sprite_Current; }
}  // namespace
// DIV-0144 (D279): 1 through PatchBytes("FishingEmptyLine") / ("FishingLowGlyph")
// in FishingEmpty_Arm, after every module's self-test.
namespace effect_1a { std::uint8_t g_empty_line_none = 0; }  namespace {
std::uint8_t g_low_glyph_none = 0;
U Key(const void* p) { return Low32(p); }
U L(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
U L(U a) { return L(At(a)); }
void SetL(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }
void SetW(unsigned char* p, U v) { move_script::SetWord(p, v); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(move_script::Word(p)); }

using Handler = void (__cdecl*)();

// jmp [table + byte * 4]: the table's `entries` handlers, read in place (the
// fuzz swaps the cells for recorders); a Fatal past them, where the original
// jumps through the dword after - the next table's entry or data.
void Run(const char* who, U table, unsigned entries, unsigned state) {
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_1a.md section 6)",
                    who, state, entries, (unsigned)::bof3::VaOf(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * state)))();
}

U MessageLine(U pen, U text, U width, U x) {
    return SH_AT(U (__cdecl*)(U, U, U, U), at::kMessageLine.v)(pen, text, width, x);
}

void CopyDword(unsigned char* to, const unsigned char* from) { std::memcpy(to, from, 4); }

}  // namespace

// ===========================================================================
// The dispatchers: Effect_KindHandlers' entries, by +1 through a state table
// (none bounds its index)
// ===========================================================================

// ===========================================================================
// Kind 1 (EffectKind01_States: _Start, _Draw) and kind 0x10: a textured quad
// ===========================================================================

// ===========================================================================
// Kind 7 (EffectKind07_States: _Start, _FadeIn, then EffectKind07_Hold / _FadeOut,
// R4F's)
// ===========================================================================

// ===========================================================================
// The panel draws (EffectHud_*): cdecl helpers of kinds 2, 3 and 0xC, and of
// E1B's kinds (docs/effect_1a.md section 2.4)
// ===========================================================================

// ===========================================================================
// Kind 3 (EffectKind03_States: BareRet, _Start, _Fade, _ShowName)
// ===========================================================================

// ===========================================================================
// Kind 5 (EffectKind05_States: BareRet, _Start, _Rise, _Land, _Bounce, _Follow)
// ===========================================================================

// ===========================================================================
// Kind 0xA (EffectKind0A_States: _Start, _Follow): record 0's sprite copied
// ===========================================================================

// ===========================================================================
// Kind 0xC (EffectKind0C_States: BareRet, _Start, _Aim)
// ===========================================================================

// ===========================================================================
// Kind 0xD (EffectKind0D_States: BareRet, _Start, _SlideIn, _Hold, _SlideOut)
// ===========================================================================

// ===========================================================================
// Kind 0xF (EffectKind0F_States: BareRet, _Start, _Open, _Choose, _Title,
// _TitleClose, _Frame, _Close, _LineStart, _LineType, _LineNext, _LineScroll,
// _LineFade): a message window at y +0x30 and the lines it types
// ===========================================================================

// ===========================================================================
// Kind 0x1A (EffectKind1A_States: BareRet, _Start, _Open, _ItemsA, _ItemsB,
// _Close, _FindKind, _PanelIn, _Panel, _PanelNext, _PanelBack, _PanelOut,
// _Reset, then E1B's 0x4672F0..): the window with option boxes, member rows
// and item lists, and the kind panel sliding by +9
// ===========================================================================

void Effect1A_Inject() {
    if (bof3::WantsShadow("effect_1a")) effect_1a::SelfTest();
}

// DIVERGENCE DIV-0144 (D279): the fishing banner declines an empty string.
namespace {

// BOF3X_SHADOW=fishing_empty: an empty line and a NUL label planted in the
// image (text record 1 and the six 0x66A2FC entries, restored after), a
// record of ours as Sprite_Current; nothing may be drawn - Gfx_PacketNext
// unmoved - and the calls that reach EffectKind0F_DrawGlyph are counted, so
// the scroller's switch is seen to stop its call and the glyph's switch to
// decline the label's. Run before FishingText_Arm: the label is then the
// original's read (the line byte's 0x66A2FC entry), not an icon or a disc's.
void FishingEmptySelfTest() {
    constexpr unsigned kText = 1;   // not the cast's text 0, whose label reads word 7
    unsigned char* const record = At(::bof3::Img(at::kKind0FTexts.v) + 8 * kText);
    unsigned char saved_record[8], saved_labels[24];
    std::memcpy(saved_record, record, sizeof saved_record);
    std::memcpy(saved_labels, At(::bof3::Img(at::kLabelTexts.v)), sizeof saved_labels);
    unsigned char* const saved_current = Sprite_Current;
    unsigned char* const saved_packet = Gfx_PacketNext;
    const std::uint8_t saved_line = effect_1a::g_empty_line_none, saved_glyph = g_low_glyph_none;
    static const unsigned char kEmpty[4] = {0, 0, 0, 0};
    static const unsigned char kSpace[2] = {0x20, 0};
    SetL(record, Key(kEmpty));
    record[4] = 0;   // line byte 0: the label is entry 0
    record[5] = 1;   // its pause
    for (unsigned e = 0; e < 6; ++e) SetL(At(::bof3::Img(at::kLabelTexts.v) + 4 * e), Key(kEmpty));
    unsigned char object[0x80];
    unsigned bad = 0, cases = 0;
    auto check = [&](bool ok, const char* what, unsigned got, unsigned want) {
        ++cases;
        if (ok) return;
        ++bad;
        bof3::Log("shadow      fishing_empty MISMATCH %s: %u, expected %u", what, got, want);
    };
    auto start = [&](unsigned char state, unsigned char wait, unsigned char line_wait) {
        std::memset(object, 0, sizeof object);
        object[0] = 1;
        object[1] = state;
        object[5] = 0xF;
        object[9] = wait;
        object[0xA] = line_wait;
        object[0x4B] = kText;
        SetL(object + 0x50, Key(kEmpty));
        SetW(object + 0x2E, 0x125);
        Sprite_Current = object;
    };
    auto run = [&](void (*state)(), const char* what, unsigned want_calls) {
        const unsigned calls = EffectKind0F_GlyphCalls();
        state();
        check(Gfx_PacketNext == saved_packet, what, Gfx_PacketNext == saved_packet ? 0 : 1, 0);
        check(EffectKind0F_GlyphCalls() - calls == want_calls, what, EffectKind0F_GlyphCalls() - calls, want_calls);
    };
    effect_1a::g_empty_line_none = 1;
    g_low_glyph_none = 1;
    // LineType 0x466310 typing an empty line: no cursor glyph is asked for.
    start(9, 6, 0);
    run(EffectKind0F_LineType, "LineType over an empty line (both on): packet / calls", 0);
    check(object[1] == 9 && object[0x49] == 0, "LineType over an empty line: the state", object[1], 9);
    // Its last frame: the whole line typed at once, the pause set, the next state.
    start(9, 1, 0);
    run(EffectKind0F_LineType, "LineType's last frame over an empty line: packet / calls", 0);
    check(object[1] == 10 && object[0x49] == 0 && object[9] == 1, "LineType's last frame: the state, +0x49, +9",
          object[1], 10);
    // The control: the scroller's switch off, the call reaches the draw, which
    // declines the NUL.
    effect_1a::g_empty_line_none = 0;
    start(9, 6, 0);
    run(EffectKind0F_LineType, "LineType over an empty line (the scroller's switch off): packet / calls", 1);
    effect_1a::g_empty_line_none = 1;
    // LineNext 0x466460 while the label waits: the label's NUL reaches the
    // draw (0x66A2FC's entry 0), which draws nothing.
    start(10, 6, 6);
    run(EffectKind0F_LineNext, "LineNext's label over a NUL label: packet / calls", 1);
    check(object[0xA] == 5, "LineNext's label wait", object[0xA], 5);
    // The draw itself (0x469AD0, which Inject aimed at ours): a NUL and a space.
    const unsigned before = EffectKind0F_GlyphCalls();
    MessageLine(0, Key(kEmpty), 0xC, 0x100);
    MessageLine(0, Key(kSpace), 0xC, 0x100);
    check(Gfx_PacketNext == saved_packet, "DrawGlyph of a NUL and a space: packet", 1, 0);
    check(EffectKind0F_GlyphCalls() - before == 2, "DrawGlyph of a NUL and a space: calls",
          EffectKind0F_GlyphCalls() - before, 2);
    std::memcpy(record, saved_record, sizeof saved_record);
    std::memcpy(At(::bof3::Img(at::kLabelTexts.v)), saved_labels, sizeof saved_labels);
    Sprite_Current = saved_current;
    effect_1a::g_empty_line_none = saved_line;
    g_low_glyph_none = saved_glyph;
    bof3::Log("shadow      fishing_empty DIV-0144 self-test: an empty line typed (two frames) and its control, a NUL "
              "label, a NUL and a space drawn: %u checks, nothing drawn, %u MISMATCHES",
              cases, bad);
    if (bad) bof3::Fatal("fishing_empty DIV-0144 self-test: %u mismatches (bof3x.log)", bad);
}

}  // namespace

bool FishingEmpty_LowGlyphNone() { return g_low_glyph_none != 0; }

void FishingEmpty_Arm() {
    static const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("FishingEmptyLine", Low32(&effect_1a::g_empty_line_none),
                     &was, &is, 1);
    bof3::PatchBytes("FishingLowGlyph", Low32(&g_low_glyph_none),
                     &was, &is, 1);
    bof3::Log("DIV-0144    the fishing banner draws no glyph for an empty line (%s) nor for a one-byte code below "
              "0x26 (%s)", effect_1a::g_empty_line_none ? "on" : "OFF", g_low_glyph_none ? "on" : "OFF");
    if (!bof3::WantsShadow("fishing_empty")) return;
    if (!bof3::IsEnabled(bof3::addr::EffectKind0F_DrawGlyph)) {
        bof3::Log("shadow      fishing_empty DIV-0144 self-test: not run, EffectKind0F_DrawGlyph is Capcom's "
                  "(BOF3X_ORIGINAL), which draws the planted NUL");
        return;
    }
    FishingEmptySelfTest();
}
