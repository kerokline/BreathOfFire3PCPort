// Round thirteen group E6D (docs/effect_6d.md): the 51 functions of
// analysis/round13_cut.tsv's group E6D (50 rows) and the one start its band
// holds that no list has (0x5166C0), 0x514270..0x516B2E, each read with
// capstone to its last instruction (2026-10-03). Effect_RunObjects (ours)
// makes each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current
// and calls Effect_KindHandlers[+5]; kind 0x18's EffectKind18_Run jumps through
// EffectKind18_States by +1 (the sub-kind, EffectKind18_Start copies it from
// +0xB). Five of the band's sub-kinds dispatch again through a table of their
// own (by +2, or by Cond_ByteFE for 0x61; none bounded by a compare); two are
// one state. What each is, as far as the code says:
//
//   sub-kind 0x5C   two panels of three textured quads each (a vertex table),
//                   placed at a cell a variant names; a story flag set while
//                   they are shut and cleared as they open: they slide apart
//                   when the leader stands at the cell (or Cond_ByteFE is 1),
//                   wait, and slide back once the leader is away (or
//                   Cond_ByteFE is 1); drawn only while not waiting shut
//   sub-kind 0x5E   two panels of one quad, the ground under each corner,
//                   sliding along x when the leader stands at the cell
//   sub-kind 0x5D   two panels at a variant's cell lying across x or z (the
//                   spawn's z cell 0 chooses), a height of their own, flat
//   sub-kind 0x61   by Cond_ByteFE: a screen overlay of textured pieces raised
//                   or slid, a glow of a disc and four rings, the followers'
//                   tracks marked on the map behind them, a scrolling mist
//   sub-kind 0x62   the map's corner heights rippled around Field_Kind2's cell
//   sub-kind 0x63   six projected Gouraud quads turning with Cond_AngleFB
//   sub-kind 0x64   a crossed pair of textured quads at a variant's cell that
//                   follows the state of an enemy record (+0xC) in battle
//   sub-kind 0x65   CLUT rows 4 and 5 faded in from black
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. Sprite_Current is read
// again wherever the original reads [0x937F88] again after a call. No
// divergence: each is a faithful replacement. Where the original jumps through
// a table past its end or indexes a variant table past its room, ours aborts
// with a message (docs/effect_6d.md section 7).
#include "game/effect_6d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_6d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6d::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S16(U a) { return S16(At(a)); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
U S8U(unsigned char b) { return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(b))); }
U AddressOf(const void* p) { return Low32(p); }
unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
const short* Vertex(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }
// The float bits of an int (fild; fstp dword).
U FloatBits(std::int32_t v) {
    const float f = static_cast<float>(v);
    U b;
    std::memcpy(&b, &f, sizeof b);
    return b;
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void DispatchEffect6d(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6d.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * sub)))();
}

// The variant the spawn's x cell (+0x36, movsx) names, checked against the
// room its tables have.
U Variant(const char* who, const unsigned char* s, unsigned room) {
    const std::int32_t v = S16(s + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(room))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u its tables have room for - the original reads on into "
                    "the next table (docs/effect_6d.md section 7)",
                    who, (int)v, room);
    return static_cast<U>(v);
}

// cdq; xor eax, edx; sub eax, edx: |d| as the original computes it -
// 0x80000000 stays itself, negative.
std::int32_t Abs(U d) {
    const U m = I(d) < 0 ? 0xFFFFFFFFu : 0u;
    return I((d ^ m) - m);
}
// A point against a cell's centre on one axis: point - ((cell + 1) << 16 |
// 0x8000) (inc; shl 16; or dh, 0x80).
U FromCentre(U point, std::int32_t cell) { return point - ((static_cast<U>(cell + 1) << 16) | 0x8000u); }
// ... and against its edge: point - (cell << 16).
U FromEdge(U point, std::int32_t cell) { return point - (static_cast<U>(cell) << 16); }
// |a| within `first`, then |b| or |b - 0x10000| within `second` (the cell or
// the one past it).
bool Within(U a, std::int32_t first, U b, std::int32_t second) {
    if (Abs(a) > first) return false;
    if (Abs(b) <= second) return true;
    return Abs(b - 0x10000u) <= second;
}
U LeaderX() { return static_cast<std::uint32_t>(ObjTrio[0].x); }
U LeaderZ() { return static_cast<std::uint32_t>(ObjTrio[0].z); }
// The leader at the record's cell, z across the centre first (sub-kind 0x5E,
// and 0x5D with +8 set), or x first (0x5D without).
bool LeaderAt(const unsigned char* s, bool z_first, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (z_first) return Within(FromCentre(LeaderZ(), cz), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx), first, FromEdge(LeaderZ(), cz), second);
}

// movsx eax, ax; cdq; sub eax, edx; sar 1: half a height, toward zero.
U Half(long height) { return static_cast<U>(static_cast<std::int16_t>(static_cast<U>(height) & 0xFFFFu) / 2); }

// The quad's four corners projected into the POLY_FT4 / G4 at p, its depths
// set. The original also pushes a tenth pointer (a flag local) the projection
// does not read.
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                               reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                               reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}
// The draw mode the panels put first: Gpu_SetDrawMode(prim, 0, 0, 0x95, 0).
void PanelMode() { SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0); }
void Link(int dy, unsigned size) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(so->x), static_cast<std::uint32_t>(so->z), dy, size);
}
// A slide of +0x30 by `by`; answers +0x30 (signed, read again).
std::int32_t Slide(U by) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 + by);
    return S16(S() + 0x30);
}
// Closed: the sound unless a message is up (Field_Request), +2 = 1 (read after
// the sound).
void Shut() {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundShut));
    S()[2] = 1;
}
void StepUp() { S()[2] = static_cast<unsigned char>(S()[2] + 1); }

}  // namespace

// ===========================================================================
// Sub-kind 0x5C: EffectKind18_States[0x5C] (0x6541DC), EffectKind18Sub5C_States
// (five) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x5E: EffectKind18_States[0x5E] (0x6541E4), EffectKind18Sub5E_States
// (five) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x5D: EffectKind18_States[0x5D] (0x6541E0), EffectKind18Sub5D_States
// (five) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x61: EffectKind18_States[0x61] (0x6541F0), its steps
// EffectKind18Sub61_Steps (seven) by Cond_ByteFE
// ===========================================================================

// ===========================================================================
// Sub-kinds 0x62 and 0x63: one state each
// ===========================================================================

// ===========================================================================
// Sub-kind 0x64: EffectKind18_States[0x64] (0x6541FC), EffectKind18Sub64_States
// (seven) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x65: EffectKind18_States[0x65] (0x654200), EffectKind18Sub65_States
// (three) by +2
// ===========================================================================

void Effect6D_Inject() {
    if (bof3::WantsShadow("effect_6d")) effect_6d::SelfTest();
}
