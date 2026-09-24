#include "game/cheats.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>

#include "hook/detour.h"
#include "hook/log.h"

// Cheats, the same three the archival sibling offers as mods (its
// docs/EXP_BOOST.md and docs/STEAL.md), as launcher settings here.
//
// DIV-0045: BOF3X_EXP=n / BOF3X_ZENNY=n, 0..50. Battle_EnemyDefeated
// (src/game/battle_flow.cpp, ours) multiplies the fallen enemy's yield by
// this before adding it to the battle total, so the results screen, the
// per-member split and any level-ups follow, and the enemy's own record is
// left as it was. 1 is the original's; 0 grants nothing.
//
// DIV-0046: BOF3X_STEAL=1. Pilfer and Steal each run from a copy of one
// routine (the PSX's MAGIC065.EMI and MAGIC216.EMI, one function each in the
// exe: the state step at 0x4B54F0 for Pilfer, 0x4F5140 for Steal). Both roll
//
//     Rand() & 0xFF  <  rate[enemy +0x1A] * agility tier
//
// with the table [0, 1, 3, 6, 12, 16, 32, 32] (0x65AC20 / 0x65C204, the
// PSX's byte for byte) and the tier 12 .. 4 by the attacker's agility less
// the enemy's. The random byte's mask, `and eax, 0xFF` at 0x4B5690 and
// 0x4F51EE, becomes `and eax, 0` - the sibling's patch - so the roll passes
// whenever the enemy's chance was above zero. An enemy at steal level 0
// stays unstealable, an enemy with nothing still says so, and the routine
// clears the item on success, so the first attempt takes it and there is no
// farming.
//
// Every harness run leaves these unset: with the variables unset nothing
// here changes a byte or a number, and the battle_flow fuzz compares ours
// against the original with the multipliers at 1.

namespace {

std::uint32_t g_exp = 1, g_zenny = 1;

// 0..50, 1 when unset; anything else is a Fatal, as a wrong BOF3X_FILTER is.
std::uint32_t Multiplier(const char* var) {
    char text[16];
    const DWORD n = GetEnvironmentVariableA(var, text, sizeof text);
    if (n == 0) return 1;
    char* end = nullptr;
    const long v = n < sizeof text ? std::strtol(text, &end, 10) : -1;
    if (end == nullptr || *end != '\0' || v < 0 || v > 50) bof3::Fatal("%s must be 0..50", var);
    return static_cast<std::uint32_t>(v);
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

    char text[8];
    const DWORD n = GetEnvironmentVariableA("BOF3X_STEAL", text, sizeof text);
    if (n == 0) return;
    if (n >= sizeof text || (text[0] != '0' && text[0] != '1') || text[1] != '\0') bof3::Fatal("BOF3X_STEAL must be 0 or 1");
    if (text[0] == '0') return;
    const std::uint8_t pilfer_was[] = {0xFF, 0x00, 0x00, 0x00, 0x3B, 0xC7}, pilfer_is[] = {0x00, 0x00, 0x00, 0x00, 0x3B, 0xC7};
    const std::uint8_t steal_was[] = {0xFF, 0x00, 0x00, 0x00, 0x3B, 0xC6}, steal_is[] = {0x00, 0x00, 0x00, 0x00, 0x3B, 0xC6};
    bof3::PatchBytes("Cheat_StealAlways", kPilferMaskImm, pilfer_was, pilfer_is, 6);
    bof3::PatchBytes("Cheat_StealAlways", kStealMaskImm, steal_was, steal_is, 6);
    bof3::Log("DIV-0046    steal always succeeds (BOF3X_STEAL)");
}

std::uint32_t Cheats_ExpMultiplier() { return g_exp; }
std::uint32_t Cheats_ZennyMultiplier() { return g_zenny; }
