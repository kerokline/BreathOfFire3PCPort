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
using move_script::Word;

U UL(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
U UL(U a) { return UL(move_script::At(a)); }
void SetUL(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }
void SetUL(U a, U v) { SetUL(move_script::At(a), v); }
unsigned char* Ptr(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

// The community's records and buildings, and the traits table by record.
unsigned char* Rec(U i) { return At(::bof3::Img(Community_Records_addr) + 8u * i); }

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
    const U flags = UL(::bof3::Img(at::kBattleFlags.v));
    if (!(flags & 0x4000)) return 1;
    const unsigned char out = SH_CALL(Battle_ActorIsOut)((flags & 0xFFFFFF00u) | BattlePhases_B8B);
    if (out) return 1;
    const unsigned char forced = BattlePhases_B8B;
    const auto a = static_cast<unsigned char>(actor);
    if (forced == a) return 1;
    if (a < 3) {
        move_script::At(::bof3::Img(at::kTarget.v))[0] = forced;
        return 0;
    }
    const U e = at::kEnemyStride * (static_cast<U>(a) - 3);
    if (move_script::Word(move_script::At(::bof3::Img(at::kEnemyIntel.v) + e)) > 1 && !(move_script::At(::bof3::Img(Battle_Enemies_addr) + 0x92 + e)[0] & 0x20) && !(UL(::bof3::Img(at::kEnemyFlags.v) + e) & 0x4000))
        return 1;
    if (move_script::At(::bof3::Img(at::kAutoMode.v))[0] != 4) {
        move_script::At(::bof3::Img(at::kTarget.v))[0] = forced;
        return 0;
    }
    const unsigned char bits = move_script::At(::bof3::Img(at::kAbilityFlags.v) + at::kAbilityStride * move_script::Word(move_script::At(::bof3::Img(Battle_Enemies_addr) + 0x106 + e)))[0];
    if (bits & 0x10) {
        if (bits & 0x80) move_script::At(::bof3::Img(at::kTarget.v))[0] = 0xC0;
        else move_script::At(::bof3::Img(at::kTarget.v))[0] = forced >= 3 ? 0x80 : 0x40;
        return 1;
    }
    if (!(bits & 0x40)) {
        move_script::At(::bof3::Img(at::kTarget.v))[0] = a;
        return 1;
    }
    move_script::At(::bof3::Img(at::kTarget.v))[0] = forced;
    return 0;
}

// original 0x452EB0 (PSX 0x800A6408, call-anchored; the two twins were cited swapped until 2026-10-23): of members 0..2, those
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

// original 0x452F10 (PSX 0x800A64BC, call-anchored; the two twins were cited swapped until 2026-10-23): of enemies 3..10, those
// Battle_ActorIsOut rules in, the one at Rand() % their count - in a list of
// four bytes. The original keeps the list at esp + 4 of its frame, the count
// in the dword after it and the loop byte in the dword after that, so a fifth
// enemy standing writes the count byte with the actor (the count becomes the
// actor's number) and later ones the count's upper bytes and the loop dword's;
// Rand() % that count then reads any of the twelve bytes. Ours keeps the same
// twelve bytes, so every pick the original can make is ours too - but the
// upper bytes of the count and loop dwords (frame bytes 5..7 and 9..11), which
// the original never writes before it may read them, are 0 here, where the
// original has whatever the stack held (DIVERGENCE DIV-0074; docs/rest_4a.md
// section 7, L1). With four or fewer enemies standing nothing differs.
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
    const unsigned short w = move_script::Word(move_script::At(::bof3::Img(ObjTrio_addr) + 0x126 + at::kMemberStride * (member & 0xFFu)));
    if (w == 0xE) return 1;
    return w == 0x128 ? 1 : 0;
}

// original 0x454290 (PSX 0x800A84A0, Weretiger's target roll): a member's fixed auto-target. The
// member's odds byte +0x142 (signed, at most 5) against Rand() % 20: below it,
// and 0x904AB1 not 1, Battle_RandomOtherMember(member); else
// Battle_DefaultTarget((Rand() & 7) + 3). Then +0x125 = 1, 0x904B35 = 1 and
// the target 0x904B44 the answer.
extern "C" void __cdecl Battle_MemberAutoFixed(unsigned member) {
    const U m = member & 0xFFu;
    auto odds = static_cast<signed char>(move_script::At(::bof3::Img(ObjTrio_addr) + 0x142 + at::kMemberStride * m)[0]);
    if (odds > 5) odds = 5;
    const int r = SH_CALL(Rand)();
    const int roll = r % 20;
    unsigned char target;
    if (roll < odds && Battle_PartyStanding != 1) {
        target = SH_CALL(Battle_RandomOtherMember)(member);
    } else {
        const int r2 = SH_CALL(Rand)();
        target = SH_CALL(Battle_DefaultTarget)((static_cast<U>(r2) & 0xFFFFFF00u) | static_cast<unsigned char>((r2 & 7) + 3));
    }
    move_script::At(::bof3::Img(ObjTrio_addr) + 0x125 + at::kMemberStride * m)[0] = 1;
    move_script::At(::bof3::Img(at::kAutoMode.v))[0] = 1;
    move_script::At(::bof3::Img(at::kTarget.v))[0] = target;
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
    const int divisor = move_script::At(::bof3::Img(at::kSlotDivisors.v) + kind)[0];
    if (divisor == 0) bof3::Fatal("Field_SlotClutCopy: object kind %u has no frames - the original divides by 0", (unsigned)kind);
    auto row = static_cast<unsigned char>(frame / divisor);
    const auto offset = static_cast<unsigned char>((frame % divisor) * move_script::At(::bof3::Img(at::kSlotSteps.v) + kind)[0]);
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

// --- the poses (each: the object's +0x88 word; +6 through Sprite_Current) ---

// ===========================================================================
// Field_ObjectTriggers entries (object, 0x904030) -> al 0
// ===========================================================================

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
}
