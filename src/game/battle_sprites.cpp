// Group BG of the seventh round (docs/battle_sprites.md): 32 functions of the
// battle engine and two small helpers, each faithful to Capcom's PC code but
// one: since DIV-0185 Battle_SetHitPopup reads a null target block as bit 1
// clear instead of faulting.
//
//   0x452BF0 Battle_RollPendingFlag     0x453FA0 Battle_MemberAutoTarget
//   0x452F70 Battle_SetTargetFlags      0x454220 Battle_MemberOutAction
//   0x4530D0 Battle_SetTargetFlag40     0x454380 Battle_PlayHitSound
//   0x453190 Battle_AnyFlagF0           0x454410 Battle_SetHitPopup
//   0x453210 Battle_ActionSuitsTarget   0x454CC0 Sprite_SetTint
//   0x4532A0 Battle_ItemSuitsTarget     0x454D60 Tint_Release
//   0x4537A0 Battle_PickFlag8Member     0x454DF0 ClutMap_Mark
//   0x453A10 Battle_MemberCoinFlip      0x454F30 ClutMap_FindFree
//   0x453B10 Battle_SettleFlag8         0x455140 ClutMap_FindOwner
//   0x453C00 Formation_ApplyStatMods    0x4551A0 Sprite_SetClutStp
//   0x453DA0 Battle_SetDamagePopup
//   0x494280 Battle_InitEncounterKind   0x494A80 Battle_OpenEnemyNames
//   0x4942A0 Battle_InitBossEncounter   0x494EA0 Battle_EnemyKindFlag
//   0x4942C0 Battle_InitEnemies         0x494ED0 Battle_SetEnemyKindFlag
//   0x494320 Battle_SetupEnemy          0x494F00 Battle_SetEnemyOffset
//   0x4946C0 Battle_CopyEnemyData
//   0x588F00 Sprite_UpdateScreenSlot    0x587900 Sound_PlayById
//
// Actors: 0..2 are the party records at ObjTrio (0x802D40, 0x14C bytes each),
// 3..10 the enemy records at 0x93B960 (0x128 bytes each, index actor - 3);
// an enemy record's first 0x80 bytes are its sprite object, the battle data
// (docs/kinship-probe-battle-engine.md's EnemyWorkingRecords) starts at +0x80.
// The party's flags dword is +0x130, the enemies' +0x110.
//
// Every index is unchecked, as the originals have it; every call out goes
// through battle_sprites::g (battle_sprites_callees.h).
#include "game/battle_sprites.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_sprites_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/sound_sp.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

// The group's functions, declared here for the pointers below; symbols.toml
// binds each name to this file (bof3/symbols.gen.h).
namespace battle_sprites {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

template <class F> F Raw(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }

const Callees kOriginals = {
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_ActorIsOut)),
    Rand,
    Raw<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_SetActorBit)),
    Raw<unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(::bof3::Code(bof3::addr::AbilityList_Add)),
    Raw<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Item_UseFlags)),   // group TWO's (2026-10-06): by address, the value unchanged
    Raw<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::BattleTask_Create)),
    Stat_AddClamped,
    Stat_AddCap100,
    Raw<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Window_Alloc)),
    Sound_PlayEffect,
    AreaMap_Elevation,
    Sprite_SetAnimationBank,
    Sprite_UpdateScreen,
    // rebound 2026-09-29 (round twelve, BE6): the values unchanged, the fuzz keys on them
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_MemberRollByAction)),
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_ActionBitSet)),
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_MemberListFull)),
    Battle_MemberCoinFlip,
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_AutoTargetCheck)),   // R4A's (round 14): by address, the value unchanged
    Raw<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_MemberAutoFixed)),            // R4A's
    Raw<void (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_PickEnemyTarget)),
    Battle_MemberOutAction,
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_MemberActionIs0E)),   // R4A's
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_RandomOtherMember)),  // R4A's
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_RandomEnemy)),
    Raw<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_DefaultTarget)),
    ClutMap_Mark,
    ClutMap_FindFree,
    ClutMap_FindOwner,
    Raw<void (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_PlaceBossActors)),   // R3G's: by address, as the other groups' above
    Battle_InitBossEncounter,
    Battle_InitEnemies,
    Battle_SetupEnemy,
    Battle_CopyEnemyData,
    Battle_SetEnemyOffset,
    Battle_EnemyKindFlag,
};
Callees g = kOriginals;

namespace {

unsigned char* Enemy(std::uint32_t i) { return At(::bof3::Img(Battle_Enemies_addr) + i * battle_sprites::at::kEnemyStride); }
unsigned char* Ptr(const unsigned char* at) { return At(static_cast<std::uint32_t>(Long(at))); }
std::int32_t Address(const void* p) { return static_cast<std::int32_t>(Low32(p)); }
void OrLong(unsigned char* at, std::uint32_t bits) { SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) | bits)); }
unsigned char* EnemyData(std::uint32_t id) { return At(::bof3::Img(battle_sprites::at::kEnemyData.v) + id * battle_sprites::at::kEnemyDataStride); }

}  // namespace
}  // namespace battle_sprites

using namespace battle_sprites;

//
    unsigned char* const s = Sprite_Current;

// original 0x588F00 (0x8014D154; PSX 0x8014D154): Sprite_Current +0x29 = Draw_OtSlot, then
// (a tail jump) Sprite_UpdateScreen.
extern "C" void __cdecl Sprite_UpdateScreenSlot(void) {
    Sprite_Current[0x29] = Draw_OtSlot;
    battle_sprites::g.update_screen();
}

// original 0x587900 (PSX 0x8015E210): Sound_PlayEffect(id). Ops BC, BD and D1 push more
// arguments; only the first is read.
//
// The PlayStation has three here (US 0x8015D71C / 0x8015D7C8 / 0x8015D820;
// JP 0x8015E10C / 0x8015E1B8 / 0x8015E210); the port folded them into one.
// Every caller but four is the first's (a jal scan of the US disc: 503 calls
// in 176 overlays; 0x8015D7C8 only from op BC and SCENA01's three, 0x8015D820
// only from ops BD and D1), and those four are ours and call sound_sp's
// level plays under DIV-0172 (move_groups.cpp, scena_sc1.cpp).
// DIVERGENCE DIV-0171: with the switch, the first's play: the cue follows
// Sprite_Current (sound_sp.h).
extern "C" void __cdecl Sound_PlayById(unsigned short id) {
    if (sound_sp::g_position) {
        sound_sp::PlayPositional(id);
        return;
    }
    battle_sprites::g.play_effect(id);
}

void BattleSprites_Inject() {
    if (bof3::WantsShadow("battle_sprites")) battle_sprites::SelfTest();
    BOF3_INJECT(Sprite_UpdateScreenSlot);
    BOF3_INJECT(Sound_PlayById);
    // DIV-0187 writes the enemy's +0x8F here; Capcom's Battle_SetupEnemy
    // would write the kind's bit and the cheat would be silently half off.
    if (Cheats_EnemyInfoShown() && !bof3::IsEnabled(bof3::addr::Battle_SetupEnemy))
        bof3::Fatal("BOF3X_ENEMY_INFO=1 needs Battle_SetupEnemy ours, and it is disabled");
}
