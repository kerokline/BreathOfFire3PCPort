// Moved here by tools/move_funcs.py from effect_2b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_35.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2b.h"
#include "game/effect_2b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2b::at;

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

void StoreFloat(unsigned char* at, double v) {
    const auto f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

// Kind 0x35's shard cursor, EffectKind35_ShardCursor 0x676108.
unsigned char* Cursor() { return EffectKind35_ShardCursor; }

void SetCursor(U v) { EffectKind35_ShardCursor = At(v); }

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

}  // namespace

// original 0x4737A0 (Effect_KindHandlers[0x35], hidden in 0x473600's recorded
// extent): a tail jump through EffectKind35_States 0x654410 by +1.
extern "C" void __cdecl EffectKind35_Run(void) {
    Entry(AddressOf(EffectKind35_States), Cur()[1], 3, "EffectKind35_Run")();
}

// original 0x4737C0 (EffectKind35_States[0]): the sixteen shards set up, the
// model split; +1 up.
extern "C" void __cdecl EffectKind35_Start(void) {
    SH_CALL(EffectKind35_ShardsInit)();
    SH_CALL(EffectKind35_SplitModel)();
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4737E0 (EffectKind35_States[1]): the pieces stepped, the shards
// stepped and drawn; none in use, +1 up.
extern "C" void __cdecl EffectKind35_Burst(void) {
    SH_CALL(EffectKind35_StepPieces)();
    if (SH_CALL(EffectKind35_ShardsDraw)()) return;
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x473800 (EffectKind35_States[2]): Sprite_ObjectsExtra[1]'s +0 = 0
// (the model's record freed); Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind35_FreeModel(void) {
    Sprite_ObjectsExtra[1].flags = 0;
    SH_CALL(Effect_Release)();
}

// original 0x473810: EffectKind1E_ShardsInit with sixteen: the cursor 0x676108
// set to EffectKind30_Shards, then sixteen times EffectKind1E_ShardInit(the
// cursor) (E1C's, ours) and the cursor up 0x28 (read again after the call).
extern "C" void __cdecl EffectKind35_ShardsInit(void) {
    SetCursor(::bof3::Img(EffectKind30_Shards_addr));
    for (U i = 0; i < at::kShard35Count; ++i) {
        SH_CALL(EffectKind1E_ShardInit)(Cursor());
        SetCursor(Key(Cursor()) + 0x28);
    }
}

// original 0x473850: EffectKind1E_ShardsDraw with sixteen: the map camera
// loaded; the cursor walked over the shards: each in use stepped by
// EffectKind35_ShardStates[+1] (a call, the handler reading the cursor) and
// drawn (EffectKind35_ShardQuad of the cursor, read again after each call); al
// 1 when any was in use.
extern "C" unsigned char __cdecl EffectKind35_ShardsDraw(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    U c = ::bof3::Img(EffectKind30_Shards_addr);
    SetCursor(c);
    unsigned char any = 0;
    for (U i = 0; i < at::kShard35Count; ++i) {
        if (At(c)[0]) {
            Entry(AddressOf(EffectKind35_ShardStates), At(c)[1], 2, "EffectKind35_ShardsDraw")();
            SH_CALL(EffectKind35_ShardQuad)(Cursor());
            c = Key(Cursor());
            any = 1;
        }
        c += 0x28;
        SetCursor(c);
    }
    return any;
}

// original 0x4738A0: EffectKind1E_ShardQuad with the page's abr 2: a shard's
// POLY_FT4 at Gfx_PacketNext (read once), semi-transparent; the point (+4)
// projected, the size (+0x24, s16, both axes) scaled at its depth in place;
// the corners x - (w >> 1) and + w, y - (h >> 1) and + h at the x87's 53 bits,
// rounded once to a float; the depth at +0x10 +0x20 +0x30 +0x40; u 0xE0 /
// 0xFF, v 0x30 / 0x4F; the CLUT Gpu_GetClut(0xA0, 0x1E3), the page
// Gpu_GetTPage(0, 2, 0x2C0, 0x100); the shade +3; MapView_LinkPrimAt(x, z of
// the point, 2, 0x48).
extern "C" void __cdecl EffectKind35_ShardQuad(unsigned char* shard) {
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
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->tpage, SH_CALL(Gpu_GetTPage)(0, 2, 0x2C0, 0x100));
    prim[4] = shard[3];
    prim[5] = shard[3];
    prim[6] = shard[3];
    SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(shard + 4)), static_cast<unsigned long>(Long(shard + 8)),
                                2, 0x48);
}

// original 0x473A90 (EffectKind35_ShardStates[0]): EffectKind1E_ShardFly on
// the cursor 0x676108: the shard moved by twice its direction, its size +0x24
// up 0x80, its count +2 down - at 0, +2 0x40 and its state +1 up. The cursor
// read again for every access.
extern "C" void __cdecl EffectKind35_ShardFly(void) {
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

// original 0x473B00 (EffectKind35_ShardStates[1]): EffectKind1E_ShardFade on
// the cursor 0x676108: moved by its direction, its shade +3 and count +2 down
// 1 - the count at 0, the shard freed (+0 = 0).
extern "C" void __cdecl EffectKind35_ShardFade(void) {
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

// original 0x473B60: EffectKind1E_SplitModel with 55 faces: the model
// (Sprite_ObjectsExtra[1] +0x50, read once) split: each face's 0x28 bytes
// copied (dword by dword, forward) to 0x92C728 + 0x28 i; its centre - the four
// vertices' (+2, +8, +0xE, +0x14: x, y, z s16 each) sums >> 2, 16 bits each -
// at the piece record 0x92C200 + 0x18 i; the centre as three longs normalised
// (Gte_VectorNormalS) into +8..+0xD and each s16 there >> 7 (kind 0x1E's >> 8);
// +0x10..+0x15 0; then the copy's twelve vertex words made relative to the
// centre.
extern "C" void __cdecl EffectKind35_SplitModel(void) {
    U src = Sprite_ObjectsExtra[1].script;
    for (U i = 0; i < at::kPiece35Count; ++i) {
        unsigned char* const d = At(::bof3::Img(at::kCopies35.v) + 0x28 * i);
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + 0x280 + 0x18 * i);
        for (U j = 0; j < 0x28; j += 4) SetUL(d + j, UL(At(src + j)));
        SetWord(r + 0, static_cast<U>((SW(d + 8) + SW(d + 0x14) + SW(d + 0xE) + SW(d + 2)) >> 2));
        SetWord(r + 2, static_cast<U>((SW(d + 4) + SW(d + 0x16) + SW(d + 0xA) + SW(d + 0x10)) >> 2));
        const int z = (SW(d + 0x18) + SW(d + 6) + SW(d + 0x12) + SW(d + 0xC)) >> 2;
        SetWord(r + 4, static_cast<U>(z));
        const long centre[3] = {SW(r + 0), SW(r + 2), static_cast<short>(z)};
        SH_CALL(Gte_VectorNormalS)(centre, reinterpret_cast<short*>(r + 8));
        for (U k = 8; k <= 0xC; k += 2) SetWord(r + k, static_cast<U>(SW(r + k) >> 7));
        SetWord(r + 0x10, 0);
        SetWord(r + 0x12, 0);
        SetWord(r + 0x14, 0);
        for (U v = 2; v <= 0x14; v += 6)
            for (U k = 0; k < 3; ++k) SetWord(d + v + 2 * k, Word(d + v + 2 * k) - Word(r + 2 * k));
        src += 0x28;
    }
}

// original 0x473C80: each of the 55 pieces stepped - its z velocity +0xC up 1
// (kind 0x1E's by the frame's parity), its centre x, z, y by its velocity
// (+8, +0xC, +0xA) - and turned (EffectKind35_TurnPiece(the copy, the model's
// face, the piece)); the model's faces from Sprite_ObjectsExtra[1] +0x50 read
// once.
extern "C" void __cdecl EffectKind35_StepPieces(void) {
    const U model = Sprite_ObjectsExtra[1].script;
    for (U i = 0; i < at::kPiece35Count; ++i) {
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + 0x280 + 0x18 * i);
        const U vx = Word(r + 8);
        SetWord(r + 0xC, Word(r + 0xC) + 1u);
        SetWord(r + 0, Word(r + 0) + vx);
        const U vz = Word(r + 0xC), vy = Word(r + 0xA);
        SetWord(r + 4, Word(r + 4) + vz);
        SetWord(r + 2, Word(r + 2) + vy);
        SH_CALL(EffectKind35_TurnPiece)(At(::bof3::Img(at::kCopies35.v) + 0x28 * i), At(model + 0x28 * i), r);
    }
}

// original 0x473CE0: EffectKind1E_TurnPiece as a loop: a piece's angles
// +0x10..+0x14 each Rand & 0xFC0; a rotation of them (Gte_RotMatrix), no
// translation, loaded (Gte_SetTransMatrix, Gte_SetRotMatrix); each of the
// copy's four relative vertices (+2, +8, +0xE, +0x14) turned (Gte_RotTrans)
// and written to the same place of the face plus the piece's centre (16 bits;
// the centre read after each call). The original hands Gte_RotTrans its third
// argument's slot as the flag; ours has no flag argument.
extern "C" void __cdecl EffectKind35_TurnPiece(const unsigned char* copy, unsigned char* face, unsigned char* piece) {
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
        SetWord(face + v + 2, static_cast<U>(out[1]) + Word(piece + 2));
        SetWord(face + v + 4, static_cast<U>(out[2]) + Word(piece + 4));
    }
}

void EffectKind35_Inject() {
    BOF3_INJECT(EffectKind35_Run);
    BOF3_INJECT(EffectKind35_Start);
    BOF3_INJECT(EffectKind35_Burst);
    BOF3_INJECT(EffectKind35_FreeModel);
    BOF3_INJECT(EffectKind35_ShardsInit);
    BOF3_INJECT(EffectKind35_ShardsDraw);
    BOF3_INJECT(EffectKind35_ShardQuad);
    BOF3_INJECT(EffectKind35_ShardFly);
    BOF3_INJECT(EffectKind35_ShardFade);
    BOF3_INJECT(EffectKind35_SplitModel);
    BOF3_INJECT(EffectKind35_StepPieces);
    BOF3_INJECT(EffectKind35_TurnPiece);
}
