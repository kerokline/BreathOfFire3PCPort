// Group FO of round twelve, wave two - the field engine's resident code in
// 0x5738A0..0x57CD89, taken with the scenario harness's field mode
// (scenario_harness.h). docs/field_o.md has each function, its callers, the
// fuzz and the controls.
//
//   Menu_DrawStatsPanel     0x5738A0  a record's four stats and its trait line
//   Menu_DrawExpPanel       0x573BF0  a record's EXP, or the EXP its next level wants
//   Menu_DrawIconWheel      0x573F70  a kind's name, a turned triangle and up to three icons
//   Menu_DrawTile16         0x574400  one 16 x 8 sprite of the menu page, dimmed or not
//   Menu_DrawEquipCompare   0x574EC0  a record's name, stats, the compare arrows and six items
//   Menu_DrawAbilityPanel   0x575F50  a member's ten abilities with their AP costs
//   Menu_DrawItemPanel      0x5763F0  a category's usable items, kept in 0x6BC760, scrolled
//   Menu_DrawSaveSlot       0x576960  a save slot's number, party icons, name, level and time
//   MoveCmd_OpF9            0x578FA0  op F9: the object eased a step along its facing
//   MoveCmd_Op88            0x5794D0  op 88 by Sprite_Current +4 through 0x663AFC
//   MoveCmd_Op88Start       0x5794F0  its state 0: a tint from 0xC0
//   MoveCmd_Op88Fade        0x579560  its state 1: the tint down to 0x80, then released
//   MoveCmd_Op87            0x5795F0  op 87 by Sprite_Current +4 through 0x663B04
//   MoveCmd_Op87Start       0x579610  its state 0: a tint from 0x80
//   MoveCmd_Op87Fade        0x579690  its state 1: the tint up to 0xC0, then released
//   EventScript_SkipIf      0x579CA0  an F0 / F1 stepped over to past its FE
//   EventOp_3x .. _Ax       0x57A7C0..0x57AEC0  the placement ops 3x, 4x, 7x, 6x, Ax
//   EventCond_*             0x57C1A0..0x57C440  event-script conditions 1, 3..9, 11..16
//   ObjTrio_ClearBit40      0x57C7E0  bit 6 of the three party objects' first byte cleared
//   MoveCmd_OpE9            0x57C8E0  op E9 by Sprite_Current +4 through 0x663B84
//   MoveCmd_OpE9Start/Arc/Kind2/Fall  0x57C920, 0x57CA80, 0x57CBB0, 0x57CCE0  its four states
//   MoveCmd_OpDB            0x57CD40  the object's x and z to the half unit
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz stands
// recorders in for ours as for the originals' copies. No divergence: each is
// a faithful replacement (Menu_DrawSaveSlot reads DIV-0029's byte back, as
// Menu_YesNo reads DIV-0027's). The unchecked indexes the originals make are
// reproduced; where the original would jump to what is not code or divide by
// zero, ours aborts with a message (docs/field_o.md section 6).
#include "game/field_o.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_o_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_o::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Ptr(U a) { return At(a); }
unsigned char* PtrAt(const unsigned char* cell) { return At(static_cast<U>(Long(cell))); }
const char* Str(U a) { return reinterpret_cast<const char*>(At(a)); }
const unsigned char* Text(U a) { return At(a); }
unsigned char* Buf() { return At(at::kTextScratch); }
char* CBuf() { return reinterpret_cast<char*>(At(at::kTextScratch)); }

unsigned char* Record(unsigned id) { return At(bof3::addr::CharacterRecords + (id & 0xFF) * at::kRecordStride); }
unsigned char Style() { return At(at::kStyle)[0]; }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Fam() { return Field_ActiveMember; }

// A signed 16-bit word, as `movsx` / a 16-bit compare reads it.
std::int32_t S16(unsigned v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
std::int32_t Sb(unsigned v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

// `cdq` / `idiv`: the original's signed division. A zero divisor is the
// original's divide fault; ours aborts with a message (round nine's rule).
std::int32_t Idiv(std::int32_t dividend, std::int32_t divisor, const char* who) {
    if (divisor == 0) bof3::Fatal("%s: a division by 0 (the original raises the divide fault here)", who);
    if (dividend == INT32_MIN && divisor == -1) bof3::Fatal("%s: INT_MIN / -1 (the original faults)", who);
    return dividend / divisor;
}

// A handler read in place from a .data table: an entry outside .text is not
// code - the original jumps there; ours aborts (round nine's rule). While the
// fuzz runs, the table holds the harness's recorders, which live outside .text.
U CodeAt(U table, unsigned index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = static_cast<U>(Long(At(cell)));
    if (!scenario_harness::g_active && (entry < at::kTextLo || entry >= at::kTextHi))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X, not code - past its table (the original jumps there)", who, index,
                    (unsigned)entry, (unsigned)cell);
    return entry;
}

// The event ops' count and bank words (event_script.cpp's terms): the word at
// DamageScratch is the next Sprite_Objects entry a placement fills, the word
// after it the animation bank a placement passes on.
short Count() {
    short v;
    std::memcpy(&v, At(bof3::addr::DamageScratch), sizeof v);
    return v;
}
unsigned short Bank() { return Word(At(bof3::addr::DamageScratch) + 2); }
void SetBank(unsigned v) { SetWord(At(bof3::addr::DamageScratch) + 2, v); }
void BumpCount() { SetWord(At(bof3::addr::DamageScratch), static_cast<unsigned>(Count() + 1)); }
unsigned char* Object(int n) { return Sprite_Objects + n * 0xA4; }
// A placement's x or z: the whole part and the half (0x8000) when `half` is not 0.
std::int32_t Coordinate(unsigned char half, unsigned char whole) {
    return static_cast<std::int32_t>((half ? 0x8000u : 0u) | static_cast<U>(whole) << 16);
}
// A float from a whole number, as `fild dword` / `fstp dword` store it.
void PutFloat(unsigned char* at, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

// Crt_sprintf into the text scratch, through the harness.
template <typename... T> void Sprintf(U format, T... v) {
    SH_CALL(Crt_sprintf)(CBuf(), Str(format), v...);
}

}  // namespace

// =============================================================================
// The menu panels
// =============================================================================

// original 0x5738A0: a character record's four stats and its trait line (the
// field menu's panel list, 0x59A065; record the party slot's EffectState id).
// Menu_DrawBox at (x + 5, y + 3), 0x94 x 0x2D; the four labels of 0x66A0F8
// (8 bytes apart) at (x + 0xB | x + 0x53, y + 8 | y + 0x15) through
// Text_DrawAt, each value - the words +0x24, +0x26, +0x2A, +0x28 - through
// Crt_sprintf and Text_DrawFont8 at x + 0x30 / x + 0x78; then, when the trait
// byte +0x1F is not 0xFF, system message 0x111 + trait into text record 0
// (8 long) and message 0x34 at (x + 0xB, y + 0x22); the frame pieces 0x663358,
// sixteen of 0x663384 eight apart, 0x663390.
// As the original has it: the record is the argument's byte, unchecked.
extern "C" void __cdecl Menu_DrawStatsPanel(int x, int y, unsigned record) {
    SH_CALL(Menu_DrawBox)(x + 5, y + 3, 0x94, 0x2D, 0, Style());
    const unsigned char* const rec = Record(record);
    const int left = x + 0xB;
    SH_CALL(Text_DrawAt)(left, y + 8, 0, 0xFF, Text(at::kStatLabels));
    Sprintf(at::kFmtNumber, static_cast<unsigned>(Word(rec + 0x24)));
    SH_CALL(Text_DrawFont8)(x + 0x30, y + 0xA, 0, Buf());
    SH_CALL(Text_DrawAt)(left, y + 0x15, 0, 0xFF, Text(at::kStatLabels + 8));
    Sprintf(at::kFmtNumber, static_cast<unsigned>(Word(rec + 0x26)));
    SH_CALL(Text_DrawFont8)(x + 0x30, y + 0x17, 0, Buf());
    SH_CALL(Text_DrawAt)(x + 0x53, y + 8, 0, 0xFF, Text(at::kStatLabels + 0x10));
    Sprintf(at::kFmtNumber, static_cast<unsigned>(Word(rec + 0x2A)));
    SH_CALL(Text_DrawFont8)(x + 0x78, y + 0xA, 0, Buf());
    SH_CALL(Text_DrawAt)(x + 0x53, y + 0x15, 0, 0xFF, Text(at::kStatLabels + 0x18));
    Sprintf(at::kFmtNumber, static_cast<unsigned>(Word(rec + 0x28)));
    SH_CALL(Text_DrawFont8)(x + 0x78, y + 0x17, 0, Buf());
    const unsigned char trait = rec[0x1F];
    if (trait != 0xFF) {
        SH_CALL(TextRecord_Set)(0, 8, SH_CALL(Msg_SystemPtr)(0x111u + trait));
        SH_CALL(Text_DrawAt)(left, y + 0x22, 0, 0xFF, SH_CALL(Msg_SystemPtr)(0x34));
    }
    SH_CALL(Menu_DrawPieces)(x, y, Text(at::kStatsPiecesA), 0);
    for (unsigned b = 0; b < 0x10; ++b) SH_CALL(Menu_DrawPieces)(x + 8 * static_cast<int>(b), y, Text(at::kStatsPiecesB), 0);
    SH_CALL(Menu_DrawPieces)(x, y, Text(at::kStatsPiecesC), 0);
}

// original 0x573BF0: a record's EXP (next 0) or the EXP its next level wants
// (next not 0: Char_ExpForLevel(record, level + 1), -1 past 99 drawn as the
// text at 0x6639B8). Box (x + 3, y + 3) 0x65 x 0x25; the label 0x663670 at
// x + 0x24, or 0x663674 at x + 0x1E; the value through Text_DrawFont12 at
// (x + 5, y + 0x1A); the pieces 0x6634CC. Callers 0x59A237 (next 0) and
// 0x59A2A7 (next 1), the field menu's panel list.
extern "C" void __cdecl Menu_DrawExpPanel(int x, int y, unsigned record, unsigned next) {
    SH_CALL(Menu_DrawBox)(x + 3, y + 3, 0x65, 0x25, 0, Style());
    const unsigned char* const rec = Record(record);
    const bool want = static_cast<unsigned char>(next) != 0;
    SH_CALL(Text_DrawAt)(x + 0x1E + (want ? 0 : 6), y + 7, 0, 0xFF, Text(want ? at::kExpLabel + 4 : at::kExpLabel));
    const std::int32_t v = want ? SH_CALL(Char_ExpForLevel)(record, static_cast<unsigned char>(rec[0xA] + 1))
                                : Long(rec + 0xC);
    if (v == -1) Sprintf(at::kFmtExpNone);
    else Sprintf(at::kFmtExp, v);
    SH_CALL(Text_DrawFont12)(x + 5, y + 0x1A, 0, Buf());
    SH_CALL(Menu_DrawPieces)(x, y, Text(at::kExpPieces), 1);
}

namespace {

// Menu_DrawIconWheel's shade of a corner by its z: ((3 z + 0x24) << 6) / 24
// (signed, truncating - the imul 0x2AAAAAAB / sar 2 idiom) + 0x3F, a byte.
unsigned char WheelShade(std::int32_t z) {
    const auto v = static_cast<std::int32_t>((static_cast<U>(z) * 3u + 0x24u) << 6);
    return static_cast<unsigned char>(v / 24 + 0x3F);
}

// (c X - s Z) >> 11 and (c Z + s X) >> 12: a point turned by the angle, x
// counted double; the four calls in the original's order.
void Turn(int angle, std::int32_t px, std::int32_t pz, std::int32_t& ox, std::int32_t& oz) {
    const auto c0 = static_cast<U>(SH_CALL(Math_Cos)(angle));
    const auto s0 = static_cast<U>(SH_CALL(Math_Sin)(angle));
    ox = static_cast<std::int32_t>(c0 * static_cast<U>(px) - s0 * static_cast<U>(pz)) >> 11;
    const auto c1 = static_cast<U>(SH_CALL(Math_Cos)(angle));
    const auto s1 = static_cast<U>(SH_CALL(Math_Sin)(angle));
    oz = static_cast<std::int32_t>(c1 * static_cast<U>(pz) + s1 * static_cast<U>(px)) >> 12;
}

}  // namespace

// original 0x573F70: a kind's name, a turned triangle and up to three icons
// (the field menu's panel list, 0x59A46E: the panel's +0xA kind, +0xC the lit
// bits, +0x10 the angle). Box (x + 3, y + 3) 0x45 x 0x30; the name, 28-byte
// entries at 0x6636B0, through Text_DrawSmall at (x + 0x16, y + 0x28); the
// three corners of 0x6637C8 turned by the angle into 0x6BC860.. and drawn as a
// POLY_G3 (0x34 bytes) at (x & 0xFFFF) + x' + 0x26, (y & 0xFFFF) + z' + 0x17,
// each corner shaded by its z; then 1 (kind 0), 2 (kinds 1..3) or 3 icons
// whose spots (the kind's s16 pairs at 0x6636C0) are turned likewise into
// 0x6BC748.., lit (bit j of the lit byte: a Frame_Counter pulse) or 0x70,
// sorted by z (a bubble sort), and drawn with Menu_DrawCell8 (u = the icon's
// index + 3, v 0x1E, CLUT (0x10, 0x1E0), the shade); the frame pieces 0x6633E0,
// six of 0x663404 eight apart, 0x663410.
// As the original has it: the sort's swap moves each coordinate through a
// byte register - the one moved down keeps only its low byte, sign-extended
// (harmless while a turned coordinate fits a signed byte); the kind indexes
// its two 28-byte tables unchecked.
extern "C" void __cdecl Menu_DrawIconWheel(int x, int y, unsigned kind, unsigned lit, int angle) {
    SH_CALL(Menu_DrawBox)(x + 3, y + 3, 0x45, 0x30, 0, Style());
    unsigned char order[3] = {0, 1, 2};
    const unsigned k = kind & 0xFF;
    SH_CALL(Text_DrawSmall)(x + 0x16, y + 0x28, 0, 0xFF, Text(at::kWheelNames + k * 28));
    const unsigned char kb = static_cast<unsigned char>(kind);
    const unsigned char count = kb == 0 ? 1 : kb <= 3 ? 2 : 3;
    for (unsigned i = 0; i < 3; ++i) {
        std::int32_t ox, oz;
        Turn(angle, Long(At(at::kWheelTriangle + 8 * i)), Long(At(at::kWheelTriangle + 8 * i + 4)), ox, oz);
        SetLong(At(at::kWheelFrame + 8 * i), ox);
        SetLong(At(at::kWheelFrame + 8 * i + 4), oz);
    }
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG3)(prim);
    const std::int32_t bx = static_cast<std::int32_t>(static_cast<U>(x) & 0xFFFF);
    const std::int32_t by = static_cast<std::int32_t>(static_cast<U>(y) & 0xFFFF);
    const unsigned char* const f = At(at::kWheelFrame);
    PutFloat(prim + 8, bx + Long(f) + 0x26);
    PutFloat(prim + 0xC, by + Long(f + 4) + 0x17);
    PutFloat(prim + 0x18, bx + Long(f + 8) + 0x26);
    PutFloat(prim + 0x1C, by + Long(f + 0xC) + 0x17);
    PutFloat(prim + 0x28, bx + Long(f + 0x10) + 0x26);
    PutFloat(prim + 0x2C, by + Long(f + 0x14) + 0x17);
    prim[6] = 0;
    prim[5] = prim[4] = WheelShade(Long(f + 4));
    prim[0x16] = 0;
    prim[0x15] = prim[0x14] = WheelShade(Long(f + 0xC));
    prim[0x26] = 0;
    prim[0x25] = prim[0x24] = WheelShade(Long(f + 0x14));
    SH_CALL(Gfx_CommitPrim)(1, 0x34);
    unsigned char colour[3] = {};
    unsigned char mask = 1;
    for (unsigned j = 0; j < count; ++j, mask = static_cast<unsigned char>(mask << 1)) {
        if (static_cast<unsigned char>(lit) & mask) {
            const auto fc = static_cast<unsigned char>(Frame_Counter);
            colour[j] = static_cast<unsigned char>(((fc & 8 ? fc : ~fc) & 6) << 5) + 0x3F;
        } else {
            colour[j] = 0x70;
        }
        const unsigned char* const spot = At(at::kWheelIcons + k * 28 + 4 * j);
        std::int32_t ox, oz;
        Turn(angle, S16(Word(spot)), S16(Word(spot + 2)), ox, oz);
        SetLong(At(at::kWheelSpots + 8 * j), ox);
        SetLong(At(at::kWheelSpots + 8 * j + 4), oz);
    }
    for (unsigned i = 0; static_cast<int>(i) < count - 1; ++i) {
        for (unsigned j = 0; static_cast<int>(j) < count - static_cast<int>(i) - 1; ++j) {
            unsigned char* const a = At(at::kWheelSpots + 8 * j);
            if (Long(a + 4) <= Long(a + 0xC)) continue;
            const std::int32_t x0 = Long(a), z0 = Long(a + 4);
            SetLong(a, Long(a + 8));
            SetLong(a + 8, Sb(static_cast<U>(x0)));
            SetLong(a + 4, Long(a + 0xC));
            SetLong(a + 0xC, Sb(static_cast<U>(z0)));
            const unsigned char o = order[j];
            order[j] = order[j + 1];
            order[j + 1] = o;
            const unsigned char c = colour[j];
            colour[j] = colour[j + 1];
            colour[j + 1] = c;
        }
    }
    for (unsigned j = 0; j < count; ++j) {
        const unsigned clut = SH_CALL(Gpu_GetClut)(0x10, 0x1E0);
        const unsigned char* const spot = At(at::kWheelSpots + 8 * j);
        const auto sx = static_cast<unsigned>(static_cast<std::uint16_t>(Word(spot) + static_cast<unsigned>(x)) + 0x23u);
        const auto sy = static_cast<unsigned>(static_cast<std::uint16_t>(Word(spot + 4) + static_cast<unsigned>(y)) + 0xFu);
        SH_CALL(Menu_DrawCell8)(sx, sy, static_cast<unsigned char>(order[j] + 3), 0x1E, clut, colour[j]);
    }
    SH_CALL(Menu_DrawPieces)(x, y, Text(at::kWheelPiecesA), 0);
    for (unsigned b = 0; b < 6; ++b) SH_CALL(Menu_DrawPieces)(x + 8 * static_cast<int>(b), y, Text(at::kWheelPiecesB), 0);
    SH_CALL(Menu_DrawPieces)(x, y, Text(at::kWheelPiecesC), 0);
}

// original 0x574400: one 16 x 8 SPRT of the menu page (CLUT 0x7800, v 0xD8, u
// the argument << 4), grey 0x80 or dimmed to 0x10, semi-transparent, after a
// draw mode (texture page 0x2F). Callers: the shop's 0x581300 (FS, three
// sites) and the field menu's 0x59AA80 (three).
extern "C" void __cdecl Menu_DrawTile16(int x, int y, unsigned u, unsigned dim) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x2F, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SetWord(p + 0x18, 0x10);
    SetWord(p + 0x1A, 8);
    SetWord(p + 0x16, 0x7800);
    const unsigned char shade = static_cast<unsigned char>(dim) ? 0x10 : 0x80;
    p[6] = p[5] = p[4] = shade;
    PutFloat(p + 8, static_cast<std::int32_t>(static_cast<U>(x) & 0xFFFF));
    p[0x15] = 0xD8;
    PutFloat(p + 0xC, static_cast<std::int32_t>(static_cast<U>(y) & 0xFFFF));
    p[0x14] = static_cast<unsigned char>(u << 4);
    SH_CALL(Gpu_SetSprt)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x574EC0: the equipment screen's record panel (the field menu's
// 0x59A91E and 0x5969CC; the shop's): box (x + 4, y + 4) 0x78 x 0xA3, the name
// (record +0, five characters) at (x + 0x20, y + 7), the four stat labels at
// x + 5 and values at x + 0x31, 13 apart from y + 0x1A; when no_preview's byte
// is 0, Equip_PreviewSet(record, set, marks, values) and for each stat an
// arrow cell (Menu_DrawCell8, u 0x11 for mark 4, 0x12 for mark 1, else 0x13)
// at x + 0x51 and the previewed value in the mark's colour at x + 0x59; then
// the six equipment names (weapon, three armour, two accessories: the name
// tables by record +0x12..+0x17) with their icons (0x66386C), the panel's +0xB
// slot raised in colours 7 / 2, its +0xA slot in 7 / 0, the rest in 0; the
// frame pieces.
// As the original has it: the six names are the tables' entries by bytes
// unchecked; the panel's +0xB and +0xA are read again for every slot.
extern "C" void __cdecl Menu_DrawEquipCompare(unsigned record, int x, int y, const unsigned char* set, unsigned no_preview,
                                              const unsigned char* panel) {
    SH_CALL(Menu_DrawBox)(x + 4, y + 4, 0x78, 0xA3, 0, Style());
    const unsigned char* const rec = Record(record);
    SH_CALL(Text_DrawAt)(x + 0x20, y + 7, 0, 5, rec);
    for (unsigned i = 0; i < 4; ++i)
        SH_CALL(Text_DrawAt)(x + 5, y + 0x1A + 13 * static_cast<int>(i), 0, 4, Text(at::kStatLabels + 8 * i));
    static const unsigned char kStats[4] = {0x24, 0x26, 0x2A, 0x28};
    for (unsigned i = 0; i < 4; ++i) {
        Sprintf(at::kFmtNumber, static_cast<unsigned>(Word(rec + kStats[i])));
        SH_CALL(Text_DrawFont8)(x + 0x31, y + 0x1C + 13 * static_cast<int>(i), 0, Buf());
    }
    if (static_cast<unsigned char>(no_preview) == 0) {
        unsigned char marks[4];
        unsigned short values[4];
        SH_CALL(Equip_PreviewSet)(record, set, marks, values);
        for (unsigned i = 0; i < 4; ++i) {
            const unsigned char m = marks[i];
            const unsigned char u = m == 4 ? 0x11 : m == 1 ? 0x12 : 0x13;
            SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xF, 0);
            SH_CALL(Gfx_CommitPrim)(1, 0xC);
            const int row = y + 13 * static_cast<int>(i) + 0x1C;
            const unsigned clut = SH_CALL(Gpu_GetClut)(0, 0x1E0);
            SH_CALL(Menu_DrawCell8)(static_cast<unsigned>(x + 0x51), static_cast<unsigned>(row), u, 0x1C, clut, 0x80);
            Sprintf(at::kFmtNumber, static_cast<unsigned>(values[i]));
            SH_CALL(Text_DrawFont8)(x + 0x59, row, marks[i], Buf());
        }
    }
    const unsigned char* names[6] = {
        At(bof3::addr::NameTable_Weapons + rec[0x12] * 28u), At(bof3::addr::NameTable_Armour + rec[0x13] * 26u),
        At(bof3::addr::NameTable_Armour + rec[0x14] * 26u),  At(bof3::addr::NameTable_Armour + rec[0x15] * 26u),
        At(bof3::addr::NameTable_Accessories + rec[0x16] * 24u), At(bof3::addr::NameTable_Accessories + rec[0x17] * 24u),
    };
    for (unsigned b = 0; b < 6; ++b) {
        const int row = y + 13 * static_cast<int>(b);
        const unsigned char icon = At(at::kEquipIcons)[b];
        const unsigned char* const name = names[b];
        const bool raised = b == panel[0xB] || b == panel[0xA];
        if (raised) {
            const unsigned char under = b == panel[0xB] ? 2 : 0;
            SH_CALL(Menu_DrawIcon8)(x + 6, row + 0x57, icon, 1);
            SH_CALL(Text_DrawAt)(x + 0x11, row + 0x55, 7, SH_CALL(Text_CharCount)(name), name);
            SH_CALL(Menu_DrawIcon8)(x + 6, row + 0x55, At(at::kEquipIcons)[b], 0);
            SH_CALL(Text_DrawAt)(x + 0x11, row + 0x53, under, SH_CALL(Text_CharCount)(name), name);
        } else {
            SH_CALL(Menu_DrawIcon8)(x + 6, row + 0x57, icon, 0);
            SH_CALL(Text_DrawAt)(x + 0x11, row + 0x55, 0, SH_CALL(Text_CharCount)(name), name);
        }
    }
    SH_CALL(Menu_DrawPieces)(x, y, Text(at::kEquipPieces), 1);
    for (unsigned b = 0; b < 7; ++b) {
        const int row = y + 8 * static_cast<int>(b) + 0x18;
        SH_CALL(Menu_DrawPiece)(x, row, 4, 1);
        SH_CALL(Menu_DrawPiece)(x + 0x78, row, 8, 1);
    }
    for (unsigned b = 0; b < 9; ++b) {
        const int row = y + 8 * static_cast<int>(b) + 0x58;
        SH_CALL(Menu_DrawPiece)(x, row, 4, 1);
        SH_CALL(Menu_DrawPiece)(x + 0x78, row, 8, 1);
    }
    const int mid = y + 0x50, bottom = y + 0xA0;
    for (unsigned b = 0; b < 0xE; ++b) {
        const int column = x + 8 * static_cast<int>(b) + 8;
        SH_CALL(Menu_DrawPiece)(column, mid, 0xE, 1);
        SH_CALL(Menu_DrawPiece)(column, bottom, 0x11, 1);
    }
    SH_CALL(Menu_DrawPiece)(x, mid, 0xD, 1);
    SH_CALL(Menu_DrawPiece)(x + 0x78, mid, 0xF, 1);
    SH_CALL(Menu_DrawPiece)(x, bottom, 0x10, 1);
    SH_CALL(Menu_DrawPiece)(x + 0x78, bottom, 0x12, 1);
}

namespace {

// A panel's x and y, the words +4 / +6, read afresh at each use as the
// original reads them.
int PanelX(const unsigned char* panel) { return static_cast<int>(Word(panel + 4)); }
int PanelY(const unsigned char* panel) { return static_cast<int>(Word(panel + 6)); }
// The member id of the menu's shown party slot: MoveScript_EffectState by the
// party list's byte at the slot (the slot a signed byte).
unsigned char ShownMember() {
    const unsigned char id = At(at::kPartyList + Sb(At(at::kShownMember)[0]))[0];
    return MoveScript_EffectState[id];
}
// A title of a table of pointers, centred on 13 six-unit columns.
int Centred(int x, unsigned n) { return x + 6 * (0xD - static_cast<int>(n)); }

}  // namespace

// original 0x575F50: the ability panel (the field menu's 0x59A9D9; panel +4 /
// +6 x and y, +8 the Skill_CanUse mode, +9 the arrows' lit bits and a
// countdown in its high nibble, +0xA the member, +0xB the list's type, +0xC /
// +0xD the rows drawn raised, +0x10 a word: the top arrows). Box (x + 3,
// y + 3) 0x9A x 0xA0; Char_AbilityList(member, type, 0)'s ten ids from
// y + 0x1A, 13 apart, through Menu_DrawSkillRow at x + 7: colour 7 for one
// Skill_CanUse refuses (dimmed), 2 on row +0xD, else 0; the rows +0xC / +0xD
// drawn raised (a copy in colour 7 unless already 7, then at y - 2); the title
// 0x663984[type] centred at y + 7; the arrows (0x66349C lit or 0x663484,
// 0x6634C0 lit or 0x6634B4), +9's countdown stepped (its high nibble less 1, or
// the byte 0); the frame pieces.
// As the original has it: the type indexes the title table unchecked, the
// shown slot 0x929F06 is a signed byte into the party list.
extern "C" void __cdecl Menu_DrawAbilityPanel(unsigned char* panel) {
    SH_CALL(Menu_DrawBox)(PanelX(panel) + 3, PanelY(panel) + 3, 0x9A, 0xA0, 0, Style());
    const unsigned char* list = SH_CALL(Char_AbilityList)(panel[0xA], panel[0xB], 0);
    int row = PanelY(panel) + 0x1A;
    for (unsigned b = 0; b < 0xA; ++b, row += 0xD, ++list) {
        const unsigned char id = list[0];
        if (id == 0) continue;
        const unsigned char can = SH_CALL(Skill_CanUse)(panel[8], At(at::kShownMember)[0], id);
        const unsigned char dim = can == 0;
        unsigned char colour = dim ? 7 : 0;
        if (b == panel[0xD]) colour = 2;
        const unsigned char kind = SH_CALL(Skill_FlagIndex)(list[0]);
        const unsigned char* const rec = At(bof3::addr::Ability_Records + list[0] * 24u);
        const unsigned char cost = SH_CALL(Skill_ApCost)(ShownMember(), list[0], 0);
        if (b != panel[0xD] && b != panel[0xC]) {
            SH_CALL(Menu_DrawSkillRow)(PanelX(panel) + 7, row, colour, kind, rec, cost, dim);
            continue;
        }
        if (colour != 7) SH_CALL(Menu_DrawSkillRow)(PanelX(panel) + 7, row, 7, kind, rec, cost, 1);
        SH_CALL(Menu_DrawSkillRow)(PanelX(panel) + 7, row - 2, colour, kind, rec, cost, dim);
    }
    const char* const title = Str(static_cast<U>(Long(At(at::kAbilityTitles + panel[0xB] * 4u))));
    const int ty = PanelY(panel) + 7;
    SH_CALL(Text_DrawAt)(Centred(PanelX(panel), static_cast<unsigned>(std::strlen(title))), ty, 0, 0x10,
                         reinterpret_cast<const unsigned char*>(title));
    SH_CALL(Menu_DrawPieces)(PanelX(panel), PanelY(panel), Text(panel[9] & 2 ? at::kListPiecesLit : at::kListPiecesA), 1);
    SH_CALL(Menu_DrawPieces)(PanelX(panel), PanelY(panel), Text(panel[9] & 1 ? at::kListPiecesBLit : at::kListPiecesB), 1);
    const unsigned char arrows = panel[9];
    panel[9] = arrows & 0xF0 ? static_cast<unsigned char>(arrows - 0x10) : 0;
    for (unsigned b = 0; b < 0xA; ++b) SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8 * static_cast<int>(b) + 0x28, PanelY(panel), 1, 1);
    for (unsigned b = 0; b < 0x11; ++b) SH_CALL(Menu_DrawPiece)(PanelX(panel), PanelY(panel) + 8 * static_cast<int>(b) + 0x18, 4, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel) + 0x18, 0x20, 1);
    for (unsigned b = 0; b < 0xF; ++b)
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel) + 8 * static_cast<int>(b) + 0x20, 0x21, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel), PanelY(panel) + 0xA0, 5, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel) + 0x98, 0x22, 1);
    if (Word(panel + 0x10) != 0) {
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8, PanelY(panel), 0x32, 1);
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel), 0x34, 1);
    }
    for (unsigned b = 0; b < 0x11; ++b)
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8 * static_cast<int>(b) + 8, PanelY(panel) + 0xA0, 6, 1);
}

// original 0x5763F0: the item panel (the field menu's 0x59AA29 and 0x596999;
// panel +8 the category, +9 the filter, +0xA the list's top, +0xB the cursor,
// +0xC the shown slot, +0xD the id under the cursor, +0x10 the scroll state).
// The category from +9: 1..3 -> 2, 4..5 -> 3, else 1; of its 0x80 ids and
// counts (Inventory_IdLists / Inventory_CountLists) those Item_CanUse(4,
// member, category, id) passes - and, for category 2, whose Item_IconKind
// less 2 is the filter less 1 - kept in order at 0x6BC760 / 0x6BC7E0 and the
// rest of both lists zeroed; +0xD the kept id under the cursor; box (x + 3,
// y + 3) 0x99 x 0x9A; Menu_ListScroll(top, offset, moving, state); rows
// moving + 9 from the kept list at its answer, y + offset + 0x1A, 13 apart,
// through Menu_DrawItemRow at x + 7 (the cursor's row raised: colour 7
// dimmed, then 0 at y - 2); two boxes, the title 0x663994[category] centred,
// "kept / 128" (0x6639C8) at (x + 0x55, y + 0x92), the frame pieces and
// Menu_DrawScrollBar over the kept ids.
// As the original has it: the rows past the list's 0x80 read on into .data;
// the cursor +0xB indexes the kept ids unchecked.
extern "C" void __cdecl Menu_DrawItemPanel(unsigned char* panel) {
    switch (panel[9]) {
    case 1: case 2: case 3: panel[8] = 2; break;
    case 4: case 5: panel[8] = 3; break;
    default: panel[8] = 1; break;
    }
    const unsigned cat = panel[8];
    const unsigned char* ids = At(static_cast<U>(Long(At(bof3::addr::Inventory_IdLists + 4 * cat))));
    const unsigned char* counts = At(static_cast<U>(Long(At(bof3::addr::Inventory_CountLists + 4 * cat))));
    unsigned char kept = 0, total = 0;
    for (unsigned n = 0; n < 0x80; ++n, ++ids, ++counts) {
        const unsigned char member = MoveScript_EffectState[At(at::kPartyList + panel[0xC])[0]];
        if (!SH_CALL(Item_CanUse)(4, member, panel[8], ids[0])) continue;
        if (panel[8] == 2) {
            const U kindv = SH_CALL(Item_IconKind)(2, ids[0]) & 0xFF;
            if (static_cast<U>(panel[9]) - 1u != kindv - 2u) continue;
        }
        At(at::kListIds)[kept] = ids[0];
        At(at::kListCounts)[kept] = counts[0];
        ++total;
        ++kept;
    }
    if (kept < 0x80) {
        std::memset(At(at::kListCounts + kept), 0, 0x80u - kept);
        std::memset(At(at::kListIds + kept), 0, 0x80u - kept);
    }
    panel[0xD] = At(at::kListIds)[panel[0xB]];
    SH_CALL(Menu_DrawBox)(PanelX(panel) + 3, PanelY(panel) + 3, 0x99, 0x9A, 0, Style());
    unsigned char offset = 0, moving = 0;
    const unsigned char start = SH_CALL(Menu_ListScroll)(panel + 0xA, &offset, &moving, panel + 0x10);
    int row = PanelY(panel) + Sb(offset) + 0x1A;
    const unsigned char* item = At(at::kListIds + start);
    const std::int32_t to_counts = static_cast<std::int32_t>(at::kListCounts - at::kListIds);
    for (unsigned j = 0; static_cast<int>(j) < moving + 9; ++j, ++item, row += 0xD) {
        const unsigned char id = item[0];
        if (id == 0) continue;
        if (panel[0xA] + j == panel[0xB]) {
            SH_CALL(Menu_DrawItemRow)(PanelX(panel) + 7, row, 7, panel[8], id, item[to_counts], 1);
            SH_CALL(Menu_DrawItemRow)(PanelX(panel) + 7, row - 2, 0, panel[8], item[0], item[to_counts], 0);
        } else {
            SH_CALL(Menu_DrawItemRow)(PanelX(panel) + 7, row, 0, panel[8], id, item[to_counts], 0);
        }
    }
    SH_CALL(Menu_DrawBox)(PanelX(panel) + 3, PanelY(panel) + 3, 0x99, 0x14, 0, Style());
    SH_CALL(Menu_DrawBox)(PanelX(panel) + 3, PanelY(panel) + 0x92, 0x99, 8, 0, Style());
    const unsigned category = panel[8];
    const unsigned char* const title = Text(static_cast<U>(Long(At(at::kItemTitles + category * 4u))));
    const int ty = PanelY(panel) + 7;
    SH_CALL(Text_DrawAt)(Centred(PanelX(panel), SH_CALL(Text_CharCount)(title)), ty, 0, 0x10, title);
    Sprintf(at::kFmtCount, static_cast<unsigned>(total), 0x80u);
    SH_CALL(Text_DrawFont8)(PanelX(panel) + 0x55, PanelY(panel) + 0x92, 0, Buf());
    SH_CALL(Menu_DrawPieces)(PanelX(panel), PanelY(panel), Text(at::kListPiecesA), 1);
    SH_CALL(Menu_DrawPieces)(PanelX(panel), PanelY(panel), Text(at::kItemPiecesB), 1);
    for (unsigned b = 0; b < 0xA; ++b) SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8 * static_cast<int>(b) + 0x28, PanelY(panel), 1, 1);
    for (unsigned b = 0; b < 0xF; ++b) SH_CALL(Menu_DrawPiece)(PanelX(panel), PanelY(panel) + 8 * static_cast<int>(b) + 0x18, 4, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel) + 0x18, 0x16, 1);
    for (unsigned b = 0; b < 0xC; ++b)
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel) + 8 * static_cast<int>(b) + 0x28, 0x17, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel) + 0x88, 0x18, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel), PanelY(panel) + 0x90, 0x19, 1);
    for (unsigned b = 0; b < 9; ++b)
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8 * static_cast<int>(b) + 8, PanelY(panel) + 0x90, 0x1A, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x50, PanelY(panel) + 0x90, 0x1B, 1);
    for (unsigned b = 0; b < 6; ++b)
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8 * static_cast<int>(b) + 0x58, PanelY(panel) + 0x90, 0x1C, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x88, PanelY(panel) + 0x90, 0x1D, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8, PanelY(panel), 0x32, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel), 0x33, 1);
    SH_CALL(Menu_DrawScrollBar)(At(at::kListIds), panel[0xA], PanelX(panel) + 0x90, PanelY(panel) + 0x18, 9, 0x80, 0x74);
}

// original 0x576960: a save or load slot (SaveMenu_DrawSlots 0x588D6B; the
// save header's summary, or 0 for an empty slot). Box (x + 8, y) 0xC8 x 0x30,
// the slot's number (0x6639D4) at (x + 9, y + 5); with a summary: a draw mode,
// the party's three icons (+5..+7, 0xFF none) through Menu_DrawBigIcon at
// x + 0x56, 41 apart, the name - +0..+4 and the four bytes +0x16 copied to
// 0x904BA0 - through Text_DrawAt (five characters) at (x + the inset, y + 2),
// the level +8 (0x64D3EC) at (x + 0x1F, y + 0x18), the time +0xC / +0xD
// (0x654830) and two colons (0x6639D0) at x + 0x27, Menu_DrawExpBar(x + 0x14,
// y + 0x12, 0, level, +0x10); without one the text 0x669CCC at (x + 0x13,
// y + 2). Then the frame pieces 0x663584 (flags 2 when +0x14 is set, else 0)
// and, with +0x14 set, piece 0x3D at (x + 0x32, y + 0x16).
// The inset is the disp8 at 0x576A48 read back - 0x13 as Capcom shipped it,
// 0x15 under DIV-0029 (YesNoLayout_Inject patches those bytes), anything else
// a Fatal - as Menu_YesNo reads DIV-0027's patch back.
extern "C" void __cdecl Menu_DrawSaveSlot(unsigned slot, int x, int y, const unsigned char* summary) {
    SH_CALL(Menu_DrawBox)(x + 8, y, 0xC8, 0x30, 0, Style());
    Sprintf(at::kFmtSlot, slot & 0xFF);
    SH_CALL(Text_DrawFont8)(x + 9, y + 5, 0, Buf());
    unsigned flags = 0;
    if (summary != nullptr) {
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xF, 0);
        SH_CALL(Gfx_CommitPrim)(1, 0xC);
        for (unsigned b = 0; b < 3; ++b) {
            const unsigned char icon = summary[5 + b];
            if (icon != 0xFF) SH_CALL(Menu_DrawBigIcon)(x + 41 * static_cast<int>(b) + 0x56, y + 2, icon, 0);
        }
        std::memcpy(Buf(), summary, 4);
        Buf()[4] = summary[4];
        std::memcpy(Buf() + 5, summary + 0x16, 4);
        const unsigned char inset = At(at::kNameInsetSite)[0];
        if (inset != 0x13 && inset != 0x15)
            bof3::Fatal("Menu_DrawSaveSlot: the name's inset byte at 0x%X is 0x%02X, neither Capcom's 0x13 nor DIV-0029's 0x15",
                        (unsigned)at::kNameInsetSite, (unsigned)inset);
        SH_CALL(Text_DrawAt)(x + inset, y + 2, 0, 5, Buf());
        Sprintf(at::kFmtLevel, static_cast<unsigned>(summary[8]));
        SH_CALL(Text_DrawFont8)(x + 0x1F, y + 0x18, 0, Buf());
        Sprintf(at::kFmtTime, static_cast<unsigned>(summary[0xC]), static_cast<unsigned>(summary[0xD]));
        SH_CALL(Text_DrawFont8)(x + 0x27, y + 0x25, 0, Buf());
        SH_CALL(Text_DrawFont8)(x + 0x27, y + 0x24, 0, Text(at::kColon));
        SH_CALL(Text_DrawFont8)(x + 0x27, y + 0x27, 0, Text(at::kColon));
        SH_CALL(Menu_DrawExpBar)(x + 0x14, y + 0x12, 0, summary[8], static_cast<U>(Long(summary + 0x10)));
        flags = summary[0x14] ? 2 : 0;
    } else {
        SH_CALL(Text_DrawAt)(x + 0x13, y + 2, 0, 5, Text(at::kEmptySlot));
    }
    SH_CALL(Menu_DrawPieces)(x, y, Text(at::kSlotPieces), static_cast<int>(flags));
    if (summary != nullptr && summary[0x14] != 0) SH_CALL(Menu_DrawPiece)(x + 0x32, y + 0x16, 0x3D, 0);
}

// =============================================================================
// The movement script's ops
// =============================================================================

// original 0x578FA0: op F9 a b (MoveScript_GroupF 0x57798A repeats the op
// while eax is not 0). The step per frame is 0x6696DC's (x, z) pair by the
// facing Sprite_Current +8 times the speed Field_MoveSpeeds[Field_ActiveMember
// +0x84]; the height +0x3E from AreaMap_Elevation. With the acceleration pair
// +0x18 / +0x1C both 0 (the op's first frame): the target (+0x8C / +0x90 of
// Field_ActiveMember) is Field_DirectionSteps' pair * 2 * b past the position,
// and toward a: the velocity +0xC / +0x10 from 0 with the step >> b as the
// acceleration; else from the step with minus that; +0x14 0. Every frame the
// velocity is added to the position and the acceleration to the velocity; a
// party object (+6 0x0A) moves its member (Party_MoveMember) when x or z
// crosses a half unit and runs its party record's tilt counts (+1 / +2) into
// +0x64..+0x6C. Reached (both coordinates at or past the target along the
// step's sign): the target taken, the velocity the step, the acceleration 0,
// eax 0; else eax 1.
// As the original has it: the speed is a whole byte into Field_MoveSpeeds'
// six; the party slot is (Field_ActiveMember - Sprite_ObjectsExtra) / 0xA4 as
// a signed byte into the party records, unchecked.
extern "C" int __cdecl MoveCmd_OpF9(unsigned char toward, unsigned char shift) {
    const unsigned char speed = Field_MoveSpeeds[Fam()[0x84]];
    unsigned char* sc = Sc();
    const unsigned dir = sc[8] & 7u;
    const auto vx = static_cast<std::int32_t>(static_cast<U>(Long(At(at::kDirectionVelocity + 8 * dir))) * speed);
    const auto vz = static_cast<std::int32_t>(static_cast<U>(Long(At(at::kDirectionVelocity + 8 * dir + 4))) * speed);
    const long ground = SH_CALL(AreaMap_Elevation)(Long(sc + 0x34), Long(sc + 0x38));
    SetWord(Sc() + 0x3E, static_cast<U>(ground));
    sc = Sc();
    const unsigned s = shift & 31u;
    if (static_cast<U>(Long(sc + 0x1C)) + static_cast<U>(Long(sc + 0x18)) == 0) {
        const unsigned d = sc[8] & 7u;
        const std::int32_t x0 = Long(sc + 0x34);
        SetLong(Fam() + 0x8C, static_cast<std::int32_t>((static_cast<U>(Field_DirectionSteps[2 * d]) << 1) * shift + static_cast<U>(x0)));
        const unsigned d2 = Sc()[8] & 7u;
        const std::int32_t z0 = Long(Sc() + 0x38);
        SetLong(Fam() + 0x90, static_cast<std::int32_t>((static_cast<U>(Field_DirectionSteps[2 * d2 + 1]) << 1) * shift + static_cast<U>(z0)));
        if (toward) {
            SetLong(Sc() + 0x18, vx >> s);
            SetLong(Sc() + 0x1C, vz >> s);
            SetLong(Sc() + 0xC, 0);
            SetLong(Sc() + 0x10, 0);
        } else {
            SetLong(Sc() + 0x18, static_cast<std::int32_t>(0u - static_cast<U>(vx >> s)));
            SetLong(Sc() + 0x1C, static_cast<std::int32_t>(0u - static_cast<U>(vz >> s)));
            SetLong(Sc() + 0xC, vx);
            SetLong(Sc() + 0x10, vz);
        }
        SetLong(Sc() + 0x14, 0);
    }
    const auto add = [](unsigned char* to, const unsigned char* by) {
        SetLong(to, static_cast<std::int32_t>(static_cast<U>(Long(to)) + static_cast<U>(Long(by))));
    };
    add(Sc() + 0x34, Sc() + 0xC);
    add(Sc() + 0x38, Sc() + 0x10);
    add(Sc() + 0xC, Sc() + 0x18);
    add(Sc() + 0x10, Sc() + 0x1C);
    sc = Sc();
    unsigned char* fam = Fam();
    if (sc[6] == 0x0A) {
        const std::int32_t slot = static_cast<std::int32_t>(static_cast<U>(Key(fam)) - static_cast<U>(Key(Sprite_ObjectsExtra))) / 0xA4;
        const auto crossed = [](const unsigned char* pos, const unsigned char* vel) {
            const U p = static_cast<U>(Long(pos));
            return (((static_cast<U>(Long(vel)) + p) ^ p) & 0x8000u) != 0;
        };
        if (crossed(sc + 0x34, sc + 0xC) || crossed(sc + 0x38, sc + 0x10)) {
            SH_CALL(Party_MoveMember)(fam, static_cast<unsigned char>(sc[8] & 7));
            fam = Fam();
            sc = Sc();
        }
        unsigned char* const r = MoveScript_PartyRecords + Sb(static_cast<U>(slot)) * 16;
        if (r[1] != 0) {
            r[1] = static_cast<unsigned char>(r[1] - 1);
            add(sc + 0x64, r + 4);
            add(Sc() + 0x68, r + 8);
            fam = Fam();
            sc = Sc();
        }
        if (r[2] != 0) {
            r[2] = static_cast<unsigned char>(r[2] - 1);
            add(sc + 0x6C, r + 0xC);
            fam = Fam();
            sc = Sc();
        }
    }
    const std::int32_t tx = Long(fam + 0x8C);
    const std::int32_t x = Long(sc + 0x34);
    const bool at_x = vx < 0 ? !(x > tx) : !(x < tx);
    const std::int32_t z = Long(sc + 0x38);
    const bool at_z = vz < 0 ? !(z > Long(fam + 0x90)) : !(z < Long(fam + 0x90));
    if (!(at_x && at_z)) return 1;
    SetLong(sc + 0x34, tx);
    SetLong(Sc() + 0x38, Long(Fam() + 0x90));
    SetLong(Sc() + 0xC, vx);
    SetLong(Sc() + 0x10, vz);
    SetLong(Sc() + 0x1C, 0);
    SetLong(Sc() + 0x18, 0);
    return 0;
}

// original 0x5794D0: op 88 (MoveScript_Group8 0x578906): Sprite_Current +4
// picks the state from 0x663AFC - MoveCmd_Op88Start, MoveCmd_Op88Fade.
// As the original has it: the byte is unchecked - 2 and 3 reach op 87's
// states (the next table); past them the words are not code (ours aborts).
extern "C" void __cdecl MoveCmd_Op88(void) {
    reinterpret_cast<void (__cdecl*)()>(CodeAt(at::kOp88States, Sc()[4], "MoveCmd_Op88"))();
}

// original 0x5794F0: op 88's state 0 - a tint record from Sprite_SetTint(the
// sprite, 0, 0, 0, 1) kept in Field_ActiveMember +0x9F, the sprite's +0 bit 5,
// its colour 0xC0 0xC0 0xC0 with +0x5C 1, the wait +0x8A of Field_ActiveMember
// less 1, state 1.
extern "C" void __cdecl MoveCmd_Op88Start(void) {
    const unsigned char n = SH_CALL(Sprite_SetTint)(Sc(), 0, 0, 0, 1);
    Fam()[0x9F] = n;
    Sc()[0] |= 0x20;
    Sc()[0x5F] = 0xC0;
    Sc()[0x5E] = 0xC0;
    Sc()[0x5D] = 0xC0;
    Sc()[0x5C] = 1;
    SetWord(Fam() + 0x8A, Word(Fam() + 0x8A) - 1u);
    Sc()[4] = 1;
}

// original 0x579560: op 88's state 1 - each of the colour bytes +0x5D..+0x5F
// not yet 0x80 less 4; all three at 0x80: Tint_Release(Field_ActiveMember
// +0x9F), +0 bit 6, state 0; else the wait less 1.
// As the original has it: a byte that is not a multiple of 4 from 0x80 steps
// past it and wraps round.
extern "C" void __cdecl MoveCmd_Op88Fade(void) {
    for (unsigned c = 0x5D; c <= 0x5F; ++c)
        if (Sc()[c] != 0x80) Sc()[c] = static_cast<unsigned char>(Sc()[c] - 4);
    unsigned char* const sc = Sc();
    if (sc[0x5D] == 0x80 && sc[0x5E] == 0x80 && sc[0x5F] == 0x80) {
        SH_CALL(Tint_Release)(Fam()[0x9F]);
        Sc()[0] |= 0x40;
        Sc()[4] = 0;
        return;
    }
    SetWord(Fam() + 0x8A, Word(Fam() + 0x8A) - 1u);
}

// original 0x5795F0: op 87 (MoveScript_Group8 0x5788F5): Sprite_Current +4
// picks the state from 0x663B04 - MoveCmd_Op87Start, MoveCmd_Op87Fade.
// As the original has it: the byte is unchecked; past 1 the words are
// EventScript_OpLengths' bytes, not code (ours aborts).
extern "C" void __cdecl MoveCmd_Op87(void) {
    reinterpret_cast<void (__cdecl*)()>(CodeAt(at::kOp87States, Sc()[4], "MoveCmd_Op87"))();
}

// original 0x579610: op 87's state 0 - the tint record kept as op 88's, the
// sprite's +0 bit 5 set and bit 6 cleared, +0x5C 1, the colour 0x80 0x80 0x80,
// state 1, the wait less 1.
extern "C" void __cdecl MoveCmd_Op87Start(void) {
    const unsigned char n = SH_CALL(Sprite_SetTint)(Sc(), 0, 0, 0, 1);
    Fam()[0x9F] = n;
    Sc()[0] |= 0x20;
    Sc()[0] &= 0xBF;
    Sc()[0x5C] = 1;
    Sc()[0x5F] = 0x80;
    Sc()[0x5E] = 0x80;
    Sc()[0x5D] = 0x80;
    Sc()[4] = 1;
    SetWord(Fam() + 0x8A, Word(Fam() + 0x8A) - 1u);
}

// original 0x579690: op 87's state 1 - each colour byte below 0xC0 as a
// signed byte (0x80..0xBF) plus 4; all three at 0xC0: Tint_Release, +0 bit 5
// cleared, +0x5C..+0x5F and the state 0; else the wait less 1.
extern "C" void __cdecl MoveCmd_Op87Fade(void) {
    for (unsigned c = 0x5D; c <= 0x5F; ++c)
        if (Sb(Sc()[c]) < Sb(0xC0)) Sc()[c] = static_cast<unsigned char>(Sc()[c] + 4);
    unsigned char* const sc = Sc();
    if (sc[0x5D] == 0xC0 && sc[0x5E] == 0xC0 && sc[0x5F] == 0xC0) {
        SH_CALL(Tint_Release)(Fam()[0x9F]);
        Sc()[0] &= 0xDF;
        Sc()[0x5C] = 0;
        Sc()[0x5F] = 0;
        Sc()[0x5E] = 0;
        Sc()[0x5D] = 0;
        Sc()[4] = 0;
        return;
    }
    SetWord(Fam() + 0x8A, Word(Fam() + 0x8A) - 1u);
}

// original 0x57C8E0: op E9 (MoveScript_GroupE 0x5775B9; the bosses' end moves
// BossNue_EndMove / BossWeretigr_EndMove): its seven words handed on to the
// state Sprite_Current +4 picks from 0x663B84, its eax the answer (the op
// repeats while al is not 0).
// As the original has it: the state byte is unchecked; past 3 the words are
// not code (ours aborts).
extern "C" unsigned char __cdecl MoveCmd_OpE9(unsigned char* object, signed char a, signed char b, unsigned short c,
                                              unsigned short d, unsigned char e, unsigned char f) {
    using State = unsigned char (__cdecl*)(unsigned char*, signed char, signed char, unsigned short, unsigned short,
                                           unsigned char, unsigned char);
    return reinterpret_cast<State>(CodeAt(at::kOpE9States, Sc()[4], "MoveCmd_OpE9"))(object, a, b, c, d, e, f);
}

namespace {

// 16 / the object's speed, a byte: the frames of a step (idiv).
unsigned char StepFrames(const unsigned char* object, const char* who) {
    return static_cast<unsigned char>(Idiv(0x10, Field_MoveSpeeds[object[4]], who));
}

}  // namespace

// original 0x57C920: op E9's state 0 - the jump's start. The speed
// Field_MoveSpeeds[object +4]; 0 answers al 0 at once. Else +9 0; the steps
// +0xB max(|a|, |b|) less 1 (0 for none); the frames +0xA 16 / speed; the
// rise +0x14 c << 8; then the kind-2 object (MoveScript_ObjectKind 2) takes
// Field_Kind2X / Z (a, b halves past Sprite_Kind2's x / z), MoveScript_
// F3Divisor speed * 8, MoveScript_FAWord 0 and state 2; any other the
// velocity +0xC / +0x10 (a << 15) / (frames * steps), (b << 15) / .. (both 0
// with no steps) and state 1. al 1.
// As the original has it: the frames are read again after the call; a speed
// above 16 (Field_MoveSpeeds read past its six) makes them 0 and the division
// by frames * steps faults (ours aborts).
extern "C" unsigned char __cdecl MoveCmd_OpE9Start(unsigned char* object, signed char a, signed char b, unsigned short c,
                                                   unsigned short, unsigned char, unsigned char) {
    const unsigned char speed = Field_MoveSpeeds[object[4]];
    if (speed == 0) return 0;
    Sc()[9] = 0;
    const std::int32_t sa = a, sb = b;
    const std::int32_t ma = sa < 0 ? -sa : sa, mb = sb < 0 ? -sb : sb;
    const auto steps = static_cast<unsigned char>(ma < mb ? mb : ma);
    Sc()[0xB] = steps ? static_cast<unsigned char>(steps - 1) : 0;
    Sc()[0xA] = static_cast<unsigned char>(Idiv(0x10, speed, "MoveCmd_OpE9Start"));
    SetLong(Sc() + 0x14, static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(static_cast<short>(c))) << 8));
    if (SH_CALL(MoveScript_ObjectKind)() == 2) {
        Field_Kind2Z = static_cast<long>(static_cast<U>(sb) << 15) + Long(Sprite_Kind2 + 0x38);
        Field_Kind2X = static_cast<long>(static_cast<U>(sa) << 15) + Long(Sprite_Kind2 + 0x34);
        MoveScript_F3Divisor = static_cast<unsigned short>(speed << 3);
        MoveScript_FAWord = 0;
        Sc()[4] = 2;
        return 1;
    }
    if (steps == 0) {
        SetLong(Sc() + 0x10, 0);
        SetLong(Sc() + 0xC, 0);
        Sc()[4] = 1;
        return 1;
    }
    unsigned char* sc = Sc();
    SetLong(sc + 0xC, Idiv(static_cast<std::int32_t>(static_cast<U>(sa) << 15), sc[0xA] * steps, "MoveCmd_OpE9Start"));
    sc = Sc();
    SetLong(sc + 0x10, Idiv(static_cast<std::int32_t>(static_cast<U>(sb) << 15), sc[0xA] * steps, "MoveCmd_OpE9Start"));
    Sc()[4] = 1;
    return 1;
}

// original 0x57CA80: op E9's state 1 - the jump in the air. The velocity into
// x / z, the rise's high part (+0x14 >> 8) into the height +0x3E, the fall d
// into the rise, the frames less 1; the ground from AreaMap_Elevation: falling
// (+0x14 below 0) and below the ground, the height is the ground and with f
// bit 1 the state 0 and al 0. At the top (the rise from not below 0 to not
// above 0) with e not 0xFF: +0x2A f's bit 0 unless the sprite is kind 6, and
// Sprite_EnsureAnimation(e). A step's frames done: the next step's frames
// (16 / speed) and a step fewer, or with none left MoveCmd_OpDB and state 3;
// Party_ApplyRecord(object); al 1.
// As the original has it: the speed index object +4 is unchecked, and a
// speed of 0 there divides by 0 (ours aborts).
extern "C" unsigned char __cdecl MoveCmd_OpE9Arc(unsigned char* object, signed char, signed char, unsigned short,
                                                 unsigned short d, unsigned char e, unsigned char f) {
    const auto add = [](unsigned char* to, std::int32_t by) {
        SetLong(to, static_cast<std::int32_t>(static_cast<U>(Long(to)) + static_cast<U>(by)));
    };
    add(Sc() + 0x34, Long(Sc() + 0xC));
    add(Sc() + 0x38, Long(Sc() + 0x10));
    SetWord(Sc() + 0x3E, Word(Sc() + 0x3E) + static_cast<U>(Long(Sc() + 0x14) >> 8));
    const std::int32_t rise = Long(Sc() + 0x14);
    SetLong(Sc() + 0x14, static_cast<std::int32_t>(static_cast<U>(static_cast<short>(d)) + static_cast<U>(rise)));
    Sc()[0xA] = static_cast<unsigned char>(Sc()[0xA] - 1);
    const long ground = SH_CALL(AreaMap_Elevation)(Long(Sc() + 0x34), Long(Sc() + 0x38));
    unsigned char* sc = Sc();
    if (Long(sc + 0x14) < 0 && S16(static_cast<U>(ground)) > S16(Word(sc + 0x3E))) {
        SetWord(sc + 0x3E, static_cast<U>(ground));
        if (f & 2) {
            Sc()[4] = 0;
            return 0;
        }
        sc = Sc();
    }
    if (!(Long(sc + 0x14) > 0) && !(rise < 0) && e != 0xFF) {
        if (sc[6] != 6) sc[0x2A] = f & 1;
        SH_CALL(Sprite_EnsureAnimation)(e);
        sc = Sc();
    }
    if (sc[0xA] == 0) {
        if (sc[0xB] != 0) {
            sc[0xA] = StepFrames(object, "MoveCmd_OpE9Arc");
            Sc()[0xB] = static_cast<unsigned char>(Sc()[0xB] - 1);
            SH_CALL(Party_ApplyRecord)(object);
            return 1;
        }
        SH_CALL(MoveCmd_OpDB)();
        Sc()[4] = 3;
    }
    SH_CALL(Party_ApplyRecord)(object);
    return 1;
}

// original 0x57CBB0: op E9's state 2 - the kind-2 object's jump: the rise into
// the height as state 1's, MapView_SetElevation(Sprite_Kind2 +0x3E), the frames
// less 1, the ground at Field_Kind2X / Z; falling below it the height is the
// ground (MapView_SetElevation again) and with f bit 1 the state 0. Frames
// left: al 1. A step left: the next step's frames, a step fewer, al 1. None:
// at or above the ground the object lands on Field_Kind2X / Z (state 0, al 0),
// else one more frame, al 1.
extern "C" unsigned char __cdecl MoveCmd_OpE9Kind2(unsigned char* object, signed char, signed char, unsigned short,
                                                   unsigned short d, unsigned char, unsigned char f) {
    SetWord(Sc() + 0x3E, Word(Sc() + 0x3E) + static_cast<U>(Long(Sc() + 0x14) >> 8));
    SetLong(Sc() + 0x14, static_cast<std::int32_t>(static_cast<U>(Long(Sc() + 0x14)) + static_cast<U>(static_cast<short>(d))));
    SH_CALL(MapView_SetElevation)(Word(Sprite_Kind2 + 0x3E));
    Sc()[0xA] = static_cast<unsigned char>(Sc()[0xA] - 1);
    const long ground = SH_CALL(AreaMap_Elevation)(Field_Kind2X, Field_Kind2Z);
    unsigned char* sc = Sc();
    if (Long(sc + 0x14) < 0 && S16(static_cast<U>(ground)) > S16(Word(sc + 0x3E))) {
        SetWord(sc + 0x3E, static_cast<U>(ground));
        SH_CALL(MapView_SetElevation)(Word(Sc() + 0x3E));
        if (f & 2) Sc()[4] = 0;
        sc = Sc();
    }
    if (sc[0xA] != 0) return 1;
    if (sc[0xB] != 0) {
        sc[0xA] = StepFrames(object, "MoveCmd_OpE9Kind2");
        Sc()[0xB] = static_cast<unsigned char>(Sc()[0xB] - 1);
        return 1;
    }
    if (S16(static_cast<U>(ground)) < S16(Word(sc + 0x3E))) {
        sc[0xA] = 1;
        return 1;
    }
    SetLong(sc + 0x34, Field_Kind2X);
    SetLong(Sc() + 0x38, Field_Kind2Z);
    SetWord(Sc() + 0x3E, static_cast<U>(ground));
    SH_CALL(MapView_SetElevation)(Word(Sc() + 0x3E));
    Sc()[4] = 0;
    return 0;
}

// original 0x57CCE0: op E9's state 3 - the fall after the last step: the
// rise into the height and d into the rise; below the ground the height is
// the ground, state 0 and al 0; else al 1.
extern "C" unsigned char __cdecl MoveCmd_OpE9Fall(unsigned char*, signed char, signed char, unsigned short, unsigned short d,
                                                  unsigned char, unsigned char) {
    SetWord(Sc() + 0x3E, Word(Sc() + 0x3E) + static_cast<U>(Long(Sc() + 0x14) >> 8));
    SetLong(Sc() + 0x14, static_cast<std::int32_t>(static_cast<U>(Long(Sc() + 0x14)) + static_cast<U>(static_cast<short>(d))));
    const long ground = SH_CALL(AreaMap_Elevation)(Long(Sc() + 0x34), Long(Sc() + 0x38));
    unsigned char* const sc = Sc();
    if (S16(static_cast<U>(ground)) > S16(Word(sc + 0x3E))) {
        SetWord(sc + 0x3E, static_cast<U>(ground));
        Sc()[4] = 0;
        return 0;
    }
    return 1;
}

// original 0x57CD40: op DB (MoveScript_GroupD 0x577F20; MoveCmd_OpE9Arc;
// Scena06_LeapAir): x and z each plus its velocity when that is above 0, then
// rounded down to the half unit (& 0xFFFF8000).
extern "C" void __cdecl MoveCmd_OpDB(void) {
    unsigned char* sc = Sc();
    if (Long(sc + 0xC) > 0) {
        SetLong(sc + 0x34, static_cast<std::int32_t>(static_cast<U>(Long(sc + 0x34)) + static_cast<U>(Long(sc + 0xC))));
        sc = Sc();
    }
    SetLong(sc + 0x34, static_cast<std::int32_t>(static_cast<U>(Long(sc + 0x34)) & 0xFFFF8000u));
    sc = Sc();
    if (Long(sc + 0x10) > 0) {
        SetLong(sc + 0x38, static_cast<std::int32_t>(static_cast<U>(Long(sc + 0x38)) + static_cast<U>(Long(sc + 0x10))));
        sc = Sc();
    }
    SetLong(sc + 0x38, static_cast<std::int32_t>(static_cast<U>(Long(sc + 0x38)) & 0xFFFF8000u));
}

// =============================================================================
// The event script
// =============================================================================

// original 0x579CA0: an F0 / F1 skipped (EventScript_SkipControl's F0 / F1,
// the event_script.cpp table's skip_if): `at` is ON the F0 / F1; three bytes
// on, then by EventScript_OpLengths and EventScript_SkipControl (for F0..FC)
// to the FE of its depth - an FD on the way is stepped over - and one past it.
// As the original has it: byte +1 is loaded and not used; an FF on the way,
// or an FD..FF that SkipControl leaves where it was, is never passed.
extern "C" const unsigned char* __cdecl EventScript_SkipIf(const unsigned char* at) {
    at += 3;
    while (at[0] != 0xFE) {
        const unsigned char b = at[0];
        if (b >= 0xF0) {
            if (b < 0xFD) at = SH_CALL(EventScript_SkipControl)(at);
            if (at[0] == 0xFD) ++at;
        } else {
            at += EventScript_OpLengths[b >> 4];
        }
    }
    return at + 1;
}

namespace {

// EventOp_3x / 4x / 7x / 6x / Ax's shared start: Sprite_Current and
// Field_ActiveMember the next Sprite_Objects entry.
unsigned char* Claim() {
    unsigned char* const o = Object(Count());
    Sprite_Current = o;
    Field_ActiveMember = o;
    return o;
}
// x, z into the sprite and into object +0x8C / +0x90 (+0x94 0), by the count
// read again each time, as EventOp_1x's.
void PlaceXZ(std::int32_t x, std::int32_t z) {
    SetLong(Sc() + 0x34, x);
    SetLong(Object(Count()) + 0x8C, Long(Sc() + 0x34));
    SetLong(Sc() + 0x38, z);
    SetLong(Object(Count()) + 0x90, Long(Sc() + 0x38));
    SetLong(Object(Count()) + 0x94, 0);
}
void Ground() {
    unsigned char* const sc = Sc();
    const long e = SH_CALL(AreaMap_Elevation)(Long(sc + 0x34), Long(sc + 0x38));
    SetWord(Sc() + 0x3E, static_cast<U>(e));
}
void Colour(unsigned char v) {
    Sc()[0x5D] = v;
    Sc()[0x5F] = v;
    Sc()[0x5E] = v;
}

}  // namespace

// original 0x57A7C0: op 3x, 18 bytes (EventScript_Op's handler 3) - EventOp_2x's
// placement with its own layout: op[1]:op[2] the bank, op[3] bit 7 +0 bit 5 and
// low nibble +0x5C, op[4] / op[5] words +0x98 / +0x9A, op[6..9] x and z,
// op[0xA] +1, op[0xB] +0x84, op[0xC] +2, op[0xD] the flags, op[0xE]:op[0xF]
// +0x88, op[0x10] +0x83, op[0x11] the dword +0x70; op[0]'s nibbles +6 / +8.
// As the original has it: nothing past the count's test of 30 checks it.
extern "C" void __cdecl EventOp_3x(const unsigned char* op) {
    if (Count() >= 30) return;
    Claim();
    SetBank(static_cast<unsigned>(op[1]) << 8 | op[2]);
    SH_CALL(EventObj_Reset)();
    SH_CALL(Sprite_SetAnimationBank)(Bank());
    Sc()[0] = 1;
    PlaceXZ(Coordinate(op[6], op[7]), Coordinate(op[8], op[9]));
    Ground();
    Sc()[6] = op[0] >> 4;
    Sc()[8] = op[0] & 0xF;
    Sc()[1] = op[0xA];
    Sc()[2] = op[0xC];
    SetLong(Sc() + 0x70, op[0x11]);
    unsigned char* const n = Object(Count());
    SetWord(n + 0x98, op[4]);
    SetWord(n + 0x9A, op[5]);
    SetWord(n + 0x88, static_cast<unsigned>(op[0xE]) << 8 | op[0xF]);
    n[0x83] = op[0x10];
    n[0x84] = op[0xB];
    n[0xA0] = 0x7F;
    SH_CALL(EventObj_SetFlags)(op + 0xD);
    if (op[3] & 0x80) Sc()[0] |= 0x20;
    Colour(0);
    Sc()[0x5C] = op[3] & 0xF;
    SH_CALL(EventObj_Face)();
    BumpCount();
}

// original 0x57A990: op 4x, 17 bytes (handler 4) - op 3x's layout without
// +0x83 and +0x70: op[0x10] is object +0xA0 (not 0x7F).
extern "C" void __cdecl EventOp_4x(const unsigned char* op) {
    if (Count() >= 30) return;
    Claim();
    SetBank(static_cast<unsigned>(op[1]) << 8 | op[2]);
    SH_CALL(EventObj_Reset)();
    SH_CALL(Sprite_SetAnimationBank)(Bank());
    Sc()[0] = 1;
    PlaceXZ(Coordinate(op[6], op[7]), Coordinate(op[8], op[9]));
    Ground();
    Sc()[1] = op[0xA];
    Sc()[2] = op[0xC];
    Sc()[8] = op[0] & 0xF;
    Sc()[6] = op[0] >> 4;
    unsigned char* const n = Object(Count());
    SetWord(n + 0x98, op[4]);
    SetWord(n + 0x9A, op[5]);
    SetWord(n + 0x88, static_cast<unsigned>(op[0xE]) << 8 | op[0xF]);
    n[0x84] = op[0xB];
    n[0xA0] = op[0x10];
    SH_CALL(EventObj_SetFlags)(op + 0xD);
    if (op[3] & 0x80) Sc()[0] |= 0x20;
    Colour(0);
    Sc()[0x5C] = op[3] & 0xF;
    SH_CALL(EventObj_Face)();
    BumpCount();
}

// original 0x57AB50: op 7x, 17 bytes (handler 7) - op[1]:op[2] the bank,
// op[3] / op[4] words +0x98 / +0x9A, op[5..8] x and z, op[9] +1, op[0xA]
// +0x84, op[0xB] +2, op[0xC] the flags, op[0xD]:op[0xE] +0x88, op[0xF] +0x83,
// op[0x10] +0x9E; +0x5C 2.
extern "C" void __cdecl EventOp_7x(const unsigned char* op) {
    if (Count() >= 30) return;
    Claim();
    SetBank(static_cast<unsigned>(op[1]) << 8 | op[2]);
    SH_CALL(EventObj_Reset)();
    SH_CALL(Sprite_SetAnimationBank)(Bank());
    Sc()[0] = 1;
    PlaceXZ(Coordinate(op[5], op[6]), Coordinate(op[7], op[8]));
    Ground();
    Sc()[1] = op[9];
    Sc()[2] = op[0xB];
    Sc()[8] = op[0] & 0xF;
    Sc()[6] = op[0] >> 4;
    unsigned char* const n = Object(Count());
    SetWord(n + 0x98, op[3]);
    SetWord(n + 0x9A, op[4]);
    SetWord(n + 0x88, static_cast<unsigned>(op[0xD]) << 8 | op[0xE]);
    n[0x83] = op[0xF];
    n[0x84] = op[0xA];
    n[0x9E] = op[0x10];
    n[0xA0] = 0x7F;
    SH_CALL(EventObj_SetFlags)(op + 0xC);
    Colour(0);
    Sc()[0x5C] = 2;
    SH_CALL(EventObj_Face)();
    BumpCount();
}

// original 0x57AD10: op 6x, 16 bytes (handler 6; also the scene code's
// placers - Area141_*, Scena08_SpawnPair, Scena13_SpawnPair*, Scena15's, FC1's
// 0x46A600) - a party member's sprite: Field_MemberSprite(op[1], op[0xF]) in
// place of the bank; op[2] / op[3] +0x98 / +0x9A, op[4..7] x and z, op[8] +1,
// op[9] +0x84, op[0xA] +2, op[0xB] the flags, op[0xC]:op[0xD] +0x88, op[0xE]
// +0x83; +0x5C 2.
extern "C" void __cdecl EventOp_6x(const unsigned char* op) {
    if (Count() >= 30) return;
    Claim();
    SH_CALL(EventObj_Reset)();
    SH_CALL(Field_MemberSprite)(op[1], op[0xF]);
    Sc()[0] = 1;
    PlaceXZ(Coordinate(op[4], op[5]), Coordinate(op[6], op[7]));
    Ground();
    Sc()[1] = op[8];
    Sc()[2] = op[0xA];
    Sc()[8] = op[0] & 0xF;
    Sc()[6] = op[0] >> 4;
    unsigned char* const n = Object(Count());
    SetWord(n + 0x98, op[2]);
    SetWord(n + 0x9A, op[3]);
    SetWord(n + 0x88, static_cast<unsigned>(op[0xC]) << 8 | op[0xD]);
    n[0x83] = op[0xE];
    n[0x84] = op[9];
    n[0xA0] = 0x7F;
    SH_CALL(EventObj_SetFlags)(op + 0xB);
    Colour(0);
    Sc()[0x5C] = 2;
    SH_CALL(EventObj_Face)();
    BumpCount();
}

// original 0x57AEC0: op Ax, 14 bytes (handler 10) - EventOp_Bx's placement into
// Sprite_Objects[count]: the descriptor +8 entry op[9] of Game_AreaNumber's
// area through Sprite_InitFromEntry, party record 4 cleared, op[1..4] x and z,
// op[5] +1, op[6] +0x84, op[7] +2, op[8] the flags, op[0xA] +0x83,
// op[0xC]:op[0xD] +0x88; +0x70 the entry's byte 2 (through +0x54); +0x5C 2
// and the colour 0x80; and with +0x84 0 the area block's cell (and the ones
// right and below it, where x / z has a half) set to 0x10.
// As the original has it: the descriptor is Game_AreaNumber's unchecked.
extern "C" void __cdecl EventOp_Ax(const unsigned char* op) {
    if (Count() >= 30) return;
    const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
    const unsigned char* const entry = PtrAt(descriptor + 8) + op[9] * 8u;
    unsigned char* const o = Claim();
    o[8] = op[0] & 0xF;
    SH_CALL(EventObj_Reset)();
    SH_CALL(Sprite_InitFromEntry)(const_cast<unsigned char*>(entry));
    SH_CALL(PartyRecord_Clear)(4);
    SetLong(Sc() + 0x34, Coordinate(op[1], op[2]));
    SetLong(Sc() + 0x38, Coordinate(op[3], op[4]));
    Ground();
    Sc()[1] = op[5];
    Sc()[2] = op[7];
    Sc()[6] = op[0] >> 4;
    SetLong(Sc() + 0x20, 0);
    SetLong(Sc() + 0x1C, 0);
    SetLong(Sc() + 0x18, 0);
    {
        unsigned char* const sc = Sc();
        sc[0x70] = PtrAt(sc + 0x54)[2];
    }
    unsigned char* const n = Object(Count());
    SetLong(n + 0x94, 0);
    SetLong(n + 0x90, 0);
    SetLong(n + 0x8C, 0);
    n[0x83] = op[0xA];
    n[0x84] = op[6];
    SetWord(n + 0x88, static_cast<unsigned>(op[0xC]) << 8 | op[0xD]);
    n[0xA0] = 0x7F;
    SH_CALL(EventObj_SetFlags)(op + 8);
    Colour(0x80);
    Sc()[0x5C] = 2;
    if (Object(Count())[0x84] == 0) {
        unsigned char* sc = Sc();
        SH_CALL(AreaMap_SetByte)(Word(sc + 0x36), Word(sc + 0x3A), 0x10);
        sc = Sc();
        if (Word(sc + 0x34) != 0) {
            SH_CALL(AreaMap_SetByte)(static_cast<std::uint16_t>(Word(sc + 0x36) + 1), Word(sc + 0x3A), 0x10);
            sc = Sc();
        }
        if (Word(sc + 0x38) != 0) {
            SH_CALL(AreaMap_SetByte)(Word(sc + 0x36), static_cast<std::uint16_t>(Word(sc + 0x3A) + 1), 0x10);
            sc = Sc();
        }
        if (Word(sc + 0x34) != 0 && Word(sc + 0x38) != 0)
            SH_CALL(AreaMap_SetByte)(static_cast<std::uint16_t>(Word(sc + 0x36) + 1),
                                     static_cast<std::uint16_t>(Word(sc + 0x3A) + 1), 0x10);
    }
    BumpCount();
}

// --- the conditions (EventScript_Conditions entries; each handed the script
//     position, its operand at **at, al the answer) ---

// original 0x57C1A0: condition 1 - Game_AreaNumber (a word) equals the operand.
extern "C" unsigned char __cdecl EventCond_Area(const unsigned char** at) { return Game_AreaNumber == **at; }
// original 0x57C3E0: condition 3 - the script counter 0x903848 equals it.
extern "C" unsigned char __cdecl EventCond_Counter0(const unsigned char** at) { return At(0x903848)[0] == **at; }
// original 0x57C400 / 0x57C420 / 0x57C440: conditions 4..6 - counters 1..3.
extern "C" unsigned char __cdecl EventCond_Counter1(const unsigned char** at) { return At(0x903849)[0] == **at; }
extern "C" unsigned char __cdecl EventCond_Counter2(const unsigned char** at) { return At(0x90384A)[0] == **at; }
extern "C" unsigned char __cdecl EventCond_Counter3(const unsigned char** at) { return At(0x90384B)[0] == **at; }
// original 0x57C1F0: condition 7 - MoveScript_Var7 (the chapter's run byte) equals it.
extern "C" unsigned char __cdecl EventCond_Run(const unsigned char** at) {
    return static_cast<unsigned char>(MoveScript_Var7) == **at;
}
// original 0x57C230: condition 9 - Field_StatusBits' bit 0, or its
// complement's unless the operand is 1.
extern "C" unsigned char __cdecl EventCond_Status1(const unsigned char** at) {
    const auto bits = static_cast<unsigned char>(Field_StatusBits);
    return static_cast<unsigned char>((**at == 1 ? bits : static_cast<unsigned char>(~bits)) & 1);
}
// original 0x57C250: condition 11 - the leader's member id (ObjTrio +0x89)
// equals it.
extern "C" unsigned char __cdecl EventCond_LeaderId(const unsigned char** at) { return ObjTrio[0x89] == **at; }
// original 0x57C270: condition 12 - story flag `operand` (Flags_Test of
// 0x904030); the answer is Flags_Test's eax.
extern "C" unsigned char __cdecl EventCond_StoryFlag(const unsigned char** at) {
    return SH_CALL(Flags_Test)(At(0x904030), **at);
}
// original 0x57C290: condition 13 - KeyItem_Has(operand).
extern "C" unsigned char __cdecl EventCond_KeyItem(const unsigned char** at) { return SH_CALL(KeyItem_Has)(**at); }
// original 0x57C2B0: condition 14 - Cond_ByteFA (unsigned) at most the operand.
extern "C" unsigned char __cdecl EventCond_ChapterAtMost(const unsigned char** at) {
    return static_cast<unsigned char>(Cond_ByteFA) <= **at;
}
// original 0x57C2D0: condition 15 - bit 0 of CharacterRecords[operand] +0xB.
// As the original has it: the operand is a whole byte into the eight records.
extern "C" unsigned char __cdecl EventCond_RecordBit0(const unsigned char** at) { return Record(**at)[0xB] & 1; }
// original 0x57C2F0: condition 16 - bit `operand` of the gene flags 0x904650
// (Flags_Test; the dword's 18 bits, battle_e5.md).
extern "C" unsigned char __cdecl EventCond_Gene(const unsigned char** at) { return SH_CALL(Flags_Test)(At(0x904650), **at); }

// original 0x57C7E0 (MoveScript_Flow 0x57701D, Scena03_Scene5 0x5438DC): bit 6
// of the three party objects' first byte cleared - ObjTrio_SetBit40's inverse.
extern "C" void __cdecl ObjTrio_ClearBit40(void) {
    ObjTrio[0] &= 0xBF;
    ObjTrio[0x14C] &= 0xBF;
    ObjTrio[0x298] &= 0xBF;
}

void FieldO_Inject() {
    if (bof3::WantsShadow("field_o")) field_o::SelfTest();
    BOF3_INJECT(Menu_DrawStatsPanel);
    BOF3_INJECT(Menu_DrawExpPanel);
    BOF3_INJECT(Menu_DrawIconWheel);
    BOF3_INJECT(Menu_DrawTile16);
    BOF3_INJECT(Menu_DrawEquipCompare);
    BOF3_INJECT(Menu_DrawAbilityPanel);
    BOF3_INJECT(Menu_DrawItemPanel);
    BOF3_INJECT(Menu_DrawSaveSlot);
    BOF3_INJECT(MoveCmd_OpF9);
    BOF3_INJECT(MoveCmd_Op88);
    BOF3_INJECT(MoveCmd_Op88Start);
    BOF3_INJECT(MoveCmd_Op88Fade);
    BOF3_INJECT(MoveCmd_Op87);
    BOF3_INJECT(MoveCmd_Op87Start);
    BOF3_INJECT(MoveCmd_Op87Fade);
    BOF3_INJECT(EventScript_SkipIf);
    BOF3_INJECT(EventOp_3x);
    BOF3_INJECT(EventOp_4x);
    BOF3_INJECT(EventOp_7x);
    BOF3_INJECT(EventOp_6x);
    BOF3_INJECT(EventOp_Ax);
    BOF3_INJECT(EventCond_Area);
    BOF3_INJECT(EventCond_Counter0);
    BOF3_INJECT(EventCond_Counter1);
    BOF3_INJECT(EventCond_Counter2);
    BOF3_INJECT(EventCond_Counter3);
    BOF3_INJECT(EventCond_Run);
    BOF3_INJECT(EventCond_Status1);
    BOF3_INJECT(EventCond_LeaderId);
    BOF3_INJECT(EventCond_StoryFlag);
    BOF3_INJECT(EventCond_KeyItem);
    BOF3_INJECT(EventCond_ChapterAtMost);
    BOF3_INJECT(EventCond_RecordBit0);
    BOF3_INJECT(EventCond_Gene);
    BOF3_INJECT(ObjTrio_ClearBit40);
    BOF3_INJECT(MoveCmd_OpE9);
    BOF3_INJECT(MoveCmd_OpE9Start);
    BOF3_INJECT(MoveCmd_OpE9Arc);
    BOF3_INJECT(MoveCmd_OpE9Kind2);
    BOF3_INJECT(MoveCmd_OpE9Fall);
    BOF3_INJECT(MoveCmd_OpDB);
}
