#include "game/cheats.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>

#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

// Cheats: the same three the archival sibling offers as mods (its
// docs/EXP_BOOST.md and docs/STEAL.md) as launcher settings here, and two
// of our own: every enemy's information shown (DIV-0187) and the minigames
// that cannot be failed (DIV-0188).
//
// DIV-0045: BOF3X_EXP=n / BOF3X_ZENNY=n, 0..10 (more is clamped to 10, with
// a log line, so that an older bof3x.ini's 50 still starts the game).
// Battle_EnemyDefeated (src/game/battle_flow.cpp, ours) multiplies the fallen
// enemy's yield by this before adding it to the battle total, so the results
// screen, the per-member split and any level-ups follow, and the enemy's own
// record is left as it was. Two boss hooks write the EXP total themselves
// and take the EXP multiplier too: Boss16_End (0x43A190, Balio and Sunder's
// second fight, src/game/boss_sc.cpp), which sets the total to the two
// enemies' own +0x96 on a win, and BossWeretigr_EndMove (0x43D5A0,
// src/game/boss_sa.cpp), which adds Weretigr's. 1 is the original's; 0
// grants nothing.
//
// DIV-0046: BOF3X_STEAL=1. Pilfer and Steal each run from a copy of one
// routine (the PSX's MAGIC065.EMI and MAGIC216.EMI, one function each in the
// exe: the state step at 0x4B54F0 for Pilfer, 0x4F5140 for Steal). Both roll
//
//     Rand() & 0xFF  <  rate[enemy +0x1A] * agility tier
//
// with Steal_RateTable (0x65AC20 / 0x65C204, eight rates, the PSX's byte for
// byte; read in place, never copied - docs/exe-table-audit.md) and the tier 12 .. 4 by the attacker's agility less
// the enemy's. The random byte's mask, `and eax, 0xFF` at 0x4B5690 and
// 0x4F51EE, becomes `and eax, 0` - the sibling's patch - so the roll passes
// whenever the enemy's chance was above zero. An enemy at steal level 0
// stays unstealable, an enemy with nothing still says so, and the routine
// clears the item on success, so the first attempt takes it and there is no
// farming.
//
// Since round eight Pilfer's step 0x4B54F0 is ours (Steal_Start in
// src/game/magic_fx_reached.cpp) and never runs Capcom's patched body, so it
// asks Cheats_PilferRollMask for the mask: the byte this patch leaves at
// 0x4B5691, read back after it. The patch is still made, so that
// BOF3X_ORIGINAL=Steal_Start keeps the cheat, and the shadow fuzz's copy of
// the original carries it. Since round nine Steal's step 0x4F5140 is ours
// too (SkillSteal_Roll in src/game/magic_steal.cpp) and reads its mask the
// same way, from 0x4F51EF (Cheats_StealRollMask).
//
// DIV-0187: BOF3X_ENEMY_INFO=1. An enemy's HP gauge and Identify's stats
// panel read the enemy record's byte +0x8F, which Battle_SetupEnemy
// (0x494320, ours) copies from the save's per-kind bit set at 0x904068
// (Battle_EnemyKindFlag 0x494EA0; the bit is set by Battle_EnemyDefeated
// 0x437470, so it reads "a kind the party has beaten before") and which
// Battle_PlaceBossActor (0x494570, ours) clears for a boss. With the cheat
// on, both write 1 instead, so every enemy shows its gauge and its stats
// from the first meeting, bosses included. The bit set, and so the save, is
// left as it was: the cheat off, the game is where it would have been.
//
// DIV-0188: BOF3X_NO_FAIL=1. Each minigame is a state of its own
// subsystem (an area's field tail kind, an effect kind, a scenario step:
// docs/button-tests.md 2.2), and each judges the player's attempt in a
// step of its own, so the switch is one flag here and one guarded branch
// per minigame, added as each is reached by a recorded route and verified.
// The rope pull (area 75): Area75_PhasePlay sends the pull to its missed
// phase on more than three presses out of turn, more than 0x1E idle frames
// or the two counters more than 0xC8 apart; with the switch on it never
// does, and the pull runs until the player's counter reaches the mark.
// The module that owns a guarded function checks at inject that the
// function is ours when the switch is on (AreaW1f_Inject): Capcom's body
// would judge as before and the cheat would be silently half off.
//
// Every harness run leaves these unset: with the variables unset nothing
// here changes a byte or a number, and the battle_flow fuzz compares ours
// against the original with the multipliers at 1.

namespace {

std::uint32_t g_exp = 1, g_zenny = 1;
bool g_enemy_info = false;
bool g_no_fail = false;
// The roll masks: the imm32 at kPilferMaskImm / kStealMaskImm in this build -
// 0xFF, or 0 once DIV-0046's patch goes in. Values of ours, never read from
// the code (docs/operand-store.md); hosted, Operands_Check compares them.
std::uint32_t g_pilfer_mask = 0xFF;
std::uint32_t g_steal_mask = 0xFF;

constexpr long kMultiplierMax = 10;   // the owner, 2026-10-02: 50 was humorously large

// 0..10, 1 when unset. A number above 10 is clamped to 10 and logged (an ini
// saved while the cap was 50 must not lock its owner out); anything that is
// not a whole number 0 or more is a Fatal, as a wrong BOF3X_FILTER is.
std::uint32_t Multiplier(const char* var) {
    char text[16];
    const DWORD n = GetEnvironmentVariableA(var, text, sizeof text);
    if (n == 0) return 1;
    char* end = nullptr;
    const long v = n < sizeof text ? std::strtol(text, &end, 10) : -1;
    if (end == nullptr || end == text || *end != '\0' || v < 0) bof3::Fatal("%s must be 0..%ld", var, kMultiplierMax);
    if (v > kMultiplierMax) {
        bof3::Log("DIV-0045    %s=%ld is above the cap, %ld used", var, v, kMultiplierMax);
        return static_cast<std::uint32_t>(kMultiplierMax);
    }
    return static_cast<std::uint32_t>(v);
}

// 0 or 1, false when unset; anything else is a Fatal, as a wrong
// BOF3X_FILTER is.
bool Flag(const char* var) {
    char text[8];
    const DWORD n = GetEnvironmentVariableA(var, text, sizeof text);
    if (n == 0) return false;
    if (n >= sizeof text || (text[0] != '0' && text[0] != '1') || text[1] != '\0') bof3::Fatal("%s must be 0 or 1", var);
    return text[0] == '1';
}

// The mask's immediate with the compare after it, so that a wrong address
// cannot pass for the right one: `25 FF 00 00 00 / 3B C7` (Pilfer, cmp eax,
// edi) and `25 FF 00 00 00 / 3B C6` (Steal, cmp eax, esi).
constexpr std::uint32_t kPilferMaskImm = 0x4B5691;
constexpr std::uint32_t kStealMaskImm = 0x4F51EF;

}  // namespace

void Cheats_Inject() {
    g_exp = Multiplier("BOF3X_EXP");
    g_zenny = Multiplier("BOF3X_ZENNY");
    if (g_exp != 1 || g_zenny != 1) bof3::Log("DIV-0045    EXP x%lu, zenny x%lu (BOF3X_EXP, BOF3X_ZENNY)", (unsigned long)g_exp, (unsigned long)g_zenny);

    g_enemy_info = Flag("BOF3X_ENEMY_INFO");
    if (g_enemy_info) bof3::Log("DIV-0187    every enemy shows its HP gauge and its Identify stats, bosses too (BOF3X_ENEMY_INFO)");

    g_no_fail = Flag("BOF3X_NO_FAIL");
    if (g_no_fail) bof3::Log("DIV-0188    the minigames cannot be failed (BOF3X_NO_FAIL)");

    bof3::Operand("DIV-0046 Pilfer roll mask", kPilferMaskImm, &g_pilfer_mask, 4);
    bof3::Operand("DIV-0046 Steal roll mask", kStealMaskImm, &g_steal_mask, 4);
    if (!Flag("BOF3X_STEAL")) return;
    const std::uint8_t pilfer_was[] = {0xFF, 0x00, 0x00, 0x00, 0x3B, 0xC7}, pilfer_is[] = {0x00, 0x00, 0x00, 0x00, 0x3B, 0xC7};
    const std::uint8_t steal_was[] = {0xFF, 0x00, 0x00, 0x00, 0x3B, 0xC6}, steal_is[] = {0x00, 0x00, 0x00, 0x00, 0x3B, 0xC6};
    if (bof3::PatchBytes("Cheat_StealAlways", kPilferMaskImm, pilfer_was, pilfer_is, 6)) g_pilfer_mask = 0;
    if (bof3::PatchBytes("Cheat_StealAlways", kStealMaskImm, steal_was, steal_is, 6)) g_steal_mask = 0;
    bof3::Log("DIV-0046    steal always succeeds (BOF3X_STEAL)");
}

std::uint32_t Cheats_ExpMultiplier() { return g_exp; }
std::uint32_t Cheats_ZennyMultiplier() { return g_zenny; }
std::uint32_t Cheats_PilferRollMask() { return g_pilfer_mask; }
std::uint32_t Cheats_StealRollMask() { return g_steal_mask; }
bool Cheats_EnemyInfoShown() { return g_enemy_info; }
bool Cheats_MinigameNoFail() { return g_no_fail; }
