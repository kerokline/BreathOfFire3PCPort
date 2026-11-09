// Moved here by tools/move_funcs.py from effect_1c.cpp, rest_3e.cpp (docs/source-layout-round.md).
#include "game/effect/effect_shards.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_shards_callees.h"
#include "game/effect_1c.h"
#include "game/effect_1c_callees.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3e.h"
#include "game/rest_3e_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/exe_image.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_shards::at;

}  // namespace

namespace {

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

U Key(const void* p) { return Low32(p); }

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

// jmp / call [table + 4 * index]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original goes through the dword after - the next kind's table or data.
Handler Entry(U table, unsigned index, unsigned entries, const char* who) {
    if (index >= entries)
        bof3::Fatal("%s: index %u past the %u entries of the table at 0x%X (the original jumps through 0x%X)", who,
                    index, entries, (unsigned)table, (unsigned)(table + 4u * index));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4u * index))));
}

// The draw mode of the debris, the model and kind 0x20's fan: the page at
// (0x380, 0x100), then committed at slot 1.
void DebrisMode(int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
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

// The debris loop of kind 0x1E (16 at 0x92C780) and kind 0x1F (32 at
// 0x92BF80): each drawn (E3C's 0x485030), its angle +0x24 up 0x10, its shade
// +0x2A up 0x20 while the record's +9 (Sprite_Current read after the draw) is
// below 4, else down 2. Answers the last Sprite_Current read.
unsigned char* DebrisLoop(U base, unsigned count) {
    unsigned char* s = nullptr;
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const d = At(base + 0x2C * i);
        SH_AT(void (__cdecl*)(unsigned char*), at::kDebrisDraw.v)(d);
        s = Cur();
        SetWord(d + 0x24, Word(d + 0x24) + 0x10u);
        SetWord(d + 0x2A, Word(d + 0x2A) + (s[9] < 4 ? 0x20u : 0xFFFEu));
    }
    return s;
}

namespace {

U ULRest3e(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U ULRest3e(U a) { return ULRest3e(move_script::At(a)); }

void Copy12Rest3e(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, ULRest3e(from + i));
}

const long* Point(const unsigned char* p) { return reinterpret_cast<const long*>(p); }

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

}  // namespace

// original 0x46E120: every speck of EffectKind30_Shards' 0x80 (0x14 apart)
// freed (+0 = 0). Kind 0x1C's and 0x1D's starts and EffectKind2B_Rise.
extern "C" void __cdecl EffectShards_Clear(void) {
    for (U r = 0; r < at::kShardCount; ++r) At(Shards() + r * at::kShardStride)[0] = 0;
}

// original 0x46E140: kind 0x1D's specks moved and drawn by 0x46E190 (the same
// TILE_1, committed at slot 1); al 1 when any was in use.
extern "C" unsigned char __cdecl EffectKind1D_MoveShards(void) { return MoveShards(at::kShardTile2.v); }

// original 0x46E190 (E1C's EffectKind1D_MoveShards calls it; PSX twin
// 0x801F752C): the speck's point +4 projected (EffectGte_ProjectPoint into a
// stack vector); a TILE_1 at the packet cursor, opaque, at it, white on an odd
// Rand, else black; committed to slot 1 (0x14). E2A's EffectSpecks_Draw links
// the same tile instead.
extern "C" void __cdecl EffectKind1D_DrawSpeck(const unsigned char* speck) {
    alignas(4) unsigned char screen[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(speck + 4), Out(screen));
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    Copy12Rest3e(p + 8, screen);
    const auto c = static_cast<unsigned char>((SH_CALL(Rand)() & 1) ? 0xFF : 0);
    p[6] = c;
    p[5] = c;
    p[4] = c;
    SH_CALL(Gfx_CommitPrim)(1, 0x14);
}

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
    Sprite_ObjectsExtra[0].flags = 0;
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
    SetUL(shard + 4, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    SetUL(shard + 8, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
    SetUL(shard + 0xC, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height));
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
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->clut, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->tpage, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
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
    U src = Sprite_ObjectsExtra[0].script;
    for (U i = 0; i < at::kPieceCount; ++i) {
        unsigned char* const d = At(::bof3::Img(EffectKind30_Shards_addr) + 0x3c8 + 0x28 * i);
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + 0x140 + 0x18 * i);
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
    const U model = Sprite_ObjectsExtra[0].script;
    for (U i = 0; i < at::kPieceCount; ++i) {
        const U odd = Frame_Counter & 1;
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + 0x140 + 0x18 * i);
        SetWord(r + 0, Word(r + 0) + Word(r + 8));
        SetWord(r + 2, Word(r + 2) + Word(r + 0xA));
        SetWord(r + 0xC, Word(r + 0xC) + odd);
        SetWord(r + 4, Word(r + 4) + Word(r + 0xC));
        SH_CALL(EffectKind1E_TurnPiece)(At(::bof3::Img(EffectKind30_Shards_addr) + 0x3c8 + 0x28 * i), At(model + 0x28 * i), r);
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
    for (U i = 0; i < 16; ++i) SH_CALL(EffectKind1E_DebrisInitOne)(At(::bof3::Img(at::kDebris.v) + 0x2C * i));
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
    SetUL(debris + 0, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    SetUL(debris + 4, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
    SetUL(debris + 8, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height));
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
    SH_AT(Turn, at::kMatrixVector.v)(m.m, reinterpret_cast<const short*>(debris + 0x10), reinterpret_cast<short*>(debris + 0x10));
    SH_AT(Turn, at::kMatrixVector.v)(m.m, reinterpret_cast<const short*>(debris + 0x18), reinterpret_cast<short*>(debris + 0x18));
    SH_CALL(Gte_PopMatrix)();
    SetWord(debris + 0x28, (static_cast<U>(SH_CALL(Rand)()) & 3u) + 10u);
    SetWord(debris + 0x20, 0);
    SetWord(debris + 0x22, 0);
    SetWord(debris + 0x24, 0);
    SetWord(debris + 0x2A, 0);
}

// original 0x46EBA0: a draw mode (DebrisMode, dtd 0) committed;
// EffectGte_LoadMapCamera; the sixteen debris drawn (DebrisLoop).
extern "C" void __cdecl EffectKind1E_DebrisDraw(void) {
    DebrisMode(0);
    SH_CALL(EffectGte_LoadMapCamera)();
    DebrisLoop(::bof3::Img(at::kDebris.v), 16);
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
    Sprite_Current = rv::Bytes(Sprite_ObjectsExtra);
    SH_CALL(Gte_PushMatrix)();
    Matrix m1;
    const auto count = static_cast<signed char>(At(Sprite_ObjectsExtra[0].frames)[0]);
    SH_CALL(Sprite_ObjectMatrix)(m1.m);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m1));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m1));
    Matrix m2;
    std::memcpy(&m2, &m1, sizeof m2);
    SH_CALL(Camera_LoadMatrix)(m2.m);
    DebrisMode(0);
    U face = Sprite_ObjectsExtra[0].script;
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

// original 0x46EE90: a tail jump through EffectKind1F_States 0x65425C by +1
// (three: _Start, _Debris, Effect_StateRelease).
extern "C" void __cdecl EffectKind1F_Run(void) { Entry(AddressOf(EffectKind1F_States), Cur()[1], 3, "EffectKind1F_Run")(); }

// original 0x46EEB0 (EffectKind1F_States[0]): 32 debris records of 0x2C at
// EffectKind30_Shards set up (E3C's 0x4851E0); +9 0, +1 up.
extern "C" void __cdecl EffectKind1F_Start(void) {
    for (U i = 0; i < 32; ++i) SH_AT(void (__cdecl*)(unsigned char*), at::kDebrisInit.v)(At(Shards() + 0x2C * i));
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

void EffectShards_InjectFromEffect1c() {
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
}

void EffectShards_InjectFromRest3e() {
    BOF3_INJECT(EffectKind1D_DrawSpeck);
}
