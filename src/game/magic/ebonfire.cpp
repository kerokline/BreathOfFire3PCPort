// Moved here by tools/move_funcs.py from magic_s10.cpp (docs/source-layout-round.md).
#include "game/magic/ebonfire.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s10.h"
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

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

}  // namespace

#define S10_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4ACFE0: FxDiscFan_Task's twin. A three-entry stack table by +1
// (Ebonfire_Start, FxDiscFan_Grow, FxDiscFan_Fade), then while the task
// lives its screen point 16 up, the disc, and the fan under the actor's
// matrix.
S10_EXPORT void __cdecl Ebonfire_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Ebonfire_Start, addr::FxDiscFan_Grow, addr::FxDiscFan_Fade};
    Dispatch(kPhases, 3, Sc()[1], "Ebonfire_Task");
    if (Sc()[0] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    {
        unsigned char* const s = Sc();
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_y, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y) - 0x10u);
    }
    MH_CALL(MagicFx_DrawDisc)();
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(MagicFx_DrawFan)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AD040: the source sprite's direction and position; six rings
// (kind 1, 0x34) owned by this task, +9 6 i + 1, counted in +0xB; words
// 1..15 of CLUT row 26 back with their STP bits, word 0 cleared; sound
// 0x100; the target flagged 0x10 (the target read after the sound); +9 and
// +0xA 1, +1 on.
S10_EXPORT void __cdecl Ebonfire_Start(void) {
    {
        const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
        Sc()[8] = src[8];
        rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
        rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
        rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    }
    Sc()[0xB] = 0;
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned slot = NewTask(0x34);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[9] = static_cast<unsigned char>(i * 6 + 1);
        Inc(self[0xB]);
    }
    for (unsigned k = 0x1A01; k < 0x1A10; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = 0;
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Sc()[9] = 1;
    Sc()[0xA] = 1;
    Inc(Sc()[1]);
}

void Ebonfire_Inject() {
    BOF3_INJECT(Ebonfire_Task);
    BOF3_INJECT(Ebonfire_Start);
}
