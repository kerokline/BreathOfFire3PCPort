// Damage, effects and affinities: group BE of the seventh round, eighteen
// functions of the battle engine (PC 0x42E400..0x4551A0, the PSX's
// BATTLE.EMI overlays compiled into the exe) that the owner's combat route
// reaches. docs/battle_damage.md has the formulas in words.
//
//   - The damage chain: Battle_ApplyDamage 0x445A30 (PSX 0x801DBB40) takes
//     Battle_CalcDamage 0x445CF0 (0x801DC00C) of the attacker and target, which
//     takes Battle_BaseDamage 0x4462B0 (0x801DCAA0), which takes
//     Battle_ScaleDamage 0x446430 (0x801DCD18), which takes
//     Battle_ElementAffinity 0x44EE80 (0x8009FA78).
//   - The effects: Effect_ApplyResult 0x44B9F0 (0x8009A160) calls one of 130
//     handlers and applies the HP and AP deltas they leave; two amounts the
//     handlers compute, Effect_SkillDamage 0x44ED10 (0x8009F820) and
//     Effect_HealAmount 0x44F130 (0x800A0080).
//   - The turn order: Battle_BuildEntryOrder 0x444F40 (0x801DA7A4),
//     Battle_BuildTurnOrder 0x4450E0 (0x801DAAB4), Battle_LevelClass 0x445640
//     (0x801DB3FC), Battle_ClearCommands 0x445680 (0x801DB45C),
//     Battle_DefaultTarget 0x445730 (0x801DB594), Battle_ActorCanCommand
//     0x4458B0 (0x801DB80C), Battle_ActorCanAct 0x445980 (0x801DB9AC),
//     Battle_RemoveFromTurnOrder 0x446650 (0x801DD114).
//   - The enemies' AI conditions: EnemyAI_ChooseActions 0x44AE90 (0x80098F8C)
//     and EnemyAI_RowDone 0x44B2C0 (0x80099830).
//
// Every call goes through battle_damage::g (battle_damage_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. The effect handlers are called through the table
// in memory, as the original calls them; the fuzz swaps the table.
//
// Several originals write into their own argument slots (0x4458B0, 0x445980,
// 0x445730, 0x446650, 0x445CF0, 0x44F130) - as scratch, never read back by a
// caller (every caller pops its arguments or pushes fresh ones: the E8 scan
// in docs/battle_damage.md section 1). Not kept.
#include "game/battle_damage.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_damage_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).
// Rebound 2026-10-04 (round fourteen group R3D, docs/rest_3d.md section 8): 0x44FA70, 0x44F1D0 and 0x44F030 likewise.

namespace battle_damage {

namespace {
template <class To> To Fn(std::uint32_t address) { return reinterpret_cast<To>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Battle_ActorCanAct,
    Battle_ActorCanCommand,
    Battle_LevelClass,
    Battle_CalcDamage,
    Battle_BaseDamage,
    Battle_ScaleDamage,
    Battle_ElementAffinity,
    EnemyAI_RowDone,
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_ActorIsOut)),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned)>(::bof3::Code(bof3::addr::BattleBanner_Add)),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_StatusResisted80)),
    Fn<void (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_InflictStatus)),
    Fn<void (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_ClearStatus)),
    Fn<int (__cdecl*)(int, unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_HitOrMissParty)),
    Fn<int (__cdecl*)(int, unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_HitOrMissEnemy)),
    Fn<int (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_PartyDefenceMean)),
    Fn<int (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_PsiStatusDeathAffinity)),
    Rand,
    Fn<void (__cdecl*)(unsigned char*, const unsigned char*)>(::bof3::Code(bof3::addr::EnemyAI_ApplyAction)),
    Fn<void (__cdecl*)(unsigned char*, unsigned, unsigned)>(::bof3::Code(bof3::addr::EnemyAI_SetRowDone)),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::EnemyAI_OtherRowsDone)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::EnemyAI_DedupMessages)),
};
Callees g = kOriginals;

}  // namespace battle_damage

using namespace battle_damage;

namespace {

using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

// An actor byte: 0..2 the party, 3 and up the enemies. Both address the
// working record by the byte alone, as the original does (`and eax, 0xFF`).
unsigned char* Party(unsigned actor) { return move_script::At(::bof3::Img(ObjTrio_addr) + 0x10 + (actor & 0xFF) * battle_damage::at::kPartyStride); }
unsigned char* Enemy(unsigned actor) { return move_script::At(::bof3::Img(Battle_Enemies_addr) + 0x80 + ((actor & 0xFF) - 3) * battle_damage::at::kEnemyStride); }
// Party(a) is +0x10 into the member record (0x802D50), Enemy(a) +0x80 into the
// enemy object (0x93B9E0): their typed views start at the records'
// starts, rv::As<records::PartyMember>(Party(a) - 0x10) and
// rv::As<records::BattleEnemy>(Enemy(a) + at::kEObject).
unsigned char Byte(std::uint32_t address) { return move_script::At(address)[0]; }
std::int32_t S32(std::uint32_t v) { return static_cast<std::int32_t>(v); }
std::uint32_t U32(std::int32_t v) { return static_cast<std::uint32_t>(v); }
short S16(unsigned v) { return static_cast<short>(static_cast<unsigned short>(v)); }
// 32-bit products wrap, as imul's low half does.
std::int32_t Mul(std::int32_t a, std::int32_t b) { return S32(U32(a) * U32(b)); }

}  // namespace

// ===========================================================================
// The turn order

// ===========================================================================
// The damage chain

// ===========================================================================
// The effects

// ===========================================================================
// The enemies' AI conditions

// ===========================================================================

void BattleDamage_Inject() {
    if (bof3::WantsShadow("battle_damage")) battle_damage::SelfTest();
}
