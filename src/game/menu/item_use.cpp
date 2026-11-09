// Moved here by tools/move_funcs.py from item_use.cpp (docs/source-layout-round.md).
#include "game/menu/item_use.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/item_use.h"
#include "game/item_use_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace item_use;

namespace item_use {

using move_script::At;

using move_script::SetWord;

}  // namespace item_use

namespace {

// The record a (id, battle) pair names, as every function here computes it:
// the battle flag's low byte picks the persistent record or the member's
// working copy, the id's low byte indexes it, unchecked.
unsigned char* CharRecord(unsigned id, unsigned battle) {
    const unsigned i = id & 0xFF;
    return (battle & 0xFF) == 0 ? item_use::At(::bof3::Img(CharacterRecords_addr) + i * at::kCharStride) : item_use::At(::bof3::Img(at::kWorkRecords.v) + i * at::kWorkStride);
}

// Either answer is a records::CharRecord: the persistent record 0x903A70 + i
// * 0xA4, or the working copy 0x802DC0 + i * 0x14C, which is PartyMember
// +0x80, a copy of the character record (records.h CharRecord, PartyMember;
// docs/item-use.md section 2). The fields here are the record's +0x10,
// +0x18, +0x1A, +0x1E, +0x20, +0x22 (item_use_callees.h at::kStatus..kMaxAp).
records::CharRecord* CharRec(unsigned id, unsigned battle) { return rv::As<records::CharRecord>(CharRecord(id, battle)); }

// The handlers' common tail: `neg al; sbb eax, eax; and al, 0xFD; add eax, 3`
// - 0 when the callee answered anything in al, 3 when it answered 0.
unsigned char UsedOr3(unsigned char answer) { return answer != 0 ? 0 : 3; }

// Party member i's record index: 0x66972C[0x904062[i]] (the party list's
// entry, then its record).
unsigned Member(unsigned char i) { return Character_IndexById[Cond_Flags[0xd2 + i]]; }

// The party walk of 0x496D50 / 0x497410 / 0x497490 / 0x497510: Party_Count(0)
// asked before the first member and again after each, compared as bytes. The
// member's index is stored into the low byte of the id argument's own slot
// and that dword pushed, so the upper bytes are the caller's: `id` is that
// slot here.
template <typename Each> unsigned char PartyWalk(unsigned id, Each each) {
    unsigned char result = 0;
    unsigned char i = 0;
    if (static_cast<unsigned char>(g.party_count(0)) != 0) {
        do {
            id = (id & 0xFFFFFF00u) | Member(i);
            result = static_cast<unsigned char>(result | each(id));
            ++i;
        } while (i < static_cast<unsigned char>(g.party_count(0)));
    }
    return result;
}

// The stat-up tail of 0x497040..0x4972E0: 0 if either call answered non-zero
// in al, else 4. The field menu treats 0 and 4 alike (item used).
unsigned char UsedOr4(unsigned char first, unsigned char second) { return (first != 0 || second != 0) ? 0 : 4; }

unsigned char ClampedUp(unsigned id, unsigned battle, unsigned base_at) {
    unsigned char* const r = CharRecord(id, battle);
    const auto base = static_cast<unsigned char>(g.add_clamped(reinterpret_cast<unsigned short*>(r + base_at), 1));
    const auto eff = static_cast<unsigned char>(g.add_clamped(reinterpret_cast<unsigned short*>(r + base_at - 0x20), 1));
    return UsedOr4(base, eff);
}

}  // namespace

// original 0x496CC0 (PSX 0x801E7E34) [0]: no field effect.
extern "C" unsigned char __cdecl ItemUse_None(unsigned, unsigned) { return 2; }

// original 0x496CD0 (PSX 0x801E7E3C) [1], 0x496CF0 [2], 0x496D10 [3], 0x496D30 [4] (0 is a
// full heal), 0x4973D0 [23], 0x4973F0 [25], 0x4975D0 [31]: Char_HealHp by a
// constant.
extern "C" unsigned char __cdecl ItemUse_HealHp20(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 0x14, battle)); }

extern "C" unsigned char __cdecl ItemUse_HealHp40(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 0x28, battle)); }

extern "C" unsigned char __cdecl ItemUse_HealHp100(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 0x64, battle)); }

extern "C" unsigned char __cdecl ItemUse_HealHpFull(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 0, battle)); }

// original 0x496D50 (PSX 0x801E7F0C) [5], 0x497410 [26], 0x497490 [27]: Char_HealHp for each
// party member; 0x497510 [28]: Char_ClearStatus(0x80) for each.
extern "C" unsigned char __cdecl ItemUse_PartyHealHp100(unsigned id, unsigned battle) {
    return UsedOr3(PartyWalk(id, [battle](unsigned m) { return g.heal_hp(m, 0x64, battle); }));
}

// original 0x496DD0 (PSX 0x801E7FA0) [6], 0x496DF0 [7], 0x497590 [29], 0x4975B0 [24, 30]:
// Char_HealAp by a constant.
extern "C" unsigned char __cdecl ItemUse_HealAp20(unsigned id, unsigned battle) { return UsedOr3(g.heal_ap(id, 0x14, battle)); }

extern "C" unsigned char __cdecl ItemUse_HealAp100(unsigned id, unsigned battle) { return UsedOr3(g.heal_ap(id, 0x64, battle)); }

// original 0x496E10 (PSX 0x801E8008) [8]: 5 HP; then, with a bit of 0xA0 in the status byte
// (read after the heal), the item counts as used and a 30 % roll clears 0xA0.
// As the original has it: the roll is Rand() % 100 by signed division, and
// "used" does not depend on it.
extern "C" unsigned char __cdecl ItemUse_HealHp5Cure(unsigned id, unsigned battle) {
    unsigned char r = g.heal_hp(id, 5, battle);
    if (CharRecord(id, battle)[at::kStatus] & 0xA0) {
        r = static_cast<unsigned char>(r | 1);
        if (g.rand() % 100 < 30) g.clear_status(id, 0xA0, battle);
    }
    return UsedOr3(r);
}

// original 0x496EA0 (PSX 0x801E8120) [9], 0x496ED0 [10], 0x496EF0 [11], 0x496F20 [12]:
// Char_ClearStatus by a constant mask.
extern "C" unsigned char __cdecl ItemUse_Cure80(unsigned id, unsigned battle) { return UsedOr3(g.clear_status(id, 0x80, battle)); }

extern "C" unsigned char __cdecl ItemUse_Cure08(unsigned id, unsigned battle) { return UsedOr3(g.clear_status(id, 8, battle)); }

extern "C" unsigned char __cdecl ItemUse_Cure100(unsigned id, unsigned battle) { return UsedOr3(g.clear_status(id, 0x100, battle)); }

extern "C" unsigned char __cdecl ItemUse_CureA0(unsigned id, unsigned battle) { return UsedOr3(g.clear_status(id, 0xA0, battle)); }

// original 0x496F50 (PSX 0x801E81F0) [13]: HP 1, then Char_ClearStatus(0x4000). As the
// original has it (and the PSX): the HP is set before the status is tested,
// so on a member without 0x4000 the HP is 1 and the answer is 3 - D21, kept.
extern "C" unsigned char __cdecl ItemUse_Revive(unsigned id, unsigned battle) {
    rv::Store<unsigned>(CharRec(id, battle)->hp, 1);
    return UsedOr3(g.clear_status(id, 0x4000, battle));
}

// original 0x496FB0 (PSX 0x801E8278) [14]: for each member Char_ClearStatus(0xA0) then a full
// Char_HealHp, every answer ORed.
extern "C" unsigned char __cdecl ItemUse_PartyRestore(unsigned id, unsigned battle) {
    return UsedOr3(PartyWalk(id, [battle](unsigned m) {
        const unsigned char cured = g.clear_status(m, 0xA0, battle);
        return static_cast<unsigned char>(cured | g.heal_hp(m, 0, battle));
    }));
}

// original 0x497040 (PSX 0x801E832C) [15], 0x4970B0 [16]: Stat_AddCap999 on a base stat and its
// effective copy 0x20 below.
extern "C" unsigned char __cdecl ItemUse_MaxHpUp(unsigned id, unsigned battle) {
    unsigned char* const r = CharRecord(id, battle);
    const unsigned char base = g.add_cap999(reinterpret_cast<unsigned short*>(r + 0x40), 1);
    return UsedOr4(base, g.add_cap999(reinterpret_cast<unsigned short*>(r + 0x20), 1));
}

extern "C" unsigned char __cdecl ItemUse_MaxApUp(unsigned id, unsigned battle) {
    unsigned char* const r = CharRecord(id, battle);
    const unsigned char base = g.add_cap999(reinterpret_cast<unsigned short*>(r + 0x42), 1);
    return UsedOr4(base, g.add_cap999(reinterpret_cast<unsigned short*>(r + 0x22), 1));
}

extern "C" unsigned char __cdecl ItemUse_AtkUp(unsigned id, unsigned battle) { return ClampedUp(id, battle, 0x44); }

extern "C" unsigned char __cdecl ItemUse_DefUp(unsigned id, unsigned battle) { return ClampedUp(id, battle, 0x46); }

extern "C" unsigned char __cdecl ItemUse_AgiUp(unsigned id, unsigned battle) { return ClampedUp(id, battle, 0x48); }

extern "C" unsigned char __cdecl ItemUse_IntUp(unsigned id, unsigned battle) { return ClampedUp(id, battle, 0x4A); }

// original 0x4972E0 (PSX 0x801E86D4) [21]: Stat_AddCap99 on the byte +0x4E and its copy +0x2E.
extern "C" unsigned char __cdecl ItemUse_Stat2EUp(unsigned id, unsigned battle) {
    unsigned char* const r = CharRecord(id, battle);
    const unsigned char base = g.add_cap99(r + 0x4E, 1);
    return UsedOr4(base, g.add_cap99(r + 0x2E, 1));
}

// original 0x497350 (PSX 0x801E8770) [22]: 3 at full HP; else the max-HP scale byte + 1 while
// below 9, the stats recomputed and HP filled to the new max; at 9, HP filled
// if below max, else 3 (HP above max).
extern "C" unsigned char __cdecl ItemUse_HpScaleUp(unsigned id, unsigned battle) {
    unsigned char* const r = CharRecord(id, battle);
    auto* const c = rv::As<records::CharRecord>(r);
    const std::uint16_t hp = c->hp, max = c->stats.max_hp;
    if (hp == max) return 3;
    const unsigned char scale = c->hp_scale;
    if (scale < 9) {
        c->hp_scale = static_cast<unsigned char>(scale + 1);
        g.recalc(r);
        rv::Store<unsigned>(c->hp, c->stats.max_hp);
        return 0;
    }
    if (hp < max) {
        rv::Store<unsigned>(c->hp, max);
        return 0;
    }
    return 3;
}

extern "C" unsigned char __cdecl ItemUse_HealHp1(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 1, battle)); }

extern "C" unsigned char __cdecl ItemUse_HealHp80(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 0x50, battle)); }

extern "C" unsigned char __cdecl ItemUse_PartyHealHp80(unsigned id, unsigned battle) {
    return UsedOr3(PartyWalk(id, [battle](unsigned m) { return g.heal_hp(m, 0x50, battle); }));
}

extern "C" unsigned char __cdecl ItemUse_PartyHealHp240(unsigned id, unsigned battle) {
    return UsedOr3(PartyWalk(id, [battle](unsigned m) { return g.heal_hp(m, 0xF0, battle); }));
}

extern "C" unsigned char __cdecl ItemUse_PartyCure80(unsigned id, unsigned battle) {
    return UsedOr3(PartyWalk(id, [battle](unsigned m) { return g.clear_status(m, 0x80, battle); }));
}

extern "C" unsigned char __cdecl ItemUse_HealAp40(unsigned id, unsigned battle) { return UsedOr3(g.heal_ap(id, 0x28, battle)); }

extern "C" unsigned char __cdecl ItemUse_HealAp5(unsigned id, unsigned battle) { return UsedOr3(g.heal_ap(id, 5, battle)); }

extern "C" unsigned char __cdecl ItemUse_HealHp5(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 5, battle)); }

// original 0x4975F0 (PSX 0x801E8B20) [32]: gives the Faerie Tiara (0x57) back, moves the
// menu on, counts the clear story flags from 0xFF down to the first set one
// (at most ten) off 0xB9, and keeps the leader's position and the area. The
// arguments are not read; Inventory_Add gets a fourth 0 it does not read.
extern "C" unsigned char __cdecl ItemUse_FaerieTiara(unsigned, unsigned) {
    g.inventory_add(0, 0x57, 1, 0);
    Game_Step = static_cast<unsigned short>(Game_Step + 1);
    FieldS_State = 0xA;
    ModeRest_AreaAsked = 0xB9;
    for (int flag = 0xFF;;) {
        if (g.flags_test(At(::bof3::Img(Cond_Flags_addr) + 0xa0), static_cast<unsigned>(flag)) != 0) break;
        const auto count = static_cast<unsigned char>(ModeRest_AreaAsked - 1);
        --flag;
        ModeRest_AreaAsked = count;
        if (flag < 0xF6) break;
    }
    std::memcpy(At(::bof3::Img(AreaW2d_ReturnPoint_addr)), At(::bof3::Img(ObjTrio_addr) + 0x34), 8);
    SetWord(At(::bof3::Img(AreaW2d_ReturnPoint_addr) + 8), Game_AreaNumber);
    return 0;
}

// original 0x497670 (PSX 0x801E8C08) [33].
extern "C" unsigned char __cdecl ItemUse_WaterJug(unsigned, unsigned) {
    Cond_Flags[0xcf] = 0xF0;
    return 0;
}

// original 0x497680 (PSX 0x801E8C1C): 2 for an item whose flags byte is 0,
// else its handler's answer. The table is read from memory, as the original
// does; both arguments reach the handler as the caller pushed them. The call
// is not a tail call, as it is not in the original (the handler's caller is
// this function, for the call trace).
extern "C" __attribute__((disable_tail_calls)) unsigned char __cdecl ItemUse_Dispatch(unsigned id, unsigned item, unsigned battle) {
    const unsigned it = item & 0xFF;
    if (At(::bof3::Img(at::kItemFlags.v))[it * at::kItemStride] == 0) return 2;
    const auto handler = reinterpret_cast<Handler>(ItemUse_Handlers[ItemUse_HandlerIndex[it]].Native());
    const unsigned char answer = handler(id, battle);
    return answer;
}

void MenuItemUse_Inject() {
    BOF3_INJECT(ItemUse_None);
    BOF3_INJECT(ItemUse_HealHp20);
    BOF3_INJECT(ItemUse_HealHp40);
    BOF3_INJECT(ItemUse_HealHp100);
    BOF3_INJECT(ItemUse_HealHpFull);
    BOF3_INJECT(ItemUse_PartyHealHp100);
    BOF3_INJECT(ItemUse_HealAp20);
    BOF3_INJECT(ItemUse_HealAp100);
    BOF3_INJECT(ItemUse_HealHp5Cure);
    BOF3_INJECT(ItemUse_Cure80);
    BOF3_INJECT(ItemUse_Cure08);
    BOF3_INJECT(ItemUse_Cure100);
    BOF3_INJECT(ItemUse_CureA0);
    BOF3_INJECT(ItemUse_Revive);
    BOF3_INJECT(ItemUse_PartyRestore);
    BOF3_INJECT(ItemUse_MaxHpUp);
    BOF3_INJECT(ItemUse_MaxApUp);
    BOF3_INJECT(ItemUse_AtkUp);
    BOF3_INJECT(ItemUse_DefUp);
    BOF3_INJECT(ItemUse_AgiUp);
    BOF3_INJECT(ItemUse_IntUp);
    BOF3_INJECT(ItemUse_Stat2EUp);
    BOF3_INJECT(ItemUse_HpScaleUp);
    BOF3_INJECT(ItemUse_HealHp1);
    BOF3_INJECT(ItemUse_HealHp80);
    BOF3_INJECT(ItemUse_PartyHealHp80);
    BOF3_INJECT(ItemUse_PartyHealHp240);
    BOF3_INJECT(ItemUse_PartyCure80);
    BOF3_INJECT(ItemUse_HealAp40);
    BOF3_INJECT(ItemUse_HealAp5);
    BOF3_INJECT(ItemUse_HealHp5);
    BOF3_INJECT(ItemUse_FaerieTiara);
    BOF3_INJECT(ItemUse_WaterJug);
    BOF3_INJECT(ItemUse_Dispatch);
}
