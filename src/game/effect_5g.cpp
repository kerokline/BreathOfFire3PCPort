// Round thirteen group E5G (docs/effect_5g.md): the 24 functions of
// analysis/round13_cut.tsv's group E5G, 0x50AF90..0x50C0CA, each read with
// capstone to its last instruction (2026-10-03). Effect_RunObjects (ours)
// makes each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current
// and calls Effect_KindHandlers[+5]; kind 0x18's EffectKind18_Run jumps through
// EffectKind18_States by +1 (the sub-kind, EffectKind18_Start copies it from
// +0xB). Four of the band's sub-kinds dispatch again by +2 through a table of
// their own (none bounded by a compare); the fifth is one state. What each is,
// as far as the code says:
//
//   sub-kind 0x2B   two textured panels (one quad each, mirrored) at a cell a
//                   variant table picks by the spawn's x cell: when the leader
//                   stands at the cell's front they slide apart over twelve
//                   frames (sound 0x200), hide while the leader stays, slide
//                   back once the leader is two cells away (sound 0x201)
//   sub-kind 0x2C   the same with panels sliding along x or z (the spawn's z
//                   cell 0 chooses), drawn always, linked into the map's depth
//                   order at the record; eight frames each way
//   sub-kind 0x36   a semi-transparent blue fill over the frame (additive), its
//                   blue stepped through four levels every eight frames - one of
//                   DIV-0041's full-frame fills (below)
//   sub-kind 0x3A   two panels at the record's cell that slide apart while any
//                   party member stands at them (sound 0x202) and back once none
//                   does (sound 0x203)
//   sub-kind 0x4A   sub-kind 0x2C's panels placed with two map bytes written
//                   under them (AreaMap_SetByte); its state 1 is E6A's
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. Sprite_Current is read
// again wherever the original reads [0x937F88] again after a call. Sub-kind
// 0x36's fill is DIV-0041's (section 3c): its corners are (Widescreen_FillX(),
// 0) .. (320 + Widescreen_Fill(), 240), which is the original's (0, 0) ..
// (320, 240) until Widescreen_ArmFills has run and whenever the picture is
// narrow. Otherwise no divergence: each is a faithful replacement. Where the
// original jumps through a sub-state table past its end or indexes a variant
// or step table past its room, ours aborts with a message (docs/effect_5g.md
// section 6).
#include "game/effect_5g.h"

#include <bit>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_5g_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5g::at;
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

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5g.md section 6)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// The variant the spawn's x cell (+0x36, movsx) names, checked against the
// room its tables have.
U Variant(const char* who, const unsigned char* s, unsigned room) {
    const std::int32_t v = S16(s + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(room))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u its tables have room for - the original reads on into "
                    "the next table (docs/effect_5g.md section 6)",
                    who, (int)v, room);
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
// z - or, for +8 set (sub-kinds 0x2C / 0x4A), z first, then x.
bool LeaderAt(const unsigned char* s, bool z_first, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (z_first) return Within(FromCentre(LeaderZ(), cz), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx), first, FromEdge(LeaderZ(), cz), second);
}

// A vertex's ground point: (s16 + 0x4000) << 9, the 16.16 the map reads.
long Ground(const unsigned char* word) { return static_cast<long>(static_cast<U>(S16(word) + 0x4000) << 9); }
// -(height / 2) - +0x3E, the low word (movsx; cdq; sub; sar 1; neg; sub ax).
U Lift(long height, const unsigned char* s) {
    const std::int32_t half = static_cast<std::int16_t>(static_cast<U>(height) & 0xFFFFu) / 2;
    return 0u - static_cast<U>(half) - Word(s + 0x3E);
}
// The four corners' heights from the ground under them, as every panel draw
// lifts them: corners 0 and 2 at vertex 0's point (two reads, the top 0x180
// above), corners 1 and 3 at vertex 1's, Sprite_Current read after each call.
void Grounded() {
    unsigned char* const v = Vertices();
    const long x0 = Ground(v), y0 = Ground(v + 2);
    long h = SH_CALL(AreaMap_Elevation)(x0, y0);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->z, Lift(h, S()) - 0x180u);
    h = SH_CALL(AreaMap_Elevation)(x0, y0);
    const long x1 = Ground(v + 8), y1 = Ground(v + 0xA);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].z, Lift(h, S()));
    h = SH_CALL(AreaMap_Elevation)(x1, y1);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].z, Lift(h, S()) - 0x180u);
    h = SH_CALL(AreaMap_Elevation)(x1, y1);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].z, Lift(h, S()));
}
// The quad's four corners projected into the POLY_FT4 at p, its depths set.
// The original also pushes a tenth pointer (a flag local) the projection
// does not read.
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                               reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                               reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}
// The draw mode the panels put first: Gpu_SetDrawMode(prim, 0, 0, 0x95, 0).
void PanelMode(unsigned char* prim) { SH_CALL(Gpu_SetDrawMode)(prim, 0, 0, 0x95, 0); }

// A slide of +0x30 by `by`; answers whether it has reached `end` (>= when
// opening, <= 0 when closing; signed words).
bool Opened(std::int32_t end) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 + 0x10u);
    return S16(S() + 0x30) >= end;
}
bool Closed() {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 - 0x10u);
    return S16(S() + 0x30) <= 0;
}
// Closed: the sound unless a message is up (Field_Request), +2 = 1 (read after
// the sound).
void Shut(unsigned sound) {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(sound));
    S()[2] = 1;
}

}  // namespace

// ===========================================================================
// The panel draws
// ===========================================================================

// ===========================================================================
// Sub-kind 0x2B: EffectKind18_States[0x2B] (0x654118), EffectKind18Sub2B_States
// (five) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x36: EffectKind18_States[0x36] (0x654144) - one state
// ===========================================================================

// ===========================================================================
// Sub-kind 0x2C: EffectKind18_States[0x2C] (0x65411C), EffectKind18Sub2C_States
// (five) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x3A: EffectKind18_States[0x3A] (0x654154), EffectKind18Sub3A_States
// (five) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x4A: EffectKind18_States[0x4A] (0x654194), EffectKind18Sub4A_States
// (four) by +2: this state, E6A's EffectKind18Sub4A_WaitCond, EffectKind18Sub2C_Open, the draw
// ===========================================================================

void Effect5G_Inject() {
    if (bof3::WantsShadow("effect_5g")) effect_5g::SelfTest();
}
