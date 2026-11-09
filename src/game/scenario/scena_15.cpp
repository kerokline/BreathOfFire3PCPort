// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_15.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/scena_calls.h"
#include "game/scena_calls_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_calls::at;

}  // namespace

namespace {

void Set(unsigned a, unsigned b, unsigned c) { SH_CALL(PartySet_Load)(a, b, c, 0); }

void Join(unsigned id) { SH_CALL(Party_Join)(id); }

void Palettes() { SH_AT(void (__cdecl*)(), at::kPalettes.v)(); }

}  // namespace

// The same pair as ScenaCall_SaveAndLeave's.
extern "C" __attribute__((naked)) void __cdecl Scena15_LeaveAllParty7(void) {
#if defined(__i386__)
    __asm__ volatile(
        "pushl %ecx\n\t"
        "call " BOF3_SYM(scena_calls_LeaveAllParty7) "\n\t"
        "addl $4, %esp\n\t"
        "ret");
#elif defined(__x86_64__)
    __asm__ volatile("jmp " BOF3_SYM(scena_calls_LeaveAllParty7));   // the caller's ecx is the body's first argument already
#else
#error "no ecx entry for this architecture"
#endif
}

// original 0x51ABC0 (0x16 bytes; 15 A[1]): Party_Join(4); Party_Join(2); 0x533E00.
extern "C" void __cdecl Scena15_Join42(void) {
    Join(4);
    Join(2);
    Palettes();
}

// original 0x51ABE0 (0x31 bytes; 15 A[2]): PartySet_Load(7, 8, 2, 0); Field_MemberCount 0; Party_Join(7); Party_Join(8); Party_Join(2); 0x533E00.
extern "C" void __cdecl Scena15_Party782(void) {
    Set(7, 8, 2);
    Field_MemberCount = 0;
    Join(7);
    Join(8);
    Join(2);
    Palettes();
}

void Scena15_Inject() {
    BOF3_INJECT(Scena15_LeaveAllParty7);
    BOF3_INJECT(Scena15_Join42);
    BOF3_INJECT(Scena15_Party782);
}
