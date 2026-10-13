// DIVERGENCE DIV-0108 (the PSP-only round's group PB, docs/psp-battle-changes.md):
// two battle rules the PSP's code adds to the PlayStation's, each behind its
// own switch. Read once at start-up (PspCode_Inject):
//
//   BOF3X_PSP unset   each change on when the cache's manifest.toml says
//                     [build] base_policy = "psp" (a PSP-only install), off
//                     otherwise - no cache, no manifest, or base_policy "psx"
//   BOF3X_PSP=none    both off
//   BOF3X_PSP=<list>  comma-separated, exactly the changes named on
//
// The launcher writes BOF3X_PSP from bof3x.ini's psp= (the data suite's two
// code members, recipes/suites.toml). The items:
//
//   duel-loss    C1, docs/psp-code-diff-battle.md: in event battle 0x1B the
//                battle-end byte with bit 0 set and bit 2 clear does not end
//                the battle (BattleAction_End, BattleRoundEnd_NextRound)
//   myria-item   C2: in area 198 under event battle 0x37, a boss actor placed
//                as an enemy gets the item word +0xA8 = 0x317 right after its
//                data is copied (Battle_PlaceBossActor)
//
// Off, every function that asks behaves as it did before this file, byte for
// byte: the battle modules' shadows compare ours against the original with
// both off, and again with both on, where the only differences allowed are
// these two rules (battle_actions_fuzz.cpp, battle_turn_steps_fuzz.cpp,
// PspCode_SelfTest).
#pragma once

#include <cstdint>

namespace psp_code {

enum Change : unsigned {
    kDuelLoss = 1u << 0,    // duel-loss
    kMyriaItem = 1u << 1,   // myria-item
};
constexpr unsigned kAll = kDuelLoss | kMyriaItem;

// The changes on (a mask of Change).
unsigned On();

// The self-tests' seam: the mask set for a test, the previous one answered.
unsigned Force(unsigned mask);

// C1. The PSP's added test, read side by side (PSP-JP 0x11DFA4..0x11DFC4 in
// the turn step's twin, 0x11F36C..0x11F38C in the round start's; PSP-EU
// 0x11DC14 / 0x11EFDC): with the event battle byte (0x904AAA, PSX 0x801462E6)
// 0x1B, the end byte (0x904AE8, PSX 0x80146324) bit 2 clear and bit 0 set,
// the end byte is treated as 0 - the battle goes on. The byte itself is left
// as it is (the PSP writes nothing). True only with duel-loss on.
bool DuelLossGoesOn(unsigned event_battle, unsigned battle_end);

// C2. The PSP's added store (PSP-JP 0x141570..0x141598, PSP-EU
// 0x14133C..0x141364, after its Battle_CopyEnemyData twin and before its
// Battle_SetEnemyOffset twin): with the area (a u16, Game_AreaNumber
// 0x904EFC, PSX 0x80143F00) 198 and the event battle byte 0x37, the enemy
// record's u16 at the PSP's +0x98 - ours +0xA8, the first item word - is
// 0x317. With myria-item on and the condition met, writes it into `record`
// (the 0x128-byte enemy record) and answers true; else touches nothing.
bool MyriaItem(unsigned char* record, unsigned area, unsigned event_battle);

// The item word C2 stores: the PSP's immediate, `ori a0, zero, 0x317` at
// PSP-JP 0x141594 / PSP-EU 0x141360 - a rule of the PSP's code held as a
// named constant, not a copied table (docs/psp-battle-changes.md section 2).
// As an item word it is category 3 (accessories), index 0x17.
constexpr unsigned kMyriaItemWord = 0x317;
// The PSP's record +0x98 is ours +0xA8: its Battle_CopyEnemyData twin (PSP-JP
// 0x141BA0) writes every field after the name 0x10 lower than ours
// (docs/psp-battle-changes.md section 2.2).
constexpr unsigned kItemWordAt = 0xA8;
constexpr unsigned kMyriaArea = 198;        // the PSP's `ori a0, zero, 0xc6`
constexpr unsigned kMyriaEvent = 0x37;      // `ori a1, zero, 0x37`
constexpr unsigned kDuelEvent = 0x1B;       // `ori a2, zero, 0x1b`

}  // namespace psp_code

// Reads BOF3X_PSP (and the cache's manifest when it is unset), logs what is on,
// and with BOF3X_SHADOW naming psp_code runs the rules' own self-test. A
// malformed BOF3X_PSP or base_policy is a Fatal. Called once from InjectAll.
void PspCode_Inject();
