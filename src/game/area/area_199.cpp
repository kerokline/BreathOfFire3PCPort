// Moved here by tools/move_funcs.py from area_w4f.cpp (docs/source-layout-round.md).
#include "game/area/area_199.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4f.h"
#include "game/area_w4f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4f::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

}  // namespace

// original 0x42D6F0 (area 199 +0x34[0]): message 0xFFFF; MoveScript_Var7's
// step byte 1 for any answer but 0, else 0.
extern "C" void __cdecl Area199_ChoiceStepIfAnswer(void) {
    const unsigned char a = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = a != 0 ? 1 : 0;
}

void Area199_Inject() {
    BOF3_INJECT(Area199_ChoiceStepIfAnswer);
}
