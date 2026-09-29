// The field core's second half - round twelve, group FC2, the band
// 0x46BBF0..0x46D5ED of the cut (analysis/round12_cut.tsv), taken with the
// scenario harness in field mode (scenario_harness.h, docs/scenario_harness.md
// section 7). Four kinds of the Effect_Objects pool (Effect_RunObjects makes a
// record Sprite_Current and calls Effect_KindHandlers[+5]):
//
//   kind 0x30  states 1..3 of FC1's 0x46BB30 and their helpers: slide a cell
//              in the leader's facing, claiming the cells it rests on; when
//              the way is blocked, shatter - 24 faces of its model tumble and
//              eight sparks fly (0x92BF80 / 0x92C4C0);
//   kind 0x34  EffectKind34_Run by +1 over five variants (and FC1's
//              0x46A310), each a small state table by +2: bursts, debris,
//              falling pieces;
//   kind 0x3A  EffectKind3A_Run by +1: thrown ahead of the leader, it clears
//              the 0xFD cells it hits (FE2's 0x5728D0) and now and then finds
//              zenny (FE1's 0x5307C0);
//   kind 0x41  EffectKind41_Run by +1: the number +6 drawn over party member
//              +0xB, bouncing and blinking out.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, or writes a record past its pool on an answer the callee
// never gives, ours aborts with a message (docs/field_c2.md section 2).
#include "game/field_c2.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_c2_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_c2::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
short SW(const unsigned char* p) { return static_cast<short>(Word(p)); }
unsigned char* PtrAt(const unsigned char* cell) { return At(UL(cell)); }
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void StoreFloat(unsigned char* at, double v) {
    const auto f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, read in
// place (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original jumps through the dword after - the next table's entry or data.
void Run(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/field_c2.md section 2)",
                    who, at, state, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// Effect_FindFree's record: a Fatal on an answer past the twenty (the callee
// answers 0xFF or 0..19; the original would write past the pool).
unsigned char* EffectRecord(const char* who, unsigned index) {
    if (index >= at::kEffects)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes past the pool "
                    "(docs/field_c2.md section 2)",
                    who, index);
    return Effect_Objects + index * at::kEffectStride;
}

// A new kind-0x34 record at the running one's place: +0 1, +5 0x34 (the caller
// sets +1 / +2), x, z and the height dword copied from Sprite_Current as read
// before the stores' copy (the originals' order).
void CopyPlace(unsigned char* e, const unsigned char* s) {
    SetUL(e + 0x34, UL(s + 0x34));
    SetUL(e + 0x38, UL(s + 0x38));
    SetUL(e + 0x3C, UL(s + 0x3C));
}

// The object Sprite_ObjectAt names, +0x80 bit 0 set ("touched"): 0..0x1D a
// Sprite_Objects record, 0x1E.. a Sprite_ObjectsExtra one. `index` is the
// answer as the caller widened it; past the four extras (or, sign-extended,
// negative) the original writes outside both lists - a Fatal here.
void Touch(const char* who, int index) {
    if (index < 0 || index >= static_cast<int>(at::kObjects + at::kExtras))
        bof3::Fatal("%s: Sprite_ObjectAt answered %d, past the 30 objects and four extras - the original writes "
                    "outside them (docs/field_c2.md section 2)",
                    who, index);
    unsigned char* const o = index < static_cast<int>(at::kObjects)
                                 ? Sprite_Objects + static_cast<U>(index) * at::kObjectStride
                                 : Sprite_ObjectsExtra + static_cast<U>(index - static_cast<int>(at::kObjects)) * at::kObjectStride;
    o[0x80] = static_cast<unsigned char>(o[0x80] | 1);
}

// The cell coordinates the kind-0x30 helpers pass: the high words of x / z,
// one more (16-bit) for the neighbour.
unsigned CellX(const unsigned char* s, unsigned d) { return static_cast<std::uint16_t>(Word(s + 0x36) + d); }
unsigned CellZ(const unsigned char* s, unsigned d) { return static_cast<std::uint16_t>(Word(s + 0x3A) + d); }

// A PSX MATRIX as the originals lay it out on their stack.
struct Matrix {
    short m[9];
    short pad;
    std::int32_t t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 0x20 bytes");

// Kind 0x41's draw: 0x46D5F0(x, y + the bounce +0x3A, +6, +0x27) at party
// member +0xB's screen words +0x2E / +0x30 (the member byte unchecked, as the
// original: a data read, not a table index).
void DrawNumber(const unsigned char* s) {
    const unsigned char* const o = At(at::kLeader + s[0xB] * at::kMemberStride);
    SH_AT(void (__cdecl*)(unsigned, unsigned, unsigned, unsigned), at::kDrawNumber)(
        Word(o + 0x2E), static_cast<std::uint16_t>(Word(o + 0x30) + Word(s + 0x3A)), s[6], s[0x27]);
}

}  // namespace

// ===========================================================================
// Kind 0x30: FC1's EffectKind30 dispatcher 0x46BB30 by +1 through 0x653FE4
// (0x46BB50 FC1's, then these three)
// ===========================================================================

// original 0x46BBF0: kind 0x30's state 1, waiting to be pushed. Game_Mode 1:
// nothing but the draw. Else Field_Request 5: EffectKind30_FreeCells, then the
// draw. Else with +0xB set: +8 the leader's facing 0x802D48, the step +0xC /
// +0x10 twice the facing's unit (0x6696DC / 0x6696E0 rows, the byte
// unchecked), +9 16 frames; the cells freed; EffectKind30_WayBlocked(x + 16
// steps, z + 16 steps) (computed before the free) - clear: EffectKind30_Slide
// and +1 up (no draw); blocked: the model (the signed byte at *(+0x54), times
// 40 bytes, from *(+0x50) on, +0x50 stepped a byte at a time) copied into
// 0x8C5D80, +0x50 set to it, EffectKind30_ShardsInit, _SparksInit,
// Sound_PlayEffect(0x10E), +1 3, the draw. The draw: Sprite_UpdateScreenA
// unless Field_Request is 3. PSX 0x8019DCC0 (pairs_propagated call-disputed).
extern "C" void __cdecl EffectKind30_Push(void) {
    if (Game_Mode != 1) {
        if (Field_Request == 5) {
            SH_CALL(EffectKind30_FreeCells)();
        } else if (S()[0xB] != 0) {
            unsigned char* s = S();
            s[8] = At(at::kLeader)[8];
            s = S();
            SetUL(s + 0xC, UL(At(at::kUnitSteps + 8u * s[8])) << 1);
            s = S();
            SetUL(s + 0x10, UL(At(at::kUnitSteps + 4 + 8u * s[8])) << 1);
            S()[9] = 0x10;
            s = S();
            const U n = s[9];
            const U x = UL(s + 0xC) * n + UL(s + 0x34);
            const U z = UL(s + 0x10) * n + UL(s + 0x38);
            SH_CALL(EffectKind30_FreeCells)();
            if (SH_CALL(EffectKind30_WayBlocked)(static_cast<long>(x), static_cast<long>(z)) == 0) {
                SH_CALL(EffectKind30_Slide)();
                s = S();
                s[1] = static_cast<unsigned char>(s[1] + 1);
                return;
            }
            s = S();
            const int bytes = static_cast<signed char>(PtrAt(s + 0x54)[0]) * 40;
            unsigned char* dst = At(at::kModelCopy);
            for (int i = bytes; i > 0; --i) {
                s = S();
                *dst++ = *PtrAt(s + 0x50);
                SetUL(s + 0x50, UL(s + 0x50) + 1);
            }
            SetUL(S() + 0x50, at::kModelCopy);
            SH_CALL(EffectKind30_ShardsInit)();
            SH_CALL(EffectKind30_SparksInit)();
            SH_CALL(Sound_PlayEffect)(0x10E);
            S()[1] = 3;
        }
    }
    if (Field_Request != 3) SH_CALL(Sprite_UpdateScreenA)();
}

// original 0x46BD10: kind 0x30's state 2, sliding. +9 down; x / z moved by the
// step +0xC / +0x10. At +9 0: in area 0x54 story flag 0x36 set when it rests
// at exactly (0x118000, 0x4A8000), cleared anywhere else; EffectKind30_
// ClaimCells, +0xB 0, +1 back to 1. Then Sprite_UpdateScreenA (a tail jump).
// Called by EffectKind30_Push too. PSX 0x8019DEE4 (gap67).
extern "C" void __cdecl EffectKind30_Slide(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    SetUL(s + 0x34, UL(s + 0x34) + UL(s + 0xC));
    s = S();
    SetUL(s + 0x38, UL(s + 0x38) + UL(s + 0x10));
    s = S();
    if (s[9] == 0) {
        if (Game_AreaNumber == 0x54) {
            if (UL(s + 0x34) == 0x118000 && UL(s + 0x38) == 0x4A8000)
                SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x36);
            else
                SH_CALL(Flags_Clear)(At(at::kStoryFlags), 0x36);
        }
        SH_CALL(EffectKind30_ClaimCells)();
        S()[0xB] = 0;
        S()[1] = 1;
    }
    SH_CALL(Sprite_UpdateScreenA)();
}

// original 0x46BDA0: kind 0x30's state 3, shattering. +9 down; not yet 0:
// EffectKind30_ShardsStep, _SparksDraw, Sprite_UpdateScreenA (a tail jump).
// At 0: in area 0x92 story flag 0x44 set; Effect_Release (a tail jump).
// PSX 0x8019DFD4 (gap67).
extern "C" void __cdecl EffectKind30_Shatter(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (S()[9] != 0) {
        SH_CALL(EffectKind30_ShardsStep)();
        SH_CALL(EffectKind30_SparksDraw)();
        SH_CALL(Sprite_UpdateScreenA)();
        return;
    }
    if (Game_AreaNumber == 0x92) SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x44);
    SH_CALL(Effect_Release)();
}

// original 0x46BDF0: whether the way to (x, z) is blocked - al 1 or 0 (the
// rest of eax left). Sprite_ObjectAt(x, z, 1): an object there is touched
// (+0x80 bit 0; the answer taken unsigned) and the test goes on. Then the four
// cells from (x >> 16, z >> 16), in the order (0, 0), (1, 0), (0, 1), (1, 1):
// one EffectKind30_CellSolid - 1; after each, AreaMap_Elevation at the cell's
// corner (x & ~0xFFFF, + 0x10000 ...). All four heights equal (16 bits): 0,
// else 1. The original writes its argument slots as it goes (x and z's low
// words 0, then the neighbours); no caller reads them. Called by
// EffectKind30_Push. PSX 0x8019E05C (gap67).
extern "C" unsigned char __cdecl EffectKind30_WayBlocked(long x, long z) {
    const unsigned char o = SH_CALL(Sprite_ObjectAt)(x, z, 1);
    if (o != 0xFF) Touch("EffectKind30_WayBlocked", o);
    const U x0 = static_cast<U>(x) & 0xFFFF0000u, z0 = static_cast<U>(z) & 0xFFFF0000u;
    const unsigned xc = static_cast<U>(x) >> 16, zc = static_cast<U>(z) >> 16;
    if (SH_CALL(EffectKind30_CellSolid)(xc, zc) != 0) return 1;
    const auto h0 = static_cast<std::uint16_t>(SH_CALL(AreaMap_Elevation)(static_cast<long>(x0), static_cast<long>(z0)));
    if (SH_CALL(EffectKind30_CellSolid)(xc + 1, zc) != 0) return 1;
    const auto h1 =
        static_cast<std::uint16_t>(SH_CALL(AreaMap_Elevation)(static_cast<long>(x0 + 0x10000), static_cast<long>(z0)));
    if (SH_CALL(EffectKind30_CellSolid)(xc, zc + 1) != 0) return 1;
    const auto h2 =
        static_cast<std::uint16_t>(SH_CALL(AreaMap_Elevation)(static_cast<long>(x0), static_cast<long>(z0 + 0x10000)));
    if (SH_CALL(EffectKind30_CellSolid)(xc + 1, zc + 1) != 0) return 1;
    const auto h3 = static_cast<std::uint16_t>(
        SH_CALL(AreaMap_Elevation)(static_cast<long>(x0 + 0x10000), static_cast<long>(z0 + 0x10000)));
    return (h1 != h0 || h2 != h0 || h3 != h0) ? 1 : 0;
}

// original 0x46BF40: whether the area byte of cell (x, z) (AreaMap_ByteAt, 16
// bits each) blocks - al 0 for 0, 0xC0, 0x20 and the rows 0x80..0x8F and
// 0xA0..0xAF, else 1 (the rest of eax AreaMap_ByteAt's). Called by
// EffectKind30_WayBlocked. PSX 0x8019E214 (gap67).
extern "C" unsigned char __cdecl EffectKind30_CellSolid(unsigned x, unsigned z) {
    const unsigned char b = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (b == 0 || b == 0xC0) return 0;
    const unsigned row = b & 0xF0u;
    if (row == 0x80 || row == 0xA0 || b == 0x20) return 0;
    return 1;
}

// original 0x46BF80: the cells the object rests on claimed - its cell (x, z)
// the high words, and (x + 1, z), (x, z + 1), (x + 1, z + 1) where x's / z's
// fraction (word +0x34 / +0x38) is not 0: each AreaMap_SetHeight(cell, 0x10),
// its old area byte (AreaMap_ByteAt) into byte 0, 1, 2, 3 of the dword +0x14
// (byte 0 stores the whole dword; the others OR in, through the pointer read
// before the call), AreaMap_SetByte(cell, 0x11). Sprite_Current read again
// after every call. EffectKind30_Slide's and FC1's 0x46BB50's. PSX 0x8019E278
// (gap67).
extern "C" void __cdecl EffectKind30_ClaimCells(void) {
    unsigned char* s = S();
    SH_CALL(AreaMap_SetHeight)(CellX(s, 0), CellZ(s, 0), 0x10);
    s = S();
    const unsigned char b0 = SH_CALL(AreaMap_ByteAt)(static_cast<short>(CellX(s, 0)), static_cast<short>(CellZ(s, 0)));
    SetUL(S() + 0x14, b0);
    s = S();
    SH_CALL(AreaMap_SetByte)(CellX(s, 0), CellZ(s, 0), 0x11);
    s = S();
    if (Word(s + 0x34) != 0) {
        SH_CALL(AreaMap_SetHeight)(CellX(s, 1), CellZ(s, 0), 0x10);
        s = S();
        unsigned char* const cell = s + 0x14;
        const unsigned char b = SH_CALL(AreaMap_ByteAt)(static_cast<short>(CellX(s, 1)), static_cast<short>(CellZ(s, 0)));
        SetUL(cell, UL(cell) | static_cast<U>(b) << 8);
        s = S();
        SH_CALL(AreaMap_SetByte)(CellX(s, 1), CellZ(s, 0), 0x11);
        s = S();
    }
    if (Word(s + 0x38) != 0) {
        SH_CALL(AreaMap_SetHeight)(CellX(s, 0), CellZ(s, 1), 0x10);
        s = S();
        unsigned char* const cell = s + 0x14;
        const unsigned char b = SH_CALL(AreaMap_ByteAt)(static_cast<short>(CellX(s, 0)), static_cast<short>(CellZ(s, 1)));
        SetUL(cell, UL(cell) | static_cast<U>(b) << 16);
        s = S();
        SH_CALL(AreaMap_SetByte)(CellX(s, 0), CellZ(s, 1), 0x11);
        s = S();
    }
    if (Word(s + 0x34) != 0 && Word(s + 0x38) != 0) {
        SH_CALL(AreaMap_SetHeight)(CellX(s, 1), CellZ(s, 1), 0x10);
        s = S();
        unsigned char* const cell = s + 0x14;
        const unsigned char b = SH_CALL(AreaMap_ByteAt)(static_cast<short>(CellX(s, 1)), static_cast<short>(CellZ(s, 1)));
        SetUL(cell, UL(cell) | static_cast<U>(b) << 24);
        s = S();
        SH_CALL(AreaMap_SetByte)(CellX(s, 1), CellZ(s, 1), 0x11);
    }
}

// original 0x46C100: EffectKind30_ClaimCells undone - the same cells, each
// AreaMap_SetHeight(cell, 0) and AreaMap_SetByte(cell, byte 0 / 1 / 2 / 3 of
// the dword +0x14, read after the height call). EffectKind30_Push's.
// PSX 0x8019E4C4 (gap67).
extern "C" void __cdecl EffectKind30_FreeCells(void) {
    unsigned char* s = S();
    SH_CALL(AreaMap_SetHeight)(CellX(s, 0), CellZ(s, 0), 0);
    s = S();
    SH_CALL(AreaMap_SetByte)(CellX(s, 0), CellZ(s, 0), s[0x14]);
    s = S();
    if (Word(s + 0x34) != 0) {
        SH_CALL(AreaMap_SetHeight)(CellX(s, 1), CellZ(s, 0), 0);
        s = S();
        SH_CALL(AreaMap_SetByte)(CellX(s, 1), CellZ(s, 0), static_cast<U>(static_cast<std::int32_t>(UL(s + 0x14)) >> 8));
        s = S();
    }
    if (Word(s + 0x38) != 0) {
        SH_CALL(AreaMap_SetHeight)(CellX(s, 0), CellZ(s, 1), 0);
        s = S();
        SH_CALL(AreaMap_SetByte)(CellX(s, 0), CellZ(s, 1), static_cast<U>(static_cast<std::int32_t>(UL(s + 0x14)) >> 16));
        s = S();
    }
    if (Word(s + 0x34) != 0 && Word(s + 0x38) != 0) {
        SH_CALL(AreaMap_SetHeight)(CellX(s, 1), CellZ(s, 1), 0);
        s = S();
        SH_CALL(AreaMap_SetByte)(CellX(s, 1), CellZ(s, 1), static_cast<U>(static_cast<std::int32_t>(UL(s + 0x14)) >> 24));
    }
}

// original 0x46C200: the shards set up from the model *(+0x50) (read once):
// for each of its 24 faces of 0x28 bytes (four vertices of three s16 at +2,
// +8, +0xE, +0x14), shard i (EffectKind30_Shards + 0x38 i) gets the centre
// (+0, +2, +4: each axis' four s16 summed as ints, >> 2), the four vertices
// relative to it (+0x18 + 8 k, three s16, 16-bit differences), a velocity
// (+8, +0xA, +0xC) Gte_VectorNormalS of the centre (as three longs) >> 6
// each, and the angles +0x10 / +0x12 / +0x14 zero. EffectKind30_Push's and
// the engine's 0x46D710. PSX 0x8019E650 (gap67).
extern "C" void __cdecl EffectKind30_ShardsInit(void) {
    const unsigned char* face = PtrAt(S() + 0x50);
    for (unsigned i = 0; i < at::kShardCount; ++i, face += at::kFaceStride) {
        unsigned char* const r = At(at::kShards + i * at::kShardStride);
        SetWord(r + 0, static_cast<U>((SW(face + 8) + SW(face + 0x14) + SW(face + 0xE) + SW(face + 2)) >> 2));
        SetWord(r + 2, static_cast<U>((SW(face + 4) + SW(face + 0x16) + SW(face + 0xA) + SW(face + 0x10)) >> 2));
        SetWord(r + 4, static_cast<U>((SW(face + 0x18) + SW(face + 6) + SW(face + 0x12) + SW(face + 0xC)) >> 2));
        for (unsigned k = 0; k < 4; ++k) {
            const unsigned char* const v = face + 2 + 6 * k;
            unsigned char* const rel = r + 0x18 + 8 * k;
            SetWord(rel + 0, static_cast<U>(Word(v + 0) - Word(r + 0)));
            SetWord(rel + 2, static_cast<U>(Word(v + 2) - Word(r + 2)));
            SetWord(rel + 4, static_cast<U>(Word(v + 4) - Word(r + 4)));
        }
        long centre[3] = {SW(r + 0), SW(r + 2), SW(r + 4)};
        SH_CALL(Gte_VectorNormalS)(centre, reinterpret_cast<short*>(r + 8));
        SetWord(r + 0xA, static_cast<U>(SW(r + 0xA) >> 6));
        SetWord(r + 0xC, static_cast<U>(SW(r + 0xC) >> 6));
        SetWord(r + 0x10, 0);
        SetWord(r + 8, static_cast<U>(SW(r + 8) >> 6));
        SetWord(r + 0x12, 0);
        SetWord(r + 0x14, 0);
    }
}

// original 0x46C310: the shards a frame on - each: +0xC up by 2 (a fall), the
// centre moved by the velocity (+0 by +8, +4 by the new +0xC, +2 by +0xA, all
// 16-bit), then EffectKind30_ShardTumble(the shard, face i of the model
// *(+0x50), read once). EffectKind30_Shatter's and the engine's 0x46D770.
// PSX 0x8019E808 (gap67).
extern "C" void __cdecl EffectKind30_ShardsStep(void) {
    unsigned char* face = PtrAt(S() + 0x50);
    for (unsigned i = 0; i < at::kShardCount; ++i, face += at::kFaceStride) {
        unsigned char* const r = At(at::kShards + i * at::kShardStride);
        const unsigned vx = Word(r + 8);
        SetWord(r + 0xC, Word(r + 0xC) + 2u);
        SetWord(r + 0, Word(r + 0) + vx);
        SetWord(r + 4, Word(r + 4) + Word(r + 0xC));
        SetWord(r + 2, Word(r + 2) + Word(r + 0xA));
        SH_CALL(EffectKind30_ShardTumble)(r, face);
    }
}

// original 0x46C360: one shard turned to new angles and its face rebuilt. Under
// Gte_PushMatrix / Gte_PopMatrix: the angles +0x10 / +0x12 / +0x14 each Rand
// & 0xFC0; Gte_RotMatrix of them into a stack MATRIX, its translation zero,
// Gte_SetTransMatrix and Gte_SetRotMatrix of it; each of the four relative
// vertices (+0x18 + 8 k) through Gte_RotTrans, plus the centre (16-bit), into
// the face's vertex k (+2 + 6 k). Its two callers' stack MATRIX padding is
// never read (DIV-0021's hole); ours zeroes it. PSX 0x8019E8C8 (callers).
extern "C" void __cdecl EffectKind30_ShardTumble(unsigned char* shard, unsigned char* face) {
    SH_CALL(Gte_PushMatrix)();
    SetWord(shard + 0x10, static_cast<U>(SH_CALL(Rand)()) & 0xFC0u);
    SetWord(shard + 0x12, static_cast<U>(SH_CALL(Rand)()) & 0xFC0u);
    SetWord(shard + 0x14, static_cast<U>(SH_CALL(Rand)()) & 0xFC0u);
    Matrix m{};
    SH_CALL(Gte_RotMatrix)(reinterpret_cast<const short*>(shard + 0x10), m.m);
    m.t[0] = m.t[1] = m.t[2] = 0;
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    for (unsigned k = 0; k < 4; ++k) {
        long out[3];
        SH_CALL(Gte_RotTrans)(reinterpret_cast<const short*>(shard + 0x18 + 8 * k), out);
        unsigned char* const v = face + 2 + 6 * k;
        SetWord(v + 0, Word(shard + 0) + static_cast<U>(out[0]));
        SetWord(v + 2, static_cast<U>(out[1]) + Word(shard + 2));
        SetWord(v + 4, static_cast<U>(out[2]) + Word(shard + 4));
    }
    SH_CALL(Gte_PopMatrix)();
}

// original 0x46C430: the eight sparks set up: the shade byte 0x92C4C0 0x40,
// the size word 0x92C4C2 0; each record (0x92C4C4 + 0x20 i) the object's x, z
// and height dwords (+0x34, +0x38, +0x3C; Sprite_Current read per spark), a
// direction (Rand & 0xFF) - 0x80, (Rand & 0xFF) - 0x80, Rand & 0x7F at +0x10
// .. +0x18 normalised in place by Gte_VectorNormal, then +0x18 << 9.
// EffectKind30_Push's and the engine's 0x46D710. No PSX twin paired.
extern "C" void __cdecl EffectKind30_SparksInit(void) {
    At(at::kSparkShade)[0] = 0x40;
    SetWord(At(at::kSparkSize), 0);
    for (unsigned i = 0; i < at::kSparkCount; ++i) {
        unsigned char* const p = At(at::kSparkRecords + i * at::kSparkStride);
        const unsigned char* const s = S();
        SetUL(p + 0, UL(s + 0x34));
        SetUL(p + 4, UL(s + 0x38));
        SetUL(p + 8, UL(s + 0x3C));
        SetUL(p + 0x10, (static_cast<U>(SH_CALL(Rand)()) & 0xFFu) - 0x80u);
        SetUL(p + 0x14, (static_cast<U>(SH_CALL(Rand)()) & 0xFFu) - 0x80u);
        SetUL(p + 0x18, static_cast<U>(SH_CALL(Rand)()) & 0x7Fu);
        SH_CALL(Gte_VectorNormal)(reinterpret_cast<const long*>(p + 0x10), reinterpret_cast<long*>(p + 0x10));
        SetUL(p + 0x18, UL(p + 0x18) << 9);
    }
}

// original 0x46C4B0: the sparks drawn and moved. 0x494060 (the map camera);
// each spark EffectKind30_SparkQuad(it, the size word, the shade byte), then
// moved by its direction - doubled while the object's +9 (Sprite_Current read
// after the call) is 0xC or more. After the eight, by the last +9 read: 0xC or
// more, the size word up 0x80; else the shade byte down 5. EffectKind30_
// Shatter's and the engine's 0x46D770 (a tail jump). No PSX twin paired.
extern "C" void __cdecl EffectKind30_SparksDraw(void) {
    SH_AT(void (__cdecl*)(void), at::kCameraMatrices)();
    const unsigned char* s = nullptr;
    for (unsigned i = 0; i < at::kSparkCount; ++i) {
        unsigned char* const p = At(at::kSparkRecords + i * at::kSparkStride);
        SH_CALL(EffectKind30_SparkQuad)(reinterpret_cast<const long*>(p), Word(At(at::kSparkSize)), At(at::kSparkShade)[0]);
        s = S();
        const unsigned shift = s[9] >= 0xC ? 1 : 0;
        SetUL(p + 0, UL(p + 0) + (UL(p + 0x10) << shift));
        SetUL(p + 4, UL(p + 4) + (UL(p + 0x14) << shift));
        SetUL(p + 8, UL(p + 8) + (UL(p + 0x18) << shift));
    }
    if (s[9] >= 0xC)
        SetWord(At(at::kSparkSize), Word(At(at::kSparkSize)) + 0x80u);
    else
        At(at::kSparkShade)[0] = static_cast<unsigned char>(At(at::kSparkShade)[0] + 0xFB);
}

// original 0x46C550: one spark's POLY_FT4 at Gfx_PacketNext (read once),
// Gpu_SetPolyFT4, semi-transparent (Gpu_SetSemiTrans 1). The point projected
// by 0x494110 (screen x, y, depth as floats) and the square `size` (16 bits,
// both axes) by 0x4941E0 at its depth into (w, h). The corners x - (w >> 1)
// and + w, y - (h >> 1) and + h - x87 at the game's 53-bit precision: the
// float less the int, plus the int, rounded once to a float - at +8/+0xC,
// +0x18/+0x1C, +0x28/+0x2C, +0x38/+0x3C; the depth at +0x10 +0x20 +0x30
// +0x40; texture u 0xE0 / 0xFF, v 0x30 / 0x4F; the CLUT Gpu_GetClut(0xA0,
// 0x1E3) at +0x16, the page Gpu_GetTPage(0, 1, 0x2C0, 0x100) at +0x26; the
// colour bytes +4..+6 the shade (a byte); MapView_LinkPrimAt(x, z of the point,
// 2, 0x48). The original uses its second argument's slot as a scratch word.
// EffectKind30_SparksDraw's. No PSX twin paired.
extern "C" void __cdecl EffectKind30_SparkQuad(const long* point, unsigned size, unsigned shade) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    float screen[3];
    SH_AT(void (__cdecl*)(const long*, float*), at::kProjectPoint)(point, screen);
    short wh[2] = {static_cast<short>(size), static_cast<short>(size)};
    SH_AT(void (__cdecl*)(const long*, const short*, short*), at::kProjectSize)(point, wh, wh);
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
    prim[4] = static_cast<unsigned char>(shade);
    prim[5] = static_cast<unsigned char>(shade);
    prim[6] = static_cast<unsigned char>(shade);
    SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(point[0]), static_cast<unsigned long>(point[1]), 2, 0x48);
}

// ===========================================================================
// Kind 0x34: Effect_KindHandlers[0x34]; +1 the variant, +2 its state
// ===========================================================================

// original 0x46C730: Effect_KindHandlers[0x34] - jmp [EffectKind34_Variants +
// 4 * +1], 6 entries (the five below and FC1's 0x46A310). No PSX twin paired.
extern "C" void __cdecl EffectKind34_Run(void) { Run("EffectKind34_Run", AddressOf(EffectKind34_Variants), 6, 1); }

// original 0x46C750: variant 0 - jmp [EffectKind34_V0States + 4 * +2], 4
// entries. PSX 0x8019EE18 (table-anchored).
extern "C" void __cdecl EffectKind34_V0Run(void) { Run("EffectKind34_V0Run", AddressOf(EffectKind34_V0States), 4, 2); }

// original 0x46C770: variant 0's state 0, a burst: ten times Effect_FindFree
// and, for each record it gives, +0 1, +5 0x34, +2 2 (variant 0 at state 2:
// the +1 a released record keeps, 0), +6 the count 0..9, x / z / height from
// Sprite_Current (read after each find); then Sound_PlayEffect(0x10C) and, with
// +0xB set, Camera_Distance down 0x80, MapView_Redraw 3, +2 up; without it,
// Effect_Release. PSX 0x8019EE5C (table-anchored).
extern "C" void __cdecl EffectKind34_V0Burst(void) {
    for (unsigned i = 0; i < 10; ++i) {
        const unsigned char n = SH_CALL(Effect_FindFree)();
        if (n == 0xFF) continue;
        const unsigned char* const s = S();
        unsigned char* const e = EffectRecord("EffectKind34_V0Burst", n);
        e[0] = 1;
        e[5] = 0x34;
        e[2] = 2;
        e[6] = static_cast<unsigned char>(i);
        CopyPlace(e, s);
    }
    SH_CALL(Sound_PlayEffect)(0x10C);
    unsigned char* const s = S();
    if (s[0xB] == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    Camera_Distance = static_cast<short>(Camera_Distance - 0x80);
    MapView_Redraw = 3;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x46C810: variant 0's state 1: Camera_Distance 0, Effect_Release (a
// tail jump). PSX 0x8019EF94 (table-anchored).
extern "C" void __cdecl EffectKind34_V0BurstEnd(void) {
    Camera_Distance = 0;
    SH_CALL(Effect_Release)();
}

// original 0x46C820: variant 0's state 2, a piece of debris set up.
// Sprite_SetAnimationBank(0x212 when Game_AreaNumber is one of the nine bytes
// EffectKind34_V0Areas, else 0x1B); al not 0 - Effect_Release (a tail jump).
// Else the height word +0x3E up Rand & 0x7F (through the pointer taken before
// the call), +0x3C word 0, +0x24 0, +0x2A 0, +0x2B up, +0x48 0; x and z dwords
// up Rand & 0x7FFF each; +0x14 0xFFF80000; +0xC and +0x10 Rand & 0xFFF each
// (read after the call); animation 1 when +0xC is above 0x800, or (+0x2A 1)
// when +0x10 is, else 0 (Sprite_SetAnimation); +0x5D..+0x5F 0, +9 0x10, +0xA 8,
// +2 up. A gap of the tool (in no cut row, table entry 2). PSX 0x8019EFBC
// (table-anchored).
extern "C" void __cdecl EffectKind34_V0DebrisStart(void) {
    const unsigned area = Game_AreaNumber;
    unsigned bank = 0x1B;
    for (int i = 8; i >= 0; --i)
        if (EffectKind34_V0Areas[i] == area) {
            bank = 0x212;
            break;
        }
    if (SH_CALL(Sprite_SetAnimationBank)(static_cast<unsigned short>(bank)) != 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    {
        unsigned char* const p = S() + 0x3E;
        const U r = static_cast<U>(SH_CALL(Rand)());
        SetWord(p, Word(p) + (r & 0x7F));
    }
    unsigned char* s = S();
    SetWord(s + 0x3C, 0);
    s[0x24] = 0;
    s[0x2A] = 0;
    s[0x2B] = static_cast<unsigned char>(s[0x2B] + 1);
    s[0x48] = 0;
    {
        unsigned char* const p = S() + 0x34;
        const U r = static_cast<U>(SH_CALL(Rand)());
        SetUL(p, UL(p) + (r & 0x7FFF));
    }
    {
        unsigned char* const p = S() + 0x38;
        const U r = static_cast<U>(SH_CALL(Rand)());
        SetUL(p, UL(p) + (r & 0x7FFF));
    }
    SetUL(S() + 0x14, 0xFFF80000u);
    {
        const U r = static_cast<U>(SH_CALL(Rand)());
        SetUL(S() + 0xC, r & 0xFFF);
    }
    {
        const U r = static_cast<U>(SH_CALL(Rand)());
        SetUL(S() + 0x10, r & 0xFFF);
    }
    s = S();
    unsigned animation = 0;
    if (static_cast<std::int32_t>(UL(s + 0xC)) > 0x800) {
        animation = 1;
    } else if (static_cast<std::int32_t>(UL(s + 0x10)) > 0x800) {
        s[0x2A] = 1;
        animation = 1;
    }
    SH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
    s = S();
    s[0x5D] = 0;
    s[0x5E] = 0;
    s[0x5F] = 0;
    s[9] = 0x10;
    s[0xA] = 8;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x46C980: variant 0's state 3, the debris flying: x, z, height
// dwords by +0xC, +0x10, +0x14; Sprite_ScriptTick; +9 counting down with a
// draw (Sprite_UpdateScreenSlot) each frame, then +0xA down with a draw on odd
// Frame_Counter only; both 0: Effect_Release. PSX 0x8019F1A8 (table-anchored).
extern "C" void __cdecl EffectKind34_V0DebrisFly(void) {
    unsigned char* s = S();
    SetUL(s + 0x34, UL(s + 0x34) + UL(s + 0xC));
    s = S();
    SetUL(s + 0x38, UL(s + 0x38) + UL(s + 0x10));
    s = S();
    SetUL(s + 0x3C, UL(s + 0x3C) + UL(s + 0x14));
    SH_CALL(Sprite_ScriptTick)();
    s = S();
    if (s[9] != 0) {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        SH_CALL(Sprite_UpdateScreenSlot)();
        return;
    }
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        if ((Frame_Counter & 1) != 0) SH_CALL(Sprite_UpdateScreenSlot)();
        return;
    }
    SH_CALL(Effect_Release)();
}

// original 0x46C9F0: variant 1 - jmp [EffectKind34_V1States + 4 * +2], 2
// entries. PSX 0x8019F26C (table-anchored).
extern "C" void __cdecl EffectKind34_V1Run(void) { Run("EffectKind34_V1Run", AddressOf(EffectKind34_V1States), 2, 2); }

// The shared start of variants 1..3: Sprite_SetAnimationBank(0x18) (its al
// unread), +0x24 0, +0x2A 0, +0x2B up, +0x48 0.
void PieceBank() {
    SH_CALL(Sprite_SetAnimationBank)(0x18);
    unsigned char* const s = S();
    s[0x24] = 0;
    s[0x2A] = 0;
    s[0x2B] = static_cast<unsigned char>(s[0x2B] + 1);
    s[0x48] = 0;
}
void ClearTints(unsigned char* s) {
    s[0x5D] = 0;
    s[0x5E] = 0;
    s[0x5F] = 0;
}

// original 0x46CA10: variant 1's state 0: the piece bank (0x18), the rise
// +0x14 0x40, the pull +0x20 -8, +0x5D..+0x5F 0, Sprite_SetAnimation(8), +2 up.
// Spawned by EffectKind3A_Hit. PSX 0x8019F2B0 (gap74).
extern "C" void __cdecl EffectKind34_V1Start(void) {
    PieceBank();
    unsigned char* const s = S();
    SetUL(s + 0x14, 0x40);
    SetUL(s + 0x20, 0xFFFFFFF8u);
    ClearTints(s);
    SH_CALL(Sprite_SetAnimation)(8);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x46CA90: variant 1's state 1: +0x14 += +0x20, the height word
// +0x3E += +0x14's low word; Sprite_ScriptTick; the ground AreaMap_Elevation(x,
// z). The height at or below it (s16): Effect_Release; more than 0x100 above:
// drawn (Sprite_UpdateScreenSlot); else drawn on odd Frame_Counter. PSX
// 0x8019F374 (gap74).
extern "C" void __cdecl EffectKind34_V1Fall(void) {
    unsigned char* s = S();
    SetUL(s + 0x14, UL(s + 0x14) + UL(s + 0x20));
    s = S();
    SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));
    SH_CALL(Sprite_ScriptTick)();
    s = S();
    const auto ground = static_cast<short>(SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38)));
    const short height = SW(S() + 0x3E);
    if (height <= ground) {
        SH_CALL(Effect_Release)();
        return;
    }
    if (static_cast<int>(height) > static_cast<int>(ground) + 0x100) {
        SH_CALL(Sprite_UpdateScreenSlot)();
        return;
    }
    if ((Frame_Counter & 1) != 0) SH_CALL(Sprite_UpdateScreenSlot)();
}

// original 0x46CB00: variant 2 - jmp [EffectKind34_V2States + 4 * +2], 4
// entries. PSX 0x8019F42C (table-anchored).
extern "C" void __cdecl EffectKind34_V2Run(void) { Run("EffectKind34_V2Run", AddressOf(EffectKind34_V2States), 4, 2); }

// original 0x46CB20: variant 2's state 0: +9 0, +0xA 5 (pieces to make), +2 up.
// PSX 0x8019F470 (gap74).
extern "C" void __cdecl EffectKind34_V2Start(void) {
    unsigned char* const s = S();
    s[9] = 0;
    s[0xA] = 5;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x46CB40: variant 2's state 1, a spawner: +9 up; at 4 it is 0 again
// and Effect_FindFree's record (if any) becomes a kind-0x34 piece of variant 2
// at state 2 (+0 1, +5 0x34, +1 2, +2 2) at Sprite_Current's place (read after
// the find), and +0xA down; at 0, Effect_Release. PSX 0x8019F4B0 (gap74).
extern "C" void __cdecl EffectKind34_V2Spawn(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] != 4) return;
    s[9] = 0;
    const unsigned char n = SH_CALL(Effect_FindFree)();
    s = S();
    if (n != 0xFF) {
        unsigned char* const e = EffectRecord("EffectKind34_V2Spawn", n);
        e[0] = 1;
        e[5] = 0x34;
        e[1] = 2;
        e[2] = 2;
        CopyPlace(e, s);
    }
    s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
    if (S()[0xA] == 0) SH_CALL(Effect_Release)();
}

// original 0x46CBD0: variant 2's state 2, a piece set up: the piece bank; x and
// z dwords up Rand & 0xFFFF each (through the pointer taken before the call);
// +0x14 0, +0x20 -8; animation 0xB + (Rand & 3, at most 2); +0x5D..+0x5F 0
// (after the draw); +2 up. PSX 0x8019F5DC (gap74).
extern "C" void __cdecl EffectKind34_V2PieceStart(void) {
    PieceBank();
    {
        unsigned char* const p = S() + 0x34;
        const U r = static_cast<U>(SH_CALL(Rand)());
        SetUL(p, UL(p) + (r & 0xFFFF));
    }
    {
        unsigned char* const p = S() + 0x38;
        const U r = static_cast<U>(SH_CALL(Rand)());
        SetUL(p, UL(p) + (r & 0xFFFF));
    }
    SetUL(S() + 0x14, 0);
    SetUL(S() + 0x20, 0xFFFFFFF8u);
    unsigned pick = static_cast<U>(SH_CALL(Rand)()) & 3;
    if (pick > 2) pick = 2;
    ClearTints(S());
    SH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(pick + 0xB));
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x46CCA0: variant 2's state 3: Sprite_ScriptTick first, then as
// EffectKind34_V1Fall's move; the height at or below the ground (s16):
// Effect_Release, else drawn. PSX 0x8019F700 (gap74).
extern "C" void __cdecl EffectKind34_V2PieceFall(void) {
    SH_CALL(Sprite_ScriptTick)();
    unsigned char* s = S();
    SetUL(s + 0x14, UL(s + 0x14) + UL(s + 0x20));
    s = S();
    SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));
    s = S();
    const auto ground = static_cast<short>(SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38)));
    if (SW(S() + 0x3E) <= ground)
        SH_CALL(Effect_Release)();
    else
        SH_CALL(Sprite_UpdateScreenSlot)();
}

// original 0x46CCF0: variant 3 - jmp [EffectKind34_V3States + 4 * +2], 2
// entries. PSX 0x8019F788 (table-anchored).
extern "C" void __cdecl EffectKind34_V3Run(void) { Run("EffectKind34_V3Run", AddressOf(EffectKind34_V3States), 2, 2); }

// original 0x46CD10: variant 3's state 0: the piece bank, +0x14 0, +0x20 -8,
// +0x5D..+0x5F 0, Sprite_SetAnimation(9), +2 up. PSX 0x8019F7CC (gap74).
extern "C" void __cdecl EffectKind34_V3Start(void) {
    PieceBank();
    unsigned char* const s = S();
    SetUL(s + 0x14, 0);
    SetUL(s + 0x20, 0xFFFFFFF8u);
    ClearTints(s);
    SH_CALL(Sprite_SetAnimation)(9);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x46CD90: variant 3's state 1: EffectKind34_V1Fall's move, then
// Sprite_ScriptTick; the height at or above the ground (s16): drawn, else
// Effect_Release. PSX 0x8019F88C (gap74).
extern "C" void __cdecl EffectKind34_V3Fall(void) {
    unsigned char* s = S();
    SetUL(s + 0x14, UL(s + 0x14) + UL(s + 0x20));
    s = S();
    SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));
    SH_CALL(Sprite_ScriptTick)();
    s = S();
    const auto ground = static_cast<short>(SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38)));
    if (SW(S() + 0x3E) >= ground)
        SH_CALL(Sprite_UpdateScreenSlot)();
    else
        SH_CALL(Effect_Release)();
}

// original 0x46CDE0: variant 4 - jmp [EffectKind34_V4States + 4 * +2], 2
// entries. PSX 0x8019F920 (table-anchored).
extern "C" void __cdecl EffectKind34_V4Run(void) { Run("EffectKind34_V4Run", AddressOf(EffectKind34_V4States), 2, 2); }

// original 0x46CE00: variant 4's state 0: Sprite_SetAnimationBank(0x46); al not
// 0 - Effect_Release (a tail jump). Else +0x29 up, +0x2A 0, +0x24 0, +0x2B up,
// +0x48 1, the step +0xC 0 / +0x10 -0x2000 / +0x14 0x40, +0x5D..+0x5F 0,
// Sprite_SetAnimation(0), +2 up. No PSX twin paired.
extern "C" void __cdecl EffectKind34_V4Start(void) {
    if (SH_CALL(Sprite_SetAnimationBank)(0x46) != 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    unsigned char* const s = S();
    s[0x29] = static_cast<unsigned char>(s[0x29] + 1);
    s[0x2A] = 0;
    s[0x24] = 0;
    s[0x2B] = static_cast<unsigned char>(s[0x2B] + 1);
    s[0x48] = 1;
    SetUL(s + 0xC, 0);
    SetUL(s + 0x10, 0xFFFFE000u);
    SetUL(s + 0x14, 0x40);
    ClearTints(s);
    SH_CALL(Sprite_SetAnimation)(0);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x46CEA0: variant 4's state 1: x, z dwords by +0xC / +0x10, the
// height word by +0x14's low word; +0 bit 7 set - Effect_Release, else drawn
// (both tail jumps). No PSX twin paired.
extern "C" void __cdecl EffectKind34_V4Move(void) {
    unsigned char* const s = S();
    SetUL(s + 0x34, UL(s + 0x34) + UL(s + 0xC));
    SetUL(s + 0x38, UL(s + 0x38) + UL(s + 0x10));
    SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));
    if ((s[0] & 0x80) != 0)
        SH_CALL(Effect_Release)();
    else
        SH_CALL(Sprite_UpdateScreenSlot)();
}

// ===========================================================================
// Kind 0x3A: Effect_KindHandlers[0x3A]
// ===========================================================================

// original 0x46CEF0: Effect_KindHandlers[0x3A] - jmp [EffectKind3A_States + 4
// * +1], 3 entries. A gap of the tool (in no cut row). No PSX twin paired.
extern "C" void __cdecl EffectKind3A_Run(void) { Run("EffectKind3A_Run", AddressOf(EffectKind3A_States), 3, 1); }

// original 0x46CF10: kind 0x3A's state 0: +8 the leader's facing 0x802D48; x /
// z the leader's (0x802D74 / 0x802D78) plus Field_DirectionSteps' half cell
// for the facing, the height word the leader's (0x802D7E) + 0xC0; the step
// +0xC / +0x10 sixteen times the facing's unit (0x6696DC rows); +0xA 0xE
// frames, +0xB 0, +1 up. The facing byte indexes both tables unchecked, as the
// original (a data read). No PSX twin paired.
extern "C" void __cdecl EffectKind3A_Start(void) {
    unsigned char* s = S();
    s[8] = At(at::kLeader)[8];
    s = S();
    SetUL(s + 0x34, UL(At(AddressOf(Field_DirectionSteps) + 8u * s[8])) + UL(At(at::kLeader + 0x34)));
    s = S();
    SetUL(s + 0x38, UL(At(AddressOf(Field_DirectionSteps) + 4 + 8u * s[8])) + UL(At(at::kLeader + 0x38)));
    SetWord(S() + 0x3E, Word(At(at::kLeader + 0x3E)) + 0xC0u);
    s = S();
    SetUL(s + 0xC, UL(At(at::kUnitSteps + 8u * s[8])) << 4);
    s = S();
    SetUL(s + 0x10, UL(At(at::kUnitSteps + 4 + 8u * s[8])) << 4);
    s = S();
    s[0xA] = 0xE;
    s[0xB] = 0;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46CFC0: kind 0x3A's state 1, flying: x / z by the step; the tile
// word of the area block at the cell (s16 z times the width byte, plus s16 x,
// plus twice the header's offset word, words from AreaMap_Header) 0 -
// Effect_Release (a tail jump). EffectKind3A_Hit: a hit - EffectKind3A_
// LeaderPose, +0x27 the leader's +0x27 (0x802D67), Sprite_SetAnimation(0x4A),
// Sound_PlayEffect(0x10E), +1 up; no hit - +0xA down, at 0 Effect_Release.
// PSX 0x8019FC28 (callers).
extern "C" void __cdecl EffectKind3A_Fly(void) {
    unsigned char* s = S();
    SetUL(s + 0x34, UL(s + 0x34) + UL(s + 0xC));
    s = S();
    SetUL(s + 0x38, UL(s + 0x38) + UL(s + 0x10));
    s = S();
    const U width = UL(AreaMap_Header) & 0xFFu;
    const U cell = static_cast<U>(static_cast<std::int32_t>(SW(s + 0x3A))) * width +
                   static_cast<U>(static_cast<std::int32_t>(SW(s + 0x36))) + Word(At(at::kAreaTileBase)) * 2u;
    if (Word(At(AddressOf(AreaMap_Header) + cell * 2u)) == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    if (SH_CALL(EffectKind3A_Hit)() != 0) {
        SH_CALL(EffectKind3A_LeaderPose)();
        S()[0x27] = At(at::kLeader)[0x27];
        SH_CALL(Sprite_SetAnimation)(0x4A);
        SH_CALL(Sound_PlayEffect)(0x10E);
        S()[1] = static_cast<unsigned char>(S()[1] + 1);
        return;
    }
    s = S();
    s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
    if (S()[0xA] == 0) SH_CALL(Effect_Release)();
}

// original 0x46D080: kind 0x3A's state 2, the landed pose playing out. With
// Field_Request set: nothing while +0xB is 0; else Sprite_ScriptTickOnce (al
// set: +0xB 1) and a draw (Sprite_UpdateScreenSlot) while +0xB is 2. With
// Field_Request 0: Sprite_ScriptTickOnce; al set or +0xB set - Effect_Release,
// else drawn. PSX 0x8019FD48 (gap44).
extern "C" void __cdecl EffectKind3A_Wait(void) {
    if (Field_Request != 0) {
        if (S()[0xB] == 0) return;
        if (SH_CALL(Sprite_ScriptTickOnce)() != 0) S()[0xB] = 1;
        if (S()[0xB] == 2) SH_CALL(Sprite_UpdateScreenSlot)();
        return;
    }
    if (SH_CALL(Sprite_ScriptTickOnce)() != 0 || S()[0xB] != 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    SH_CALL(Sprite_UpdateScreenSlot)();
}

// original 0x46D0E0: Sprite_Current drawn with the leader's graphics: +0x24 1,
// +0x2C the leader's word +0x2C (0x802D6C), +0x28 2, +0x4C its dword +0x4C
// (0x802D8C), +0x2B 1, +0x29 its +0x29 (0x802D69), +0x70 0, +0x25 0x1D, +0x26
// 0, +0x5D..+0x5F 0, +0x5C 0, +0x48 0. eax is left 0; no caller reads it.
// EffectKind3A_Fly's and FC1's 0x46B7C0. PSX 0x8019FE18 (gap44).
extern "C" void __cdecl EffectKind3A_LeaderPose(void) {
    unsigned char* const s = S();
    const unsigned char* const l = At(at::kLeader);
    s[0x24] = 1;
    SetWord(s + 0x2C, Word(l + 0x2C));
    s[0x28] = 2;
    SetUL(s + 0x4C, UL(l + 0x4C));
    s[0x2B] = 1;
    s[0x29] = l[0x29];
    SetUL(s + 0x70, 0);
    s[0x25] = 0x1D;
    s[0x26] = 0;
    ClearTints(s);
    s[0x5C] = 0;
    s[0x48] = 0;
}

// original 0x46D180: whether the thrown object hits something - al 1 or 0 (the
// rest of eax left). The ground AreaMap_Elevation(x, z) first; then
// Sprite_ObjectAt(x, z, 0): an object - touched (+0x80 bit 0; the answer taken
// signed), 1. Else by the leader's height word 0x802D7E against the ground
// (s16): more than 0x80 above it - 0; the ground more than 0x100 above the
// leader - 1. The area byte (AreaMap_ByteAt) of the cell 0x11 - 1. Then each
// 0xFD cell of the cell and its neighbours (x + 1 when x's fraction word is not
// 0, z + 1 likewise, both when both), until one is found, cleared by FE2's
// 0x5728D0 (the diagonal only when neither single neighbour was). None - 0.
// One: Rand's low nibble above 0xC - +0xB 2, FE1's 0x5307C0(0xA for 0xF, else
// 5), Effect_FindFree's record a kind-0x34 piece of variant 1 (+0 1, +5 0x34,
// +1 1, +0xB 0) at x, z and the ground there + 0x100, record or not the
// leader's +7 (0x802D47) 1; 0xC or below - +0xB 0. Either way 1.
// EffectKind3A_Fly's. PSX 0x8019FEEC (callers).
extern "C" unsigned char __cdecl EffectKind3A_Hit(void) {
    unsigned char* s = S();
    const auto ground = static_cast<short>(SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38)));
    s = S();
    const unsigned char o = SH_CALL(Sprite_ObjectAt)(Long(s + 0x34), Long(s + 0x38), 0);
    if (o != 0xFF) {
        Touch("EffectKind3A_Hit", static_cast<signed char>(o));
        return 1;
    }
    const int leader = SW(At(at::kLeader + 0x3E));
    if (leader - ground > 0x80) return 0;
    if (ground - leader > 0x100) return 1;
    s = S();
    if (SH_CALL(AreaMap_ByteAt)(static_cast<short>(CellX(s, 0)), static_cast<short>(CellZ(s, 0))) == 0x11) return 1;
    bool found = false;
    s = S();
    if (SH_CALL(AreaMap_ByteAt)(static_cast<short>(CellX(s, 0)), static_cast<short>(CellZ(s, 0))) == 0xFD) {
        s = S();
        SH_AT(void (__cdecl*)(unsigned, unsigned), at::kClearCell)(CellX(s, 0), CellZ(s, 0));
        found = true;
    }
    s = S();
    if (Word(s + 0x34) != 0 && !found) {
        const unsigned char b = SH_CALL(AreaMap_ByteAt)(static_cast<short>(CellX(s, 1)), static_cast<short>(CellZ(s, 0)));
        s = S();
        if (b == 0xFD) {
            SH_AT(void (__cdecl*)(unsigned, unsigned), at::kClearCell)(CellX(s, 1), CellZ(s, 0));
            s = S();
            found = true;
        }
    }
    if (Word(s + 0x38) != 0 && !found) {
        const unsigned char b = SH_CALL(AreaMap_ByteAt)(static_cast<short>(CellX(s, 0)), static_cast<short>(CellZ(s, 1)));
        s = S();
        if (b == 0xFD) {
            SH_AT(void (__cdecl*)(unsigned, unsigned), at::kClearCell)(CellX(s, 0), CellZ(s, 1));
            s = S();
            found = true;
        }
    }
    if (Word(s + 0x34) != 0 && Word(s + 0x38) != 0) {
        if (!found) {
            if (SH_CALL(AreaMap_ByteAt)(static_cast<short>(CellX(s, 1)), static_cast<short>(CellZ(s, 1))) != 0xFD) return 0;
            s = S();
            SH_AT(void (__cdecl*)(unsigned, unsigned), at::kClearCell)(CellX(s, 1), CellZ(s, 1));
        }
    } else if (!found) {
        return 0;
    }
    const unsigned luck = static_cast<U>(SH_CALL(Rand)()) & 0xF;
    s = S();
    if (luck <= 0xC) {
        s[0xB] = 0;
        return 1;
    }
    s[0xB] = 2;
    SH_AT(void (__cdecl*)(unsigned), at::kZennyFind)(luck == 0xF ? 0xA : 5);
    const unsigned char n = SH_CALL(Effect_FindFree)();
    if (n != 0xFF) {
        // the original widens the answer signed (movsx): 0x80..0xFE would be a
        // record before the pool, 0x14..0x7F one past it - a Fatal either way
        unsigned char* const e = EffectRecord("EffectKind3A_Hit", n);
        const unsigned char* const cur = S();
        e[0] = 1;
        e[5] = 0x34;
        e[1] = 1;
        SetUL(e + 0x34, UL(cur + 0x34));
        SetUL(e + 0x38, UL(cur + 0x38));
        const long at_ground = SH_CALL(AreaMap_Elevation)(Long(e + 0x34), Long(e + 0x38));
        SetWord(e + 0x3E, static_cast<U>(at_ground) + 0x100u);
        e[0xB] = 0;
    }
    At(at::kLeader)[7] = 1;
    return 1;
}

// ===========================================================================
// Kind 0x41: Effect_KindHandlers[0x41]
// ===========================================================================

// original 0x46D400: Effect_KindHandlers[0x41] - jmp [EffectKind41_States + 4
// * +1], 4 entries. A gap of the tool (in no cut row). No PSX twin paired.
extern "C" void __cdecl EffectKind41_Run(void) { Run("EffectKind41_Run", AddressOf(EffectKind41_States), 4, 1); }

// original 0x46D420: kind 0x41's state 0: the offset's low word +0x38 0, its
// high word +0x3A -10 with Field_InputFlags bit 0, else -20; the rise +0x10
// 0x20000, the commit slot +0x29 3, +9 6 frames, +1 up. No PSX twin paired.
extern "C" void __cdecl EffectKind41_Start(void) {
    SetWord(S() + 0x38, 0);
    SetWord(S() + 0x3A, (Field_InputFlags & 1) != 0 ? 0xFFF6u : 0xFFECu);
    SetUL(S() + 0x10, 0x20000);
    S()[0x29] = 3;
    S()[9] = 6;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x46D480: kind 0x41's state 1: +9 down, the number drawn (0x46D5F0
// at the member's screen words, the offset added to y); +9 0 - +1 up. No PSX
// twin paired.
extern "C" void __cdecl EffectKind41_Hold(void) {
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    DrawNumber(S());
    if (S()[9] == 0) S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x46D4E0: kind 0x41's state 2, the bounce: +0x10 up 0x14000, the
// offset dword +0x38 by it; the offset's high word not negative - +0x10
// negated, +9 6, +1 up; then drawn. No PSX twin paired.
extern "C" void __cdecl EffectKind41_Bounce(void) {
    unsigned char* s = S();
    SetUL(s + 0x10, UL(s + 0x10) + 0x14000u);
    s = S();
    SetUL(s + 0x38, UL(s + 0x38) + UL(s + 0x10));
    s = S();
    if (SW(s + 0x3A) >= 0) {
        SetUL(s + 0x10, 0u - UL(s + 0x10));
        S()[9] = 6;
        S()[1] = static_cast<unsigned char>(S()[1] + 1);
        s = S();
    }
    DrawNumber(s);
}

// original 0x46D570: kind 0x41's state 3: +9 0 - Effect_Release (a tail jump);
// else +9 down, the offset moved as the bounce's, and drawn on odd
// Frame_Counter. No PSX twin paired.
extern "C" void __cdecl EffectKind41_Fade(void) {
    unsigned char* s = S();
    if (s[9] == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    SetUL(s + 0x10, UL(s + 0x10) + 0x14000u);
    s = S();
    SetUL(s + 0x38, UL(s + 0x38) + UL(s + 0x10));
    if ((Frame_Counter & 1) != 0) DrawNumber(S());
}

void FieldC2_Inject() {
    if (bof3::WantsShadow("field_c2")) field_c2::SelfTest();
    BOF3_INJECT(EffectKind30_Push);
    BOF3_INJECT(EffectKind30_Slide);
    BOF3_INJECT(EffectKind30_Shatter);
    BOF3_INJECT(EffectKind30_WayBlocked);
    BOF3_INJECT(EffectKind30_CellSolid);
    BOF3_INJECT(EffectKind30_ClaimCells);
    BOF3_INJECT(EffectKind30_FreeCells);
    BOF3_INJECT(EffectKind30_ShardsInit);
    BOF3_INJECT(EffectKind30_ShardsStep);
    BOF3_INJECT(EffectKind30_ShardTumble);
    BOF3_INJECT(EffectKind30_SparksInit);
    BOF3_INJECT(EffectKind30_SparksDraw);
    BOF3_INJECT(EffectKind30_SparkQuad);
    BOF3_INJECT(EffectKind34_Run);
    BOF3_INJECT(EffectKind34_V0Run);
    BOF3_INJECT(EffectKind34_V0Burst);
    BOF3_INJECT(EffectKind34_V0BurstEnd);
    BOF3_INJECT(EffectKind34_V0DebrisStart);
    BOF3_INJECT(EffectKind34_V0DebrisFly);
    BOF3_INJECT(EffectKind34_V1Run);
    BOF3_INJECT(EffectKind34_V1Start);
    BOF3_INJECT(EffectKind34_V1Fall);
    BOF3_INJECT(EffectKind34_V2Run);
    BOF3_INJECT(EffectKind34_V2Start);
    BOF3_INJECT(EffectKind34_V2Spawn);
    BOF3_INJECT(EffectKind34_V2PieceStart);
    BOF3_INJECT(EffectKind34_V2PieceFall);
    BOF3_INJECT(EffectKind34_V3Run);
    BOF3_INJECT(EffectKind34_V3Start);
    BOF3_INJECT(EffectKind34_V3Fall);
    BOF3_INJECT(EffectKind34_V4Run);
    BOF3_INJECT(EffectKind34_V4Start);
    BOF3_INJECT(EffectKind34_V4Move);
    BOF3_INJECT(EffectKind3A_Run);
    BOF3_INJECT(EffectKind3A_Start);
    BOF3_INJECT(EffectKind3A_Fly);
    BOF3_INJECT(EffectKind3A_Wait);
    BOF3_INJECT(EffectKind3A_LeaderPose);
    BOF3_INJECT(EffectKind3A_Hit);
    BOF3_INJECT(EffectKind41_Run);
    BOF3_INJECT(EffectKind41_Start);
    BOF3_INJECT(EffectKind41_Hold);
    BOF3_INJECT(EffectKind41_Bounce);
    BOF3_INJECT(EffectKind41_Fade);
}
