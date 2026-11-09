// Round thirteen group E6A (docs/effect_6a.md): the 48 functions of
// analysis/round13_cut.tsv's group E6A, 0x50C0D0..0x50E3FA, each read with
// capstone to its last instruction (2026-10-03). Effect_RunObjects (ours)
// makes each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current
// and calls Effect_KindHandlers[+5]; kind 0x18's EffectKind18_Run jumps through
// EffectKind18_States by +1 (the sub-kind, EffectKind18_Start copies it from
// +0xB). Seven sub-kinds of the band dispatch again by +2 through a table of
// their own (none bounded by a compare), five states each, the same shape as
// E5G's sub-kind 0x2C: place (0), wait for the leader at the cell's front (1),
// slide open (2), wait for the leader two cells off (3), slide shut (4).
//
//   sub-kind 0x2D   one textured quad at a variant's cell and height (+0x3E),
//                   lying across x or z (bit 0 of the spawn's z cell chooses),
//                   slid by a sign bit 1 of the z cell chooses; flat, linked
//                   into the map's depth order at the record (dy -2)
//   sub-kind 0x3E   the same states with the front one cell further back
//                   (the centre at cell - 2) and nothing drawn
//   sub-kinds 0x2E, 0x30, 0x31   one quad on the ground (four AreaMap_Elevation
//                   reads), slid toward -0x100; three draws that differ only
//                   in the texture word
//   sub-kinds 0x2F, 0x32   the same slid toward +0x100, sharing a draw
//                   (EffectKind18Sub2F_Draw, which E6B's sub-kinds call too)
//   sub-kind 0x4A   its state 1 (E5G's table): once a bit of +0xB is set in
//                   Cond_ByteFE (and Field_Request is not 2) the map byte 0xA1
//                   at the cell and the next along z, sound 0x200, +2 up; the
//                   draw is E5G's EffectKind18Sub2C_Draw
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. Sprite_Current is read
// again wherever the original reads [0x937F88] again after a call. No
// divergence: each is a faithful replacement. Where the original jumps
// through a sub-state table past its end or indexes a variant or sign table
// past its room, ours aborts with a message (docs/effect_6a.md section 7).
#include "game/effect_6a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_6a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6a::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
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
// after - data or the next table.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6a.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// The variant the spawn's x cell (+0x36, movsx) names, checked against the
// room its tables have.
U Variant(const char* who, const unsigned char* s, unsigned room) {
    const std::int32_t v = S16(s + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(room))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u its tables have room for - the original reads on into "
                    "the next table (docs/effect_6a.md section 7)",
                    who, (int)v, room);
    return static_cast<U>(v);
}

// cdq; xor eax, edx; sub eax, edx; cmp eax, n; jg: |d| as the original
// computes it - 0x80000000 stays itself, negative, and so counts as near.
std::int32_t Abs(U d) {
    const U m = static_cast<std::int32_t>(d) < 0 ? 0xFFFFFFFFu : 0u;
    return static_cast<std::int32_t>((d ^ m) - m);
}
// A point against a cell's centre on one axis: point - (cell << 16 | 0x8000)
// (the caller's cell already moved by the sub-kind's delta: inc, or sub 2).
U FromCentre(U point, std::int32_t cell) { return point - ((static_cast<U>(cell) << 16) | 0x8000u); }
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
// The leader at the record's cell: with +8 set across z first (the centre of
// cell z + delta), then x against the edge; without, x first, then z.
bool LeaderAt(const unsigned char* s, std::int32_t delta, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (s[8] != 0) return Within(FromCentre(LeaderZ(), cz + delta), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx + delta), first, FromEdge(LeaderZ(), cz), second);
}

// The front's delta: the centre at cell + 1 (every sub-kind but 0x3E), cell -
// 2 (0x3E).
constexpr std::int32_t kFront = 1, kFront3E = -2;

// --- the states' bodies --------------------------------------------------------------

// Sub-state 0 of 0x3E and 0x2E..0x32: +8 = whether the spawn's z cell is 0,
// the variant's cell into +0x36 / +0x3A, the slide 0, +2 up; the leader
// already at the front: the slide `open` and +2 = 3. Answers whether it was.
bool PlaceCells(const char* who, U cells, unsigned room, std::int32_t delta, U open) {
    unsigned char* const s = S();
    s[8] = rv::As<records::EffectObject>(s)->z_whole == 0 ? 1 : 0;
    const U v = Variant(who, s, room);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole, At(cells + 2 * v)[0]);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole, At(cells + 2 * v + 1)[0]);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0);
    s[2] = static_cast<unsigned char>(s[2] + 1);
    if (!LeaderAt(s, delta, 0x8000, 0x10000)) return false;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, open);
    s[2] = 3;
    return true;
}
// Sub-state 1: the leader at the front: sound 0x200 unless a message is up,
// +2 up (Sprite_Current read after the sound).
void WaitNear(std::int32_t delta) {
    unsigned char* s = S();
    if (!LeaderAt(s, delta, 0x8000, 0x10000)) return;
    if (Field_Request == 0) {
        SH_CALL(Sound_PlayEffect)(at::kSoundOpen);
        s = S();
    }
    s[2] = static_cast<unsigned char>(s[2] + 1);
}
// Sub-state 2: the slide 0x20 toward +0x100 (`up`) or -0x100; there (signed
// words), +2 up.
void Open(bool up) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 + (up ? 0x20u : 0xFFE0u));
    const std::int32_t slide = S16(S() + 0x30);
    if (up ? slide >= 0x100 : slide <= -0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
}
// Sub-state 3: once the leader is two cells off (the front test at 0x20000
// both ways fails), +2 up.
void WaitFar(std::int32_t delta) {
    unsigned char* const s = S();
    if (!LeaderAt(s, delta, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
}
// Sub-state 4: the slide 0x20 back toward 0; at 0 or past it, sound 0x201
// unless a message is up, +2 = 1 (read after the sound).
void Close(bool up) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 + (up ? 0xFFE0u : 0x20u));
    const std::int32_t slide = S16(S() + 0x30);
    if (up ? slide > 0 : slide < 0) return;
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(at::kSoundShut);
    S()[2] = 1;
}

// --- the draws' parts -------------------------------------------------------------------

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
// A vertex's ground point: (s16 + 0x4000) << 9, the 16.16 the map reads.
long Ground(const unsigned char* word) { return static_cast<long>(static_cast<U>(S16(word) + 0x4000) << 9); }
// base - ground / 2 (movsx ax; cdq; sub; sar 1: toward zero), the low word.
U Above(U base, long height) {
    const std::int32_t half = static_cast<std::int16_t>(static_cast<U>(height) & 0xFFFFu) / 2;
    return base - static_cast<U>(half);
}

// The draw sub-kinds 0x2E..0x32 share but for the texture word: the draw mode
// (page 0x95) committed at slot 6 (0xC); one POLY_FT4. With +8 the quad lies
// across z - its y (+0x3A << 7) - 0x3FC0 - and its x (+0x36 << 7) minus the
// slide +0x30, - 0x3F40 / - 0x4040; without, across x - x (+0x36 << 7) -
// 0x3FC0 - and its y (+0x3A << 7) plus the slide, - 0x3F40 / - 0x4040. Every
// word the low sixteen bits. The heights from the ground: corner 0 0x40 above
// half vertex 0's ground and corner 2 0x180, corners 1 and 3 likewise at
// vertex 1's (four AreaMap_Elevation calls, two pairs with the same
// arguments; vertex 1's point read after the second). Projected; then the
// sub-kind's texture word (from Sprite_Current, read after the projection),
// Prim_SetTexture(word, p, 1), committed at slot 6 (0x48).
void GroundDraw(U (*texture)(const unsigned char* s)) {
    unsigned char* const v = Vertices();
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    {
        const unsigned char* const s = S();
        const U slide = rv::As<records::EffectWork>(s)->work_30;
        if (s[8] != 0) {
            const U y = (static_cast<U>(rv::As<records::EffectObject>(s)->z_whole) << 7) - 0x3FC0u;
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].y, y);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].y, y);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].y, y);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->y, y);
            const U x = (static_cast<U>(rv::As<records::EffectObject>(s)->x_whole) << 7) - slide;
            SetWord(v + 0x10, x - 0x3F40u);
            SetWord(v, x - 0x3F40u);
            SetWord(v + 0x18, x - 0x4040u);
            SetWord(v + 8, x - 0x4040u);
        } else {
            const U x = (static_cast<U>(rv::As<records::EffectObject>(s)->x_whole) << 7) - 0x3FC0u;
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].x, x);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].x, x);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].x, x);
            SetWord(v, x);
            const U y = (static_cast<U>(rv::As<records::EffectObject>(s)->z_whole) << 7) + slide;
            SetWord(v + 0x12, y - 0x3F40u);
            SetWord(v + 2, y - 0x3F40u);
            SetWord(v + 0x1A, y - 0x4040u);
            SetWord(v + 0xA, y - 0x4040u);
        }
    }
    const long x0 = Ground(v), y0 = Ground(v + 2);
    long h = SH_CALL(AreaMap_Elevation)(x0, y0);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->z, Above(0x40, h));
    h = SH_CALL(AreaMap_Elevation)(x0, y0);
    const long x1 = Ground(v + 8), y1 = Ground(v + 0xA);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].z, Above(0x180, h));
    h = SH_CALL(AreaMap_Elevation)(x1, y1);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].z, Above(0x40, h));
    h = SH_CALL(AreaMap_Elevation)(x1, y1);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].z, Above(0x180, h));
    Project(p);
    const U word = texture(S());
    SH_CALL(Prim_SetTexture)(word, p, 1);
    SH_CALL(Gfx_CommitPrim)(6, 0x48);
}

}  // namespace

// ===========================================================================
// Sub-kind 0x4A's state 1 (EffectKind18Sub4A_States[1], E5G's table)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x2D: EffectKind18_States[0x2D] (0x654120), EffectKind18Sub2D_States
// (five) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x3E: EffectKind18_States[0x3E] (0x654164), EffectKind18Sub3E_States
// (five) by +2 - nothing drawn
// ===========================================================================

// ===========================================================================
// Sub-kinds 0x2E..0x32: the ground draws
// ===========================================================================

// ===========================================================================
// Sub-kinds 0x2E, 0x30, 0x31 (the slide toward -0x100) and 0x2F, 0x32 (toward
// +0x100): EffectKind18_States[0x2E..0x32] (0x654124..0x654134), each its own
// table of five by +2
// ===========================================================================

void Effect6A_Inject() {
    if (bof3::WantsShadow("effect_6a")) effect_6a::SelfTest();
}
