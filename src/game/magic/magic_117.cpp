// Moved here by tools/move_funcs.py from magic_s26.cpp (docs/source-layout-round.md).
#include "game/magic/magic_117.h"

#include <cstdint>

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

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char* TintRecord(unsigned i) { return MoveScript_TintRecords + 12 * i; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

}  // namespace

#define S26_EXPORT extern "C" __attribute__((disable_tail_calls))

// The tint record +0xB's colour bytes +2..+4 moved by one (the index read
// again for each).
static void TintStep(int by) {
    const unsigned char* const s = Sc();
    AddB(TintRecord(s[0xB])[2], static_cast<unsigned>(by));
    AddB(TintRecord(s[0xB])[3], static_cast<unsigned>(by));
    AddB(TintRecord(s[0xB])[4], static_cast<unsigned>(by));
}

// original 0x4D9F40: row 73's kind-2 task. A four-entry stack table by +1:
// Magic117_TintSet, Magic117_Brighten, Magic117_Dim, and group E's 0x43F460
// (the target's flag 0x40, the done flag, free).
S26_EXPORT void __cdecl Magic117_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::Magic117_TintSet, addr::Magic117_Brighten, addr::Magic117_Dim,
                                                 addr::MagicFx_FlagTargetEnd};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Magic117_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D9F80: the source sprite's tints released and a tint (0, 0, 0,
// 1) set on it (the source read again), kept in +0xB; +9 0, +0xA 2 (pulses),
// +1 on.
S26_EXPORT void __cdecl Magic117_TintSet(void) {
    MH_CALL(Sprite_ReleaseTint)(Source());
    const unsigned char tint = MH_CALL(Sprite_SetTint)(Source(), 0, 0, 0, 1);
    Sc()[0xB] = tint;
    Sc()[9] = 0;
    Sc()[0xA] = 2;
    Inc(Sc()[1]);
}

// original 0x4D9FD0 (also MAGIC010's): the tint up one; +9 up, at 0x10 +1 on.
S26_EXPORT void __cdecl Magic117_Brighten(void) {
    TintStep(1);
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (Sc()[9] == 0x10) Inc(Sc()[1]);
}

// original 0x4DA040: the tint down one; +9 down; at 0 the pulses +0xA down -
// one left: +1 back to Magic117_Brighten; none: the tint released, the
// target flashed, +1 on.
S26_EXPORT void __cdecl Magic117_Dim(void) {
    TintStep(-1);
    unsigned char* s = Sc();
    Dec(s[9]);
    s = Sc();
    if (s[9] != 0) return;
    Dec(s[0xA]);
    s = Sc();
    if (s[0xA] != 0) {
        Dec(s[1]);
        return;
    }
    MH_CALL(Sprite_ReleaseTint)(Source());
    MH_CALL(BattleActor_Flash)(TargetByte());
    Inc(Sc()[1]);
}

// original 0x4DA0E0: row 106's kind-2 task. A four-entry stack table by +1:
// group C1's 0x49DF30 (MAGIC010: the tint set), Magic117_Darken,
// Magic117_Lighten, group E's 0x43F460.
S26_EXPORT void __cdecl Magic117_TaskB(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::WhiteFlag_TintSource, addr::Magic117_Darken,
                                                 addr::Magic117_Lighten, addr::MagicFx_FlagTargetEnd};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Magic117_TaskB", phase, 4);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4DA120: the tint down one; +9 up, at 0x10 +1 on.
S26_EXPORT void __cdecl Magic117_Darken(void) {
    TintStep(-1);
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (Sc()[9] == 0x10) Inc(Sc()[1]);
}

// original 0x4DA190: the tint up one; +9 down; at 0 the tint released, the
// target flashed, +1 on.
S26_EXPORT void __cdecl Magic117_Lighten(void) {
    TintStep(1);
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    MH_CALL(BattleActor_Flash)(TargetByte());
    Inc(Sc()[1]);
}

void Magic117_Inject() {
    BOF3_INJECT(Magic117_Task);
    BOF3_INJECT(Magic117_TintSet);
    BOF3_INJECT(Magic117_Brighten);
    BOF3_INJECT(Magic117_Dim);
    BOF3_INJECT(Magic117_TaskB);
    BOF3_INJECT(Magic117_Darken);
    BOF3_INJECT(Magic117_Lighten);
}
