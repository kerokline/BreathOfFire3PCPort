// Group R3D of round fourteen (wave three): 36 functions in 0x44E4B0..0x44FF0E -
// the cut's 36 rows for R3D (analysis/round14_cut.tsv), all functions, none
// added or dropped. docs/rest_3d.md.
//
// What they are, by the code:
//   - Effect_Handlers' last eighteen slots (112..129), each a void (void) entry
//     Effect_ApplyResult calls through the table for the target 0x904B54;
//   - the helpers the effect slots of R3B, R3C and this group call: the "no hit
//     reaction" mark (0x44FB30, 23 sites in R3B and R3C), the status rolls and
//     their rate lookups, the status inflict and its two equipment tests, the
//     stat-step byte and its two rolled forms, the two rolled inflicts, the
//     HP-based damage;
//   - Battle_PsiStatusDeathAffinity (Effect_SkillDamage's psi branch) and the
//     resist roll Battle_ApplyDamage makes for a weapon's status (0x44FA70);
//   - 0x44FF00, Battle_MenuSteps[7]: the Dragon command's part dispatcher by
//     0x904AA3 over DragonCmd_Parts (group BE5's seven parts).
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void Rest3D_Inject();

namespace rest_3d {
// BOF3X_SHADOW=rest_3d: the start-up fuzz, rest_3d_fuzz.cpp - one
// boss_harness::Run over the group, Group::engine set. Clones every original
// before Rest3D_Inject patches it.
void SelfTest();
}  // namespace rest_3d
