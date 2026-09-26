// Three overlays of the spell round's group S21 (the PSX's MAGIC093, 094 and
// 095; Magic_Rows rows 103, 20 and 22, whose abilities read one id down are
// Inferno, Frost and Iceblast), compiled into the exe at 0x4C6300..0x4C8D34:
// 48 functions, every one a battle task's step or a draw it calls. Taken
// with the shared spell harness (magic_harness.h). docs/magic_s21.md.
#pragma once

void MagicS21_Inject();

namespace magic_s21 {
// BOF3X_SHADOW=magic_s21: the start-up fuzz, magic_s21_fuzz.cpp. Clones
// every original before MagicS21_Inject patches it.
void SelfTest();
}  // namespace magic_s21
