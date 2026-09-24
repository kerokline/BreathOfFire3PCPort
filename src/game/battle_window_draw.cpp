// The battle windows' draw helpers (the seventh round's group BD).
// docs/battle_window_draw.md.
//
// Nineteen functions the owner's combat route reaches: the four openers of
// battle windows 1..4, and the primitives the battle window handlers (group
// BC's 0x442FA0, 0x443870, 0x443B10, 0x443D90, 0x443F60) draw with - the digit
// strip, a flat quad of a shape table, two tiles, a two-part bar, a 16 x 8
// cell, three lines - the test that keeps one enemy name per kind, and, from
// the skill list, the ability helpers: can it be used, its row, its AP cost,
// the lowest of its flag bits, a member's list of a type. Each was read to its
// last instruction (2026-09-23) against its PSX twin - BATTLE.EMI's for the
// first fourteen, GAME.EMI's for the skill test and row, the boot EXE's for the
// three ability helpers (docs/battle_window_draw.md, section 1).
//
// Every call goes through battle_window_draw::g (battle_window_draw_callees.h),
// so that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. Where the original reads memory after a call,
// ours reads it after the same call.
//
// Upper halves. Some originals push a register whose upper bits are the
// caller's (the icon byte of Menu_DrawSkillRow) or a full argument dword
// (Skill_CanUse's member and id to Skill_ApCost); every callee here reads only
// the low 16 bits of a coordinate and the low byte of a colour, flag, count or
// id, so ours passes values with the upper bits C++ gives them - except where
// the original's own dword is cheap to rebuild exactly, which ours does.
//
// Floats. Every coordinate is an int that the original loads with `fild` and
// stores with `fstp dword`: a 16-bit value plus at most two bytes, exact in a
// float, so a plain conversion is the same bits.
#include "game/battle_window_draw.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_window_draw_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_window_draw {

const Callees kOriginals = {
    Window_Alloc,
    Crt_sprintf,
    Gpu_GetTPage,
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_GetClut,
    Gpu_SetSprt,
    Gpu_SetPolyF4,
    Gpu_SetSemiTrans,
    Gpu_SetTile,
    Gpu_SetPolyFT4,
    Gpu_SetLineF2,
    Menu_DrawIcon8,
    Text_CharCount,
    Text_DrawAt,
    Text_DrawFont8,
    Skill_ApCost,
};
Callees g = kOriginals;

}  // namespace battle_window_draw

using namespace battle_window_draw;

namespace {

using move_script::At;
using move_script::SetWord;
using move_script::Word;

unsigned char* Next() { return Gfx_PacketNext; }
unsigned char* Window(unsigned slot) { return At(at::kWindows + slot * at::kWindowStride); }
char* PrintBuf() { return reinterpret_cast<char*>(At(at::kPrintBuf)); }
const char* Format(std::uint32_t at) { return reinterpret_cast<const char*>(At(at)); }

std::int32_t S16(std::uint32_t v) { return static_cast<std::int16_t>(v); }
float F(std::int32_t v) { return static_cast<float>(v); }
void Put(unsigned char* at, float f) { std::memcpy(at, &f, sizeof f); }

// Entry 0 of CLUT row (s8 window colour * 2 + row) in the shadow at 0x80B788,
// its 5-bit channels widened to 8 into the primitive's r g b - the colour byte
// read again for each channel, as the originals do.
void ShadeColour(unsigned char* p, unsigned row) {
    const auto entry = [row]() {
        const std::int32_t colour = static_cast<signed char>(At(at::kColour)[0]);
        return At(at::kClutShadow + static_cast<std::uint32_t>((static_cast<std::int32_t>(row) + colour * 2) << 5));
    };
    p[4] = static_cast<unsigned char>((entry()[0] & 0x1F) << 3);
    p[5] = static_cast<unsigned char>(((Word(entry()) >> 5) & 0x1F) << 3);
    p[6] = static_cast<unsigned char>(((Word(entry()) >> 10) & 0x1F) << 3);
}

}  // namespace

// ===========================================================================
// The four window openers: battle window records 1..4, handler 3 (the battle
// windows), sub-kind 0..3 in +2 and the caller's state in +3. Window_Alloc's
// answer is not looked at - the fields are written whether or not the record
// was free, as the original has it (PSX 0x801D92D4 .. 0x801D93F8 alike).

// original 0x444230 (PSX 0x801D92D4): window 1, the party's status panel: x
// from the byte table 0x64E307 by the party size (the low byte of 0x904AB0,
// also kept in +0xA), y 0xF0 (its handler slides it up to 0xC8), +8 and +9
// cleared.
extern "C" void __cdecl BattleWin_OpenStatus(unsigned state) {
    g.window_alloc(1, 3);
    unsigned char* const w = Window(1);
    w[3] = static_cast<unsigned char>(state);
    const unsigned size = At(at::kPartyCount)[0];
    w[0xA] = static_cast<unsigned char>(size);
    w[2] = 0;
    SetWord(w + 4, At(at::kStatusX + size)[0]);
    SetWord(w + 6, 0xF0);
    w[8] = 0;
    w[9] = 0;
}

// original 0x444290 (PSX 0x801D9358): window 2, sub-kind 1, at (0x88, 0x58).
extern "C" void __cdecl BattleWin_OpenSub1(unsigned state) {
    g.window_alloc(2, 3);
    unsigned char* const w = Window(2);
    w[2] = 1;
    w[3] = static_cast<unsigned char>(state);
    SetWord(w + 4, 0x88);
    SetWord(w + 6, 0x58);
}

// original 0x4442C0 (PSX 0x801D93B4): window 3, sub-kind 2; its place is
// left as it was.
extern "C" void __cdecl BattleWin_OpenSub2(unsigned state) {
    g.window_alloc(3, 3);
    unsigned char* const w = Window(3);
    w[2] = 2;
    w[3] = static_cast<unsigned char>(state);
}

// original 0x4442E0 (PSX 0x801D93F8): window 4, sub-kind 3, at (0x7C, 0x2A).
extern "C" void __cdecl BattleWin_OpenSub3(unsigned state) {
    g.window_alloc(4, 3);
    unsigned char* const w = Window(4);
    w[2] = 3;
    w[3] = static_cast<unsigned char>(state);
    SetWord(w + 4, 0x7C);
    SetWord(w + 6, 0x2A);
}

// ===========================================================================

// original 0x444340 (PSX 0x801D94A4): a number in the battle's 6 x 5 digit
// strip - "%4d" of the low word of `value`, or ":" (the strip's cell 10) for
// 0xFFFF; draw mode page (0x3C0, 0); then per character, 5 to the right each,
// spaces skipped: the character less '0' written back into the print buffer,
// an SPRT of 6 x 5 at (s16 x, s16 y), u = c * 6 - 0x50 as a byte, v 0xD8, the
// CLUT at (colour << 4, 0x1E0), shade 0x80.
//
// As the original has it: the character is changed in the print buffer before
// the CLUT call and read back after it; the index is a byte and the end test
// is on the next character, so the first is drawn whatever it is; x steps as
// a dword and only its low 16 bits are placed.
extern "C" void __cdecl BattleWin_DrawNumber(int x, int y, int colour, int value) {
    if ((static_cast<std::uint32_t>(value) & 0xFFFF) == 0xFFFF)
        g.sprintf_(PrintBuf(), Format(at::kFmtNone));
    else
        g.sprintf_(PrintBuf(), Format(at::kFmtNumber), static_cast<std::uint32_t>(value) & 0xFFFF);
    const unsigned page = g.get_tpage(0, 0, 0x3C0, 0);
    g.draw_mode(Next(), 0, 0, page & 0xFFFF, 0);
    g.commit(1, 0xC);
    std::int32_t px = x;
    unsigned char i = 0;
    unsigned char* c = reinterpret_cast<unsigned char*>(PrintBuf());
    do {
        if (*c != 0x20) {
            unsigned char* const p = Next();
            *c = static_cast<unsigned char>(*c - 0x30);
            SetWord(p + 0x16, g.get_clut(static_cast<int>((static_cast<unsigned>(colour) & 0xFF) << 4), 0x1E0));
            Put(p + 8, F(S16(static_cast<std::uint32_t>(px))));
            Put(p + 0xC, F(S16(static_cast<std::uint32_t>(y))));
            p[4] = p[5] = p[6] = 0x80;
            p[0x15] = 0xD8;
            p[0x14] = static_cast<unsigned char>(*c * 6 - 0x50);
            SetWord(p + 0x18, 6);
            SetWord(p + 0x1A, 5);
            g.set_sprt(p);
            g.commit(1, 0x1C);
        }
        ++i;
        px += 5;
        c = reinterpret_cast<unsigned char*>(PrintBuf()) + i;
    } while (*c != 0);
}

// original 0x4447B0 (PSX 0x801D9A84): a flat POLY_F4 of shape `shape` - four
// corners (u16 dx, u16 dy) of the 16-byte records at 0x64E148 added to
// (s16 x, s16 y) - in entry 0 of CLUT row (window colour * 2 + shade), made
// semi-transparent by the shade's bit 0.
extern "C" void __cdecl BattleWin_DrawQuadF4(int x, int y, int shape, int shade) {
    unsigned char* const p = Next();
    g.set_poly_f4(p);
    const unsigned char* const t = At(at::kQuadShapes + ((static_cast<unsigned>(shape) & 0xFF) << 4));
    const std::int32_t sx = S16(static_cast<std::uint32_t>(x)), sy = S16(static_cast<std::uint32_t>(y));
    Put(p + 0x08, F(Word(t + 0x0) + sx));
    Put(p + 0x0C, F(Word(t + 0x2) + sy));
    Put(p + 0x14, F(Word(t + 0x4) + sx));
    Put(p + 0x18, F(Word(t + 0x6) + sy));
    Put(p + 0x20, F(Word(t + 0x8) + sx));
    Put(p + 0x24, F(Word(t + 0xA) + sy));
    Put(p + 0x2C, F(Word(t + 0xC) + sx));
    Put(p + 0x30, F(Word(t + 0xE) + sy));
    const unsigned row = static_cast<unsigned>(shade) & 0xFF;
    ShadeColour(p, row);
    g.set_semi(p, row);
    g.commit(1, 0x38);
}

// original 0x444900 (PSX 0x801D9C50): a TILE at (s16 x, s16 y) of size
// `size` - u16 w, u16 h of the 4-byte records at 0x64E268 - coloured and
// made semi-transparent as BattleWin_DrawQuadF4.
extern "C" void __cdecl BattleWin_DrawTile(int x, int y, int size, int shade) {
    unsigned char* const p = Next();
    g.set_tile(p);
    Put(p + 0x08, F(S16(static_cast<std::uint32_t>(x))));
    Put(p + 0x0C, F(S16(static_cast<std::uint32_t>(y))));
    const unsigned char* const t = At(at::kTileSizes + ((static_cast<unsigned>(size) & 0xFF) << 2));
    Put(p + 0x14, F(Word(t)));
    Put(p + 0x18, F(Word(t + 2)));
    const unsigned row = static_cast<unsigned>(shade) & 0xFF;
    ShadeColour(p, row);
    g.set_semi(p, row);
    g.commit(1, 0x1C);
}

// original 0x4449E0 (PSX 0x801D9D8C): the same tile in a given 15-bit colour -
// as the PlayStation has it, r from bits 10..14 and b from bits 0..4 - made
// semi-transparent by `abe`'s bit 0.
extern "C" void __cdecl BattleWin_DrawTileRgb(int x, int y, int size, int colour, int abe) {
    unsigned char* const p = Next();
    g.set_tile(p);
    Put(p + 0x08, F(S16(static_cast<std::uint32_t>(x))));
    Put(p + 0x0C, F(S16(static_cast<std::uint32_t>(y))));
    const unsigned char* const t = At(at::kTileSizes + ((static_cast<unsigned>(size) & 0xFF) << 2));
    Put(p + 0x14, F(Word(t)));
    Put(p + 0x18, F(Word(t + 2)));
    const auto c = static_cast<std::uint32_t>(colour);
    p[4] = static_cast<unsigned char>(((c >> 10) & 0x1F) << 3);
    p[5] = static_cast<unsigned char>(((c >> 5) & 0x1F) << 3);
    p[6] = static_cast<unsigned char>((c & 0x1F) << 3);
    g.set_semi(p, static_cast<unsigned>(abe) & 0xFF);
    g.commit(1, 0x1C);
}

// original 0x444A90 (PSX 0x801D9E6C): a bar 7 high of two POLY_FT4s from the
// page (0x3C0, 0): the body from s16 x to x + w (a byte) stretched over u
// 0x90..0x97 with the CLUT at (0xE0, 0x1E0) - or u 0x80..0x87 and CLUT
// (0xB0, 0x1E0) when `lit`'s low byte is set - then its end cap from x + w to
// x + w + cap (a byte) over u 0x98..0x9F, CLUT (0xB0, 0x1E0); v 0xD8..0xDF,
// shade 0x80.
//
// As the original has it: the cap's corners are the body's floats, kept in
// the argument slots and copied as bits - the same values ours converts again.
extern "C" void __cdecl BattleWin_DrawBar(int x, int y, int w, int cap, int lit) {
    unsigned char* p = Next();
    g.set_poly_ft4(p);
    SetWord(p + 0x26, g.get_tpage(0, 0, 0x3C0, 0));
    const bool on = (static_cast<unsigned>(lit) & 0xFF) != 0;
    SetWord(p + 0x16, g.get_clut(on ? 0xB0 : 0xE0, 0x1E0));
    const std::int32_t left = S16(static_cast<std::uint32_t>(x)), top = S16(static_cast<std::uint32_t>(y));
    const std::int32_t mid = left + static_cast<std::int32_t>(static_cast<unsigned>(w) & 0xFF);
    const std::int32_t bottom = top + 7;
    Put(p + 0x08, F(left));
    Put(p + 0x0C, F(top));
    Put(p + 0x18, F(mid));
    Put(p + 0x1C, F(top));
    Put(p + 0x28, F(left));
    Put(p + 0x2C, F(bottom));
    Put(p + 0x38, F(mid));
    Put(p + 0x3C, F(bottom));
    p[4] = p[5] = p[6] = 0x80;
    const unsigned char u0 = on ? 0x80 : 0x90, u1 = on ? 0x87 : 0x97;
    p[0x14] = u0;
    p[0x15] = 0xD8;
    p[0x24] = u1;
    p[0x25] = 0xD8;
    p[0x34] = u0;
    p[0x35] = 0xDF;
    p[0x44] = u1;
    p[0x45] = 0xDF;
    g.commit(1, 0x48);

    p = Next();
    g.set_poly_ft4(p);
    SetWord(p + 0x26, g.get_tpage(0, 0, 0x3C0, 0));
    SetWord(p + 0x16, g.get_clut(0xB0, 0x1E0));
    const std::int32_t right = mid + static_cast<std::int32_t>(static_cast<unsigned>(cap) & 0xFF);
    Put(p + 0x08, F(mid));
    Put(p + 0x0C, F(top));
    Put(p + 0x18, F(right));
    Put(p + 0x1C, F(top));
    Put(p + 0x28, F(mid));
    Put(p + 0x2C, F(bottom));
    Put(p + 0x38, F(right));
    Put(p + 0x3C, F(bottom));
    p[4] = p[5] = p[6] = 0x80;
    p[0x14] = 0x98;
    p[0x15] = 0xD8;
    p[0x24] = 0x9F;
    p[0x25] = 0xD8;
    p[0x34] = 0x98;
    p[0x35] = 0xDF;
    p[0x44] = 0x9F;
    p[0x45] = 0xDF;
    g.commit(1, 0x48);
}

// original 0x444C40 (PSX 0x801DA048): cell `cell` of the battle's 16 x 8 strip -
// draw mode page (0x3C0, 0) under abr 1; an SPRT of 16 x 8 at (u16 x, u16 y),
// u = cell << 4 as a byte, v 0xD8, CLUT word 0x7800, shade 0x80.
//
// As the original has it: x and y are taken unsigned (`and 0xFFFF`, then
// `fild`), where the other helpers here sign-extend - a coordinate below 0
// lands at 65,536 less it. The PlayStation stores the halfwords, where the two
// readings are the same.
extern "C" void __cdecl BattleWin_DrawCell16(int x, int y, int cell) {
    const unsigned page = g.get_tpage(0, 1, 0x3C0, 0);
    g.draw_mode(Next(), 0, 0, page & 0xFFFF, 0);
    g.commit(1, 0xC);
    unsigned char* const p = Next();
    p[4] = p[5] = p[6] = 0x80;
    p[0x14] = static_cast<unsigned char>(static_cast<unsigned>(cell) << 4);
    SetWord(p + 0x18, 0x10);
    Put(p + 0x08, F(static_cast<std::int32_t>(static_cast<std::uint32_t>(x) & 0xFFFF)));
    SetWord(p + 0x1A, 8);
    Put(p + 0x0C, F(static_cast<std::int32_t>(static_cast<std::uint32_t>(y) & 0xFFFF)));
    SetWord(p + 0x16, 0x7800);
    p[0x15] = 0xD8;
    g.set_sprt(p);
    g.commit(1, 0x1C);
}

namespace {

// The LINE_F2 the three line helpers share: (s16 x0, s16 y0) to (s16 x1,
// s16 y1) in (r, g, b), the colour bytes the arguments' low bytes.
void LineBody(unsigned char* p, int x0, int y0, int x1, int y1, int r, int gr, int b) {
    p[4] = static_cast<unsigned char>(r);
    p[5] = static_cast<unsigned char>(gr);
    p[6] = static_cast<unsigned char>(b);
    Put(p + 0x08, F(S16(static_cast<std::uint32_t>(x0))));
    Put(p + 0x0C, F(S16(static_cast<std::uint32_t>(y0))));
    Put(p + 0x14, F(S16(static_cast<std::uint32_t>(x1))));
    Put(p + 0x18, F(S16(static_cast<std::uint32_t>(y1))));
}

// The two semi-transparent ones: draw mode page (0x3C0, 0) under `abr`, then
// the line, semi-transparent.
void BlendLine(unsigned abr, int x0, int y0, int x1, int y1, int r, int gr, int b) {
    const unsigned page = g.get_tpage(0, abr, 0x3C0, 0);
    g.draw_mode(Next(), 0, 0, page & 0xFFFF, 0);
    g.commit(1, 0xC);
    unsigned char* const p = Next();
    g.set_line_f2(p);
    LineBody(p, x0, y0, x1, y1, r, gr, b);
    g.set_semi(p, 1);
    g.commit(1, 0x20);
}

}  // namespace

// original 0x444CE0 (PSX 0x801DA3D8): an opaque LINE_F2, no draw mode of its own.
extern "C" void __cdecl BattleWin_DrawLine(int x0, int y0, int x1, int y1, int r, int gr, int b) {
    unsigned char* const p = Next();
    g.set_line_f2(p);
    LineBody(p, x0, y0, x1, y1, r, gr, b);
    g.commit(1, 0x20);
}

// original 0x444D50 (PSX 0x801DA484): the line under abr 0 (half and half).
extern "C" void __cdecl BattleWin_DrawLineHalf(int x0, int y0, int x1, int y1, int r, int gr, int b) {
    BlendLine(0, x0, y0, x1, y1, r, gr, b);
}

// original 0x444E00 (PSX 0x801DA578): the line under abr 1 (added).
extern "C" void __cdecl BattleWin_DrawLineAdd(int x0, int y0, int x1, int y1, int r, int gr, int b) {
    BlendLine(1, x0, y0, x1, y1, r, gr, b);
}

// original 0x444EB0 (PSX 0x801DA66C): 1 unless another battle window already
// shows an enemy of this one's kind. The kind is byte +0xC of enemy `enemy`'s
// record (0x93B9E0, stride 0x128); windows 5..12 carry an actor in +0xA
// (enemies are actors 3 and up). The highest of them showing this enemy is
// found (none: -1); every window above it whose state (+3) is not 2 or 3 and
// whose actor's kind byte is the same gives 0.
//
// As the original has it: a window above with an actor below 3 is looked up
// at a negative enemy index, before the records (0x93B674 for actor 0).
extern "C" unsigned char __cdecl BattleWin_FirstOfKind(unsigned enemy) {
    const unsigned e = enemy & 0xFF;
    const unsigned char kind = At(at::kEnemies + e * at::kEnemyStride + at::kEnemyKind)[0];
    const unsigned actor = e + 3;
    signed char found = 7;
    while (Window(5 + found)[0xA] != actor) {
        --found;
        if (found < 0) break;
    }
    if (found >= 7) return 1;
    signed char k = 7;
    do {
        const unsigned char* const w = Window(static_cast<unsigned>(5 + k));
        if (w[3] != 2 && w[3] != 3) {
            const std::int32_t index = static_cast<std::int32_t>(w[0xA]) - 3;
            const std::uint32_t at = at::kEnemies + at::kEnemyKind + static_cast<std::uint32_t>(index * static_cast<std::int32_t>(at::kEnemyStride));
            if (At(at)[0] == kind) return 0;
        }
        --k;
    } while (k > found);
    return 1;
}

// ===========================================================================
// The skill list's helpers. The ability table 0x65C4C8 is 0x18 bytes a
// record: the name in 16, then the PSX record's eight bytes (b0 at +0x10, the
// cost b2 at +0x12, u16_4 at +0x14 - the sibling's names/abilities.toml).

// original 0x57DA70 (GAME.EMI 0x801B05B0): whether member `member` may use
// ability `id` in list mode `mode`. Id 0: no. Mode 1 (the field's lists):
// the member's record through the party list 0x904062 and 0x66972C; no when
// its AP (+0x1A) is below Skill_ApCost or the ability's b0 lacks bit 0. Mode
// 2 (battle): the working record 0x802DC0 + member * 0x14C; no when its AP
// is below the cost, b0 lacks bit 1, or the member's +0x10 has bit 4 while
// the ability's u16_4 has 0x400; then five abilities have tests of their own
// - 0x14 only while +0x1E is below 5, 0x15 only while the four bytes at
// 0x904660 are all 0, 0x3E the same for 0x90465C, 0x8C only at 0 AP, 0x97
// not while 0x904AAA is 0x25. Any other mode: yes.
//
// As the original has it: in battle the AP is read before the cost call and
// the rest after it; in mode 1 the record is pushed as the member dword with
// its low byte replaced (the argument's own stack slot), and the AP is read
// after the call. The PSX tests the four bytes as one word each.
extern "C" unsigned char __cdecl Skill_CanUse(unsigned mode, unsigned member, unsigned id) {
    const unsigned ability = id & 0xFF;
    if (ability == 0) return 0;
    const unsigned char* const rec = At(at::kAbilities + ability * at::kAbilityStride);
    const unsigned m = mode & 0xFF;
    if (m == 1) {
        const unsigned character = At(at::kPartyLists + (member & 0xFF))[0];
        const unsigned record = At(at::kMemberRecord + character)[0];
        const unsigned cost = g.ap_cost((member & 0xFFFFFF00u) | record, id, 0) & 0xFF;
        if (Word(At(at::kCharRecords + record * at::kCharStride + 0x1A)) < cost) return 0;
        return (rec[0x10] & 1) ? 1 : 0;
    }
    if (m != 2) return 1;
    const unsigned char* const w = At(at::kWorking + (member & 0xFF) * at::kWorkingStride);
    const unsigned ap = Word(w + 0x1A);
    const unsigned cost = g.ap_cost(member, id, 1) & 0xFF;
    if (ap < cost) return 0;
    if ((rec[0x10] & 2) == 0) return 0;
    if ((w[0x10] & 0x10) != 0 && (rec[0x15] & 4) != 0) return 0;
    switch (ability) {
    case 0x14: return w[0x1E] < 5 ? 1 : 0;
    case 0x15: {
        const unsigned char* const f = At(at::kFlags15);
        return (f[0] | f[1] | f[2] | f[3]) == 0 ? 1 : 0;
    }
    case 0x3E: {
        const unsigned char* const f = At(at::kFlags3E);
        return (f[0] | f[1] | f[2] | f[3]) == 0 ? 1 : 0;
    }
    case 0x8C: return ap == 0 ? 1 : 0;
    case 0x97: return At(at::kBattle97)[0] == 0x25 ? 0 : 1;
    default: return 1;
    }
}

// original 0x57DC90 (GAME.EMI 0x801B0928): one row of the skill list - the
// type's 8 x 8 icon (table 0x663D70) at (x, y + 2), the name at (x + 10, y)
// for Text_CharCount characters, "%2d" of the cost's low byte in the 8 px font
// at (x + 0x6D, y + 2).
extern "C" void __cdecl Menu_DrawSkillRow(int x, int y, int colour, unsigned type, const unsigned char* name,
                                          unsigned cost, int dim) {
    const unsigned char icon = At(at::kSkillIcons + (type & 0xFF))[0];
    g.icon8(x, y + 2, icon, dim);
    const unsigned char n = g.char_count(name);
    g.text_draw_at(x + 10, y, colour, n, name);
    g.sprintf_(PrintBuf(), Format(at::kFmtCost), cost & 0xFF);
    g.font8(x + 0x6D, y + 2, colour, reinterpret_cast<const unsigned char*>(PrintBuf()));
}

// original 0x5918A0 (boot 0x80166A74): the lowest set bit of ability `id`'s
// u16_4 & 0x1FF, counted from 1; 0 when none is.
//
// As the original has it: the scan runs to 10 and would answer 10, which the
// mask makes unreachable.
extern "C" unsigned char __cdecl Skill_FlagIndex(unsigned id) {
    const unsigned flags = Word(At(at::kAbilities + (id & 0xFF) * at::kAbilityStride + 0x14));
    if ((flags & 0x1FF) == 0) return 0;
    unsigned bit = 1;
    unsigned char n = 1;
    for (;;) {
        if ((bit & flags & 0xFFFF) != 0) return n;
        bit <<= 1;
        ++n;
        if (n > 9) return n;
    }
}

// original 0x591DB0 (boot 0x80167398): ability `id`'s AP cost (b2, +0x12) for
// member `member` - of CharacterRecords when `battle`'s low byte is 0, else of
// the working record 0x802DC0 + member * 0x14C: halved, rounding up, when the
// record's +0x16 or +0x17 is 0x1A; else three quarters, rounding up, when
// either is 0x19.
extern "C" unsigned char __cdecl Skill_ApCost(unsigned member, unsigned id, unsigned battle) {
    const unsigned m = member & 0xFF;
    const unsigned char* const r =
        (battle & 0xFF) == 0 ? At(at::kCharRecords + m * at::kCharStride) : At(at::kWorking + m * at::kWorkingStride);
    const unsigned cost = At(at::kAbilities + (id & 0xFF) * at::kAbilityStride + 0x12)[0];
    const unsigned char a = r[0x16];
    if (a == 0x1A) return static_cast<unsigned char>((cost + 1) >> 1);
    const unsigned char b = r[0x17];
    if (b == 0x1A) return static_cast<unsigned char>((cost + 1) >> 1);
    if (a != 0x19 && b != 0x19) return static_cast<unsigned char>(cost);
    const unsigned three = cost * 3;
    if ((three & 3) != 0) return static_cast<unsigned char>((three >> 2) + 1);
    return static_cast<unsigned char>(three >> 2);
}

// original 0x591E50 (boot 0x80167464): member `member`'s ability list of type
// `type` - +0x6A for 1, +0x74 for 2, +0x7E for 3, +0x60 for anything else - of
// its CharacterRecords record through the party list 0x904062 and 0x66972C
// when `battle`'s low byte is 0, else of the working record 0x802DC0 +
// member * 0x14C. The PSX lists are 4 lower (+0x5C .. +0x7A,
// AbilityList_ForType 0x80167514, which takes an ability id for the type).
extern "C" unsigned char* __cdecl Char_AbilityList(unsigned member, unsigned type, unsigned battle) {
    unsigned char* r;
    if ((battle & 0xFF) == 0) {
        const unsigned character = At(at::kPartyLists + (member & 0xFF))[0];
        r = At(at::kCharRecords + At(at::kMemberRecord + character)[0] * at::kCharStride);
    } else {
        r = At(at::kWorking + (member & 0xFF) * at::kWorkingStride);
    }
    switch (type & 0xFF) {
    case 1: return r + 0x6A;
    case 2: return r + 0x74;
    case 3: return r + 0x7E;
    default: return r + 0x60;
    }
}

// ===========================================================================

void BattleWindowDraw_Inject() {
    g = kOriginals;
    if (bof3::WantsShadow("battle_window_draw")) SelfTest();

    BOF3_INJECT(BattleWin_OpenStatus);
    BOF3_INJECT(BattleWin_OpenSub1);
    BOF3_INJECT(BattleWin_OpenSub2);
    BOF3_INJECT(BattleWin_OpenSub3);
    BOF3_INJECT(BattleWin_DrawNumber);
    BOF3_INJECT(BattleWin_DrawQuadF4);
    BOF3_INJECT(BattleWin_DrawTile);
    BOF3_INJECT(BattleWin_DrawTileRgb);
    BOF3_INJECT(BattleWin_DrawBar);
    BOF3_INJECT(BattleWin_DrawCell16);
    BOF3_INJECT(BattleWin_DrawLine);
    BOF3_INJECT(BattleWin_DrawLineHalf);
    BOF3_INJECT(BattleWin_DrawLineAdd);
    BOF3_INJECT(BattleWin_FirstOfKind);
    BOF3_INJECT(Skill_CanUse);
    BOF3_INJECT(Menu_DrawSkillRow);
    BOF3_INJECT(Skill_FlagIndex);
    BOF3_INJECT(Skill_ApCost);
    BOF3_INJECT(Char_AbilityList);
}
