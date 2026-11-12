// Moved here by tools/move_funcs.py from rest_2d.cpp (docs/source-layout-round.md).
#include "game/field/field_ability.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field/field_idle.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2d.h"
#include "game/rest_2d_callees.h"
#include "game/scenario_harness.h"
#include "game/yes_no_layout.h"
#include "hook/behaviour.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_2d::at;

}  // namespace

namespace {

using U = std::uint32_t;

// move_script::At is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::Long;

using move_script::SetWord;

using move_script::Word;

// A record the original writes by an index it never bounds. Past the array it
// writes into the next block; no path in play gives such an index (section 5),
// so ours aborts there.
U RecordChecked(U base, U stride, unsigned count, unsigned index, const char* array, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: %s index %u is past its %u records; the original writes at 0x%X", who, array, index, count,
                    static_cast<unsigned>(::bof3::VaOf(base + index * stride)));
    return base + index * stride;
}

// Field_Members' byte of the party slot `slot` (ObjTrio + 0x148, 0x14C apart).
unsigned char MemberAt(unsigned slot) { return (rv::Bytes(ObjTrio) + 0x148)[slot * rest_2d::at::kObjStride]; }

U RecordOf(unsigned member) { return ::bof3::Img(CharacterRecords_addr) + member * rest_2d::at::kRecordStride; }

// The handlers' answer: 1 when the callee did something (al not 0), else 4.
unsigned Done(unsigned char al) { return al != 0 ? 1u : 4u; }

// (the caster's word +0x2A + 100) * k / 100, as the original's signed division.
unsigned Amount(unsigned caster, int k) {
    const int stat = Word(move_script::At(RecordOf(caster & 0xFF) + 0x2A));
    return static_cast<unsigned>((stat + 100) * k / 100);
}

}  // namespace

// DIVERGENCE DIV-0208: 1 through PatchBytes("FieldHealParty") in Rest2D_Inject,
// after the self-test. The walk then reads the party list, as the item heals
// do, not the rows the world map leaves holding Ryu alone in row 0 (D408).
namespace rest_2d { std::uint8_t g_field_heal_party = 0; }

namespace {

unsigned HealAll(unsigned caster, unsigned battle, int k) {
    unsigned char any = 0;
    for (unsigned char i = 0; i < static_cast<unsigned char>(SH_CALL(Party_Count)(0));) {
        const unsigned amount = Amount(caster, k);
        const unsigned char member = rest_2d::g_field_heal_party ? Character_IndexById[Cond_Flags[0xd2 + i]] : MemberAt(i);
        any = static_cast<unsigned char>(any | SH_CALL(Char_HealHp)(member, amount, battle));
        ++i;
    }
    return Done(any);
}

}  // namespace

// original 0x58A0E0 (3 bytes), FieldAbility_Effects[0]: al 3.
extern "C" unsigned char __cdecl FieldAbility_NotHere(unsigned, unsigned, unsigned) { return 3; }

// original 0x58A0F0 (0x50 bytes), [1]: Char_HealHp(target, (stat + 100) * 20 / 100, battle).
extern "C" unsigned __cdecl FieldAbility_HealOne20(unsigned caster, unsigned target, unsigned battle) {
    return Done(SH_CALL(Char_HealHp)(target, Amount(caster, 20), battle));
}

// original 0x58A140 (0x50 bytes), [2]: the same at 40.
extern "C" unsigned __cdecl FieldAbility_HealOne40(unsigned caster, unsigned target, unsigned battle) {
    return Done(SH_CALL(Char_HealHp)(target, Amount(caster, 40), battle));
}

// original 0x58A190 (0x1E bytes), [3]: Char_HealHp(target, 0, battle) - 0 heals to the full.
extern "C" unsigned __cdecl FieldAbility_HealOneFull(unsigned, unsigned target, unsigned battle) {
    return Done(SH_CALL(Char_HealHp)(target, 0, battle));
}

// original 0x58A1B0 (0xA9 bytes), [4]: each party member (Party_Count(0) asked
// each pass), Char_HealHp(member, (stat + 100) * 40 / 100, battle); 1 when any took.
extern "C" unsigned __cdecl FieldAbility_HealAll40(unsigned caster, unsigned, unsigned battle) {
    return HealAll(caster, battle, 40);
}

// original 0x58A260 (0xAC bytes), [5]: the same at 120.
extern "C" unsigned __cdecl FieldAbility_HealAll120(unsigned caster, unsigned, unsigned battle) {
    return HealAll(caster, battle, 120);
}

// original 0x58A310 (0x21 bytes), [6]: Char_ClearStatus(target, 0x80, battle).
extern "C" unsigned __cdecl FieldAbility_Clear80(unsigned, unsigned target, unsigned battle) {
    return Done(SH_CALL(Char_ClearStatus)(target, 0x80, battle));
}

// original 0x58A340 (3 bytes), [7]: al 4. FieldAbility_Use gives it ability
// 0x50 alone.
//
// DIV-0218 SmokeSkill. The Definitive Edition romhack (evidence: the patched
// US disc, START.EMI 8 = STATUS.EMI 0, this handler's twin 0x801E7C70 made a
// jump to GAME.EMI's new 0x801AE2B4): the handler sets the encounter
// counter to 128 (field_idle.h) and answers 1, so FieldAbility_Use takes the
// AP cost and the menu plays its "used" sound. Field_EncounterDue spends it.
extern "C" unsigned char __cdecl FieldAbility_NoEffect(unsigned, unsigned, unsigned) {
    if (bof3::BehaviourOn(bof3::behaviours::Id::SmokeSkill)) {
        field_idle::g_smoke_steps = 0x80;
        return 1;
    }
    return 4;
}

// original 0x58A350 (0x21 bytes), [8]: Char_ClearStatus(target, 0xA0, battle).
extern "C" unsigned __cdecl FieldAbility_ClearA0(unsigned, unsigned target, unsigned battle) {
    return Done(SH_CALL(Char_ClearStatus)(target, 0xA0, battle));
}

// original 0x58A380 (0x34 bytes), [9]: Char_HealHp(target, 0, battle), then
// Char_ClearStatus(target, 0xA0, battle); 1 when either took.
extern "C" unsigned __cdecl FieldAbility_HealFullClearA0(unsigned, unsigned target, unsigned battle) {
    const unsigned char healed = SH_CALL(Char_HealHp)(target, 0, battle);
    const unsigned char cleared = SH_CALL(Char_ClearStatus)(target, 0xA0, battle);
    return Done(static_cast<unsigned char>(healed | cleared));
}

// original 0x58A3C0 (0xF7 bytes): an ability used from the field menu. The
// caster's record - the working record 0x802DC0 + 0x14C * caster when
// `battle`'s low byte is set, else CharacterRecords - and its AP word +0x1A
// below Skill_ApCost(caster, ability, 0): al 0. Else the handler of
// FieldAbility_Effects by the ability byte (0x46..0x4B and 0xAE..0xB3 the
// handlers 1..6, 0x50 7, 0x73 and 0xD7 8, 0xA8 9, any other 0) called (caster,
// target, battle); its answer 1 or 5 takes the cost (asked again) off the AP
// word. Answers the handler's al.
extern "C" unsigned char __cdecl FieldAbility_Use(unsigned caster, unsigned target, unsigned ability, unsigned battle) {
    const unsigned c = caster & 0xFF;
    const bool working = (battle & 0xFF) != 0;
    const U record = working ? ::bof3::Img(rest_2d::at::kWorking.v) + c * rest_2d::at::kObjStride : RecordOf(c);
    const unsigned char cost = SH_CALL(Skill_ApCost)(caster, ability, 0);
    if (Word(move_script::At(record + 0x1A)) < cost) return 0;
    const auto b = static_cast<unsigned char>(ability);
    unsigned index;
    if (b >= 0x46 && b <= 0x4B) index = b - 0x45u;
    else if (b >= 0xAE && b <= 0xB3) index = static_cast<unsigned char>(b + 0x53);
    else if (b == 0x73 || b == 0xD7) index = 8;
    else if (b == 0x50) index = 7;
    else index = b == 0xA8 ? 9 : 0;
    using Effect = unsigned (__cdecl*)(unsigned, unsigned, unsigned);
    unsigned char answer;
    if (index == 0 && b == 0x6F && bof3::BehaviourOn(bof3::behaviours::Id::TownWarp)) {
        // DIV-0225 TownWarp. The Definitive Edition romhack (evidence: the
        // patched US disc, START.EMI 8's handler table 0x801ED224 slot 0 made
        // GAME.EMI's 0x8019C304; its dispatcher 0x801E7D20 passes the ability
        // in s0) answers ability 0x6F there: on the world map, "asked" and 1,
        // so the AP cost is taken; else 3, handler 0's own answer.
        answer = field_idle::TownWarpUse();
    } else {
        const auto handler = reinterpret_cast<Effect>(static_cast<std::uintptr_t>(static_cast<U>(Long(move_script::At(::bof3::Img(FieldAbility_Effects_addr) + 4 * index)))));
        answer = static_cast<unsigned char>(handler(caster, target, battle));
    }
    if (answer == 1 || answer == 5) {
        const unsigned char again = SH_CALL(Skill_ApCost)(caster, ability, 0);
        const U checked = working ? RecordChecked(::bof3::Img(rest_2d::at::kWorking.v), rest_2d::at::kObjStride, 3, c, "the working records", "FieldAbility_Use")
                                  : RecordChecked(::bof3::Img(CharacterRecords_addr), rest_2d::at::kRecordStride, 8, c, "CharacterRecords", "FieldAbility_Use");
        SetWord(move_script::At(checked + 0x1A), Word(move_script::At(checked + 0x1A)) - again);
    }
    return answer;
}

void FieldAbility_Inject() {
    BOF3_INJECT(FieldAbility_NotHere);
    BOF3_INJECT(FieldAbility_HealOne20);
    BOF3_INJECT(FieldAbility_HealOne40);
    BOF3_INJECT(FieldAbility_HealOneFull);
    BOF3_INJECT(FieldAbility_HealAll40);
    BOF3_INJECT(FieldAbility_HealAll120);
    BOF3_INJECT(FieldAbility_Clear80);
    BOF3_INJECT(FieldAbility_NoEffect);
    BOF3_INJECT(FieldAbility_ClearA0);
    BOF3_INJECT(FieldAbility_HealFullClearA0);
    BOF3_INJECT(FieldAbility_Use);
    {
        // DIV-0208: after the self-test, which compared Capcom's walk.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("FieldHealParty", Low32(&rest_2d::g_field_heal_party), &was, &is, 1);
        bof3::Log("DIV-0208    a field heal for the whole party walks the party list (on unless the line above says OFF)");
    }
}
