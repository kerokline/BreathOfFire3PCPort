// Spell group S19 of round nine: two BMAGIC overlays compiled into the exe -
// MAGIC083 (Magic_Rows row 94, 0x4C1470..0x4C27E6, ability id 0x53: Shield
// read one id down) and MAGIC086 (row 28, 0x4C27F0..0x4C3487, ability id
// 0x56: no English label one id down; the sibling's abilities.toml calls that
// id's neighbour Barrier). 43 functions, taken with the shared spell harness
// (magic_harness.h). docs/magic_s19.md.
#pragma once

void MagicS19_Inject();

namespace magic_s19 {
// BOF3X_SHADOW=magic_s19: the start-up fuzz, magic_s19_fuzz.cpp. Clones
// every original before MagicS19_Inject patches it.
void SelfTest();
}  // namespace magic_s19
