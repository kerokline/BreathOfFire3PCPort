// Round fourteen group R4A (docs/rest_4a.md): the 48 functions of
// analysis/round14_cut.tsv's group R4A, 0x452DD0..0x456D4F, each read with
// capstone to its last instruction (2026-10-05). The band is three things, not
// the "field core" the cut proposed:
//
//   Battle_*        six helpers of the battle's targeting (0x452DD0..0x454372)
//   Field_RunSlot, Field_SlotClutCopy   a Field_Slots record's step: its
//                   script's frame copies columns of a CLUT row
//   CommuSim_*, CommuPose_*   the community's simulation (areas 175..185):
//                   the records 0x9046D0 (60 of 8) and the buildings 0x9048B0
//                   (8 of 8) moved on by the battle count 0x904134, the event queue
//                   0x904CA0, and the village's objects placed and posed
//   FieldTrigger*   twelve entries of Field_ObjectTriggers (ids 1..11, 61)
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again; a memory cell
// the original reads after a call is read after it here, each call's answer
// into a local first. Where the original divides by zero or walks a record
// list that has no end, ours aborts with a message before the fault
// (docs/rest_4a.md section 7). One function is not a plain copy:
// Battle_RandomLiveEnemy reads six bytes of its own frame it never wrote when
// five or more enemies are standing; ours takes them as 0 (section 7: wants a
// ledger entry).
#include "game/rest_4a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_4a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_4a::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

U UL(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
U UL(U a) { return UL(move_script::At(a)); }
void SetUL(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }
void SetUL(U a, U v) { SetUL(move_script::At(a), v); }

// The community's records and buildings, and the traits table by record.
unsigned char* Rec(U i) { return At(::bof3::Img(Community_Records_addr) + 8u * i); }
unsigned char* Bld(U b) { return At(::bof3::Img(Community_Buildings_addr) + 8u * b); }
unsigned char Trait(U i, U field) { return At(::bof3::Img(at::kRecordTraits.v) + at::kTraitStride * i + field)[0]; }
unsigned char* Pose(U object) { return At(::bof3::Img(Sprite_Objects_addr) + 0x88 + at::kSpriteStride * object); }
unsigned char Count(U building) { return At(::bof3::Img(at::kResidentCounts.v) + building)[0]; }

// The event queue: kind at +0, the record at +1 (the levels and the area
// queue write +0 only, in place).
void QueueKindRecord(unsigned char kind, unsigned char record) {
    const unsigned char n = Rest4b_BusyB;
    At(::bof3::Img(at::kEvents.v) + 2u * n)[0] = kind;
    At(::bof3::Img(at::kEvents.v) + 2u * n + 1)[0] = record;
    Rest4b_BusyB = static_cast<unsigned char>(n + 1);
}
// The first record in use whose +1 is building + 1; 60 when none.
unsigned char FirstOf(U building) {
    unsigned char j = 0;
    for (U i = 0; i < at::kRecordCount; ++i, ++j)
        if (Rec(i)[0] != 0 && Rec(i)[1] == building + 1) break;
    return j;
}

unsigned char CommuCount() { return SH_AT(unsigned char (__cdecl*)(), at::kCommuCount.v)(); }

// Rand's answer reduced as the community's pickers do: & 0x7F, above 100
// less 100 (a byte).
unsigned char Percent(int r) {
    auto v = static_cast<unsigned char>(r & 0x7F);
    if (v > 0x64) v = static_cast<unsigned char>(v + 0x9C);
    return v;
}

}  // namespace

// ===========================================================================
// The battle's targeting helpers
// ===========================================================================

// ===========================================================================
// A Field_Slots record's step
// ===========================================================================

// ===========================================================================
// The community's simulation
// ===========================================================================

// original 0x455450 (PSX 0x801EECA4, call; the sibling has no name for it):
// the community's area entry, from Area179_Init (shared by areas 175..185).
// In the same area as the word 0x802290: CommuSim_PlaceObjects only. Else the
// records in use counted (R4D's 0x45E6B0), CommuSim_RollOffers, the removed,
// spawned and event counts cleared; with none in use, three records spawned and
// the clocks and levels set (the dword 0x9046B0 0, the stamps the clock, mood
// 10, the dark level 1, the lit level and the sums 0, the area seen the area);
// with some, the queue of new areas, mood, the two levels, population, the
// ticks of kinds 9, 0xD, 5 and 0xB and the sums, each in that order. Both end
// placing the objects. The count is handed on as the original's dword whose
// low byte it is (the callees read the byte).
extern "C" void __cdecl CommuSim_AreaEnter(void) {
    if (Game_AreaNumber == AreaW2c_LastArea) {
        SH_CALL(CommuSim_PlaceObjects)();
        return;
    }
    const unsigned char n = CommuCount();
    SH_CALL(CommuSim_RollOffers)();
    Rest4a_RemovedCount = 0;
    Rest4b_BusyA = 0;
    Rest4b_BusyB = 0;
    if (n == 0) {
        for (int k = 0; k < 3; ++k) SH_CALL(CommuSim_AddRecord)();
        const U clock = Game_BattleCount;
        const unsigned short area = Game_AreaNumber;
        Rest4a_ClockA = 0;
        Rest4a_StampGrow = clock;
        Rest4a_StampShrink = clock;
        Rest4a_StampMood = clock;
        Rest4a_StampLevelLit = clock;
        Rest4a_StampLevelDark = clock;
        Rest4b_CountA = 0xA;
        FieldE2_LevelA = 1;
        Rest4a_LevelLit = 0;
        FieldE2_BiasA = 0;
        FieldE2_BiasB = 0;
        Rest4a_AreaSeen = area;
        SH_CALL(CommuSim_PlaceObjects)();
        return;
    }
    SH_CALL(CommuSim_QueueAreas)();
    SH_CALL(CommuSim_Mood)(n);
    SH_CALL(CommuSim_LevelLit)();
    SH_CALL(CommuSim_LevelDark)();
    SH_CALL(CommuSim_Population)(n);
    SH_CALL(CommuSim_TickKind9)();
    SH_CALL(CommuSim_TickKindD)();
    SH_CALL(CommuSim_TickKind5)();
    SH_CALL(CommuSim_TickKindB)();
    SH_CALL(CommuSim_SumKindsAB)();
    SH_CALL(CommuSim_PlaceObjects)();
}

// original 0x455540 (no PSX twin paired): when the area number is above the
// area seen (u16 0x9046C8), each area number from the seen one + 1 to it whose
// byte 0x652850[n] is 4 or 5 queues an event of that kind (+0 only), each
// kind once; then the area seen is the area.
extern "C" void __cdecl CommuSim_QueueAreas(void) {
    const unsigned short area = Game_AreaNumber;
    if (area <= Rest4a_AreaSeen) return;
    int n = static_cast<int>((UL(::bof3::Img(at::kAreaSeen.v)) & 0xFFFF) + 1);
    const int last = area;
    if (n <= last) {
        unsigned char count = Rest4b_BusyB;
        unsigned char seen = 0;
        for (; n <= last; ++n) {
            const unsigned char kind = At(::bof3::Img(at::kAreaKinds.v) + static_cast<U>(n))[0];
            if (kind == 4) {
                if (!(seen & 1)) {
                    At(::bof3::Img(at::kEvents.v) + 2u * count)[0] = 4;
                    ++count;
                    seen |= 1;
                }
            } else if (kind == 5) {
                if (!(seen & 2)) {
                    At(::bof3::Img(at::kEvents.v) + 2u * count)[0] = 5;
                    ++count;
                    seen |= 2;
                }
            }
        }
        Rest4b_BusyB = count;
    }
    Rest4a_AreaSeen = area;
}

// original 0x4555D0 (no PSX twin paired): population by mood. With mood
// (s8 0x9046CA) above 4 x the count: the shrink stamp kept within 20 of the
// clock; below 20 records, (clock - the grow stamp) / 10 spawns, at most
// (mood - 4 x count) / 4 + 1 and up to 20 records, then the grow stamp is the
// clock. Else: the grow stamp kept within 10; with mood not 0 the shrink stamp
// within 20; with mood 0 and above one record, (clock - the shrink stamp) / 20
// removals down to one record, then the shrink stamp is the clock.
extern "C" void __cdecl CommuSim_Population(unsigned count) {
    const auto mood = Rest4b_CountA;
    const auto n0 = static_cast<unsigned char>(count);
    U n = n0;
    const int m = mood;
    const int four = static_cast<int>(4 * n);
    if (m > four) {
        const U clock = Game_BattleCount;
        if (clock - Rest4a_StampShrink > 0x14) Rest4a_StampShrink = clock - 0x14;
        if (n0 >= 0x14) return;
        const U grow = Rest4a_StampGrow;
        const int room = ((m - four) >> 2) + 1;
        int k = static_cast<int>((clock - grow) / 10u);
        if (k > room) k = room;
        if (k == 0) return;
        for (int j = 0; j < k && n != 0x14; ++j, ++n) SH_CALL(CommuSim_AddRecord)();
        Rest4a_StampGrow = Game_BattleCount;
        return;
    }
    const U clock = Game_BattleCount;
    const U grow = Rest4a_StampGrow;
    if (mood != 0) {
        if (clock - grow > 0xA) Rest4a_StampGrow = clock - 0xA;
        if (clock - Rest4a_StampShrink > 0x14) Rest4a_StampShrink = clock - 0x14;
        return;
    }
    if (clock - grow > 0xA) Rest4a_StampGrow = clock - 0xA;
    if (n0 <= 1) return;
    const int k = static_cast<int>((clock - Rest4a_StampShrink) / 20u);
    if (k == 0) return;
    for (int j = 0; j < k && n != 1; ++j, --n) SH_CALL(CommuSim_RemoveRecord)();
    Rest4a_StampShrink = Game_BattleCount;
}

// original 0x455700 (no PSX twin paired): mood. When (clock - the mood stamp)
// / 5 (its low 16 bits) is not 0: the records in use of kind 9 each add their
// trait +0x10 and 3 (a 16-bit sum), less twice the count; times that, plus the
// mood; below 0 the mood is 0, above 99 it is 99; the stamp is the clock.
extern "C" void __cdecl CommuSim_Mood(unsigned count) {
    const U clock = Game_BattleCount;
    const U steps = ((clock - Rest4a_StampMood) / 5u) & 0xFFFF;
    if (steps == 0) return;
    U sum = 0;
    for (U i = 0; i < at::kRecordCount; ++i)
        if (Rec(i)[0] != 0 && Rec(i)[1] == 9) sum += Trait(i, 0x10) + 3u;
    const auto twice = static_cast<std::int16_t>(static_cast<unsigned short>((count & 0xFFu) * 2));
    const U v = static_cast<U>(static_cast<int>(static_cast<std::int16_t>(sum)) - twice) * steps +
                static_cast<std::uint32_t>(Rest4b_CountA);
    if (static_cast<std::int32_t>(v) < 0) {
        Rest4a_StampMood = clock;
        Rest4b_CountA = 0;
        return;
    }
    Rest4b_CountA = static_cast<unsigned char>(static_cast<std::int32_t>(v) > 0x63 ? 0x63 : v);
    Rest4a_StampMood = clock;
}

namespace {
// The records in the buildings of `kind` 4 whose +1 is set (lit) or clear:
// the sum of their trait +0x13 (16 bits) and their count (a byte).
void LevelSums(bool lit, unsigned short& sum, unsigned char& count) {
    sum = 0;
    count = 0;
    for (U b = 0; b < at::kBuildingCount; ++b) {
        if (Bld(b)[0] != 4 || (Bld(b)[1] != 0) != lit) continue;
        for (U i = 0; i < at::kRecordCount; ++i)
            if (Rec(i)[0] != 0 && Rec(i)[1] == b + 1) {
                sum = static_cast<unsigned short>(sum + Trait(i, 0x13));
                ++count;
            }
    }
}
}  // namespace

// original 0x4557A0 (no PSX twin paired): the lit level (0x9046CC, below 7).
// The residents of kind-4 buildings whose +1 is set: with any, the cost
// 0x65290C[level] less their trait +0x13 sum (at least 1); when the clock is
// that far past the lit stamp, the stamp moves by it, the level goes up and
// an event 7 is queued (+0 only).
extern "C" void __cdecl CommuSim_LevelLit(void) {
    if (Rest4a_LevelLit >= 7) return;
    unsigned short sum;
    unsigned char count;
    LevelSums(true, sum, count);
    if (count == 0) return;
    auto v = static_cast<std::int16_t>(static_cast<unsigned short>(Word(At(::bof3::Img(at::kLevelLitCosts.v) + 2u * Rest4a_LevelLit)) - sum));
    if (v <= 0) v = 1;
    const U last = Rest4a_StampLevelLit;
    if (Game_BattleCount - last < static_cast<U>(static_cast<int>(v))) return;
    const unsigned char level = Rest4a_LevelLit;
    Rest4a_StampLevelLit = last + static_cast<U>(static_cast<int>(v));
    const unsigned char n = Rest4b_BusyB;
    Rest4a_LevelLit = static_cast<unsigned char>(level + 1);
    At(::bof3::Img(at::kEvents.v) + 2u * n)[0] = 7;
    Rest4b_BusyB = static_cast<unsigned char>(n + 1);
}

// original 0x455870 (no PSX twin paired): the dark level (0x9046CB, below 10):
// as CommuSim_LevelLit over kind-4 buildings whose +1 is clear, the cost 20
// less the sum (at least 1) from the dark stamp, and only when the lit level
// is at least 0x65291B[dark level]; event 6.
extern "C" void __cdecl CommuSim_LevelDark(void) {
    if (FieldE2_LevelA >= 0xA) return;
    unsigned short sum;
    unsigned char count;
    LevelSums(false, sum, count);
    if (count == 0) return;
    auto v = static_cast<std::int16_t>(static_cast<unsigned short>(0x14 - sum));
    if (v <= 0) v = 1;
    const U last = Rest4a_StampLevelDark;
    if (Game_BattleCount - last < static_cast<U>(static_cast<int>(v))) return;
    const unsigned char level = FieldE2_LevelA;
    if (Rest4a_LevelLit < At(::bof3::Img(at::kLevelDarkNeeds.v) + level)[0]) return;
    Rest4a_StampLevelDark = last + static_cast<U>(static_cast<int>(v));
    const unsigned char n = Rest4b_BusyB;
    FieldE2_LevelA = static_cast<unsigned char>(level + 1);
    At(::bof3::Img(at::kEvents.v) + 2u * n)[0] = 6;
    Rest4b_BusyB = static_cast<unsigned char>(n + 1);
}

// original 0x455950 (no PSX twin paired; R4B's 0x456E20 jumps here): the
// buildings of kind 5. A level +3 of 0 becomes 1 with the stamp +4 the clock;
// one below 11, with residents: (clock - stamp) / max(5, (+2 + 2) x 6 - their
// trait +0x12 sum) (16-bit, signed) is added, capped at the lit level + 4 (and
// only from below it); the stamp is the clock and an event 0 is queued with
// the building's first resident.
extern "C" void __cdecl CommuSim_TickKind5(void) {
    for (U b = 0; b < at::kBuildingCount; ++b) {
        unsigned char* const bl = Bld(b);
        if (bl[0] != 5) continue;
        if (bl[3] == 0) {
            bl[3] = 1;
            SetUL(bl + 4, Game_BattleCount);
            continue;
        }
        if (bl[3] >= 0xB) continue;
        unsigned short sum = 0;
        unsigned char count = 0;
        for (U i = 0; i < at::kRecordCount; ++i)
            if (Rec(i)[0] != 0 && Rec(i)[1] == b + 1) {
                sum = static_cast<unsigned short>(sum + Trait(i, 0x12));
                ++count;
            }
        if (count == 0) continue;
        auto d = static_cast<std::int16_t>(static_cast<unsigned short>((bl[2] + 2u) * 6u - sum));
        if (d < 5) d = 5;
        const U q = (Game_BattleCount - UL(bl + 4)) / static_cast<U>(static_cast<int>(d));
        const auto cap = static_cast<std::int16_t>(Rest4a_LevelLit + 4);
        if (q == 0) continue;
        const unsigned char level = bl[3];
        if (static_cast<std::int16_t>(level) >= cap) continue;
        const auto raised = static_cast<unsigned char>(level + q);
        bl[3] = raised;
        if (static_cast<std::int16_t>(raised) > cap) bl[3] = static_cast<unsigned char>(cap);
        SetUL(bl + 4, Game_BattleCount);
        const unsigned char n = Rest4b_BusyB;
        At(::bof3::Img(at::kEvents.v) + 2u * n)[0] = 0;
        const unsigned char first = FirstOf(b);
        const unsigned char n2 = Rest4b_BusyB;
        At(::bof3::Img(at::kEvents.v) + 2u * n + 1)[0] = first;
        Rest4b_BusyB = static_cast<unsigned char>(n2 + 1);
    }
}

// original 0x455AA0 (no PSX twin paired): the buildings of kind 9 and each
// resident. Too recent ((clock - its stamp) below (level +1 + 1) x 6): its
// +2 is 1. Else, with +2 at most 1, a roll Percent(Rand()) against (2 x level
// + 1) x (10 - trait +0x10) (a byte): under it, with more than one record in
// use (R4D's 0x45E6B0) the record is removed (+0 = 0, listed with bit 7),
// else +2 = 3 and an event 2; over it, a second roll against the byte
// {0x41, 0x37, 0xF}[level] picks +2 = 3 (under) or 2, and an event 2. The
// level indexes a three-byte table on the original's stack: above 2 it reads
// the frame past it (ours aborts).
extern "C" void __cdecl CommuSim_TickKind9(void) {
    static constexpr unsigned char kOdds[3] = {0x41, 0x37, 0x0F};
    for (U b = 0; b < at::kBuildingCount; ++b) {
        if (Bld(b)[0] != 9) continue;
        for (U i = 0; i < at::kRecordCount; ++i) {
            unsigned char* const r = Rec(i);
            if (r[0] == 0) continue;
            if (r[1] != b + 1) continue;
            const unsigned char level = Bld(b)[1];
            if (Game_BattleCount - rv::As<records::CommunityRecord>(r)->stamp < (level + 1u) * 6u) {
                r[2] = 1;
                continue;
            }
            if (r[2] > 1) continue;
            const auto chance = static_cast<unsigned char>((level * 2 + 1) * static_cast<unsigned char>(0xA - Trait(i, 0x10)));
            const int roll = SH_CALL(Rand)();
            if (chance > Percent(roll)) {
                const unsigned char inUse = CommuCount();
                if (inUse > 1) {
                    const unsigned char n = Rest4a_RemovedCount;
                    r[0] = 0;
                    Rest4a_RemovedCount = static_cast<unsigned char>(n + 1);
                    At(::bof3::Img(at::kRemoved.v) + n)[0] = static_cast<unsigned char>(i | 0x80);
                    continue;
                }
                r[2] = 3;
            } else {
                const int roll2 = SH_CALL(Rand)();
                const unsigned char p = Percent(roll2);
                const unsigned char lv = Bld(b)[1];
                if (lv > 2)
                    bof3::Fatal("CommuSim_TickKind9: building %u's level %u indexes past the three odds - the original reads its frame",
                                (unsigned)b, (unsigned)lv);
                r[2] = static_cast<unsigned char>((p < kOdds[lv] ? 1 : 0) + 2);
            }
            QueueKindRecord(2, static_cast<unsigned char>(i));
        }
    }
}

// original 0x455BE0 (no PSX twin paired): the buildings of kind 0xD and each
// resident whose +3 high nibble is 1: its item +2 of the category +3 & 0xF (0
// consumables, 1 weapons, 2 armour, else accessories) has a price, the first
// of the eight tiers 0x652928 whose bound is not below it (the ninth past the
// table when none) a clock span; once (clock - stamp) reaches it +3 keeps its
// category and: an item flag bit 3 - +3 | 0x30; else the tier's odds plus
// trait +0x13 x the odds / 20, within 0..100, against Percent(Rand()) (above
// 100 less 28) - over it +3 | 0x20; else (15 - trait) x 2 against a second
// roll - over it +3 = 0x40 and +2 = 0x37, not +3 | 0x30. Each ends with an
// event 3 for the record.
extern "C" void __cdecl CommuSim_TickKindD(void) {
    for (U b = 0; b < at::kBuildingCount; ++b) {
        if (Bld(b)[0] != 0xD) continue;
        for (U i = 0; i < at::kRecordCount; ++i) {
            unsigned char* const r = Rec(i);
            if (r[0] == 0) continue;
            if (r[1] != b + 1) continue;
            const unsigned char status = r[3];
            if ((status & 0xF0) != 0x10) continue;
            const U item = r[2];
            unsigned short price;
            unsigned char flags;
            switch (status & 0xF) {
            case 0:
                price = Word(At(::bof3::Img(at::kConsumables.v) + 22u * item + 4));
                flags = At(::bof3::Img(at::kConsumables.v) + 22u * item)[0];
                break;
            case 1:
                price = Word(At(::bof3::Img(at::kWeapons.v) + 28u * item + 9));
                flags = At(::bof3::Img(at::kWeapons.v) + 28u * item)[0];
                break;
            case 2:
                price = Word(At(::bof3::Img(at::kArmour.v) + 26u * item + 7));
                flags = At(::bof3::Img(at::kArmour.v) + 26u * item)[0];
                break;
            default:
                price = Word(At(::bof3::Img(at::kAccessories.v) + 24u * item + 5));
                flags = At(::bof3::Img(at::kAccessories.v) + 24u * item)[0];
                break;
            }
            U tier = 0;
            for (U p = ::bof3::Img(at::kPriceTiers.v); price > Word(At(p));) {
                p += 6;
                ++tier;
                if (p >= ::bof3::Img(at::kPriceTiersEnd.v)) break;
            }
            const U stamp = rv::As<records::CommunityRecord>(r)->stamp;
            if (Game_BattleCount - stamp < Word(At(::bof3::Img(at::kPriceTiers.v) + 6 * tier + 2))) continue;
            r[3] = static_cast<unsigned char>(status & 0xF);
            if (flags & 8) {
                r[3] = static_cast<unsigned char>((status & 0xF) | 0x30);
            } else {
                const unsigned short odds = Word(At(::bof3::Img(at::kPriceTiers.v) + 6 * tier + 4));
                const int scaled = static_cast<int>(static_cast<U>(Trait(i, 0x13)) * odds) / 20;
                const auto e = static_cast<std::int16_t>(static_cast<unsigned short>(scaled + odds));
                const std::int16_t chance = e < 0 ? 0 : (e > 0x64 ? 0x64 : e);
                int roll = SH_CALL(Rand)() & 0x7F;
                if (static_cast<std::int16_t>(roll) > 0x64) roll -= 0x1C;
                if (chance > static_cast<std::int16_t>(roll)) {
                    r[3] |= 0x20;
                } else {
                    const auto keep = static_cast<std::int16_t>(static_cast<unsigned short>((0xFu - Trait(i, 0x13)) * 2u));
                    int roll2 = SH_CALL(Rand)() & 0x7F;
                    if (static_cast<std::int16_t>(roll2) > 0x64) roll2 -= 0x1C;
                    if (keep > static_cast<std::int16_t>(roll2)) {
                        r[3] = 0x40;
                        r[2] = 0x37;
                    } else {
                        r[3] |= 0x30;
                    }
                }
            }
            QueueKindRecord(3, static_cast<unsigned char>(i));
        }
    }
}

// original 0x455DF0 (no PSX twin paired): the buildings of kind 0xB. A level +3 of 0
// becomes 5 with the stamp the clock; one below the cap 0x652958[lit level],
// with residents: (clock - stamp) / (6 - their count) (unsigned; 6 residents
// divide by zero: ours aborts) is added when its low 16 bits are not 0,
// capped; the stamp is the clock and an event 1 is queued with the first
// resident.
extern "C" void __cdecl CommuSim_TickKindB(void) {
    for (U b = 0; b < at::kBuildingCount; ++b) {
        unsigned char* const bl = Bld(b);
        if (bl[0] != 0xB) continue;
        const unsigned char level = bl[3];
        if (level == 0) {
            bl[3] = 5;
            SetUL(bl + 4, Game_BattleCount);
            continue;
        }
        if (level >= At(::bof3::Img(at::kKindBCaps.v) + (UL(::bof3::Img(at::kLevelLit.v)) & 0xFF))[0]) continue;
        unsigned char count = 0;
        for (U i = 0; i < at::kRecordCount; ++i)
            if (Rec(i)[0] != 0 && Rec(i)[1] == b + 1) ++count;
        if (count == 0) continue;
        const U divisor = 6u - count;
        if (divisor == 0)
            bof3::Fatal("CommuSim_TickKindB: building %u has 6 residents - the original divides by 0 (docs/rest_4a.md section 7)", (unsigned)b);
        const U q = (Game_BattleCount - UL(bl + 4)) / divisor;
        if ((q & 0xFFFF) == 0) continue;
        const auto raised = static_cast<unsigned char>(bl[3] + q);
        bl[3] = raised;
        const unsigned char cap = At(::bof3::Img(at::kKindBCaps.v) + (UL(::bof3::Img(at::kLevelLit.v)) & 0xFF))[0];
        if (raised > cap) bl[3] = cap;
        SetUL(bl + 4, Game_BattleCount);
        const unsigned char n = Rest4b_BusyB;
        At(::bof3::Img(at::kEvents.v) + 2u * n)[0] = 1;
        const unsigned char first = FirstOf(b);
        const unsigned char n2 = Rest4b_BusyB;
        At(::bof3::Img(at::kEvents.v) + 2u * n + 1)[0] = first;
        Rest4b_BusyB = static_cast<unsigned char>(n2 + 1);
    }
}

// original 0x455F40 (no PSX twin paired): a record spawned: from Rand() & 0x3F
// (60..63 less 60) the first free record on, wrapping at 60: +0 = 1, +1 = 0,
// +4 the clock, +2 = 0, its five bytes 0x9048F0 + 5r the trait record's +0..+4,
// and r appended to the spawned list 0x904F00. With all 60 in use the
// original walks forever: ours aborts before the walk.
extern "C" void __cdecl CommuSim_AddRecord(void) {
    auto r = static_cast<unsigned char>(SH_CALL(Rand)() & 0x3F);
    if (r >= 0x3C) r = static_cast<unsigned char>(r + 0xC4);
    bool any = false;
    for (U i = 0; i < at::kRecordCount && !any; ++i) any = Rec(i)[0] == 0;
    if (!any) bof3::Fatal("CommuSim_AddRecord: all 60 records are in use - the original searches for a free one forever");
    while (Rec(r)[0] != 0) {
        ++r;
        if (r >= 0x3C) r = 0;
    }
    unsigned char* const rec = Rec(r);
    rec[0] = 1;
    rec[1] = 0;
    rv::As<records::CommunityRecord>(rec)->stamp = Game_BattleCount;
    rec[2] = 0;
    unsigned char* const copy = At(::bof3::Img(at::kRecordCopies.v) + 5u * r);
    SetUL(copy, UL(::bof3::Img(at::kRecordTraits.v) + at::kTraitStride * r));
    copy[4] = Trait(r, 4);
    const unsigned char n = Rest4b_BusyA;
    Rest4b_BusyA = static_cast<unsigned char>(n + 1);
    At(::bof3::Img(at::kSpawned.v) + n)[0] = r;
}

// original 0x455FF0 (no PSX twin paired): a record removed: from Rand() & 0x3F
// (60..63 less 60) the first record in use on, wrapping at 60, passing over
// kind 9 until the walk is back at its start: +0 = 0 and r appended to the
// removed list 0x9039C0. With none in use the original walks forever: ours
// aborts before the walk.
extern "C" void __cdecl CommuSim_RemoveRecord(void) {
    auto r = static_cast<unsigned char>(SH_CALL(Rand)() & 0x3F);
    if (r >= 0x3C) r = static_cast<unsigned char>(r + 0xC4);
    const unsigned char start = r;
    bool any = false;
    for (U i = 0; i < at::kRecordCount && !any; ++i) any = Rec(i)[0] != 0;
    if (!any) bof3::Fatal("CommuSim_RemoveRecord: no record is in use - the original searches for one forever");
    bool round = false;
    for (;;) {
        if (Rec(r)[0] != 0 && (Rec(r)[1] != 9 || round)) break;
        ++r;
        if (r >= 0x3C) r = 0;
        if (r == start) round = true;
    }
    Rec(r)[0] = 0;
    const unsigned char n = Rest4a_RemovedCount;
    Rest4a_RemovedCount = static_cast<unsigned char>(n + 1);
    At(::bof3::Img(at::kRemoved.v) + n)[0] = r;
}

// original 0x456080 (no PSX twin paired; R4B's 0x456E20 calls it): the bytes
// 0x9046CD / 0x9046CE the sums of trait +0x11 over the records in use of kinds
// 0xA and 0xB.
extern "C" void __cdecl CommuSim_SumKindsAB(void) {
    FieldE2_BiasA = 0;
    unsigned char sumB = 0;
    FieldE2_BiasB = sumB;
    for (U i = 0; i < at::kRecordCount; ++i) {
        if (Rec(i)[0] == 0) continue;
        const unsigned char kind = Rec(i)[1];
        if (kind == 0xA) {
            FieldE2_BiasA = static_cast<unsigned char>(FieldE2_BiasA + Trait(i, 0x11));
        } else if (kind == 0xB) {
            sumB = static_cast<unsigned char>(sumB + Trait(i, 0x11));
            FieldE2_BiasB = sumB;
        }
    }
}

// original 0x4560D0 (no PSX twin paired; GameMode_Field and Shop_Close call
// it too): the nine words 0x675F60 cleared, then three groups of three
// distinct words 0x9B + n, n = Rand() & 0x1F brought to at most {15, 20,
// 25}[group] by subtracting Rand() & 0xF, never 0x9F or 0xA0.
extern "C" void __cdecl CommuSim_RollOffers(void) {
    static constexpr unsigned char kLimits[3] = {0xF, 0x14, 0x19};
    SetUL(::bof3::Img(at::kOfferWords.v), 0);
    SetUL(::bof3::Img(at::kOfferWords.v) + 4, 0);
    SetUL(::bof3::Img(at::kOfferWords.v) + 8, 0);
    SetUL(::bof3::Img(at::kOfferWords.v) + 0xC, 0);
    SetWord(At(::bof3::Img(at::kOfferWords.v) + 0x10), 0);
    for (U g = 0; g < 3; ++g) {
        const U group = ::bof3::Img(at::kOfferWords.v) + 6 * g;
        U filled = 0;
        do {
            auto n = static_cast<unsigned char>(SH_CALL(Rand)() & 0x1F);
            while (n > kLimits[g]) n = static_cast<unsigned char>(n - (SH_CALL(Rand)() & 0xF));
            const auto v = static_cast<unsigned short>(n + 0x9B);
            U k = 0;
            while (k < 3 && v != Word(At(group + 2 * k))) ++k;
            if (v != 0x9F && v != 0xA0 && k >= 3) {
                SetWord(At(::bof3::Img(at::kOfferWords.v) + 2 * (filled + 3 * g)), v);
                ++filled;
            }
        } while (filled < 3);
    }
}

// original 0x4561A0 (no PSX twin paired): the village's objects. The resident
// counts 0x675F58 (8 bytes) cleared; for each record in use, in order, the
// next Sprite_Objects record: kind 0 - CommuPose_Kind0 and CommuSim_PlaceLoose;
// 1..8 (a building) - CommuSim_PlaceResident; 0xA - the pose word +0x88 0xCD
// from area 0xB6 on, else 0xB6 + a roll of 0..5, and PlaceLoose; 0xB - 0xCE in
// areas 0xAF, 0xB2, 0xB5, 0xB9, else 0xBC + a roll, and PlaceLoose; any other
// kind takes no object. The objects after the last up to 20 are cleared (+0).
// The object index is not bounded (60 records reach past Sprite_Objects).
extern "C" void __cdecl CommuSim_PlaceObjects(void) {
    SetUL(::bof3::Img(at::kResidentCounts.v), 0);
    SetUL(::bof3::Img(at::kResidentCounts.v) + 4, 0);
    U object = 0;
    for (U i = 0; i < at::kRecordCount; ++i) {
        if (Rec(i)[0] == 0) continue;
        const unsigned char kind = Rec(i)[1];
        if (kind == 0) {
            SH_CALL(CommuPose_Kind0)(object);
            SH_CALL(CommuSim_PlaceLoose)(i, object);
        } else if (kind < 9) {
            SH_CALL(CommuSim_PlaceResident)(i, object);
        } else if (kind == 0xA) {
            if (Game_AreaNumber >= 0xB6) {
                SetWord(Pose(object), 0xCD);
            } else {
                auto roll = static_cast<unsigned char>(SH_CALL(Rand)() & 7);
                if (roll > 5) roll = static_cast<unsigned char>(roll + 0xFB);
                SetWord(Pose(object), roll + 0xB6u);
            }
            SH_CALL(CommuSim_PlaceLoose)(i, object);
        } else if (kind == 0xB) {
            const unsigned short area = Game_AreaNumber;
            if (area == 0xAF || area == 0xB2 || area == 0xB5 || area == 0xB9) {
                SetWord(Pose(object), 0xCE);
            } else {
                auto roll = static_cast<unsigned char>(SH_CALL(Rand)() & 7);
                if (roll > 5) roll = static_cast<unsigned char>(roll + 0xFB);
                SetWord(Pose(object), roll + 0xBCu);
            }
            SH_CALL(CommuSim_PlaceLoose)(i, object);
        } else {
            continue;
        }
        ++object;
    }
    if (static_cast<int>(object) < 0x14)
        for (U o = object; o < 0x14; ++o) At(::bof3::Img(Sprite_Objects_addr) + at::kSpriteStride * o)[0] = 0;
}

namespace {
// The part CommuSim_PlaceLoose and _PlaceResident end with: the animation bank
// by +5 % 6, then the facing +8 (Sprite_Current read again for each).
void BankAndFace() {
    const unsigned short bank = Word(At(::bof3::Img(at::kBanks.v) + 2u * (Sprite_Current[5] % 6u)));
    SH_CALL(Sprite_SetAnimationBank)(bank);
    SH_CALL(Sprite_FaceDirection)(Sprite_Current[8]);
}
}  // namespace

// original 0x4562B0 (no PSX twin paired): a loose object (kinds 0, 0xA, 0xB):
// Sprite_Current the object; +6 = 1; by the object's place 0x652960 (3 bytes
// by object: x, z, a flag): flag set - +1 = 0, +0x84 = 2, +2 = 1; clear - +1 =
// 6, +0x84 = 0, +2 = 0; x and z in the high words of +0x34 / +0x38 (low words
// 0); +0x3E AreaMap_Elevation of the two (the record's own, not through
// Sprite_Current); +5 the record; the bank and the facing.
extern "C" void __cdecl CommuSim_PlaceLoose(unsigned record, unsigned object) {
    const U o = object & 0xFFu;
    unsigned char* const base = At(::bof3::Img(Sprite_Objects_addr) + at::kSpriteStride * o);
    Sprite_Current = base;
    base[6] = 1;
    if (At(::bof3::Img(at::kObjectPlaces.v) + 3 * o + 2)[0] != 0) {
        Sprite_Current[1] = 0;
        base[0x84] = 2;
        Sprite_Current[2] = 1;
    } else {
        Sprite_Current[1] = 6;
        base[0x84] = 0;
        Sprite_Current[2] = 0;
    }
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->x_whole, At(::bof3::Img(at::kObjectPlaces.v) + 3 * o)[0]);
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->x_frac, 0);
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->z_whole, At(::bof3::Img(at::kObjectPlaces.v) + 3 * o + 1)[0]);
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->z_frac, 0);
    const long height = SH_CALL(AreaMap_Elevation)(rv::As<records::SpriteObject>(base)->x, rv::As<records::SpriteObject>(base)->z);
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->height_whole, static_cast<unsigned>(height));
    Sprite_Current[5] = static_cast<unsigned char>(record);
    BankAndFace();
}

// original 0x4563B0 (no PSX twin paired): a resident (record kind 1..8, its
// building the kind less 1): Sprite_Current the object; +5 the record, +1 = 6,
// +0x84 = 0, +2 = 0; from 0x65299C by (the building's resident count + 3 x
// building) x 3: x, z (high words of +0x34 / +0x38), AreaMap_Elevation into
// +0x3E, the facing +8; +0x48 = 0; the bank and the facing; then the pose by
// the building's kind through the 14-entry table at 0x45663C (0
// CommuPose_Kind0, 4..0xD their CommuPose_Kind*, 1..3 none; above 0xD none),
// and the building's count up. The count is not bounded (a fourth resident
// reads the next building's places).
extern "C" void __cdecl CommuSim_PlaceResident(unsigned record, unsigned object) {
    const auto building = static_cast<unsigned char>(Rec(record & 0xFFu)[1] - 1);
    const U o = object & 0xFFu;
    const U row = 3u * building;
    unsigned char* const base = At(::bof3::Img(Sprite_Objects_addr) + at::kSpriteStride * o);
    Sprite_Current = base;
    Sprite_Current[5] = static_cast<unsigned char>(record);
    Sprite_Current[1] = 6;
    base[0x84] = 0;
    Sprite_Current[2] = 0;
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->x_whole, At(::bof3::Img(at::kResidentPlaces.v) + 3 * (Count(building) + row))[0]);
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->x_frac, 0);
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->z_whole, At(::bof3::Img(at::kResidentPlaces.v) + 3 * (Count(building) + row) + 1)[0]);
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->z_frac, 0);
    unsigned char* const s = Sprite_Current;
    const long height = SH_CALL(AreaMap_Elevation)(rv::As<records::SpriteObject>(s)->x, rv::As<records::SpriteObject>(s)->z);
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->height_whole, static_cast<unsigned>(height));
    const unsigned char facing = At(::bof3::Img(at::kResidentPlaces.v) + 3 * (Count(building) + row) + 2)[0];
    Sprite_Current[8] = facing;
    Sprite_Current[0x48] = 0;
    BankAndFace();
    switch (Bld(building)[0]) {
    case 0: SH_CALL(CommuPose_Kind0)(object); break;
    case 4: SH_CALL(CommuPose_Kind4)(object, building); break;
    case 5: SH_CALL(CommuPose_Kind5)(object, building); break;
    case 6: SH_CALL(CommuPose_Kind6)(object, building); break;
    case 7: SH_CALL(CommuPose_Kind7)(object); break;
    case 8: SH_CALL(CommuPose_Kind8)(record, object, building); break;
    case 9: SH_CALL(CommuPose_Kind9)(record, object); break;
    case 0xA: SH_CALL(CommuPose_KindA)(object, building); break;
    case 0xB: SH_CALL(CommuPose_KindB)(object, building); break;
    case 0xC: SH_CALL(CommuPose_KindC)(object, building); break;
    case 0xD: SH_CALL(CommuPose_KindD)(object); break;
    default: break;
    }
    At(::bof3::Img(at::kResidentCounts.v) + building)[0] = static_cast<unsigned char>(Count(building) + 1);
}

// --- the poses (each: the object's +0x88 word; +6 through Sprite_Current) ---

// original 0x456680 (no PSX twin paired): the object's +6 = 1 and its pose
// 0xC2 + a roll of 0..4 (Rand() & 7, above 4 less 4).
extern "C" void __cdecl CommuPose_Kind0(unsigned object) {
    auto roll = static_cast<unsigned char>(SH_CALL(Rand)() & 7);
    if (roll > 4) roll = static_cast<unsigned char>(roll + 0xFC);
    const U o = object & 0xFFu;
    At(::bof3::Img(Sprite_Objects_addr) + 0x6 + at::kSpriteStride * o)[0] = 1;
    SetWord(Pose(o), roll + 0xC2u);
}

// original 0x4566C0 (no PSX twin paired): +6 = 1; 0xCC when the lit level is 7
// and the building's +1 set, or the dark level 10 and it clear; else 0xC8 +
// the building's +1.
extern "C" void __cdecl CommuPose_Kind4(unsigned object, unsigned building) {
    Sprite_Current[6] = 1;
    const unsigned char flag = Bld(building & 0xFFu)[1];
    const U o = object & 0xFFu;
    if ((Rest4a_LevelLit == 7 && flag != 0) || (FieldE2_LevelA == 0xA && Bld(building & 0xFFu)[1] == 0))
        SetWord(Pose(o), 0xCC);
    else
        SetWord(Pose(o), Bld(building & 0xFFu)[1] + 0xC8u);
}

// original 0x456750 (no PSX twin paired): with residents counted already, +6
// = 1 and 0xD1 + the count; the first: +6 = 5, pose 0xC008, +0x18 = 0 and
// +0x1C = +2 + 2 x +1 + 0x11 of the building (dwords).
extern "C" void __cdecl CommuPose_Kind5(unsigned object, unsigned building) {
    const U b = building & 0xFFu;
    const U o = object & 0xFFu;
    if (Count(b) != 0) {
        Sprite_Current[6] = 1;
        SetWord(Pose(o), Count(b) + 0xD1u);
        return;
    }
    Sprite_Current[6] = 5;
    SetWord(Pose(o), 0xC008);
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->work_18, 0);
    const U v = Bld(b)[2] + 2u * Bld(b)[1] + 0x11;
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->work_1c, v);
}

// original 0x4567F0 (no PSX twin paired): the first resident: +6 = 5, +0x18 =
// 1, +0x1C = 1, pose 0xCA; later ones +6 = 1 and 0xC000 + the count.
extern "C" void __cdecl CommuPose_Kind6(unsigned object, unsigned building) {
    const U b = building & 0xFFu;
    const U o = object & 0xFFu;
    const unsigned char count = Count(b);
    unsigned char* const s = Sprite_Current;
    if (count == 0) {
        s[6] = 5;
        rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->work_18, 1);
        rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->work_1c, 1);
        SetWord(Pose(o), 0xCA);
        return;
    }
    s[6] = 1;
    SetWord(Pose(o), Count(b) + 0xC000u);
}

// original 0x456870 (no PSX twin paired): +6 = 1, pose 0xC003.
extern "C" void __cdecl CommuPose_Kind7(unsigned object) {
    Sprite_Current[6] = 1;
    SetWord(Pose(object & 0xFFu), 0xC003);
}

// original 0x4568A0 (no PSX twin paired): +6 = 1; the pose the offer word
// 0x675F60[count + 3 x age], age 0 below 20 since the record's stamp, 1 below
// 40, else 2. (The original keeps the age in its first argument's slot.)
extern "C" void __cdecl CommuPose_Kind8(unsigned record, unsigned object, unsigned building) {
    const U r = record & 0xFFu;
    Sprite_Current[6] = 1;
    const U age = Game_BattleCount - rv::As<records::CommunityRecord>(Rec(r))->stamp;
    const U row = age < 0x14 ? 0 : (age < 0x28 ? 1 : 2);
    const U index = Count(building & 0xFFu) + 3 * row;
    SetWord(Pose(object & 0xFFu), Word(At(::bof3::Img(at::kOfferWords.v) + 2 * index)));
}

// original 0x456920 (no PSX twin paired): by the record's +2: 0 - +6 = 1, pose
// 0x51; 1 - Sprite_Current +0 = 0 (the object off); else +6 = 1, pose 0xC004.
extern "C" void __cdecl CommuPose_Kind9(unsigned record, unsigned object) {
    const unsigned char state = Rec(record & 0xFFu)[2];
    if (state == 0) {
        Sprite_Current[6] = 1;
        SetWord(Pose(object & 0xFFu), 0x51);
        return;
    }
    if (state == 1) {
        Sprite_Current[0] = 0;
        return;
    }
    Sprite_Current[6] = 1;
    SetWord(Pose(object & 0xFFu), 0xC004);
}

// original 0x456990 (no PSX twin paired): later residents +6 = 1, pose 0xCF +
// the count; the first +6 = 5, +0x18 = 6, +0x1C = 0, pose 0xCB.
extern "C" void __cdecl CommuPose_KindA(unsigned object, unsigned building) {
    const U b = building & 0xFFu;
    const U o = object & 0xFFu;
    const unsigned char count = Count(b);
    unsigned char* const s = Sprite_Current;
    if (count != 0) {
        s[6] = 1;
        SetWord(Pose(o), Count(b) + 0xCFu);
        return;
    }
    s[6] = 5;
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->work_18, 6);
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Sprite_Current)->work_1c, 0);
    SetWord(Pose(o), 0xCB);
}

// original 0x456A10 (no PSX twin paired): +6 = 1; the first resident 0xC009,
// the second 0xC00B with story flag 0x92 set (Flags_Test) or 0xD4, later ones
// 0xD5.
extern "C" void __cdecl CommuPose_KindB(unsigned object, unsigned building) {
    Sprite_Current[6] = 1;
    const unsigned char count = Count(building & 0xFFu);
    const U o = object & 0xFFu;
    if (count == 0) {
        SetWord(Pose(o), 0xC009);
        return;
    }
    if (count == 1) {
        const unsigned char set = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x92);
        SetWord(Pose(o), set ? 0xC00B : 0xD4);
        return;
    }
    SetWord(Pose(o), 0xD5);
}

// original 0x456AB0 (no PSX twin paired): +6 = 1, pose 0xC005 + the count.
extern "C" void __cdecl CommuPose_KindC(unsigned object, unsigned building) {
    Sprite_Current[6] = 1;
    SetWord(Pose(object & 0xFFu), Count(building & 0xFFu) + 0xC005u);
}

// original 0x456AF0 (no PSX twin paired): +6 = 1, pose 0xC00A.
extern "C" void __cdecl CommuPose_KindD(unsigned object) {
    Sprite_Current[6] = 1;
    SetWord(Pose(object & 0xFFu), 0xC00A);
}

// ===========================================================================
// Field_ObjectTriggers entries (object, 0x904030) -> al 0
// ===========================================================================

namespace {
// The tail-kind triggers: the kind to 0x9039F3, the object to 0x939A38, a
// sub-kind (when given) to 0x9039F5, ScriptFlags_Set40, al 0.
unsigned char TailTrigger(unsigned char* object, unsigned char kind) {
    AreaW0a_TailKind = kind;
    Rest4d_Pointer = Low32(object);
    SH_CALL(ScriptFlags_Set40)();
    return 0;
}
unsigned char TailTriggerSub(unsigned char* object, unsigned char kind, unsigned char sub) {
    AreaW0a_TailKind = kind;
    Rest4d_Pointer = Low32(object);
    AreaW0c_TailSub = sub;
    SH_CALL(ScriptFlags_Set40)();
    return 0;
}
char* Row(U a) { return reinterpret_cast<char*>(At(a)); }
const char* Format() { return reinterpret_cast<const char*>(At(::bof3::Img(at::kNumberFormat.v))); }
}  // namespace

// original 0x456B20 (Field_ObjectTriggers id 1; no PSX twin paired): tail kind
// 0xE for the object.
extern "C" unsigned char __cdecl FieldTrigger01(unsigned char* object, unsigned char*) { return TailTrigger(object, 0xE); }

// original 0x456B70 (id 2): four numbers into Text_Records rows 0..3 by the
// format Area08_MessageFormat - the bytes 0x9040C8 and 0x9040C9, the dwords
// 0x904134 (the community's clock) and 0x904138 - then Msg_OpenScript(0x55)
// and Field_Request 2.
extern "C" unsigned char __cdecl FieldTrigger02(unsigned char*, unsigned char*) {
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow0.v)), Format(), static_cast<U>(At(::bof3::Img(at::kCountA.v))[0]));
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow1.v)), Format(), static_cast<std::uint32_t>(MapFieldObjects_ClockMinutes));
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow2.v)), Format(), Game_BattleCount);
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow3.v)), Format(), Rest4a_CountC);
    SH_CALL(Msg_OpenScript)(0x55);
    Field_Request = 2;
    return 0;
}

// original 0x456BF0 (id 3): three numbers - the dwords 0x90413C, 0x904140,
// 0x904144 - into rows 0..2, Msg_OpenScript(0x56), Field_Request 2.
extern "C" unsigned char __cdecl FieldTrigger03(unsigned char*, unsigned char*) {
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow0.v)), Format(), Rest4a_CountD);
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow1.v)), Format(), FieldE1_ContentCount);
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow2.v)), Format(), Rest4a_CountF);
    SH_CALL(Msg_OpenScript)(0x56);
    Field_Request = 2;
    return 0;
}

// original 0x456C50 (id 4): tail kind 0x15, sub-kind the object's +5.
extern "C" unsigned char __cdecl FieldTrigger04(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x15, object[5]); }
// original 0x456C70 (id 5): tail kind 0x16, sub-kind the object's +5.
extern "C" unsigned char __cdecl FieldTrigger05(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x16, object[5]); }
// original 0x456C90 (id 6): tail kind 0x17 (the sub-kind left).
extern "C" unsigned char __cdecl FieldTrigger06(unsigned char* object, unsigned char*) { return TailTrigger(object, 0x17); }
// original 0x456CB0 (id 7): tail kind 0x17, sub-kind 2.
extern "C" unsigned char __cdecl FieldTrigger07(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x17, 2); }
// original 0x456CD0 (id 8): tail kind 0x17, sub-kind 3.
extern "C" unsigned char __cdecl FieldTrigger08(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x17, 3); }

// original 0x456B40 (id 9): the shop record by the object's dword +0x1C
// (23-byte Shop_Records, unchecked) takes at +0 the level +3 of the building
// (1-based) of the community record the object's +5 names; al 0.
extern "C" unsigned char __cdecl FieldTrigger09(unsigned char* object, unsigned char*) {
    const unsigned char building = Rec(object[5])[1];
    const U shop = UL(object + 0x1C);
    At(::bof3::Img(at::kShopRecords.v) + at::kShopStride * shop)[0] = At(::bof3::Img(at::kBuildingLevels.v) + 8u * building)[0];
    return 0;
}

// original 0x456CF0 (id 10): tail kind 0x18, sub-kind the object's +5.
extern "C" unsigned char __cdecl FieldTrigger10(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x18, object[5]); }
// original 0x456D30 (id 11): tail kind 0x19, sub-kind the object's +5.
extern "C" unsigned char __cdecl FieldTrigger11(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x19, object[5]); }
// original 0x456D10 (id 61): tail kind 0x3C, sub-kind the object's +5.
extern "C" unsigned char __cdecl FieldTrigger61(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x3C, object[5]); }

void Rest4A_Inject() {
    if (bof3::WantsShadow("rest_4a")) rest_4a::SelfTest();
    BOF3_INJECT(CommuSim_AreaEnter);
    BOF3_INJECT(CommuSim_QueueAreas);
    BOF3_INJECT(CommuSim_Population);
    BOF3_INJECT(CommuSim_Mood);
    BOF3_INJECT(CommuSim_LevelLit);
    BOF3_INJECT(CommuSim_LevelDark);
    BOF3_INJECT(CommuSim_TickKind5);
    BOF3_INJECT(CommuSim_TickKind9);
    BOF3_INJECT(CommuSim_TickKindD);
    BOF3_INJECT(CommuSim_TickKindB);
    BOF3_INJECT(CommuSim_AddRecord);
    BOF3_INJECT(CommuSim_RemoveRecord);
    BOF3_INJECT(CommuSim_SumKindsAB);
    BOF3_INJECT(CommuSim_RollOffers);
    BOF3_INJECT(CommuSim_PlaceObjects);
    BOF3_INJECT(CommuSim_PlaceLoose);
    BOF3_INJECT(CommuSim_PlaceResident);
    BOF3_INJECT(CommuPose_Kind0);
    BOF3_INJECT(CommuPose_Kind4);
    BOF3_INJECT(CommuPose_Kind5);
    BOF3_INJECT(CommuPose_Kind6);
    BOF3_INJECT(CommuPose_Kind7);
    BOF3_INJECT(CommuPose_Kind8);
    BOF3_INJECT(CommuPose_Kind9);
    BOF3_INJECT(CommuPose_KindA);
    BOF3_INJECT(CommuPose_KindB);
    BOF3_INJECT(CommuPose_KindC);
    BOF3_INJECT(CommuPose_KindD);
    BOF3_INJECT(FieldTrigger01);
    BOF3_INJECT(FieldTrigger09);
    BOF3_INJECT(FieldTrigger02);
    BOF3_INJECT(FieldTrigger03);
    BOF3_INJECT(FieldTrigger04);
    BOF3_INJECT(FieldTrigger05);
    BOF3_INJECT(FieldTrigger06);
    BOF3_INJECT(FieldTrigger07);
    BOF3_INJECT(FieldTrigger08);
    BOF3_INJECT(FieldTrigger10);
    BOF3_INJECT(FieldTrigger61);
    BOF3_INJECT(FieldTrigger11);
}
