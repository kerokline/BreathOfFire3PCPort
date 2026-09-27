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
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_011::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

}  // namespace

// original 0x401750 (area 11's handler 0, Area11_Handlers[0]; the movement
// script's op 03 / DE runs it; PSX 0x801F2C04): while Camera_Distance, as a
// signed word, is above -0x1400, 0xA0 less; then MapView_Redraw = 2.
extern "C" void __cdecl Area11_StepCameraDistance(void) {
    if (Camera_Distance > -0x1400) Camera_Distance = static_cast<short>(Camera_Distance - 0xA0);
    MapView_Redraw = 2;
}

// original 0x401770 (area 11's handler 1, Area11_Handlers[1]; PSX
// 0x801F2C44): Sprite_Current made the leader; Effect_Spawn(1, 0,
// Area11_EffectKinds[the first party list's third member], the leader's
// words +0x2E and +0x30); an answer other than 0xFF stored to
// Sprite_Current[0xB], Sprite_Current read again after the call. The member
// id indexes the eight-byte table unchecked, as the original's.
extern "C" void __cdecl Area11_SpawnEffect(void) {
    const unsigned member = Mem(at::kPartyThird)[0];
    const unsigned char* const leader = Mem(at::kLeader);
    const auto z = static_cast<short>(Word(leader + 0x30));
    const auto x = static_cast<short>(Word(leader + 0x2E));
    const unsigned char kind = Area11_EffectKinds[member];
    Sprite_Current = Mem(at::kLeader);
    const unsigned char slot = AH_CALL(Effect_Spawn)(1, 0, static_cast<signed char>(kind), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// original 0x4017C0 (area 11's init, the descriptor's +0x40; Area_Enter runs
// it; PSX 0x801F2CC4): with Cond_Flags row 14's bit 0x13 set and bit 0x14
// clear, the first header entry of kind 0x81 (AreaMap_DrawBackdrop's) from
// AreaMap_EntryBase gets both its colour dwords, +8 and the alternate +12,
// 0x21080000 - black above, a dark grey below. The walk as
// AreaMap_HeaderPass's: a zero dword ends it, the byte +2 is the step in
// dwords. As the original: a step of 0 on an entry of another kind never
// ends (read, not run: the fuzz keeps the steps above 0).
extern "C" void __cdecl Area11_DimBackdrop(void) {
    if (!AH_CALL(Flags_Test)(Mem(at::kInitFlags), at::kInitFlagOn)) return;
    if (AH_CALL(Flags_Test)(Mem(at::kInitFlags), at::kInitFlagOff)) return;
    unsigned char* entry = AreaMap_Header + static_cast<std::uint32_t>(AreaMap_EntryBase) * 4u;
    for (std::uint32_t e = static_cast<std::uint32_t>(Long(entry)); e != 0; e = static_cast<std::uint32_t>(Long(entry))) {
        if ((e & 0xFF000000u) == at::kBackdropKind) {
            SetLong(entry + 12, static_cast<std::int32_t>(at::kDimColour));
            SetLong(entry + 8, static_cast<std::int32_t>(at::kDimColour));
            return;
        }
        entry += ((e >> 16) & 0xFFu) * 4u;
    }
}

void Area011_Inject() {
    if (bof3::WantsShadow("area_011")) area_011::SelfTest();
    BOF3_INJECT(Area11_StepCameraDistance);
    BOF3_INJECT(Area11_SpawnEffect);
    BOF3_INJECT(Area11_DimBackdrop);
}
