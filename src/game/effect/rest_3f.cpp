// Round fourteen group R3F (docs/rest_3f.md): the 48 functions of
// analysis/round14_cut.tsv's group R3F and the two states of kind 0x9E no list
// held (0x48F1E0, 0x48F300), 0x480210..0x49259C, each read with capstone to its
// last instruction (2026-10-04). What round thirteen's effect groups left in
// their bands: the states of kinds whose dispatchers they took, and the draws
// those and their neighbours call. Effect_RunObjects (ours) makes each live
// record of Effect_Objects (20 of 0x80) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind's dispatcher (E3A's, E4E's, E4F's) jumps
// or calls through its state table by +1.
//
//   kind 0x60   a grey line from (0x198000, 0x198000) whose ends slide 0x20000
//               a frame to 0x98000 (EffectKind60_DrawLine, E3A's)
//   kind 0x5F   (R3E's states) a disc of semi-transparent LINE_F2, one a row,
//               round a projected point, wobbling: EffectKind5F_DrawLineDisc
//   kind 0x69   (E3B's part 2) a chain of LINE_G2 down a wave: _DrawLines
//   kind 0x9C   (E4E's states 1..6) a trail of two quads and eight dots
//   kind 0x9E   a box outline that grows from the record's point, a flash of
//               its plate, a wall and a textured plate until the chapter's
//               count, the plate again and the outline shrinking (E4E's
//               four plate draws)
//   kinds 0xA1, 0xA3   the sprite the record's +0x4C points at drawn into VRAM
//               (0x340, 0x100) by a borrowed effect record, read back, and
//               every pixel not 0 made a particle - TILE_1s that blink, fly
//               apart or come together; 0xA3 waits for the count 0x29 and
//               sets it to 0x2B when done
//   kind 0xA2   a ring of 32 POLY_G4 between two radii that grow and fall
//   kind 0xA7   a glow (EffectKindA0_DrawGlow's on slot 7) that grows, holds
//               and shrinks
//   kind 0xA8   sixteen red bars across the screen started at random
//   kind 0xA9   a half disc from the screen's bottom centre
//   kind 0xAA   a red gradient over the whole frame that fades in and out
//               (drawn through DIV-0041's fill: docs/rest_3f.md section 2)
//   kind 0xAB   its state 1: the drops (E4F's) while Draw_PassFlags is set
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies; Sprite_Current
// is read again wherever the original reads [0x937F88] again after a call. The
// x87 arithmetic is done in long double, as the originals' fild / fadd / fsub /
// fstp chains are (effect_4e.cpp's form). Where the original indexes past a
// table or its row loop could never end, ours aborts with a message
// (docs/rest_3f.md section 7).
#include "game/rest_3f.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3f_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3f::at;
using U = std::uint32_t;
using LD = long double;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// Twelve bytes moved as the original moves them (three dword movs).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, UL(from + i));
}

// --- x87 as the original has it (effect_4e.cpp's form): `fld dword` through
// inline assembly, every operation in long double, `fstp dword` out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}
LD I(U v) { return static_cast<LD>(static_cast<std::int32_t>(v)); }
void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

long* PointRest3f(unsigned char* p) { return reinterpret_cast<long*>(p); }
float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

// Gpu_GetTPage(0, abr, x, y), then Gpu_SetDrawMode(cursor, 0, dtd, the page's
// low word, 0 - the fifth word the leftover of GetTPage's five pushes).
void SetMode(U abr, int x, int y, int dtd) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
}
// The same, committed 0xC to `slot`.
void DrawMode(U abr, int x, int y, int dtd, U slot) {
    SetMode(abr, x, y, dtd);
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

// --- kinds 0xA1 / 0xA3 --------------------------------------------------------------

// DIVERGENCE DIV-0206 (D217's shape, docs/rest_3f.md section 7): kinds 0xA1 and
// 0xA3's particles. The original writes one 0x14-byte particle per opaque
// pixel of the captured rectangle at 0x931980 with no bound; Sprite_Current
// (0x937F88) lies 1,306 particles in, and capture record 1 is 0x60 x 0x78
// (11,520). Seen 2026-11-07 on the owner's myriaFightAndEnding route: in
// Myria's chamber Garr and Rei, with more than 1,306 opaque pixels, vanished
// and came back in a blink where Nina and Momo scattered and gathered. Ours:
// once g_pixel_room is armed (after the self-test, PatchBytes "PixelRoom",
// the switch shared with kind 0x61's room in effect_3a.cpp) the particles go
// to a room of ours that holds the rectangle's every pixel; the cursor, the
// count, the fields, the flight and the draw are the original's. The three
// cursor cells and the particles are named by these ten functions only (the
// doc's raw scan). BOF3X_ORIGINAL=PixelRoom (or BOF3X_OFF) leaves them at
// 0x931980, the overrun with them.
}  namespace rest_3f { unsigned char g_pixel_room = 0; }  namespace {

// --- kind 0xA8's bars -------------------------------------------------------------------

}  // namespace

// ===========================================================================
// Kind 0x60 (EffectKind60_Run 0x4801F0, E3A's): EffectKind60_States 0..2
// ===========================================================================

// ===========================================================================
// Kind 0x5F's disc (R3E's states call it) and kind 0x69's lines (E3B's)
// ===========================================================================

// ===========================================================================
// Kind 0x9C's trail (E4E's states 1..6 call it)
// ===========================================================================

// ===========================================================================
// Kind 0x9E (EffectKind9E_Run 0x48F070, E4E's): EffectKind9E_States 0..5
// ===========================================================================

// ===========================================================================
// Kinds 0xA1 and 0xA3 (EffectKindA1_Run 0x490A60, EffectKindA3_Run 0x4912F0,
// E4E's): EffectKindA1_States 0..4 = EffectKindA3_States 0..4, and 0xA3's 5..9
// ===========================================================================

// ===========================================================================
// Kind 0xA2 (EffectKindA2_Run 0x4910D0, E4E's): EffectKindA2_States 0..3
// ===========================================================================

// ===========================================================================
// Kind 0xA7 (EffectKindA7_Run 0x491AA0 calls its states, then the glow)
// ===========================================================================

// ===========================================================================
// Kind 0xA8 (EffectKindA8_Run 0x491BA0): EffectKindA8_States 0..2, the bars
// ===========================================================================

// ===========================================================================
// Kind 0xA9 (EffectKindA9_Run 0x491C60 calls its states, then the disc)
// ===========================================================================

// ===========================================================================
// Kind 0xAA (EffectKindAA_Run 0x491D70, E4F's): EffectKindAA_States 0..2
// ===========================================================================

// ===========================================================================
// Kind 0xAB's state 1 (EffectKindAB_Run 0x492510, E4F's)
// ===========================================================================

void Rest3F_Inject() {
    if (bof3::WantsShadow("rest_3f")) rest_3f::SelfTest();
    // DIV-0206: armed after the self-test, which compares ours with Capcom's at
    // 0x931980.
    static const std::uint8_t pixel_was = 0, pixel_is = 1;
    bof3::PatchBytes("PixelRoom", Low32(&rest_3f::g_pixel_room), &pixel_was, &pixel_is, 1);
    bof3::Log("DIV-0206    kinds 0xA1 / 0xA3's particles in a room of %u (the original's holds 1,306): %s", at::kPartRoom,
              rest_3f::g_pixel_room ? "on" : "OFF (BOF3X_ORIGINAL=PixelRoom)");
}
