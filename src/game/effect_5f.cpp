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
#include "game/record_views.h"
#include "game/records.h"
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
U W(U a) { return Word(At(a)); }
U B(U a) { return At(a)[0]; }
std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }
std::int32_t S8(unsigned char v) { return static_cast<signed char>(v); }
U AddressOf(const void* p) { return Low32(p); }
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
    const std::int32_t i = S16(rv::As<records::EffectObject>(S())->x_whole);
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
    const U lx = static_cast<std::uint32_t>(ObjTrio[0].x), lz = static_cast<std::uint32_t>(ObjTrio[0].z);
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
    SH_CALL(Gte_RotTransPers4)(reinterpret_cast<const short*>(Vertex(::bof3::Img(Prim_VertexScratch_addr))), reinterpret_cast<const short*>(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8)),
                               reinterpret_cast<const short*>(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10)), reinterpret_cast<const short*>(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18)),
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
    unsigned char* const v0 = Vertex(::bof3::Img(Prim_VertexScratch_addr));
    unsigned char* const v1 = Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8);
    U gx = GroundOf(Word(v0)), gz = GroundOf(static_cast<std::uint16_t>(rv::As<records::SVector>(v0)->y));
    long e = Elevation(gx, gz);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v0)->z, low - Half(e));
    e = Elevation(gx, gz);
    gz = GroundOf(static_cast<std::uint16_t>(rv::As<records::SVector>(v1)->y));
    gx = GroundOf(Word(v1));
    Prim_VertexScratch[2].z = high - Half(e);
    e = Elevation(gx, gz);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v1)->z, low - Half(e));
    e = Elevation(gx, gz);
    Prim_VertexScratch[3].z = high - Half(e);
}

// The same for the lifted panels (sub-kinds 0x29 and 0x2A): every height less
// half the ground's and less +0x3E (Sprite_Current read after each call), v0
// and v1 0x180 lower.
void GroundLifted() {
    unsigned char* const v0 = Vertex(::bof3::Img(Prim_VertexScratch_addr));
    unsigned char* const v1 = Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8);
    U gz = GroundOf(static_cast<std::uint16_t>(rv::As<records::SVector>(v0)->y)), gx = GroundOf(Word(v0));
    long e = Elevation(gx, gz);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v0)->z, 0u - Half(e) - rv::As<records::EffectObject>(S())->height_whole - 0x180u);
    e = Elevation(gx, gz);
    gz = GroundOf(static_cast<std::uint16_t>(rv::As<records::SVector>(v1)->y));
    gx = GroundOf(Word(v1));
    Prim_VertexScratch[2].z = 0u - Half(e) - rv::As<records::EffectObject>(S())->height_whole;
    e = Elevation(gx, gz);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v1)->z, 0u - Half(e) - rv::As<records::EffectObject>(S())->height_whole - 0x180u);
    e = Elevation(gx, gz);
    Prim_VertexScratch[3].z = 0u - Half(e) - rv::As<records::EffectObject>(S())->height_whole;
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
    auto* const so = rv::As<records::EffectObject>(s);
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(so->x), static_cast<std::uint32_t>(so->z), dy, size);
}

// The panel's four corners on the axis +8 does not pick: c = (w << 7) - 0x3FC0
// of the record's other cell word, into the four vertices' word `off`.
void Across(unsigned off, U c) {
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18) + off, c);
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10) + off, c);
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8) + off, c);
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr)) + off, c);
}

}  // namespace

// ===========================================================================
// Sub-kind 0x27: EffectKind18_States[0x27] (0x654108), EffectKind18Sub27_States (five)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x28: EffectKind18_States[0x28] (0x65410C), EffectKind18Sub28_States (six)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x42: EffectKind18_States[0x42] (0x654174), EffectKind18Sub42_States (six)
// ===========================================================================

// ===========================================================================
// Sub-kinds 0x29, 0x48, 0x49: EffectKind18_States[0x29] (0x654110), [0x48]
// (0x65418C), [0x49] (0x654190); EffectKind18Sub29_States (five),
// EffectKind18Sub48_States (four), EffectKind18Sub49_States (five)
// ===========================================================================

// ===========================================================================
// Sub-kinds 0x4B and 0x4C: EffectKind18_States[0x4B] and [0x4C] (0x654198, 0x65419C)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x2A: EffectKind18_States[0x2A] (0x654114), EffectKind18Sub2A_States (five)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x3C: EffectKind18_States[0x3C] (0x65415C), EffectKind18Sub3C_States (three)
// ===========================================================================

void Effect5F_Inject() {
    if (bof3::WantsShadow("effect_5f")) effect_5f::SelfTest();
}
