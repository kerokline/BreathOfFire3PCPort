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
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
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
U LeaderX() { return UL(At(at::kLeaderX)); }
U LeaderZ() { return UL(At(at::kLeaderZ)); }
// The leader at the record's cell: across the x axis first (the centre), then
// z - or, z_first, z first, then x.
bool LeaderAt(const unsigned char* s, bool z_first, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (z_first) return Within(FromCentre(LeaderZ(), cz), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx), first, FromEdge(LeaderZ(), cz), second);
}

// A vertex's ground point: (s16 + 0x4000) << 9, the 16.16 the map reads.
long Ground(const unsigned char* word) { return static_cast<long>(static_cast<U>(S16(word) + 0x4000) << 9); }
// movsx eax, ax; cdq; sub eax, edx; sar eax, 1: the height halved toward zero.
U Half(long height) { return static_cast<U>(static_cast<std::int16_t>(static_cast<U>(height) & 0xFFFFu) / 2); }

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
void DrawE6A() { scenario_harness::Phase(at::kE6ADraw)(); }
void Draw35() { SH_CALL(EffectKind18Sub35_Draw)(); }
void Draw3B() { SH_CALL(EffectKind18Sub3B_Draw)(); }
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
// Shut and on: the sound 0x201 unless a message is up, then +2 up (the record
// read again only when the sound was made - the same record either way unless
// the sound moves it).
void ShutOn() {
    unsigned char* s = S();
    if (Field_Request == 0) {
        SH_CALL(Sound_PlayEffect)(at::kSoundShut);
        s = S();
    }
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

}  // namespace

// ===========================================================================
// Sub-kind 0x33: EffectKind18_States[0x33] (0x654138), EffectKind18Sub33_States
// (five) by +2; drawn by E6A's 0x50E1C0
// ===========================================================================

// original 0x50E400 (hidden in E6A's 0x50E1C0): jmp [EffectKind18Sub33_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub33_Run(void) {
    Dispatch("EffectKind18Sub33_Run", AddressOf(EffectKind18Sub33_States), EffectKind18Sub33_States_count);
}
// original 0x50E420 (sub-state 0): Place, the variant of 0x65EBD4 (room 2),
// open 0x100.
extern "C" void __cdecl EffectKind18Sub33_Place(void) {
    Place("EffectKind18Sub33_Place", at::kSub33Cells, 0, at::kRoom2, 0x100, &DrawE6A);
}
// original 0x50E540 (sub-state 1).
extern "C" void __cdecl EffectKind18Sub33_WaitNear(void) { WaitNear(&DrawE6A); }
// original 0x50E620 (sub-state 2): the slide up 0x20; at 0x100 +2 up. A tail
// jump to the draw.
extern "C" void __cdecl EffectKind18Sub33_Open(void) {
    if (Slide(0x20) >= 0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    DrawE6A();
}
// original 0x50E640 (sub-state 3).
extern "C" void __cdecl EffectKind18Sub33_WaitFar(void) { WaitFar(&DrawE6A); }
// original 0x50E710 (sub-state 4): the slide down 0x20; at 0 or below Shut. A
// tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub33_Close(void) {
    if (Slide(0xFFE0u) <= 0) Shut();
    DrawE6A();
}

// ===========================================================================
// Sub-kind 0x34: EffectKind18_States[0x34] (0x65413C), EffectKind18Sub34_States
// (five) by +2; 0x33's states with their own variant table
// ===========================================================================

// original 0x50E750 (hidden in 0x50E1C0).
extern "C" void __cdecl EffectKind18Sub34_Run(void) {
    Dispatch("EffectKind18Sub34_Run", AddressOf(EffectKind18Sub34_States), EffectKind18Sub34_States_count);
}
// original 0x50E770 (sub-state 0): Place, the variant of 0x65EBEC (room 2),
// open 0x100.
extern "C" void __cdecl EffectKind18Sub34_Place(void) {
    Place("EffectKind18Sub34_Place", at::kSub34Cells, 0, at::kRoom2, 0x100, &DrawE6A);
}
// original 0x50E890 (sub-state 1).
extern "C" void __cdecl EffectKind18Sub34_WaitNear(void) { WaitNear(&DrawE6A); }
// original 0x50E970 (sub-state 2): as 0x33's.
extern "C" void __cdecl EffectKind18Sub34_Open(void) {
    if (Slide(0x20) >= 0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    DrawE6A();
}
// original 0x50E990 (sub-state 3).
extern "C" void __cdecl EffectKind18Sub34_WaitFar(void) { WaitFar(&DrawE6A); }
// original 0x50EA60 (sub-state 4): as 0x33's.
extern "C" void __cdecl EffectKind18Sub34_Close(void) {
    if (Slide(0xFFE0u) <= 0) Shut();
    DrawE6A();
}

// ===========================================================================
// Sub-kind 0x37: EffectKind18_States[0x37] (0x654148), EffectKind18Sub37_States
// (four) by +2: these three and E6A's draw 0x50E1C0 itself
// ===========================================================================

// original 0x50EAA0 (hidden in 0x50E1C0).
extern "C" void __cdecl EffectKind18Sub37_Run(void) {
    Dispatch("EffectKind18Sub37_Run", AddressOf(EffectKind18Sub37_States), EffectKind18Sub37_States_count);
}
// original 0x50EAC0 (sub-state 0): +8 = whether the spawn's z cell is 0; the
// variant's cell (0x65EC04, room 2); +0x3E = -0x80, the slide 0x100 (open),
// +2 up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub37_Place(void) {
    unsigned char* const s = S();
    const U v = Variant("EffectKind18Sub37_Place", s, at::kSub37Cells, at::kRoom2);
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    SetWord(S() + 0x36, At(at::kSub37Cells + 2 * v)[0]);
    SetWord(S() + 0x3A, At(at::kSub37Cells + 2 * v + 1)[0]);
    SetWord(S() + 0x3E, 0xFF80);
    SetWord(S() + 0x30, 0x100);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}
// original 0x50EB20 (sub-state 1, and EffectKind18Sub38_States[1]): once the
// count +9 has reached 8 (unsigned), +2 up; +9 up every frame. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub37_Hold(void) {
    unsigned char* s = S();
    if (s[9] >= 8) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
}
// original 0x50EB40 (sub-state 2): the slide down 0x10; at 0 or below ShutOn
// (+2 up, to E6A's draw held). A tail jump to E6A's draw.
extern "C" void __cdecl EffectKind18Sub37_Close(void) {
    if (Slide(0xFFF0u) <= 0) ShutOn();
    DrawE6A();
}

// ===========================================================================
// Sub-kind 0x35: EffectKind18_States[0x35] (0x654140), EffectKind18Sub35_States
// (five) by +2; 0x33's cycle sliding 0 .. -0x100, drawn by its own quad
// ===========================================================================

// original 0x50EB80 (hidden in 0x50E1C0).
extern "C" void __cdecl EffectKind18Sub35_Run(void) {
    Dispatch("EffectKind18Sub35_Run", AddressOf(EffectKind18Sub35_States), EffectKind18Sub35_States_count);
}
// original 0x50EBA0 (sub-state 0): Place, the variant of 0x65EC1C (room 4),
// open -0x100.
extern "C" void __cdecl EffectKind18Sub35_Place(void) {
    Place("EffectKind18Sub35_Place", at::kSub35Cells, 0, at::kRoom4, 0xFF00, &Draw35);
}
// original 0x50ECC0 (sub-state 1).
extern "C" void __cdecl EffectKind18Sub35_WaitNear(void) { WaitNear(&Draw35); }
// original 0x50EDA0 (sub-state 2): the slide down 0x20; at -0x100 or below +2
// up. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub35_Open(void) {
    if (Slide(0xFFE0u) <= -0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    Draw35();
}
// original 0x50EDC0 (sub-state 3).
extern "C" void __cdecl EffectKind18Sub35_WaitFar(void) { WaitFar(&Draw35); }
// original 0x50EE90 (sub-state 4): the slide up 0x20; at 0 or above Shut. A
// tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub35_Close(void) {
    if (Slide(0x20) >= 0) Shut();
    Draw35();
}

// original 0x50EED0 (sub-kind 0x35's draw; EffectKind18Sub38_States[3]; called
// or tail-jumped to by 0x35's states and 0x38's last): a draw mode (page 0x95)
// committed at slot 6 (0xC); one POLY_FT4: with +8 its corners lie across z
// at y (+0x3A << 7) - 0x3FC0 and run along x from (+0x36 << 7) - +0x30 -
// 0x3F40 to - 0x4040; without, across x at (+0x36 << 7) - 0x3FC0 and along z
// from (+0x3A << 7) + +0x30 - 0x3F40 to - 0x4040 (every word the low sixteen
// bits). Heights from the ground: corners 0 and 2 at vertex 0's point, 0x40 and
// 0x180 less half the ground, corners 1 and 3 at vertex 1's - four
// AreaMap_Elevation calls. Projected, the texture (+8 << 21) | 0x151010F,
// committed at slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub35_Draw(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    unsigned char* const v = Vertices();
    {
        const unsigned char* const s = S();
        if (s[8] != 0) {
            const U z = (static_cast<U>(Word(s + 0x3A)) << 7) - 0x3FC0u;
            SetWord(v + 0x1A, z);
            SetWord(v + 0x12, z);
            SetWord(v + 0xA, z);
            SetWord(v + 2, z);
            const U x0 = (static_cast<U>(Word(s + 0x36)) << 7) - Word(s + 0x30) - 0x3F40u;
            SetWord(v + 0x10, x0);
            SetWord(v, x0);
            const U x1 = (static_cast<U>(Word(s + 0x36)) << 7) - Word(s + 0x30) - 0x4040u;
            SetWord(v + 0x18, x1);
            SetWord(v + 8, x1);
        } else {
            const U x = (static_cast<U>(Word(s + 0x36)) << 7) - 0x3FC0u;
            SetWord(v + 0x18, x);
            SetWord(v + 0x10, x);
            SetWord(v + 8, x);
            SetWord(v, x);
            const U z0 = (static_cast<U>(Word(s + 0x3A)) << 7) + Word(s + 0x30) - 0x3F40u;
            SetWord(v + 0x12, z0);
            SetWord(v + 2, z0);
            const U z1 = (static_cast<U>(Word(s + 0x3A)) << 7) + Word(s + 0x30) - 0x4040u;
            SetWord(v + 0x1A, z1);
            SetWord(v + 0xA, z1);
        }
    }
    const long x0 = Ground(v), z0 = Ground(v + 2);
    long h = SH_CALL(AreaMap_Elevation)(x0, z0);
    SetWord(v + 4, 0x40u - Half(h));
    h = SH_CALL(AreaMap_Elevation)(x0, z0);
    const long x1 = Ground(v + 8), z1 = Ground(v + 0xA);
    SetWord(v + 0x14, 0x180u - Half(h));
    h = SH_CALL(AreaMap_Elevation)(x1, z1);
    SetWord(v + 0xC, 0x40u - Half(h));
    h = SH_CALL(AreaMap_Elevation)(x1, z1);
    SetWord(v + 0x1C, 0x180u - Half(h));
    Project(p);
    SH_CALL(Prim_SetTexture)((static_cast<U>(S()[8]) << 21) | 0x151010Fu, p, 1);
    SH_CALL(Gfx_CommitPrim)(6, 0x48);
}

// ===========================================================================
// Sub-kind 0x38: EffectKind18_States[0x38] (0x65414C), EffectKind18Sub38_States
// (four) by +2: _Place, 0x37's _Hold, _Close, 0x35's draw held
// ===========================================================================

// original 0x50F110 (hidden in 0x50EED0).
extern "C" void __cdecl EffectKind18Sub38_Run(void) {
    Dispatch("EffectKind18Sub38_Run", AddressOf(EffectKind18Sub38_States), EffectKind18Sub38_States_count);
}
// original 0x50F130 (sub-state 0): +8 = whether the spawn's z cell is 0; the
// variant's cell (0x65EC38, room 4); +0x3E = -0x80. The leader at the cell's
// front - z first, whatever +8 says: the slide -0x100 and +2 up (to the hold);
// otherwise the slide 0 and +2 = 3 (the draw held). Nothing drawn.
extern "C" void __cdecl EffectKind18Sub38_Place(void) {
    unsigned char* s = S();
    const U v = Variant("EffectKind18Sub38_Place", s, at::kSub38Cells, at::kRoom4);
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    SetWord(S() + 0x36, At(at::kSub38Cells + 2 * v)[0]);
    SetWord(S() + 0x3A, At(at::kSub38Cells + 2 * v + 1)[0]);
    SetWord(S() + 0x3E, 0xFF80);
    s = S();
    if (LeaderAt(s, true, 0x8000, 0x10000)) {
        SetWord(s + 0x30, 0xFF00);
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    } else {
        SetWord(s + 0x30, 0);
        S()[2] = 3;
    }
}
// original 0x50F1F0 (sub-state 2): the slide up 0x10; at 0 or above ShutOn
// (+2 up, to the draw held). A tail jump to 0x35's draw.
extern "C" void __cdecl EffectKind18Sub38_Close(void) {
    if (Slide(0x10) >= 0) ShutOn();
    Draw35();
}

// ===========================================================================
// Sub-kind 0x3B: EffectKind18_States[0x3B] (0x654158), EffectKind18Sub3B_States
// (five) by +2; 0x33's cycle at 0xC0 by 0x10, drawn flat
// ===========================================================================

// original 0x50F230 (hidden in 0x50EED0).
extern "C" void __cdecl EffectKind18Sub3B_Run(void) {
    Dispatch("EffectKind18Sub3B_Run", AddressOf(EffectKind18Sub3B_States), EffectKind18Sub3B_States_count);
}
// original 0x50F250 (sub-state 0): Place, the variant of 0x65EC58 (room 2) and
// its lift from 0x65EC54 into +0x3E, open 0xC0.
extern "C" void __cdecl EffectKind18Sub3B_Place(void) {
    Place("EffectKind18Sub3B_Place", at::kSub3BCells, at::kSub3BHeights, at::kRoom2, 0xC0, &Draw3B);
}
// original 0x50F380 (sub-state 1).
extern "C" void __cdecl EffectKind18Sub3B_WaitNear(void) { WaitNear(&Draw3B); }
// original 0x50F460 (sub-state 2): the slide up 0x10; at 0xC0 +2 up. A tail
// jump to the draw.
extern "C" void __cdecl EffectKind18Sub3B_Open(void) {
    if (Slide(0x10) >= 0xC0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    Draw3B();
}
// original 0x50F480 (sub-state 3).
extern "C" void __cdecl EffectKind18Sub3B_WaitFar(void) { WaitFar(&Draw3B); }
// original 0x50F550 (sub-state 4): the slide down 0x10; at 0 or below Shut. A
// tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub3B_Close(void) {
    if (Slide(0xFFF0u) <= 0) Shut();
    Draw3B();
}

// original 0x50F590 (sub-kind 0x3B's draw, called or tail-jumped to by its
// five states): with +8 the corners' z (+0x3A << 7) - 0x3FC0, else their x
// (+0x36 << 7) - 0x3FC0; corners 0 and 1 at +0x3E - 0x180, 2 and 3 at +0x3E
// (all read before any call). A draw mode (page 0x95) committed at slot 6
// (0xC); two POLY_FT4s, side 0 and 1: the other axis from the cell (<< 7) -
// 0x3F40 / - 0x4040 plus the slide +0x30 times the s8 0x65EC5C[side] (a 16-bit
// product); projected, the texture (+8 << 21) | (0x10C - side) | 0x12510000,
// committed at slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub3B_Draw(void) {
    unsigned char* const v = Vertices();
    {
        const unsigned char* const s = S();
        if (s[8] != 0) {
            const U z = (static_cast<U>(Word(s + 0x3A)) << 7) - 0x3FC0u;
            SetWord(v + 0x1A, z);
            SetWord(v + 0x12, z);
            SetWord(v + 0xA, z);
            SetWord(v + 2, z);
        } else {
            const U x = (static_cast<U>(Word(s + 0x36)) << 7) - 0x3FC0u;
            SetWord(v + 0x18, x);
            SetWord(v + 0x10, x);
            SetWord(v + 8, x);
            SetWord(v, x);
        }
        SetWord(v + 0xC, Word(s + 0x3E) - 0x180u);
        SetWord(v + 4, Word(s + 0x3E) - 0x180u);
        SetWord(v + 0x1C, Word(s + 0x3E));
        SetWord(v + 0x14, Word(s + 0x3E));
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    U texture = 0x10C;
    for (unsigned side = 0; side < 2; ++side, --texture) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        const unsigned char* const s = S();
        const U sign = SignedByte(At(at::kSub3BSides)[side]);
        if (s[8] != 0) {
            const U x0 = Word(s + 0x30) * sign + (static_cast<U>(Word(s + 0x36)) << 7) - 0x3F40u;
            SetWord(v + 0x10, x0);
            SetWord(v, x0);
            const U x1 = Word(s + 0x30) * sign + (static_cast<U>(Word(s + 0x36)) << 7) - 0x4040u;
            SetWord(v + 0x18, x1);
            SetWord(v + 8, x1);
        } else {
            const U z0 = Word(s + 0x30) * sign + (static_cast<U>(Word(s + 0x3A)) << 7) - 0x3F40u;
            SetWord(v + 0x12, z0);
            SetWord(v + 2, z0);
            const U z1 = Word(s + 0x30) * sign + (static_cast<U>(Word(s + 0x3A)) << 7) - 0x4040u;
            SetWord(v + 0x1A, z1);
            SetWord(v + 0xA, z1);
        }
        Project(p);
        SH_CALL(Prim_SetTexture)((static_cast<U>(S()[8]) << 21) | texture | 0x12510000u, p, 1);
        SH_CALL(Gfx_CommitPrim)(6, 0x48);
    }
}

// ===========================================================================
// Sub-kind 0x54: EffectKind18_States[0x54] (0x6541BC) - one state
// ===========================================================================

// original 0x50F780 (hidden in 0x50F590): a draw mode (page 0xB5, additive)
// committed at slot 3 (0xC); a semi-transparent POLY_F4 over the frame -
// DIV-0041's fill (0x50F7B5 is the original's mov ecx, 320.0f),
// (Widescreen_FillX(), 0)..(320 + Widescreen_Fill(), 240), the original's (0,
// 0)..(320, 240) narrow or unarmed - coloured (0, 0x30, 0x65EC60[+2] << 3),
// committed at slot 3 (0x38); every eighth frame (Frame_Counter & 7, read after
// the commit) +2 = (+2 + 1) & 3. E5G's EffectKind18Sub36_Pulse with its own
// table.
extern "C" void __cdecl EffectKind18Sub54_Pulse(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    const U left = std::bit_cast<U>(Widescreen_FillX());                                 // DIV-0041: -53 wide, 0 narrow
    const U right = std::bit_cast<U>(320.0f + static_cast<float>(Widescreen_Fill()));    // 0x43A00000, 320.0f narrow
    SetUL(p + 8, left);
    SetUL(p + 0xC, 0);
    SetUL(p + 0x14, right);
    SetUL(p + 0x18, 0);
    SetUL(p + 0x20, left);
    SetUL(p + 0x24, 0x43700000u);   // 240.0f
    SetUL(p + 0x2C, right);
    SetUL(p + 0x30, 0x43700000u);
    p[4] = 0;
    p[5] = 0x30;
    const unsigned step = S()[2];
    if (step >= at::kSub54Steps)
        bof3::Fatal("EffectKind18Sub54_Pulse: +2 is %u, past the %u steps of 0x%X - the original reads on into "
                    "EffectKind18Sub40_States (docs/effect_6b.md section 7)",
                    step, at::kSub54Steps, (unsigned)at::kSub54Blues);
    p[6] = static_cast<unsigned char>(At(at::kSub54Blues)[step] << 3);
    SH_CALL(Gfx_CommitPrim)(3, 0x38);
    if ((Frame_Counter & 7) == 0) {
        unsigned char* const s = S();
        s[2] = static_cast<unsigned char>((s[2] + 1) & 3);
    }
}

// ===========================================================================
// Sub-kind 0x3D: EffectKind18_States[0x3D] (0x654160) - one state
// ===========================================================================

// original 0x50F820 (hidden in 0x50F590): nothing while Cond_ByteFE is 0.
// Otherwise 35 rows from the leader's cell row (the high word of Field_Kind2Z)
// - 0x14, k = 0, 3, .. 0x66 the row's phase: a = (sin(((F + k) << 7) - 0x80) >>
// 10) .. (sin((F + k) << 7) >> 10) subtracted the other way, b = (sin((F + k +
// 3) << 7) >> 10) - (sin((F + k + 2) << 7) >> 10), each angle & 0xFFF and F
// Frame_Counter read before each Math_Sin; then 35 columns from the leader's
// cell column - 0x14: every cell inside the map (AreaMap_Header's width and
// height bytes, signed tests) has its AreaMap_Corners bytes 0 and 1 raised by
// a, 2 and 3 by b (byte adds). Then the leader's height word ObjTrio +0x3E from
// AreaMap_Elevation at its point, and MapView_Redraw = 2. Sprite_Current is
// not read.
extern "C" void __cdecl EffectKind18Sub3D_Ripple(void) {
    if (Cond_ByteFE == 0) return;
    const std::int32_t column = S16(At(at::kLeaderCellX)) - 0x14;
    std::int32_t row = S16(At(at::kLeaderCellZ)) - 0x14;
    unsigned char* const corners = reinterpret_cast<unsigned char*>(&AreaMap_Corners);
    for (U k = 0; k < 0x69; k += 3, ++row) {
        const std::int32_t s0 = SH_CALL(Math_Sin)(static_cast<int>((((Frame_Counter + k) << 7) - 0x80u) & 0xFFFu)) >> 10;
        const std::int32_t s1 = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + k) << 7) & 0xFFFu)) >> 10;
        const unsigned char a = static_cast<unsigned char>(s1 - s0);
        const std::int32_t s2 = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + k + 3) << 7) & 0xFFFu)) >> 10;
        const std::int32_t s3 = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + k + 2) << 7) & 0xFFFu)) >> 10;
        const unsigned char b = static_cast<unsigned char>(s2 - s3);
        for (std::int32_t i = 0; i < 0x23; ++i) {
            const std::int32_t width = AreaMap_Header[0], height = AreaMap_Header[1];
            if (row < 0 || row >= height) continue;
            const std::int32_t col = i + column;
            if (col < 0 || col >= width) continue;
            unsigned char* const c = corners + 4 * static_cast<U>(row * width + col);
            c[0] = static_cast<unsigned char>(c[0] + a);
            c[1] = static_cast<unsigned char>(c[1] + a);
            c[2] = static_cast<unsigned char>(c[2] + b);
            c[3] = static_cast<unsigned char>(c[3] + b);
        }
    }
    const long h = SH_CALL(AreaMap_Elevation)(Long(At(at::kLeaderX)), Long(At(at::kLeaderZ)));
    SetWord(At(at::kLeaderHeight), static_cast<U>(h));
    MapView_Redraw = 2;
}

// ===========================================================================
// Sub-kind 0x40: EffectKind18_States[0x40] (0x65416C), EffectKind18Sub40_States
// (four) by +2
// ===========================================================================

// original 0x50FA00 (hidden in 0x50F590).
extern "C" void __cdecl EffectKind18Sub40_Run(void) {
    Dispatch("EffectKind18Sub40_Run", AddressOf(EffectKind18Sub40_States), EffectKind18Sub40_States_count);
}
// original 0x50FA20 (sub-state 0): when the chapter's run byte (MoveScript_Var7)
// is 0xC and its step byte 0x8034E5 is 2: sound 0x202 and +2 up (read after the
// sound). Nothing drawn.
extern "C" void __cdecl EffectKind18Sub40_WaitCue(void) {
    if (static_cast<unsigned char>(MoveScript_Var7) != 0xC || At(at::kStep)[0] != 2) return;
    SH_CALL(Sound_PlayEffect)(at::kSoundCue);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}
// original 0x50FA50 (sub-state 1): when Cond_ByteFE is 1, +2 up and the panes
// at step 0; otherwise nothing drawn.
extern "C" void __cdecl EffectKind18Sub40_WaitOne(void) {
    if (Cond_ByteFE != 1) return;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub40_Draw)(0);
}
// original 0x50FA70 (sub-state 2): when Cond_ByteFE is 2, sounds 0x206 and 0x203
// and +2 up (read after them). The panes at step 0.
extern "C" void __cdecl EffectKind18Sub40_WaitTwo(void) {
    if (Cond_ByteFE == 2) {
        SH_CALL(Sound_PlayEffect)(at::kSoundEndA);
        SH_CALL(Sound_PlayEffect)(at::kSoundEnd);
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub40_Draw)(0);
}
// original 0x50FAB0 (sub-state 3): +9 up; when Cond_ByteFE is 0 Effect_Release.
// The panes at step +9 (read after the release).
extern "C" void __cdecl EffectKind18Sub40_Move(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (Cond_ByteFE == 0) SH_CALL(Effect_Release)();
    SH_CALL(EffectKind18Sub40_Draw)(S()[9]);
}

// original 0x50FAE0 (sub-kind 0x40's draw, called by its last three states): a
// RECT {0x60, 0x10, 0x10, 0x10} put at the packet cursor (the cursor past it)
// as the texture window of a draw mode (page 0x1B) committed at slot 5 (0xC);
// three semi-transparent POLY_FT4s (tpage 0x7B, clut 0x78C6, colour 0x80 each),
// pane i from the four bytes of 0x65EC74 + 4i: x from (t0 << 5) + step to
// ((t0 + t2) << 5) + step, z likewise from t1 and t3, each times 4 - 0x4040;
// every corner's height 6 * step - 0x380; the third pane's corner 0 moved by 8
// * step in x and z; u, v 0..(t2 << 4) - 1, (t3 << 4) - 1; projected and
// committed at slot 5 (0x48). Then a RECT {0, 0, 0x100, 0x100} likewise
// (page 0x1B, slot 5).
extern "C" void __cdecl EffectKind18Sub40_Draw(int step) {
    const U f = static_cast<U>(step);
    {
        unsigned char* const rect = Gfx_PacketNext;
        Gfx_PacketNext = rect + 8;
        SetWord(rect, 0x60);
        SetWord(rect + 2, 0x10);
        SetWord(rect + 6, 0x10);
        SetWord(rect + 4, 0x10);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1B, AddressOf(rect));
        SH_CALL(Gfx_CommitPrim)(5, 0xC);
    }
    const U height = 6 * f - 0x380u;
    unsigned char* const v = Vertices();
    for (unsigned i = 0; i < at::kSub40PaneCount; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetWord(p + 0x26, 0x7B);
        SetWord(p + 0x16, 0x78C6);
        const unsigned char* const t = At(at::kSub40Panes + 4 * i);
        const U x0 = ((static_cast<U>(t[0]) << 5) + f) * 4 - 0x4040u;
        SetWord(v + 0x10, x0);
        SetWord(v, x0);
        const U x1 = ((static_cast<U>(t[0] + t[2]) << 5) + f) * 4 - 0x4040u;
        SetWord(v + 0x18, x1);
        SetWord(v + 8, x1);
        const U z0 = ((static_cast<U>(t[1]) << 5) + f) * 4 - 0x4040u;
        SetWord(v + 0xA, z0);
        SetWord(v + 2, z0);
        const U z1 = ((static_cast<U>(t[1] + t[3]) << 5) + f) * 4 - 0x4040u;
        SetWord(v + 0x1A, z1);
        SetWord(v + 0x12, z1);
        SetWord(v + 0x1C, height);
        SetWord(v + 0x14, height);
        SetWord(v + 0xC, height);
        SetWord(v + 4, height);
        if (i == 2) {
            SetWord(v, UL(v + 0x10) + 8 * f);
            SetWord(v + 2, 8 * f + z0);
        }
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x24] = static_cast<unsigned char>((t[2] << 4) - 1);
        p[0x35] = static_cast<unsigned char>((t[3] << 4) - 1);
        p[0x44] = static_cast<unsigned char>((t[2] << 4) - 1);
        p[0x45] = static_cast<unsigned char>((t[3] << 4) - 1);
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
        Project(p);
        SH_CALL(Gfx_CommitPrim)(5, 0x48);
    }
    unsigned char* const rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    SetWord(rect, 0);
    SetWord(rect + 2, 0);
    SetWord(rect + 6, 0x100);
    SetWord(rect + 4, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1B, AddressOf(rect));
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// ===========================================================================
// Sub-kind 0x41: EffectKind18_States[0x41] (0x654170), EffectKind18Sub41_States
// (four) by +2
// ===========================================================================

namespace {
// The curve byte at +9 << 4 (the original's (b << 12) / 256 with b unsigned).
U Curve(const char* who, unsigned c) {
    if (c >= at::kSub41CurveRoom)
        bof3::Fatal("%s: +9 is %u, past the %u bytes of 0x%X - the original reads on into EffectKind18Sub41_States "
                    "(docs/effect_6b.md section 7)",
                    who, c, at::kSub41CurveRoom, (unsigned)at::kSub41Curve);
    return static_cast<U>(static_cast<std::int32_t>(static_cast<U>(At(at::kSub41Curve)[c]) << 12) / 256);
}
bool FlagSet(unsigned index) {
    return (SH_CALL(Flags_Test)(At(at::kStoryFlags), index) & 0xFFu) != 0;
}
}  // namespace

// original 0x50FD30 (hidden in 0x50FAE0).
extern "C" void __cdecl EffectKind18Sub41_Run(void) {
    Dispatch("EffectKind18Sub41_Run", AddressOf(EffectKind18Sub41_States), EffectKind18Sub41_States_count);
}
// original 0x50FD50 (sub-state 0): Cond_ByteFE = 2, +2 up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub41_Start(void) {
    unsigned char* const s = S();
    Cond_ByteFE = 2;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}
// original 0x50FD60 (sub-state 1): when Cond_ByteFE is 1, +9 = 0, +2 up and the
// draw (variant 0, angle 0); otherwise nothing drawn.
extern "C" void __cdecl EffectKind18Sub41_WaitOne(void) {
    if (Cond_ByteFE != 1) return;
    S()[9] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub41_Draw)(0, 0);
}
// original 0x50FD90 (sub-state 2): the draw (variant +9 > 0xB, angle the curve
// at +9); +9 up; past 0xF: sound 0x200 when story flag 0x4F is set, else 0x201,
// +0xA = 0, +2 up (each read afresh).
extern "C" void __cdecl EffectKind18Sub41_Turn(void) {
    const unsigned c = S()[9];
    const U angle = Curve("EffectKind18Sub41_Turn", c);
    SH_CALL(EffectKind18Sub41_Draw)(c > 0xB ? 1u : 0u, static_cast<int>(angle));
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] <= 0xF) return;
    const unsigned short sound = FlagSet(0x4F) ? at::kSoundOpen : at::kSoundShut;
    SH_CALL(Sound_PlayEffect)(sound);
    S()[0xA] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}
// original 0x50FE20 (sub-state 3): while +9 is not 0, the draw (variant 1, the
// curve at +9) and +9 up; at 0x17 story flag 0x50 set, MoveCmd_TestFB(0x13,
// 0x25) and +9 = 0. While story flag 0x4F is set: Capcom's 0x5100B0(0x65ECAC[+0xA]
// << 2, 0x65ECC8[+0xA >> 1]) when +0xA is below 0x19, and 0x5101C0(+0xA). +0xA
// up; past 0x30 a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind18Sub41_Hold(void) {
    const unsigned c = S()[9];
    if (c != 0) {
        const U angle = Curve("EffectKind18Sub41_Hold", c);
        SH_CALL(EffectKind18Sub41_Draw)(1, static_cast<int>(angle));
        S()[9] = static_cast<unsigned char>(S()[9] + 1);
    }
    if (S()[9] == 0x17) {
        SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x50);
        SH_CALL(MoveCmd_TestFB)(0x13, 0x25);
        S()[9] = 0;
    }
    if (FlagSet(0x4F)) {
        const unsigned d = S()[0xA];
        if (d < 0x19) {
            const U first = static_cast<U>(At(at::kSub41MarkA)[d]) << 2;
            const U second = At(at::kSub41MarkB)[d >> 1];
            SH_AT(void(__cdecl*)(U, U), at::kWaveMark)(first, second);
        }
        SH_AT(void(__cdecl*)(U), at::kWaveStep)(S()[0xA]);
    }
    S()[0xA] = static_cast<unsigned char>(S()[0xA] + 1);
    if (S()[0xA] > 0x30) SH_CALL(Effect_Release)();
}

// original 0x50FF10 (sub-kind 0x41's draw, called by its states 1..3): a draw
// mode (page 0x95) committed at slot 5 (0xC); the GTE matrix pushed; a matrix
// turned (0, -angle, 0) about the point (-0x37C0, -0x2DC0, -0x480) rotated and
// translated (Gte_RotTrans into the matrix's translation, Gte_RotMatrix, the
// camera's Gte_MulMatrix0) and set; four POLY_FT4s, quad q = 0x65ED08[variant *
// 4 + i]: its four vertices from the twelve bytes at 0x65ECD8 + 12q (x, z,
// -y, each << 7), projected, the texture 0x65ED10[q] + ((s8 0x65ED20[q] *
// angle) & ~0x7FF) << 8, committed at slot 5 (0x48); the matrix popped.
extern "C" void __cdecl EffectKind18Sub41_Draw(unsigned variant, int angle) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    alignas(4) unsigned char angles[8] = {};
    alignas(4) unsigned char point[8] = {};
    alignas(4) unsigned char matrix[0x20] = {};
    SetWord(angles + 4, 0);
    SetWord(angles + 2, 0u - static_cast<U>(angle));
    SetWord(angles, 0);
    SetWord(point, 0xC840);
    SetWord(point + 2, 0xD240);
    SetWord(point + 4, 0xFB80);
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(Gte_RotTrans)(reinterpret_cast<const short*>(point), reinterpret_cast<long*>(matrix + 0x14));
    SH_CALL(Gte_RotMatrix)(reinterpret_cast<const short*>(angles), reinterpret_cast<short*>(matrix));
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<const short*>(matrix), reinterpret_cast<short*>(matrix));
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    if (variant >= at::kSub41Variants)
        bof3::Fatal("EffectKind18Sub41_Draw: variant %u, past the %u rows of 0x%X - the original reads on into the "
                    "texture words (docs/effect_6b.md section 7)",
                    variant, at::kSub41Variants, (unsigned)at::kSub41Order);
    unsigned char* const v = Vertices();
    for (unsigned i = 0; i < 4; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        const unsigned q = At(at::kSub41Order + 4 * variant)[i];
        if (q >= at::kSub41QuadCount)
            bof3::Fatal("EffectKind18Sub41_Draw: quad %u, past the %u of 0x%X (docs/effect_6b.md section 7)", q,
                        at::kSub41QuadCount, (unsigned)at::kSub41Quads);
        const unsigned char* const b = At(at::kSub41Quads + 12 * q);
        for (unsigned j = 0; j < 4; ++j) {
            SetWord(v + 8 * j, static_cast<U>(b[3 * j]) << 7);
            SetWord(v + 8 * j + 2, static_cast<U>(b[3 * j + 1]) << 7);
            SetWord(v + 8 * j + 4, (0u - b[3 * j + 2]) << 7);
        }
        Project(p);
        const U turn = (SignedByte(At(at::kSub41Turns)[q]) * static_cast<U>(angle)) & 0xFFFFF800u;
        SH_CALL(Prim_SetTexture)((turn << 8) + UL(At(at::kSub41Textures + 4 * q)), p, 1);
        SH_CALL(Gfx_CommitPrim)(5, 0x48);
    }
    SH_CALL(Gte_PopMatrix)();
}

// ===========================================================================
// Sub-kind 0x44: EffectKind18_States[0x44] (0x65417C), EffectKind18Sub44_States
// (two) by +2
// ===========================================================================

// original 0x510C20 (hidden in Capcom's 0x510BB0, a helper in no group).
extern "C" void __cdecl EffectKind18Sub44_Run(void) {
    Dispatch("EffectKind18Sub44_Run", AddressOf(EffectKind18Sub44_States), EffectKind18Sub44_States_count);
}
// original 0x510C40 (sub-state 0): Sprite_SetAnimationBank(0x2A8), +0x24 = 0,
// Sprite_SetAnimation(0), +0x29 = 6, +0x48 = 0, +2 up (each read afresh).
extern "C" void __cdecl EffectKind18Sub44_Start(void) {
    SH_CALL(Sprite_SetAnimationBank)(0x2A8);
    S()[0x24] = 0;
    SH_CALL(Sprite_SetAnimation)(0);
    S()[0x29] = 6;
    S()[0x48] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}
// original 0x510C80 (sub-state 1): E6C's 0x510C90, then a tail jump to E6C's
// 0x510EB0 (both void, on Sprite_Current; raw until E6C merges).
extern "C" void __cdecl EffectKind18Sub44_Step(void) {
    scenario_harness::Phase(at::kE6CStep)();
    scenario_harness::Phase(at::kE6CTail)();
}

void Effect6B_Inject() {
    if (bof3::WantsShadow("effect_6b")) effect_6b::SelfTest();
    BOF3_INJECT(EffectKind18Sub33_Run);
    BOF3_INJECT(EffectKind18Sub33_Place);
    BOF3_INJECT(EffectKind18Sub33_WaitNear);
    BOF3_INJECT(EffectKind18Sub33_Open);
    BOF3_INJECT(EffectKind18Sub33_WaitFar);
    BOF3_INJECT(EffectKind18Sub33_Close);
    BOF3_INJECT(EffectKind18Sub34_Run);
    BOF3_INJECT(EffectKind18Sub34_Place);
    BOF3_INJECT(EffectKind18Sub34_WaitNear);
    BOF3_INJECT(EffectKind18Sub34_Open);
    BOF3_INJECT(EffectKind18Sub34_WaitFar);
    BOF3_INJECT(EffectKind18Sub34_Close);
    BOF3_INJECT(EffectKind18Sub37_Run);
    BOF3_INJECT(EffectKind18Sub37_Place);
    BOF3_INJECT(EffectKind18Sub37_Hold);
    BOF3_INJECT(EffectKind18Sub37_Close);
    BOF3_INJECT(EffectKind18Sub35_Run);
    BOF3_INJECT(EffectKind18Sub35_Place);
    BOF3_INJECT(EffectKind18Sub35_WaitNear);
    BOF3_INJECT(EffectKind18Sub35_Open);
    BOF3_INJECT(EffectKind18Sub35_WaitFar);
    BOF3_INJECT(EffectKind18Sub35_Close);
    BOF3_INJECT(EffectKind18Sub35_Draw);
    BOF3_INJECT(EffectKind18Sub38_Run);
    BOF3_INJECT(EffectKind18Sub38_Place);
    BOF3_INJECT(EffectKind18Sub38_Close);
    BOF3_INJECT(EffectKind18Sub3B_Run);
    BOF3_INJECT(EffectKind18Sub3B_Place);
    BOF3_INJECT(EffectKind18Sub3B_WaitNear);
    BOF3_INJECT(EffectKind18Sub3B_Open);
    BOF3_INJECT(EffectKind18Sub3B_WaitFar);
    BOF3_INJECT(EffectKind18Sub3B_Close);
    BOF3_INJECT(EffectKind18Sub3B_Draw);
    BOF3_INJECT(EffectKind18Sub54_Pulse);
    BOF3_INJECT(EffectKind18Sub3D_Ripple);
    BOF3_INJECT(EffectKind18Sub40_Run);
    BOF3_INJECT(EffectKind18Sub40_WaitCue);
    BOF3_INJECT(EffectKind18Sub40_WaitOne);
    BOF3_INJECT(EffectKind18Sub40_WaitTwo);
    BOF3_INJECT(EffectKind18Sub40_Move);
    BOF3_INJECT(EffectKind18Sub40_Draw);
    BOF3_INJECT(EffectKind18Sub41_Run);
    BOF3_INJECT(EffectKind18Sub41_Start);
    BOF3_INJECT(EffectKind18Sub41_WaitOne);
    BOF3_INJECT(EffectKind18Sub41_Turn);
    BOF3_INJECT(EffectKind18Sub41_Hold);
    BOF3_INJECT(EffectKind18Sub41_Draw);
    BOF3_INJECT(EffectKind18Sub44_Run);
    BOF3_INJECT(EffectKind18Sub44_Start);
    BOF3_INJECT(EffectKind18Sub44_Step);
}
