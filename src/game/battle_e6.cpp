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

unsigned char& B(U address) { return At(address)[0]; }
U L(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t S8(unsigned char v) { return static_cast<std::int8_t>(v); }
std::int32_t S16(unsigned v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
float F(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return f;
}
void SetF(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

// The party's records and the enemies' objects as the originals index them:
// unchecked (an actor past 2 reaches past the three members, as there).
unsigned char* Party(unsigned member) { return At(at::kParty + (member & 0xFFu) * at::kPartyStride); }
unsigned char* Enemy(unsigned index) { return At(at::kEnemies + index * at::kEnemyStride); }
unsigned Actor() { return B(at::kActor); }
unsigned char* Owner() { return At(L(At(at::kOwner))); }

// The three pure percent helpers nobody owns: (value * percent) / 100 clamped.
using Percent = U (__cdecl*)(U value, std::int32_t percent);
U Percent999(U value, std::int32_t percent) { return BH_AT(Percent, at::kPercent999)(value, percent); }
U Percent9999(U value, std::int32_t percent) { return BH_AT(Percent, at::kPercent9999)(value, percent); }
U Percent100(U value, std::int32_t percent) { return BH_AT(Percent, at::kPercent100)(value, percent); }
// A BMAGIC vertex's height (Quake_VertexLift 0x4CF4B0, R3G's): ax is read.
U CellHeight(std::int32_t x, std::int32_t z) { return BH_AT(U (__cdecl*)(std::int32_t, std::int32_t), at::kCellHeight)(x, z); }

// The party records copied to 0x939AE0 before a transformation changes them:
// 0x904AB0 records of 0x14C bytes, a dword at a time.
void BackUpParty() {
    const unsigned n = B(at::kPartyCount);
    if (n == 0) return;
    for (U i = 0; i < n * at::kPartyStride; i += 4) SetL(At(at::kPartyBackup + i), L(At(at::kParty + i)));
}

// The first stat of a form (the u16 at +0xC0, +0xA0 and +0x98): `percent` of
// the character record's u16 +0x40 through 0x446F50, less (that times the
// record's byte +0x1E, + 5) / 10; 0x939EE0 keeps the value, 0x939EE2 the
// member's u16 +0xC4 as it was.
void FirstStat(unsigned char* member, const unsigned char* character, std::uint16_t value) {
    SetWord(At(at::kStatDeltas), value);
    const std::int32_t cut = (static_cast<std::int32_t>(character[0x1E]) * S16(value) + 5) / 10;
    const auto stat = static_cast<std::uint16_t>(L(At(at::kStatDeltas)) - static_cast<U>(cut));
    const std::uint16_t next = Word(member + 0xC4);
    SetWord(member + 0xC0, stat);
    SetWord(member + 0xA0, stat);
    SetWord(member + 0x98, stat);
    SetWord(At(at::kStatDeltas + 2), next);
}

// Kept by the recipe path: the stat at `field` (0xC4 .. 0xCA) through
// 0x446F20 by `percent`, mirrored at field - 0x20, its change into the cell
// `delta` (which held the old value).
void RecipeStat(unsigned char* member, unsigned field, unsigned percent, U delta) {
    SetWord(At(delta), Word(member + field));
    const auto v = static_cast<std::uint16_t>(Percent999(Word(member + field), static_cast<std::int32_t>(percent) * 10));
    SetWord(member + field, v);
    SetWord(member + field - 0x20, v);
    SetWord(At(delta), static_cast<std::uint16_t>(v - Word(At(delta))));
}

// A mixed form's list end: 0xD9 at the count, the rest of the ten zeroed.
void EndAbilities(unsigned char* list, unsigned char count) {
    list[count] = 0xD9;
    const auto next = static_cast<unsigned char>(count + 1);
    for (unsigned i = next; i < 10; ++i) list[i] = 0;
}

// Picks a party form when the actor holds `gene` (the group code then 1).
unsigned char PartyPick(unsigned char gene, unsigned char held, unsigned char plain) {
    if (BH_CALL(DragonGenes_Held)(gene) != 0) {
        B(at::kFormGroupWork) = 1;
        return held;
    }
    return plain;
}

// A task's `call [table + 4 * +1]`, a Fatal past its entries (the original
// calls through whatever follows), then Sprite_UpdateScreen while the slot's
// +0 is not 0 (Sprite_Current read again after the call).
void TaskDispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: the task's state +1 is %u, past the %u entries of 0x%X - the original calls through the dword after "
                    "(docs/battle_e6.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(L(At(table + 4 * state))))();   // as read (the fuzz swaps the cells)
    if (Sprite_Current[0] != 0) BH_CALL(Sprite_UpdateScreen)();
}

// A dash state's velocity toward the actor: (member - sprite) / 16, toward zero.
U Sixteenth(U to, U from) { return static_cast<U>(static_cast<std::int32_t>(to - from) / 16); }

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
    const U d = L(at);
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

// original 0x451480 (the gene history's step table 0x64ED50, entry 6, by
// 0x904AA4): windows 18 and 19's +3 = 4 and 2, 0x904AA3 = 3, 0x904AA4 = 2.
extern "C" void __cdecl DragonHistory_Leave(void) {
    B(at::kWindow18State) = 4;
    B(at::kWindow19State) = 2;
    B(at::kStep2) = 3;
    B(at::kStep3) = 2;
}

// original 0x4514A0 (called by Accession_Start 0x4EAEA0): the actor's
// record's +0x138 / +0x13C = 0, +0x130 &= ~0x18000, +0x134 &= 0xFFFB84FF;
// Battle_RecalcStats(actor); 0x675F56 / 57 = 0; +0x134 |= 2 (the actor read
// again). A recipe (DragonForm_FindRecipe): DragonForm_ApplyRecipe(it), the
// form code = it + 4 into 0x675F56, and +0x134 |= 0x10000 for code 8,
// 0x20000 for 7 and 4 (the actor read again). None: DragonForm_Mix, the code
// read back. Then the history's five older records move down one, the genes
// (0x904B87 of them) copied into record 0 and the rest of its three 0xFF;
// 0x904B79 = the group code, record 0's byte 3 = (group << 5) + (code &
// 0x1F), 0x904B89 = the code; Battle_RecalcStats(the actor read again).
extern "C" void __cdecl DragonForm_Transform(void) {
    {
        unsigned char* const member = Party(Actor());
        const unsigned who = Actor();
        SetL(member + 0x138, 0);
        SetL(member + 0x13C, 0);
        SetL(member + 0x130, L(member + 0x130) & 0xFFFE7FFFu);
        SetL(member + 0x134, L(member + 0x134) & 0xFFFB84FFu);
        BH_CALL(Battle_RecalcStats)(who);
    }
    unsigned char* const member = Party(Actor());
    B(at::kFormCode) = 0;
    B(at::kFormGroupWork) = 0;
    SetL(member + 0x134, L(member + 0x134) | 2);
    unsigned char form = BH_CALL(DragonForm_FindRecipe)();
    if (form != 0xFF) {
        BH_CALL(DragonForm_ApplyRecipe)(form);
        form = static_cast<unsigned char>(form + 4);
        B(at::kFormCode) = form;
        if (form == 8) {
            unsigned char* const m = Party(Actor());
            SetL(m + 0x134, L(m + 0x134) | 0x10000);
        } else if (form == 7 || form == 4) {
            unsigned char* const m = Party(Actor());
            SetL(m + 0x134, L(m + 0x134) | 0x20000);
        }
    } else {
        BH_CALL(DragonForm_Mix)(0xFF);
        form = B(at::kFormCode);
    }
    for (U record = at::kHistoryLast; record > at::kHistory; record -= 4)
        for (U i = 0; i < 4; ++i) B(record + i) = B(record - 4 + i);
    const unsigned count = B(at::kGeneCount);
    unsigned kept = 0;
    if (count > 0) {
        for (unsigned i = 0; i < count; ++i) B(at::kHistory + i) = B(at::kGenes + i);
        kept = count;
    }
    for (unsigned i = kept; i < 3; ++i) B(at::kHistory + i) = 0xFF;
    const unsigned char group = B(at::kFormGroupWork);
    B(at::kFormGroup) = group;
    const unsigned who = Actor();
    B(at::kHistoryForm) = static_cast<unsigned char>((group << 5) + (form & 0x1F));
    B(at::kForm) = form;
    BH_CALL(Battle_RecalcStats)(who);
}

// original 0x451670: the first recipe 0..10 whose three genes the actor holds
// (DragonForm_RecipeSlotHeld for each), its group code (0x64ED6C) into
// 0x675F57, the recipe the answer; at 6, DragonForm_TryPartyRecipe first -
// its answer when not 0, else recipe 6 is skipped. 0xFF for none.
extern "C" unsigned char __cdecl DragonForm_FindRecipe(void) {
    unsigned recipe = 0;
    for (;;) {
        if (recipe == 6) {
            const unsigned char party = BH_CALL(DragonForm_TryPartyRecipe)();
            if (party != 0) return party;
            recipe = 7;
        }
        unsigned slot = 0;
        while (slot < 3 && BH_CALL(DragonForm_RecipeSlotHeld)(recipe, slot) != 0) ++slot;
        if (slot == 3) {
            B(at::kFormGroupWork) = At(at::kRecipeGroups)[recipe];
            return static_cast<unsigned char>(recipe);
        }
        if (++recipe > 10) return 0xFF;
    }
}

// original 0x4516F0: recipe `recipe`'s gene `slot` (0x64ED78 + 3 recipe +
// slot): 1 when it is 0xFF (any) or among the chosen genes, else 0.
extern "C" unsigned char __cdecl DragonForm_RecipeSlotHeld(unsigned recipe, unsigned slot) {
    const unsigned char gene = At(at::kRecipeGenes)[(recipe & 0xFF) * 3 + (slot & 0xFF)];
    if (gene == 0xFF) return 1;
    const unsigned count = B(at::kGeneCount);
    for (unsigned i = 0; i < count; ++i)
        if (B(at::kGenes + i) == gene) return 1;
    return 0;
}

// original 0x451750: when a chosen gene is 0x10 and the party's size byte
// 0x904AB1 (read once) is 3, DragonForm_PartyRecipe's answer; else 0.
extern "C" unsigned char __cdecl DragonForm_TryPartyRecipe(void) {
    const unsigned count = B(at::kGeneCount);
    if (count == 0) return 0;
    const unsigned char size = B(at::kPartySize);
    for (unsigned i = 0; i < count; ++i)
        if (B(at::kGenes + i) == 0x10 && size == 3) return BH_CALL(DragonForm_PartyRecipe)();
    return 0;
}

// original 0x4517A0 (its word unread): the mix. The party records backed up;
// the 14 sums 0x675F48.. zeroed, each chosen gene but 0xA / 0xB / 0xC adding
// its row (0x64ED9C + 14 gene), each 0xB negating the first ten, each 0xA
// doubling all fourteen; each 0xC moving every sum by one either way with
// Rand (a zero one time in ten, another half the time; the count read again
// after). The form code 3 / 2 / 1 / 0 by sums 13, 11 (at least 1) and 10 (at
// least 2); five stats from sums 5..9 through DragonForm_StatShift and the
// code's column of 0x64EE98 (the first as FirstStat with 1 below 1, the
// others through 0x446F20 with 0 below 1, each change kept at 0x939EE2 ..);
// the group code the last of sums 0..4 above 0 (1..5), 6 for two or more;
// DragonForm_SetMixBytes(sums 0..4), DragonForm_MixAbilities.
extern "C" void __cdecl DragonForm_Mix(unsigned) {
    BackUpParty();
    unsigned char* const member = Party(Actor());
    const unsigned char* const character = At(at::kCharRecords + member[0x148] * at::kCharStride);
    unsigned char* const sums = At(at::kMixSums);
    for (unsigned i = 0; i < 14; ++i) sums[i] = 0;
    const unsigned count = B(at::kGeneCount);
    if (count > 0) {
        for (unsigned k = 0; k < count; ++k) {
            const unsigned char gene = B(at::kGenes + k);
            if (gene == 0xA || gene == 0xB || gene == 0xC) continue;
            for (unsigned i = 0; i < 14; ++i) sums[i] = static_cast<unsigned char>(sums[i] + At(at::kGeneRows + gene * 14u)[i]);
        }
        for (unsigned k = 0; k < count; ++k)
            if (B(at::kGenes + k) == 0xB)
                for (unsigned i = 0; i < 10; ++i) sums[i] = static_cast<unsigned char>(-sums[i]);
        for (unsigned k = 0; k < count; ++k)
            if (B(at::kGenes + k) == 0xA)
                for (unsigned i = 0; i < 14; ++i) sums[i] = static_cast<unsigned char>(sums[i] << 1);
    }
    unsigned limit = count;
    for (unsigned k = 0; k < limit; ++k) {
        if (B(at::kGenes + k) != 0xC) continue;
        for (unsigned i = 0; i < 14; ++i) {
            const bool move = sums[i] == 0 ? BH_CALL(Rand)() % 10 == 0 : (BH_CALL(Rand)() & 1) != 0;
            if (!move) continue;
            const int way = BH_CALL(Rand)();
            const unsigned char v = sums[i];
            sums[i] = static_cast<unsigned char>(way & 1 ? v + 1 : v - 1);
        }
        limit = B(at::kGeneCount);
    }
    const unsigned char s13 = sums[13];
    B(at::kFormCode) = 0;
    if (S8(s13) >= 1) B(at::kFormCode) = 3;
    else if (S8(sums[11]) >= 1) B(at::kFormCode) = 2;
    else if (S8(sums[10]) >= 2) B(at::kFormCode) = 1;

    // the first stat: 1 below 1
    {
        const unsigned char shift = BH_CALL(DragonForm_StatShift)(sums[5]);
        const unsigned column = At(at::kShiftColumns)[B(at::kFormCode) * 5];
        const auto sum = static_cast<std::uint16_t>(S8(shift) + static_cast<std::int32_t>(column));
        std::uint16_t value = 1;
        if (S16(sum) > 0) value = static_cast<std::uint16_t>(Percent9999(Word(character + 0x40), S16(sum) * 10));
        FirstStat(member, character, value);
    }
    // the other four: 0 below 1; the old value's cell takes the change
    const U deltas[4] = {at::kStatDeltas + 2, at::kStatDeltas + 4, at::kStatDeltas + 6, at::kStatDeltas + 8};
    for (unsigned n = 0; n < 4; ++n) {
        const unsigned field = 0xC4 + 2 * n;
        const unsigned char shift = BH_CALL(DragonForm_StatShift)(sums[6 + n]);
        const unsigned column = At(at::kShiftColumns)[B(at::kFormCode) * 5 + 1 + n];
        const auto sum = static_cast<std::uint16_t>(S8(shift) + static_cast<std::int32_t>(column));
        std::uint16_t value = 0;
        if (S16(sum) > 0) value = static_cast<std::uint16_t>(Percent999(Word(member + field), S16(sum) * 10));
        SetWord(At(deltas[n]), static_cast<std::uint16_t>(value - Word(At(deltas[n]))));
        const std::uint16_t next = n < 3 ? Word(member + field + 2) : 0;
        SetWord(member + field, value);
        SetWord(member + field - 0x20, value);
        if (n < 3) SetWord(At(deltas[n + 1]), next);
    }
    unsigned char positive = 0;
    B(at::kFormGroupWork) = 0;
    for (unsigned i = 0; i < 5; ++i)
        if (S8(sums[i]) >= 1) {
            ++positive;
            B(at::kFormGroupWork) = static_cast<unsigned char>(i + 1);
        }
    if (positive >= 2) B(at::kFormGroupWork) = 6;
    BH_CALL(DragonForm_SetMixBytes)(sums[0], sums[1], sums[2], sums[3], sums[4]);
    BH_CALL(DragonForm_MixAbilities)();
}

// original 0x451C50: 0x64EEAC's step by a signed sum + 2, held to 0..4.
extern "C" unsigned char __cdecl DragonForm_StatShift(unsigned sum) {
    std::int32_t i = S8(static_cast<unsigned char>(sum + 2));
    if (i < 0) i = 0;
    else if (i > 4) i = 4;
    return At(at::kShiftSteps)[i];
}

namespace {
unsigned Clamp4(unsigned sum) {
    std::int32_t i = S8(static_cast<unsigned char>(sum + 2));
    if (i < 0) return 0;
    return i > 4 ? 4u : static_cast<unsigned>(i);
}
void MixByte(unsigned char* record, unsigned at, U cell, unsigned char v) {
    record[at + 0x80] = v;   // +0xCF ..: the form's
    record[at + 0x60] = v;   // +0xAF ..
    B(cell) = v;
}
}  // namespace

// original 0x451C80: the actor's nine form bytes (+0xCF .. +0xD7, mirrored
// at +0xAF .., kept at 0x939EEA ..): sums 0, 1, 2 through 0x64EEB4, 2, 2, sum
// 4 through 0x64EEBE, 2, sum 3 through 0x64EEB9 twice - each by the sum + 2
// held to 0..4. Answers 0.
extern "C" unsigned char __cdecl DragonForm_SetMixBytes(unsigned s0, unsigned s1, unsigned s2, unsigned s3, unsigned s4) {
    unsigned char* const record = Party(Actor());
    MixByte(record, 0x4F, at::kStatDeltas + 0xA, At(at::kMixBytesA)[Clamp4(s0)]);
    MixByte(record, 0x50, at::kStatDeltas + 0xB, At(at::kMixBytesA)[Clamp4(s1)]);
    MixByte(record, 0x51, at::kStatDeltas + 0xC, At(at::kMixBytesA)[Clamp4(s2)]);
    MixByte(record, 0x52, at::kStatDeltas + 0xD, 2);
    MixByte(record, 0x53, at::kStatDeltas + 0xE, 2);
    MixByte(record, 0x54, at::kStatDeltas + 0xF, At(at::kMixBytesC)[Clamp4(s4)]);
    MixByte(record, 0x55, at::kStatDeltas + 0x10, 2);
    const unsigned third = Clamp4(s3);
    MixByte(record, 0x56, at::kStatDeltas + 0x11, At(at::kMixBytesB)[third]);
    MixByte(record, 0x57, at::kStatDeltas + 0x12, At(at::kMixBytesB)[third]);
    return 0;
}

// original 0x451DA0: the mixed form's abilities into the actor's list
// (+0xF4, ten bytes): the form code's three of 0x64EEC4 to the first 0, then
// DragonForm_AddAbilityRow by the sums - 0 (and 5, 14 by sums 9 and 3), 1
// (6), 2 (7), 3 (8), 4 (9), 12, 11, 13 - then 0xD9 and zeros.
extern "C" void __cdecl DragonForm_MixAbilities(void) {
    unsigned char* const list = Party(Actor()) + 0xF4;
    unsigned char count = 0;
    for (unsigned j = 0; j < 3; ++j) {
        const unsigned char ability = At(at::kFormAbilities)[B(at::kFormCode) * 3u + j];
        if (ability == 0) break;
        list[count++] = ability;
    }
    auto sum = [](unsigned i) { return S8(At(at::kMixSums)[i]); };
    auto add = [&count](unsigned row) { count = BH_CALL(DragonForm_AddAbilityRow)(row, count); };
    if (sum(0) >= 1) {
        add(0);
        if (sum(9) >= 1) add(5);
        if (sum(3) >= 1) add(0xE);
    }
    if (sum(1) >= 1) {
        add(1);
        if (sum(9) >= 1) add(6);
    }
    if (sum(2) >= 1) {
        add(2);
        if (sum(9) >= 1) add(7);
    }
    if (sum(3) >= 1) {
        add(3);
        if (sum(9) >= 1) add(8);
    }
    if (sum(4) >= 1) {
        add(4);
        if (sum(9) >= 1) add(9);
    }
    if (sum(12) >= 1) add(0xC);
    if (sum(11) >= 1) add(0xB);
    if (sum(9) >= 1) add(0xD);
    EndAbilities(list, count);
}

// original 0x451FE0: row `row`'s three abilities (0x64EED0 + 3 row) added to
// the actor's list at `count` (a 0 ends the row; one already there skipped;
// at 9 the answer is 9), the new count the answer.
extern "C" unsigned char __cdecl DragonForm_AddAbilityRow(unsigned row, unsigned count) {
    unsigned char* const list = Party(Actor()) + 0xF4;
    const unsigned base = (row & 0xFF) * 3;
    auto n = static_cast<unsigned char>(count);
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned char ability = At(at::kAbilityRows)[base + k];
        if (ability == 0) return n;
        bool held = false;
        for (unsigned char d = 0; d < n; ++d)
            if (list[d] == ability) {
                held = true;
                break;
            }
        if (held) continue;
        if (n == 9) return 9;
        list[n] = ability;
        n = static_cast<unsigned char>(n + 1);
    }
    return n;
}

// original 0x452080: a recipe's form (0..20). The party backed up; the first
// stat by the recipe's percent of the character's +0x40 (FirstStat), four more
// through 0x446F20 (RecipeStat); nine bytes into +0xCF.., +0xAF.. and
// 0x939EEA..; its abilities (0x64F028 + 8 recipe, to a 0, eight at most),
// then 0xD9 and zeros.
extern "C" void __cdecl DragonForm_ApplyRecipe(unsigned recipe) {
    BackUpParty();
    unsigned char* const member = Party(Actor());
    const unsigned r = recipe & 0xFF;
    const unsigned char* const row = At(at::kRecipeStats + r * 14);
    const unsigned char* const character = At(at::kCharRecords + member[0x148] * at::kCharStride);
    FirstStat(member, character,
              static_cast<std::uint16_t>(Percent9999(Word(character + 0x40), static_cast<std::int32_t>(row[0]) * 10)));
    // RecipeStat's first store is the old value: the first stat left it at 0x939EE2 already
    {
        const auto v = static_cast<std::uint16_t>(Percent999(Word(member + 0xC4), static_cast<std::int32_t>(row[1]) * 10));
        SetWord(member + 0xC4, v);
        SetWord(member + 0xA4, v);
        SetWord(At(at::kStatDeltas + 2), static_cast<std::uint16_t>(v - Word(At(at::kStatDeltas + 2))));
    }
    RecipeStat(member, 0xC6, row[2], at::kStatDeltas + 4);
    RecipeStat(member, 0xC8, row[3], at::kStatDeltas + 6);
    RecipeStat(member, 0xCA, row[4], at::kStatDeltas + 8);
    for (unsigned i = 0; i < 9; ++i) MixByte(member, 0x4F + i, at::kStatDeltas + 0xA + i, row[5 + i]);
    unsigned char* const list = member + 0xF4;
    unsigned char n = 0;
    for (;;) {
        const unsigned char ability = At(at::kRecipeAbilities)[n + r * 8];
        if (ability == 0) break;
        list[n] = ability;
        n = static_cast<unsigned char>(n + 1);
        if (n >= 8) break;
    }
    EndAbilities(list, n);
}

// original 0x4523C0: the other members' +0x89 bytes (not the actor, not out
// by Battle_ActorIsOut), the first two as a pair tried both ways round: 1 or
// 8 with 2 or 6 - gene 7 held ? 0xC : 0xB; 2 with 4 or 5 - 0; 4 with 5, 8 or
// 1 - gene 5 ? 0x12 : 0x11; 5 with 8, 1 or 6 - gene 0xD ? 0x14 : 0x13; 6 with
// 2 or 4 - gene 3 ? 0xE : 0xD (a held gene sets the group code 1); else
// 0xFF. With fewer than two members found the original reads stack bytes it
// never wrote (docs/battle_e6.md section 7, L1); ours answers 0, the answer
// the failing pairings give - no party form, recipe 6 skipped (DIV-0063).
extern "C" unsigned char __cdecl DragonForm_PartyRecipe(void) {
    unsigned char found[3] = {};
    unsigned n = 0;
    for (unsigned m = 0; m < 3; ++m) {
        if (B(at::kActor) == m) continue;
        if (BH_CALL(Battle_ActorIsOut)(m) != 0) continue;
        found[n++] = Party(m)[0x89];
    }
    if (n < 2) return 0;   // DIV-0063: no pair to try
    unsigned char key = found[0], other = found[1];
    for (int pass = 0; pass < 2; ++pass) {
        switch (key) {
        case 1:
        case 8:
            if (other == 2 || other == 6) return PartyPick(7, 0xC, 0xB);
            break;
        case 2:
            if (other == 4 || other == 5) return 0;
            break;
        case 4:
            if (other == 5 || other == 8 || other == 1) return PartyPick(5, 0x12, 0x11);
            break;
        case 5:
            if (other == 8 || other == 1 || other == 6) return PartyPick(0xD, 0x14, 0x13);
            break;
        case 6:
            if (other == 2 || other == 4) return PartyPick(3, 0xE, 0xD);
            break;
        default:
            break;
        }
        const unsigned char was = key;
        key = other;
        other = was;
    }
    return 0xFF;
}

// original 0x452570: 1 when `gene` is among the chosen genes, else 0.
extern "C" unsigned char __cdecl DragonGenes_Held(unsigned gene) {
    const unsigned count = B(at::kGeneCount);
    for (unsigned i = 0; i < count; ++i)
        if (B(at::kGenes + i) == static_cast<unsigned char>(gene)) return 1;
    return 0;
}

// original 0x4525B0 (called by BE5's Dragon run steps 0x44FFA0, 0x450610,
// 0x450700, 0x450E70): 0x904B78 = the chosen genes' costs (0x64EC9C) summed,
// a byte, stored after each.
extern "C" void __cdecl DragonGenes_SumCost(void) {
    const unsigned count = B(at::kGeneCount);
    unsigned char sum = 0;
    B(at::kGeneCost) = 0;
    for (unsigned i = 0; i < count; ++i) {
        sum = static_cast<unsigned char>(sum + At(at::kGeneCosts)[B(at::kGenes + i)]);
        B(at::kGeneCost) = sum;
    }
}

// ============================================================================
// Three battle effect tasks (BattleFx_Dispatch slots 15, 16, 18;
// Sprite_Current the task slot)
// ============================================================================

// original 0x452680: slot 15 (a copy of the acting member, 0x442E60's task):
// through BattleFxDash_Steps by +1 (7).
extern "C" void __cdecl BattleFxDash_Dispatch(void) { TaskDispatch("BattleFxDash_Dispatch", at::kDashSteps, 7); }

// original 0x4526B0: state 0: +0x20 = +0x3C + 0x4000000; pose +8 + 0x38;
// +0xB = 0x10; on.
extern "C" void __cdecl BattleFxDash_Start(void) {
    unsigned char* const s = Sprite_Current;
    SetL(s + 0x20, L(s + 0x3C) + 0x4000000u);
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 0x38));
    Sprite_Current[0xB] = 0x10;
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
}

// original 0x4526F0: state 1: when BattleObj_ScriptTickOnce answers, pose
// +8 + 0x3C and on.
extern "C" void __cdecl BattleFxDash_WaitPose(void) {
    if (BH_CALL(BattleObj_ScriptTickOnce)() == 0) return;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 0x3C));
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
}

// original 0x452720: state 2: +0x3C up 0x1000000 while below +0x20; there,
// x and z at the target's side (Field_Kind2X / _Kind2Z + the pair of
// 0x64E4F4 by 0x904AAC - its other pair, and +8 ^= 2, when the target is a
// member), pose +8 + 0x40, on. A tail jump to BattleObj_ScriptTickOnce.
extern "C" unsigned char __cdecl BattleFxDash_Rise(void) {
    unsigned char* const s = Sprite_Current;
    const auto z = static_cast<std::int32_t>(L(s + 0x3C));
    if (z < static_cast<std::int32_t>(L(s + 0x20))) {
        SetL(s + 0x3C, static_cast<U>(z) + 0x1000000u);
    } else {
        unsigned side;
        if (B(at::kTarget) >= 3) {
            side = B(at::kFormTable);
            SetL(s + 0x34, static_cast<U>(S8(At(at::kDashSides)[side * 2])) + L(At(at::kKind2X)));
            side = B(at::kFormTable);
        } else {
            s[8] ^= 2;
            side = B(at::kFormTable) ^ 2u;
            SetL(Sprite_Current + 0x34, static_cast<U>(S8(At(at::kDashSides)[side * 2])) + L(At(at::kKind2X)));
            side = B(at::kFormTable) ^ 2u;
        }
        SetL(Sprite_Current + 0x38, static_cast<U>(S8(At(at::kDashSides)[side * 2 + 1])) + L(At(at::kKind2Z)));
        BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 0x40));
        Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
    }
    return BH_CALL(BattleObj_ScriptTickOnce)();
}

// original 0x4527F0: state 3: a trail task (BattleTask_Create(0, 0x12), slot
// 18) copied from this slot's first 0x80 bytes with +0 bit 6 clear, +1..+4 /
// +6 = 0, +5 = 0x12, +0x80 = this slot, +0xB = this +0xB (this +0xB down one
// when not 0); a step toward the target (MagicFx_StepToward 0x50); near it
// (MagicFx_NearSprite3D 0x10000, the target read again), pose +8 + 0x44,
// Battle_SetTargetFlag40(target), on. BattleObj_ScriptTickOnce's answer.
extern "C" unsigned char __cdecl BattleFxDash_Advance(void) {
    const unsigned char slot = BH_CALL(BattleTask_Create)(0, 0x12);
    if (slot != 0xFF) {
        unsigned char* const s = Sprite_Current;
        unsigned char* const t = At(at::kTasks + slot * at::kTaskStride);
        for (U i = 0; i < 0x80; i += 4) SetL(t + i, L(s + i));
        t[0] &= 0xBF;
        t[6] = 0;
        t[5] = 0x12;
        t[1] = 0;
        t[2] = 0;
        t[3] = 0;
        t[4] = 0;
        SetL(t + 0x80, Addr(s));
        t[0xB] = s[0xB];
        if (s[0xB] != 0) s[0xB] = static_cast<unsigned char>(s[0xB] - 1);
    }
    const unsigned target = B(at::kTarget);
    int near;
    if (target <= 2) {
        BH_CALL(MagicFx_StepToward)(Party(target), 0x50);
        near = BH_CALL(MagicFx_NearSprite3D)(Party(B(at::kTarget)), 0x10000);
    } else {
        BH_CALL(MagicFx_StepToward)(Enemy(target - 3), 0x50);
        near = BH_CALL(MagicFx_NearSprite3D)(Enemy(B(at::kTarget) - 3u), 0x10000);
    }
    if (near != 0) {
        BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 0x44));
        BH_CALL(Battle_SetTargetFlag40)(B(at::kTarget));
        Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
    }
    return BH_CALL(BattleObj_ScriptTickOnce)();
}

// original 0x452950: state 4: when BattleObj_ScriptTickOnce answers, the way
// back: +0xC / +0x10 / +0x18 = (the actor's +0x34 / +0x38 / +0x3C less
// this's) / 16, +0x14 = 0x40, +0x20 = -8 (the actor and Sprite_Current read
// again for each); on and a tail jump to BattleObj_ScriptTickOnce. Else 0.
extern "C" unsigned char __cdecl BattleFxDash_Return(void) {
    const unsigned char ticked = BH_CALL(BattleObj_ScriptTickOnce)();
    if (ticked == 0) return ticked;
    SetL(Sprite_Current + 0xC, Sixteenth(L(Party(Actor()) + 0x34), L(Sprite_Current + 0x34)));
    SetL(Sprite_Current + 0x10, Sixteenth(L(Party(Actor()) + 0x38), L(Sprite_Current + 0x38)));
    SetL(Sprite_Current + 0x14, 0x40);
    SetL(Sprite_Current + 0x20, 0xFFFFFFF8u);
    SetL(Sprite_Current + 0x18, Sixteenth(L(Party(Actor()) + 0x3C), L(Sprite_Current + 0x3C)));
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
    return BH_CALL(BattleObj_ScriptTickOnce)();
}

// original 0x452A20: state 5: +0x34 += +0xC, +0x38 += +0x10, the word +0x3E
// += the word +0x14, +0x3C += +0x18 (over +0x3E's new word), +0x14 += +0x20;
// at +0x14 == -0x40 on. A tail jump to BattleObj_ScriptTickOnce.
extern "C" unsigned char __cdecl BattleFxDash_Arc(void) {
    unsigned char* s = Sprite_Current;
    SetL(s + 0x34, L(s + 0x34) + L(s + 0xC));
    s = Sprite_Current;
    SetL(s + 0x38, L(s + 0x38) + L(s + 0x10));
    s = Sprite_Current;
    SetWord(s + 0x3E, static_cast<std::uint16_t>(Word(s + 0x3E) + Word(s + 0x14)));
    s = Sprite_Current;
    SetL(s + 0x3C, L(s + 0x3C) + L(s + 0x18));
    s = Sprite_Current;
    SetL(s + 0x14, L(s + 0x14) + L(s + 0x20));
    s = Sprite_Current;
    if (L(s + 0x14) == 0xFFFFFFC0u) s[1] = static_cast<unsigned char>(s[1] + 1);
    return BH_CALL(BattleObj_ScriptTickOnce)();
}

// original 0x452A80: state 6: the actor's +0 bit 6 cleared; the owner (0x93B940)
// given pose +8 + 4 as Sprite_Current, which is put back; a tail jump to
// BattleTask_FreeCurrent.
extern "C" void __cdecl BattleFxDash_Land(void) {
    unsigned char* const member = Party(Actor());
    unsigned char* const self = Sprite_Current;
    member[0] &= 0xBF;
    unsigned char* const owner = Owner();
    Sprite_Current = owner;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(owner[8] + 4));
    Sprite_Current = self;
    BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x452AD0: slot 16: through BattleFxPose_Steps by +1 (2).
extern "C" void __cdecl BattleFxPose_Dispatch(void) { TaskDispatch("BattleFxPose_Dispatch", at::kPoseSteps, 2); }

// original 0x452B00: state 0: pose +8 + 0x50, BattleObj_ScriptTickOnce (not
// read), +0x29 = 3, +9 = 8, on.
extern "C" void __cdecl BattleFxPose_Start(void) {
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 0x50));
    BH_CALL(BattleObj_ScriptTickOnce)();
    Sprite_Current[0x29] = 3;
    Sprite_Current[9] = 8;
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
}

// original 0x452B40: state 1: BattleObj_ScriptTickOnce (not read); the slot
// freed (a tail jump) once the owner's +1 is 5.
extern "C" void __cdecl BattleFxPose_WaitOwner(void) {
    BH_CALL(BattleObj_ScriptTickOnce)();
    if (Owner()[1] == 5) BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x452B60: slot 18 (the trail BattleFxDash_Advance creates): through
// BattleFxTrail_Steps by +1 (2: this group's start, then
// BossWeretigrFx_TrailFade).
extern "C" void __cdecl BattleFxTrail_Dispatch(void) { TaskDispatch("BattleFxTrail_Dispatch", at::kTrailSteps, 2); }

// original 0x452B90: state 0: +0 |= 0x20, +0x5C = 3, the shade bytes +0x5D,
// +0x5F, +0x5E = +0xB * 0xF0 (a byte), +9 = 8, on; a tail jump to
// Sprite_ScriptTickOnce.
extern "C" unsigned char __cdecl BattleFxTrail_Start(void) {
    Sprite_Current[0] |= 0x20;
    Sprite_Current[0x5C] = 3;
    Sprite_Current[0x5D] = static_cast<unsigned char>(Sprite_Current[0xB] * 0xF0u);
    Sprite_Current[0x5F] = static_cast<unsigned char>(Sprite_Current[0xB] * 0xF0u);
    Sprite_Current[0x5E] = static_cast<unsigned char>(Sprite_Current[0xB] * 0xF0u);
    Sprite_Current[9] = 8;
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
    return BH_CALL(Sprite_ScriptTickOnce)();
}

// ============================================================================
// Stats, rolls, pop-ups
// ============================================================================

// original 0x453300: an actor's stats rebuilt from the buffs. A member (0..2):
// Battle_RecalcMemberStats(its record + 0x80, actor). An enemy (actor - 3):
// four words +0xB4.. = 0x446F20(+0xD4.., the s8 buff +0x118.. * 2 + 100),
// four bytes +0xC9.. = 0x446F80(+0xE9.., +0x11C.. likewise), eight bytes
// copied (+0xBF.. from +0xDF.., not +0xC6), +0x114 bit 14 triples +0xB4, bit
// 12 moves +0xB6 into it; +0xB4 and +0xB6 through 0x446F20(v, 100). Then,
// for either, ENEMY index (the member's own number for a member) +0x114 bit
// 11 zeroes its +0xC4 and bit 13 its +0xBF - latent: a member's rebuild
// clears enemy 0..2's bytes (docs/battle_e6.md section 7).
extern "C" void __cdecl Battle_RecalcStats(unsigned actor) {
    unsigned index;
    if ((actor & 0xFF) <= 2) {
        index = actor & 0xFF;
        BH_CALL(Battle_RecalcMemberStats)(At(at::kParty + 0x80 + index * at::kPartyStride), actor);
    } else {
        index = (actor - 3) & 0xFF;
        unsigned char* const e = Enemy(index);
        for (unsigned i = 0; i < 4; ++i)
            SetWord(e + 0xB4 + 2 * i,
                    static_cast<std::uint16_t>(Percent999(Word(e + 0xD4 + 2 * i), S8(e[0x118 + i]) * 2 + 100)));
        for (unsigned i = 0; i < 4; ++i)
            e[0xC9 + i] = static_cast<unsigned char>(Percent100(e[0xE9 + i], S8(e[0x11C + i]) * 2 + 100));
        for (unsigned i = 0; i < 7; ++i) e[0xBF + i] = e[0xDF + i];
        e[0xC7] = e[0xE7];
        if (L(e + 0x114) & 0x4000) SetWord(e + 0xB4, static_cast<std::uint16_t>(Word(e + 0xB4) * 3));
        if (L(e + 0x114) & 0x1000) {
            SetWord(e + 0xB4, static_cast<std::uint16_t>(Word(e + 0xB4) + Word(e + 0xB6)));
            SetWord(e + 0xB6, 0);
        }
        SetWord(e + 0xB4, static_cast<std::uint16_t>(Percent999(Word(e + 0xB4), 100)));
        SetWord(e + 0xB6, static_cast<std::uint16_t>(Percent999(Word(e + 0xB6), 100)));
    }
    unsigned char* const e = Enemy(index);
    if (L(e + 0x114) & 0x800) e[0xC4] = 0;
    if (L(e + 0x114) & 0x2000) e[0xBF] = 0;
}

// original 0x453560: a member's stats rebuilt: `view` (its record + 0x80)
// four words +0x24.. = 0x446F20(+0x44.., the member's s8 buff +0x138.. * 2 +
// 100), four bytes +0x39.. = 0x446F80(+0x59.., +0x13C..), eight bytes +0x2F..
// from +0x4F.. (not +0x36); then on the member's record: +0x134 bit 0 and bit
// 14 each triple +0xA4, bit 12 moves +0xA6 into it; with +0x134 bit 1 and form
// 0x16 (0x904B89) +0xBB and +0xBA up 0x1E each and through 0x446F80(v, 100);
// bit 11 zeroes +0xB4, bit 13 +0xAF; +0xA4 and +0xA6 through 0x446F20(v, 100).
extern "C" void __cdecl Battle_RecalcMemberStats(unsigned char* view, unsigned member) {
    unsigned char* const r = Party(member);
    for (unsigned i = 0; i < 4; ++i)
        SetWord(view + 0x24 + 2 * i,
                static_cast<std::uint16_t>(Percent999(Word(view + 0x44 + 2 * i), S8(r[0x138 + i]) * 2 + 100)));
    for (unsigned i = 0; i < 3; ++i)
        view[0x39 + i] = static_cast<unsigned char>(Percent100(view[0x59 + i], S8(r[0x13C + i]) * 2 + 100));
    const auto fourth = static_cast<unsigned char>(Percent100(view[0x5C], S8(r[0x13F]) * 2 + 100));
    const unsigned char b50 = view[0x50], b51 = view[0x51];   // read before the fourth is stored, as there
    view[0x3C] = fourth;
    view[0x2F] = view[0x4F];
    const unsigned char b52 = view[0x52];
    view[0x30] = b50;
    view[0x31] = b51;
    const unsigned char b53 = view[0x53], b54 = view[0x54];
    view[0x32] = b52;
    const unsigned char b55 = view[0x55];
    view[0x33] = b53;
    const unsigned char b57 = view[0x57];
    view[0x34] = b54;
    view[0x35] = b55;
    view[0x37] = b57;
    if (r[0x134] & 1) SetWord(r + 0xA4, static_cast<std::uint16_t>(Word(r + 0xA4) * 3));
    if (L(r + 0x134) & 0x4000) SetWord(r + 0xA4, static_cast<std::uint16_t>(Word(r + 0xA4) * 3));
    if (L(r + 0x134) & 0x1000) {
        SetWord(r + 0xA4, static_cast<std::uint16_t>(Word(r + 0xA4) + Word(r + 0xA6)));
        SetWord(r + 0xA6, 0);
    }
    if ((r[0x134] & 2) && B(at::kForm) == 0x16) {
        r[0xBB] = static_cast<unsigned char>(r[0xBB] + 0x1E);
        r[0xBA] = static_cast<unsigned char>(r[0xBA] + 0x1E);
        r[0xBB] = static_cast<unsigned char>(Percent100(r[0xBB], 100));
        r[0xBA] = static_cast<unsigned char>(Percent100(r[0xBA], 100));
    }
    if (L(r + 0x134) & 0x800) r[0xB4] = 0;
    if (L(r + 0x134) & 0x2000) r[0xAF] = 0;
    SetWord(r + 0xA4, static_cast<std::uint16_t>(Percent999(Word(r + 0xA4), 100)));
    SetWord(r + 0xA6, static_cast<std::uint16_t>(Percent999(Word(r + 0xA6), 100)));
}

// original 0x453910 (Battle_PickFlag8Member's test when the action record's
// +4 has bit 9): 0 when the member is out, lacks +0x130 bit 0, its +0x124 is
// not the actor, the action's bit (Battle_ActionBitSet of 0x904B40's byte +2)
// is set or its list is full (Battle_MemberListFull); else 1 when Rand leaves
// no remainder by the odds of the action row's high nibble (0x65C4D9 + 0x18
// by 0x904B40's word +2; 0x64F104 when the member's effect state
// (0x66972C + its +0x89) is 6, else 0x64F0FC). The original divides by the
// odds unchecked: ours aborts on 0.
extern "C" unsigned char __cdecl Battle_MemberRollByAction(unsigned member) {
    if (BH_CALL(Battle_ActorIsOut)(member) != 0) return 0;
    unsigned char* const r = Party(member);
    if (!(r[0x130] & 1)) return 0;
    if (r[0x124] != B(at::kActor)) return 0;
    if (BH_CALL(Battle_ActionBitSet)(At(L(At(at::kActingSprite2)))[2]) != 0) return 0;
    if (BH_CALL(Battle_MemberListFull)(member) != 0) return 0;
    const U table = At(at::kEffectState)[r[0x89]] == 6 ? at::kRollOddsB : at::kRollOdds;
    const unsigned id = Word(At(L(At(at::kActingSprite2))) + 2);
    const unsigned nibble = At(at::kActionRows + id * at::kActionRowStride)[0] >> 4;
    const unsigned odds = At(table)[nibble];
    const int roll = BH_CALL(Rand)();
    if (odds == 0)
        bof3::Fatal("Battle_MemberRollByAction: the odds for action 0x%X (nibble %u of 0x%X) are 0 - the original divides by "
                    "them (docs/battle_e6.md section 7)",
                    id, nibble, (unsigned)table);
    return (static_cast<unsigned>(roll % static_cast<int>(odds)) & 0xFF) == 0 ? 1 : 0;
}

// original 0x453A90: bit (action & 31) of the dword 0x904088 + 4 (action >> 5)
// set: 1, else 0 (the action's low byte).
extern "C" unsigned char __cdecl Battle_ActionBitSet(unsigned action) {
    const unsigned a = action & 0xFF;
    return (L(At(at::kActionBits + (a >> 5) * 4)) >> (a & 31)) & 1 ? 1 : 0;
}

// original 0x453AC0: 1 when every one of the member's ten bytes +0xFE.. is
// not 0, else 0.
extern "C" unsigned char __cdecl Battle_MemberListFull(unsigned member) {
    const unsigned char* const r = Party(member);
    for (unsigned i = 0; i < 10; ++i)
        if (r[0xFE + i] == 0) return 0;
    return 1;
}

// original 0x453EB0 (EnemyOp_ReceiveAction and three more: an AP change): a
// pop-up task (BattleTask_Create(0, 1) - not checked for 0xFF, as
// Battle_SetDamagePopup, D54) with +0xB = 1, +0x80 the actor's record, +0x60
// the amount's magnitude (s16), +0x27 1 for a negative amount else 2; +7 by
// the actor's +0x12C (member) / +0x10C (enemy): 4 without bit 1; 2 (and +0x27
// = 1) with bit 3 and not bit 5; else 0.
extern "C" void __cdecl Battle_SetApPopup(unsigned amount, unsigned actor) {
    const unsigned char slot = BH_CALL(BattleTask_Create)(0, 1);
    unsigned char* const t = At(at::kTasks + slot * at::kTaskStride);
    t[0xB] = 1;
    const unsigned who = actor & 0xFF;
    unsigned char bits;
    if (who <= 2) {
        bits = Party(who)[0x12C];
        SetL(t + 0x80, Addr(Party(who)));
    } else {
        bits = Enemy(who - 3)[0x10C];
        SetL(t + 0x80, Addr(Enemy(who - 3)));
    }
    const std::int32_t value = S16(amount);
    if (value < 0) {
        SetL(t + 0x60, static_cast<U>(-value));
        t[0x27] = 1;
    } else {
        SetL(t + 0x60, static_cast<U>(value));
        t[0x27] = 2;
    }
    if (!(bits & 2)) {
        t[7] = 4;
        return;
    }
    if (!(bits & 0x20) && (bits & 8)) {
        t[0x27] = 1;
        t[7] = 2;
        return;
    }
    t[7] = 0;
}

// ============================================================================
// Field_Slots (round eleven's debts)
// ============================================================================

// original 0x454A80: every Field_Slots record (8 of 0x10) whose +0xC is
// `object` released (Field_SlotRelease).
extern "C" void __cdecl Field_SlotsReleaseOwner(const unsigned char* object) {
    for (unsigned i = 0; i < 8; ++i)
        if (L(At(at::kFieldSlots + 0xC + 0x10 * i)) == Addr(object)) BH_CALL(Field_SlotRelease)(i);
}

// original 0x455290: the first Field_Slots record without +0 bit 0 taken:
// +0 = 1, +0xC = `object`, +4 = `script`, +2 = 0xFF, +3 = the object's
// +0x27; its index the answer, 0xFF when none is free.
extern "C" unsigned char __cdecl Field_SlotStart(unsigned char* object, unsigned script) {
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const slot = At(at::kFieldSlots + 0x10 * i);
        if (slot[0] & 1) continue;
        slot[0] = 1;
        SetL(slot + 0xC, Addr(object));
        SetL(slot + 4, script);
        slot[2] = 0xFF;
        slot[3] = object[0x27];
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

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
        if (!(F(prim + 8) > F(At(at::kScreenXLo)))) continue;
        if (!(F(prim + 8) < F(At(at::kScreenXHi)))) continue;
        if (!(F(prim + 0xC) > F(At(at::kScreenYLo)))) continue;
        if (!(F(prim + 0xC) < F(At(at::kScreenYHi)))) continue;
        BH_CALL(Gpu_SetPolyFT4)(prim);
        BH_CALL(Gpu_SetShadeTex)(prim, 0);
        SVec v[3];
        for (unsigned k = 0; k < 3; ++k) v[k] = CellVertex(x0, z0, p - 0xC + 4 * k);
        BH_CALL(Gte_LoadVertices3)(reinterpret_cast<const unsigned long*>(v));
        BH_CALL(Prim_SetTexture)(L(p), prim, 1);
        BH_CALL(Gte_Rtpt)();
        const U texture = L(p);
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
            BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, (((L(mode) >> 24) & 3) << 5) | 0x95, 0);
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
            BH_CALL(Gpu_SetSemiTrans)(prim, L(vertex) >> 31);
            for (unsigned k = 0; k < 4; ++k) {
                const U colour = L(vertex);
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
    const U d1 = L(record + 4);
    SVec centre{};
    centre.x = static_cast<std::int16_t>((static_cast<U>(S8(static_cast<unsigned char>(d1 >> 24))) + ((b1 - 0x80) << 6)) << 1);
    centre.y = static_cast<std::int16_t>((static_cast<U>(S8(static_cast<unsigned char>(d1 >> 16))) + ((b0 - 0x80) << 6)) << 1);
    centre.z = static_cast<std::int16_t>(CellHeight(centre.x, centre.y) + Word(record + 4));
    const U t = Frame_Counter & 0xFFF;
    const U d2 = L(record + 8);
    const U spin[3] = {((d2 >> 20) & 0x3FF) * t * 4, ((d2 >> 10) & 0x3FF) * t * 4, (d2 & 0x3FF) * t * 4};
    if (record[2] == 3) return;
    CheckEntries("MapCell_DrawSpinQuads", record, 3, 6);
    const unsigned char* p = record + 0xC;
    U counter = 3;
    do {
        BH_CALL(Gte_PushMatrix)();
        const U e = L(p);
        const std::int16_t angles[4] = {static_cast<std::int16_t>((spin[0] + ((e >> 20) & 0x3FF) * 4) & 0xFFF),
                                        static_cast<std::int16_t>((spin[1] + ((e >> 10) & 0x3FF) * 4) & 0xFFF),
                                        static_cast<std::int16_t>((spin[2] + (e & 0x3FF) * 4) & 0xFFF), 0};
        Matrix m{};
        BH_CALL(Gte_RotTrans)(&centre.x, reinterpret_cast<long*>(m.t));
        BH_CALL(Gte_RotMatrix)(angles, m.m);
        BH_CALL(Gte_MulMatrix0)(reinterpret_cast<const short*>(At(at::kCameraMatrix)), m.m, m.m);
        BH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
        BH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
        unsigned char* const prim = Gfx_PacketNext;
        BH_CALL(Gpu_SetPolyFT4)(prim);
        BH_CALL(Gpu_SetShadeTex)(prim, 0);
        SVec q[4];
        for (unsigned k = 0; k < 4; ++k) {
            const U v = L(p + 4 + 4 * k);
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
        BH_CALL(Prim_SetTexture)(L(p + 0x14), prim, 1);
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
    unsigned char* const point = At(at::kVertexScratch);
    SetWord(point, (static_cast<U>(S8(static_cast<unsigned char>(L(record + 4) >> 24))) + ((b1 - 0x80) << 6)) << 1);
    SetWord(point + 2, (static_cast<U>(S8(static_cast<unsigned char>(L(record + 4) >> 16))) + ((b0 - 0x80) << 6)) << 1);
    const long height = BH_CALL(AreaMap_Elevation)(static_cast<long>(static_cast<U>(S16(Word(point)) + 0x4000) << 9),
                                                   static_cast<long>(static_cast<U>(S16(Word(point + 2)) + 0x4000) << 9));
    SetWord(point + 4, static_cast<U>(-(S16(static_cast<U>(height)) / 2)));
    long p = 0;
    unsigned char* const screen = At(at::kScreenXY);
    const long z = BH_AT(Pers, bof3::addr::Gte_RotTransPers)(point, screen, &p);
    if (!(F(screen) >= F(At(at::kSpriteXLo)))) return;
    if (F(screen) > F(At(at::kSpriteXHi))) return;
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
    x += static_cast<double>(S8(static_cast<unsigned char>(L(corners) >> 8)) * 1125 / zz);
    SetF(prim + 0x38, static_cast<float>(x));
    SetF(prim + 0x18, static_cast<float>(x));
    double y = static_cast<double>(F(screen + 4)) - static_cast<double>(S8(static_cast<unsigned char>(L(corners) >> 16)) * 1125 / zz);
    SetF(prim + 0x1C, static_cast<float>(y));
    SetF(prim + 0xC, static_cast<float>(y));
    y += static_cast<double>(S8(static_cast<unsigned char>(L(corners) >> 24)) * 1125 / zz);
    SetF(prim + 0x3C, static_cast<float>(y));
    SetF(prim + 0x2C, static_cast<float>(y));
    BH_CALL(Prim_SetTexture)(L(record + (head + 2 * frame) * 4 + 0xC), prim, 1);
    BH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0x48);
}

// ============================================================================

void BattleE6_Inject() {
    if (bof3::WantsShadow("battle_e6")) battle_e6::SelfTest();
    BOF3_INJECT(DragonHistory_Leave);
    BOF3_INJECT(DragonForm_Transform);
    BOF3_INJECT(DragonForm_FindRecipe);
    BOF3_INJECT(DragonForm_RecipeSlotHeld);
    BOF3_INJECT(DragonForm_TryPartyRecipe);
    BOF3_INJECT(DragonForm_Mix);
    BOF3_INJECT(DragonForm_StatShift);
    BOF3_INJECT(DragonForm_SetMixBytes);
    BOF3_INJECT(DragonForm_MixAbilities);
    BOF3_INJECT(DragonForm_AddAbilityRow);
    BOF3_INJECT(DragonForm_ApplyRecipe);
    BOF3_INJECT(DragonForm_PartyRecipe);
    BOF3_INJECT(DragonGenes_Held);
    BOF3_INJECT(DragonGenes_SumCost);
    BOF3_INJECT(BattleFxDash_Dispatch);
    BOF3_INJECT(BattleFxDash_Start);
    BOF3_INJECT(BattleFxDash_WaitPose);
    BOF3_INJECT(BattleFxDash_Rise);
    BOF3_INJECT(BattleFxDash_Advance);
    BOF3_INJECT(BattleFxDash_Return);
    BOF3_INJECT(BattleFxDash_Arc);
    BOF3_INJECT(BattleFxDash_Land);
    BOF3_INJECT(BattleFxPose_Dispatch);
    BOF3_INJECT(BattleFxPose_Start);
    BOF3_INJECT(BattleFxPose_WaitOwner);
    BOF3_INJECT(BattleFxTrail_Dispatch);
    BOF3_INJECT(BattleFxTrail_Start);
    BOF3_INJECT(Battle_RecalcStats);
    BOF3_INJECT(Battle_RecalcMemberStats);
    BOF3_INJECT(Battle_MemberRollByAction);
    BOF3_INJECT(Battle_ActionBitSet);
    BOF3_INJECT(Battle_MemberListFull);
    BOF3_INJECT(Battle_SetApPopup);
    BOF3_INJECT(Field_SlotsReleaseOwner);
    BOF3_INJECT(Field_SlotStart);
    BOF3_INJECT(MapCell_DrawTexQuads);
    BOF3_INJECT(MapCell_DrawShadedQuads);
    BOF3_INJECT(MapCell_DrawSpinQuads);
    BOF3_INJECT(MapCell_DrawGroundSprite);
}
