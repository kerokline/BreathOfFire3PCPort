// Two spell overlays of Magic_Rows, round nine group S33 (the PSX's MAGIC151
// and MAGIC154.EMI, rows 86 and 35; read one id down, the sibling's labels
// Accession and Mighty Chop): 57 functions, 0x4EAE70..0x4ED66E, taken with the
// shared harness (magic_harness.h). docs/magic_s33.md.
#pragma once

void MagicS33_Inject();

namespace magic_s33 {

// BOF3X_SHADOW=magic_s33: the start-up fuzz, magic_s33_fuzz.cpp. Clones every
// original before MagicS33_Inject patches it.
void SelfTest();

}  // namespace magic_s33
