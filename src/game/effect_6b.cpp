// Round thirteen group E6B (docs/effect_6b.md): the 50 functions of
// analysis/round13_cut.tsv's group E6B, 0x50E400..0x510C8A, each read with
// capstone to its last instruction (2026-10-03). Effect_RunObjects (ours)
// makes each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current
// and calls Effect_KindHandlers[+5]; kind 0x18's EffectKind18_Run jumps through
// EffectKind18_States by +1 (the sub-kind, EffectKind18_Start copies it from
// +0xB). Nine of the band's sub-kinds dispatch again by +2 through a table of
// their own (none bounded by a compare); two are one state. What each is, as
// far as the code says:
//
//   sub-kinds 0x33, 0x34   two panels at a cell a variant table picks by the
//                          spawn's x cell (E6A's draw 0x50E1C0): when the leader
//                          stands at the cell's front they slide 0x100 over
//                          eight frames (sound 0x200), and back once the leader
//                          is two cells away (sound 0x201)
//   sub-kind 0x35          the same sliding the other way (0 .. -0x100), drawn
//                          by EffectKind18Sub35_Draw, one quad on the ground
//   sub-kind 0x37          the panels placed open, held eight frames, slid shut
//                          by 0x10 a frame (sound 0x201), then E6A's draw held
//   sub-kind 0x38          the same placed shut (-0x100) when the leader is at
//                          the front, slid open to 0, then EffectKind18Sub35_Draw
//                          held; placed open and drawn when the leader is not
//   sub-kind 0x3B          sub-kind 0x33's cycle at 0xC0 by 0x10, with a lift
//                          from its variant, drawn as two flat quads
//                          (EffectKind18Sub3B_Draw)
//   sub-kind 0x3D          while Cond_ByteFE is set, a ripple run through the
//                          map's corner heights over 35 x 35 cells about the
//                          leader, the leader's height then read again
//   sub-kind 0x40          three semi-transparent textured panes (a texture
//                          window 16 x 16 at (0x60, 0x10)) that wait on the
//                          chapter's run 0xC step 2 and Cond_ByteFE 1 and 2,
//                          then move a step a frame until Cond_ByteFE is 0
//   sub-kind 0x41          four quads turned about y by an angle from a curve
//                          table, story flags 0x4F / 0x50 and two of Capcom's
//                          helpers in no group
//   sub-kind 0x44          a sprite animation bank 0x2A8 set, then E6C's step
//   sub-kind 0x54          a semi-transparent blue fill over the frame
//                          (additive), its blue stepped through four levels
//                          every eighth frame - one of DIV-0041's full-frame
//                          fills (below)
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies.
// Sprite_Current is read again wherever the original reads [0x937F88] again
// after a call. Sub-kind 0x54's fill is DIV-0041's (section 3c): its corners
// are (Widescreen_FillX(), 0) .. (320 + Widescreen_Fill(), 240), which is the
// original's (0, 0) .. (320, 240) until Widescreen_ArmFills has run and
// whenever the picture is narrow. Otherwise no divergence: each is a faithful
// replacement. Where the original jumps through a sub-state table past its
// end or indexes a variant, step, curve or quad table past its room, ours
// aborts with a message (docs/effect_6b.md section 7).
#include "game/effect_6b.h"

#include <bit>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_6b_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6b::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
U AddressOf(const void* p) { return Low32(p); }
unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
const short* Vertex(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }
U SignedByte(unsigned char b) { return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(b))); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6b.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// The variant the spawn's x cell (+0x36, movsx) names, checked against the
// room its table has.
U Variant(const char* who, const unsigned char* s, U table, unsigned room) {
    const std::int32_t v = S16(s + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(room))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u its table 0x%X has room for - the original reads on "
                    "into the data after (docs/effect_6b.md section 7)",
                    who, (int)v, room, (unsigned)table);
    return static_cast<U>(v);
}

// cdq; xor eax, edx; sub eax, edx; cmp eax, n; jg: |d| as the original
// computes it - 0x80000000 stays itself, negative, and so counts as near.
std::int32_t Abs(U d) {
    const U m = static_cast<std::int32_t>(d) < 0 ? 0xFFFFFFFFu : 0u;
    return static_cast<std::int32_t>((d ^ m) - m);
}
// A point against a cell's centre on one axis: point - ((cell + 1) << 16 |
// 0x8000) (inc; shl 16; or dh, 0x80).
U FromCentre(U point, std::int32_t cell) { return point - ((static_cast<U>(cell + 1) << 16) | 0x8000u); }
// ... and against its edge: point - (cell << 16).
U FromEdge(U point, std::int32_t cell) { return point - (static_cast<U>(cell) << 16); }
// The tests every waiting state makes: |a| within `first`, then |b| or |b -
// 0x10000| within `second` (the cell or the one past it).
bool Within(U a, std::int32_t first, U b, std::int32_t second) {
    if (Abs(a) > first) return false;
    if (Abs(b) <= second) return true;
    return Abs(b - 0x10000u) <= second;
}
U LeaderX() { return static_cast<std::uint32_t>(ObjTrio[0].x); }
U LeaderZ() { return static_cast<std::uint32_t>(ObjTrio[0].z); }
// The leader at the record's cell: across the x axis first (the centre), then
// z - or, z_first, z first, then x.
bool LeaderAt(const unsigned char* s, bool z_first, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (z_first) return Within(FromCentre(LeaderZ(), cz), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx), first, FromEdge(LeaderZ(), cz), second);
}

// The quad's four corners projected into the POLY_FT4 at p, its depths set.
// The original also pushes a tenth pointer (a flag local) the projection does
// not read.
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                               reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                               reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}

// E6A's panel draw 0x50E1C0 (void, on Sprite_Current), raw until E6A merges.
void DrawE6A() { scenario_harness::Phase(at::kE6ADraw.v)(); }
using Draw = void (*)();

// The place state sub-kinds 0x33, 0x34, 0x35 and 0x3B share (0x50E420): +8 =
// whether the spawn's z cell is 0; the variant's cell into +0x36 / +0x3A (and,
// with `heights`, its lift into +0x3E); the slide +0x30 0, +2 up. The leader
// already at the cell's front (z first when +8): the slide `open` and +2 = 3.
// Then the draw.
void Place(const char* who, U cells, U heights, unsigned room, U open, Draw draw) {
    unsigned char* s = S();
    const U v = Variant(who, s, cells, room);
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    SetWord(S() + 0x36, At(cells + 2 * v)[0]);
    SetWord(S() + 0x3A, At(cells + 2 * v + 1)[0]);
    if (heights != 0) SetWord(S() + 0x3E, Word(At(heights + 2 * v)));
    SetWord(S() + 0x30, 0);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    s = S();
    if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {
        SetWord(s + 0x30, open);
        S()[2] = 3;
    }
    draw();
}
// The leader at the cell's front: sound 0x200 unless a message is up, +2 up
// (Sprite_Current read after the sound). The draw.
void WaitNear(Draw draw) {
    unsigned char* s = S();
    if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {
        if (Field_Request == 0) {
            SH_CALL(Sound_PlayEffect)(at::kSoundOpen);
            s = S();
        }
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    draw();
}
// Once the leader is two cells away (the front test at 0x20000 both ways
// fails), +2 up. The draw.
void WaitFar(Draw draw) {
    unsigned char* const s = S();
    if (!LeaderAt(s, s[8] != 0, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
    draw();
}
// The slide +0x30 moved by `step` (a word add); answers it as a signed word.
std::int32_t Slide(U step) {
    SetWord(S() + 0x30, Word(S() + 0x30) + step);
    return S16(S() + 0x30);
}
// Shut: the sound 0x201 unless a message is up, then +2 = 1 (read after the
// sound).
void Shut() {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(at::kSoundShut);
    S()[2] = 1;
}

}  // namespace

// ===========================================================================
// Sub-kind 0x33: EffectKind18_States[0x33] (0x654138), EffectKind18Sub33_States
// (five) by +2; drawn by E6A's 0x50E1C0
// ===========================================================================

// ===========================================================================
// Sub-kind 0x34: EffectKind18_States[0x34] (0x65413C), EffectKind18Sub34_States
// (five) by +2; 0x33's states with their own variant table
// ===========================================================================

// ===========================================================================
// Sub-kind 0x37: EffectKind18_States[0x37] (0x654148), EffectKind18Sub37_States
// (four) by +2: these three and E6A's draw 0x50E1C0 itself
// ===========================================================================

// ===========================================================================
// Sub-kind 0x35: EffectKind18_States[0x35] (0x654140), EffectKind18Sub35_States
// (five) by +2; 0x33's cycle sliding 0 .. -0x100, drawn by its own quad
// ===========================================================================

// ===========================================================================
// Sub-kind 0x38: EffectKind18_States[0x38] (0x65414C), EffectKind18Sub38_States
// (four) by +2: _Place, 0x37's _Hold, _Close, 0x35's draw held
// ===========================================================================

// ===========================================================================
// Sub-kind 0x3B: EffectKind18_States[0x3B] (0x654158), EffectKind18Sub3B_States
// (five) by +2; 0x33's cycle at 0xC0 by 0x10, drawn flat
// ===========================================================================

// ===========================================================================
// Sub-kind 0x54: EffectKind18_States[0x54] (0x6541BC) - one state
// ===========================================================================

// ===========================================================================
// Sub-kind 0x3D: EffectKind18_States[0x3D] (0x654160) - one state
// ===========================================================================

// ===========================================================================
// Sub-kind 0x40: EffectKind18_States[0x40] (0x65416C), EffectKind18Sub40_States
// (four) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x41: EffectKind18_States[0x41] (0x654170), EffectKind18Sub41_States
// (four) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x44: EffectKind18_States[0x44] (0x65417C), EffectKind18Sub44_States
// (two) by +2
// ===========================================================================

void Effect6B_Inject() {
    if (bof3::WantsShadow("effect_6b")) effect_6b::SelfTest();
}
