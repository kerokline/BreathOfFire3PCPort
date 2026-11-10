// Moved here by tools/move_funcs.py from battle_window_draw.cpp, char_stats.cpp, game_last.cpp, item_use.cpp, rest_2f.cpp, scena_se.cpp, scena_sx.cpp, scena_sx2.cpp (docs/source-layout-round.md).
#include "game/menu/char.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_window_draw.h"
#include "game/battle_window_draw_callees.h"
#include "game/char_stats_callees.h"
#include "game/game_last_callees.h"
#include "game/item_use_callees.h"
#include "game/list_title.h"
#include "game/map_rim.h"
#include "game/menu/char_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2f.h"
#include "game/rest_2f_callees.h"
#include "game/scena_se_callees.h"
#include "game/scena_sx2_callees.h"
#include "game/scena_sx_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = char_::at;

}  // namespace

using namespace battle_window_draw;

using namespace char_stats;

using namespace item_use;

namespace {

using move_script::At;

using move_script::Word;

}  // namespace

namespace char_stats {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

}  // namespace char_stats

namespace {

unsigned short* Stat(unsigned char* record, unsigned offset) { return reinterpret_cast<unsigned short*>(record + offset); }

// The inventory's list pointers for a category's low byte, read from the
// tables as the original reads them - unbounded (a category above 4 reads the
// neighbouring pointers, then the consumables' names).
unsigned char* IdList(unsigned cat) { return char_stats::At(static_cast<std::uint32_t>(Long(char_stats::At(::bof3::Img(char_stats::at::kInvIds.v) + 4 * cat)))); }

unsigned char* CountList(unsigned cat) { return char_stats::At(static_cast<std::uint32_t>(Long(char_stats::At(::bof3::Img(char_stats::at::kInvCounts.v) + 4 * cat)))); }

void SetFloat(unsigned char* at, int value) {
    const auto f = static_cast<float>(value);   // exact: every value here is below 2^24
    std::memcpy(at, &f, sizeof f);
}

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

void Resist3(unsigned char* r, unsigned first, unsigned second, unsigned third, unsigned step) {
    char_stats::g.add_resist(r + first, step);
    char_stats::g.add_resist(r + second, step);
    char_stats::g.add_resist(r + third, step);
}

// ATK, DEF, INT, AGI - the order the previews answer in (the menu's column
// order, docs/char-stats.md).
constexpr unsigned kPreviewStats[4] = {char_stats::at::kAtk, char_stats::at::kDef, char_stats::at::kInt, char_stats::at::kAgi};

// For each of the four: the mark 0 (same), 3 (the copy is higher) or 2 (the
// copy is lower), and the copy's value. Every word is read from memory where
// the original reads it, and the stores are in its order.
void ComparePreview(unsigned char* record, unsigned char* copy, unsigned char* marks, unsigned short* values) {
    for (unsigned k = 0; k < 4; ++k) {
        unsigned char* const now = record + kPreviewStats[k];
        unsigned char* const then = copy + kPreviewStats[k];
        marks[k] = 0;
        char_stats::SetWord(reinterpret_cast<unsigned char*>(values + k), char_stats::Word(then));
        if (char_stats::Word(now) < char_stats::Word(then)) marks[k] = 3;
        if (char_stats::Word(now) > char_stats::Word(then)) marks[k] = 2;
    }
}

}  // namespace

namespace item_use {

using move_script::At;

using move_script::SetWord;

using move_script::Word;

}  // namespace item_use

namespace {

// The record a (id, battle) pair names, as every function here computes it:
// the battle flag's low byte picks the persistent record or the member's
// working copy, the id's low byte indexes it, unchecked.
unsigned char* CharRecord(unsigned id, unsigned battle) {
    const unsigned i = id & 0xFF;
    return (battle & 0xFF) == 0 ? item_use::At(::bof3::Img(CharacterRecords_addr) + i * item_use::at::kCharStride) : item_use::At(::bof3::Img(item_use::at::kWorkRecords.v) + i * item_use::at::kWorkStride);
}

// Either answer is a records::CharRecord: the persistent record 0x903A70 + i
// * 0xA4, or the working copy 0x802DC0 + i * 0x14C, which is PartyMember
// +0x80, a copy of the character record (records.h CharRecord, PartyMember;
// docs/item-use.md section 2). The fields here are the record's +0x10,
// +0x18, +0x1A, +0x1E, +0x20, +0x22 (item_use_callees.h at::kStatus..kMaxAp).
records::CharRecord* CharRec(unsigned id, unsigned battle) { return rv::As<records::CharRecord>(CharRecord(id, battle)); }

using U = std::uint32_t;

// kCharacters: now CharacterRecords (symbols.toml). CharacterRecords: 0xA4 a member
constexpr U kCharacterStride = 0xA4;

constexpr ::bof3::image::Va kWorking{0x802DC0};     // the party's working records' +0x80 (ObjTrio + 0x80), 0x14C a slot

constexpr U kWorkingStride = 0x14C;

unsigned char& BRest2f(U address) { return move_script::At(address)[0]; }

// A character's record +0xB (CharacterRecords, stride 0xA4) through
// Character_IndexById[id] - both indexes a whole byte, unchecked.
unsigned char* RecordFlags(unsigned char id) {
    return move_script::At(::bof3::Img(CharacterRecords_addr) + 0xB + Character_IndexById[id] * scena_se::at::kObjectStride);
}

// move_script::At is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::Long;

using move_script::SetLong;

constexpr auto& AtScenaSx2 = move_script::At;

}  // namespace

// char_stats's callee seam, moved here from char_stats.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace char_stats {

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
    char_stats::g.set_draw_mode(Gfx_PacketNext, 0, 0, i < 0xC ? 0xF : 0x1E, 0);
    char_stats::g.commit_prim(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    char_stats::g.set_poly_ft4(p);
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
    rv::As<records::GpuPolyFT4>(p)->tpage = tpage;
    const unsigned char clut = i < sizeof kIconClut ? kIconClut[i] : IconClutPastTable(i, frame, x1, y1);
    rv::As<records::GpuPolyFT4>(p)->clut = 0x7800u | (clut & 0x3Fu);
    char_stats::g.commit_prim(1, 0x48);
}

// original 0x5905D0 (PSX 0x801651B4): the pointing hand, a 0x18 x 0xC SPRT
// at (x - 0x16, y + 2) from (0xCC, 0x9C), CLUT word 0x7802, shade 0x80, under
// a draw mode of page 0xF. x and y are the arguments' low words; the third
// argument is not read.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Menu_DrawHand(int x, int y, int) {
    char_stats::g.set_draw_mode(Gfx_PacketNext, 0, 0, 0xF, 0);
    char_stats::g.commit_prim(0, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    char_stats::g.set_sprt(p);
    SetFloat(p + 0x08, static_cast<int>(static_cast<unsigned>(x) & 0xFFFF) - 0x16);
    SetFloat(p + 0x0C, static_cast<int>(static_cast<unsigned>(y) & 0xFFFF) + 2);
    p[0x14] = 0xCC;
    p[0x15] = 0x9C;
    rv::As<records::GpuSprt>(p)->w = 0x18;
    rv::As<records::GpuSprt>(p)->h = 0xC;
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    rv::As<records::GpuSprt>(p)->clut = 0x7802;
    char_stats::g.commit_prim(0, 0x1C);
}

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
    std::memcpy(r + char_stats::at::kEffective, r + char_stats::at::kBase, 0x20);
    char_stats::g.apply_weapon(r);
    char_stats::g.apply_armour(r);
    char_stats::g.apply_accessories(r);
    char_stats::g.apply_traits(r);
    const int n = static_cast<int>(r[char_stats::at::kHpScale]) * static_cast<int>(char_stats::Word(r + char_stats::at::kBase)) + 5;
    char_stats::SetWord(r + char_stats::at::kMaxHp, char_stats::Word(r + char_stats::at::kMaxHp) + n / -10);   // the original's magic divides by -10
    const std::uint16_t max = char_stats::Word(r + char_stats::at::kMaxHp);
    if (char_stats::Word(r + char_stats::at::kHp) > max) char_stats::SetWord(r + char_stats::at::kHp, max);
    static constexpr unsigned char kHalved[8] = {0x2F, 0x30, 0x31, 0x32, 0x33, 0x35, 0x36, 0x37};
    for (unsigned bit = 0; bit < 8; ++bit)
        if (r[char_stats::at::kHalveBits] & (1u << bit)) char_stats::g.add_resist(r + kHalved[bit], 2);
    for (unsigned i = 0; i < 8; ++i) {
        if (r != char_stats::At(::bof3::Img(CharacterRecords_addr) + i * char_stats::at::kCharStride)) continue;
        for (unsigned j = 0; j < 5; ++j)
            if (r[char_stats::at::kPercent + j] != 0) char_stats::g.add_cap100(r + char_stats::at::kPercent + j, char_stats::At(::bof3::Img(char_stats::at::kRosterBonus.v) + 5 * i + j)[0]);
    }
}

// original 0x590800 (PSX 0x80165290): the record's trait byte +0x1F (0xFF:
// none) picks a list from 0x667548 (null: none) of (kind, amount) byte pairs
// ended by kind 0xE. Kinds 0..8 step the resistance byte +0x2F + kind
// (Stat_AddResist), 9..13 the byte +0x2F + kind in 0..100 (Stat_AddCap100);
// a kind above 13 is skipped. The amount's byte reaches the callee
// zero-extended.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Char_ApplyTraits(unsigned char* record) {
    const unsigned trait = record[char_stats::at::kMaster];
    if (trait == 0xFF) return;
    const unsigned char* e = char_stats::At(static_cast<std::uint32_t>(Long(char_stats::At(::bof3::Img(char_stats::at::kTraitLists.v) + 4 * trait))));
    if (e == nullptr) return;
    unsigned kind = e[0];
    if (kind == 0xE) return;
    do {
        if (kind <= 0xD) {
            if (kind <= 8) char_stats::g.add_resist(record + char_stats::at::kResist + kind, e[1]);
            else char_stats::g.add_cap100(record + char_stats::at::kResist + kind, e[1]);
        }
        kind = e[2];
        e += 2;
    } while (kind != 0xE);
}

// original 0x590960 (no PSX twin): the persistent record id (its low byte)
// copied, equipment slot `slot` (0 weapon, 1..3 armour, 4..5 accessories;
// above 5 none) set to the item's low byte in the copy, the copy recomputed,
// and the four stats compared.
extern "C" void __cdecl Equip_PreviewSlot(unsigned id, unsigned slot, unsigned item, unsigned char* marks, unsigned short* values) {
    unsigned char* const record = char_stats::At(::bof3::Img(CharacterRecords_addr) + (id & 0xFF) * char_stats::at::kCharStride);
    unsigned char copy[char_stats::at::kRecordBytes];
    std::memcpy(copy, record, sizeof copy);
    if ((slot & 0xFF) <= 5) copy[char_stats::at::kWeapon + (slot & 0xFF)] = static_cast<unsigned char>(item);
    char_stats::g.recalc(copy);
    ComparePreview(record, copy, marks, values);
}

// original 0x590AB0 (no PSX twin): the same with all six equipment bytes
// from `set`.
extern "C" void __cdecl Equip_PreviewSet(unsigned id, const unsigned char* set, unsigned char* marks, unsigned short* values) {
    unsigned char* const record = char_stats::At(::bof3::Img(CharacterRecords_addr) + (id & 0xFF) * char_stats::at::kCharStride);
    unsigned char copy[char_stats::at::kRecordBytes];
    std::memcpy(copy, record, sizeof copy);
    for (unsigned k = 0; k < 6; ++k) copy[char_stats::at::kWeapon + k] = set[k];
    char_stats::g.recalc(copy);
    ComparePreview(record, copy, marks, values);
}

// original 0x590BB0 (PSX 0x80165AA4): 0 for an item or count of 0 (their low
// bytes). SharedScratch (the PSX scratchpad's first byte) 0. Outside category
// 4, an existing stack of the item takes the count: above 99 it is 99 and the
// answer 0, else 1. Otherwise the first slot with id 0 - or, outside category
// 4, count 0 - takes the item (and the count), SharedScratch 1, answer 1; no
// slot, 0. The category's low byte indexes the list pointers unbounded, and
// category 4 (the key items, 32 bytes) is searched 128 long like the others,
// as on the PSX (D34).
extern "C" unsigned char __cdecl Inventory_Add(unsigned category, unsigned item, unsigned count) {
    const auto id = static_cast<unsigned char>(item);
    if (id == 0) return 0;
    const auto n = static_cast<unsigned char>(count);
    if (n == 0) return 0;
    const unsigned cat = category & 0xFF;
    unsigned char* const scratch = char_stats::At(::bof3::Img(SharedScratch_addr));
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

// original 0x590C90: an ability id into a list (PSX AbilityList_Add
// 0x80165BCC, the sibling's name; pairs_propagated "gap44"). An id of 0
// (low byte): al 0. With the third argument's byte 0: the member's list,
// 0x591EC0(member, id, fourth) - one of four 10-byte lists of its record by
// the id's class - 10 slots; else the shared list 0x904574, 128 slots. The
// first 0 slot takes the id, al 1; none, al 0.
extern "C" unsigned char __cdecl AbilityList_Add(unsigned id_arg, unsigned member, unsigned shared, unsigned which) {
    const auto id = static_cast<unsigned char>(id_arg);
    if (id == 0) return 0;
    unsigned char* list;
    unsigned slots;
    if (static_cast<unsigned char>(shared) == 0) {
        list = SH_AT(unsigned char* (__cdecl*)(unsigned, unsigned, unsigned), scena_sx::at::kAbilityListOf.v)(member, id_arg, which);
        slots = 10;
    } else {
        list = move_script::At(::bof3::Img(scena_sx::at::kAbilityShared));
        slots = 0x80;
    }
    for (unsigned i = 0; i < slots; ++i) {
        if (list[i] != 0) continue;
        list[i] = id;
        return 1;
    }
    return 0;
}

// original 0x590CE0 (PSX 0x80165C70): 0 at full HP. Else HP += the amount's
// low word, filled to max when it passes max or the amount is 0, and the
// status bit 0x2000 cleared unless HP is below a quarter of max; 1.
// As the original has it: the addition wraps at 16 bits (HP 0xFFF0 + 0x20 is
// 0x10, below max, kept), and Char_ClearStatus gets the id and battle dwords
// as they were pushed.
extern "C" unsigned char __cdecl Char_HealHp(unsigned id, unsigned amount, unsigned battle) {
    auto* const r = CharRec(id, battle);
    if (r->hp == r->stats.max_hp) return 0;
    rv::Store<unsigned>(r->hp, r->hp + amount);
    if (r->hp > r->stats.max_hp || static_cast<std::uint16_t>(amount) == 0)
        rv::Store<unsigned>(r->hp, r->stats.max_hp);
    if (static_cast<std::uint16_t>(r->stats.max_hp >> 2) <= r->hp) item_use::g.clear_status(id, 0x2000, battle);
    return 1;
}

// original 0x590D70 (PSX 0x80165D3C): Char_HealHp on AP, without the status.
extern "C" unsigned char __cdecl Char_HealAp(unsigned id, unsigned amount, unsigned battle) {
    auto* const r = CharRec(id, battle);
    if (r->ap == r->stats.max_ap) return 0;
    rv::Store<unsigned>(r->ap, r->ap + amount);
    if (r->ap > r->stats.max_ap || static_cast<std::uint16_t>(amount) == 0)
        rv::Store<unsigned>(r->ap, r->stats.max_ap);
    return 1;
}

// original 0x590DE0 (PSX 0x80165E5C): 0 at 999; else add (wrapping at 16
// bits), 999 if above, 1.
extern "C" unsigned char __cdecl Stat_AddCap999(unsigned short* stat, unsigned amount) {
    auto* const p = reinterpret_cast<unsigned char*>(stat);
    const std::uint16_t v = item_use::Word(p);
    if (v == 999) return 0;
    const auto sum = static_cast<std::uint16_t>(v + amount);
    item_use::SetWord(p, sum);
    if (sum > 999) item_use::SetWord(p, 999);
    return 1;
}

// original 0x590E10 (PSX 0x80165EA0): the same on a byte, capped at 99.
extern "C" unsigned char __cdecl Stat_AddCap99(unsigned char* stat, unsigned amount) {
    const unsigned char v = *stat;
    if (v == 99) return 0;
    const auto sum = static_cast<unsigned char>(v + amount);
    *stat = sum;
    if (sum > 99) *stat = 99;
    return 1;
}

// original 0x590E30 (PSX 0x80165EE4): the delta as s16. Up: 0 at 999, else
// add and, past 999 (unsigned), store 999 and answer 999 - old. Down: 0 at 0,
// else add and, below 0 (s16), store 0 and answer -old. 0 otherwise - an
// unclamped change answers 0, as on the PSX. Only the low word of the answer
// is defined in the original (ax), so a short.
extern "C" short __cdecl Stat_AddClamped(unsigned short* stat, unsigned delta) {
    auto* const p = reinterpret_cast<unsigned char*>(stat);
    const auto d = static_cast<std::int16_t>(delta);
    const std::uint16_t v = item_use::Word(p);
    if (d > 0) {
        if (v == 999) return 0;
        const auto sum = static_cast<std::uint16_t>(v + static_cast<std::uint16_t>(d));
        item_use::SetWord(p, sum);
        if (sum <= 999) return 0;
        item_use::SetWord(p, 999);
        return static_cast<short>(999 - v);
    }
    if (d == 0) return 0;
    if (v == 0) return 0;
    const auto sum = static_cast<std::uint16_t>(v + static_cast<std::uint16_t>(d));
    item_use::SetWord(p, sum);
    if (static_cast<std::int16_t>(sum) >= 0) return 0;
    item_use::SetWord(p, 0);
    return static_cast<short>(-static_cast<int>(v));
}

// original 0x590E80 (PSX 0x80165F60, pairs_propagated gap47): *stat (u16) +=
// delta's low word as s16, clamped to [0, cap's low word] (unsigned against
// the cap); answers the change made: 0 in ax when the stat is already at the
// cap (a rise) or at 0 (a fall), or the delta is 0; the delta when nothing was
// clamped; cap - old or -old when clamped. Callers (Effect_DrainHp /
// Effect_DrainAp) read the answer's low word; the original's upper half is
// the delta's, or in the clamped answers its caller's ecx's (not reproducible:
// ours leaves cap's upper half there for a rise, 0xFFFF for a fall).
extern "C" unsigned __cdecl Stat_AddClampedTo(unsigned short* stat, unsigned cap, unsigned delta) {
    const U old = *stat;
    const auto d = static_cast<short>(delta);
    if (d > 0) {
        if (old == (cap & 0xFFFF)) return delta & 0xFFFF0000u;
        const U sum = old + delta;
        *stat = static_cast<unsigned short>(sum);
        if ((sum & 0xFFFF) <= (cap & 0xFFFF)) return delta;
        *stat = static_cast<unsigned short>(cap);
        return cap - old;
    }
    if (d < 0) {
        if (old == 0) return delta & 0xFFFF0000u;
        const U sum = old + delta;
        *stat = static_cast<unsigned short>(sum);
        if (static_cast<short>(sum) >= 0) return delta;
        *stat = 0;
        return 0u - old;
    }
    return delta & 0xFFFF0000u;
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

// original 0x590F60 (PSX 0x801660DC): clear the mask's bits from the status
// word if any is set, and say so. Only the mask's low word reaches the word.
extern "C" unsigned char __cdecl Char_ClearStatus(unsigned id, unsigned mask, unsigned battle) {
    auto* const r = CharRec(id, battle);
    const std::uint16_t status = r->status;
    if ((status & mask & 0xFFFF) == 0) return 0;
    rv::Store<unsigned>(r->status, status & ~mask);
    return 1;
}

// original 0x590FC0 (PSX 0x80166150): the weapon (+0x12): ATK + its power
// (+0x16), AGI - its weight (+0x14), both through Stat_AddClamped; then the
// weapon's own effect by id. The id is read once.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Char_ApplyWeapon(unsigned char* record) {
    unsigned char* const r = record;
    const unsigned id = r[char_stats::at::kWeapon];
    const unsigned char* const item = char_stats::At(::bof3::Img(char_stats::at::kWeapons.v) + id * char_stats::at::kWeaponStride);
    char_stats::g.add_clamped(Stat(r, char_stats::at::kAtk), item[0x16]);
    char_stats::g.add_clamped(Stat(r, char_stats::at::kAgi), 0u - item[0x14]);
    switch (id) {
    case 0x0D: case 0x43: char_stats::g.add_cap100(r + 0x3A, 8); break;
    case 0x15:
        char_stats::g.add_clamped(Stat(r, char_stats::at::kDef), 5);
        char_stats::g.add_clamped(Stat(r, char_stats::at::kAgi), 5);
        char_stats::g.add_clamped(Stat(r, char_stats::at::kInt), 5);
        break;
    case 0x1A: case 0x44: char_stats::g.add_resist(r + 0x37, 3); break;
    case 0x20: char_stats::g.add_clamped(Stat(r, char_stats::at::kInt), 3); break;
    case 0x25: char_stats::g.add_clamped(Stat(r, char_stats::at::kInt), 5); break;
    case 0x27: char_stats::g.add_resist(r + 0x37, 1); break;
    case 0x28:
        char_stats::g.add_clamped(Stat(r, char_stats::at::kInt), 0xA);
        char_stats::g.add_resist(r + 0x37, 1);
        char_stats::g.add_resist(r + 0x36, 1);
        break;
    case 0x34: char_stats::g.add_cap100(r + 0x3C, 0xA); break;
    case 0x41:
        char_stats::g.add_cap100(r + 0x38, 5);
        char_stats::g.add_cap100(r + 0x3A, 4);
        break;
    case 0x4D: char_stats::g.add_cap100(r + 0x3C, 0x1E); break;
    default: break;
    }
}

// original 0x591190 (PSX 0x801662CC): the three armour ids (+0x13..+0x15),
// copied first; for each, DEF + its defence (+0x14) and AGI - its weight
// (+0x13), then its effect by id.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Char_ApplyArmour(unsigned char* record) {
    unsigned char* const r = record;
    const unsigned char ids[3] = {r[char_stats::at::kArmour], r[char_stats::at::kArmour + 1], r[char_stats::at::kArmour + 2]};
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned id = ids[k];
        const unsigned char* const item = char_stats::At(::bof3::Img(char_stats::at::kArmourTable.v) + id * char_stats::at::kArmourStride);
        char_stats::g.add_clamped(Stat(r, char_stats::at::kDef), item[0x14]);
        char_stats::g.add_clamped(Stat(r, char_stats::at::kAgi), 0u - item[0x13]);
        switch (id) {
        case 0x0C: case 0x2D: case 0x39: char_stats::g.add_resist(r + 0x2F, 2); break;
        case 0x0F:
            char_stats::g.add_resist(r + 0x2F, 0);
            char_stats::g.add_resist(r + 0x30, 0xFFFFFFFFu);
            break;
        case 0x11:
            char_stats::g.add_resist(r + 0x31, 2);
            char_stats::g.add_resist(r + 0x32, 4);
            char_stats::g.add_resist(r + 0x33, 2);
            break;
        case 0x13:
            char_stats::g.add_resist(r + 0x34, 0xFFFFFFFDu);
            Resist3(r, 0x37, 0x36, 0x35, 0);
            break;
        case 0x14: case 0x3B: char_stats::g.add_resist(r + 0x30, 2); break;
        case 0x16: char_stats::g.add_clamped(Stat(r, char_stats::at::kInt), 5); break;
        case 0x18: case 0x1D: case 0x2F: Resist3(r, 0x37, 0x35, 0x36, 2); break;
        case 0x19: char_stats::g.add_clamped(Stat(r, char_stats::at::kAgi), 5); break;
        case 0x1B: char_stats::g.add_resist(r + 0x35, 0); break;
        case 0x1C: case 0x42: char_stats::g.add_clamped(Stat(r, char_stats::at::kAtk), 0xA); break;
        case 0x1E: case 0x31: case 0x41: char_stats::g.add_resist(r + 0x2F, 0); break;
        case 0x20:
            char_stats::g.add_resist(r + 0x34, 1);
            Resist3(r, 0x37, 0x35, 0x36, 2);
            break;
        case 0x28: char_stats::g.add_clamped(Stat(r, 0x2C), 1); break;
        case 0x2C: case 0x3C: char_stats::g.add_clamped(Stat(r, char_stats::at::kAtk), 5); break;
        case 0x30: char_stats::g.add_resist(r + 0x37, 0); break;
        case 0x3E: char_stats::g.add_resist(r + 0x36, 2); break;
        case 0x40:
            char_stats::g.add_resist(r + 0x35, 2);
            char_stats::g.add_clamped(Stat(r, char_stats::at::kInt), 5);
            break;
        default: break;
        }
    }
}

// original 0x591490 (PSX 0x8016651C): the two accessory ids (+0x16, +0x17),
// copied first; for each, AGI - its weight (+0x13), then its effect by id.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Char_ApplyAccessories(unsigned char* record) {
    unsigned char* const r = record;
    const unsigned char ids[2] = {r[char_stats::at::kAccessory], r[char_stats::at::kAccessory + 1]};
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned id = ids[k];
        const unsigned char* const item = char_stats::At(::bof3::Img(char_stats::at::kAccessories.v) + id * char_stats::at::kAccessoryStride);
        char_stats::g.add_clamped(Stat(r, char_stats::at::kAgi), 0u - item[0x13]);
        switch (id) {
        case 0x01: char_stats::g.add_clamped(Stat(r, char_stats::at::kAtk), 0xA); break;
        case 0x02: char_stats::g.add_clamped(Stat(r, char_stats::at::kDef), 5); break;
        case 0x03: char_stats::g.add_clamped(Stat(r, char_stats::at::kAgi), 0xA); break;
        case 0x04: char_stats::g.add_clamped(Stat(r, char_stats::at::kInt), 0x1E); break;
        case 0x05: char_stats::g.add_cap99(r + 0x2E, 0xA); break;
        case 0x0A: char_stats::g.add_resist(r + 0x30, 0); break;
        case 0x0B: char_stats::g.add_resist(r + 0x2F, 0); break;
        case 0x0C: char_stats::g.add_resist(r + 0x31, 0); break;
        case 0x0D: char_stats::g.add_resist(r + 0x37, 1); break;
        case 0x0E: char_stats::g.add_resist(r + 0x37, 3); break;
        case 0x0F: char_stats::g.add_resist(r + 0x36, 1); break;
        case 0x10: char_stats::g.add_resist(r + 0x36, 3); break;
        case 0x11: char_stats::g.add_resist(r + 0x35, 3); break;
        case 0x12: char_stats::g.add_cap100(r + 0x38, 0x14); break;
        case 0x13: char_stats::g.add_cap100(r + 0x3C, 0xA); break;
        case 0x17:
            for (unsigned b = 0x2F; b <= 0x37; ++b) char_stats::g.add_resist(r + b, 2);
            break;
        default: break;
        }
    }
}

// original 0x591680 (PSX 0x80166720, the sibling's Item_NamePtr): the record,
// whose first 16 bytes are the name.
extern "C" unsigned char* __cdecl Item_NamePtr(unsigned category, unsigned item) {
    const unsigned i = item & 0xFF;
    switch (category & 0xFF) {
    case 1: return char_stats::At(::bof3::Img(char_stats::at::kWeapons.v) + i * char_stats::at::kWeaponStride);
    case 2: return char_stats::At(::bof3::Img(char_stats::at::kArmourTable.v) + i * char_stats::at::kArmourStride);
    case 3: return char_stats::At(::bof3::Img(char_stats::at::kAccessories.v) + i * char_stats::at::kAccessoryStride);
    case 4: return char_stats::At(::bof3::Img(char_stats::at::kKeyItemTable.v) + i * char_stats::at::kKeyItemStride);
    default: return char_stats::At(::bof3::Img(char_stats::at::kConsumables.v) + i * char_stats::at::kConsumableStride);
    }
}

// original 0x591720 (PSX 0x801667D4): the low nibble of the equipment's byte
// +0x12, or of a consumable's +0x11 - every category but 1..3, the key items
// included, reads the consumables' table. The one caller read here indexes
// an icon table with it (0x57DC1B).
extern "C" unsigned __cdecl Item_IconKind(unsigned category, unsigned item) {
    const unsigned i = item & 0xFF;
    switch (category & 0xFF) {
    case 1: return char_stats::At(::bof3::Img(char_stats::at::kWeapons.v) + i * char_stats::at::kWeaponStride)[0x12] & 0xFu;
    case 2: return char_stats::At(::bof3::Img(char_stats::at::kArmourTable.v) + i * char_stats::at::kArmourStride)[0x12] & 0xFu;
    case 3: return char_stats::At(::bof3::Img(char_stats::at::kAccessories.v) + i * char_stats::at::kAccessoryStride)[0x12] & 0xFu;
    default: return char_stats::At(::bof3::Img(char_stats::at::kConsumables.v) + i * char_stats::at::kConsumableStride)[0x11] & 0xFu;
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
    case 1: return char_stats::At(::bof3::Img(char_stats::at::kWeapons.v) + i * char_stats::at::kWeaponStride)[0x10];
    case 2: return ((i * 13) & ~0xFFu) | char_stats::At(::bof3::Img(char_stats::at::kArmourTable.v) + i * char_stats::at::kArmourStride)[0x10];   // eax held id * 13
    case 3: return char_stats::At(::bof3::Img(char_stats::at::kAccessories.v) + i * char_stats::at::kAccessoryStride)[0x10];
    default: return (cat - 1u) | 0xFFu;
    }
}

// original 0x591810 (PSX 0x80166918): by the category's low byte through the
// 4-entry table 0x591888 (dec; cmp 3; ja default):
//   1  the byte at 0x657461 + 28 * id (a weapon's +0x11)
//   2  the byte at 0x657D79 + 26 * id (armour's +0x11)
//   3  the byte at 0x658461 + 24 * id (an accessory's +0x11)
//   4  0 (xor al, al over the category less 1, 3: eax 0)
//   any other (0, 5..255)  the byte at 0x656B38 + 22 * id (a consumable's flags' low byte)
// id is the item's low byte. The whole eax as the original leaves it - its
// callers read al: for 1 and 3 the byte is loaded over the id (below 0x100),
// for 2 over id * 13 (lea edx, [eax+eax*2]; lea eax, [eax+edx*4]) and for the
// default over id * 11 (lea edx, [eax+eax*4]; lea eax, [eax+edx*2]), so their
// upper bytes are that product's.
extern "C" unsigned __cdecl Item_UseFlags(unsigned category, unsigned item) {
    const unsigned id = item & 0xFF;
    switch (category & 0xFF) {
    case 1: return move_script::At(::bof3::Img(game_last::at::kWeaponFlags.v) + id * game_last::at::kWeaponStride)[0];
    case 2: return ((id * 13) & ~0xFFu) | move_script::At(::bof3::Img(game_last::at::kArmourFlags.v) + id * game_last::at::kArmourStride)[0];
    case 3: return move_script::At(::bof3::Img(game_last::at::kAccessoryFlags.v) + id * game_last::at::kAccessoryStride)[0];
    case 4: return 0;
    default: return ((id * 11) & ~0xFFu) | move_script::At(::bof3::Img(game_last::at::kConsumableFlags.v) + id * game_last::at::kConsumableStride)[0];
    }
}

// original 0x5918A0 (boot 0x80166A74): the lowest set bit of ability `id`'s
// u16_4 & 0x1FF, counted from 1; 0 when none is.
//
// As the original has it: the scan runs to 10 and would answer 10, which the
// mask makes unreachable.
extern "C" unsigned char __cdecl Skill_FlagIndex(unsigned id) {
    const unsigned flags = move_script::Word(move_script::At(::bof3::Img(battle_window_draw::at::kAbilities.v) + (id & 0xFF) * battle_window_draw::at::kAbilityStride + 0x14));
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

// original 0x5918E0 (PSX 0x80166AD0): 1 if the item's low byte is among the
// 32 key-item bytes 0x904554.
extern "C" unsigned char __cdecl KeyItem_Has(unsigned item) {
    const auto id = static_cast<unsigned char>(item);
    for (unsigned i = 0; i < 0x20; ++i)
        if (char_stats::At(::bof3::Img(char_stats::at::kKeyItems.v))[i] == id) return 1;
    return 0;
}

// original 0x591900 (PSX 0x80166B10): a key item (the argument's low byte) into the first 0
// byte of the 32-byte key-item list 0x904554, al 1; full, al 0. PSX twin
// 0x80166B10 (pairs_propagated "call-anchored").
extern "C" unsigned char __cdecl KeyItem_Add(unsigned item) {
    unsigned char* const list = move_script::At(::bof3::Img(scena_sx::at::kKeyItems));
    for (unsigned i = 0; i < 0x20; ++i) {
        if (list[i] != 0) continue;
        list[i] = static_cast<unsigned char>(item);
        return 1;
    }
    return 0;
}

// original 0x591920 (PSX 0x80166B54): the first of the 32 key-item bytes (0x904554) equal to
// the argument's low byte cleared, al 1; none, al 0 (the rest of eax the
// list's address bits either way). An item of 0 clears the first empty slot
// and answers 1. KeyItem_Add's opposite; chapter 13's Scena13_Run3 takes
// item 0xB. PSX 0x80166B54 (pairs_propagated "gap9").
extern "C" unsigned char __cdecl KeyItem_Remove(unsigned item) {
    unsigned char* const list = AtScenaSx2(::bof3::Img(scena_sx2::at::kKeyItems));
    const auto id = static_cast<unsigned char>(item);
    for (unsigned i = 0; i < 0x20; ++i) {
        if (list[i] != id) continue;
        list[i] = 0;
        return 1;
    }
    return 0;
}

// original 0x591940 (PSX 0x80166B9C): up to 31 bytes (the length's low byte,
// 32 and up cut to 31) copied a byte at a time, forwards, into Text_Records
// entry `slot` (its low byte), then a NUL after them. A source overlapping the
// entry below it repeats, as the original's byte loop does - so no memcpy
// (the volatile store keeps the compiler from making one of the loop).
extern "C" void __cdecl TextRecord_Set(unsigned slot, unsigned length, const unsigned char* text) {
    unsigned n = length & 0xFF;
    if (n >= 0x20) n = 0x1F;
    unsigned char* const d = char_stats::At(::bof3::Img(char_stats::at::kTextRecords.v) + (slot & 0xFF) * 0x20);
    for (unsigned i = 0; i < n; ++i) *static_cast<volatile unsigned char*>(d + i) = *static_cast<const volatile unsigned char*>(text + i);
    d[n] = 0;
}

// original 0x5919B0 (PSX 0x80166C1C): with `equipped`'s low byte 0, the
// count of the first stack of the item in the category's list (0 if none) -
// category 4's count list is null, so a key item found there reads address
// i, as in the original (D35). Otherwise how many of the eight records
// with bit 0 of +0x0B wear it: category 1 the weapon, 2 the three armour
// bytes, 3 the two accessory bytes, any other none; category 3 also counts
// the byte 0x904130 once and adds 0x90412F when 0x90412E is the item. The
// answer is the low word the original defines.
extern "C" unsigned short __cdecl Inventory_Count(unsigned category, unsigned item, unsigned equipped) {
    const auto id = static_cast<unsigned char>(item);
    const unsigned cat = category & 0xFF;
    if ((equipped & 0xFF) == 0) {
        const unsigned char* const ids = IdList(cat);
        const std::uint32_t counts = Low32(CountList(cat));
        for (unsigned i = 0; i < 0x80; ++i)
            if (ids[i] == id) return *reinterpret_cast<const volatile unsigned char*>(static_cast<std::uintptr_t>(counts + i));
        return 0;
    }
    unsigned char n = 0;
    for (unsigned k = 0; k < 8; ++k) {
        const unsigned char* const r = char_stats::At(::bof3::Img(CharacterRecords_addr) + k * char_stats::at::kCharStride);
        if ((r[char_stats::at::kFlags] & 1) == 0) continue;
        switch (cat) {
        case 1:
            if (r[char_stats::at::kWeapon] == id) ++n;
            break;
        case 2:
            for (unsigned b = 0; b < 3; ++b)
                if (r[char_stats::at::kArmour + b] == id) ++n;
            break;
        case 3:
            for (unsigned b = 0; b < 2; ++b)
                if (r[char_stats::at::kAccessory + b] == id) ++n;
            break;
        default: break;
        }
    }
    if (cat == 3) {
        if (char_stats::At(::bof3::Img(char_stats::at::kExtraAccessory.v))[0] == id) ++n;
        if (CharStats_ExtraItem == id) n = static_cast<unsigned char>(n + CharStats_ExtraCount);
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

// original 0x591AC0: called by SharedList_DrawList (field_s.cpp) - how many of
// a member's ten ability bytes of list `which` (0..3: +0x60, +0x6A, +0x74,
// +0x7E) are set, in the CharacterRecords record of member byte (current 0) or
// the party's working record (ObjTrio + 0x80, 0x14C a slot); any other `which`
// counts the 0x80 bytes at 0x904574. Answers al.
extern "C" unsigned char __cdecl AbilityList_CountSet(unsigned member, unsigned which, unsigned current) {
    const U m = member & 0xFF;
    const U base = (current & 0xFF) == 0 ? ::bof3::Img(CharacterRecords_addr) + kCharacterStride * m : ::bof3::Img(kWorking.v) + kWorkingStride * m;
    U p;
    unsigned n = 10;
    switch (which & 0xFF) {
    case 0: p = base + 0x60; break;
    case 1: p = base + 0x6A; break;
    case 2: p = base + 0x74; break;
    case 3: p = base + 0x7E; break;
    default:
        p = ::bof3::Img(0x904574);
        n = 0x80;
        break;
    }
    unsigned char set = 0;
    for (unsigned i = 0; i < n; ++i, ++p)
        if (BRest2f(p) != 0) ++set;
    return set;
}

// original 0x591B60: `count` of `item` out of a category's lists (PSX
// Inventory_Remove 0x80166F30, the sibling's name; pairs_propagated
// "call-disputed"). Item or count 0 (low bytes): al 0. The category's id and
// count lists through Inventory_IdLists / Inventory_CountLists (its low
// byte, unbounded, as Inventory_Add reads them); the item's first slot of
// 128, none: al 0. Category 4 (key items): the id cleared, al 1. Else fewer
// than `count`: al 0; else the count less, the id cleared at 0, al 1.
extern "C" unsigned char __cdecl Inventory_Remove(unsigned category, unsigned item_arg, unsigned count_arg) {
    const auto item = static_cast<unsigned char>(item_arg);
    if (item == 0) return 0;
    if (static_cast<unsigned char>(count_arg) == 0) return 0;
    const unsigned cat = category & 0xFF;
    unsigned char* ids = move_script::At(static_cast<std::uint32_t>(Long(move_script::At(::bof3::Img(bof3::addr::Inventory_IdLists) + 4 * cat))));
    // the count list's pointer, stepped with the id's though category 4's is 0 and never read
    const auto counts = static_cast<std::uint32_t>(Long(move_script::At(::bof3::Img(bof3::addr::Inventory_CountLists) + 4 * cat)));
    for (unsigned i = 0; i < 0x80; ++i, ++ids) {
        if (*ids != item) continue;
        if (static_cast<unsigned char>(category) != 4) {
            unsigned char* const have = move_script::At(counts + i);
            const auto n = static_cast<unsigned char>(count_arg);
            if (*have < n) return 0;
            *have = static_cast<unsigned char>(*have - n);
            if (*have != 0) return 1;
        }
        *ids = 0;
        return 1;
    }
    return 0;
}

// original 0x591BC0 (PSX 0x80166FCC): the party's zenny (Party_Zenny, u32) down by the
// argument when it has at least that much, al 1; else al 0 (the rest of eax
// the argument's). PSX Zenny_Sub 0x80166FCC (the sibling's name;
// pairs_propagated "gap15").
extern "C" unsigned char __cdecl Zenny_Sub(std::uint32_t amount) {
    const auto money = static_cast<std::uint32_t>(Party_Zenny);
    if (money < amount) return 0;
    Party_Zenny = money - amount;
    return 1;
}

// original 0x591BE0 (PSX 0x80166FFC): the party's zenny up by the argument (u32, wrapping),
// and 0x904138 too when the second argument's byte is 0; then above
// 9,999,999 it is 9,999,999 and al 0, else al 1. PSX Zenny_Add 0x80166FFC
// (the sibling's name; pairs_propagated "call-anchored").
extern "C" unsigned char __cdecl Zenny_Add(std::uint32_t amount, unsigned tally) {
    unsigned char* const money = move_script::At(::bof3::Img(Party_Zenny_addr));
    SetLong(money, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(money)) + amount));
    if (static_cast<unsigned char>(tally) == 0)
        Rest4a_CountC = static_cast<std::int32_t>(Rest4a_CountC + amount);
    if (static_cast<std::uint32_t>(Long(money)) > scena_sx::at::kZennyCap) {
        SetLong(money, static_cast<std::int32_t>(scena_sx::at::kZennyCap));
        return 0;
    }
    return 1;
}

// original 0x591C20 (PSX 0x80167058): the price word - weapons +0x18, armour
// +0x16, accessories +0x14, key items +0x10, consumables +0x12.
extern "C" unsigned __cdecl Item_HelpMessage(unsigned category, unsigned item) {
    const unsigned i = item & 0xFF;
    switch (category & 0xFF) {
    case 1: return char_stats::Word(char_stats::At(::bof3::Img(char_stats::at::kWeapons.v) + i * char_stats::at::kWeaponStride + 0x18));
    case 2: return char_stats::Word(char_stats::At(::bof3::Img(char_stats::at::kArmourTable.v) + i * char_stats::at::kArmourStride + 0x16));
    case 3: return char_stats::Word(char_stats::At(::bof3::Img(char_stats::at::kAccessories.v) + i * char_stats::at::kAccessoryStride + 0x14));
    case 4: return char_stats::Word(char_stats::At(::bof3::Img(char_stats::at::kKeyItemTable.v) + i * char_stats::at::kKeyItemStride + 0x10));
    default: return char_stats::Word(char_stats::At(::bof3::Img(char_stats::at::kConsumables.v) + i * char_stats::at::kConsumableStride + 0x12));
    }
}

// original 0x591CC0: a member into the two party lists without a sprite
// (Party_Join's list half). The member's character record +0xB |= 3 (joined,
// and bit 1); below three members, the next slot of both lists and the count
// one on, al 1. With three or more: the first slot i of the first list whose
// member's record +0xB lacks bit 1 and whose id stands in the second list
// (slot j) takes the member in both, al 1; none, al 0. Its one caller
// (chapter 6, 0x54D053) does not read al.
extern "C" unsigned char __cdecl Party_AddToLists(unsigned member_arg) {
    const unsigned char member = static_cast<unsigned char>(member_arg);
    unsigned char* const flags = RecordFlags(member);
    *flags = static_cast<unsigned char>(*flags | 3);
    unsigned char* const lists = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd2);
    const unsigned char n = Field_MemberCount;
    if (n < 3) {
        Field_MemberCount = static_cast<unsigned char>(n + 1);
        lists[n] = member;
        lists[n + 3] = member;
        return 1;
    }
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char id = lists[i];
        if (*RecordFlags(id) & 2) continue;
        for (unsigned j = 0; j < 3; ++j) {
            if (id != lists[3 + j]) continue;
            lists[i] = member;
            lists[3 + j] = member;
            return 1;
        }
    }
    return 0;
}

// original 0x591DB0 (boot 0x80167398): ability `id`'s AP cost (b2, +0x12) for
// member `member` - of CharacterRecords when `battle`'s low byte is 0, else of
// the working record 0x802DC0 + member * 0x14C: halved, rounding up, when the
// record's +0x16 or +0x17 is 0x1A; else three quarters, rounding up, when
// either is 0x19.
extern "C" unsigned char __cdecl Skill_ApCost(unsigned member, unsigned id, unsigned battle) {
    const unsigned m = member & 0xFF;
    const unsigned char* const r =
        (battle & 0xFF) == 0 ? move_script::At(::bof3::Img(CharacterRecords_addr) + m * battle_window_draw::at::kCharStride) : move_script::At(::bof3::Img(battle_window_draw::at::kWorking.v) + m * battle_window_draw::at::kWorkingStride);
    const unsigned cost = move_script::At(::bof3::Img(battle_window_draw::at::kAbilities.v) + (id & 0xFF) * battle_window_draw::at::kAbilityStride + 0x12)[0];
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
        const unsigned character = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd2 + (member & 0xFF))[0];
        r = move_script::At(::bof3::Img(CharacterRecords_addr) + move_script::At(::bof3::Img(Character_IndexById_addr) + character)[0] * battle_window_draw::at::kCharStride);
    } else {
        r = move_script::At(::bof3::Img(battle_window_draw::at::kWorking.v) + (member & 0xFF) * battle_window_draw::at::kWorkingStride);
    }
    switch (type & 0xFF) {
    case 1: return r + 0x6A;
    case 2: return r + 0x74;
    case 3: return r + 0x7E;
    default: return r + 0x60;
    }
}

// original 0x591EC0: the ability list an id files in (PSX AbilityList_ForType
// 0x80167514, the sibling's name, confirmed there; pairs_propagated "call").
// With the third argument's low byte 0, CharacterRecords[member byte] (the
// saved record, by the record's own index); else the member's working record
// ObjTrio[member byte] +0x80. The id's type byte (0x65C4D9 + id * 24, & 3: 1
// +0x6A, 2 +0x74, 3 +0x7E, 0 +0x60) picks the 10-byte list. Every index
// unchecked. AbilityList_Add's one callee. Char_AbilityList 0x591E50 is its
// sibling by a member's list slot and a type.
extern "C" unsigned char* __cdecl AbilityList_ForType(unsigned member, unsigned id, unsigned working) {
    static const unsigned char kLists[4] = {0x60, 0x6A, 0x74, 0x7E};
    const unsigned m = member & 0xFF;
    unsigned char* const base = (working & 0xFF) != 0 ? AtScenaSx2(::bof3::Img(scena_sx2::at::kWorkingRecords) + m * scena_sx2::at::kObjStride)
                                                      : AtScenaSx2(::bof3::Img(CharacterRecords_addr) + m * scena_sx2::at::kRecordStride);
    const unsigned type = AtScenaSx2(::bof3::Img(scena_sx2::at::kAbilityType))[(id & 0xFF) * 24] & 3;
    return base + kLists[type];
}

void MenuChar_Inject() {
    BOF3_INJECT(Menu_DrawIcon);
    BOF3_INJECT(Menu_DrawHand);
    BOF3_INJECT(Char_RecalcStats);
    BOF3_INJECT(Char_ApplyTraits);
    BOF3_INJECT(Equip_PreviewSlot);
    BOF3_INJECT(Equip_PreviewSet);
    BOF3_INJECT(Inventory_Add);
    BOF3_INJECT(AbilityList_Add);
    BOF3_INJECT(Char_HealHp);
    BOF3_INJECT(Char_HealAp);
    BOF3_INJECT(Stat_AddCap999);
    BOF3_INJECT(Stat_AddCap99);
    BOF3_INJECT(Stat_AddClamped);
    BOF3_INJECT(Stat_AddClampedTo);
    BOF3_INJECT(Stat_AddResist);
    BOF3_INJECT(Stat_AddCap100);
    BOF3_INJECT(Char_ClearStatus);
    BOF3_INJECT(Char_ApplyWeapon);
    BOF3_INJECT(Char_ApplyArmour);
    BOF3_INJECT(Char_ApplyAccessories);
    BOF3_INJECT(Item_NamePtr);
    BOF3_INJECT(Item_IconKind);
    BOF3_INJECT(Item_EquipMask);
    BOF3_INJECT(Item_UseFlags);
    BOF3_INJECT(Skill_FlagIndex);
    BOF3_INJECT(KeyItem_Has);
    BOF3_INJECT(KeyItem_Add);
    BOF3_INJECT(KeyItem_Remove);
    BOF3_INJECT(TextRecord_Set);
    BOF3_INJECT(Inventory_Count);
    BOF3_INJECT(Inventory_CountUsed);
    BOF3_INJECT(AbilityList_CountSet);
    BOF3_INJECT(Inventory_Remove);
    BOF3_INJECT(Zenny_Sub);
    BOF3_INJECT(Zenny_Add);
    BOF3_INJECT(Item_HelpMessage);
    BOF3_INJECT(Party_AddToLists);
    BOF3_INJECT(Skill_ApCost);
    BOF3_INJECT(Char_AbilityList);
    BOF3_INJECT(AbilityList_ForType);
}
