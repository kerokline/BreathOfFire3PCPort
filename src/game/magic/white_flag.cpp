// Moved here by tools/move_funcs.py from magic_c1.cpp (docs/source-layout-round.md).
#include "game/magic/white_flag.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

}  // namespace

#define C1_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x49DEF0: the kind-2 task. A four-entry stack table by +1:
// WhiteFlag_TintSource, MAGIC117's tint-up (0x4D9FD0), WhiteFlag_Untint, the
// engine's end (0x43F460).
C1_EXPORT void __cdecl WhiteFlag_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::WhiteFlag_TintSource, bof3::addr::Magic117_Brighten,
                                                 bof3::addr::WhiteFlag_Untint, bof3::addr::MagicFx_FlagTargetEnd};
    StackCall(kPhases, 4, Sc()[1], "WhiteFlag_Task");
}

// original 0x49DF30: the source sprite's tints released and a tint (0, 0, 0,
// 1) set on it, its record kept in +0xB; +9 0; +1 on.
C1_EXPORT void __cdecl WhiteFlag_TintSource(void) {
    MH_CALL(Sprite_ReleaseTint)(Source());
    const unsigned char tint = MH_CALL(Sprite_SetTint)(Source(), 0, 0, 0, 1);
    Sc()[0xB] = tint;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x49DF70: the tint record +0xB's colour bytes down by one; +9 down;
// at 0 the source's tints released, the target actor flashed, +1 on. The
// record index is unbounded, as in the original (up to 255 records of 12
// bytes, inside MoveScript_TintRecords' 3,072).
C1_EXPORT void __cdecl WhiteFlag_Untint(void) {
    unsigned char* const s = Sc();
    for (unsigned k = 2; k <= 4; ++k) Dec(MoveScript_TintRecords[s[0xB] * 12u + k]);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    MH_CALL(BattleActor_Flash)(TargetByte());
    Inc(Sc()[1]);
}

void WhiteFlag_Inject() {
    BOF3_INJECT(WhiteFlag_Task);
    BOF3_INJECT(WhiteFlag_TintSource);
    BOF3_INJECT(WhiteFlag_Untint);
}
