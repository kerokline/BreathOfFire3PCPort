// Round thirteen group E1C (docs/effect_1c.md): analysis/round13_cut.tsv's 52
// rows for E1C and the one start its span holds that no list has (0x46F230,
// kind 0x20's draw, reached by its dispatcher's tail jump), each read to its
// last instruction with capstone (2026-09-29) and fuzzed through the scenario
// harness's effect mode (scenario_harness.h, docs/scenario_harness.md section 8).
//
//   EffectKind36_Run           0x46A850  Effect_KindHandlers[0x36]: four VRAM moves, one a frame
//   EffectKind3C_Run           0x46A930  Effect_KindHandlers[0x3C]: EffectKind3C_States by +1
//   EffectKind70_Run           0x46D780  Effect_KindHandlers[0x70]: five areas' frame hooks, members' states
//   EffectKind1C_Run .. _End   0x46D890 .. 0x46DA10  Effect_KindHandlers[0x1C], EffectKind1C_States 0..4
//   EffectKind1C_DrawRing      0x46DA20  sixteen G4 quads round the record's point (E8)
//   EffectKind1C_MoveShards    0x46DD00  the 0x80 specks risen and drawn (E8)
//   EffectKind1D_Run .. _End   0x46DD50 .. 0x46DE70  Effect_KindHandlers[0x1D], EffectKind1D_States 0..4
//   EffectKind1D_DrawRing      0x46DE80  thirty-two G4 quads, the back eleven not committed (E8)
//   EffectShards_Clear         0x46E120  the 0x80 specks freed (kinds 0x1C, 0x1D, EffectKind2B_Rise)
//   EffectKind1D_MoveShards    0x46E140  as kind 0x1C's, drawn by 0x46E190
//   EffectKind1E_Run .. _FreeModel  0x46E200 .. 0x46E2D0  Effect_KindHandlers[0x1E], EffectKind1E_States 0..3
//   EffectKind1E_ShardsInit .. _ShardQuad  0x46E2E0 .. 0x46E400  eight shards (E8)
//   EffectKind1E_ShardFly .. _ShardNext3  0x46E5F0 .. 0x46E6D0  EffectKind1E_ShardStates 0..3
//   EffectKind1E_SplitModel .. _TurnPiece  0x46E6E0 .. 0x46E890  the model's 27 faces (E8)
//   EffectKind1E_DebrisInit .. _DrawModel  0x46EA60 .. 0x46EC20  debris and the model's lines (E8)
//   EffectKind1E_Winding       0x46EE20  a 2D cross product of three s16 pairs (E8)
//   EffectKind1F_Run .. _Debris  0x46EE90 .. 0x46EEE0  Effect_KindHandlers[0x1F], EffectKind1F_States 0, 1
//   EffectKind20_Run .. _Fall  0x46EF70 .. 0x46F1C0  Effect_KindHandlers[0x20], EffectKind20_States 0..6
//   EffectKind20_Draw          0x46F230  the fan, by the dispatcher's tail jump
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again after a call, and
// kept where it keeps it in a register. No divergence: each is a faithful
// replacement. Where the original would jump through a state table past its
// code, index a table past its end or write past ObjTrio's three records, ours
// aborts with a message (docs/effect_1c.md section 7).
#include "game/effect_1c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1c::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;
using U = std::uint32_t;
using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
short SW(const unsigned char* p) { return static_cast<short>(Word(p)); }
U Key(const void* p) { return Low32(p); }
U AddressOf(const void* p) { return Key(p); }
U Shards() { return AddressOf(EffectKind30_Shards); }

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32.
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");

// jmp / call [table + 4 * index]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original goes through the dword after - the next kind's table or data.
Handler Entry(U table, unsigned index, unsigned entries, const char* who) {
    if (index >= entries)
        bof3::Fatal("%s: index %u past the %u entries of the table at 0x%X (the original jumps through 0x%X)", who,
                    index, entries, (unsigned)table, (unsigned)(table + 4u * index));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4u * index))));
}

// (trig * radius) >> 4 + base, as the rings and the specks place a point: imul
// of the whole answer by the s16 radius, sar 4, add.
U Arm(int trig, short radius, U base) {
    return static_cast<U>(static_cast<std::int32_t>(static_cast<U>(trig) * static_cast<U>(static_cast<int>(radius))) >> 4) +
           base;
}

// The draw mode both rings put before each quad: Gpu_GetTPage(0, 1, 0x3C0, 0)
// (its fifth pushed zero left on the stack becomes the mode's texture window).
void RingMode(int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}
// The draw mode of the debris, the model and kind 0x20's fan: the page at
// (0x380, 0x100), then committed at slot 1.
void DebrisMode(int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
}

// The two projections of a ring's point: at the height +0x3C (out1), then at
// +0x3C + (+0x30 << 16) (out2), the point (x, z) at angle `angle` and radius
// +0x2E round (+0x34, +0x38). Sprite_Current read after each call, as the
// originals read it.
void RingPoints(U angle, long* point, float* out1, float* out2) {
    const int c = SH_CALL(Math_Cos)(static_cast<int>(angle));
    const unsigned char* s = Cur();
    point[0] = static_cast<long>(Arm(c, static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x)));
    const int n = SH_CALL(Math_Sin)(static_cast<int>(angle));
    s = Cur();
    point[1] = static_cast<long>(Arm(n, static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z)));
    point[2] = rv::As<records::EffectObject>(s)->height;
    SH_CALL(EffectGte_ProjectPoint)(point, out1);
    s = Cur();
    point[2] = static_cast<long>((static_cast<U>(static_cast<int>(SW(s + 0x30))) << 16) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height));
    SH_CALL(EffectGte_ProjectPoint)(point, out2);
}

// A quad's four vertices of three dwords (x, y, depth: floats) at +8, +0x18,
// +0x28, +0x38: the previous projections, then the new ones.
void RingVertices(unsigned char* prim, const U* prev1, const U* prev2, const float* out1, const float* out2) {
    for (unsigned i = 0; i < 3; ++i) {
        SetUL(prim + 0x08 + 4 * i, prev1[i]);
        SetUL(prim + 0x18 + 4 * i, prev2[i]);
        std::memcpy(prim + 0x28 + 4 * i, &out1[i], 4);
        std::memcpy(prim + 0x38 + 4 * i, &out2[i], 4);
    }
}
void Shade(unsigned char* prim, U at1, U at2, unsigned char v) {
    prim[at1] = v;
    prim[at1 + 1] = v;
    prim[at1 + 2] = v;
    prim[at2] = v;
    prim[at2 + 1] = v;
    prim[at2 + 2] = v;
}

// Kind 0x1C's and 0x1D's specks: 0x80 records of 0x14 at EffectKind30_Shards,
// each in use (+0) risen by its speed (+2, s16, << 8, off +0xC), drawn by
// `draw`, and freed below the record's height +0x3C (Sprite_Current read after
// the draw); al 1 when any was in use.
unsigned char MoveShards(U draw) {
    unsigned char any = 0;
    for (U r = 0; r < at::kShardCount; ++r) {
        unsigned char* const shard = At(Shards() + r * at::kShardStride);
        if (!shard[0]) continue;
        rv::Store<std::uint32_t>(rv::As<records::EffectSpeck>(shard)->height, static_cast<std::uint32_t>(rv::As<records::EffectSpeck>(shard)->height) - (static_cast<U>(static_cast<int>(static_cast<std::int16_t>(rv::As<records::EffectSpeck>(shard)->fall))) << 8));
        SH_WORD(void (__cdecl*)(unsigned char*), draw)(shard);
        if (rv::As<records::EffectSpeck>(shard)->height < rv::Current<records::EffectObject>()->height) shard[0] = 0;
        any = 1;
    }
    return any;
}

}  // namespace

// ===========================================================================
// Kind 0x36 and kind 0x3C's dispatcher; kind 0x70
// ===========================================================================

// ===========================================================================
// Kind 0x1C: Effect_KindHandlers[0x1C], EffectKind1C_States by +1
// ===========================================================================

// ===========================================================================
// Kind 0x1D: Effect_KindHandlers[0x1D], EffectKind1D_States by +1
// ===========================================================================

// ===========================================================================
// Kind 0x1E: Effect_KindHandlers[0x1E], EffectKind1E_States by +1
// ===========================================================================

// ===========================================================================
// Kind 0x1F: Effect_KindHandlers[0x1F], EffectKind1F_States by +1
// ===========================================================================

// ===========================================================================
// Kind 0x20: Effect_KindHandlers[0x20], EffectKind20_States by +1
// ===========================================================================

void Effect1C_Inject() {
    if (bof3::WantsShadow("effect_1c")) effect_1c::SelfTest();
}
