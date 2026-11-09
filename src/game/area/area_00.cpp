// Moved here by tools/move_funcs.py from area_w0a.cpp (docs/source-layout-round.md).
#include "game/area/area_00.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0a.h"
#include "game/area_w0a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0a::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

// The cursor row as a table index (movsx) and as a test (a byte).
int CursorRow() { return static_cast<signed char>(Mem(::bof3::Img(AreaW0c_ChoiceAnswer_addr))[0]); }

void SetMessage(unsigned v) { SetWord(Mem(::bof3::Img(at::kMessage.v)), v); }

}  // namespace

// original 0x401000 (area 0's choice 0, Area00_Choices[0]; PSX 0x801F2C04):
// no new message; row 0 sets variables 3 and 6 to 0x14, row 1 both to 0x19.
extern "C" void __cdecl Area00_ChoiceVars3And6(void) {
    const int row = CursorRow();
    SetMessage(0xFFFF);
    if (row == 0) {
        Mem(::bof3::Img(at::kVar3.v))[0] = 0x14;
        Mem(::bof3::Img(at::kVar6.v))[0] = 0x14;
    } else if (row == 1) {
        Mem(::bof3::Img(at::kVar3.v))[0] = 0x19;
        Mem(::bof3::Img(at::kVar6.v))[0] = 0x19;
    }
}

// original 0x401040 (area 0's choice 1; PSX 0x801F2C4C): no new message; row
// 0 sets variable 3 to 0x14, row 1 to 0x19.
extern "C" void __cdecl Area00_ChoiceVar3(void) {
    const int row = CursorRow();
    SetMessage(0xFFFF);
    if (row == 0) Mem(::bof3::Img(at::kVar3.v))[0] = 0x14;
    else if (row == 1) Mem(::bof3::Img(at::kVar3.v))[0] = 0x19;
}

// original 0x401070 (area 0's handler 0 and choice 2; PSX 0x801F2C8C):
// Sprite_Current[8] = Field_ActiveMember[0x85], then
// Sprite_FaceDirection(Sprite_Current[8]), Sprite_Current read again.
extern "C" void __cdecl Area00_FaceAsActiveMember(void) {
    Sprite_Current[8] = Field_ActiveMember[0x85];
    AH_CALL(Sprite_FaceDirection)(Sprite_Current[8]);
}

// original 0x4010A0 (area 0's handler 1 and choice 3; PSX 0x801F2CD8):
// MoveCmd_Move(MoveScript_Object, Sprite_Current[8]), then the script
// object's +7 = 2, MoveScript_Object read again.
extern "C" void __cdecl Area00_MoveFacing(void) {
    AH_CALL(MoveCmd_Move)(MoveScript_Object, Sprite_Current[8]);
    MoveScript_Object[7] = 2;
}

void Area00_Inject() {
    BOF3_INJECT(Area00_ChoiceVars3And6);
    BOF3_INJECT(Area00_ChoiceVar3);
    BOF3_INJECT(Area00_FaceAsActiveMember);
    BOF3_INJECT(Area00_MoveFacing);
}
