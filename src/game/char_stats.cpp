// Stats and inventory: group W of the sixth round, the functions in
// 0x5903F0..0x591CAB that the owner's shop route reaches, and two next to
// them. docs/char-stats.md.
//
//   - Char_RecalcStats 0x590660 (PSX 0x80165434) and its four passes: the
//     weapon 0x590FC0, the armour 0x591190, the accessories 0x591490 and the
//     per-character trait list 0x590800 (PSX 0x80166150 / 0x801662CC /
//     0x8016651C / 0x80165290), with the two byte steps under them,
//     Stat_AddResist 0x590EE0 and Stat_AddCap100 0x590F30 (PSX 0x80165FE4 /
//     0x80166084). 0x590EE0 is not on the queue (the shop route never calls
//     it) but every one of its 60 call sites is in this file.
//   - The equipment previews 0x590960 and its twin 0x590AB0 (no PSX twin):
//     the stat recompute on a copy of a record, compared with the record.
//     0x590AB0 is not on the queue either (unreached); taken as the twin.
//   - Inventory_Add 0x590BB0 (PSX 0x80165AA4), the counts 0x5919B0 /
//     0x591A80, the key-item test 0x5918E0, the item-table getters 0x591680 /
//     0x591720 / 0x5917A0 / 0x591C20, the text record copy 0x591940.
//   - Two menu draws: the icon quad 0x5903F0 (PSX 0x80164EC8, re-laid on the
//     PC) and the pointing hand Menu_DrawHand 0x5905D0 (PSX 0x801651B4).
//
// Every call goes through char_stats::g (char_stats_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike.
#include "game/char_stats.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/char_stats_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace char_stats {

using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

const Callees kOriginals = {
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_SetPolyFT4,
    Gpu_SetSprt,
    Stat_AddClamped,
    Stat_AddCap99,
    Stat_AddResist,
    Stat_AddCap100,
    Char_ApplyWeapon,
    Char_ApplyArmour,
    Char_ApplyAccessories,
    Char_ApplyTraits,
    Char_RecalcStats,
};
Callees g = kOriginals;

}  // namespace char_stats

using namespace char_stats;

namespace {

unsigned short* Stat(unsigned char* record, unsigned offset) { return reinterpret_cast<unsigned short*>(record + offset); }

// The inventory's list pointers for a category's low byte, read from the
// tables as the original reads them - unbounded (a category above 4 reads the
// neighbouring pointers, then the consumables' names).
unsigned char* IdList(unsigned cat) { return At(static_cast<std::uint32_t>(Long(At(at::kInvIds + 4 * cat)))); }
unsigned char* CountList(unsigned cat) { return At(static_cast<std::uint32_t>(Long(At(at::kInvCounts + 4 * cat)))); }

void SetFloat(unsigned char* at, int value) {
    const auto f = static_cast<float>(value);   // exact: every value here is below 2^24
    std::memcpy(at, &f, sizeof f);
}

}  // namespace

// ===========================================================================
// The two menu draws

// The icon quad's CLUT row per icon, the 21 bytes the original builds on its
// stack (PSX: 21 bytes copied from 0x80149C78).
namespace {
constexpr unsigned char kIconClut[21] = {8, 9, 8, 9, 8, 8, 8, 8, 8, 9, 9, 9, 8, 8, 9, 9, 8, 9, 9, 9, 9};

// The CLUT byte of an icon past the table: the original indexes its stack
// frame without a bound (`mov dl, [esp + ebx + 0x10]`), so icon b reads the
// byte at its entry ESP - 0x18 + b. Ours is entered at the same ESP (the
// detour is a jmp), `frame` is our EBP = entry ESP - 4. What the original
// holds there at that moment: 21..23 three stack bytes it never wrote (ours
// reads its own saved EBP there - the original's are undefined too); 24..27
// the return address; 28..31 its first argument's slot, into which it has
// stored the float x + w; 32..47 the next four arguments; 48..51 its last
// argument's slot, now y + h; 52 and up the caller's frame.
unsigned char IconClutPastTable(unsigned icon, const unsigned char* frame, int x1, int y1) {
    if (icon >= 28 && icon < 32) {
        const auto f = static_cast<float>(x1);
        unsigned char b[4];
        std::memcpy(b, &f, 4);
        return b[icon - 28];
    }
    if (icon >= 48 && icon < 52) {
        unsigned char b[4];
        std::memcpy(b, &y1, 4);
        return b[icon - 48];
    }
    return frame[static_cast<int>(icon) - 0x14];
}
}  // namespace

// original 0x5903F0 (PSX 0x80164EC8, re-laid): a 16 x 16 icon (15 x 15 past
// 20) from the menu texture as a POLY_FT4 at (x, y) sized w x h, shade r = g
// = b. Icons 0..11 from page 0xF at u = 20 * icon, v 0x50; 12..19 page 0x1E
// at u = 20 * icon + 16 (a byte, so it wraps), v 0x64; the rest one fixed
// cell of page 0x1E. The draw mode's page is 0xF below icon 12, else 0x1E.
// Gfx_PacketNext is read again after each commit, as in the original. x and
// y are the arguments' low words, w and h their low bytes.
// As the original has it: the CLUT row comes from a 21-byte table without a
// bound (IconClutPastTable). Not kept: the original also leaves x + w and
// y + h in its first and last argument slots - no caller reads its argument
// area after the call (the seven call sites, docs/char-stats.md).
extern "C" __attribute__((disable_tail_calls, noinline)) void __cdecl Menu_DrawIcon(unsigned icon, unsigned x, unsigned y, unsigned w,
                                                                                   unsigned h, unsigned shade) {
    const auto* const frame = static_cast<const unsigned char*>(__builtin_frame_address(0));
    const unsigned i = icon & 0xFF;
    g.set_draw_mode(Gfx_PacketNext, 0, 0, i < 0xC ? 0xF : 0x1E, 0);
    g.commit_prim(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    g.set_poly_ft4(p);
    const auto s = static_cast<unsigned char>(shade);
    p[4] = s;
    p[5] = s;
    p[6] = s;
    const int x0 = static_cast<int>(x & 0xFFFF), y0 = static_cast<int>(y & 0xFFFF);
    const int x1 = x0 + static_cast<int>(w & 0xFF), y1 = y0 + static_cast<int>(h & 0xFF);
    SetFloat(p + 0x08, x0);
    SetFloat(p + 0x0C, y0);
    SetFloat(p + 0x18, x1);
    SetFloat(p + 0x1C, y0);
    SetFloat(p + 0x28, x0);
    SetFloat(p + 0x2C, y1);
    SetFloat(p + 0x38, x1);
    SetFloat(p + 0x3C, y1);
    unsigned char u0, u1, v0, v1;
    unsigned tpage;
    if (i < 0xC) {
        u0 = static_cast<unsigned char>(i * 0x14);
        u1 = static_cast<unsigned char>(u0 + 0x10);
        v0 = 0x50;
        v1 = 0x60;
        tpage = 0xF;
    } else if (i < 0x14) {
        const auto base = static_cast<unsigned char>(i * 0x14);
        u0 = static_cast<unsigned char>(base + 0x10);
        u1 = static_cast<unsigned char>(base + 0x20);
        v0 = 0x64;
        v1 = 0x74;
        tpage = 0x1E;
    } else {
        u0 = 0xA8;
        u1 = 0xB7;
        v0 = 0xF0;
        v1 = 0xFF;
        tpage = 0x1E;
    }
    p[0x14] = u0;
    p[0x15] = v0;
    p[0x24] = u1;
    p[0x25] = v0;
    p[0x34] = u0;
    p[0x35] = v1;
    p[0x44] = u1;
    p[0x45] = v1;
    SetWord(p + 0x26, tpage);
    const unsigned char clut = i < sizeof kIconClut ? kIconClut[i] : IconClutPastTable(i, frame, x1, y1);
    SetWord(p + 0x16, 0x7800u | (clut & 0x3Fu));
    g.commit_prim(1, 0x48);
}

// original 0x5905D0 (PSX 0x801651B4): the pointing hand, a 0x18 x 0xC SPRT
// at (x - 0x16, y + 2) from (0xCC, 0x9C), CLUT word 0x7802, shade 0x80, under
// a draw mode of page 0xF. x and y are the arguments' low words; the third
// argument is not read.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Menu_DrawHand(int x, int y, int) {
    g.set_draw_mode(Gfx_PacketNext, 0, 0, 0xF, 0);
    g.commit_prim(0, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    g.set_sprt(p);
    SetFloat(p + 0x08, static_cast<int>(static_cast<unsigned>(x) & 0xFFFF) - 0x16);
    SetFloat(p + 0x0C, static_cast<int>(static_cast<unsigned>(y) & 0xFFFF) + 2);
    p[0x14] = 0xCC;
    p[0x15] = 0x9C;
    SetWord(p + 0x18, 0x18);
    SetWord(p + 0x1A, 0xC);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    SetWord(p + 0x16, 0x7802);
    g.commit_prim(0, 0x1C);
}

// ===========================================================================
// The stat recompute and its passes

// original 0x590660 (PSX 0x80165434): the effective block +0x20..+0x3F copied
// from the base block +0x40; the weapon, armour, accessory and trait passes;
// max HP less (base max HP * scale + 5) / 10 and HP cut to it; each bit of
// +0x1D steps one resistance byte by 2 (bit 5 is +0x35: +0x34 has no bit, on
// the PSX too); then, for the persistent record it is (any of the eight - the
// loop does not stop at the match), each non-zero byte of +0x38..+0x3C raised
// by that record's roster bonus 0x903640 + 5 * i. A record anywhere else (the
// previews' copies, the battle's working copies) gets no roster bonus.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Char_RecalcStats(unsigned char* record) {
    unsigned char* const r = record;
    std::memcpy(r + at::kEffective, r + at::kBase, 0x20);
    g.apply_weapon(r);
    g.apply_armour(r);
    g.apply_accessories(r);
    g.apply_traits(r);
    const int n = static_cast<int>(r[at::kHpScale]) * static_cast<int>(Word(r + at::kBase)) + 5;
    SetWord(r + at::kMaxHp, Word(r + at::kMaxHp) + n / -10);   // the original's magic divides by -10
    const std::uint16_t max = Word(r + at::kMaxHp);
    if (Word(r + at::kHp) > max) SetWord(r + at::kHp, max);
    static constexpr unsigned char kHalved[8] = {0x2F, 0x30, 0x31, 0x32, 0x33, 0x35, 0x36, 0x37};
    for (unsigned bit = 0; bit < 8; ++bit)
        if (r[at::kHalveBits] & (1u << bit)) g.add_resist(r + kHalved[bit], 2);
    for (unsigned i = 0; i < 8; ++i) {
        if (r != At(at::kCharRecords + i * at::kCharStride)) continue;
        for (unsigned j = 0; j < 5; ++j)
            if (r[at::kPercent + j] != 0) g.add_cap100(r + at::kPercent + j, At(at::kRosterBonus + 5 * i + j)[0]);
    }
}

// original 0x590800 (PSX 0x80165290): the record's trait byte +0x1F (0xFF:
// none) picks a list from 0x667548 (null: none) of (kind, amount) byte pairs
// ended by kind 0xE. Kinds 0..8 step the resistance byte +0x2F + kind
// (Stat_AddResist), 9..13 the byte +0x2F + kind in 0..100 (Stat_AddCap100);
// a kind above 13 is skipped. The amount's byte reaches the callee
// zero-extended.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Char_ApplyTraits(unsigned char* record) {
    const unsigned trait = record[at::kTrait];
    if (trait == 0xFF) return;
    const unsigned char* e = At(static_cast<std::uint32_t>(Long(At(at::kTraitLists + 4 * trait))));
    if (e == nullptr) return;
    unsigned kind = e[0];
    if (kind == 0xE) return;
    do {
        if (kind <= 0xD) {
            if (kind <= 8) g.add_resist(record + at::kResist + kind, e[1]);
            else g.add_cap100(record + at::kResist + kind, e[1]);
        }
        kind = e[2];
        e += 2;
    } while (kind != 0xE);
}

// original 0x590FC0 (PSX 0x80166150): the weapon (+0x12): ATK + its power
// (+0x16), AGI - its weight (+0x14), both through Stat_AddClamped; then the
// weapon's own effect by id. The id is read once.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Char_ApplyWeapon(unsigned char* record) {
    unsigned char* const r = record;
    const unsigned id = r[at::kWeapon];
    const unsigned char* const item = At(at::kWeapons + id * at::kWeaponStride);
    g.add_clamped(Stat(r, at::kAtk), item[0x16]);
    g.add_clamped(Stat(r, at::kAgi), 0u - item[0x14]);
    switch (id) {
    case 0x0D: case 0x43: g.add_cap100(r + 0x3A, 8); break;
    case 0x15:
        g.add_clamped(Stat(r, at::kDef), 5);
        g.add_clamped(Stat(r, at::kAgi), 5);
        g.add_clamped(Stat(r, at::kInt), 5);
        break;
    case 0x1A: case 0x44: g.add_resist(r + 0x37, 3); break;
    case 0x20: g.add_clamped(Stat(r, at::kInt), 3); break;
    case 0x25: g.add_clamped(Stat(r, at::kInt), 5); break;
    case 0x27: g.add_resist(r + 0x37, 1); break;
    case 0x28:
        g.add_clamped(Stat(r, at::kInt), 0xA);
        g.add_resist(r + 0x37, 1);
        g.add_resist(r + 0x36, 1);
        break;
    case 0x34: g.add_cap100(r + 0x3C, 0xA); break;
    case 0x41:
        g.add_cap100(r + 0x38, 5);
        g.add_cap100(r + 0x3A, 4);
        break;
    case 0x4D: g.add_cap100(r + 0x3C, 0x1E); break;
    default: break;
    }
}

namespace {
void Resist3(unsigned char* r, unsigned first, unsigned second, unsigned third, unsigned step) {
    g.add_resist(r + first, step);
    g.add_resist(r + second, step);
    g.add_resist(r + third, step);
}
}  // namespace

// original 0x591190 (PSX 0x801662CC): the three armour ids (+0x13..+0x15),
// copied first; for each, DEF + its defence (+0x14) and AGI - its weight
// (+0x13), then its effect by id.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Char_ApplyArmour(unsigned char* record) {
    unsigned char* const r = record;
    const unsigned char ids[3] = {r[at::kArmour], r[at::kArmour + 1], r[at::kArmour + 2]};
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned id = ids[k];
        const unsigned char* const item = At(at::kArmourTable + id * at::kArmourStride);
        g.add_clamped(Stat(r, at::kDef), item[0x14]);
        g.add_clamped(Stat(r, at::kAgi), 0u - item[0x13]);
        switch (id) {
        case 0x0C: case 0x2D: case 0x39: g.add_resist(r + 0x2F, 2); break;
        case 0x0F:
            g.add_resist(r + 0x2F, 0);
            g.add_resist(r + 0x30, 0xFFFFFFFFu);
            break;
        case 0x11:
            g.add_resist(r + 0x31, 2);
            g.add_resist(r + 0x32, 4);
            g.add_resist(r + 0x33, 2);
            break;
        case 0x13:
            g.add_resist(r + 0x34, 0xFFFFFFFDu);
            Resist3(r, 0x37, 0x36, 0x35, 0);
            break;
        case 0x14: case 0x3B: g.add_resist(r + 0x30, 2); break;
        case 0x16: g.add_clamped(Stat(r, at::kInt), 5); break;
        case 0x18: case 0x1D: case 0x2F: Resist3(r, 0x37, 0x35, 0x36, 2); break;
        case 0x19: g.add_clamped(Stat(r, at::kAgi), 5); break;
        case 0x1B: g.add_resist(r + 0x35, 0); break;
        case 0x1C: case 0x42: g.add_clamped(Stat(r, at::kAtk), 0xA); break;
        case 0x1E: case 0x31: case 0x41: g.add_resist(r + 0x2F, 0); break;
        case 0x20:
            g.add_resist(r + 0x34, 1);
            Resist3(r, 0x37, 0x35, 0x36, 2);
            break;
        case 0x28: g.add_clamped(Stat(r, 0x2C), 1); break;
        case 0x2C: case 0x3C: g.add_clamped(Stat(r, at::kAtk), 5); break;
        case 0x30: g.add_resist(r + 0x37, 0); break;
        case 0x3E: g.add_resist(r + 0x36, 2); break;
        case 0x40:
            g.add_resist(r + 0x35, 2);
            g.add_clamped(Stat(r, at::kInt), 5);
            break;
        default: break;
        }
    }
}

// original 0x591490 (PSX 0x8016651C): the two accessory ids (+0x16, +0x17),
// copied first; for each, AGI - its weight (+0x13), then its effect by id.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Char_ApplyAccessories(unsigned char* record) {
    unsigned char* const r = record;
    const unsigned char ids[2] = {r[at::kAccessory], r[at::kAccessory + 1]};
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned id = ids[k];
        const unsigned char* const item = At(at::kAccessories + id * at::kAccessoryStride);
        g.add_clamped(Stat(r, at::kAgi), 0u - item[0x13]);
        switch (id) {
        case 0x01: g.add_clamped(Stat(r, at::kAtk), 0xA); break;
        case 0x02: g.add_clamped(Stat(r, at::kDef), 5); break;
        case 0x03: g.add_clamped(Stat(r, at::kAgi), 0xA); break;
        case 0x04: g.add_clamped(Stat(r, at::kInt), 0x1E); break;
        case 0x05: g.add_cap99(r + 0x2E, 0xA); break;
        case 0x0A: g.add_resist(r + 0x30, 0); break;
        case 0x0B: g.add_resist(r + 0x2F, 0); break;
        case 0x0C: g.add_resist(r + 0x31, 0); break;
        case 0x0D: g.add_resist(r + 0x37, 1); break;
        case 0x0E: g.add_resist(r + 0x37, 3); break;
        case 0x0F: g.add_resist(r + 0x36, 1); break;
        case 0x10: g.add_resist(r + 0x36, 3); break;
        case 0x11: g.add_resist(r + 0x35, 3); break;
        case 0x12: g.add_cap100(r + 0x38, 0x14); break;
        case 0x13: g.add_cap100(r + 0x3C, 0xA); break;
        case 0x17:
            for (unsigned b = 0x2F; b <= 0x37; ++b) g.add_resist(r + b, 2);
            break;
        default: break;
        }
    }
}

// original 0x590EE0 (PSX 0x80165FE4): a resistance byte's step by the
// amount's low byte as s8. Up: refused at 6 or more, else add and cap at 6.
// Down: refused at 6 or more too, else add and floor at 0 (as s8). A step of
// 0 sets 7, refused at 7 or more. 1 when it wrote.
extern "C" unsigned char __cdecl Stat_AddResist(unsigned char* level, unsigned step) {
    const auto n = static_cast<signed char>(step);
    if (n > 0) {
        if (*level >= 6) return 0;
        const auto v = static_cast<unsigned char>(*level + n);
        *level = v;
        if (v > 6) *level = 6;
        return 1;
    }
    if (n < 0) {
        if (*level >= 6) return 0;
        const auto v = static_cast<unsigned char>(*level + n);
        *level = v;
        if (static_cast<signed char>(v) < 0) *level = 0;
        return 1;
    }
    if (*level >= 7) return 0;
    *level = 7;
    return 1;
}

// original 0x590F30 (PSX 0x80166084): a byte + the amount's low byte,
// refused at 100; below 0 as s8 it is 0, above 100 it is 100. 1 when it wrote.
extern "C" unsigned char __cdecl Stat_AddCap100(unsigned char* value, unsigned amount) {
    if (*value == 100) return 0;
    const auto v = static_cast<unsigned char>(*value + amount);
    *value = v;
    if (static_cast<signed char>(v) < 0) *value = 0;
    if (*value > 100) *value = 100;
    return 1;
}

// ===========================================================================
// The equipment previews

namespace {
// ATK, DEF, INT, AGI - the order the previews answer in (the menu's column
// order, docs/char-stats.md).
constexpr unsigned kPreviewStats[4] = {at::kAtk, at::kDef, at::kInt, at::kAgi};

// For each of the four: the mark 0 (same), 3 (the copy is higher) or 2 (the
// copy is lower), and the copy's value. Every word is read from memory where
// the original reads it, and the stores are in its order.
void ComparePreview(unsigned char* record, unsigned char* copy, unsigned char* marks, unsigned short* values) {
    for (unsigned k = 0; k < 4; ++k) {
        unsigned char* const now = record + kPreviewStats[k];
        unsigned char* const then = copy + kPreviewStats[k];
        marks[k] = 0;
        SetWord(reinterpret_cast<unsigned char*>(values + k), Word(then));
        if (Word(now) < Word(then)) marks[k] = 3;
        if (Word(now) > Word(then)) marks[k] = 2;
    }
}
}  // namespace

// original 0x590960 (no PSX twin): the persistent record id (its low byte)
// copied, equipment slot `slot` (0 weapon, 1..3 armour, 4..5 accessories;
// above 5 none) set to the item's low byte in the copy, the copy recomputed,
// and the four stats compared.
extern "C" void __cdecl Equip_PreviewSlot(unsigned id, unsigned slot, unsigned item, unsigned char* marks, unsigned short* values) {
    unsigned char* const record = At(at::kCharRecords + (id & 0xFF) * at::kCharStride);
    unsigned char copy[at::kRecordBytes];
    std::memcpy(copy, record, sizeof copy);
    if ((slot & 0xFF) <= 5) copy[at::kWeapon + (slot & 0xFF)] = static_cast<unsigned char>(item);
    g.recalc(copy);
    ComparePreview(record, copy, marks, values);
}

// original 0x590AB0 (no PSX twin): the same with all six equipment bytes
// from `set`.
extern "C" void __cdecl Equip_PreviewSet(unsigned id, const unsigned char* set, unsigned char* marks, unsigned short* values) {
    unsigned char* const record = At(at::kCharRecords + (id & 0xFF) * at::kCharStride);
    unsigned char copy[at::kRecordBytes];
    std::memcpy(copy, record, sizeof copy);
    for (unsigned k = 0; k < 6; ++k) copy[at::kWeapon + k] = set[k];
    g.recalc(copy);
    ComparePreview(record, copy, marks, values);
}

// ===========================================================================
// The inventory

// original 0x590BB0 (PSX 0x80165AA4): 0 for an item or count of 0 (their low
// bytes). DamageScratch (the PSX scratchpad's first byte) 0. Outside category
// 4, an existing stack of the item takes the count: above 99 it is 99 and the
// answer 0, else 1. Otherwise the first slot with id 0 - or, outside category
// 4, count 0 - takes the item (and the count), DamageScratch 1, answer 1; no
// slot, 0. The category's low byte indexes the list pointers unbounded, and
// category 4 (the key items, 32 bytes) is searched 128 long like the others,
// as on the PSX (D-NEW-W1).
extern "C" unsigned char __cdecl Inventory_Add(unsigned category, unsigned item, unsigned count) {
    const auto id = static_cast<unsigned char>(item);
    if (id == 0) return 0;
    const auto n = static_cast<unsigned char>(count);
    if (n == 0) return 0;
    const unsigned cat = category & 0xFF;
    unsigned char* const scratch = At(bof3::addr::DamageScratch);
    scratch[0] = 0;
    if (cat != 4) {
        unsigned char* const ids = IdList(cat);
        unsigned char* const counts = CountList(cat);
        for (unsigned i = 0; i < 0x80; ++i) {
            if (ids[i] != id) continue;
            const unsigned old = counts[i];
            if (static_cast<int>(old + n) > 0x63) {
                counts[i] = 0x63;
                return 0;
            }
            counts[i] = static_cast<unsigned char>(old + n);
            return 1;
        }
    }
    unsigned char* const ids = IdList(cat);
    unsigned char* const counts = CountList(cat);
    for (unsigned i = 0; i < 0x80; ++i) {
        if (cat == 4) {
            if (ids[i] != 0) continue;
            ids[i] = id;
            scratch[0] = 1;
            return 1;
        }
        if (ids[i] == 0 || counts[i] == 0) {
            ids[i] = id;
            counts[i] = n;
            scratch[0] = 1;
            return 1;
        }
    }
    return 0;
}

// original 0x5919B0 (PSX 0x80166C1C): with `equipped`'s low byte 0, the
// count of the first stack of the item in the category's list (0 if none) -
// category 4's count list is null, so a key item found there reads address
// i, as in the original (D-NEW-W2). Otherwise how many of the eight records
// with bit 0 of +0x0B wear it: category 1 the weapon, 2 the three armour
// bytes, 3 the two accessory bytes, any other none; category 3 also counts
// the byte 0x904130 once and adds 0x90412F when 0x90412E is the item. The
// answer is the low word the original defines.
extern "C" unsigned short __cdecl Inventory_Count(unsigned category, unsigned item, unsigned equipped) {
    const auto id = static_cast<unsigned char>(item);
    const unsigned cat = category & 0xFF;
    if ((equipped & 0xFF) == 0) {
        const unsigned char* const ids = IdList(cat);
        const std::uint32_t counts = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(CountList(cat)));
        for (unsigned i = 0; i < 0x80; ++i)
            if (ids[i] == id) return *reinterpret_cast<const volatile unsigned char*>(static_cast<std::uintptr_t>(counts + i));
        return 0;
    }
    unsigned char n = 0;
    for (unsigned k = 0; k < 8; ++k) {
        const unsigned char* const r = At(at::kCharRecords + k * at::kCharStride);
        if ((r[at::kFlags] & 1) == 0) continue;
        switch (cat) {
        case 1:
            if (r[at::kWeapon] == id) ++n;
            break;
        case 2:
            for (unsigned b = 0; b < 3; ++b)
                if (r[at::kArmour + b] == id) ++n;
            break;
        case 3:
            for (unsigned b = 0; b < 2; ++b)
                if (r[at::kAccessory + b] == id) ++n;
            break;
        default: break;
        }
    }
    if (cat == 3) {
        if (At(at::kExtraAccessory)[0] == id) ++n;
        if (At(at::kExtraItem)[0] == id) n = static_cast<unsigned char>(n + At(at::kExtraCount)[0]);
    }
    return n;
}

// original 0x591A80 (no PSX pair recorded): the non-zero ids in the
// category's list - 128 long below category 4, else 32 (the key items).
extern "C" unsigned char __cdecl Inventory_CountUsed(unsigned category) {
    const unsigned cat = category & 0xFF;
    const unsigned char* const ids = IdList(cat);
    const unsigned length = static_cast<unsigned char>(category) < 4 ? 0x80 : 0x20;
    unsigned char n = 0;
    for (unsigned i = 0; i < length; ++i)
        if (ids[i] != 0) ++n;
    return n;
}

// original 0x5918E0 (PSX 0x80166AD0): 1 if the item's low byte is among the
// 32 key-item bytes 0x904554.
extern "C" unsigned char __cdecl KeyItem_Has(unsigned item) {
    const auto id = static_cast<unsigned char>(item);
    for (unsigned i = 0; i < 0x20; ++i)
        if (At(at::kKeyItems)[i] == id) return 1;
    return 0;
}

// ===========================================================================
// The item tables: by category 1 weapons, 2 armour, 3 accessories, 4 key
// items, any other the consumables; the item's low byte indexes, unbounded.

// original 0x591680 (PSX 0x80166720, the sibling's Item_NamePtr): the record,
// whose first 16 bytes are the name.
extern "C" unsigned char* __cdecl Item_NamePtr(unsigned category, unsigned item) {
    const unsigned i = item & 0xFF;
    switch (category & 0xFF) {
    case 1: return At(at::kWeapons + i * at::kWeaponStride);
    case 2: return At(at::kArmourTable + i * at::kArmourStride);
    case 3: return At(at::kAccessories + i * at::kAccessoryStride);
    case 4: return At(at::kKeyItemTable + i * at::kKeyItemStride);
    default: return At(at::kConsumables + i * at::kConsumableStride);
    }
}

// original 0x591720 (PSX 0x801667D4): the low nibble of the equipment's byte
// +0x12, or of a consumable's +0x11 - every category but 1..3, the key items
// included, reads the consumables' table. The one caller read here indexes
// an icon table with it (0x57DC1B).
extern "C" unsigned __cdecl Item_IconKind(unsigned category, unsigned item) {
    const unsigned i = item & 0xFF;
    switch (category & 0xFF) {
    case 1: return At(at::kWeapons + i * at::kWeaponStride)[0x12] & 0xFu;
    case 2: return At(at::kArmourTable + i * at::kArmourStride)[0x12] & 0xFu;
    case 3: return At(at::kAccessories + i * at::kAccessoryStride)[0x12] & 0xFu;
    default: return At(at::kConsumables + i * at::kConsumableStride)[0x11] & 0xFu;
    }
}

// original 0x5917A0 (PSX 0x80166880): the equipment's byte +0x10 (a caller
// tests a member's bit in it, 0x5754A3); every other category 0xFF. The
// whole eax as the original leaves it: the armour's byte is loaded into al
// over the id * 13 its address arithmetic left in eax, and the other
// categories are `or al, 0xFF` on the category less 1, so 0xFFFFFFFF for
// category 0.
extern "C" unsigned __cdecl Item_EquipMask(unsigned category, unsigned item) {
    const unsigned i = item & 0xFF;
    const unsigned cat = category & 0xFF;
    switch (cat) {
    case 1: return At(at::kWeapons + i * at::kWeaponStride)[0x10];
    case 2: return ((i * 13) & ~0xFFu) | At(at::kArmourTable + i * at::kArmourStride)[0x10];   // eax held id * 13
    case 3: return At(at::kAccessories + i * at::kAccessoryStride)[0x10];
    default: return (cat - 1u) | 0xFFu;
    }
}

// original 0x591C20 (PSX 0x80167058): the price word - weapons +0x18, armour
// +0x16, accessories +0x14, key items +0x10, consumables +0x12.
extern "C" unsigned __cdecl Item_Price(unsigned category, unsigned item) {
    const unsigned i = item & 0xFF;
    switch (category & 0xFF) {
    case 1: return Word(At(at::kWeapons + i * at::kWeaponStride + 0x18));
    case 2: return Word(At(at::kArmourTable + i * at::kArmourStride + 0x16));
    case 3: return Word(At(at::kAccessories + i * at::kAccessoryStride + 0x14));
    case 4: return Word(At(at::kKeyItemTable + i * at::kKeyItemStride + 0x10));
    default: return Word(At(at::kConsumables + i * at::kConsumableStride + 0x12));
    }
}

// ===========================================================================
// The text record

// original 0x591940 (PSX 0x80166B9C): up to 31 bytes (the length's low byte,
// 32 and up cut to 31) copied a byte at a time, forwards, into Text_Records
// entry `slot` (its low byte), then a NUL after them. A source overlapping the
// entry below it repeats, as the original's byte loop does - so no memcpy
// (the volatile store keeps the compiler from making one of the loop).
extern "C" void __cdecl TextRecord_Set(unsigned slot, unsigned length, const unsigned char* text) {
    unsigned n = length & 0xFF;
    if (n >= 0x20) n = 0x1F;
    unsigned char* const d = At(at::kTextRecords + (slot & 0xFF) * 0x20);
    for (unsigned i = 0; i < n; ++i) *static_cast<volatile unsigned char*>(d + i) = *static_cast<const volatile unsigned char*>(text + i);
    d[n] = 0;
}

// ===========================================================================

void CharStats_Inject() {
    if (bof3::WantsShadow("char_stats")) char_stats::SelfTest();
    BOF3_INJECT(Menu_DrawIcon);
    BOF3_INJECT(Menu_DrawHand);
    BOF3_INJECT(Char_RecalcStats);
    BOF3_INJECT(Char_ApplyTraits);
    BOF3_INJECT(Equip_PreviewSlot);
    BOF3_INJECT(Equip_PreviewSet);
    BOF3_INJECT(Inventory_Add);
    BOF3_INJECT(Stat_AddResist);
    BOF3_INJECT(Stat_AddCap100);
    BOF3_INJECT(Char_ApplyWeapon);
    BOF3_INJECT(Char_ApplyArmour);
    BOF3_INJECT(Char_ApplyAccessories);
    BOF3_INJECT(Item_NamePtr);
    BOF3_INJECT(Item_IconKind);
    BOF3_INJECT(Item_EquipMask);
    BOF3_INJECT(KeyItem_Has);
    BOF3_INJECT(TextRecord_Set);
    BOF3_INJECT(Inventory_Count);
    BOF3_INJECT(Inventory_CountUsed);
    BOF3_INJECT(Item_Price);
}
