// Moved here by tools/move_funcs.py from magic_s37.cpp (docs/source-layout-round.md).
#include "game/magic/combustion.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

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

unsigned char* Sc() { return Sprite_Current; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

void Sound(unsigned id) { MH_CALL(Sound_PlayById)(static_cast<unsigned short>(id)); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// Gfx_ClutStrip's row 26 (0x1A00..0x1AFF) back from its source with the
// semi-transparency bit, then the first `row2` words of row 2 (0x200..) the
// same; the first word of each plain (26's first); the strip dirty.
void RestoreRow26ThenRow2(unsigned row2) {
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    for (unsigned k = 0x200; k < 0x200 + row2; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    const unsigned short first26 = Gfx_ClutStripSource[0x1A00], first2 = Gfx_ClutStripSource[0x200];
    Gfx_ClutStrip[0x1A00] = first26;
    Gfx_ClutStrip[0x200] = first2;
    Gfx_ClutStripDirty = 1;
}

}  // namespace

#define S37_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4F71F0: the kind-2 task. A three-entry stack table by +1
// (Combustion_Start, _Wait, BattleFx_Finish), unchecked.
S37_EXPORT void __cdecl Combustion_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Combustion_Start, addr::Combustion_Wait, addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "Combustion_Task");
}

// original 0x4F7220: the task at the side's centre and on the screen; +0xB 0,
// +9 0x10, +1 on; two children (kind 1, 0x60), +0x80 this task, +9 0x10 - the
// glow (+1 1) and the sprite (+1 0) - counted in +0xB; CLUT row 26 back whole
// and the first 16 words of row 2 with their STP bits, the first word of each
// without.
S37_EXPORT void __cdecl Combustion_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Inc(Sc()[1]);
    for (unsigned kind : {1u, 0u}) {
        const unsigned slot = NewTask(0x60);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[1] = static_cast<unsigned char>(kind);
        child[9] = 0x10;
        Inc(self[0xB]);
    }
    RestoreRow26ThenRow2(0x10);
}

// original 0x4F7320: +9 down; at 0 sound 0x100 and +1 on.
S37_EXPORT void __cdecl Combustion_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sound(0x100);
    Inc(Sc()[1]);
}

void Combustion_Inject() {
    BOF3_INJECT(Combustion_Task);
    BOF3_INJECT(Combustion_Start);
    BOF3_INJECT(Combustion_Wait);
}
