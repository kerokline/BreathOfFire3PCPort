// Area 11's code: the PSX's BIN/WORLD00/AREA011.EMI (descriptor 0x801F30B0
// there, 0x5E2738 here, Area_Descriptors entry 11), compiled into the exe at
// 0x401750..0x401839 - three functions, each read to its last instruction
// with capstone (2026-09-27) and taken through the area harness
// (area_harness.h) as its proof. docs/area_011.md.
//
//   Area11_StepCameraDistance  0x401750 (0x1C)  handler 0 (Area11_Handlers 0x5E2730, the descriptor's +0x3C)
//   Area11_SpawnEffect         0x401770 (0x46)  handler 1
//   Area11_DimBackdrop         0x4017C0 (0x79)  the init (+0x40)
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Every
// call goes through the harness (AH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies.
#include "game/area_011.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_011_callees.h"
#include "game/area_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_011::at;
using move_script::Word;

}  // namespace

void Area011_Inject() {
    if (bof3::WantsShadow("area_011")) area_011::SelfTest();
}
