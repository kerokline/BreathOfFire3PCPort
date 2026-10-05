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
//                   (8 of 8) moved on by the clock 0x904134, the event queue
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

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
void SetUL(U a, U v) { SetUL(At(a), v); }
unsigned char* Ptr(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

// The community's records and buildings, and the traits table by record.
unsigned char* Rec(U i) { return At(at::kRecords + 8u * i); }
unsigned char* Bld(U b) { return At(at::kBuildings + 8u * b); }
unsigned char Trait(U i, U field) { return At(at::kRecordTraits + at::kTraitStride * i + field)[0]; }
unsigned char* Pose(U object) { return At(at::kSpritePose + at::kSpriteStride * object); }
unsigned char Count(U building) { return At(at::kResidentCounts + building)[0]; }

// The event queue: kind at +0, the record at +1 (the levels and the area
// queue write +0 only, in place).
void QueueKindRecord(unsigned char kind, unsigned char record) {
    const unsigned char n = At(at::kEventCount)[0];
    At(at::kEvents + 2u * n)[0] = kind;
    At(at::kEvents + 2u * n + 1)[0] = record;
    At(at::kEventCount)[0] = static_cast<unsigned char>(n + 1);
}
// The first record in use whose +1 is building + 1; 60 when none.
unsigned char FirstOf(U building) {
    unsigned char j = 0;
    for (U i = 0; i < at::kRecordCount; ++i, ++j)
        if (Rec(i)[0] != 0 && Rec(i)[1] == building + 1) break;
    return j;
}

unsigned char CommuCount() { return SH_AT(unsigned char (__cdecl*)(), at::kCommuCount)(); }

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

// original 0x452DD0 (PSX 0x800A626C, call-anchored; its name not in the
// sibling): the auto-target check for an actor. 1 at once when the battle
// flags 0x904AA8 lack bit 14, when the actor in 0x904B8B is out
// (Battle_ActorIsOut of the flags' dword with that byte in al, as pushed), or
// when that actor is the argument's low byte. A party argument stores 0x904B8B
// as the target 0x904B44 and answers 0. An enemy argument answers 1 when its
// record has +0xBA above 1, +0x92 bit 5 clear and +0x114 bit 14 clear; else,
// with 0x904B35 not 4, the target is 0x904B8B and the answer 0; with it 4, the
// enemy's ability (+0x106) flags byte: bit 4 - target 0xC0 with bit 7, else
// 0x80 when 0x904B8B is an enemy, 0x40 a member, answer 1; bit 4 and 6 clear -
// the target is the argument, answer 1; bit 6 alone - 0x904B8B, 0.
// The enemy index is not checked (argument 3..255 indexes 0x128 records).
extern "C" unsigned char __cdecl Battle_AutoTargetCheck(unsigned actor) {
    const U flags = UL(at::kBattleFlags);
    if (!(flags & 0x4000)) return 1;
    const unsigned char out = SH_CALL(Battle_ActorIsOut)((flags & 0xFFFFFF00u) | At(at::kForcedActor)[0]);
    if (out) return 1;
    const unsigned char forced = At(at::kForcedActor)[0];
    const auto a = static_cast<unsigned char>(actor);
    if (forced == a) return 1;
    if (a < 3) {
        At(at::kTarget)[0] = forced;
        return 0;
    }
    const U e = at::kEnemyStride * (static_cast<U>(a) - 3);
    if (Word(At(at::kEnemyHp + e)) > 1 && !(At(at::kEnemyStatus + e)[0] & 0x20) && !(UL(at::kEnemyFlags + e) & 0x4000))
        return 1;
    if (At(at::kAutoMode)[0] != 4) {
        At(at::kTarget)[0] = forced;
        return 0;
    }
    const unsigned char bits = At(at::kAbilityFlags + at::kAbilityStride * Word(At(at::kEnemyAbility + e)))[0];
    if (bits & 0x10) {
        if (bits & 0x80) At(at::kTarget)[0] = 0xC0;
        else At(at::kTarget)[0] = forced >= 3 ? 0x80 : 0x40;
        return 1;
    }
    if (!(bits & 0x40)) {
        At(at::kTarget)[0] = a;
        return 1;
    }
    At(at::kTarget)[0] = forced;
    return 0;
}

// original 0x452EB0 (PSX 0x800A64BC, call-anchored): of members 0..2, those
// Battle_ActorIsOut rules in, the one at Rand() % their count. The count is
// not tested (0 divides by zero: ours aborts); the CRT's Rand is 0..0x7FFF
// (a negative answer would read below the list: ours aborts).
extern "C" unsigned char __cdecl Battle_RandomLiveMember(void) {
    unsigned char list[4] = {};
    unsigned char count = 0;
    for (unsigned char m = 0; m <= 2; ++m)
        if (SH_CALL(Battle_ActorIsOut)(m) == 0) list[count++] = m;
    const int r = SH_CALL(Rand)();
    if (count == 0) bof3::Fatal("Battle_RandomLiveMember: every member is out - the original divides by 0 (docs/rest_4a.md section 7)");
    const int k = r % static_cast<int>(count);
    if (k < 0) bof3::Fatal("Battle_RandomLiveMember: Rand answered %d - the original reads its frame below the list", r);
    return list[k];
}

// original 0x452F10 (PSX 0x800A6408, call-anchored): of enemies 3..10, those
// Battle_ActorIsOut rules in, the one at Rand() % their count - in a list of
// four bytes. The original keeps the list at esp + 4 of its frame, the count
// in the dword after it and the loop byte in the dword after that, so a fifth
// enemy standing writes the count byte with the actor (the count becomes the
// actor's number) and later ones the count's upper bytes and the loop dword's;
// Rand() % that count then reads any of the twelve bytes. Ours keeps the same
// twelve bytes, so every pick the original can make is ours too - but the
// upper bytes of the count and loop dwords (frame bytes 5..7 and 9..11), which
// the original never writes before it may read them, are 0 here, where the
// original has whatever the stack held (docs/rest_4a.md section 7, L1: wants a
// ledger entry). With four or fewer enemies standing nothing differs.
extern "C" unsigned char __cdecl Battle_RandomLiveEnemy(void) {
    unsigned char f[12] = {};   // the list f[0..3], the count dword f[4..7], the loop dword f[8..11]
    unsigned char i = 3;
    f[8] = i;
    do {
        const U pushed = f[8] | static_cast<U>(f[9]) << 8 | static_cast<U>(f[10]) << 16 | static_cast<U>(f[11]) << 24;
        if (SH_CALL(Battle_ActorIsOut)(pushed) == 0) {
            const unsigned char c = f[4];
            if (c >= 12) bof3::Fatal("Battle_RandomLiveEnemy: the list's index %u reaches the return address", (unsigned)c);
            f[4] = static_cast<unsigned char>(c + 1);
            f[c] = i;
        }
        ++i;
        f[8] = i;
    } while (i <= 10);
    const int r = SH_CALL(Rand)();
    const unsigned char count = f[4];
    if (count == 0) bof3::Fatal("Battle_RandomLiveEnemy: every enemy is out - the original divides by 0 (docs/rest_4a.md section 7)");
    const int k = r % static_cast<int>(count);
    if (k < 0) bof3::Fatal("Battle_RandomLiveEnemy: Rand answered %d - the original reads its frame below the list", r);
    return f[k];
}

// original 0x454260 (no PSX twin paired): 1 when the member's action word
// (ObjTrio + 0x126 of the argument's low byte, unchecked) is 0xE or 0x128.
extern "C" unsigned char __cdecl Battle_MemberActionIs0E(unsigned member) {
    const unsigned short w = Word(At(at::kMemberAction + at::kMemberStride * (member & 0xFFu)));
    if (w == 0xE) return 1;
    return w == 0x128 ? 1 : 0;
}

// original 0x454290 (no PSX twin paired): a member's fixed auto-target. The
// member's odds byte +0x142 (signed, at most 5) against Rand() % 20: below it,
// and 0x904AB1 not 1, Battle_RandomOtherMember(member); else
// Battle_DefaultTarget((Rand() & 7) + 3). Then +0x125 = 1, 0x904B35 = 1 and
// the target 0x904B44 the answer.
extern "C" void __cdecl Battle_MemberAutoFixed(unsigned member) {
    const U m = member & 0xFFu;
    auto odds = static_cast<signed char>(At(at::kMemberOdds + at::kMemberStride * m)[0]);
    if (odds > 5) odds = 5;
    const int r = SH_CALL(Rand)();
    const int roll = r % 20;
    unsigned char target;
    if (roll < odds && At(at::kPartyPick)[0] != 1) {
        target = SH_CALL(Battle_RandomOtherMember)(member);
    } else {
        const int r2 = SH_CALL(Rand)();
        target = SH_CALL(Battle_DefaultTarget)((static_cast<U>(r2) & 0xFFFFFF00u) | static_cast<unsigned char>((r2 & 7) + 3));
    }
    At(at::kMemberMode + at::kMemberStride * m)[0] = 1;
    At(at::kAutoMode)[0] = 1;
    At(at::kTarget)[0] = target;
}

// original 0x454310 (no PSX twin paired): of members 0..2, those
// Battle_ActorIsOut rules in and that are not the argument's low byte, the one
// at Rand() % their count. As Battle_RandomLiveMember: 0 divides by zero (a
// member alone and standing, picking another: ours aborts).
extern "C" unsigned char __cdecl Battle_RandomOtherMember(unsigned exclude) {
    unsigned char list[4] = {};
    unsigned char count = 0;
    for (unsigned char m = 0; m <= 2; ++m)
        if (SH_CALL(Battle_ActorIsOut)(m) == 0 && static_cast<unsigned char>(exclude) != m) list[count++] = m;
    const int r = SH_CALL(Rand)();
    if (count == 0) bof3::Fatal("Battle_RandomOtherMember: no other member standing - the original divides by 0 (docs/rest_4a.md section 7)");
    const int k = r % static_cast<int>(count);
    if (k < 0) bof3::Fatal("Battle_RandomOtherMember: Rand answered %d - the original reads its frame below the list", r);
    return list[k];
}

// ===========================================================================
// A Field_Slots record's step
// ===========================================================================

// original 0x455300 (Field_RunSlots' callee; the PSX FUN_80197D84): record
// Field_Slots + 16 * index. When its object's (+0xC) pose byte +0x4B differs
// from +2, the script +8 is the dword table +4's entry by that pose, its
// frame count +1 the script's byte 3, and +2 the pose (read again). When +1
// is 0 the script steps 4 bytes, back by 4 * byte 1 at an 0xFF, and +1 is the
// new step's byte 3. Then Field_SlotClutCopy(record) and +1 less 1.
extern "C" void __cdecl Field_RunSlot(unsigned char index) {
    unsigned char* const r = Field_Slots + 16u * index;
    unsigned char* const obj = Ptr(UL(r + 0xC));
    const unsigned char pose = obj[0x4B];
    if (pose != r[2]) {
        const U p = UL(Ptr(UL(r + 4)) + 4u * pose);
        SetUL(r + 8, p);
        r[1] = Ptr(p)[3];
        r[2] = obj[0x4B];
    }
    if (r[1] == 0) {
        U p = UL(r + 8) + 4;
        SetUL(r + 8, p);
        if (Ptr(p)[0] == 0xFF) {
            p -= 4u * Ptr(p)[1];
            SetUL(r + 8, p);
        }
        r[1] = Ptr(UL(r + 8))[3];
    }
    SH_CALL(Field_SlotClutCopy)(r);
    r[1] = static_cast<unsigned char>(r[1] - 1);
}

// original 0x455380 (no PSX twin paired): the slot's frame +3 split by the
// object's (+0xC) kind +0x28 through the divisors 0x6528E4: the quotient is a
// CLUT row (+0x10 with the object's +0x24 bit 2), the remainder times
// 0x6528EC[kind] an offset added to the script's (+8) bytes 0 and 1; then
// script byte 2 colours (its count read again each turn) are copied along the
// row from column byte 1 + offset to byte 0 + offset, and Gfx_ClutStripDirty
// set. Columns and the row are bytes (they wrap); a divisor of 0 faults in the
// original (kinds 5..7 of the table): ours aborts.
extern "C" void __cdecl Field_SlotClutCopy(unsigned char* slot) {
    unsigned char* const obj = Ptr(UL(slot + 0xC));
    const int frame = slot[3];
    const unsigned char kind = obj[0x28];
    const int divisor = At(at::kSlotDivisors + kind)[0];
    if (divisor == 0) bof3::Fatal("Field_SlotClutCopy: object kind %u has no frames - the original divides by 0", (unsigned)kind);
    auto row = static_cast<unsigned char>(frame / divisor);
    const auto offset = static_cast<unsigned char>((frame % divisor) * At(at::kSlotSteps + kind)[0]);
    const unsigned char* const script = Ptr(UL(slot + 8));
    const auto to = static_cast<unsigned char>(script[0] + offset);
    const auto from = static_cast<unsigned char>(script[1] + offset);
    if (obj[0x24] & 4) row = static_cast<unsigned char>(row + 0x10);
    if (script[2] != 0) {
        unsigned char i = 0;
        do {
            const U base = static_cast<U>(row) << 8 | i;
            const unsigned short c = Gfx_ClutStrip[base + from];
            ++i;
            Gfx_ClutStrip[base + to] = c;
        } while (i < Ptr(UL(slot + 8))[2]);
    }
    Gfx_ClutStripDirty = 1;
}

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
    if (Game_AreaNumber == Word(At(at::kPrevArea))) {
        SH_CALL(CommuSim_PlaceObjects)();
        return;
    }
    const unsigned char n = CommuCount();
    SH_CALL(CommuSim_RollOffers)();
    At(at::kRemovedCount)[0] = 0;
    At(at::kSpawnCount)[0] = 0;
    At(at::kEventCount)[0] = 0;
    if (n == 0) {
        for (int k = 0; k < 3; ++k) SH_CALL(CommuSim_AddRecord)();
        const U clock = UL(at::kClock);
        const unsigned short area = Game_AreaNumber;
        SetUL(at::kClockA, 0);
        SetUL(at::kStampGrow, clock);
        SetUL(at::kStampShrink, clock);
        SetUL(at::kStampMood, clock);
        SetUL(at::kStampLevelLit, clock);
        SetUL(at::kStampLevelDark, clock);
        At(at::kMood)[0] = 0xA;
        At(at::kLevelDark)[0] = 1;
        At(at::kLevelLit)[0] = 0;
        At(at::kSumA)[0] = 0;
        At(at::kSumB)[0] = 0;
        SetWord(At(at::kAreaSeen), area);
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
    if (area <= Word(At(at::kAreaSeen))) return;
    int n = static_cast<int>((UL(at::kAreaSeen) & 0xFFFF) + 1);
    const int last = area;
    if (n <= last) {
        unsigned char count = At(at::kEventCount)[0];
        unsigned char seen = 0;
        for (; n <= last; ++n) {
            const unsigned char kind = At(at::kAreaKinds + static_cast<U>(n))[0];
            if (kind == 4) {
                if (!(seen & 1)) {
                    At(at::kEvents + 2u * count)[0] = 4;
                    ++count;
                    seen |= 1;
                }
            } else if (kind == 5) {
                if (!(seen & 2)) {
                    At(at::kEvents + 2u * count)[0] = 5;
                    ++count;
                    seen |= 2;
                }
            }
        }
        At(at::kEventCount)[0] = count;
    }
    SetWord(At(at::kAreaSeen), area);
}

// original 0x4555D0 (no PSX twin paired): population by mood. With mood
// (s8 0x9046CA) above 4 x the count: the shrink stamp kept within 20 of the
// clock; below 20 records, (clock - the grow stamp) / 10 spawns, at most
// (mood - 4 x count) / 4 + 1 and up to 20 records, then the grow stamp is the
// clock. Else: the grow stamp kept within 10; with mood not 0 the shrink stamp
// within 20; with mood 0 and above one record, (clock - the shrink stamp) / 20
// removals down to one record, then the shrink stamp is the clock.
extern "C" void __cdecl CommuSim_Population(unsigned count) {
    const auto mood = static_cast<signed char>(At(at::kMood)[0]);
    const auto n0 = static_cast<unsigned char>(count);
    U n = n0;
    const int m = mood;
    const int four = static_cast<int>(4 * n);
    if (m > four) {
        const U clock = UL(at::kClock);
        if (clock - UL(at::kStampShrink) > 0x14) SetUL(at::kStampShrink, clock - 0x14);
        if (n0 >= 0x14) return;
        const U grow = UL(at::kStampGrow);
        const int room = ((m - four) >> 2) + 1;
        int k = static_cast<int>((clock - grow) / 10u);
        if (k > room) k = room;
        if (k == 0) return;
        for (int j = 0; j < k && n != 0x14; ++j, ++n) SH_CALL(CommuSim_AddRecord)();
        SetUL(at::kStampGrow, UL(at::kClock));
        return;
    }
    const U clock = UL(at::kClock);
    const U grow = UL(at::kStampGrow);
    if (mood != 0) {
        if (clock - grow > 0xA) SetUL(at::kStampGrow, clock - 0xA);
        if (clock - UL(at::kStampShrink) > 0x14) SetUL(at::kStampShrink, clock - 0x14);
        return;
    }
    if (clock - grow > 0xA) SetUL(at::kStampGrow, clock - 0xA);
    if (n0 <= 1) return;
    const int k = static_cast<int>((clock - UL(at::kStampShrink)) / 20u);
    if (k == 0) return;
    for (int j = 0; j < k && n != 1; ++j, --n) SH_CALL(CommuSim_RemoveRecord)();
    SetUL(at::kStampShrink, UL(at::kClock));
}

// original 0x455700 (no PSX twin paired): mood. When (clock - the mood stamp)
// / 5 (its low 16 bits) is not 0: the records in use of kind 9 each add their
// trait +0x10 and 3 (a 16-bit sum), less twice the count; times that, plus the
// mood; below 0 the mood is 0, above 99 it is 99; the stamp is the clock.
extern "C" void __cdecl CommuSim_Mood(unsigned count) {
    const U clock = UL(at::kClock);
    const U steps = ((clock - UL(at::kStampMood)) / 5u) & 0xFFFF;
    if (steps == 0) return;
    U sum = 0;
    for (U i = 0; i < at::kRecordCount; ++i)
        if (Rec(i)[0] != 0 && Rec(i)[1] == 9) sum += Trait(i, 0x10) + 3u;
    const auto twice = static_cast<std::int16_t>(static_cast<unsigned short>((count & 0xFFu) * 2));
    const U v = static_cast<U>(static_cast<int>(static_cast<std::int16_t>(sum)) - twice) * steps +
                static_cast<U>(static_cast<int>(static_cast<signed char>(At(at::kMood)[0])));
    if (static_cast<std::int32_t>(v) < 0) {
        SetUL(at::kStampMood, clock);
        At(at::kMood)[0] = 0;
        return;
    }
    At(at::kMood)[0] = static_cast<unsigned char>(static_cast<std::int32_t>(v) > 0x63 ? 0x63 : v);
    SetUL(at::kStampMood, clock);
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
    if (At(at::kLevelLit)[0] >= 7) return;
    unsigned short sum;
    unsigned char count;
    LevelSums(true, sum, count);
    if (count == 0) return;
    auto v = static_cast<std::int16_t>(static_cast<unsigned short>(Word(At(at::kLevelLitCosts + 2u * At(at::kLevelLit)[0])) - sum));
    if (v <= 0) v = 1;
    const U last = UL(at::kStampLevelLit);
    if (UL(at::kClock) - last < static_cast<U>(static_cast<int>(v))) return;
    const unsigned char level = At(at::kLevelLit)[0];
    SetUL(at::kStampLevelLit, last + static_cast<U>(static_cast<int>(v)));
    const unsigned char n = At(at::kEventCount)[0];
    At(at::kLevelLit)[0] = static_cast<unsigned char>(level + 1);
    At(at::kEvents + 2u * n)[0] = 7;
    At(at::kEventCount)[0] = static_cast<unsigned char>(n + 1);
}

// original 0x455870 (no PSX twin paired): the dark level (0x9046CB, below 10):
// as CommuSim_LevelLit over kind-4 buildings whose +1 is clear, the cost 20
// less the sum (at least 1) from the dark stamp, and only when the lit level
// is at least 0x65291B[dark level]; event 6.
extern "C" void __cdecl CommuSim_LevelDark(void) {
    if (At(at::kLevelDark)[0] >= 0xA) return;
    unsigned short sum;
    unsigned char count;
    LevelSums(false, sum, count);
    if (count == 0) return;
    auto v = static_cast<std::int16_t>(static_cast<unsigned short>(0x14 - sum));
    if (v <= 0) v = 1;
    const U last = UL(at::kStampLevelDark);
    if (UL(at::kClock) - last < static_cast<U>(static_cast<int>(v))) return;
    const unsigned char level = At(at::kLevelDark)[0];
    if (At(at::kLevelLit)[0] < At(at::kLevelDarkNeeds + level)[0]) return;
    SetUL(at::kStampLevelDark, last + static_cast<U>(static_cast<int>(v)));
    const unsigned char n = At(at::kEventCount)[0];
    At(at::kLevelDark)[0] = static_cast<unsigned char>(level + 1);
    At(at::kEvents + 2u * n)[0] = 6;
    At(at::kEventCount)[0] = static_cast<unsigned char>(n + 1);
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
            SetUL(bl + 4, UL(at::kClock));
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
        const U q = (UL(at::kClock) - UL(bl + 4)) / static_cast<U>(static_cast<int>(d));
        const auto cap = static_cast<std::int16_t>(At(at::kLevelLit)[0] + 4);
        if (q == 0) continue;
        const unsigned char level = bl[3];
        if (static_cast<std::int16_t>(level) >= cap) continue;
        const auto raised = static_cast<unsigned char>(level + q);
        bl[3] = raised;
        if (static_cast<std::int16_t>(raised) > cap) bl[3] = static_cast<unsigned char>(cap);
        SetUL(bl + 4, UL(at::kClock));
        const unsigned char n = At(at::kEventCount)[0];
        At(at::kEvents + 2u * n)[0] = 0;
        const unsigned char first = FirstOf(b);
        const unsigned char n2 = At(at::kEventCount)[0];
        At(at::kEvents + 2u * n + 1)[0] = first;
        At(at::kEventCount)[0] = static_cast<unsigned char>(n2 + 1);
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
            if (UL(at::kClock) - UL(r + 4) < (level + 1u) * 6u) {
                r[2] = 1;
                continue;
            }
            if (r[2] > 1) continue;
            const auto chance = static_cast<unsigned char>((level * 2 + 1) * static_cast<unsigned char>(0xA - Trait(i, 0x10)));
            const int roll = SH_CALL(Rand)();
            if (chance > Percent(roll)) {
                const unsigned char inUse = CommuCount();
                if (inUse > 1) {
                    const unsigned char n = At(at::kRemovedCount)[0];
                    r[0] = 0;
                    At(at::kRemovedCount)[0] = static_cast<unsigned char>(n + 1);
                    At(at::kRemoved + n)[0] = static_cast<unsigned char>(i | 0x80);
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
                price = Word(At(at::kConsumables + 22u * item + 4));
                flags = At(at::kConsumables + 22u * item)[0];
                break;
            case 1:
                price = Word(At(at::kWeapons + 28u * item + 9));
                flags = At(at::kWeapons + 28u * item)[0];
                break;
            case 2:
                price = Word(At(at::kArmour + 26u * item + 7));
                flags = At(at::kArmour + 26u * item)[0];
                break;
            default:
                price = Word(At(at::kAccessories + 24u * item + 5));
                flags = At(at::kAccessories + 24u * item)[0];
                break;
            }
            U tier = 0;
            for (U p = at::kPriceTiers; price > Word(At(p));) {
                p += 6;
                ++tier;
                if (p >= at::kPriceTiersEnd) break;
            }
            const U stamp = UL(r + 4);
            if (UL(at::kClock) - stamp < Word(At(at::kPriceTiers + 6 * tier + 2))) continue;
            r[3] = static_cast<unsigned char>(status & 0xF);
            if (flags & 8) {
                r[3] = static_cast<unsigned char>((status & 0xF) | 0x30);
            } else {
                const unsigned short odds = Word(At(at::kPriceTiers + 6 * tier + 4));
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
            SetUL(bl + 4, UL(at::kClock));
            continue;
        }
        if (level >= At(at::kKindBCaps + (UL(at::kLevelLit) & 0xFF))[0]) continue;
        unsigned char count = 0;
        for (U i = 0; i < at::kRecordCount; ++i)
            if (Rec(i)[0] != 0 && Rec(i)[1] == b + 1) ++count;
        if (count == 0) continue;
        const U divisor = 6u - count;
        if (divisor == 0)
            bof3::Fatal("CommuSim_TickKindB: building %u has 6 residents - the original divides by 0 (docs/rest_4a.md section 7)", (unsigned)b);
        const U q = (UL(at::kClock) - UL(bl + 4)) / divisor;
        if ((q & 0xFFFF) == 0) continue;
        const auto raised = static_cast<unsigned char>(bl[3] + q);
        bl[3] = raised;
        const unsigned char cap = At(at::kKindBCaps + (UL(at::kLevelLit) & 0xFF))[0];
        if (raised > cap) bl[3] = cap;
        SetUL(bl + 4, UL(at::kClock));
        const unsigned char n = At(at::kEventCount)[0];
        At(at::kEvents + 2u * n)[0] = 1;
        const unsigned char first = FirstOf(b);
        const unsigned char n2 = At(at::kEventCount)[0];
        At(at::kEvents + 2u * n + 1)[0] = first;
        At(at::kEventCount)[0] = static_cast<unsigned char>(n2 + 1);
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
    SetUL(rec + 4, UL(at::kClock));
    rec[2] = 0;
    unsigned char* const copy = At(at::kRecordCopies + 5u * r);
    SetUL(copy, UL(at::kRecordTraits + at::kTraitStride * r));
    copy[4] = Trait(r, 4);
    const unsigned char n = At(at::kSpawnCount)[0];
    At(at::kSpawnCount)[0] = static_cast<unsigned char>(n + 1);
    At(at::kSpawned + n)[0] = r;
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
    const unsigned char n = At(at::kRemovedCount)[0];
    At(at::kRemovedCount)[0] = static_cast<unsigned char>(n + 1);
    At(at::kRemoved + n)[0] = r;
}

// original 0x456080 (no PSX twin paired; R4B's 0x456E20 calls it): the bytes
// 0x9046CD / 0x9046CE the sums of trait +0x11 over the records in use of kinds
// 0xA and 0xB.
extern "C" void __cdecl CommuSim_SumKindsAB(void) {
    At(at::kSumA)[0] = 0;
    unsigned char sumB = 0;
    At(at::kSumB)[0] = sumB;
    for (U i = 0; i < at::kRecordCount; ++i) {
        if (Rec(i)[0] == 0) continue;
        const unsigned char kind = Rec(i)[1];
        if (kind == 0xA) {
            At(at::kSumA)[0] = static_cast<unsigned char>(At(at::kSumA)[0] + Trait(i, 0x11));
        } else if (kind == 0xB) {
            sumB = static_cast<unsigned char>(sumB + Trait(i, 0x11));
            At(at::kSumB)[0] = sumB;
        }
    }
}

// original 0x4560D0 (no PSX twin paired; GameMode_Field and Shop_Close call
// it too): the nine words 0x675F60 cleared, then three groups of three
// distinct words 0x9B + n, n = Rand() & 0x1F brought to at most {15, 20,
// 25}[group] by subtracting Rand() & 0xF, never 0x9F or 0xA0.
extern "C" void __cdecl CommuSim_RollOffers(void) {
    static constexpr unsigned char kLimits[3] = {0xF, 0x14, 0x19};
    SetUL(at::kOfferWords, 0);
    SetUL(at::kOfferWords + 4, 0);
    SetUL(at::kOfferWords + 8, 0);
    SetUL(at::kOfferWords + 0xC, 0);
    SetWord(At(at::kOfferWords + 0x10), 0);
    for (U g = 0; g < 3; ++g) {
        const U group = at::kOfferWords + 6 * g;
        U filled = 0;
        do {
            auto n = static_cast<unsigned char>(SH_CALL(Rand)() & 0x1F);
            while (n > kLimits[g]) n = static_cast<unsigned char>(n - (SH_CALL(Rand)() & 0xF));
            const auto v = static_cast<unsigned short>(n + 0x9B);
            U k = 0;
            while (k < 3 && v != Word(At(group + 2 * k))) ++k;
            if (v != 0x9F && v != 0xA0 && k >= 3) {
                SetWord(At(at::kOfferWords + 2 * (filled + 3 * g)), v);
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
    SetUL(at::kResidentCounts, 0);
    SetUL(at::kResidentCounts + 4, 0);
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
        for (U o = object; o < 0x14; ++o) At(at::kSpriteBase + at::kSpriteStride * o)[0] = 0;
}

namespace {
// The part CommuSim_PlaceLoose and _PlaceResident end with: the animation bank
// by +5 % 6, then the facing +8 (Sprite_Current read again for each).
void BankAndFace() {
    const unsigned short bank = Word(At(at::kBanks + 2u * (Sprite_Current[5] % 6u)));
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
    unsigned char* const base = At(at::kSpriteBase + at::kSpriteStride * o);
    Sprite_Current = base;
    base[6] = 1;
    if (At(at::kObjectPlaces + 3 * o + 2)[0] != 0) {
        Sprite_Current[1] = 0;
        base[0x84] = 2;
        Sprite_Current[2] = 1;
    } else {
        Sprite_Current[1] = 6;
        base[0x84] = 0;
        Sprite_Current[2] = 0;
    }
    SetWord(Sprite_Current + 0x36, At(at::kObjectPlaces + 3 * o)[0]);
    SetWord(Sprite_Current + 0x34, 0);
    SetWord(Sprite_Current + 0x3A, At(at::kObjectPlaces + 3 * o + 1)[0]);
    SetWord(Sprite_Current + 0x38, 0);
    const long height = SH_CALL(AreaMap_Elevation)(Long(base + 0x34), Long(base + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(height));
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
    unsigned char* const base = At(at::kSpriteBase + at::kSpriteStride * o);
    Sprite_Current = base;
    Sprite_Current[5] = static_cast<unsigned char>(record);
    Sprite_Current[1] = 6;
    base[0x84] = 0;
    Sprite_Current[2] = 0;
    SetWord(Sprite_Current + 0x36, At(at::kResidentPlaces + 3 * (Count(building) + row))[0]);
    SetWord(Sprite_Current + 0x34, 0);
    SetWord(Sprite_Current + 0x3A, At(at::kResidentPlaces + 3 * (Count(building) + row) + 1)[0]);
    SetWord(Sprite_Current + 0x38, 0);
    unsigned char* const s = Sprite_Current;
    const long height = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(height));
    const unsigned char facing = At(at::kResidentPlaces + 3 * (Count(building) + row) + 2)[0];
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
    At(at::kResidentCounts + building)[0] = static_cast<unsigned char>(Count(building) + 1);
}

// --- the poses (each: the object's +0x88 word; +6 through Sprite_Current) ---

// original 0x456680 (no PSX twin paired): the object's +6 = 1 and its pose
// 0xC2 + a roll of 0..4 (Rand() & 7, above 4 less 4).
extern "C" void __cdecl CommuPose_Kind0(unsigned object) {
    auto roll = static_cast<unsigned char>(SH_CALL(Rand)() & 7);
    if (roll > 4) roll = static_cast<unsigned char>(roll + 0xFC);
    const U o = object & 0xFFu;
    At(at::kSpriteKind + at::kSpriteStride * o)[0] = 1;
    SetWord(Pose(o), roll + 0xC2u);
}

// original 0x4566C0 (no PSX twin paired): +6 = 1; 0xCC when the lit level is 7
// and the building's +1 set, or the dark level 10 and it clear; else 0xC8 +
// the building's +1.
extern "C" void __cdecl CommuPose_Kind4(unsigned object, unsigned building) {
    Sprite_Current[6] = 1;
    const unsigned char flag = Bld(building & 0xFFu)[1];
    const U o = object & 0xFFu;
    if ((At(at::kLevelLit)[0] == 7 && flag != 0) || (At(at::kLevelDark)[0] == 0xA && Bld(building & 0xFFu)[1] == 0))
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
    SetUL(Sprite_Current + 0x18, 0);
    const U v = Bld(b)[2] + 2u * Bld(b)[1] + 0x11;
    SetUL(Sprite_Current + 0x1C, v);
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
        SetUL(Sprite_Current + 0x18, 1);
        SetUL(Sprite_Current + 0x1C, 1);
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
    const U age = UL(at::kClock) - UL(Rec(r) + 4);
    const U row = age < 0x14 ? 0 : (age < 0x28 ? 1 : 2);
    const U index = Count(building & 0xFFu) + 3 * row;
    SetWord(Pose(object & 0xFFu), Word(At(at::kOfferWords + 2 * index)));
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
    SetUL(Sprite_Current + 0x18, 6);
    SetUL(Sprite_Current + 0x1C, 0);
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
        const unsigned char set = SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x92);
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
    At(at::kTailKind)[0] = kind;
    SetUL(at::kTriggerObject, static_cast<U>(reinterpret_cast<std::uintptr_t>(object)));
    SH_CALL(ScriptFlags_Set40)();
    return 0;
}
unsigned char TailTriggerSub(unsigned char* object, unsigned char kind, unsigned char sub) {
    At(at::kTailKind)[0] = kind;
    SetUL(at::kTriggerObject, static_cast<U>(reinterpret_cast<std::uintptr_t>(object)));
    At(at::kTailSub)[0] = sub;
    SH_CALL(ScriptFlags_Set40)();
    return 0;
}
char* Row(U a) { return reinterpret_cast<char*>(At(a)); }
const char* Format() { return reinterpret_cast<const char*>(At(at::kNumberFormat)); }
}  // namespace

// original 0x456B20 (Field_ObjectTriggers id 1; no PSX twin paired): tail kind
// 0xE for the object.
extern "C" unsigned char __cdecl FieldTrigger01(unsigned char* object, unsigned char*) { return TailTrigger(object, 0xE); }

// original 0x456B70 (id 2): four numbers into Text_Records rows 0..3 by the
// format Area08_MessageFormat - the bytes 0x9040C8 and 0x9040C9, the dwords
// 0x904134 (the community's clock) and 0x904138 - then Msg_OpenScript(0x55)
// and Field_Request 2.
extern "C" unsigned char __cdecl FieldTrigger02(unsigned char*, unsigned char*) {
    SH_CALL(Crt_sprintf)(Row(at::kTextRow0), Format(), static_cast<U>(At(at::kCountA)[0]));
    SH_CALL(Crt_sprintf)(Row(at::kTextRow1), Format(), static_cast<U>(At(at::kCountB)[0]));
    SH_CALL(Crt_sprintf)(Row(at::kTextRow2), Format(), UL(at::kClock));
    SH_CALL(Crt_sprintf)(Row(at::kTextRow3), Format(), UL(at::kCountC));
    SH_CALL(Msg_OpenScript)(0x55);
    Field_Request = 2;
    return 0;
}

// original 0x456BF0 (id 3): three numbers - the dwords 0x90413C, 0x904140,
// 0x904144 - into rows 0..2, Msg_OpenScript(0x56), Field_Request 2.
extern "C" unsigned char __cdecl FieldTrigger03(unsigned char*, unsigned char*) {
    SH_CALL(Crt_sprintf)(Row(at::kTextRow0), Format(), UL(at::kCountD));
    SH_CALL(Crt_sprintf)(Row(at::kTextRow1), Format(), UL(at::kCountE));
    SH_CALL(Crt_sprintf)(Row(at::kTextRow2), Format(), UL(at::kCountF));
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
    At(at::kShopRecords + at::kShopStride * shop)[0] = At(at::kBuildingLevels + 8u * building)[0];
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
    BOF3_INJECT(Battle_AutoTargetCheck);
    BOF3_INJECT(Battle_RandomLiveMember);
    BOF3_INJECT(Battle_RandomLiveEnemy);
    BOF3_INJECT(Battle_MemberActionIs0E);
    BOF3_INJECT(Battle_MemberAutoFixed);
    BOF3_INJECT(Battle_RandomOtherMember);
    BOF3_INJECT(Field_RunSlot);
    BOF3_INJECT(Field_SlotClutCopy);
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
