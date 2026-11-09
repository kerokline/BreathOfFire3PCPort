// Round thirteen group E4D (docs/effect_4d.md): the 51 functions of
// analysis/round13_cut.tsv's group E4D, 0x48C990..0x48DF87, each read with
// capstone to its last instruction (2026-10-03). Effect_RunObjects (ours)
// makes each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current
// and calls Effect_KindHandlers[+5]; each kind here is a dispatcher by +1
// through its state table (none bounded by a compare) and the states it names.
// What each kind is, as far as the code says:
//
//   kind 0x91   a full-screen tint brightened over 0x3C frames, then message
//               0x86 opened; once it closes the chapter's step byte raised; the
//               tint held (chapter 14's scenes spawn it: Scena14_Run2)
//   kind 0x94   the tint faded in over 0x5A frames (blend 2) with the first six
//               field sprites and the party members at pose 7 drawn again over
//               it, held; or (+1 = 3, Scena14_Run6's spawn) a white flash (blend
//               1) for four frames and released
//   kind 0x95   a clock: the word 0x67629A counted in ticks of 30 (or 40)
//               frames while no message, menu or mode stops it; at 15 ticks the
//               flag 0x40 set, the chapter's run 7 step 5, released
//   kind 0x96   a full-screen (320 x 320) magenta quad, its level stepped through
//               an eight-entry table every seventeen frames, never released
//   kind 0x97   a ring of 32 grey triangles grown at the record's point, a
//               burst of 32 debris triangles and eight rising spark rings at the
//               counter 0x903848 = 10, transitions 8 and 9, the chapter's step
//               0x14 (Scena14_Run7's spawns)
//   kind 0x98   a grey flash rising over 0x20 frames, then a burst ring of 32
//               triangles at the record's point fading (Scena14_EnterArea's and
//               Scena14_Run7's spawns)
//   kind 0x9A   Sprite_Objects record 1 nudged 0x1000 along y or x for five
//               frames and back, again and again until the counter 0x903848 is
//               0x28
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. Sprite_Current is read
// again wherever the original reads [0x937F88] again. The full-screen fills of
// 0x48CA90, 0x48CC90 and 0x48DBA0 are DIV-0041's (section 3c): they draw at
// (Widescreen_FillX(), 0) Widescreen_FillWidth() x 240, which is the original's
// (0, 0) 320 x 240 until Widescreen_ArmFills has run and whenever the picture
// is narrow. Otherwise no divergence: each is a faithful replacement. Where the
// original jumps through a state table past its end, indexes kind 0x96's table
// past its eight entries or ObjTrio past its three records, ours aborts with a
// message (docs/effect_4d.md section 6).
#include "game/effect_4d.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_4d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4d::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U AddressOf(const void* p) { return Low32(p); }

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32 (the original's 0x20-byte local).
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4d.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 up, Sprite_Current read afresh.
void Step() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}
// mov eax, [S]; mov dl, [eax + 9]; dec dl; mov [eax + 9], dl; mov eax, [S];
// test [eax + 9]: +9 down one; answers whether it is now 0.
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
}

// The draw mode most of these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page), dtd 1 - its fifth
// argument is the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
}

// The full-screen TILE 0x48CA90 and 0x48CC90 build after their draw mode: at
// the cursor (read after the commit), its colour Sprite_Current's +0x5D, +0x5E,
// +0x5F (read for each), at (0, 0) 320.0 x 240.0 - DIV-0041's fill - semi-
// transparent, untextured, committed at slot 5 (0x1C bytes).
void TintTile() {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(prim);
    prim[4] = S()[0x5D];
    prim[5] = S()[0x5E];
    prim[6] = S()[0x5F];
    SetUL(prim + 8, std::bit_cast<std::uint32_t>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
    SetUL(prim + 0xC, 0);
    SetUL(prim + 0x14, std::bit_cast<std::uint32_t>(Widescreen_FillWidth()));   // 0x43A00000, 320.0f narrow
    SetUL(prim + 0x18, 0x43700000u);   // 240.0f
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0x1C);
}

}  // namespace

// ===========================================================================
// The tint 0x48CA90 (kinds 0x91, 0x63, 0x75 and E4A's / E4C's states draw it)
// ===========================================================================

// ===========================================================================
// Kind 0x91: Effect_KindHandlers[0x91] (0x655594), EffectKind91_States (four)
// ===========================================================================

// ===========================================================================
// Kind 0x94: Effect_KindHandlers[0x94] (0x6555A0), EffectKind94_States (five)
// ===========================================================================

// ===========================================================================
// Kind 0x95: Effect_KindHandlers[0x95] (0x6555A4), EffectKind95_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x96: Effect_KindHandlers[0x96] (0x6555A8), EffectKind96_States (two)
// ===========================================================================

// ===========================================================================
// Kind 0x97: Effect_KindHandlers[0x97] (0x6555AC), EffectKind97_States (six)
// ===========================================================================

// The ring at the record's point (+0x34) of its size +0x2E, shade 7: each
// state's first draw (Sprite_Current read for the word, its point the same
// read's).
void Kind97Ring() {
    unsigned char* const s = S();
    SH_CALL(EffectKind97_DrawRing)(reinterpret_cast<const long*>(s + 0x34), rv::As<records::EffectWork>(s)->work_2e, 7);
}

// The two burst states' opening: +6 up, and a spark emitted when it was a
// multiple of 4 (one every fourth frame).
void Kind97Emit() {
    unsigned char* const s = S();
    const unsigned char n = s[6];
    s[6] = static_cast<unsigned char>(n + 1);
    if ((n & 3) == 0) SH_CALL(EffectKind97_EmitSpark)();
}

// ===========================================================================
// Kind 0x98: Effect_KindHandlers[0x98] (0x6555B0), EffectKind98_States (four)
// ===========================================================================

// The burst states' opening: Gte_PushMatrix, EffectGte_LoadMapCamera, the
// record's point (+0x34..+0x3C, copied to a local) projected into its own
// +0x74..+0x7F, Gte_PopMatrix.
void Kind98Project() {
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    long point[4];
    point[0] = so->x;
    point[1] = so->z;
    point[2] = so->height;
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(s + 0x74));
    SH_CALL(Gte_PopMatrix)();
}
// EffectKind98_DrawBurst(+0x32, +0xC, +0x10), Sprite_Current read once.
void Kind98Burst() {
    const unsigned char* const s = S();
    SH_CALL(EffectKind98_DrawBurst)(rv::As<records::EffectWork>(s)->work_32, Word(s + 0xC), Word(s + 0x10));
}

// ===========================================================================
// Kind 0x9A: Effect_KindHandlers[0x9A] (0x6555B8), EffectKind9A_States (eight)
// ===========================================================================

// Sprite_Objects record 1 made current (Sprite_Current kept), its word at
// `offset` (+0x38 or +0x34) moved by `step` four times through Sprite_Current
// read again, the kept record's +1 and +9 set, Sprite_Current put back.
void Kind9ANudge(unsigned offset, U step, unsigned char state, unsigned char count) {
    unsigned char* const kept = Sprite_Current;
    Sprite_Current = At(::bof3::Img(Sprite_Objects_addr) + 0xa4);
    for (unsigned n = 4; n != 0; --n) {
        unsigned char* const o = Sprite_Current;
        SetUL(o + offset, UL(o + offset) + step);
    }
    kept[1] = state;
    Sprite_Current = kept;
    kept[9] = count;
}

void Effect4D_Inject() {
    if (bof3::WantsShadow("effect_4d")) effect_4d::SelfTest();
}
