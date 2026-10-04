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
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1c::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;
using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
short SW(const unsigned char* p) { return static_cast<short>(Word(p)); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U AddressOf(const void* p) { return Key(p); }
U Shards() { return AddressOf(EffectKind30_Shards); }
// Kind 0x1E's shard cursor, EffectKind1E_ShardCursor 0x675FDC.
unsigned char* Cursor() { return EffectKind1E_ShardCursor; }
void SetCursor(U v) { EffectKind1E_ShardCursor = At(v); }
void StoreFloat(unsigned char* at, double v) {
    const auto f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

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
    point[0] = static_cast<long>(Arm(c, SW(s + 0x2E), UL(s + 0x34)));
    const int n = SH_CALL(Math_Sin)(static_cast<int>(angle));
    s = Cur();
    point[1] = static_cast<long>(Arm(n, SW(s + 0x2E), UL(s + 0x38)));
    point[2] = static_cast<long>(UL(s + 0x3C));
    SH_CALL(EffectGte_ProjectPoint)(point, out1);
    s = Cur();
    point[2] = static_cast<long>((static_cast<U>(static_cast<int>(SW(s + 0x30))) << 16) + UL(s + 0x3C));
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
        SetUL(shard + 0xC, UL(shard + 0xC) - (static_cast<U>(static_cast<int>(SW(shard + 2))) << 8));
        SH_AT(void (__cdecl*)(unsigned char*), draw)(shard);
        if (Long(shard + 0xC) < Long(Cur() + 0x3C)) shard[0] = 0;
        any = 1;
    }
    return any;
}

}  // namespace

// ===========================================================================
// Kind 0x36 and kind 0x3C's dispatcher; kind 0x70
// ===========================================================================

// original 0x46A850 (Effect_KindHandlers[0x36], hidden in ours
// EffectKind32_Arc's recorded extent): +9 counting down first; at 0 a VRAM
// move (Gpu_SetDrawMove at Gfx_PacketNext) of the 16 x 40 rectangle at
// EffectKind36_Frames[+1] (x / 4 toward zero + 0x2C0, y + 0x100) to frame 0's
// (the same sums, as dwords); +1 up, +0x29 0, +9 the new +1, committed at
// slot +0x29 (0x18 bytes); at +1 4, Effect_Release. An index past the four is
// the original's read of what follows the table: ours aborts.
extern "C" void __cdecl EffectKind36_Run(void) {
    unsigned char* s = Cur();
    if (s[9]) {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        return;
    }
    const unsigned i = s[1];
    if (i >= 4) bof3::Fatal("EffectKind36_Run: +1 is %u, past EffectKind36_Frames' four", i);
    const unsigned char* const frames = reinterpret_cast<const unsigned char*>(EffectKind36_Frames);
    short rect[4];
    rect[0] = static_cast<short>(SW(frames + 4 * i) / 4 + 0x2C0);
    rect[1] = static_cast<short>(Word(frames + 4 * i + 2) + 0x100u);
    rect[2] = 0x10;
    rect[3] = 0x28;
    const U x = static_cast<U>(SW(frames) / 4 + 0x2C0);
    const U y = static_cast<U>(static_cast<int>(SW(frames + 2)) + 0x100);
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, reinterpret_cast<const unsigned char*>(rect), x, y);
    s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
    Cur()[0x29] = 0;
    s = Cur();
    s[9] = s[1];
    SH_CALL(Gfx_CommitPrim)(Cur()[0x29], 0x18);
    if (Cur()[1] == 4) SH_CALL(Effect_Release)();
}

// original 0x46A930 (Effect_KindHandlers[0x3C], hidden in ours
// EffectKind32_Arc's recorded extent): a tail jump through EffectKind3C_States
// by +1 (three entries: FC1's _Start, _Hold, E2D's EffectTwinSprites_Release).
extern "C" void __cdecl EffectKind3C_Run(void) {
    Entry(AddressOf(EffectKind3C_States), Cur()[1], 3, "EffectKind3C_Run")();
}

// original 0x46D780 (Effect_KindHandlers[0x70]): nothing while bit 10 of
// Field_ScriptFlags2 is set; else the area's frame hook by Game_AreaNumber (49,
// 117, 118, 169, 171: ours Area49_EffectFrame, Area117 / 118 / 169 /
// 171_MembersFrame), then for each member below Field_MemberCount (read after
// the hook) whose bit is set in the record's +0xB (Sprite_Current read once,
// after the hook; the bit `1 << i` taken to its low byte, the shift count
// masked to five bits) the ObjTrio record's +1 = 1, +2 = 2 for the leader and
// 1 for the others, +3 = +4 = 0. A set bit for a member past the three writes
// past ObjTrio in the original: ours aborts.
extern "C" void __cdecl EffectKind70_Run(void) {
    if (Field_ScriptFlags2 & 0x400) return;
    switch (Game_AreaNumber) {
    case 0x31: SH_CALL(Area49_EffectFrame)(); break;
    case 0x75: SH_CALL(Area117_MembersFrame)(); break;
    case 0x76: SH_CALL(Area118_MembersFrame)(); break;
    case 0xA9: SH_CALL(Area169_MembersFrame)(); break;
    case 0xAB: SH_CALL(Area171_MembersFrame)(); break;
    default: break;
    }
    const unsigned n = Field_MemberCount;
    if (n == 0) return;
    const unsigned char* const s = Cur();
    for (unsigned i = 0; i < n; ++i) {
        const unsigned bit = (1u << (i & 31)) & 0xFFu;
        if (!(s[0xB] & bit)) continue;
        if (i >= 3)
            bof3::Fatal("EffectKind70_Run: +0xB has bit %u set with %u members; the original writes ObjTrio record %u, "
                        "past the three", i, n, i);
        unsigned char* const o = ObjTrio + i * 0x14Cu;
        o[1] = 1;
        o[2] = static_cast<unsigned char>(i == 0 ? 2 : 1);
        o[3] = 0;
        o[4] = 0;
    }
}

// ===========================================================================
// Kind 0x1C: Effect_KindHandlers[0x1C], EffectKind1C_States by +1
// ===========================================================================

// original 0x46D890: a tail jump through EffectKind1C_States 0x654210 by +1.
extern "C" void __cdecl EffectKind1C_Run(void) { Entry(AddressOf(EffectKind1C_States), Cur()[1], 5, "EffectKind1C_Run")(); }

// original 0x46D8B0 (EffectKind1C_States[0]): the point set to (0x600000,
// 0x110000) and its height +0x3C the ground there (AreaMap_Elevation, s16 <<
// 16); the ring's radius +0x2E 0x180 and height +0x30 0x500; +9 0; the specks
// cleared (EffectShards_Clear); +1 up; Sound_PlayEffect(0x202).
extern "C" void __cdecl EffectKind1C_Start(void) {
    SetUL(Cur() + 0x34, 0x600000);
    SetUL(Cur() + 0x38, 0x110000);
    unsigned char* s = Cur();
    const long ground = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    SetUL(Cur() + 0x3C, static_cast<U>(static_cast<int>(static_cast<short>(ground))) << 16);
    SetWord(Cur() + 0x2E, 0x180);
    SetWord(Cur() + 0x30, 0x500);
    Cur()[9] = 0;
    SH_CALL(EffectShards_Clear)();
    s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
    SH_CALL(Sound_PlayEffect)(0x202);
}

// original 0x46D930 (EffectKind1C_States[1]): the ring at brightness +9, a
// speck spawned (EffectSpecks_Spawn) on even frames, the specks moved; +9 up 4 -
// at 0 (a wrap), +9 0x20 and +1 up.
extern "C" void __cdecl EffectKind1C_Rise(void) {
    SH_CALL(EffectKind1C_DrawRing)(Cur()[9]);
    if (!(Frame_Counter & 1)) SH_AT(unsigned char (__cdecl*)(void), at::kShardSpawn)();
    SH_CALL(EffectKind1C_MoveShards)();
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] + 4);
    s = Cur();
    if (s[9] == 0) {
        s[9] = 0x20;
        s = Cur();
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
}

// original 0x46D980 (EffectKind1C_States[2]): the ring at full brightness
// (0xFF), a speck on even frames, the specks moved; +9 down 1 - at 0, +9 0xFC
// and +1 up.
extern "C" void __cdecl EffectKind1C_Hold(void) {
    SH_CALL(EffectKind1C_DrawRing)(0xFF);
    if (!(Frame_Counter & 1)) SH_AT(unsigned char (__cdecl*)(void), at::kShardSpawn)();
    SH_CALL(EffectKind1C_MoveShards)();
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Cur();
    if (s[9] == 0) {
        s[9] = 0xFC;
        s = Cur();
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
}

// original 0x46D9D0 (EffectKind1C_States[3]): the ring at brightness +9, the
// specks moved (none spawned); +9 down 4 - at 0, +1 up.
extern "C" void __cdecl EffectKind1C_Fade(void) {
    SH_CALL(EffectKind1C_DrawRing)(Cur()[9]);
    SH_CALL(EffectKind1C_MoveShards)();
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] + 0xFC);
    s = Cur();
    if (s[9] == 0) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46DA10 (EffectKind1C_States[4]): the specks moved; none in use,
// Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind1C_End(void) {
    if (!SH_CALL(EffectKind1C_MoveShards)()) SH_CALL(Effect_Release)();
}

// original 0x46DA20: the ring. EffectGte_LoadMapCamera; the point at angle 0
// projected at the ground and at the ring's height (RingPoints); the shade
// byte 0x40, or 0x44 on odd frames; then for the angles 0x100 .. 0x1000 (16
// steps; the original keeps the angle in its argument's slot): both points
// projected again; a draw mode (RingMode, dtd 1) linked at the point
// (MapView_LinkPrimAt(x, z, 0, 0xC)); a POLY_G4 at Gfx_PacketNext (read
// again), semi-transparent, its vertices the last and the new projections;
// the first two vertices' colour shade * brightness >> 8, the shade then down
// 8 for steps 4..11 and up 8 for the others (a byte), the last two's the new
// shade * brightness >> 8; the quad linked (0x44), a second draw mode (dtd 0)
// linked after it. Only the argument's low byte is read.
extern "C" void __cdecl EffectKind1C_DrawRing(unsigned brightness) {
    SH_CALL(EffectGte_LoadMapCamera)();
    long point[3];
    float out1[3], out2[3];
    RingPoints(0, point, out1, out2);
    unsigned char shade = static_cast<unsigned char>(((Frame_Counter & 1) + 0x10) << 2);
    const U bright = brightness & 0xFFu;
    U angle = 0;
    for (unsigned k = 0; k < 0x10; ++k) {
        U prev1[3], prev2[3];
        std::memcpy(prev1, out1, sizeof prev1);
        std::memcpy(prev2, out2, sizeof prev2);
        angle += 0x100;
        RingPoints(angle & 0xFFFF, point, out1, out2);
        RingMode(1);
        SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(point[0]), static_cast<unsigned long>(point[1]), 0, 0xC);
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        RingVertices(prim, prev1, prev2, out1, out2);
        Shade(prim, 4, 0x14, static_cast<unsigned char>((shade * bright) >> 8));
        shade = static_cast<unsigned char>(k >= 4 && k < 0xC ? shade - 8 : shade + 8);
        Shade(prim, 0x24, 0x34, static_cast<unsigned char>((shade * bright) >> 8));
        SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(point[0]), static_cast<unsigned long>(point[1]), 0, 0x44);
        RingMode(0);
        SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(point[0]), static_cast<unsigned long>(point[1]), 0, 0xC);
    }
}

// original 0x46DD00: kind 0x1C's specks moved and drawn (EffectSpecks_Draw, a
// TILE_1 linked at the speck's point); al 1 when any was in use.
extern "C" unsigned char __cdecl EffectKind1C_MoveShards(void) { return MoveShards(at::kShardTile); }

// ===========================================================================
// Kind 0x1D: Effect_KindHandlers[0x1D], EffectKind1D_States by +1
// ===========================================================================

// original 0x46DD50: a tail jump through EffectKind1D_States 0x654224 by +1.
extern "C" void __cdecl EffectKind1D_Run(void) { Entry(AddressOf(EffectKind1D_States), Cur()[1], 5, "EffectKind1D_Run")(); }

// original 0x46DD70 (EffectKind1D_States[0]): the ring's radius +0x2E 0x280
// and height +0x30 0x600 (the point where the spawner put it); +9 0; the
// specks cleared; +1 up.
extern "C" void __cdecl EffectKind1D_Start(void) {
    SetWord(Cur() + 0x2E, 0x280);
    SetWord(Cur() + 0x30, 0x600);
    Cur()[9] = 0;
    SH_CALL(EffectShards_Clear)();
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46DDA0 (EffectKind1D_States[1]): the ring at brightness +9, a
// speck spawned (EffectSpecks_Spawn) on odd frames, the specks moved; +9 up 4 -
// at 0, +9 0x20 and +1 up.
extern "C" void __cdecl EffectKind1D_Rise(void) {
    SH_CALL(EffectKind1D_DrawRing)(Cur()[9]);
    if (Frame_Counter & 1) SH_AT(unsigned char (__cdecl*)(void), at::kShardSpawn)();
    SH_CALL(EffectKind1D_MoveShards)();
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] + 4);
    s = Cur();
    if (s[9] == 0) {
        s[9] = 0x20;
        s = Cur();
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
}

// original 0x46DDF0 (EffectKind1D_States[2]): the ring at full brightness, a
// speck on odd frames, the specks moved; held until the counter byte 0x903848
// is 0x1F - then +9 0xFC and +1 up.
extern "C" void __cdecl EffectKind1D_Hold(void) {
    SH_CALL(EffectKind1D_DrawRing)(0xFF);
    if (Frame_Counter & 1) SH_AT(unsigned char (__cdecl*)(void), at::kShardSpawn)();
    SH_CALL(EffectKind1D_MoveShards)();
    if (At(0x903848)[0] == 0x1F) {
        Cur()[9] = 0xFC;
        unsigned char* const s = Cur();
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
}

// original 0x46DE30 (EffectKind1D_States[3]): the ring at brightness +9, the
// specks moved; +9 down 4 - at 0, +1 up.
extern "C" void __cdecl EffectKind1D_Fade(void) {
    SH_CALL(EffectKind1D_DrawRing)(Cur()[9]);
    SH_CALL(EffectKind1D_MoveShards)();
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] + 0xFC);
    s = Cur();
    if (s[9] == 0) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46DE70 (EffectKind1D_States[4]): the specks moved; none in use,
// Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind1D_End(void) {
    if (!SH_CALL(EffectKind1D_MoveShards)()) SH_CALL(Effect_Release)();
}

// original 0x46DE80: kind 0x1C's ring with 32 steps of 0x80: the draw mode
// (dtd 1) and the quad committed at slot 1 (Gfx_CommitPrim, 0xC and 0x44)
// only for steps below 0xB or from 0x1C - the others are built at the same
// Gfx_PacketNext and overwritten, a gap in the ring; the shade down 8 for steps
// 8..0x17, up 8 for the others. No second draw mode.
extern "C" void __cdecl EffectKind1D_DrawRing(unsigned brightness) {
    SH_CALL(EffectGte_LoadMapCamera)();
    long point[3];
    float out1[3], out2[3];
    RingPoints(0, point, out1, out2);
    unsigned char shade = static_cast<unsigned char>(((Frame_Counter & 1) + 0x10) << 2);
    const U bright = brightness & 0xFFu;
    U angle = 0;
    for (unsigned k = 0; k < 0x20; ++k) {
        U prev1[3], prev2[3];
        std::memcpy(prev1, out1, sizeof prev1);
        std::memcpy(prev2, out2, sizeof prev2);
        angle += 0x80;
        RingPoints(angle & 0xFFFF, point, out1, out2);
        RingMode(1);
        const bool drawn = k < 0xB || k >= 0x1C;
        if (drawn) SH_CALL(Gfx_CommitPrim)(1, 0xC);
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        RingVertices(prim, prev1, prev2, out1, out2);
        Shade(prim, 4, 0x14, static_cast<unsigned char>((shade * bright) >> 8));
        shade = static_cast<unsigned char>(k >= 8 && k < 0x18 ? shade - 8 : shade + 8);
        Shade(prim, 0x24, 0x34, static_cast<unsigned char>((shade * bright) >> 8));
        if (drawn) SH_CALL(Gfx_CommitPrim)(1, 0x44);
    }
}

// original 0x46E120: every speck of EffectKind30_Shards' 0x80 (0x14 apart)
// freed (+0 = 0). Kind 0x1C's and 0x1D's starts and EffectKind2B_Rise.
extern "C" void __cdecl EffectShards_Clear(void) {
    for (U r = 0; r < at::kShardCount; ++r) At(Shards() + r * at::kShardStride)[0] = 0;
}

// original 0x46E140: kind 0x1D's specks moved and drawn by 0x46E190 (the same
// TILE_1, committed at slot 1); al 1 when any was in use.
extern "C" unsigned char __cdecl EffectKind1D_MoveShards(void) { return MoveShards(at::kShardTile2); }

// ===========================================================================
// Kind 0x1E: Effect_KindHandlers[0x1E], EffectKind1E_States by +1
// ===========================================================================

// original 0x46E200: a tail jump through EffectKind1E_States 0x654238 by +1
// (five: _Start, _Glow, _Burst, _FreeModel, Effect_StateRelease).
extern "C" void __cdecl EffectKind1E_Run(void) { Entry(AddressOf(EffectKind1E_States), Cur()[1], 5, "EffectKind1E_Run")(); }

// original 0x46E220 (EffectKind1E_States[0]): the sixteen debris, the eight
// shards and the model's 27 faces set up; +9 0, +1 up.
extern "C" void __cdecl EffectKind1E_Start(void) {
    SH_CALL(EffectKind1E_DebrisInit)();
    SH_CALL(EffectKind1E_ShardsInit)();
    SH_CALL(EffectKind1E_SplitModel)();
    Cur()[9] = 0;
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46E250 (EffectKind1E_States[1]): the model's lines at shade +9;
// +9 up 2 - from 0x80, Sound_PlayEffect 0x206, 0x20E, 0x203, +9 0 and +1 up.
extern "C" void __cdecl EffectKind1E_Glow(void) {
    SH_CALL(EffectKind1E_DrawModel)(Cur()[9]);
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] + 2);
    if (Cur()[9] < 0x80) return;
    SH_CALL(Sound_PlayEffect)(0x206);
    SH_CALL(Sound_PlayEffect)(0x20E);
    SH_CALL(Sound_PlayEffect)(0x203);
    Cur()[9] = 0;
    s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46E2B0 (EffectKind1E_States[2]): the debris drawn, the faces
// stepped, the shards drawn - none left, +1 up.
extern "C" void __cdecl EffectKind1E_Burst(void) {
    SH_CALL(EffectKind1E_DebrisDraw)();
    SH_CALL(EffectKind1E_StepPieces)();
    if (!SH_CALL(EffectKind1E_ShardsDraw)()) {
        unsigned char* const s = Cur();
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
}

// original 0x46E2D0 (EffectKind1E_States[3]): Sprite_ObjectsExtra[0]'s +0 = 0
// (the extra sprite whose model it drew freed); +1 up (Sprite_Current read
// before the store).
extern "C" void __cdecl EffectKind1E_FreeModel(void) {
    unsigned char* const s = Cur();
    Sprite_ObjectsExtra[0] = 0;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46E2E0: the cursor 0x675FDC set to EffectKind30_Shards, then
// eight times EffectKind1E_ShardInit(the cursor) and the cursor up 0x28 (read
// again after the call).
extern "C" void __cdecl EffectKind1E_ShardsInit(void) {
    SetCursor(Shards());
    for (unsigned i = 0; i < 8; ++i) {
        SH_CALL(EffectKind1E_ShardInit)(Cursor());
        SetCursor(Key(Cursor()) + 0x28);
    }
}

// original 0x46E320: a shard of 0x28 bytes: its point +4 / +8 / +0xC the
// record's +0x34 / +0x38 / +0x3C; a direction (Rand & 0xFF) - 0x80, (Rand &
// 0xFF) - 0x80, Rand & 0x7F at +0x14 .. +0x1C normalised in place
// (Gte_VectorNormal), then +0x1C << 8; +0 1 (in use), +1 0 (its state), +0x24
// 0 (its size), +3 0x40 (its shade), +2 4 (its count). Also E2B's EffectKind35_ShardsInit's.
extern "C" void __cdecl EffectKind1E_ShardInit(unsigned char* shard) {
    SetUL(shard + 4, UL(Cur() + 0x34));
    SetUL(shard + 8, UL(Cur() + 0x38));
    SetUL(shard + 0xC, UL(Cur() + 0x3C));
    SetUL(shard + 0x14, (static_cast<U>(SH_CALL(Rand)()) & 0xFFu) - 0x80u);
    SetUL(shard + 0x18, (static_cast<U>(SH_CALL(Rand)()) & 0xFFu) - 0x80u);
    SetUL(shard + 0x1C, static_cast<U>(SH_CALL(Rand)()) & 0x7Fu);
    SH_CALL(Gte_VectorNormal)(reinterpret_cast<const long*>(shard + 0x14), reinterpret_cast<long*>(shard + 0x14));
    SetUL(shard + 0x1C, UL(shard + 0x1C) << 8);
    shard[0] = 1;
    shard[1] = 0;
    SetWord(shard + 0x24, 0);
    shard[3] = 0x40;
    shard[2] = 4;
}

// original 0x46E3B0: EffectGte_LoadMapCamera; the cursor 0x675FDC walked over
// the eight shards: each in use stepped by EffectKind1E_ShardStates[+1] (a
// call, the handler reading the cursor) and drawn (EffectKind1E_ShardQuad of
// the cursor, read again after each call); al 1 when any was in use.
extern "C" unsigned char __cdecl EffectKind1E_ShardsDraw(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    U c = Shards();
    SetCursor(c);
    unsigned char any = 0;
    for (unsigned i = 0; i < 8; ++i) {
        if (At(c)[0]) {
            Entry(AddressOf(EffectKind1E_ShardStates), At(c)[1], 4, "EffectKind1E_ShardsDraw")();
            SH_CALL(EffectKind1E_ShardQuad)(Cursor());
            c = Key(Cursor());
            any = 1;
        }
        c += 0x28;
        SetCursor(c);
    }
    return any;
}

// original 0x46E400: a shard's POLY_FT4 at Gfx_PacketNext (read once),
// semi-transparent - FC2's EffectKind30_SparkQuad with the shard's point
// (+4), size (+0x24, s16, both axes) and shade (+3, read for each byte): the
// point projected (EffectGte_ProjectPoint), the size scaled at its depth in
// place (EffectGte_ProjectSize); the corners x - (w >> 1) and + w, y - (h >>
// 1) and + h at the x87's 53 bits, rounded once to a float; the depth at
// +0x10 +0x20 +0x30 +0x40; u 0xE0 / 0xFF, v 0x30 / 0x4F; the CLUT
// Gpu_GetClut(0xA0, 0x1E3), the page Gpu_GetTPage(0, 1, 0x2C0, 0x100);
// MapView_LinkPrimAt(x, z of the point, 2, 0x48). The original keeps a scratch
// word in its argument's slot.
extern "C" void __cdecl EffectKind1E_ShardQuad(unsigned char* shard) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const long* const point = reinterpret_cast<const long*>(shard + 4);
    float screen[3];
    SH_CALL(EffectGte_ProjectPoint)(point, screen);
    short wh[2] = {SW(shard + 0x24), SW(shard + 0x24)};
    SH_CALL(EffectGte_ProjectSize)(point, wh, wh);
    const int w = wh[0], h = wh[1];
    const int hw = static_cast<short>(wh[0]) >> 1, hh = static_cast<short>(wh[1]) >> 1;
    const double left = static_cast<double>(screen[0]) - static_cast<double>(hw);
    const double top = static_cast<double>(screen[1]) - static_cast<double>(hh);
    StoreFloat(prim + 0x08, left);
    StoreFloat(prim + 0x0C, top);
    StoreFloat(prim + 0x18, left + static_cast<double>(w));
    StoreFloat(prim + 0x1C, top);
    StoreFloat(prim + 0x28, left);
    StoreFloat(prim + 0x2C, top + static_cast<double>(h));
    StoreFloat(prim + 0x38, left + static_cast<double>(w));
    prim[0x15] = 0x30;
    prim[0x25] = 0x30;
    prim[0x14] = 0xE0;
    prim[0x24] = 0xFF;
    prim[0x34] = 0xE0;
    prim[0x35] = 0x4F;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x4F;
    StoreFloat(prim + 0x3C, top + static_cast<double>(h));
    std::memcpy(prim + 0x40, &screen[2], 4);
    std::memcpy(prim + 0x30, &screen[2], 4);
    std::memcpy(prim + 0x20, &screen[2], 4);
    std::memcpy(prim + 0x10, &screen[2], 4);
    SetWord(prim + 0x16, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    SetWord(prim + 0x26, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    prim[4] = shard[3];
    prim[5] = shard[3];
    prim[6] = shard[3];
    SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(shard + 4)), static_cast<unsigned long>(Long(shard + 8)),
                                2, 0x48);
}

// original 0x46E5F0 (EffectKind1E_ShardStates[0]): the cursor's shard moved by
// twice its direction, its size +0x24 up 0x80, its count +2 down - at 0, +2
// 0x40 and its state +1 up. The cursor read again for every access.
extern "C" void __cdecl EffectKind1E_ShardFly(void) {
    unsigned char* r = Cursor();
    SetUL(r + 4, UL(r + 4) + (UL(r + 0x14) << 1));
    r = Cursor();
    SetUL(r + 8, UL(r + 8) + (UL(r + 0x18) << 1));
    r = Cursor();
    SetUL(r + 0xC, UL(r + 0xC) + (UL(r + 0x1C) << 1));
    r = Cursor();
    SetWord(r + 0x24, Word(r + 0x24) + 0x80u);
    r = Cursor();
    r[2] = static_cast<unsigned char>(r[2] - 1);
    r = Cursor();
    if (r[2] == 0) {
        r[2] = 0x40;
        r = Cursor();
        r[1] = static_cast<unsigned char>(r[1] + 1);
    }
}

// original 0x46E660 (EffectKind1E_ShardStates[1]): moved by its direction, its
// shade +3 and count +2 down 1 - the count at 0, the shard freed (+0 = 0).
extern "C" void __cdecl EffectKind1E_ShardFade(void) {
    unsigned char* r = Cursor();
    SetUL(r + 4, UL(r + 4) + UL(r + 0x14));
    r = Cursor();
    SetUL(r + 8, UL(r + 8) + UL(r + 0x18));
    r = Cursor();
    SetUL(r + 0xC, UL(r + 0xC) + UL(r + 0x1C));
    r = Cursor();
    r[3] = static_cast<unsigned char>(r[3] - 1);
    r = Cursor();
    r[2] = static_cast<unsigned char>(r[2] - 1);
    r = Cursor();
    if (r[2] == 0) r[0] = 0;
}

// original 0x46E6C0 / 0x46E6D0 (EffectKind1E_ShardStates[2] / [3], two copies
// of one body): the cursor's shard's state +1 up. No shard reaches them in
// the original's flow ([1] frees it and never steps +1).
extern "C" void __cdecl EffectKind1E_ShardNext2(void) {
    unsigned char* const r = Cursor();
    r[1] = static_cast<unsigned char>(r[1] + 1);
}
extern "C" void __cdecl EffectKind1E_ShardNext3(void) {
    unsigned char* const r = Cursor();
    r[1] = static_cast<unsigned char>(r[1] + 1);
}

// original 0x46E6E0: the model (Sprite_ObjectsExtra[0] +0x50, read once) split
// into its 27 faces: each face's 0x28 bytes copied (dword by dword, forward)
// to 0x92C348 + 0x28 i; its centre - the four vertices' (+2, +8, +0xE,
// +0x14: x, y, z s16 each) sums >> 2, 16 bits each - at the piece record
// 0x92C0C0 + 0x18 i; the centre as three longs normalised (Gte_VectorNormalS)
// into +8..+0xD and each s16 there >> 8 (a direction); +0x10..+0x15 0 (a
// velocity); then the copy's twelve vertex words made relative to the centre.
extern "C" void __cdecl EffectKind1E_SplitModel(void) {
    U src = UL(At(at::kModelFaces));
    for (U i = 0; i < at::kPieceCount; ++i) {
        unsigned char* const d = At(at::kPieceCopies + 0x28 * i);
        unsigned char* const r = At(at::kPieces + 0x18 * i);
        for (U j = 0; j < 0x28; j += 4) SetUL(d + j, UL(At(src + j)));
        SetWord(r + 0, static_cast<U>((SW(d + 0x14) + SW(d + 2) + SW(d + 8) + SW(d + 0xE)) >> 2));
        SetWord(r + 2, static_cast<U>((SW(d + 0xA) + SW(d + 0x10) + SW(d + 4) + SW(d + 0x16)) >> 2));
        const int z = (SW(d + 0xC) + SW(d + 0x12) + SW(d + 0x18) + SW(d + 6)) >> 2;
        SetWord(r + 4, static_cast<U>(z));
        const long centre[3] = {SW(r + 0), SW(r + 2), static_cast<short>(z)};
        SH_CALL(Gte_VectorNormalS)(centre, reinterpret_cast<short*>(r + 8));
        for (U k = 8; k <= 0xC; k += 2) SetWord(r + k, static_cast<U>(SW(r + k) >> 8));
        SetWord(r + 0x10, 0);
        SetWord(r + 0x12, 0);
        SetWord(r + 0x14, 0);
        for (U v = 2; v <= 0x14; v += 6)
            for (U k = 0; k < 3; ++k) SetWord(d + v + 2 * k, Word(d + v + 2 * k) - Word(r + 2 * k));
        src += 0x28;
    }
}

// original 0x46E830: each of the 27 pieces stepped - its centre x, y by its
// velocity +8 / +0xA, its z velocity +0xC up by bit 0 of Frame_Counter (read
// for each), its z +4 by that - and turned (EffectKind1E_TurnPiece(the copy,
// the model's face, the piece)); the model's faces from
// Sprite_ObjectsExtra[0] +0x50 read once.
extern "C" void __cdecl EffectKind1E_StepPieces(void) {
    const U model = UL(At(at::kModelFaces));
    for (U i = 0; i < at::kPieceCount; ++i) {
        const U odd = Frame_Counter & 1;
        unsigned char* const r = At(at::kPieces + 0x18 * i);
        SetWord(r + 0, Word(r + 0) + Word(r + 8));
        SetWord(r + 2, Word(r + 2) + Word(r + 0xA));
        SetWord(r + 0xC, Word(r + 0xC) + odd);
        SetWord(r + 4, Word(r + 4) + Word(r + 0xC));
        SH_CALL(EffectKind1E_TurnPiece)(At(at::kPieceCopies + 0x28 * i), At(model + 0x28 * i), r);
    }
}

// original 0x46E890: a piece's angles +0x10..+0x14 each Rand & 0xFC0; a
// rotation of them (Gte_RotMatrix), no translation, loaded
// (Gte_SetTransMatrix, Gte_SetRotMatrix); each of the copy's four relative
// vertices (+2, +8, +0xE, +0x14) turned (Gte_RotTrans) and written to the same
// place of the face plus the piece's centre (16 bits; the centre read after
// each call). The original hands Gte_RotTrans its third argument's slot as the
// flag; ours has no flag argument.
extern "C" void __cdecl EffectKind1E_TurnPiece(const unsigned char* copy, unsigned char* face, unsigned char* piece) {
    SetWord(piece + 0x10, static_cast<U>(SH_CALL(Rand)()) & 0xFC0u);
    SetWord(piece + 0x12, static_cast<U>(SH_CALL(Rand)()) & 0xFC0u);
    SetWord(piece + 0x14, static_cast<U>(SH_CALL(Rand)()) & 0xFC0u);
    Matrix m;
    SH_CALL(Gte_RotMatrix)(reinterpret_cast<const short*>(piece + 0x10), m.m);
    m.t[0] = m.t[1] = m.t[2] = 0;
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    for (U v = 2; v <= 0x14; v += 6) {
        short vector[4];
        vector[0] = SW(copy + v);
        vector[1] = SW(copy + v + 2);
        vector[2] = SW(copy + v + 4);
        long out[3];
        SH_CALL(Gte_RotTrans)(vector, out);
        SetWord(face + v, Word(piece + 0) + static_cast<U>(out[0]));
        SetWord(face + v + 2, Word(piece + 2) + static_cast<U>(out[1]));
        SetWord(face + v + 4, Word(piece + 4) + static_cast<U>(out[2]));
    }
}

// original 0x46EA60: the sixteen debris records of 0x2C at 0x92C780 set up
// (EffectKind1E_DebrisInitOne).
extern "C" void __cdecl EffectKind1E_DebrisInit(void) {
    for (U i = 0; i < 16; ++i) SH_CALL(EffectKind1E_DebrisInitOne)(At(at::kDebris + 0x2C * i));
}

// original 0x46EA80 (PSX twin 0x801F3BE4 by callers): a debris record: its
// point +0 / +4 / +8 the record's +0x34 / +0x38 / +0x3C; three angles Rand &
// 0xFFF, Rand & 0x3FF, Rand & 0xFFF; two edges (Math_Cos, Math_Sin of 0x10;
// of -0x10; z 0) at +0x10 and +0x18 turned in place by a matrix of the three
// angles (EffectGte_SetDiagonalOne, then Gte_RotMatrixX, _Y by the second
// negated, _Z; 0x5A7C70 between Gte_PushMatrix and Gte_PopMatrix); its scale
// +0x28 10 + Rand % 4; +0x20, +0x22, +0x24 (its angle), +0x2A (its shade) 0.
// E3C's 0x4851E0 is the same with 0x20 and 8 + Rand % 8.
extern "C" void __cdecl EffectKind1E_DebrisInitOne(unsigned char* debris) {
    SetUL(debris + 0, UL(Cur() + 0x34));
    SetUL(debris + 4, UL(Cur() + 0x38));
    SetUL(debris + 8, UL(Cur() + 0x3C));
    const U ax = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U ay = static_cast<U>(SH_CALL(Rand)()) & 0x3FFu;
    const U az = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    SetWord(debris + 0x10, static_cast<U>(SH_CALL(Math_Cos)(0x10)));
    SetWord(debris + 0x12, static_cast<U>(SH_CALL(Math_Sin)(0x10)));
    SetWord(debris + 0x14, 0);
    SetWord(debris + 0x18, static_cast<U>(SH_CALL(Math_Cos)(-0x10)));
    SetWord(debris + 0x1A, static_cast<U>(SH_CALL(Math_Sin)(-0x10)));
    SetWord(debris + 0x1C, 0);
    Matrix m;
    SH_CALL(EffectGte_SetDiagonalOne)(m.m);
    SH_CALL(Gte_RotMatrixX)(static_cast<short>(ax), m.m);
    SH_CALL(Gte_RotMatrixY)(-static_cast<int>(static_cast<short>(ay)), m.m);
    SH_CALL(Gte_RotMatrixZ)(static_cast<short>(az), m.m);
    SH_CALL(Gte_PushMatrix)();
    using Turn = void (__cdecl*)(const short*, const short*, short*);
    SH_AT(Turn, at::kMatrixVector)(m.m, reinterpret_cast<const short*>(debris + 0x10), reinterpret_cast<short*>(debris + 0x10));
    SH_AT(Turn, at::kMatrixVector)(m.m, reinterpret_cast<const short*>(debris + 0x18), reinterpret_cast<short*>(debris + 0x18));
    SH_CALL(Gte_PopMatrix)();
    SetWord(debris + 0x28, (static_cast<U>(SH_CALL(Rand)()) & 3u) + 10u);
    SetWord(debris + 0x20, 0);
    SetWord(debris + 0x22, 0);
    SetWord(debris + 0x24, 0);
    SetWord(debris + 0x2A, 0);
}

// The debris loop of kind 0x1E (16 at 0x92C780) and kind 0x1F (32 at
// 0x92BF80): each drawn (E3C's 0x485030), its angle +0x24 up 0x10, its shade
// +0x2A up 0x20 while the record's +9 (Sprite_Current read after the draw) is
// below 4, else down 2. Answers the last Sprite_Current read.
unsigned char* DebrisLoop(U base, unsigned count) {
    unsigned char* s = nullptr;
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const d = At(base + 0x2C * i);
        SH_AT(void (__cdecl*)(unsigned char*), at::kDebrisDraw)(d);
        s = Cur();
        SetWord(d + 0x24, Word(d + 0x24) + 0x10u);
        SetWord(d + 0x2A, Word(d + 0x2A) + (s[9] < 4 ? 0x20u : 0xFFFEu));
    }
    return s;
}

// original 0x46EBA0: a draw mode (DebrisMode, dtd 0) committed;
// EffectGte_LoadMapCamera; the sixteen debris drawn (DebrisLoop).
extern "C" void __cdecl EffectKind1E_DebrisDraw(void) {
    DebrisMode(0);
    SH_CALL(EffectGte_LoadMapCamera)();
    DebrisLoop(at::kDebris, 16);
}

// original 0x46EC20: the model's lines. Sprite_Current kept, set to
// Sprite_ObjectsExtra[0] and put back at the end; Gte_PushMatrix; the face count
// (a signed byte at Sprite_ObjectsExtra[0] +0x54's pointer) read; that sprite's
// matrix (Sprite_ObjectMatrix) loaded (Gte_SetRotMatrix, Gte_SetTransMatrix)
// and a copy of it given to Camera_LoadMatrix; a draw mode (DebrisMode, dtd 0);
// then for each face (0x28 bytes from +0x50's pointer, read after those calls;
// its four vertices at +2..+0x19): a LINE_F4 at Gfx_PacketNext,
// semi-transparent, the colour the shade byte; the vertices projected
// (Gte_RotTransPers4 to +8, +0x14, +0x2C, +0x20); drawn - the depths
// (Gte_StoreDepthF4 to +0x10, +0x1C, +0x34, +0x28) and Gfx_CommitPrim(1, 0x38)
// - when EffectKind1E_Winding(+8, +0x14, +0x20) or (+0x20, +0x2C, +8) is above
// 0; Gte_PopMatrix. A negative count byte is 0xFF80.. faces in the original (a
// read far past the model): ours aborts.
extern "C" void __cdecl EffectKind1E_DrawModel(unsigned shade) {
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = Sprite_ObjectsExtra;
    SH_CALL(Gte_PushMatrix)();
    Matrix m1;
    const auto count = static_cast<signed char>(At(UL(At(at::kModelCount)))[0]);
    SH_CALL(Sprite_ObjectMatrix)(m1.m);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m1));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m1));
    Matrix m2;
    std::memcpy(&m2, &m1, sizeof m2);
    SH_CALL(Camera_LoadMatrix)(m2.m);
    DebrisMode(0);
    U face = UL(At(at::kModelFaces));
    if (count < 0)
        bof3::Fatal("EffectKind1E_DrawModel: the face count byte is %d; the original draws 0x%X faces past the model",
                    count, (unsigned)(static_cast<unsigned short>(count)));
    for (int n = count; n > 0; --n) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetLineF4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        prim[4] = static_cast<unsigned char>(shade);
        prim[5] = static_cast<unsigned char>(shade);
        prim[6] = static_cast<unsigned char>(shade);
        short v[4][4];
        for (U k = 0; k < 4; ++k)
            for (U j = 0; j < 3; ++j) v[k][j] = SW(At(face + 2 + 6 * k + 2 * j));
        long p;
        SH_CALL(Gte_RotTransPers4)(v[0], v[1], v[2], v[3], reinterpret_cast<float*>(prim + 8),
                                   reinterpret_cast<float*>(prim + 0x14), reinterpret_cast<float*>(prim + 0x2C),
                                   reinterpret_cast<float*>(prim + 0x20), &p);
        face += 0x28;
        if (SH_CALL(EffectKind1E_Winding)(prim + 8, prim + 0x14, prim + 0x20) > 0 ||
            SH_CALL(EffectKind1E_Winding)(prim + 0x20, prim + 0x2C, prim + 8) > 0) {
            SH_CALL(Gte_StoreDepthF4)(reinterpret_cast<float*>(prim + 0x10), reinterpret_cast<float*>(prim + 0x1C),
                                      reinterpret_cast<float*>(prim + 0x34), reinterpret_cast<float*>(prim + 0x28));
            SH_CALL(Gfx_CommitPrim)(1, 0x38);
        }
    }
    SH_CALL(Gte_PopMatrix)();
    Sprite_Current = saved;
}

// original 0x46EE20: (b.x - a.x) * (c.y - b.y) - (c.x - b.x) * (b.y - a.y) of
// three pairs of s16, to 16 bits - the PlayStation's screen-space winding
// test. The original's upper 16 bits of eax are its stack's leftovers; its
// caller reads ax. On the PC the pairs it is handed are the float screen x
// of LINE_F4 vertices (docs/effect_1c.md section 7).
extern "C" short __cdecl EffectKind1E_Winding(const unsigned char* a, const unsigned char* b, const unsigned char* c) {
    const U w1 = Word(b) - Word(a), w2 = Word(b + 2) - Word(a + 2);
    const U w3 = Word(c) - Word(b), w4 = Word(c + 2) - Word(b + 2);
    return static_cast<short>(static_cast<std::uint16_t>(w4 * w1 - w3 * w2));
}

// ===========================================================================
// Kind 0x1F: Effect_KindHandlers[0x1F], EffectKind1F_States by +1
// ===========================================================================

// original 0x46EE90: a tail jump through EffectKind1F_States 0x65425C by +1
// (three: _Start, _Debris, Effect_StateRelease).
extern "C" void __cdecl EffectKind1F_Run(void) { Entry(AddressOf(EffectKind1F_States), Cur()[1], 3, "EffectKind1F_Run")(); }

// original 0x46EEB0 (EffectKind1F_States[0]): 32 debris records of 0x2C at
// EffectKind30_Shards set up (E3C's 0x4851E0); +9 0, +1 up.
extern "C" void __cdecl EffectKind1F_Start(void) {
    for (U i = 0; i < 32; ++i) SH_AT(void (__cdecl*)(unsigned char*), at::kDebrisInit)(At(Shards() + 0x2C * i));
    Cur()[9] = 0;
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46EEE0 (EffectKind1F_States[1]): a draw mode committed,
// EffectGte_LoadMapCamera, the 32 debris drawn (DebrisLoop); the last
// Sprite_Current read's +9 up 1; +9 (read again) above 0x44, +1 up.
extern "C" void __cdecl EffectKind1F_Debris(void) {
    DebrisMode(0);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const last = DebrisLoop(Shards(), 32);
    last[9] = static_cast<unsigned char>(last[9] + 1);
    unsigned char* const s = Cur();
    if (s[9] > 0x44) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// ===========================================================================
// Kind 0x20: Effect_KindHandlers[0x20], EffectKind20_States by +1
// ===========================================================================

// original 0x46EF70: a CALL through EffectKind20_States 0x654268 by +1 (seven
// entries), then - +1 (read again) not 0 - a tail jump to EffectKind20_Draw.
extern "C" void __cdecl EffectKind20_Run(void) {
    Entry(AddressOf(EffectKind20_States), Cur()[1], 7, "EffectKind20_Run")();
    if (Cur()[1] != 0) SH_CALL(EffectKind20_Draw)();
}

// original 0x46EFA0 (EffectKind20_States[0]): the timer word +0x2C 0x20; the
// dwords +0x64 (width), +0x68 (height), +0x6C (angle) 0; +1 up.
extern "C" void __cdecl EffectKind20_Start(void) {
    SetWord(Cur() + 0x2C, 0x20);
    SetUL(Cur() + 0x64, 0);
    SetUL(Cur() + 0x68, 0);
    SetUL(Cur() + 0x6C, 0);
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// The timer word +0x2C down 1; at 0, `reload` and +1 up.
void Tick(unsigned reload) {
    unsigned char* s = Cur();
    SetWord(s + 0x2C, Word(s + 0x2C) - 1u);
    s = Cur();
    if (Word(s + 0x2C) != 0) return;
    SetWord(s + 0x2C, reload);
    s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46EFE0 (EffectKind20_States[1]): +0x64 and +0x68 up 4; Tick(0x10).
extern "C" void __cdecl EffectKind20_Grow(void) {
    SetUL(Cur() + 0x64, UL(Cur() + 0x64) + 4);
    SetUL(Cur() + 0x68, UL(Cur() + 0x68) + 4);
    Tick(0x10);
}

// original 0x46F030 (EffectKind20_States[2]): Tick(0xC).
extern "C" void __cdecl EffectKind20_Wait(void) { Tick(0xC); }

// original 0x46F060 (EffectKind20_States[3]): the height +0x3C up 0x40 (<<
// 16), +0x64 down 6, +0x68 up 6; Tick(0x18).
extern "C" void __cdecl EffectKind20_Rise(void) {
    SetUL(Cur() + 0x3C, UL(Cur() + 0x3C) + 0x400000);
    SetUL(Cur() + 0x64, UL(Cur() + 0x64) - 6);
    SetUL(Cur() + 0x68, UL(Cur() + 0x68) + 6);
    Tick(0x18);
}

// original 0x46F0C0 (EffectKind20_States[4]): +0x64 up 3, +0x68 down 3;
// Tick(0x10).
extern "C" void __cdecl EffectKind20_Shrink(void) {
    SetUL(Cur() + 0x64, UL(Cur() + 0x64) + 3);
    SetUL(Cur() + 0x68, UL(Cur() + 0x68) - 3);
    Tick(0x10);
}

// original 0x46F100 (EffectKind20_States[5]): the timer down 1; at 0: the
// point (+0x34, +0x38, +0x3C) projected and again with its z (+0x38) one unit
// (0x10000) further (EffectGte_LoadMapCamera first); +0x6C the angle
// Math_Ratan2(dy, dx) of the second screen point less the first (each a float
// difference at the x87's 53 bits, stored as a float) + 0x400; the timer 0x1E,
// the fall speed +0x10 0, +1 up.
extern "C" void __cdecl EffectKind20_Aim(void) {
    unsigned char* s = Cur();
    SetWord(s + 0x2C, Word(s + 0x2C) - 1u);
    if (Word(Cur() + 0x2C) != 0) return;
    SH_CALL(EffectGte_LoadMapCamera)();
    s = Cur();
    long point[3] = {Long(s + 0x34), Long(s + 0x38), Long(s + 0x3C)};
    float near[3], far[3];
    SH_CALL(EffectGte_ProjectPoint)(point, near);
    point[1] = static_cast<long>(static_cast<U>(point[1]) + 0x10000u);
    SH_CALL(EffectGte_ProjectPoint)(point, far);
    const auto dx = static_cast<float>(static_cast<double>(far[0]) - static_cast<double>(near[0]));
    const auto dy = static_cast<float>(static_cast<double>(far[1]) - static_cast<double>(near[1]));
    const int angle = SH_CALL(Math_Ratan2)(dy, dx);
    SetUL(Cur() + 0x6C, static_cast<U>(angle) + 0x400u);
    SetWord(Cur() + 0x2C, 0x1E);
    SetUL(Cur() + 0x10, 0);
    s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46F1C0 (EffectKind20_States[6]): the fall speed +0x10 up 0x2000
// and added to z +0x38; +0x64 down 6, not below 8; +0x68 up 6; the timer down
// 1 - at 0, Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind20_Fall(void) {
    SetUL(Cur() + 0x10, UL(Cur() + 0x10) + 0x2000);
    unsigned char* s = Cur();
    SetUL(s + 0x38, UL(s + 0x38) + UL(s + 0x10));
    SetUL(Cur() + 0x64, UL(Cur() + 0x64) - 6);
    s = Cur();
    if (Long(s + 0x64) < 8) SetUL(s + 0x64, 8);
    SetUL(Cur() + 0x68, UL(Cur() + 0x68) + 6);
    s = Cur();
    SetWord(s + 0x2C, Word(s + 0x2C) - 1u);
    if (Word(Cur() + 0x2C) == 0) SH_CALL(Effect_Release)();
}

// original 0x46F230 (reached only by EffectKind20_Run's tail jump; the code no
// list has in 0x46F1C0's span): a draw mode (DebrisMode, dtd 1) committed;
// E4F's 0x493090(the point +0x34 / +0x38 / +0x3C copied, the words +0x64,
// +0x68, +0x6C, 0xC0, 0) - each word pushed with the high half the register
// held, as the original's: +0x38's, +0x3C's and +0x38's high words.
extern "C" void __cdecl EffectKind20_Draw(void) {
    DebrisMode(1);
    const unsigned char* const s = Cur();
    const long point[3] = {Long(s + 0x34), Long(s + 0x38), Long(s + 0x3C)};
    const U width = (UL(s + 0x38) & 0xFFFF0000u) | Word(s + 0x64);
    const U height = (UL(s + 0x3C) & 0xFFFF0000u) | Word(s + 0x68);
    const U angle = (UL(s + 0x38) & 0xFFFF0000u) | Word(s + 0x6C);
    SH_AT(void (__cdecl*)(const long*, U, U, U, U, U), at::kCone)(point, width, height, angle, 0xC0, 0);
}

void Effect1C_Inject() {
    if (bof3::WantsShadow("effect_1c")) effect_1c::SelfTest();
    BOF3_INJECT(EffectKind36_Run);
    BOF3_INJECT(EffectKind3C_Run);
    BOF3_INJECT(EffectKind70_Run);
    BOF3_INJECT(EffectKind1C_Run);
    BOF3_INJECT(EffectKind1C_Start);
    BOF3_INJECT(EffectKind1C_Rise);
    BOF3_INJECT(EffectKind1C_Hold);
    BOF3_INJECT(EffectKind1C_Fade);
    BOF3_INJECT(EffectKind1C_End);
    BOF3_INJECT(EffectKind1C_DrawRing);
    BOF3_INJECT(EffectKind1C_MoveShards);
    BOF3_INJECT(EffectKind1D_Run);
    BOF3_INJECT(EffectKind1D_Start);
    BOF3_INJECT(EffectKind1D_Rise);
    BOF3_INJECT(EffectKind1D_Hold);
    BOF3_INJECT(EffectKind1D_Fade);
    BOF3_INJECT(EffectKind1D_End);
    BOF3_INJECT(EffectKind1D_DrawRing);
    BOF3_INJECT(EffectShards_Clear);
    BOF3_INJECT(EffectKind1D_MoveShards);
    BOF3_INJECT(EffectKind1E_Run);
    BOF3_INJECT(EffectKind1E_Start);
    BOF3_INJECT(EffectKind1E_Glow);
    BOF3_INJECT(EffectKind1E_Burst);
    BOF3_INJECT(EffectKind1E_FreeModel);
    BOF3_INJECT(EffectKind1E_ShardsInit);
    BOF3_INJECT(EffectKind1E_ShardInit);
    BOF3_INJECT(EffectKind1E_ShardsDraw);
    BOF3_INJECT(EffectKind1E_ShardQuad);
    BOF3_INJECT(EffectKind1E_ShardFly);
    BOF3_INJECT(EffectKind1E_ShardFade);
    BOF3_INJECT(EffectKind1E_ShardNext2);
    BOF3_INJECT(EffectKind1E_ShardNext3);
    BOF3_INJECT(EffectKind1E_SplitModel);
    BOF3_INJECT(EffectKind1E_StepPieces);
    BOF3_INJECT(EffectKind1E_TurnPiece);
    BOF3_INJECT(EffectKind1E_DebrisInit);
    BOF3_INJECT(EffectKind1E_DebrisInitOne);
    BOF3_INJECT(EffectKind1E_DebrisDraw);
    BOF3_INJECT(EffectKind1E_DrawModel);
    BOF3_INJECT(EffectKind1E_Winding);
    BOF3_INJECT(EffectKind1F_Run);
    BOF3_INJECT(EffectKind1F_Start);
    BOF3_INJECT(EffectKind1F_Debris);
    BOF3_INJECT(EffectKind20_Run);
    BOF3_INJECT(EffectKind20_Start);
    BOF3_INJECT(EffectKind20_Grow);
    BOF3_INJECT(EffectKind20_Wait);
    BOF3_INJECT(EffectKind20_Rise);
    BOF3_INJECT(EffectKind20_Shrink);
    BOF3_INJECT(EffectKind20_Aim);
    BOF3_INJECT(EffectKind20_Fall);
    BOF3_INJECT(EffectKind20_Draw);
}
