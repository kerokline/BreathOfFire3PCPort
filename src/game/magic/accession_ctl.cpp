// Moved here by tools/move_funcs.py from magic_s33.cpp (docs/source-layout-round.md).
#include "game/magic/accession_ctl.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A new kind-1 child (0x44) owned by this task with phase `phase`, counted in
// +0xB; the child's slot.
unsigned char* Child44(unsigned char phase) {
    const unsigned slot = NewTask(0x44);
    unsigned char* const s = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(s));
    child[1] = phase;
    return child;
}

}  // namespace

#define S33_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4EB660: the controller, a six-entry stack table by +2: _Start,
// _SpawnRing, _SpawnOrb, _Signal, _WaitOwner, _End.
S33_EXPORT void __cdecl AccessionCtl_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::AccessionCtl_Start, bof3::addr::AccessionCtl_SpawnRing,
                                                bof3::addr::AccessionCtl_SpawnOrb, bof3::addr::AccessionCtl_Signal,
                                                bof3::addr::AccessionCtl_WaitOwner, bof3::addr::AccessionCtl_End};
    const unsigned phase = Sc()[2];
    if (phase >= 6) PastTable("AccessionCtl_Run", phase, 6);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4EB6B0: the source sprite's direction; the fight's centre on the
// ground; +0xB 0, +9 4, +2 on; a bolt child (+1 2, +9 1) counted in +0xB;
// sound 0x102.
S33_EXPORT void __cdecl AccessionCtl_Start(void) {
    Sc()[8] = Pointer(::bof3::Img(BattleOdds_ActorObject_addr))[8];
    rv::Store(rv::Current<records::BattleTask>()->x, move_script::Long(Mem(::bof3::Img(BossSi_CentreX_addr))));
    rv::Store(rv::Current<records::BattleTask>()->z, move_script::Long(Mem(::bof3::Img(BossSi_CentreZ_addr))));
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        const long ground = MH_CALL(AreaMap_Elevation)(ts->x, ts->z);
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, U(ground) & 0xFFFF);
    }
    Sc()[0xB] = 0;
    Sc()[9] = 4;
    Inc(Sc()[2]);
    unsigned char* const child = Child44(2);
    child[9] = 1;
    Inc(Sc()[0xB]);
    MH_CALL(Sound_PlayById)(0x102);
}

// original 0x4EB760: +9 down; at 0 a ring child (+1 3) counted in +0xB, +9 4,
// +2 on.
S33_EXPORT void __cdecl AccessionCtl_SpawnRing(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Child44(3);
    Inc(Sc()[0xB]);
    Sc()[9] = 4;
    Inc(Sc()[2]);
}

// original 0x4EB7D0: +9 down; at 0 an orb child (+1 0) counted in +0xB, sound
// 0x103, +9 0x14, +2 on.
S33_EXPORT void __cdecl AccessionCtl_SpawnOrb(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Child44(0);
    Inc(Sc()[0xB]);
    MH_CALL(Sound_PlayById)(0x103);
    Sc()[9] = 0x14;
    Inc(Sc()[2]);
}

// original 0x4EB850: +9 down; at 0 the owner's +0xB 1 (its load may start), +2
// on.
S33_EXPORT void __cdecl AccessionCtl_Signal(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Owner()[0xB] = 1;
    Inc(Sc()[2]);
}

// original 0x4EB880: once the owner's +1 is 2 (its steps are done), +2 on.
S33_EXPORT void __cdecl AccessionCtl_WaitOwner(void) {
    if (Owner()[1] == 2) Inc(Sc()[2]);
}

// original 0x4EB8A0: once every child has ended (+0xB 0), the owner's +0xB 2
// and the task freed.
S33_EXPORT void __cdecl AccessionCtl_End(void) {
    if (Sc()[0xB] != 0) return;
    Owner()[0xB] = 2;
    MH_CALL(BattleTask_FreeCurrent)();
}

void AccessionCtl_Inject() {
    BOF3_INJECT(AccessionCtl_Run);
    BOF3_INJECT(AccessionCtl_Start);
    BOF3_INJECT(AccessionCtl_SpawnRing);
    BOF3_INJECT(AccessionCtl_SpawnOrb);
    BOF3_INJECT(AccessionCtl_Signal);
    BOF3_INJECT(AccessionCtl_WaitOwner);
    BOF3_INJECT(AccessionCtl_End);
}
