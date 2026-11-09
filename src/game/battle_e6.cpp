// Round twelve group BE6 (docs/takeover-queue-field-battle.md section 3,
// analysis/round12_cut.tsv's BE6 rows): 39 battle-engine functions of
// 0x451480..0x4552F6 and 0x4CEB40..0x4CF4A4, each read to its last instruction
// with capstone (2026-09-29) and taken through the boss harness's engine frame
// (boss_harness.h, docs/boss_harness.md section 10). docs/battle_e6.md has
// every function one row each.
//
//   - the transformation (DragonForm_*): the chosen genes (0x904B84, count
//     0x904B87) looked up as a recipe of eleven, or - gene 0x10 in a party of
//     three - a pair of the other members' +0x89 bytes, or mixed from the
//     genes' rows; a recipe's or the mix's stats, bytes and abilities written
//     to the actor's record; the history of six gene sets; the stats rebuilt;
//   - the genes' cost (DragonGenes_SumCost) and the history's last step;
//   - three BattleFx_Dispatch tasks (slots 15, 16, 18) and their states;
//   - the stat rebuild from the buffs (Battle_RecalcStats, a member's and an
//     enemy's), a member's roll for an action and its two tests, the AP pop-up;
//   - the Field_Slots release by owner and start (round eleven's two debts);
//   - BMAGIC's four MapCell_Handlers slots (40..43).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original indexes past a table, divides by a byte that can be 0, reads a
// stack byte it never wrote or loops past its record, ours aborts with a
// message (the owner's rule, round9 doc section 6; known-defects D106 for the
// unwritten stack bytes); each is described in docs/battle_e6.md section 7.
// Every call goes through the harness (BH_CALL / BH_AT), so the start-up fuzz
// can stand recorders in for the callees.
#include "game/battle_e6.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e6_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e6::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

U LBattleE6(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
void SetL(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t S8(unsigned char v) { return static_cast<std::int8_t>(v); }
std::int32_t S16BattleE6(unsigned v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
U Addr(const void* p) { return Low32(p); }
float F(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return f;
}
void SetF(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

// Sprite_Current in the effect tasks is the task slot (docs/battle_e6.md 2.3).

// A BMAGIC vertex's height (Quake_VertexLift 0x4CF4B0, R3G's): ax is read.
U CellHeight(std::int32_t x, std::int32_t z) { return BH_AT(U (__cdecl*)(std::int32_t, std::int32_t), at::kCellHeight.v)(x, z); }

// A PSX short vector and matrix as the BMAGIC cells build them on the stack.
struct SVec {
    std::int16_t x, y, z, pad;
};
struct Matrix {
    std::int16_t m[9];
    std::int16_t pad;
    std::int32_t t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a PSX MATRIX");

// A BMAGIC vertex from a record dword: x = x0 + 2 * its byte 3, z = z0 + 2 *
// its byte 2 (16 bits), y = the cell height there + its low word.
SVec CellVertex(U x0, U z0, const unsigned char* at) {
    const U d = LBattleE6(at);
    SVec v{};
    v.x = static_cast<std::int16_t>(x0 + 2 * static_cast<U>(S8(static_cast<unsigned char>(d >> 24))));
    v.y = static_cast<std::int16_t>(z0 + 2 * static_cast<U>(S8(static_cast<unsigned char>(d >> 16))));
    v.z = static_cast<std::int16_t>(CellHeight(v.x, v.y) + Word(at));
    return v;
}
// A spin vertex: three 10-bit fields, bit 9 extending the sign over the upper
// six bits (the first field keeps the dword's bits 30 and 31 when bit 9 is 0).
std::int16_t Field10(U v) {
    if (v & 0x200) v |= 0xFFFFFC00u;
    return static_cast<std::int16_t>(v);
}

using Pers4 = long (__cdecl*)(const SVec*, const SVec*, const SVec*, const SVec*, unsigned char*, unsigned char*, unsigned char*,
                              unsigned char*, long*, long*);
using Pers = long (__cdecl*)(const void*, void*, long*);

// The loop over a record's entries ends when the counter meets byte +2; one
// that never can runs past the record in the original (reading on until it
// faults). Ours stops first.
void CheckEntries(const char* who, const unsigned char* record, unsigned first, unsigned step) {
    const unsigned len = record[2];
    if (len == first) return;
    if (len < first || (len - first) % step != 0)
        bof3::Fatal("%s: record 0x%X's length byte %u is not %u + %u n - the original's loop runs past the record "
                    "(docs/battle_e6.md section 7)",
                    who, (unsigned)Addr(record), len, first, step);
}

}  // namespace

// ============================================================================
// The transformation
// ============================================================================

// ============================================================================
// Three battle effect tasks (BattleFx_Dispatch slots 15, 16, 18;
// Sprite_Current the task slot)
// ============================================================================

// ============================================================================
// Stats, rolls, pop-ups
// ============================================================================

// ============================================================================
// Field_Slots (round eleven's debts)
// ============================================================================

// ============================================================================
// BMAGIC's four map cells (MapCell_Handlers 40..43: DrawLayer_Open's
// (record, byte 1, byte 0)); each draws only when Area_TestCondition(the
// record's word) answers
// ============================================================================

// original 0x4CEB40 (slot 40): quads of five dwords after the record's head,
// the cell at x0 = (b1 - 0x80) << 7, z0 = (b0 - 0x80) << 7 (0x4CF4B0(x0 -
// 0x80, z0) first, unread): each one's first vertex projected
// (Gte_LoadVertex, Gte_Rtps, Gte_StoreScreenXY into the next packet +8); on
// screen (x between the bounds at 0x5C4208 / 0x5C4204, y between 0x5C4200 /
// 0x5C41FC, all exclusive): an FT4 with its three other vertices (Gte_Rtpt),
// the fifth dword its texture, committed to 7 / 4 (texture bit 14, by bit
// 30) or 6 / Draw_OtSlot, 0x48 bytes, then the three screen points and the
// depths.
extern "C" void __cdecl MapCell_DrawTexQuads(const unsigned char* record, unsigned b1, unsigned b0) {
    if (BH_CALL(Area_TestCondition)(Word(record)) == 0) return;
    const U x0 = (b1 - 0x80) << 7, z0 = (b0 - 0x80) << 7;
    CellHeight(static_cast<std::int32_t>(x0 - 0x80), static_cast<std::int32_t>(z0));
    CheckEntries("MapCell_DrawTexQuads", record, 1, 5);
    const unsigned char* p = record + 0x14;
    for (U counter = 1; counter != record[2]; counter += 5, p += 0x14) {
        const SVec first = CellVertex(x0, z0, p - 0x10);
        BH_CALL(Gte_LoadVertex)(reinterpret_cast<const unsigned long*>(&first));
        BH_CALL(Gte_Rtps)();
        unsigned char* const prim = Gfx_PacketNext;
        BH_CALL(Gte_StoreScreenXY)(reinterpret_cast<unsigned long*>(prim + 8));
        // DIV-0041 (UC2): the x bounds moved out by the columns; .rdata's
        // floats stay Capcom's (their code reads them too).
        const float fill = static_cast<float>(Widescreen_Fill());
        if (!(F(prim + 8) > F(At(at::kScreenXLo)) - fill)) continue;
        if (!(F(prim + 8) < F(At(at::kScreenXHi)) + fill)) continue;
        if (!(F(prim + 0xC) > F(At(at::kScreenYLo)))) continue;
        if (!(F(prim + 0xC) < F(At(at::kScreenYHi)))) continue;
        BH_CALL(Gpu_SetPolyFT4)(prim);
        BH_CALL(Gpu_SetShadeTex)(prim, 0);
        SVec v[3];
        for (unsigned k = 0; k < 3; ++k) v[k] = CellVertex(x0, z0, p - 0xC + 4 * k);
        BH_CALL(Gte_LoadVertices3)(reinterpret_cast<const unsigned long*>(v));
        BH_CALL(Prim_SetTexture)(LBattleE6(p), prim, 1);
        BH_CALL(Gte_Rtpt)();
        const U texture = LBattleE6(p);
        unsigned slot;
        if (texture & 0x4000) slot = texture & 0x40000000 ? 7 : 4;
        else slot = texture & 0x40000000 ? 6 : Draw_OtSlot;
        BH_CALL(Gfx_CommitPrim)(slot, 0x48);
        BH_CALL(Gte_StoreScreenXY3)(reinterpret_cast<unsigned long*>(prim + 0x18), reinterpret_cast<unsigned long*>(prim + 0x28),
                                    reinterpret_cast<unsigned long*>(prim + 0x38));
        BH_CALL(Gte_PrimDepths4_10)(prim);
    }
}

// original 0x4CED60 (slot 41): entries of eight dwords after the record's
// head word, the cell as slot 40's: each a draw mode ((its fifth dword >> 24)
// & 3) << 5 | 0x95 committed (0xC to Draw_OtSlot), then a G4 of its four
// vertices (Gte_RotTransPers4 in the order 1, 2, 3, 0 and the depths), semi-
// transparent by the fifth dword's bit 31, the four colours from dwords 5..8,
// committed 0x44. Then draw mode 0x95 committed.
extern "C" void __cdecl MapCell_DrawShadedQuads(const unsigned char* record, unsigned b1, unsigned b0) {
    if (BH_CALL(Area_TestCondition)(Word(record)) == 0) return;
    const U x0 = (b1 - 0x80) << 7, z0 = (b0 - 0x80) << 7;
    CellHeight(static_cast<std::int32_t>(x0 - 0x80), static_cast<std::int32_t>(z0));
    if (record[2] != 1) {
        CheckEntries("MapCell_DrawShadedQuads", record, 1, 8);
        const unsigned char* vertex = record + 4;
        const unsigned char* mode = record + 0x14;
        for (U counter = 1; counter != record[2]; counter += 8, mode += 0x20) {
            BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, (((LBattleE6(mode) >> 24) & 3) << 5) | 0x95, 0);
            BH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0xC);
            unsigned char* const prim = Gfx_PacketNext;
            BH_CALL(Gpu_SetPolyG4)(prim);
            SVec v[4];
            for (unsigned k = 0; k < 4; ++k) v[k] = CellVertex(x0, z0, vertex + 4 * k);
            vertex += 0x10;
            long depth = 0, flag = 0;
            BH_AT(Pers4, bof3::addr::Gte_RotTransPers4)(&v[1], &v[2], &v[3], &v[0], prim + 0x18, prim + 0x28, prim + 0x38, prim + 8,
                                                        &depth, &flag);
            BH_CALL(Gte_StoreDepthF4)(reinterpret_cast<float*>(prim + 0x20), reinterpret_cast<float*>(prim + 0x30),
                                      reinterpret_cast<float*>(prim + 0x40), reinterpret_cast<float*>(prim + 0x10));
            BH_CALL(Gpu_SetSemiTrans)(prim, LBattleE6(vertex) >> 31);
            for (unsigned k = 0; k < 4; ++k) {
                const U colour = LBattleE6(vertex);
                prim[4 + 0x10 * k] = static_cast<unsigned char>(colour >> 16);
                prim[5 + 0x10 * k] = static_cast<unsigned char>(colour >> 8);
                prim[6 + 0x10 * k] = static_cast<unsigned char>(colour);
                vertex += 4;
            }
            BH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0x44);
        }
    }
    BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    BH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0xC);
}

// original 0x4CEFC0 (slot 42): a centre from dword 1 (x and z bytes at 64 a
// cell unit, doubled, and 0x4CF4B0 plus its low word) and a spin from dword 2
// (three 10-bit rates times Frame_Counter & 0xFFF, times 4); then entries of
// six dwords from dword 3: each under a pushed matrix, the centre rotated and
// translated into the matrix (Gte_RotTrans), its angles (the spin plus four
// times its three 10-bit fields, & 0xFFF) made a matrix times Camera_Matrix,
// set; an FT4 of its four sign-extended 10-bit vertices (the order 1, 2, 3,
// 0), its sixth dword the texture, committed 0x48 to Draw_OtSlot; popped.
extern "C" void __cdecl MapCell_DrawSpinQuads(const unsigned char* record, unsigned b1, unsigned b0) {
    if (BH_CALL(Area_TestCondition)(Word(record)) == 0) return;
    const U d1 = LBattleE6(record + 4);
    SVec centre{};
    centre.x = static_cast<std::int16_t>((static_cast<U>(S8(static_cast<unsigned char>(d1 >> 24))) + ((b1 - 0x80) << 6)) << 1);
    centre.y = static_cast<std::int16_t>((static_cast<U>(S8(static_cast<unsigned char>(d1 >> 16))) + ((b0 - 0x80) << 6)) << 1);
    centre.z = static_cast<std::int16_t>(CellHeight(centre.x, centre.y) + Word(record + 4));
    const U t = Frame_Counter & 0xFFF;
    const U d2 = LBattleE6(record + 8);
    const U spin[3] = {((d2 >> 20) & 0x3FF) * t * 4, ((d2 >> 10) & 0x3FF) * t * 4, (d2 & 0x3FF) * t * 4};
    if (record[2] == 3) return;
    CheckEntries("MapCell_DrawSpinQuads", record, 3, 6);
    const unsigned char* p = record + 0xC;
    U counter = 3;
    do {
        BH_CALL(Gte_PushMatrix)();
        const U e = LBattleE6(p);
        const std::int16_t angles[4] = {static_cast<std::int16_t>((spin[0] + ((e >> 20) & 0x3FF) * 4) & 0xFFF),
                                        static_cast<std::int16_t>((spin[1] + ((e >> 10) & 0x3FF) * 4) & 0xFFF),
                                        static_cast<std::int16_t>((spin[2] + (e & 0x3FF) * 4) & 0xFFF), 0};
        Matrix m{};
        BH_CALL(Gte_RotTrans)(&centre.x, reinterpret_cast<long*>(m.t));
        BH_CALL(Gte_RotMatrix)(angles, m.m);
        BH_CALL(Gte_MulMatrix0)(reinterpret_cast<const short*>(At(::bof3::Img(Camera_Matrix_addr))), m.m, m.m);
        BH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
        BH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
        unsigned char* const prim = Gfx_PacketNext;
        BH_CALL(Gpu_SetPolyFT4)(prim);
        BH_CALL(Gpu_SetShadeTex)(prim, 0);
        SVec q[4];
        for (unsigned k = 0; k < 4; ++k) {
            const U v = LBattleE6(p + 4 + 4 * k);
            q[k].x = Field10((v >> 20) & 0xFFFF);
            q[k].y = Field10((v >> 10) & 0x3FF);
            q[k].z = Field10(v & 0x3FF);
            q[k].pad = 0;
        }
        long depth = 0, flag = 0;
        BH_AT(Pers4, bof3::addr::Gte_RotTransPers4)(&q[1], &q[2], &q[3], &q[0], prim + 0x18, prim + 0x28, prim + 0x38, prim + 8,
                                                    &depth, &flag);
        BH_CALL(Gte_StoreDepthF4)(reinterpret_cast<float*>(prim + 0x20), reinterpret_cast<float*>(prim + 0x30),
                                  reinterpret_cast<float*>(prim + 0x40), reinterpret_cast<float*>(prim + 0x10));
        BH_CALL(Prim_SetTexture)(LBattleE6(p + 0x14), prim, 1);
        BH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0x48);
        counter += 6;
        p += 0x18;
        BH_CALL(Gte_PopMatrix)();
    } while (counter != record[2]);
}

// original 0x4CF270 (slot 43): a sprite standing on the ground: the point
// (x and z as slot 42's centre into Prim_VertexScratch, y minus half the
// AreaMap_Elevation there) projected (Gte_RotTransPers into MapView_ScreenXY,
// z its answer); on screen (x from the bound at 0x5C4210 to 0x5C420C, y from
// 0x5C4200 to 0x5C41FC, inclusive) and z not 0: the frame by Frame_Counter
// modulo the record's byte 8 against the thresholds from byte 0xA, the
// corners of that frame's dword (four s8 * 1125 / z around the point), an FT4
// with its texture dword, committed 0x48 to Draw_OtSlot. The original divides
// by byte 8 unchecked: ours aborts on 0.
extern "C" void __cdecl MapCell_DrawGroundSprite(const unsigned char* record, unsigned b1, unsigned b0) {
    if (BH_CALL(Area_TestCondition)(Word(record)) == 0) return;
    unsigned char* const point = At(::bof3::Img(Prim_VertexScratch_addr));
    SetWord(point, (static_cast<U>(S8(static_cast<unsigned char>(LBattleE6(record + 4) >> 24))) + ((b1 - 0x80) << 6)) << 1);
    SetWord(point + 2, (static_cast<U>(S8(static_cast<unsigned char>(LBattleE6(record + 4) >> 16))) + ((b0 - 0x80) << 6)) << 1);
    const long height = BH_CALL(AreaMap_Elevation)(static_cast<long>(static_cast<U>(S16BattleE6(Word(point)) + 0x4000) << 9),
                                                   static_cast<long>(static_cast<U>(S16BattleE6(Word(point + 2)) + 0x4000) << 9));
    SetWord(point + 4, static_cast<U>(-(S16BattleE6(static_cast<U>(height)) / 2)));
    long p = 0;
    unsigned char* const screen = At(::bof3::Img(MapView_ScreenXY_addr));
    const long z = BH_AT(Pers, bof3::addr::Gte_RotTransPers)(point, screen, &p);
    const float fill = static_cast<float>(Widescreen_Fill());   // DIV-0041 (UC2): x out by the columns
    if (!(F(screen) >= F(At(at::kSpriteXLo)) - fill)) return;
    if (F(screen) > F(At(at::kSpriteXHi)) + fill) return;
    if (!(F(screen + 4) >= F(At(at::kScreenYLo)))) return;
    if (F(screen + 4) > F(At(at::kScreenYHi))) return;
    if (z == 0) return;
    const unsigned frames = record[8];
    if (frames == 0)
        bof3::Fatal("MapCell_DrawGroundSprite: record 0x%X's frame count byte +8 is 0 - the original divides by it "
                    "(docs/battle_e6.md section 7)",
                    (unsigned)Addr(record));
    const auto tick = static_cast<std::int32_t>(Frame_Counter % frames);
    unsigned frame = 0;
    while (tick >= static_cast<std::int32_t>(record[0xA + frame])) ++frame;
    unsigned char* const prim = Gfx_PacketNext;
    const unsigned head = (record[9] + 5u) >> 2;
    BH_CALL(Gpu_SetPolyFT4)(prim);
    BH_CALL(Gpu_SetShadeTex)(prim, 0);
    BH_CALL(Gte_PrimDepthFlat4_10)(prim);
    const unsigned char* const corners = record + (head + 2 * frame + 2) * 4;
    const auto zz = static_cast<std::int32_t>(z);
    double x = static_cast<double>(F(screen)) - static_cast<double>(S8(corners[0]) * 1125 / zz);
    SetF(prim + 0x28, static_cast<float>(x));
    SetF(prim + 8, static_cast<float>(x));
    x += static_cast<double>(S8(static_cast<unsigned char>(LBattleE6(corners) >> 8)) * 1125 / zz);
    SetF(prim + 0x38, static_cast<float>(x));
    SetF(prim + 0x18, static_cast<float>(x));
    double y = static_cast<double>(F(screen + 4)) - static_cast<double>(S8(static_cast<unsigned char>(LBattleE6(corners) >> 16)) * 1125 / zz);
    SetF(prim + 0x1C, static_cast<float>(y));
    SetF(prim + 0xC, static_cast<float>(y));
    y += static_cast<double>(S8(static_cast<unsigned char>(LBattleE6(corners) >> 24)) * 1125 / zz);
    SetF(prim + 0x3C, static_cast<float>(y));
    SetF(prim + 0x2C, static_cast<float>(y));
    BH_CALL(Prim_SetTexture)(LBattleE6(record + (head + 2 * frame) * 4 + 0xC), prim, 1);
    BH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0x48);
}

// ============================================================================

void BattleE6_Inject() {
    if (bof3::WantsShadow("battle_e6")) battle_e6::SelfTest();
    BOF3_INJECT(MapCell_DrawTexQuads);
    BOF3_INJECT(MapCell_DrawShadedQuads);
    BOF3_INJECT(MapCell_DrawSpinQuads);
    BOF3_INJECT(MapCell_DrawGroundSprite);
}
