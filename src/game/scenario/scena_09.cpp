// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_09.h"

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
#include "game/solo/solo_scenes.h"  // DIV-0228: the scene guards

namespace {

namespace at = scena_calls::at;

}  // namespace

namespace {

using move_script::At;

void Set(unsigned a, unsigned b, unsigned c) { SH_CALL(PartySet_Load)(a, b, c, 0); }

void Reload() { SH_CALL(Field_PartyLoad)(0); }

void Join(unsigned id) { SH_CALL(Party_Join)(id); }

void Leave(unsigned id) { SH_AT(void (__cdecl*)(unsigned), at::kLeave.v)(id); }

void Palettes() { SH_AT(void (__cdecl*)(), at::kPalettes.v)(); }

}  // namespace

// original 0x519FD0 (0x23 bytes; 9 A[0]): PartySet_Load(7, 8, 2, 0); Field_PartyLoad(0); Party_Join(8); 0x533E00.
extern "C" void __cdecl Scena09_Set782ReloadJoin8(void) {
    Set(7, 8, 2);
    Reload();
    Join(8);
    Palettes();
}

// original 0x51A000 (0x23 bytes; 9 A[1]): PartySet_Load(7, 8, 2, 0); Field_PartyLoad(0); Party_Join(2); 0x533E00.
extern "C" void __cdecl Scena09_Set782ReloadJoin2(void) {
    Set(7, 8, 2);
    Reload();
    Join(2);
    Palettes();
}

// original 0x51A030 (0x2A bytes; 9 A[2], 11 A[0], 11 A[3], 12 A[0], 12 A[3]): PartySet_Load(7, 8, 5, 0); Field_PartyLoad(0); Party_Join(8); Party_Join(5); 0x533E00.
extern "C" void __cdecl ScenaCall_Set785ReloadJoin85(void) {
    Set(7, 8, 5);
    Reload();
    Join(8);
    Join(5);
    Palettes();
}

// original 0x51A060 (0x23 bytes; 9 A[3], 11 A[2], 11 A[5], 12 A[2], 12 A[5]): PartySet_Load(7, 8, 5, 0); Field_PartyLoad(0); Party_Join(5); 0x533E00.
extern "C" void __cdecl ScenaCall_Set785ReloadJoin5(void) {
    Set(7, 8, 5);
    Reload();
    Join(5);
    Palettes();
}

// original 0x51A090 (0x23 bytes; 9 A[4], 11 A[1], 12 A[1]): PartySet_Load(7, 8, 5, 0); Field_PartyLoad(0); Party_Join(8); 0x533E00.
extern "C" void __cdecl ScenaCall_Set785ReloadJoin8(void) {
    Set(7, 8, 5);
    Reload();
    Join(8);
    Palettes();
}

// original 0x51A0C0 (0x16 bytes; 9 A[5]): Field_PartyLoad(0); Party_Join(6); 0x533E00.
extern "C" void __cdecl Scena09_ReloadJoin6(void) {
    Reload();
    Join(6);
    Palettes();
}

// original 0x51A0E0 (0x2A bytes; 9 A[6], 11 A[6], 12 A[6]): PartySet_Load(7, 6, 5, 0); Field_PartyLoad(0); Party_Join(5); Party_Join(6); 0x533E00.
extern "C" void __cdecl ScenaCall_Set765ReloadJoin56(void) {
    Set(7, 6, 5);
    Reload();
    Join(5);
    Join(6);
    Palettes();
}

// original 0x51A110 (0x23 bytes; 9 A[7], 11 A[7], 12 A[7]): PartySet_Load(7, 6, 5, 0); Field_PartyLoad(0); Party_Join(6); 0x533E00.
extern "C" void __cdecl ScenaCall_Set765ReloadJoin6(void) {
    Set(7, 6, 5);
    Reload();
    Join(6);
    Palettes();
}

// original 0x51A140 (0x23 bytes; 9 A[8]): PartySet_Load(7, 6, 5, 0); Field_PartyLoad(0); Party_Join(5); 0x533E00.
extern "C" void __cdecl Scena09_Set765ReloadJoin5(void) {
    Set(7, 6, 5);
    Reload();
    Join(5);
    Palettes();
}

// original 0x51A170 (0x5C bytes; 9 A[9], 10 A[5]): the saved member - the last
// of the three saved ids that is neither 7 nor 2, else 8 - with 7 and 2; the
// three saved ids cleared on the way.
//
// As the original has it: the id is a byte in a pushed dword whose upper three
// bytes are the caller's ecx; PartySet_Load and Party_Join read only the byte.
extern "C" void __cdecl ScenaCall_Party72Saved(void) {
    unsigned char* const saved = At(::bof3::Img(at::kSaved));
    unsigned char k = 8;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char b = saved[i];
        // DIV-0228 SoloCharacter: a 0xFF a folded save left is no member
        // (solo_scenes.h); off, the original's test.
        if (b != 7 && b != 2 && !solo::Scene_SavedNone(b)) k = b;
        saved[i] = 0;
    }
    Set(7, 2, k);
    Field_MemberCount = 0;
    Join(7);
    Join(2);
    Join(k);
    Palettes();
}

// original 0x51A1D0 (0x23 bytes; 9 A[10]): PartySet_Load(7, 4, 8, 0); Field_PartyLoad(0); Party_Join(8); 0x533E00.
extern "C" void __cdecl Scena09_Set748ReloadJoin8(void) {
    Set(7, 4, 8);
    Reload();
    Join(8);
    Palettes();
}

// original 0x51A200 (0x2A bytes; 9 A[11]): PartySet_Load(7, 4, 8, 0); Field_PartyLoad(0); Party_Join(4); Party_Join(8); 0x533E00.
extern "C" void __cdecl Scena09_Set748ReloadJoin48(void) {
    Set(7, 4, 8);
    Reload();
    Join(4);
    Join(8);
    Palettes();
}

// original 0x51A230 (0x23 bytes; 9 A[12]): PartySet_Load(7, 4, 8, 0); Field_PartyLoad(0); Party_Join(4); 0x533E00.
extern "C" void __cdecl Scena09_Set748ReloadJoin4(void) {
    Set(7, 4, 8);
    Reload();
    Join(4);
    Palettes();
}

// original 0x51A260 (0x23 bytes; 9 A[13]): PartySet_Load(7, 4, 8, 0); Field_PartyLoad(0); Party_Join(7); 0x533E00.
extern "C" void __cdecl Scena09_Set748ReloadJoin7(void) {
    Set(7, 4, 8);
    Reload();
    Join(7);
    Palettes();
}

// original 0x51A290 (0x16 bytes; 9 B[1]): 0x534030(4); 0x534030(2); 0x533E00.
extern "C" void __cdecl Scena09_Leave42(void) {
    Leave(4);
    Leave(2);
    Palettes();
}

// original 0x51A2B0 (0x16 bytes; 9 B[3]): 0x534030(2); 0x534030(8); 0x533E00.
extern "C" void __cdecl Scena09_Leave28(void) {
    Leave(2);
    Leave(8);
    Palettes();
}

// original 0x51A2D0 (0x16 bytes; 9 B[4]): 0x534030(4); 0x534030(8); 0x533E00.
extern "C" void __cdecl Scena09_Leave48(void) {
    Leave(4);
    Leave(8);
    Palettes();
}

// original 0x51A2F0 (0xF bytes; 9 B[5], 11 B[4], 12 B[4]): 0x534030(8); 0x533E00.
extern "C" void __cdecl ScenaCall_Leave8(void) {
    Leave(8);
    Palettes();
}

// The caller's ecx (Scenario_CallA/B's n & 0xFF on both widths) as the
// body's argument: a pair by width (docs/x64-asm-policy.md 8 unit 4).
extern "C" __attribute__((naked)) void __cdecl ScenaCall_SaveAndLeave(void) {
#if defined(__i386__)
    __asm__ volatile(
        "pushl %ecx\n\t"
        "call " BOF3_SYM(scena_calls_SaveAndLeave) "\n\t"
        "addl $4, %esp\n\t"
        "ret");
#elif defined(__x86_64__)
    __asm__ volatile("jmp " BOF3_SYM(scena_calls_SaveAndLeave));   // the caller's ecx is the body's first argument already
#else
#error "no ecx entry for this architecture"
#endif
}

// original 0x51A360 (0xF bytes; 9 B[9]): 0x534030(7); 0x533E00.
extern "C" void __cdecl Scena09_Leave7(void) {
    Leave(7);
    Palettes();
}

void Scena09_Inject() {
    BOF3_INJECT(Scena09_Set782ReloadJoin8);
    BOF3_INJECT(Scena09_Set782ReloadJoin2);
    BOF3_INJECT(ScenaCall_Set785ReloadJoin85);
    BOF3_INJECT(ScenaCall_Set785ReloadJoin5);
    BOF3_INJECT(ScenaCall_Set785ReloadJoin8);
    BOF3_INJECT(Scena09_ReloadJoin6);
    BOF3_INJECT(ScenaCall_Set765ReloadJoin56);
    BOF3_INJECT(ScenaCall_Set765ReloadJoin6);
    BOF3_INJECT(Scena09_Set765ReloadJoin5);
    BOF3_INJECT(ScenaCall_Party72Saved);
    BOF3_INJECT(Scena09_Set748ReloadJoin8);
    BOF3_INJECT(Scena09_Set748ReloadJoin48);
    BOF3_INJECT(Scena09_Set748ReloadJoin4);
    BOF3_INJECT(Scena09_Set748ReloadJoin7);
    BOF3_INJECT(Scena09_Leave42);
    BOF3_INJECT(Scena09_Leave28);
    BOF3_INJECT(Scena09_Leave48);
    BOF3_INJECT(ScenaCall_Leave8);
    BOF3_INJECT(ScenaCall_SaveAndLeave);
    BOF3_INJECT(Scena09_Leave7);
}
