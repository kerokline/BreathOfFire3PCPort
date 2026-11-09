// Moved here by tools/move_funcs.py from effect_6b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_40.h"

#include <bit>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_6b.h"
#include "game/effect_6b_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U AddressOf(const void* p) { return Low32(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6b.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

}  // namespace

// original 0x50FA00 (hidden in 0x50F590).
extern "C" void __cdecl EffectKind18Sub40_Run(void) {
    Dispatch("EffectKind18Sub40_Run", AddressOf(EffectKind18Sub40_States), EffectKind18Sub40_States_count);
}

// original 0x50FA20 (sub-state 0): when the chapter's run byte (MoveScript_Var7)
// is 0xC and its step byte 0x8034E5 is 2: sound 0x202 and +2 up (read after the
// sound). Nothing drawn.
extern "C" void __cdecl EffectKind18Sub40_WaitCue(void) {
    if (static_cast<unsigned char>(MoveScript_Var7) != 0xC || MoveScript_PartyRecords[0x65] != 2) return;
    SH_CALL(Sound_PlayEffect)(at::kSoundCue);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x50FA50 (sub-state 1): when Cond_ByteFE is 1, +2 up and the panes
// at step 0; otherwise nothing drawn.
extern "C" void __cdecl EffectKind18Sub40_WaitOne(void) {
    if (Cond_ByteFE != 1) return;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub40_Draw)(0);
}

// original 0x50FA70 (sub-state 2): when Cond_ByteFE is 2, sounds 0x206 and 0x203
// and +2 up (read after them). The panes at step 0.
extern "C" void __cdecl EffectKind18Sub40_WaitTwo(void) {
    if (Cond_ByteFE == 2) {
        SH_CALL(Sound_PlayEffect)(at::kSoundEndA);
        SH_CALL(Sound_PlayEffect)(at::kSoundEnd);
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub40_Draw)(0);
}

// original 0x50FAB0 (sub-state 3): +9 up; when Cond_ByteFE is 0 Effect_Release.
// The panes at step +9 (read after the release).
extern "C" void __cdecl EffectKind18Sub40_Move(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (Cond_ByteFE == 0) SH_CALL(Effect_Release)();
    SH_CALL(EffectKind18Sub40_Draw)(S()[9]);
}

void EffectKind18Sub40_Inject() {
    BOF3_INJECT(EffectKind18Sub40_Run);
    BOF3_INJECT(EffectKind18Sub40_WaitCue);
    BOF3_INJECT(EffectKind18Sub40_WaitOne);
    BOF3_INJECT(EffectKind18Sub40_WaitTwo);
    BOF3_INJECT(EffectKind18Sub40_Move);
}
