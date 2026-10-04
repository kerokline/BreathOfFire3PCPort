// BOF3X_SHADOW=rest_3a: group R3A's 45 functions through the boss harness
// (boss_harness.h, docs/boss_harness.md section 10), once at start-up: two
// boss_harness::Runs - the engine frame's (Group::engine) for the 36 in the
// battle engine's runs (at::kEngineBands), and a boss frame's (no engine) for
// the nine below them: area 33's three world-map states 0x404180..0x404220
// and BATE's six 0x42D710..0x42D796, which the engine group would refuse as
// lying outside the runs (a check the harness keeps; it is not this group's
// to widen). docs/rest_3a.md section 4.
//
// The clone rows are tools/band_rows.py's (--group R3A --clones, 2026-10-04),
// each read against the disassembly; the tool's extents for 0x404180 (0x58,
// over 0x4041B0's body) are cut at its tail jmp. BOF3X_R3A_ONLY=<hex address>
// runs that one clone (the controls script's shortcut); unset, all 45 run.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/rest_3a.h"
#include "game/rest_3a_callees.h"
#include "hook/log.h"

namespace rest_3a {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::SetLong;
using move_script::SetWord;
using S = bh::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define BH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BH_FN(name) reinterpret_cast<const void*>(&::name)

bh::Clone C(const char* name, U base, U size, const bh::CallSite* calls, int n, const bh::Imm* imms, int n_imms, const void* ours,
            S shape, U ret = 0, std::uint8_t state_at = 1, std::uint8_t states = 0, U state_cell = 0, bool calm = false) {
    bh::Clone c{name, base, size, calls, n, imms, n_imms, nullptr, 0, ours, ret, calm, shape};
    c.state_at = state_at;
    c.states = states;
    c.state_cell = state_cell;
    return c;
}

// ===========================================================================
// The clone rows (tools/band_rows.py --group R3A --clones)
// ===========================================================================

// --- outside the engine's runs (the boss frame) ---
constexpr bh::CallSite kCalls404180[] = {{0x1C, 0x4041B0}};   // the tail jmp to state 2
constexpr bh::CallSite kCalls4041B0[] = {{0x1F, 0x404390}};
constexpr bh::CallSite kCalls4041E0[] = {{0x38, 0x404390}};
constexpr bh::CallSite kCalls42D750[] = {{0x0, 0x59E330}};
constexpr bh::CallSite kCalls42D780[] = {{0x2, 0x495040}};

const bh::Clone kOutside[] = {
    C("WorldMap33_FrameSlideIn", 0x404180, 0x21, kCalls404180, BH_N(kCalls404180), nullptr, 0, BH_FN(WorldMap33_FrameSlideIn),
      S::kState),
    C("WorldMap33_FrameShown", 0x4041B0, 0x28, kCalls4041B0, BH_N(kCalls4041B0), nullptr, 0, BH_FN(WorldMap33_FrameShown), S::kState),
    C("WorldMap33_FrameSlideOut", 0x4041E0, 0x41, kCalls4041E0, BH_N(kCalls4041E0), nullptr, 0, BH_FN(WorldMap33_FrameSlideOut),
      S::kState),
    C("BattleExtra_Dispatch", 0x42D710, 0x11, nullptr, 0, nullptr, 0, BH_FN(BattleExtra_Dispatch), S::kDispatch, 0, 1, 4, at::kMode),
    C("BattleExtra_Start", 0x42D730, 0x19, nullptr, 0, nullptr, 0, BH_FN(BattleExtra_Start), S::kState),
    C("BattleExtra_Leave", 0x42D750, 0xD, kCalls42D750, BH_N(kCalls42D750), nullptr, 0, BH_FN(BattleExtra_Leave), S::kState),
    C("BattleExtra_TallyDispatch", 0x42D760, 0xE, nullptr, 0, nullptr, 0, BH_FN(BattleExtra_TallyDispatch), S::kDispatch, 0, 1, 3,
      at::kModeStep),
    C("BattleExtra_TallyOpenDispatch", 0x42D770, 0xE, nullptr, 0, nullptr, 0, BH_FN(BattleExtra_TallyOpenDispatch), S::kDispatch, 0, 1,
      2, at::kModeSub),
    C("BattleExtra_OpenTransition", 0x42D780, 0x17, kCalls42D780, BH_N(kCalls42D780), nullptr, 0, BH_FN(BattleExtra_OpenTransition),
      S::kState),
};

// --- the battle engine's runs (the engine frame) ---
constexpr bh::CallSite kCalls42E250[] = {{0x5E, 0x591B60}, {0x71, 0x590BB0}, {0x8D, 0x590660}};
constexpr bh::CallSite kCalls42E2F0[] = {{0x18, 0x5917A0}};
constexpr bh::CallSite kCalls431550[] = {{0xB, 0x587B40}, {0x17, 0x587AE0}, {0x1F, 0x444310}, {0x26, 0x497740}, {0x39, 0x44A6E0}};
constexpr bh::CallSite kCalls4315A0[] = {{0x0, 0x454810}};
constexpr bh::CallSite kCalls431710[] = {{0x0, 0x454810}, {0x15, 0x454770}};
constexpr bh::CallSite kCalls431740[] = {{0x0, 0x454810}};
constexpr bh::Imm kImms432F90[] = {{0xF, 0x432FC0}, {0x17, 0x433020}, {0x22, 0x4330E0}};
constexpr bh::CallSite kCalls432FC0[] = {{0x20, 0x44A650}};
constexpr bh::CallSite kCalls433020[] = {{0x1D, 0x5A79A0}, {0x36, 0x5A77C0}, {0x3F, 0x461E50}, {0x78, 0x4449E0}};
constexpr bh::CallSite kCalls4330E0[] = {{0x1D, 0x5A79A0}, {0x36, 0x5A77C0}, {0x3F, 0x461E50}, {0x78, 0x4449E0}, {0xA8, 0x4351F0}};
constexpr bh::Imm kImms4332B0[] = {{0xF, 0x4332E0}, {0x17, 0x433300}, {0x22, 0x433350}};
constexpr bh::CallSite kCalls433300[] = {{0x1D, 0x4446E0}};
constexpr bh::CallSite kCalls433350[] = {{0x1D, 0x4446E0}};
constexpr bh::CallSite kCalls433380[] = {{0x35, 0x5890E0}};
constexpr bh::Imm kImms433380[] = {{0xF, 0x4333C0}, {0x17, 0x433410}, {0x22, 0x433430}};
constexpr bh::CallSite kCalls4333C0[] = {{0x2, 0x589590}, {0x3C, 0x5891F0}};
constexpr bh::CallSite kCalls433410[] = {{0x0, 0x5893A0}};
constexpr bh::CallSite kCalls433430[] = {{0x0, 0x589410}, {0x25, 0x4351F0}};
constexpr bh::CallSite kCalls433550[] = {{0xD, 0x434870}, {0xD2, 0x434730}, {0xDA, 0x5891F0}};
constexpr bh::Imm kImms433970[] = {{0xF, 0x4339B0}, {0x17, 0x4339D0}, {0x22, 0x433A00}, {0x2A, 0x433A50}, {0x32, 0x433B20}};
constexpr bh::CallSite kCalls433A00[] = {{0x39, 0x454590}};
constexpr bh::CallSite kCalls433A50[] = {{0x0, 0x454810},  {0x41, 0x5891F0}, {0x62, 0x454DC0}, {0x7E, 0x5366A0},
                                         {0xA0, 0x446BB0}, {0xA8, 0x4551A0}, {0xB9, 0x442310}};
constexpr bh::CallSite kCalls433B20[] = {{0x4F, 0x4351F0}};
constexpr bh::Imm kImms433B80[] = {{0xF, 0x433BC0}, {0x17, 0x4339D0}, {0x22, 0x433C00}, {0x2A, 0x433DA0}, {0x32, 0x4346C0}};
constexpr bh::CallSite kCalls433C00[] = {{0xE, 0x433D60}, {0x40, 0x454590}, {0x6E, 0x454590}, {0x9C, 0x454590}, {0x146, 0x454590}};
constexpr bh::Imm kImms4357D0[] = {{0xF, 0x437CC0},  {0x17, 0x499EB0}, {0x22, 0x43CA20}, {0x2A, 0x43EB80},
                                   {0x32, 0x43FAC0}, {0x3A, 0x440830}, {0x42, 0x43D6D0}, {0x4A, 0x43F5F0}};
constexpr bh::CallSite kCalls435A20[] = {{0x36, 0x4358D0}};
constexpr bh::CallSite kCalls435A70[] = {{0x2A, 0x4358D0}};

const bh::Clone kEngine[] = {
    // BATE's equipment screen's helpers (EquipRefresh calm: the engine
    // disturbance's random window byte would put window 1's cursor past the six
    // bytes, where ours aborts and the original writes .data outside the state)
    C("BattleExtra_EquipSetupWindows", 0x42E0E0, 0x16A, nullptr, 0, nullptr, 0, BH_FN(BattleExtra_EquipSetupWindows), S::kHelper),
    C("BattleExtra_EquipCommit", 0x42E250, 0x9D, kCalls42E250, BH_N(kCalls42E250), nullptr, 0, BH_FN(BattleExtra_EquipCommit),
      S::kHelper),
    C("BattleExtra_EquipRefresh", 0x42E2F0, 0x72, kCalls42E2F0, BH_N(kCalls42E2F0), nullptr, 0, BH_FN(BattleExtra_EquipRefresh),
      S::kHelper, 0xFF, 1, 0, 0, true),
    // the battle's end
    C("BattleEnd_LossDispatch", 0x431540, 0xE, nullptr, 0, nullptr, 0, BH_FN(BattleEnd_LossDispatch), S::kDispatch, 0, 1, 3,
      at::kSubStep),
    C("BattleEnd_LossBanner", 0x431550, 0x4E, kCalls431550, BH_N(kCalls431550), nullptr, 0, BH_FN(BattleEnd_LossBanner), S::kStep),
    C("BattleEnd_LossAwaitLoad", 0x4315A0, 0x10, kCalls4315A0, BH_N(kCalls4315A0), nullptr, 0, BH_FN(BattleEnd_LossAwaitLoad), S::kStep),
    C("BattleEnd_RestoreDispatch", 0x4315B0, 0xE, nullptr, 0, nullptr, 0, BH_FN(BattleEnd_RestoreDispatch), S::kDispatch, 0, 1, 5,
      at::kSubStep),
    C("BattleEnd_RestoreLoadBank", 0x431710, 0x2A, kCalls431710, BH_N(kCalls431710), nullptr, 0, BH_FN(BattleEnd_RestoreLoadBank),
      S::kStep),
    C("BattleEnd_RestoreAwaitBank", 0x431740, 0x18, kCalls431740, BH_N(kCalls431740), nullptr, 0, BH_FN(BattleEnd_RestoreAwaitBank),
      S::kStep),
    C("BattleEnd_ExitAwaitFade", 0x4318F0, 0x1E, nullptr, 0, nullptr, 0, BH_FN(BattleEnd_ExitAwaitFade), S::kStep),
    // kind 0, slot 2
    C("BattleFxFlash_Dispatch", 0x432F90, 0x2E, nullptr, 0, kImms432F90, BH_N(kImms432F90), BH_FN(BattleFxFlash_Dispatch), S::kTask, 0,
      1, 3),
    C("BattleFxFlash_Banner", 0x432FC0, 0x5B, kCalls432FC0, BH_N(kCalls432FC0), nullptr, 0, BH_FN(BattleFxFlash_Banner), S::kTask),
    C("BattleFxFlash_Rise", 0x433020, 0xBB, kCalls433020, BH_N(kCalls433020), nullptr, 0, BH_FN(BattleFxFlash_Rise), S::kTask),
    C("BattleFxFlash_Fall", 0x4330E0, 0xAD, kCalls4330E0, BH_N(kCalls4330E0), nullptr, 0, BH_FN(BattleFxFlash_Fall), S::kTask),
    // slot 4
    C("BattleFxTint_Dispatch", 0x4332B0, 0x2E, nullptr, 0, kImms4332B0, BH_N(kImms4332B0), BH_FN(BattleFxTint_Dispatch), S::kTask, 0, 1,
      3),
    C("BattleFxTint_Brighten", 0x433300, 0x41, kCalls433300, BH_N(kCalls433300), nullptr, 0, BH_FN(BattleFxTint_Brighten), S::kTask),
    C("BattleFxTint_Hold", 0x433350, 0x26, kCalls433350, BH_N(kCalls433350), nullptr, 0, BH_FN(BattleFxTint_Hold), S::kTask),
    // slot 5
    C("BattleFxAnim_Dispatch", 0x433380, 0x3E, kCalls433380, BH_N(kCalls433380), kImms433380, BH_N(kImms433380),
      BH_FN(BattleFxAnim_Dispatch), S::kTask, 0, 1, 3),
    C("BattleFxAnim_Start", 0x4333C0, 0x4D, kCalls4333C0, BH_N(kCalls4333C0), nullptr, 0, BH_FN(BattleFxAnim_Start), S::kTask),
    C("BattleFxAnim_Run", 0x433410, 0x20, kCalls433410, BH_N(kCalls433410), nullptr, 0, BH_FN(BattleFxAnim_Run), S::kTask),
    C("BattleFxAnim_Linger", 0x433430, 0x2B, kCalls433430, BH_N(kCalls433430), nullptr, 0, BH_FN(BattleFxAnim_Linger), S::kTask),
    // the actor watch's states 1 and 4
    C("BattleFx_WatchIconStart", 0x433550, 0xEF, kCalls433550, BH_N(kCalls433550), nullptr, 0, BH_FN(BattleFx_WatchIconStart),
      S::kTask),
    C("BattleFx_WatchRecheck", 0x433790, 0x5A, nullptr, 0, nullptr, 0, BH_FN(BattleFx_WatchRecheck), S::kTask),
    // slots 11 and 12
    C("BattleFxReform_Dispatch", 0x433970, 0x3E, nullptr, 0, kImms433970, BH_N(kImms433970), BH_FN(BattleFxReform_Dispatch), S::kTask,
      0, 1, 5),
    C("BattleFxReform_Begin", 0x4339B0, 0x12, nullptr, 0, nullptr, 0, BH_FN(BattleFxReform_Begin), S::kTask),
    C("BattleFxReform_Shrink", 0x4339D0, 0x26, nullptr, 0, nullptr, 0, BH_FN(BattleFxReform_Shrink), S::kTask),
    C("BattleFxReform_LoadDat", 0x433A00, 0x4A, kCalls433A00, BH_N(kCalls433A00), nullptr, 0, BH_FN(BattleFxReform_LoadDat), S::kTask),
    // calm: the engine disturbance re-points the owner 0x93B940 during File_LoadDone, to a record whose +5 is any
    // byte - a member index ours aborts on where the original writes past ObjTrio (docs/rest_3a.md section 4)
    C("BattleFxReform_Reload", 0x433A50, 0xCE, kCalls433A50, BH_N(kCalls433A50), nullptr, 0, BH_FN(BattleFxReform_Reload), S::kTask, 0,
      1, 0, 0, true),
    C("BattleFxReform_Grow", 0x433B20, 0x54, kCalls433B20, BH_N(kCalls433B20), nullptr, 0, BH_FN(BattleFxReform_Grow), S::kTask),
    C("BattleFxRestore_Dispatch", 0x433B80, 0x3E, nullptr, 0, kImms433B80, BH_N(kImms433B80), BH_FN(BattleFxRestore_Dispatch),
      S::kTask, 0, 1, 5),
    C("BattleFxRestore_Begin", 0x433BC0, 0x32, nullptr, 0, nullptr, 0, BH_FN(BattleFxRestore_Begin), S::kTask),
    C("BattleFxRestore_LoadDat", 0x433C00, 0x157, kCalls433C00, BH_N(kCalls433C00), nullptr, 0, BH_FN(BattleFxRestore_LoadDat),
      S::kTask),
    // kind 3
    C("BattleBossFx_Dispatch", 0x4357D0, 0x56, nullptr, 0, kImms4357D0, BH_N(kImms4357D0), BH_FN(BattleBossFx_Dispatch), S::kTask, 0, 5,
      8),
    // the enemy animation helpers, New Game
    C("BattleEnemy_SetAnimationAs", 0x435A20, 0x4D, kCalls435A20, BH_N(kCalls435A20), nullptr, 0, BH_FN(BattleEnemy_SetAnimationAs),
      S::kHelper),
    C("BattleEnemy_SetAnimationOf", 0x435A70, 0x3A, kCalls435A70, BH_N(kCalls435A70), nullptr, 0, BH_FN(BattleEnemy_SetAnimationOf),
      S::kHelper),
    C("NewGame_InitCharacters", 0x437820, 0x8B, nullptr, 0, nullptr, 0, BH_FN(NewGame_InitCharacters), S::kHelper),
};
static_assert(BH_COUNT(kOutside) + BH_COUNT(kEngine) == 45, "the cut's 44 and 0x4041B0");
const bh::Clone* g_base = kEngine;   // the Run's clones (Seed and Args index them by k)

// ===========================================================================
// The callees the standard sets lack or record too coarsely, the tables, the
// regions
// ===========================================================================

const bh::Callee kOutsideCallees[] = {
    // the y pushed with a stale high half: WorldMap_DrawFrame reads its low word (world_map.cpp)
    {"WorldMap_DrawFrame", bof3::addr::WorldMap_DrawFrame, KeyOf(&::WorldMap_DrawFrame), 2, {kAll, kU16}, bh::Answer::kGarbage, 0, 0},
    // 0x404180's tail jmp: the group's own, logged as a phase
    {"WorldMap33_FrameShown", bof3::addr::WorldMap33_FrameShown, KeyOf(&::WorldMap33_FrameShown), 0, {}, bh::Answer::kPhase, 0, 0},
    {"Window_ResetAll", bof3::addr::Window_ResetAll, KeyOf(&::Window_ResetAll), 0, {}, bh::Answer::kGarbage, 0, 0},
};

const bh::Callee kEngineCallees[] = {
    // the colour pushed with a left-over high half: BattleWin_DrawTileRgb reads
    // bits 0..14 (battle_window_draw.cpp), x / y their low words, size and abe a byte
    {"BattleWin_DrawTileRgb", bof3::addr::BattleWin_DrawTileRgb, KeyOf(&::BattleWin_DrawTileRgb), 5, {kU16, kU16, kU8, 0x7FFF, kU8},
     bh::Answer::kGarbage, 0, 0},
    {"BattleWin_DrawTileTint", bof3::addr::BattleWin_DrawTileTint, KeyOf(&::BattleWin_DrawTileTint), 3, {kU16, kU16, kU8},
     bh::Answer::kGarbage, 0, 0},
    {"Music_FadeOutStop", bof3::addr::Music_FadeOutStop, KeyOf(&::Music_FadeOutStop), 1, {kAll}, bh::Answer::kGarbage, 0, 0},
    {"Music_Play", bof3::addr::Music_Play, KeyOf(&::Music_Play), 2, {kAll, kAll}, bh::Answer::kGarbage, 0, 0},
    {"Snd_LoadBankFile", bof3::addr::Snd_LoadBankFile, KeyOf(&::Snd_LoadBankFile), 1, {kAll}, bh::Answer::kGarbage, 0, 0},
    // BE2's: answers 0..15 or 0xFF (battle_e2.md)
    {"BattleFx_NextStatusIcon", bof3::addr::BattleFx_NextStatusIcon, KeyOf(&::BattleFx_NextStatusIcon), 0, {}, bh::Answer::kByte, 0xFF,
     0x0F},
    {"BattleFx_PlaceOverOwner", bof3::addr::BattleFx_PlaceOverOwner, KeyOf(&::BattleFx_PlaceOverOwner), 0, {}, bh::Answer::kGarbage, 0,
     0},
    {"BattleParty_RecalcStats", bof3::addr::BattleParty_RecalcStats, KeyOf(&::BattleParty_RecalcStats), 0, {}, bh::Answer::kGarbage, 0,
     0},
    {"Battle_BackupFlagged", bof3::addr::Battle_BackupFlagged, KeyOf(&::Battle_BackupFlagged), 0, {}, bh::Answer::kFlag, 0, 0},
    // pushed whole (a zero-extended word, the caller's whole word): compared whole
    {"Battle_StatusTint", bof3::addr::Battle_StatusTint, KeyOf(&::Battle_StatusTint), 1, {kAll}, bh::Answer::kGarbage, 0, 0},
    {"BattleEnemy_SetAnimation", bof3::addr::BattleEnemy_SetAnimation, KeyOf(&::BattleEnemy_SetAnimation), 1, {kAll},
     bh::Answer::kGarbage, 0, 0},
};

// The dispatchers' .data tables, their counts the code's (section 3 of the doc).
const bh::DataTable kOutsideTables[] = {{at::kStates, 4}, {at::kTallySteps, 3}, {at::kTallyOpenSteps, 2}};
const bh::DataTable kEngineTables[] = {{at::kLossSteps, 3}, {at::kRestoreSteps, 5}};

const bh::Region kOutsideRegions[] = {
    {at::kMode, 0x10},        // BATE's mode bytes
    {at::kModeRequest, 1},
    {at::kGameStep, 2},
    {at::kMapMode, 1},
};
const bh::Region kEngineRegions[] = {
    {at::kChosen, 0x10},          // the six chosen bytes and what follows them
    {0x903B24, 0x46C},            // character records 1..7 past the engine frame's 0x903A50..0x903B24
    {0x904060, 8},                // the party list
    {at::kAreaNumber, 2},
    {at::kWaitWord, 2},
    {at::kPassFlags, 1},
    {at::kCameraDistance, 0x14},  // Camera_Distance .. the tint's three bytes 0x903850
    {at::kRedraw, 1},
    {at::kPartySet, 1},
    {at::kWhelpSlot, 1},
};

// ===========================================================================
// Seeds, arguments, disturbance
// ===========================================================================

unsigned char& B(U address) { return Mem(address)[0]; }
unsigned char Byte(std::initializer_list<U> often) {
    if (!bh::Often()) return static_cast<unsigned char>(bh::Next());
    const U* v = often.begin();
    return static_cast<unsigned char>(v[bh::Next() % often.size()]);
}
std::uint16_t Word16(std::initializer_list<U> often) {
    if (!bh::Often()) return static_cast<std::uint16_t>(bh::Next());
    const U* v = often.begin();
    return static_cast<std::uint16_t>(v[bh::Next() % often.size()]);
}
unsigned char* Slot() { return bh::Pointer(at::kTaskCurrent); }
unsigned char* Owner() { return bh::Pointer(at::kTaskOwner); }
// The owner as a party member: a member's record with its own index at +5,
// half the time; else the harness's owner with +5 a member index.
void OwnerMember() {
    if (bh::Half()) {
        const unsigned m = bh::Next() % 3;
        bh::SetPointer(at::kTaskOwner, bh::PartyOf(static_cast<unsigned char>(m)));
        Owner()[5] = static_cast<unsigned char>(m);
    } else {
        Owner()[5] = static_cast<unsigned char>(bh::Next() % 3);
    }
}

void SeedOutside(unsigned k) {
    unsigned char* const s = Sprite_Current;
    switch (g_base[k].base) {
    case 0x404180:   // around the bound 0x10 after the add, and the wrap at 0x8000
        SetWord(s + 0x2E, Word16({0xFFFF, 0, 1, 0xFFFE, 0x7FF0, 0x7FEF, 0xFFD0}));
        B(at::kMapMode) = Byte({2, 0, 1});
        break;
    case 0x4041B0:
        B(at::kMapMode) = Byte({2, 0, 1, 3});
        break;
    case 0x4041E0:   // around -0x30 after the subtract
        SetWord(s + 0x2E, Word16({0xFFE0, 0xFFDF, 0xFFE1, 0xFFD0, 0x800F, 0x8010, 0x10}));
        B(at::kMapMode) = Byte({2, 0, 1});
        break;
    default:
        break;
    }
}

void SeedEngine(unsigned k) {
    // every round: the party set inside the 20 entries the file tables have
    Mem(at::kPartySet)[0] = static_cast<unsigned char>(bh::Often() ? BH_PICK(7, 0xD, 0xE, 0xF) : bh::Next() % at::kPartySetCount);
    switch (g_base[k].base) {
    case 0x42E0E0:   // the party list: 0, 7 or another member at each place
        for (U i = 0; i < 3; ++i) Mem(at::kPartyList + i)[0] = Byte({0, 7, 1, 2, 3, 6});
        break;
    case 0x42E250:   // each chosen byte 0, the one held, or another
        for (U i = 0; i < at::kChosenCount; ++i) {
            const unsigned pick = bh::Next() % 3;
            Mem(at::kChosen + i)[0] = pick == 0 ? 0 : pick == 1 ? Mem(at::kGuestEquip + i)[0] : static_cast<unsigned char>(bh::Next());
        }
        break;
    case 0x42E2F0:   // window 1's slot cursor inside the six bytes (0 and 3 the screen's two slots)
        Mem(at::kWin1Slot)[0] = static_cast<unsigned char>(bh::Often() ? BH_PICK(0, 3) : bh::Next() % at::kChosenCount);
        break;
    case 0x431550:
        Mem(at::kMusicFlags)[0] = static_cast<unsigned char>(bh::Half() ? Mem(at::kMusicFlags)[0] | 0x40 : Mem(at::kMusicFlags)[0] & ~0x40);
        break;
    case 0x4318F0:
        if (bh::Half()) SetWord(Mem(at::kWaitWord), 0);
        break;
    case 0x432F90: case 0x4332B0:   // the running slot's +1 inside the stack table
        Slot()[1] = static_cast<unsigned char>(bh::Next() % 3);
        break;
    case 0x432FC0: case 0x433020: case 0x4330E0:
        Mem(at::kActor)[0] = Byte({0, 1, 2, 3, 4, 10});
        Slot()[9] = Byte({0, 1, 2});
        break;
    case 0x433300:
        Sprite_Current[9] = Byte({0xFE, 0xFF, 0, 0xFD});
        break;
    case 0x4333C0: {   // the action's ability id inside Ability_Records' 228 (unchecked, a read)
        unsigned char* const action = bh::Pointer(at::kAction);
        SetWord(action + 2, bh::Next() % 228);
        break;
    }
    case 0x433430:
        Sprite_Current[9] = Byte({1, 2, 0});
        break;
    case 0x433790:   // the round flags' bit 2, the owner a member or an enemy, its status
        Mem(at::kFlags)[0] = static_cast<unsigned char>(bh::Half() ? Mem(at::kFlags)[0] | 4 : Mem(at::kFlags)[0] & ~4);
        Owner()[5] = static_cast<unsigned char>(bh::Next() % 11);
        break;
    case 0x4339D0: case 0x433B20:   // the owner's scale at its ends
        SetLong(Owner() + 0x40, static_cast<std::int32_t>(bh::Often() ? BH_PICK(0, 0x2000, 0xE000, 0x10000, 0xFFFFE000u) : bh::Next()));
        OwnerMember();
        break;
    case 0x433A00:
        if (bh::Half()) SetWord(Owner() + 0x2C, 0);
        Owner()[8] = Byte({0, 1, 2, 3});
        break;
    case 0x433A50: case 0x433BC0:
        OwnerMember();
        Owner()[8] = Byte({0, 1, 2, 3});
        break;
    case 0x433C00: {
        U flags = static_cast<U>(move_script::Long(Mem(at::kFlags)));
        flags = bh::Half() ? flags | 0x8000 : flags & ~0x8000u;
        SetLong(Mem(at::kFlags), static_cast<std::int32_t>(flags));
        Owner()[8] = Byte({0, 1, 2});
        break;
    }
    case 0x4357D0:   // the running slot's +5 inside the eight
        Slot()[5] = static_cast<unsigned char>(bh::Next() % 8);
        break;
    case 0x437820:
        Mem(at::kWhelpSlot)[0] = static_cast<unsigned char>(bh::Often() ? 7 : bh::Next() % at::kCharCount);
        break;
    default:
        break;
    }
}

// The words: an enemy's battle index 3..10 with garbage above it half the
// time (ours aborts outside the eight), any animation word.
void ArgsEngine(unsigned k, U* a) {
    switch (g_base[k].base) {
    case 0x435A20: case 0x435A70:
        a[0] = (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | (3 + bh::Next() % 8);
        a[1] = bh::Often() ? bh::Next() % 0x20 : bh::Next();
        break;
    default:
        break;
    }
}

// What the group's functions read again after a call - from the hash only.
void DisturbOutside(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 4) {
    case 0: B(at::kModeSub) = b; break;
    case 1: B(at::kMapMode) = static_cast<unsigned char>(b & 3); break;
    case 2: SetWord(Sprite_Current + 0x2E, static_cast<unsigned>(h >> 16)); break;
    default: Sprite_Current[2] = static_cast<unsigned char>(b & 3); break;
    }
}

void DisturbEngine(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 10) {
    case 0: B(at::kSubStep) = b; break;
    case 1: B(at::kChosen + (h >> 24) % at::kChosenCount) = b; break;
    case 2: B(at::kGuestEquip + (h >> 24) % at::kChosenCount) = b; break;
    case 3: B(at::kPartySet) = static_cast<unsigned char>(b % at::kPartySetCount); break;
    case 4: Slot()[9 + (h >> 24) % 2] = b; break;
    case 5: SetWord(Slot() + 0x10, h >> 16); break;
    case 6: Owner()[8] = b; break;
    case 7: B(at::kMusicFlags) = b; break;
    case 8: SetWord(Mem(at::kAreaNumber), h >> 16); break;
    default: Sprite_Current[9] = b; break;
    }
}

// The run, filtered to one clone by BOF3X_R3A_ONLY.
void RunGroup(const char* shadow, const bh::Clone* all, unsigned count, U want, const bh::Callee* callees, unsigned n_callees,
              const bh::DataTable* tables, unsigned n_tables, const bh::Region* regions, unsigned n_regions, void (*seed)(unsigned),
              void (*disturb)(U), void (*args)(unsigned, U*), bool engine) {
    const bh::Clone* clones = all;
    unsigned n = count;
    if (want != 0) {
        n = 0;
        for (unsigned i = 0; i < count; ++i)
            if (all[i].base == want) {
                clones = &all[i];
                n = 1;
            }
        if (n == 0) return;
    }
    g_base = clones;
    bh::Group g{shadow, clones, n, callees, n_callees, tables, n_tables, regions, n_regions, seed, disturb, 6000};
    g.args = args;
    g.engine = engine;
    bh::Run(g);
}

}  // namespace

void SelfTest() {
    const char* const only = std::getenv("BOF3X_R3A_ONLY");
    const U want = only && *only ? static_cast<U>(std::strtoul(only, nullptr, 16)) : 0;
    if (want != 0) {
        bool found = false;
        for (const bh::Clone& c : kOutside) found = found || c.base == want;
        for (const bh::Clone& c : kEngine) found = found || c.base == want;
        if (!found) bof3::Fatal("rest_3a: BOF3X_R3A_ONLY=%s names no clone", only);
    }
    // the nine below the engine's runs: a boss frame (Seed and Args index
    // the run's clones by k through g_base, which RunGroup sets)
    RunGroup("rest_3a.outside", kOutside, BH_COUNT(kOutside), want, kOutsideCallees, BH_COUNT(kOutsideCallees), kOutsideTables,
             BH_COUNT(kOutsideTables), kOutsideRegions, BH_COUNT(kOutsideRegions), &SeedOutside, &DisturbOutside, nullptr, false);
    // the 36 in the engine's runs: the engine frame
    RunGroup("rest_3a", kEngine, BH_COUNT(kEngine), want, kEngineCallees, BH_COUNT(kEngineCallees), kEngineTables,
             BH_COUNT(kEngineTables), kEngineRegions, BH_COUNT(kEngineRegions), &SeedEngine, &DisturbEngine, &ArgsEngine, true);
}

}  // namespace rest_3a
