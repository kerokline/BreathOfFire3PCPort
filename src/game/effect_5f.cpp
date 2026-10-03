// Round thirteen group E5F (docs/effect_5f.md): the 48 functions of
// analysis/round13_cut.tsv's group E5F, 0x508CC0..0x50AF8D, and one start the
// cut does not list (sub-kind 0x42's draw 0x509A70, a catalog row of no group
// whose only callers are that sub-kind's six states), each read with capstone
// to its last instruction. Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; kind 0x18's handler jumps through
// EffectKind18_States by +1 to a sub-kind's program, here a dispatcher by +2
// through the sub-kind's state table (none bounded by a compare). What each
// sub-kind is, as far as the code says:
//
//   0x27, 0x28, 0x29, 0x2A   a textured panel on the ground at the record's
//               cell (one quad, or two halves for 0x29 and 0x2A), its side
//               (+8) the axis the cell's z word picks; it slides by +0x30 when
//               the leader (ObjTrio record 0) comes within the cell's gate and
//               back once the leader has left two cells behind. 0x28 waits on
//               story flag 0x54 first.
//   0x48, 0x49  0x29's panel and states, opened by Cond_ByteFE 0x10 (0x48:
//               then it stays open) or 0x20 (0x49, which also opens to the
//               leader once story flag 0x8F is set)
//   0x42        six pieces raised from the ground at three places by a story
//               flag (the place's +0xB) when no party member stands within
//               reach; the cells marked on the map view as they rise
//   0x3C        two quads at a fixed point that rise by +0x30 once story flag
//               0x67 is set, then release the record
//   0x4B, 0x4C  one program: three quads drifting along z, the width pulsing
//               with the frame counter, drawn only on Draw_PassFlags bit 2
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. No divergence: each is a
// faithful replacement, but for one stack word the original reads and never
// writes (EffectKind18Sub4B_Run, docs/effect_5f.md section 6), where ours
// takes the value the rest of the quad does. Where the original jumps through
// a state table past its end or indexes a table or the party past its records,
// ours aborts with a message (section 6).
#include "game/effect_5f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_5f_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5f::at;
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
U W(U a) { return Word(At(a)); }
U B(U a) { return At(a)[0]; }
std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }
std::int32_t S8(unsigned char v) { return static_cast<signed char>(v); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// cdq; xor; sub: |v| in 32 bits (0x80000000 stays itself, below every bound)
std::int32_t Abs(U v) {
    const U m = static_cast<U>(static_cast<std::int32_t>(v) >> 31);
    return static_cast<std::int32_t>((v ^ m) - m);
}
// movsx eax, ax; cdq; sub eax, edx; sar eax, 1: half the height, toward zero
U Half(long e) { return static_cast<U>(S16(static_cast<U>(e)) / 2); }

unsigned char* Vertex(U a) { return At(a); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next sub-kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5f.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

// A placement index the start states read from +0x36 (movsx: a signed word),
// checked against its table's records.
U Place(const char* who, U table, unsigned count) {
    const std::int32_t i = S16(Word(S() + 0x36));
    if (i < 0 || i >= static_cast<std::int32_t>(count))
        bof3::Fatal("%s: +0x36 is %d, outside the %u records of 0x%X - the original reads what lies beside them "
                    "(docs/effect_5f.md section 6)",
                    who, (int)i, count, (unsigned)table);
    return static_cast<U>(i);
}

// The leader against the record's cell (+0x36 x, +0x3A z, s16 cells; the
// leader's point 16.16). Along the axis +8 picks (z when set), the leader's
// distance from the cell's far edge's middle ((c + 1) << 16 | 0x8000) is at
// most `edge`; across it, the leader is within `cross` of the cell's or the
// next cell's start. The gate's test is (0x8000, 0x10000); "two cells away"
// is its negation at (0x20000, 0x20000).
bool Within(const unsigned char* s, bool z_axis, std::int32_t edge, std::int32_t cross) {
    const U lx = UL(at::kLeaderX), lz = UL(at::kLeaderZ);
    const U x = static_cast<U>(S16(Word(s + 0x36))), z = static_cast<U>(S16(Word(s + 0x3A)));
    U along, across;
    if (z_axis) {
        along = lz - (((z + 1) << 16) | 0x8000u);
        across = lx - (x << 16);
    } else {
        along = lx - (((x + 1) << 16) | 0x8000u);
        across = lz - (z << 16);
    }
    return Abs(along) <= edge && (Abs(across) <= cross || Abs(across - 0x10000u) <= cross);
}
bool Near(const unsigned char* s, bool z_axis) { return Within(s, z_axis, 0x8000, 0x10000); }
bool Far(const unsigned char* s, bool z_axis) { return !Within(s, z_axis, 0x20000, 0x20000); }

// The leader within the gate: Sound_PlayEffect(0x200) unless Field_Request,
// then +2 up (Sprite_Current read again after the sound).
void StepIfNear(bool z_axis) {
    unsigned char* s = S();
    if (!Near(s, z_axis)) return;
    if (Field_Request == 0) {
        SH_CALL(Sound_PlayEffect)(0x200);
        s = S();
    }
    ++s[2];
}

// +2 back to 1 with Sound_PlayEffect(0x201) unless Field_Request.
void Closed() {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x201);
    S()[2] = 1;
}

// The quad's projection, its depths: the four vertices of Prim_VertexScratch
// into the packet's four screen points (the original pushes a tenth word, a
// local, which Gte_RotTransPers4 does not read).
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(reinterpret_cast<const short*>(Vertex(at::kV0)), reinterpret_cast<const short*>(Vertex(at::kV1)),
                               reinterpret_cast<const short*>(Vertex(at::kV2)), reinterpret_cast<const short*>(Vertex(at::kV3)),
                               reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
                               reinterpret_cast<float*>(p + 0x28), reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}

long Elevation(U x, U z) { return SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z)); }
U GroundOf(U word) { return static_cast<U>(S16(word) + 0x4000) << 9; }

// The ground under v0 and v1 (each vertex's +0 x and +2 z words as 16.16 map
// points: (w + 0x4000) << 9), four AreaMap_Elevation calls, the second pair's
// point read after the second call: v0 and v1 `low` less half the ground's
// height, v2 and v3 `high` less it (sub-kinds 0x27 and 0x28: 0x40 and 0x180).
void GroundFixed(U low, U high) {
    unsigned char* const v0 = Vertex(at::kV0);
    unsigned char* const v1 = Vertex(at::kV1);
    U gx = GroundOf(Word(v0)), gz = GroundOf(Word(v0 + 2));
    long e = Elevation(gx, gz);
    SetWord(v0 + 4, low - Half(e));
    e = Elevation(gx, gz);
    gz = GroundOf(Word(v1 + 2));
    gx = GroundOf(Word(v1));
    SetWord(Vertex(at::kV2) + 4, high - Half(e));
    e = Elevation(gx, gz);
    SetWord(v1 + 4, low - Half(e));
    e = Elevation(gx, gz);
    SetWord(Vertex(at::kV3) + 4, high - Half(e));
}

// The same for the lifted panels (sub-kinds 0x29 and 0x2A): every height less
// half the ground's and less +0x3E (Sprite_Current read after each call), v0
// and v1 0x180 lower.
void GroundLifted() {
    unsigned char* const v0 = Vertex(at::kV0);
    unsigned char* const v1 = Vertex(at::kV1);
    U gz = GroundOf(Word(v0 + 2)), gx = GroundOf(Word(v0));
    long e = Elevation(gx, gz);
    SetWord(v0 + 4, 0u - Half(e) - Word(S() + 0x3E) - 0x180u);
    e = Elevation(gx, gz);
    gz = GroundOf(Word(v1 + 2));
    gx = GroundOf(Word(v1));
    SetWord(Vertex(at::kV2) + 4, 0u - Half(e) - Word(S() + 0x3E));
    e = Elevation(gx, gz);
    SetWord(v1 + 4, 0u - Half(e) - Word(S() + 0x3E) - 0x180u);
    e = Elevation(gx, gz);
    SetWord(Vertex(at::kV3) + 4, 0u - Half(e) - Word(S() + 0x3E));
}

// Gpu_SetDrawMode(cursor, 0, 0, 0x95, 0): every draw here opens with it.
void DrawMode() { SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0); }
// The next packet as a shaded POLY_FT4.
unsigned char* Quad() {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    return p;
}
void LinkAtRecord(int dy, unsigned size) {
    unsigned char* const s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), dy, size);
}

// The panel's four corners on the axis +8 does not pick: c = (w << 7) - 0x3FC0
// of the record's other cell word, into the four vertices' word `off`.
void Across(unsigned off, U c) {
    SetWord(Vertex(at::kV3) + off, c);
    SetWord(Vertex(at::kV2) + off, c);
    SetWord(Vertex(at::kV1) + off, c);
    SetWord(Vertex(at::kV0) + off, c);
}

}  // namespace

// ===========================================================================
// Sub-kind 0x27: EffectKind18_States[0x27] (0x654108), EffectKind18Sub27_States (five)
// ===========================================================================

// original 0x508CC0 (hidden in E5E's 0x508BA0): jmp [EffectKind18Sub27_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub27_Run(void) {
    Dispatch("EffectKind18Sub27_Run", AddressOf(EffectKind18Sub27_States), EffectKind18Sub27_States_count);
}

// original 0x509030 (states 0..4 call or jump to it): one shaded POLY_FT4, a
// draw mode linked at the record's point (dy -2) first. Along +8's axis the
// panel spans the cell's 0x80 less +0x30 (its slide), across it the cell's
// width; each corner 0x40 / 0x180 above half the ground's height; texture
// +0x2E (signed) | +8 << 21 | 0x1500000; linked at the point with 0x48 bytes.
extern "C" void __cdecl EffectKind18Sub27_Draw(void) {
    DrawMode();
    LinkAtRecord(-2, 0xC);
    unsigned char* const p = Quad();
    unsigned char* s = S();
    if (s[8] != 0) {
        Across(2, (Word(s + 0x3A) << 7) - 0x3FC0u);
        const U a = (Word(s + 0x36) << 7) - Word(s + 0x30) - 0x3F40u;
        SetWord(Vertex(at::kV2), a);
        SetWord(Vertex(at::kV0), a);
        const U b = (Word(s + 0x36) << 7) - Word(s + 0x30) - 0x4040u;
        SetWord(Vertex(at::kV3), b);
        SetWord(Vertex(at::kV1), b);
    } else {
        Across(0, (Word(s + 0x36) << 7) - 0x3FC0u);
        const U a = (Word(s + 0x3A) << 7) + Word(s + 0x30) - 0x3F40u;
        SetWord(Vertex(at::kV2) + 2, a);
        SetWord(Vertex(at::kV0) + 2, a);
        const U b = (Word(s + 0x3A) << 7) + Word(s + 0x30) - 0x4040u;
        SetWord(Vertex(at::kV3) + 2, b);
        SetWord(Vertex(at::kV1) + 2, b);
    }
    GroundFixed(0x40, 0x180);
    Project(p);
    s = S();
    SH_CALL(Prim_SetTexture)((static_cast<U>(s[8]) << 21) | static_cast<U>(S16(Word(s + 0x2E))) | 0x1500000u, p, 1);
    LinkAtRecord(-2, 0x48);
}

// original 0x508CE0 (state 0, hidden in 0x508BA0): +8 = (+0x3A word is 0); the
// placement +0x36 (two) gives the cell (+0x36, +0x3A), +0x3E and +0x2E; +0x30
// 0, +2 up; the leader within the gate: +0x30 -0x100, +2 = 3 (open at once).
// The draw.
extern "C" void __cdecl EffectKind18Sub27_Start(void) {
    unsigned char* const s = S();
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    const U i = Place("EffectKind18Sub27_Start", at::kSub27Cell, at::kSub27Places);
    SetWord(s + 0x36, B(at::kSub27Cell + 2 * i));
    SetWord(s + 0x3A, B(at::kSub27Cell + 2 * i + 1));
    SetWord(s + 0x3E, W(at::kSub27Height + 2 * i));
    SetWord(s + 0x2E, W(at::kSub27Texture + 2 * i));
    SetWord(s + 0x30, 0);
    ++s[2];
    if (Near(s, s[8] != 0)) {
        SetWord(s + 0x30, 0xFF00);
        s[2] = 3;
    }
    SH_CALL(EffectKind18Sub27_Draw)();
}

// original 0x508E20 (state 1): the leader within the gate: the sound, +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub27_WaitNear(void) {
    StepIfNear(S()[8] != 0);
    SH_CALL(EffectKind18Sub27_Draw)();
}

// original 0x508F00 (state 2): +0x30 down 0x20; at -0x100 or below +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub27_Open(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) - 0x20u);
    if (S16(Word(s + 0x30)) <= -0x100) ++s[2];
    SH_CALL(EffectKind18Sub27_Draw)();
}

// original 0x508F20 (state 3): the leader two cells or more away: +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub27_WaitFar(void) {
    unsigned char* const s = S();
    if (Far(s, s[8] != 0)) ++s[2];
    SH_CALL(EffectKind18Sub27_Draw)();
}

// original 0x508FF0 (state 4): +0x30 up 0x20; at 0 or above the sound 0x201
// and +2 = 1. The draw.
extern "C" void __cdecl EffectKind18Sub27_Close(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) + 0x20u);
    if (S16(Word(s + 0x30)) >= 0) Closed();
    SH_CALL(EffectKind18Sub27_Draw)();
}

// ===========================================================================
// Sub-kind 0x28: EffectKind18_States[0x28] (0x65410C), EffectKind18Sub28_States (six)
// ===========================================================================

// original 0x509290 (hidden in 0x509030): jmp [EffectKind18Sub28_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub28_Run(void) {
    Dispatch("EffectKind18Sub28_Run", AddressOf(EffectKind18Sub28_States), EffectKind18Sub28_States_count);
}

// original 0x5094E0 (states 0..5 call or jump to it): a draw mode committed
// (Gfx_CommitPrim(6, 0xC)), one shaded POLY_FT4 across x at the cell, along z
// from the cell plus +0x30; corners 0x40 / 0x180 above half the ground's
// height; texture 0x1780119; committed with 0x48 bytes.
extern "C" void __cdecl EffectKind18Sub28_Draw(void) {
    DrawMode();
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const p = Quad();
    unsigned char* const s = S();
    Across(0, (Word(s + 0x36) << 7) - 0x3FC0u);
    const U a = (Word(s + 0x3A) << 7) + Word(s + 0x30) - 0x3F40u;
    SetWord(Vertex(at::kV2) + 2, a);
    SetWord(Vertex(at::kV0) + 2, a);
    const U b = (Word(s + 0x3A) << 7) + Word(s + 0x30) - 0x4040u;
    SetWord(Vertex(at::kV3) + 2, b);
    SetWord(Vertex(at::kV1) + 2, b);
    GroundFixed(0x40, 0x180);
    Project(p);
    SH_CALL(Prim_SetTexture)(0x1780119, p, 1);
    SH_CALL(Gfx_CommitPrim)(6, 0x48);
}

// original 0x5092B0 (state 0): +0x3E -0x5C0, +0x30 0, +2 up; story flag 0x54
// clear: +2 = 5 (wait for it); set and the leader within the gate (along x):
// +0x30 -0x100, +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub28_Start(void) {
    SetWord(S() + 0x3E, 0xFA40);
    SetWord(S() + 0x30, 0);
    ++S()[2];
    const unsigned char set = SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x54);
    if (set == 0) {
        S()[2] = 5;
        SH_CALL(EffectKind18Sub28_Draw)();
        return;
    }
    unsigned char* const s = S();
    if (Near(s, false)) {
        SetWord(s + 0x30, 0xFF00);
        S()[2] = 3;
    }
    SH_CALL(EffectKind18Sub28_Draw)();
}

// original 0x509360 (state 1): the leader within the gate (along x): the sound, +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub28_WaitNear(void) {
    StepIfNear(false);
    SH_CALL(EffectKind18Sub28_Draw)();
}

// original 0x5093E0 (state 2): +0x30 down 0x20; at -0x100 or below +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub28_Open(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) - 0x20u);
    if (S16(Word(s + 0x30)) <= -0x100) ++s[2];
    SH_CALL(EffectKind18Sub28_Draw)();
}

// original 0x509400 (state 3): the leader two cells or more away (along x): +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub28_WaitFar(void) {
    unsigned char* const s = S();
    if (Far(s, false)) ++s[2];
    SH_CALL(EffectKind18Sub28_Draw)();
}

// original 0x509460 (state 4): +0x30 up 0x20; at 0 or above the sound 0x201 and +2 = 1. The draw.
extern "C" void __cdecl EffectKind18Sub28_Close(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) + 0x20u);
    if (S16(Word(s + 0x30)) >= 0) Closed();
    SH_CALL(EffectKind18Sub28_Draw)();
}

// original 0x5094A0 (state 5): story flag 0x54 set: the sound 0x200 unless
// Field_Request, +2 = 2. The draw.
extern "C" void __cdecl EffectKind18Sub28_WaitFlag(void) {
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x54) != 0) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub28_Draw)();
}

// ===========================================================================
// Sub-kind 0x42: EffectKind18_States[0x42] (0x654174), EffectKind18Sub42_States (six)
// ===========================================================================

// original 0x509690 (hidden in 0x5094E0): jmp [EffectKind18Sub42_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub42_Run(void) {
    Dispatch("EffectKind18Sub42_Run", AddressOf(EffectKind18Sub42_States), EffectKind18Sub42_States_count);
}

// original 0x509A70 (a catalog row of no group; the six states call it): the
// six pieces of `variant` (the record's +8, two), each a draw mode linked at
// its cell (the record's cell plus the piece's unsigned dx, dz; dy 1) and one
// shaded POLY_FT4 of the piece's shape (three: four vertices of three s8 -
// x and z in cells from the piece's cell, the height << 7 less `height` plus
// the variant's lift), texture ((the shape's page byte + 0x1200) << 16) + the
// piece's byte, linked at the cell with 0x48 bytes. Sprite_Current read again
// for each piece and each vertex.
extern "C" void __cdecl EffectKind18Sub42_Draw(unsigned variant, int height) {
    if (variant >= at::kSub42Variants)
        bof3::Fatal("EffectKind18Sub42_Draw: variant %u, past the two piece lists of 0x%X - the original reads the "
                    "tables after them (docs/effect_5f.md section 6)",
                    variant, (unsigned)at::kSub42Pieces);
    U piece = at::kSub42Pieces + 0x18 * variant;
    for (unsigned n = 0; n < at::kSub42PieceCount; ++n, piece += 4) {
        unsigned char* s = S();
        const U dx = B(piece), dz = B(piece + 1);
        const U zz = (static_cast<U>(S16(Word(s + 0x3A))) + dz) << 16;
        const U xx = (static_cast<U>(S16(Word(s + 0x36))) + dx) << 16;
        DrawMode();
        SH_CALL(MapView_LinkPrimAt)(xx, zz, 1, 0xC);
        unsigned char* const p = Quad();
        const U lift = W(at::kSub42Lift + 2 * variant);
        const U shape = B(piece + 2);
        if (shape >= at::kSub42ShapeCount)
            bof3::Fatal("EffectKind18Sub42_Draw: shape %u, past the three of 0x%X (docs/effect_5f.md section 6)",
                        (unsigned)shape, (unsigned)at::kSub42Shapes);
        U point = at::kSub42Shapes + 12 * shape;
        for (unsigned j = 0; j < 4; ++j, point += 3) {
            s = S();
            unsigned char* const v = Vertex(at::kV0 + 8 * j);
            SetWord(v, ((static_cast<U>(S8(B(point))) + dx + Word(s + 0x36)) << 7) - 0x4040u);
            SetWord(v + 2, ((static_cast<U>(S8(B(point + 1))) + Word(s + 0x3A) + dz) << 7) - 0x4040u);
            SetWord(v + 4, (static_cast<U>(S8(B(point + 2))) << 7) - static_cast<U>(height) + lift);
        }
        Project(p);
        SH_CALL(Prim_SetTexture)(((B(at::kSub42ShapeTexture + shape) + 0x1200u) << 16) + B(piece + 3), p, 1);
        SH_CALL(MapView_LinkPrimAt)(xx, zz, 1, 0x48);
    }
}

namespace {
void Draw42() {
    unsigned char* const s = S();
    SH_CALL(EffectKind18Sub42_Draw)(s[8], S16(Word(s + 0x3E)));
}
}  // namespace

// original 0x509C00: the party member within reach of the point (x, z), 16.16:
// for each of the Field_MemberCount ObjTrio records in use, its point moved by
// its velocity (+0xC, +0x10) times +9, within ((+0x70 byte) + 3) << 15 on both
// axes. Answers the member in al, 0xFF for none.
extern "C" unsigned char __cdecl EffectKind18Sub42_MemberNear(long x, long z) {
    const U count = Field_MemberCount;
    for (U i = 0; i < count; ++i) {
        if (i >= at::kObjTrioCount)
            bof3::Fatal("EffectKind18Sub42_MemberNear: Field_MemberCount is %u, past the three ObjTrio records - the "
                        "original reads what follows them (docs/effect_5f.md section 6)",
                        (unsigned)count);
        const unsigned char* const o = ObjTrio + at::kObjTrioStride * i;
        if (o[0] == 0) continue;
        const U t = o[9];
        const std::int32_t reach = static_cast<std::int32_t>((static_cast<U>(o[0x70]) + 3) << 15);
        const U px = UL(o + 0xC) * t + UL(o + 0x34);
        if (Abs(px - static_cast<U>(x)) >= reach) continue;
        const U pz = UL(o + 0x10) * t + UL(o + 0x38);
        if (Abs(pz - static_cast<U>(z)) < reach) return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x5096B0 (state 0): the placement +0x36 (three) gives the cell, +8
// (the variant) and +0xB (the story flag); +8 clear: +0x3E 0, +2 up (wait for
// the flag); set: +0x3E 0x80, +2 = 4 (raised). The draw.
extern "C" void __cdecl EffectKind18Sub42_Start(void) {
    unsigned char* const s = S();
    const U i = Place("EffectKind18Sub42_Start", at::kSub42Places, at::kSub42PlaceCount);
    const U place = at::kSub42Places + 4 * i;
    SetWord(s + 0x36, B(place));
    SetWord(s + 0x3A, B(place + 1));
    s[8] = At(place + 2)[0];
    s[0xB] = At(place + 3)[0];
    if (s[8] == 0) {
        SetWord(s + 0x3E, 0);
        ++s[2];
    } else {
        SetWord(s + 0x3E, 0x80);
        s[2] = 4;
    }
    Draw42();
}

// original 0x509740 (state 1): the story flag +0xB differing from +8 (al ^ +8)
// and no party member within reach of the cell's middle: MoveCmd_TestFB at the
// cell + 1 (answer unread), +2 up, the sound 0x205 unless Field_Request, the
// draw. Otherwise nothing, not even the draw.
extern "C" void __cdecl EffectKind18Sub42_WaitFlag(void) {
    unsigned char* s = S();
    const unsigned char set = SH_CALL(Flags_Test)(At(at::kStoryFlags), s[0xB]);
    s = S();
    if (static_cast<unsigned char>(set ^ s[8]) == 0) return;
    const unsigned char who = SH_CALL(EffectKind18Sub42_MemberNear)(static_cast<long>(UL(s + 0x34) | 0x8000u),
                                                                    static_cast<long>(UL(s + 0x38) | 0x8000u));
    if (static_cast<signed char>(who) >= 0) return;
    s = S();
    SH_CALL(MoveCmd_TestFB)(static_cast<short>(Word(s + 0x36) + 1), static_cast<short>(Word(s + 0x3A) + 1));
    ++S()[2];
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x205);
    Draw42();
}

// original 0x5097E0 (state 2): +0x3E up 0x10; at 0x80 or above
// MoveCmd_TestFB at the cell and MoveCmd_TestFC at the cell + 1 (answers
// unread), +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub42_Raise(void) {
    unsigned char* s = S();
    SetWord(s + 0x3E, Word(s + 0x3E) + 0x10u);
    if (S16(Word(s + 0x3E)) >= 0x80) {
        SH_CALL(MoveCmd_TestFB)(static_cast<short>(Word(s + 0x36)), static_cast<short>(Word(s + 0x3A)));
        s = S();
        SH_CALL(MoveCmd_TestFC)(static_cast<short>(Word(s + 0x36) + 1), static_cast<short>(Word(s + 0x3A) + 1));
        ++S()[2];
    }
    Draw42();
}

// original 0x509850 (state 3; PSX twin 0x801F35B0): for the cell and its
// neighbour along the axis +8 picks (two), the map's cell word at
// AreaMap_Header + 2 * (x + width * z + 2 * the header's cell base) indexes a
// texture dword past the cells; the cell's draw item (MapView_ItemAt) keeps
// in its +0x7E (+8 set) or +0x8E word an item of its own, allocated
// (DrawItemPool_Alloc) and textured (Prim_SetTexture(that dword, the item, 2))
// when the word is 0. Then MapView_Redraw = 2, +2 up, the draw. The item array
// is draw_pool's (DIV-0062: the original's immediates are re-aimed at inject).
extern "C" void __cdecl EffectKind18Sub42_Mark(void) {
    unsigned char* s = S();
    for (U i = 0; i < 2; ++i) {
        const U b = s[8];
        const U flat = b == 0 ? 1u : 0u;
        const U x = static_cast<U>(S16(Word(s + 0x36))) + b + flat * i;
        const U z = static_cast<U>(S16(Word(s + 0x3A))) + flat + b * i;
        const U width = B(at::kMapWidth);
        const U cell = x + width * z + W(at::kMapCellBase) * 2;
        const U word = Word(AreaMap_Header + cell * 2);
        const U half = static_cast<U>(static_cast<std::int32_t>(B(at::kMapDepth) * width + 1) / 2);
        const U texture = at::kMapTextures + 4 * (half + W(at::kMapCellBase) + word);
        const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(static_cast<long>(x), static_cast<long>(z)));
        s = S();
        if (item >= draw_pool::Count())
            bof3::Fatal("EffectKind18Sub42_Mark: MapView_ItemAt answered %u, past the %u draw items - the original "
                        "reads and writes what follows them (docs/effect_5f.md section 6)",
                        (unsigned)item, draw_pool::Count());
        unsigned char* const keep = draw_pool::Items() + item * at::kDrawItemStride + (s[8] != 0 ? at::kItemHalfB : at::kItemHalfA);
        if (Word(keep) != 0) continue;
        const U got = SH_CALL(DrawItemPool_Alloc)();
        SetWord(keep, got);
        const U mine = got & 0xFFFFu;
        if (mine >= draw_pool::Count())
            bof3::Fatal("EffectKind18Sub42_Mark: DrawItemPool_Alloc answered %u, past the %u draw items "
                        "(docs/effect_5f.md section 6)",
                        (unsigned)mine, draw_pool::Count());
        SH_CALL(Prim_SetTexture)(UL(texture), draw_pool::Items() + mine * at::kDrawItemStride, 2);
        s = S();
    }
    MapView_Redraw = 2;
    ++s[2];
    Draw42();
}

// original 0x509980 (state 4): the story flag +0xB clear-ness ((al == 0)
// against +8, whole) differing: MoveCmd_TestFB at the cell and at the cell + 1
// (answers unread), +2 up, the sound 0x205 unless Field_Request, the draw.
// Otherwise nothing.
extern "C" void __cdecl EffectKind18Sub42_WaitFlagBack(void) {
    unsigned char* s = S();
    const unsigned char set = SH_CALL(Flags_Test)(At(at::kStoryFlags), s[0xB]);
    s = S();
    const U clear = set == 0 ? 1u : 0u;
    if (clear == s[8]) return;
    SH_CALL(MoveCmd_TestFB)(static_cast<short>(Word(s + 0x36)), static_cast<short>(Word(s + 0x3A)));
    s = S();
    SH_CALL(MoveCmd_TestFB)(static_cast<short>(Word(s + 0x36) + 1), static_cast<short>(Word(s + 0x3A) + 1));
    ++S()[2];
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x205);
    Draw42();
}

// original 0x509A20 (state 5): +0x3E down 0x10; at 0 or below MoveCmd_TestFC
// at the cell + 1 (answer unread) and +2 = 1. The draw.
extern "C" void __cdecl EffectKind18Sub42_Lower(void) {
    unsigned char* const s = S();
    SetWord(s + 0x3E, Word(s + 0x3E) - 0x10u);
    if (S16(Word(s + 0x3E)) <= 0) {
        SH_CALL(MoveCmd_TestFC)(static_cast<short>(Word(s + 0x36) + 1), static_cast<short>(Word(s + 0x3A) + 1));
        S()[2] = 1;
    }
    Draw42();
}

// ===========================================================================
// Sub-kinds 0x29, 0x48, 0x49: EffectKind18_States[0x29] (0x654110), [0x48]
// (0x65418C), [0x49] (0x654190); EffectKind18Sub29_States (five),
// EffectKind18Sub48_States (four), EffectKind18Sub49_States (five)
// ===========================================================================

// original 0x509C90 (hidden in 0x509C00): jmp [EffectKind18Sub29_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub29_Run(void) {
    Dispatch("EffectKind18Sub29_Run", AddressOf(EffectKind18Sub29_States), EffectKind18Sub29_States_count);
}

// original 0x50A020 (the three sub-kinds' states call or jump to it; state 3 of
// 0x48 is it): the panel in two halves, each a draw mode linked at the
// record's point (dy -1) and one shaded POLY_FT4. Across +8's axis the cell's
// width; along it half k (0, 1) at the cell + k (z - k when +8 is clear)
// moved by +0x30 times the half's direction; every corner less half the
// ground's height and +0x3E, two 0x180 lower; texture (+0x20 + k) | +8 << 21;
// linked at the point with 0x48 bytes.
extern "C" void __cdecl EffectKind18Sub29_Draw(void) {
    unsigned char* s = S();
    if (s[8] != 0)
        Across(2, (Word(s + 0x3A) << 7) - 0x3FC0u);
    else
        Across(0, (Word(s + 0x36) << 7) - 0x3FC0u);
    for (U k = 0; k < 2; ++k) {
        DrawMode();
        LinkAtRecord(-1, 0xC);
        unsigned char* const p = Quad();
        s = S();
        const U d = static_cast<U>(S8(B(at::kSub29Slide + k)));
        if (s[8] != 0) {
            const U a = ((k + Word(s + 0x36)) << 7) - Word(s + 0x30) * d - 0x4040u;
            SetWord(Vertex(at::kV2), a);
            SetWord(Vertex(at::kV0), a);
            const U b = ((k + Word(s + 0x36)) << 7) - Word(s + 0x30) * d - 0x3FC0u;
            SetWord(Vertex(at::kV3), b);
            SetWord(Vertex(at::kV1), b);
        } else {
            const U a = ((Word(s + 0x3A) - k) << 7) + Word(s + 0x30) * d - 0x3F40u;
            SetWord(Vertex(at::kV2) + 2, a);
            SetWord(Vertex(at::kV0) + 2, a);
            const U b = ((Word(s + 0x3A) - k) << 7) + Word(s + 0x30) * d - 0x3FC0u;
            SetWord(Vertex(at::kV3) + 2, b);
            SetWord(Vertex(at::kV1) + 2, b);
        }
        GroundLifted();
        Project(p);
        s = S();
        SH_CALL(Prim_SetTexture)((UL(s + 0x20) + k) | (static_cast<U>(s[8]) << 21), p, 1);
        LinkAtRecord(-1, 0x48);
    }
}

namespace {
// 0x509CB0's and 0x50A2F0's common start: +8 = (+0x3A word is 0); the
// placement +0x36 (four) gives the cell; +0x3E = -(half the ground's height
// at the record's point) less the placement's word; +0x20 its texture; +0x30
// 0, +2 up.
void Start29(const char* who) {
    unsigned char* s = S();
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    const U i = Place(who, at::kSub29Cell, at::kSub29Places);
    SetWord(s + 0x36, B(at::kSub29Cell + 2 * i));
    SetWord(s + 0x3A, B(at::kSub29Cell + 2 * i + 1));
    s = S();
    const long e = Elevation(UL(s + 0x34), UL(s + 0x38));
    s = S();
    SetWord(s + 0x3E, 0u - Half(e) - W(at::kSub29Height + 2 * i));
    SetUL(s + 0x20, UL(at::kSub29Texture + 4 * i));
    SetWord(s + 0x30, 0);
    ++s[2];
}
}  // namespace

// original 0x509CB0 (state 0 of 0x29 and of 0x49): the start; the leader
// within the gate: +0x30 0x80, +2 = 3 (open at once). The draw.
extern "C" void __cdecl EffectKind18Sub29_Start(void) {
    Start29("EffectKind18Sub29_Start");
    unsigned char* const s = S();
    if (Near(s, s[8] != 0)) {
        SetWord(s + 0x30, 0x80);
        s[2] = 3;
    }
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x509E10 (state 1): the leader within the gate: the sound, +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub29_WaitNear(void) {
    StepIfNear(S()[8] != 0);
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x509EF0 (state 2 of 0x29, 0x48 and 0x49): +0x30 up 0x10; at 0x80
// or above +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub29_Open(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) + 0x10u);
    if (S16(Word(s + 0x30)) >= 0x80) ++s[2];
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x509F10 (state 3): the leader two cells or more away: +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub29_WaitFar(void) {
    unsigned char* const s = S();
    if (Far(s, s[8] != 0)) ++s[2];
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x509FE0 (state 4 of 0x29 and 0x49): +0x30 down 0x10; at 0 or
// below the sound 0x201 and +2 = 1. The draw.
extern "C" void __cdecl EffectKind18Sub29_Close(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) - 0x10u);
    if (S16(Word(s + 0x30)) <= 0) Closed();
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x50A2D0 (hidden in 0x50A020): jmp [EffectKind18Sub48_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub48_Run(void) {
    Dispatch("EffectKind18Sub48_Run", AddressOf(EffectKind18Sub48_States), EffectKind18Sub48_States_count);
}

// original 0x50A2F0 (state 0): 0x29's start without the leader's test. The draw.
extern "C" void __cdecl EffectKind18Sub48_Start(void) {
    Start29("EffectKind18Sub48_Start");
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x50A390 (state 1): Cond_ByteFE 0x10: the sound 0x200 unless
// Field_Request, +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub48_WaitCue(void) {
    if (Cond_ByteFE == 0x10) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        ++S()[2];
    }
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x50A3C0 (hidden in 0x50A020): jmp [EffectKind18Sub49_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub49_Run(void) {
    Dispatch("EffectKind18Sub49_Run", AddressOf(EffectKind18Sub49_States), EffectKind18Sub49_States_count);
}

// original 0x50A3E0 (state 1): Cond_ByteFE 0x20: the sound 0x200 unless
// Field_Request, +2 up; then, story flag 0x8F set and the leader within the
// gate (along x): the sound again, +2 up again. The draw.
extern "C" void __cdecl EffectKind18Sub49_WaitCue(void) {
    if (Cond_ByteFE == 0x20) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        ++S()[2];
    }
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x8F) != 0) StepIfNear(false);
    SH_CALL(EffectKind18Sub29_Draw)();
}

// original 0x50A4A0 (state 3): unless Cond_ByteFE is 0x20, the leader two
// cells or more away (along x): +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub49_WaitFar(void) {
    if (Cond_ByteFE != 0x20) {
        unsigned char* const s = S();
        if (Far(s, false)) ++s[2];
    }
    SH_CALL(EffectKind18Sub29_Draw)();
}

// ===========================================================================
// Sub-kinds 0x4B and 0x4C: EffectKind18_States[0x4B] and [0x4C] (0x654198, 0x65419C)
// ===========================================================================

// original 0x50A510 (hidden in 0x50A020; PSX twin 0x801F38F0): v = +0xB less
// 0x4B. +2 at 0 (the first frame): +0x10 = (Rand & 0xFF) + ((3 v + 6) << 9),
// +0x2E / +0x30 from the pair (+0x34's low word is 0) picks, +0x3A up by
// Rand & 7, +2 up. Then, on Draw_PassFlags bit 2 only: +0x38 += +0x10 (the
// drift); +0x3A past +0x30 (signed) back to +0x2E; a draw mode committed and
// three shaded POLY_FT4s 0xA00 apart in x and 0x180 in z from the point's
// (>> 9) corner, half-width b = |0xF - (Frame_Counter & 0x1F)| + ((2 - v) << 8),
// height 0xB00, texture v | 0xBB509100, each committed with 0x48 bytes.
//
// The original's far corners (v1, v3) take their x from a stack word it never
// writes, and so do every corner of the second and third quads (the register
// that held the near x is reloaded from it): a latent defect (docs/effect_5f.md
// section 6). Ours takes the near corners' x there, the quad's own: x0 + a + b
// for v1 and v3, x0 + a - b for v0 and v2 of every quad.
extern "C" void __cdecl EffectKind18Sub4B_Run(void) {
    unsigned char* s = S();
    const U v = static_cast<U>(s[0xB]) - 0x4Bu;
    if (s[2] == 0) {
        const U r = static_cast<U>(SH_CALL(Rand)());
        s = S();
        SetUL(s + 0x10, (r & 0xFF) + ((v * 3 + 6) << 9));
        const U pick = Word(s + 0x34) == 0 ? 1u : 0u;
        SetWord(s + 0x2E, B(at::kSub4BBounds + 2 * pick));
        SetWord(s + 0x30, B(at::kSub4BBounds + 2 * pick + 1));
        unsigned char* const z = S() + 0x3A;
        const U r2 = static_cast<U>(SH_CALL(Rand)());
        SetWord(z, Word(z) + (r2 & 7));
        s = S();
        ++s[2];
    }
    if ((Draw_PassFlags & 4) == 0) return;
    SetUL(s + 0x38, UL(s + 0x38) + UL(s + 0x10));
    if (S16(Word(s + 0x3A)) > S16(Word(s + 0x30))) SetWord(s + 0x3A, Word(s + 0x2E));
    const U pulse = Frame_Counter & 0x1F;
    const U z0 = static_cast<U>(static_cast<std::int32_t>(UL(s + 0x38)) >> 9) - 0x3FC0u;
    const U b = static_cast<U>(Abs(0xFu - pulse)) + ((2u - v) << 8);
    const U x0 = static_cast<U>(static_cast<std::int32_t>(UL(s + 0x34)) >> 9) - 0x4EC0u;
    DrawMode();
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    const U texture = v | 0xBB509100u;
    unsigned char* const v0 = Vertex(at::kV0);
    unsigned char* const v1 = Vertex(at::kV1);
    unsigned char* const v2 = Vertex(at::kV2);
    unsigned char* const v3 = Vertex(at::kV3);
    for (U k = 0; k < 3; ++k) {
        unsigned char* const p = Quad();
        const U a = k * 0xA00, c = k * 0x180;
        SetWord(v0 + 4, 0xB00);
        SetWord(v0, x0 + a - b);
        SetWord(v0 + 2, c - b + z0);
        SetWord(v1 + 2, c - b + z0);
        SetWord(v2, x0 + a - b);
        SetWord(v1, x0 + a + b);
        SetWord(v3, x0 + a + b);
        SetWord(v2 + 2, c + z0 + b);
        SetWord(v3 + 2, c + z0 + b);
        SetWord(v2 + 4, 0xB00);
        SetWord(v3 + 4, 0xB00);
        SetWord(v1 + 4, 0xB00);
        Project(p);
        SH_CALL(Prim_SetTexture)(texture, p, 1);
        SH_CALL(Gfx_CommitPrim)(6, 0x48);
    }
}

// ===========================================================================
// Sub-kind 0x2A: EffectKind18_States[0x2A] (0x654114), EffectKind18Sub2A_States (five)
// ===========================================================================

// original 0x50A760 (hidden in 0x50A020): jmp [EffectKind18Sub2A_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub2A_Run(void) {
    Dispatch("EffectKind18Sub2A_Run", AddressOf(EffectKind18Sub2A_States), EffectKind18Sub2A_States_count);
}

// original 0x50AA10 (states 0, 1, 2 and 4 call or jump to it): a draw mode
// committed, then the panel in two halves of one shaded POLY_FT4 each: across
// +8's axis the cell's width, along it the cell moved by +0x30 times the
// half's direction; heights as 0x29's; texture (+8 << 21) | (0x127 - k) |
// 0x25510000; each committed with 0x48 bytes.
extern "C" void __cdecl EffectKind18Sub2A_Draw(void) {
    unsigned char* s = S();
    if (s[8] != 0)
        Across(2, (Word(s + 0x3A) << 7) - 0x3FC0u);
    else
        Across(0, (Word(s + 0x36) << 7) - 0x3FC0u);
    DrawMode();
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    U page = 0x127;
    for (U k = 0; k < 2; ++k, --page) {
        unsigned char* const p = Quad();
        s = S();
        const U d = static_cast<U>(S8(B(at::kSub2ASlide + k)));
        if (s[8] != 0) {
            const U a = Word(s + 0x30) * d + (Word(s + 0x36) << 7) - 0x3F40u;
            SetWord(Vertex(at::kV2), a);
            SetWord(Vertex(at::kV0), a);
            const U b = Word(s + 0x30) * d + (Word(s + 0x36) << 7) - 0x4040u;
            SetWord(Vertex(at::kV3), b);
            SetWord(Vertex(at::kV1), b);
        } else {
            const U a = Word(s + 0x30) * d + (Word(s + 0x3A) << 7) - 0x3F40u;
            SetWord(Vertex(at::kV2) + 2, a);
            SetWord(Vertex(at::kV0) + 2, a);
            const U b = Word(s + 0x30) * d + (Word(s + 0x3A) << 7) - 0x4040u;
            SetWord(Vertex(at::kV3) + 2, b);
            SetWord(Vertex(at::kV1) + 2, b);
        }
        GroundLifted();
        Project(p);
        s = S();
        SH_CALL(Prim_SetTexture)((static_cast<U>(s[8]) << 21) | page | 0x25510000u, p, 1);
        SH_CALL(Gfx_CommitPrim)(6, 0x48);
    }
}

// original 0x50A780 (state 0): +8 = (+0x3A word is 0); the placement +0x36
// (six) gives the cell; +0x3E = -(half the ground's height) less the
// placement's word; +0x30 0, +2 up; the leader within the gate: +0x30 0xC0,
// +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub2A_Start(void) {
    unsigned char* s = S();
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    const U i = Place("EffectKind18Sub2A_Start", at::kSub2ACell, at::kSub2APlaces);
    SetWord(s + 0x36, B(at::kSub2ACell + 2 * i));
    SetWord(s + 0x3A, B(at::kSub2ACell + 2 * i + 1));
    s = S();
    const long e = Elevation(UL(s + 0x34), UL(s + 0x38));
    s = S();
    SetWord(s + 0x3E, 0u - Half(e) - W(at::kSub2AHeight + 2 * i));
    SetWord(s + 0x30, 0);
    ++s[2];
    if (Near(s, s[8] != 0)) {
        SetWord(s + 0x30, 0xC0);
        s[2] = 3;
    }
    SH_CALL(EffectKind18Sub2A_Draw)();
}

// original 0x50A8D0 (state 1): the leader within the gate: the sound, +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub2A_WaitNear(void) {
    StepIfNear(S()[8] != 0);
    SH_CALL(EffectKind18Sub2A_Draw)();
}

// original 0x50A9B0 (state 2): +0x30 up 0x10; at 0xC0 or above +2 up. The
// draw. (State 3 is E5E's 0x508670: the same wait for the leader to leave,
// without a draw.)
extern "C" void __cdecl EffectKind18Sub2A_Open(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) + 0x10u);
    if (S16(Word(s + 0x30)) >= 0xC0) ++s[2];
    SH_CALL(EffectKind18Sub2A_Draw)();
}

// original 0x50A9D0 (state 4): +0x30 down 0x10; at 0 or below the sound 0x201
// and +2 = 1. The draw.
extern "C" void __cdecl EffectKind18Sub2A_Close(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) - 0x10u);
    if (S16(Word(s + 0x30)) <= 0) Closed();
    SH_CALL(EffectKind18Sub2A_Draw)();
}

// ===========================================================================
// Sub-kind 0x3C: EffectKind18_States[0x3C] (0x65415C), EffectKind18Sub3C_States (three)
// ===========================================================================

// original 0x50ACB0 (hidden in 0x50AA10): jmp [EffectKind18Sub3C_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub3C_Run(void) {
    Dispatch("EffectKind18Sub3C_Run", AddressOf(EffectKind18Sub3C_States), EffectKind18Sub3C_States_count);
}

// original 0x50AD70 (the three states call it): two quads, each a draw mode
// linked at the record's point (dy 2) and one shaded POLY_FT4 - x from +0x30
// (-0x3BC0 and -0x3AC0), z fixed (the near pair (-0x388 - k) << 4, the far
// 0xC780); `edge` set: the heights from the ground (less half its height, the
// far pair of the first quad 0x180 above); clear: fixed (-0x480, the far pair
// -0x300 for the first quad, -0x480 for the second). Textures the two dwords
// of 0x65E9E4; each linked at the point with 0x48 bytes.
extern "C" void __cdecl EffectKind18Sub3C_Draw(int edge) {
    unsigned char* s = S();
    unsigned char* const v0 = Vertex(at::kV0);
    unsigned char* const v1 = Vertex(at::kV1);
    unsigned char* const v2 = Vertex(at::kV2);
    unsigned char* const v3 = Vertex(at::kV3);
    SetWord(v2, Word(s + 0x30) - 0x3BC0u);
    SetWord(v0, Word(s + 0x30) - 0x3BC0u);
    SetWord(v3, Word(s + 0x30) - 0x3AC0u);
    SetWord(v1, Word(s + 0x30) - 0x3AC0u);
    for (U k = 0; k < 2; ++k) {
        DrawMode();
        LinkAtRecord(2, 0xC);
        unsigned char* const p = Quad();
        SetWord(v3 + 2, 0xC780);
        SetWord(v2 + 2, 0xC780);
        const U near = (0xFFFFFC78u - k) << 4;
        SetWord(v1 + 2, near);
        SetWord(v0 + 2, near);
        if (edge != 0) {
            U gx = GroundOf(Word(v0)), gz = GroundOf(near);
            long e = Elevation(gx, gz);
            SetWord(v0 + 4, 0u - Half(e));
            const U t = k == 0 ? 0x180u : 0u;
            e = Elevation(gx, gz);
            gz = GroundOf(Word(v1 + 2));
            gx = GroundOf(Word(v1));
            SetWord(v2 + 4, t - Half(e));
            e = Elevation(gx, gz);
            SetWord(v1 + 4, 0u - Half(e));
            e = Elevation(gx, gz);
            SetWord(v3 + 4, t - Half(e));
        } else {
            SetWord(v1 + 4, 0xFB80);
            SetWord(v0 + 4, 0xFB80);
            const U t = (((k == 0 ? 1u : 0u) - 3u) * 3u) << 7;
            SetWord(v3 + 4, t);
            SetWord(v2 + 4, t);
        }
        Project(p);
        SH_CALL(Prim_SetTexture)(UL(at::kSub3CTextures + 4 * k), p, 1);
        LinkAtRecord(2, 0x48);
    }
}

// original 0x50ACD0 (state 0): +0x30 0, +2 up; the draw, flat.
extern "C" void __cdecl EffectKind18Sub3C_Start(void) {
    SetWord(S() + 0x30, 0);
    ++S()[2];
    SH_CALL(EffectKind18Sub3C_Draw)(0);
}

// original 0x50ACF0 (state 1): story flag 0x67 set: the sound 0x200 unless
// Field_Request, +2 up; the draw on the ground.
extern "C" void __cdecl EffectKind18Sub3C_WaitFlag(void) {
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x67) != 0) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        ++S()[2];
    }
    SH_CALL(EffectKind18Sub3C_Draw)(1);
}

// original 0x50AD30 (state 2): +0x30 up 0x20; at 0x100 or above
// MoveCmd_TestFB(9, 0xF) (answer unread) and Effect_Release. The draw, flat
// (after the release too).
extern "C" void __cdecl EffectKind18Sub3C_Rise(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) + 0x20u);
    if (S16(Word(s + 0x30)) >= 0x100) {
        SH_CALL(MoveCmd_TestFB)(9, 0xF);
        SH_CALL(Effect_Release)();
    }
    SH_CALL(EffectKind18Sub3C_Draw)(0);
}

void Effect5F_Inject() {
    if (bof3::WantsShadow("effect_5f")) effect_5f::SelfTest();
    BOF3_INJECT(EffectKind18Sub27_Run);
    BOF3_INJECT(EffectKind18Sub27_Start);
    BOF3_INJECT(EffectKind18Sub27_WaitNear);
    BOF3_INJECT(EffectKind18Sub27_Open);
    BOF3_INJECT(EffectKind18Sub27_WaitFar);
    BOF3_INJECT(EffectKind18Sub27_Close);
    BOF3_INJECT(EffectKind18Sub27_Draw);
    BOF3_INJECT(EffectKind18Sub28_Run);
    BOF3_INJECT(EffectKind18Sub28_Start);
    BOF3_INJECT(EffectKind18Sub28_WaitNear);
    BOF3_INJECT(EffectKind18Sub28_Open);
    BOF3_INJECT(EffectKind18Sub28_WaitFar);
    BOF3_INJECT(EffectKind18Sub28_Close);
    BOF3_INJECT(EffectKind18Sub28_WaitFlag);
    BOF3_INJECT(EffectKind18Sub28_Draw);
    BOF3_INJECT(EffectKind18Sub42_Run);
    BOF3_INJECT(EffectKind18Sub42_Start);
    BOF3_INJECT(EffectKind18Sub42_WaitFlag);
    BOF3_INJECT(EffectKind18Sub42_Raise);
    BOF3_INJECT(EffectKind18Sub42_Mark);
    BOF3_INJECT(EffectKind18Sub42_WaitFlagBack);
    BOF3_INJECT(EffectKind18Sub42_Lower);
    BOF3_INJECT(EffectKind18Sub42_Draw);
    BOF3_INJECT(EffectKind18Sub42_MemberNear);
    BOF3_INJECT(EffectKind18Sub29_Run);
    BOF3_INJECT(EffectKind18Sub29_Start);
    BOF3_INJECT(EffectKind18Sub29_WaitNear);
    BOF3_INJECT(EffectKind18Sub29_Open);
    BOF3_INJECT(EffectKind18Sub29_WaitFar);
    BOF3_INJECT(EffectKind18Sub29_Close);
    BOF3_INJECT(EffectKind18Sub29_Draw);
    BOF3_INJECT(EffectKind18Sub48_Run);
    BOF3_INJECT(EffectKind18Sub48_Start);
    BOF3_INJECT(EffectKind18Sub48_WaitCue);
    BOF3_INJECT(EffectKind18Sub49_Run);
    BOF3_INJECT(EffectKind18Sub49_WaitCue);
    BOF3_INJECT(EffectKind18Sub49_WaitFar);
    BOF3_INJECT(EffectKind18Sub4B_Run);
    BOF3_INJECT(EffectKind18Sub2A_Run);
    BOF3_INJECT(EffectKind18Sub2A_Start);
    BOF3_INJECT(EffectKind18Sub2A_WaitNear);
    BOF3_INJECT(EffectKind18Sub2A_Open);
    BOF3_INJECT(EffectKind18Sub2A_Close);
    BOF3_INJECT(EffectKind18Sub2A_Draw);
    BOF3_INJECT(EffectKind18Sub3C_Run);
    BOF3_INJECT(EffectKind18Sub3C_Start);
    BOF3_INJECT(EffectKind18Sub3C_WaitFlag);
    BOF3_INJECT(EffectKind18Sub3C_Rise);
    BOF3_INJECT(EffectKind18Sub3C_Draw);
}
