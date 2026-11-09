// Moved here by tools/move_funcs.py from effect_1c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_1c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1c.h"
#include "game/effect_1c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1c::at;

}  // namespace

namespace {

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

// original 0x46D890: a tail jump through EffectKind1C_States 0x654210 by +1.
extern "C" void __cdecl EffectKind1C_Run(void) { Entry(AddressOf(EffectKind1C_States), Cur()[1], 5, "EffectKind1C_Run")(); }

// original 0x46D8B0 (EffectKind1C_States[0]): the point set to (0x600000,
// 0x110000) and its height +0x3C the ground there (AreaMap_Elevation, s16 <<
// 16); the ring's radius +0x2E 0x180 and height +0x30 0x500; +9 0; the specks
// cleared (EffectShards_Clear); +1 up; Sound_PlayEffect(0x202).
extern "C" void __cdecl EffectKind1C_Start(void) {
    rv::Current<records::EffectObject>()->x = 0x600000;
    rv::Current<records::EffectObject>()->z = 0x110000;
    unsigned char* s = Cur();
    const long ground = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z);
    rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<U>(static_cast<int>(static_cast<short>(ground))) << 16);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(Cur())->work_2e, 0x180);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(Cur())->work_30, 0x500);
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
    if (!(Frame_Counter & 1)) SH_AT(unsigned char (__cdecl*)(void), at::kShardSpawn.v)();
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
    if (!(Frame_Counter & 1)) SH_AT(unsigned char (__cdecl*)(void), at::kShardSpawn.v)();
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
extern "C" unsigned char __cdecl EffectKind1C_MoveShards(void) { return MoveShards(at::kShardTile.v); }

void EffectKind1c_Inject() {
    BOF3_INJECT(EffectKind1C_Run);
    BOF3_INJECT(EffectKind1C_Start);
    BOF3_INJECT(EffectKind1C_Rise);
    BOF3_INJECT(EffectKind1C_Hold);
    BOF3_INJECT(EffectKind1C_Fade);
    BOF3_INJECT(EffectKind1C_End);
    BOF3_INJECT(EffectKind1C_DrawRing);
    BOF3_INJECT(EffectKind1C_MoveShards);
}
