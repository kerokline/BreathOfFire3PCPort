// Moved here by tools/move_funcs.py from area_w2a.cpp (docs/source-layout-round.md).
#include "game/area/area_78.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2a::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

// The choice answer, as every choice of the band reads it: a signed byte.
signed char Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// A message word from a choice's table by the signed answer (unchecked: a
// negative answer reads before the table, a large one past it - in .data).
unsigned MessageAt(std::uint32_t table, signed char answer) {
    return Word(Mem(table + static_cast<std::uint32_t>(static_cast<int>(answer) * 2)));
}

// A free effect record of kind `kind` (Effect_FindFree; none: nothing), its
// +0 set 1 and +5 the kind; answers the record, or null for none.
unsigned char* SpawnEffectRecord(unsigned char kind) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return nullptr;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = kind;
    return e;
}

}  // namespace

// original 0x40F220 (Area78_Choices[7]): the message word from
// Area78_ChoiceMessages by the signed answer (unchecked); answer 0:
// ScriptFlags_Set40, the four counters 0, the byte and word after
// MoveScript_Var7 0, MoveScript_Var7 0xE; answer 1: counter 0 0.
extern "C" void __cdecl Area78_ChoiceResetScene(void) {
    const signed char answer = Answer();
    SetMessage(MessageAt(::bof3::Img(Area78_ChoiceMessages_addr), answer));
    if (answer == 0) {
        AH_CALL(ScriptFlags_Set40)();
        B(::bof3::Img(at::kCounter0.v)) = 0;
        B(::bof3::Img(at::kCounter1.v)) = 0;
        B(::bof3::Img(at::kCounter2.v)) = 0;
        B(::bof3::Img(at::kCounter3.v)) = 0;
        B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0;
        SetWord(Mem(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), 0);
        MoveScript_Var7 = 0xE;
    } else if (answer == 1) {
        B(::bof3::Img(at::kCounter0.v)) = 0;
    }
}

// original 0x40F280 (Area78_Choices[8] = Area78_Handlers[0], PSX 0x801F2E1C;
// area 80's choice 6 = handler 3, PSX 0x801F3D50): a free effect record of
// kind 0x44.
extern "C" void __cdecl Area78_SpawnEffect44(void) { SpawnEffectRecord(0x44); }

void Area78_Inject() {
    BOF3_INJECT(Area78_ChoiceResetScene);
    BOF3_INJECT(Area78_SpawnEffect44);
}
