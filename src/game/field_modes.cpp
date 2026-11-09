// The field's mode handlers (docs/field-modes.md). Field_ModeDispatch
// 0x56D690 is slot 0 of the current scenario chapter's vtable - the PSX's
// thunk 0x801A8834 - and the two tail dispatchers after it; scenario 16, the
// attract sequence's demo (PSX SCENA16.EMI), is a state machine of eleven
// handlers under its slot 0; Scenario_CallA 0x5341A0 is the chapter's call
// table's thunk (PSX 0x801C2DE8). Every call goes through field_modes::g, so
// that the start-up fuzz (field_modes_fuzz.cpp) tests each function alone.
#include "game/field_modes.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/field_modes_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_modes {

using move_script::At;
using move_script::Long;

const Callees kOriginals = {
    Scenario_CallA,
    reinterpret_cast<void (__cdecl*)(int, int, int, int)>(::bof3::Code(kChangeArea.v)),
    reinterpret_cast<void (__cdecl*)(int)>(::bof3::Code(kStartMusic.v)),
    File_LoadDone,
    Task_Sleep,
    reinterpret_cast<void (__cdecl*)()>(::bof3::Code(kTaskEnd.v)),
    ObjTrio_SetBit40,
    Flags_Test,
    Flags_Set,
    ScriptFlags_Set40,
    MapView_SetElevation,
    Kind2_Place,
    reinterpret_cast<unsigned char (__cdecl*)()>(::bof3::Code(kEffectAlloc.v)),
    reinterpret_cast<void (__cdecl*)(int)>(::bof3::Code(kPlaceParty.v)),
    Sound_PlayEffect,
    Music_FadeOutStop,
    Transition_Start,
    Music_Play,
    Text_DrawAt,
    reinterpret_cast<void (__cdecl*)(unsigned short)>(::bof3::Code(kOpenScript.v)),
    reinterpret_cast<void (__cdecl*)()>(::bof3::Code(kViewShift.v)),
    ClutStrip_FadeTo,
    ClutStrip_Restore,
    Scena16_Area1F,
    Scena16_Area04,
    Scena16_Area02,
    Field_ModeTail,
};
Callees g = kOriginals;

}  // namespace field_modes

namespace {

using namespace field_modes;
using VoidFn = void (__cdecl*)();

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
VoidFn Entry(std::uint32_t table, int index) {
    return reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(field_modes::Long(field_modes::At(table + 4u * static_cast<std::uint32_t>(index))))));
}

}  // namespace

// original 0x5341A0 (PSX 0x801C2DE8): entry (n & 0xFF) of the current chapter's call table
// (0x660B84[chapter], PSX Scenario_CallA 0x801C2DE8 through 0x801CDC4C).
// Kept a tail jump: the entries take the caller's arguments where they lie,
// and 153 call sites push anything from one argument to several (cleaned up
// later, some of them together), so no C++ signature forwards them all. None
// of the 153 reads eax after the call (capstone, to the first branch or
// write of eax); the jump passes the entry's through regardless.
// A pair by width (docs/x64-asm-policy.md 8 unit 3): the i686 text as it
// was, and the same tail jump for the x86-64 register arguments.
extern "C" __attribute__((naked)) void __cdecl Scenario_CallA(unsigned) {
#if defined(__i386__)
    // edx holds the image's move first (Bof3_ImageMove, src/hook/host.cpp;
    // task_switch_i686.h says why the VAs are displacements); eax, ecx and
    // edx are the three the original writes.
    asm("movl " BOF3_SYM(Bof3_ImageMove) ", %%edx\n\t"
        "movsbl %c0(%%edx), %%eax\n\t"     // Cond_ByteFA, the chapter
        "movl 4(%%esp), %%ecx\n\t"
        "movl %c1(%%edx,%%eax,4), %%edx\n\t"   // field_modes::kCallATables
        "andl $0xFF, %%ecx\n\t"
        "jmp *(%%edx,%%ecx,4)"
        :
        : "i"(0x8034E0), "i"(0x660B84));
#elif defined(__x86_64__)
    // The same for the Win64 arguments: n in ecx, the rest in rdx, r8, r9 and
    // the caller's stack, forwarded untouched by a tail jump; the chapter
    // sign-extended to 64 bits (a negative one reads below the table, as the
    // i686 wrap does); the table word is a 32-bit code place, loaded into
    // eax and jumped through; r10 is the scratch (volatile, not an
    // argument). The entry sees ecx = n & 0xFF, as the i686 entry does.
    asm("movslq " BOF3_SYM(Bof3_ImageMove) "(%%rip), %%r10\n\t"
        "movsbq %c0(%%r10), %%rax\n\t"          // Cond_ByteFA, the chapter
        "andl $0xFF, %%ecx\n\t"
        "movl %c1(%%r10,%%rax,4), %%r10d\n\t"   // the chapter's table, a 32-bit place
        "movl (%%r10,%%rcx,4), %%eax\n\t"       // the entry's code word
        "jmp *%%rax"
        :
        : "i"(0x8034E0), "i"(0x660B84));
#else
#error "Scenario_CallA: no tail jump for this architecture"
#endif
}

void FieldModes_Inject() {
    if (bof3::WantsShadow("field_modes")) field_modes::SelfTest();
    BOF3_INJECT(Scenario_CallA);
}
