// The battle effect tasks: the three battle-task kinds BattleTask_RunAll
// dispatches by +6 - kind 0's nineteen effects (0x4352A0), kind 1's 110 magic
// effects (0x435350) and kind 2's magic rows (0x4378B0) - and, under kind 0,
// the damage-number popup and its five states (0x432B70..0x432F0B), the pose
// task (0x433190..0x4332F1), the actor watch (0x433460..0x433549) and the
// sprite that follows its owner (0x4337F0..0x433969); plus the two round
// hooks an enemy state stores at 0x904B64 / 0x904B68 (0x437720, 0x437750).
// docs/battle_fx_tasks.md.
//
// Every call goes through battle_fx_tasks::g (battle_fx_tasks_callees.h), so
// that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. Everything here is a faithful replacement.
//
// Two pointers are read throughout, and they are not the same read: the
// battle-task slot being run (0x93B8C4) and Sprite_Current (0x937F88).
// BattleTask_RunAll sets both to the slot, but each function reads the one
// the original reads, afresh wherever the original re-reads it.
#include "game/battle_fx_tasks.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_fx_tasks_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_fx_tasks {

using move_script::At;
using move_script::Long;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
Handler H(std::uint32_t address) { return Fn<Handler>(::bof3::CodePlace(address)); }
}  // namespace

const Callees kOriginals = {
    // Rebound 2026-09-28 (round eleven's cleanup, docs/round-11-cleanup.md item 2): the
    // entries the boss round made ours read their names (BareRet in both stack tables,
    // Boss26Fx_Dispatch - BattleFx_Dispatch's slot 8, group BSF's); the values are
    // unchanged. The rest keep the round-eight form, raw (the fuzz's kFxImm / kMagicImm
    // key on the same values). Rebound 2026-09-29 (round twelve group BE2): slots 9, 10,
    // 13, 14 and 17 and the watch's state 3 read BE2's names, the values unchanged.
    {H(bof3::addr::BareRet), H(bof3::addr::BattleFx_DamagePopup), H(bof3::addr::BattleFxFlash_Dispatch), H(bof3::addr::BattleFx_PoseTask), H(bof3::addr::BattleFxTint_Dispatch), H(bof3::addr::BattleFxAnim_Dispatch), H(bof3::addr::BattleFx_ActorWatch),
     H(bof3::addr::BattleFx_Follow), H(bof3::addr::Boss26Fx_Dispatch), H(bof3::addr::BattleFx_GridMark), H(bof3::addr::BattleFx_ListHand), H(bof3::addr::BattleFxReform_Dispatch), H(bof3::addr::BattleFxRestore_Dispatch), H(bof3::addr::BattleFx_Win18Cursor),
     H(bof3::addr::BattleFx_Win19Cursor), H(bof3::addr::BattleFxDash_Dispatch), H(bof3::addr::BattleFxPose_Dispatch), H(bof3::addr::BattleFx_RestoreMemberTask),
     H(bof3::addr::BattleFxTrail_Dispatch)},   // slots 15, 16, 18 (BE6) and 9, 10, 13, 14, 17 (BE2) rebound 2026-09-29, round twelve: the values unchanged
    {H(bof3::addr::BareRet), H(bof3::addr::SuperComboChild_Task), H(bof3::addr::BuffPopupAt_Task), H(bof3::addr::PentagramChild_Task), H(bof3::addr::PollenMote_Task), H(bof3::addr::BuffFx_Dispatch), H(bof3::addr::EvilEyeChild_Task), H(bof3::addr::Magic114_ChildTask),
     H(bof3::addr::BenedictionChild_Task), H(bof3::addr::BonebreakChild_Task), H(bof3::addr::FxFunnel_Dispatch), H(bof3::addr::Fx104_Child), H(bof3::addr::MyollnirChild_Task), H(bof3::addr::LeechOrb_Dispatch), H(bof3::addr::LightningBolt_Task), H(bof3::addr::WhelpBreathChild_Task),
     H(bof3::addr::SpellRagnarok_Child), H(bof3::addr::TsunamiChild_Task), H(bof3::addr::IdentifyChild_Task), H(bof3::addr::ChillChild_Task), H(bof3::addr::JoltBolt_Task), H(bof3::addr::FrostRing_Task), H(bof3::addr::FxRing_Task), H(bof3::addr::IceShard_Task),
     H(bof3::addr::IntimidateChild_Task), H(bof3::addr::SpellSleep_Child), H(bof3::addr::SpellConfuse_Child), H(bof3::addr::FxSpiral_Dispatch), H(bof3::addr::ForetellChild_Task), H(bof3::addr::BarrierPart_Dispatch), H(bof3::addr::DreamBreathMote_Task), H(bof3::addr::Magic008Double_Task),
     H(bof3::addr::Magic088_Child), H(bof3::addr::Magic017Image_Task), H(bof3::addr::ShieldAura_Dispatch), H(bof3::addr::KickImage_Task), H(bof3::addr::Magic074_ActorDispatch), H(bof3::addr::PurifyMote_Dispatch), H(bof3::addr::Magic073_ActorDispatch), H(bof3::addr::Magic087_Child),
     H(bof3::addr::Magic092_Child), H(bof3::addr::SnapWave_Task), H(bof3::addr::ChlorineChild_Task), H(bof3::addr::DrainOrb_Dispatch), H(bof3::addr::HowlingChild_Task), H(bof3::addr::SimoonFx_Dispatch), H(bof3::addr::Magic020Child_Task), H(bof3::addr::BurnFlame_Task),
     H(bof3::addr::Magic115_ChildTask), H(bof3::addr::BoneDartChild_Task), H(bof3::addr::ElemBreathChild_Task), H(bof3::addr::Fx105_Child), H(bof3::addr::EbonfireRing_Task), H(bof3::addr::ThunderClapBolt_Task), H(bof3::addr::WallOfFireChild_Task), H(bof3::addr::BlitzBolt_Task),
     H(bof3::addr::BreathBeam_Task), H(bof3::addr::InfluenceChild_Task), H(bof3::addr::WeretigerChild_Task), H(bof3::addr::FocusChild_Task), H(bof3::addr::EnlightenChild_Task), H(bof3::addr::BerserkChild_Task), H(bof3::addr::CounterChild_Task), H(bof3::addr::SanctuaryRing_Dispatch),
     H(bof3::addr::TornadoFunnel_Dispatch), H(bof3::addr::BoneDanceChild_Task), H(bof3::addr::RottenBreathChild_Task), H(bof3::addr::FxDim_Dispatch), H(bof3::addr::AccessionChild_Task), H(bof3::addr::StealClone_Dispatch), H(bof3::addr::ElemStrikeChild_Task), H(bof3::addr::LastResortChild_Task),
     H(bof3::addr::BuffPopup_Task), H(bof3::addr::NueStompChild_Task), H(bof3::addr::CelerityChild_Task), H(bof3::addr::CoronaChild_Task), H(bof3::addr::BlizzardShard_Task), H(bof3::addr::FxScorch_Dispatch), H(bof3::addr::ShadowBreathFx_Task), H(bof3::addr::JumpChild_Task),
     H(bof3::addr::OvumChild_Task), H(bof3::addr::MagicBallChild_Task), H(bof3::addr::MightyChopChild_Task), H(bof3::addr::MindSwordChild_Task), H(bof3::addr::TimedBlowCopy_Task), H(bof3::addr::SacrificeChild_Task), H(bof3::addr::MainCannonChild_Task), H(bof3::addr::Magic111Child_Task),
     H(bof3::addr::Magic159Child_Task), H(bof3::addr::WarShoutChild_Task), H(bof3::addr::DragonBreathBeam_Task), H(bof3::addr::Port_DroppedCall), H(bof3::addr::DivineBreathFx_Task), H(bof3::addr::HeadCrackerRock_Task), H(bof3::addr::LavaburstChild_Task), H(bof3::addr::MeteorStrikeRock_Task),
     H(bof3::addr::CombustionChild_Task), H(bof3::addr::GeoBreathChild_Task), H(bof3::addr::ReviveHalo_Dispatch), H(bof3::addr::TempestFlash_Task), H(bof3::addr::DoomBreathOrb_Task), H(bof3::addr::Magic225Veil_Task), H(bof3::addr::VenomChild_Task), H(bof3::addr::AuraBreathDome_Task),
     H(bof3::addr::SuddenDeathChild_Task), H(bof3::addr::KaiserChild_Task), H(bof3::addr::HolocaustBeam_Task), H(bof3::addr::Magic213Actor_Task), H(bof3::addr::InkPuff_Task), H(bof3::addr::InkInkActor_Task)},
    {H(bof3::addr::BattleFx_PopupStart), H(bof3::addr::BattleFx_PopupHold), H(bof3::addr::BattleFx_PopupRise), H(bof3::addr::BattleFx_PopupBounce), H(bof3::addr::BattleFx_PopupEnd)},
    {H(bof3::addr::BattleFx_PoseStart), H(bof3::addr::BattleFx_PosePlay)},
    {H(bof3::addr::BattleFx_ActorWatchTest), H(bof3::addr::BattleFx_WatchIconStart), H(bof3::addr::Sprite_StateRestart), H(bof3::addr::BattleFx_WatchIcon), H(bof3::addr::BattleFx_WatchRecheck)},
    {H(bof3::addr::BattleFx_FollowOwner)},
    BattleFx_RollingDigits, Battle_DrawNumber, Battle_DrawLabel, BattleTask_FreeCurrent, Battle_ActorIsOut,
    Sprite_SetAnimation, Sprite_QueueOverlay, Sprite_ScriptTick, Sprite_UpdateScreen, ScriptFlags_Set40,
    Fn<void (__cdecl*)()>(::bof3::Code(kAfterAreaScript.v)), Transition_Start,
};
Callees g = kOriginals;

}  // namespace battle_fx_tasks

using namespace battle_fx_tasks;

namespace {

// A pointer the original keeps in a dword, read afresh.
unsigned char* PtrAtBattleFxTasks(std::uint32_t address) { return battle_fx_tasks::At(static_cast<std::uint32_t>(battle_fx_tasks::Long(battle_fx_tasks::At(address)))); }
// The battle-task slot being run (0x93B8C4), and a slot's owner (+0x80).
unsigned char* Cur() { return rv::Bytes(BattleFlow_TaskCurrent.Native()); }
// Sprite_Current, read afresh at every use.
unsigned char* Sc() { return Sprite_Current; }
// The running slot is a battle task; Sprite_Current is the slot too while
// BattleTask_RunAll runs it (docs/battle_fx_tasks.md 1).

void CallIndex(const battle_fx_tasks::Handler* table, unsigned n, unsigned index, const char* who) {
    if (index >= n) bof3::Fatal("%s: index %u, past the %u-entry table the original builds on its stack", who, index, n);
    table[index]();
}

}  // namespace

// ===========================================================================
// The three kinds
// ===========================================================================

// ===========================================================================
// Kind 0, effect 1: the damage-number popup
// ===========================================================================

// ===========================================================================
// Kind 0, effect 3: the pose task
// ===========================================================================

// ===========================================================================
// Kind 0, effect 6: the actor watch
// ===========================================================================

// ===========================================================================
// Kind 0, effect 7: the follower
// ===========================================================================

// ===========================================================================
// The round hooks
// ===========================================================================

// ===========================================================================

void BattleFxTasks_Inject() {
    if (bof3::WantsShadow("battle_fx_tasks")) battle_fx_tasks::SelfTest();
}
