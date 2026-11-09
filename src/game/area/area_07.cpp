// Moved here by tools/move_funcs.py from area_w0a.cpp (docs/source-layout-round.md).
#include "game/area/area_07.h"

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

using move_script::Word;

// The cursor row as a table index (movsx) and as a test (a byte).
int CursorRow() { return static_cast<signed char>(Mem(::bof3::Img(AreaW0c_ChoiceAnswer_addr))[0]); }

void SetMessage(unsigned v) { SetWord(Mem(::bof3::Img(at::kMessage.v)), v); }

// A u16 of an area's message table by the s8 cursor row, unchecked as the
// original's (a row outside the table reads its neighbours in the image).
unsigned MessageAt(std::uint32_t table, int row) {
    return Word(Mem(static_cast<std::uint32_t>(static_cast<std::int32_t>(table) + row * 2)));
}

void SetCells(const unsigned char (*cells)[3], unsigned n) {
    for (unsigned i = 0; i < n; ++i) AH_CALL(AreaMap_SetByte)(cells[i][0], cells[i][1], cells[i][2]);
}

}  // namespace

// original 0x4012D0 / 0x4012E0 / 0x4012F0 (area 7's handlers 1..3, area 19's
// 2..4; PSX 0x801F2C24 / 0x801F2C44 / 0x801F2C64 as area 7's): Kind2_Place
// with 1, 2, 3.
extern "C" void __cdecl Area07_PlaceKind2At1(void) { AH_CALL(Kind2_Place)(1); }

extern "C" void __cdecl Area07_PlaceKind2At2(void) { AH_CALL(Kind2_Place)(2); }

extern "C" void __cdecl Area07_PlaceKind2At3(void) { AH_CALL(Kind2_Place)(3); }

// original 0x401300 (area 7's handler 4; PSX 0x801F2C84): 38 map cells set,
// in this order - 0 on twelve, 0x50 on the twelve beside them, 0 on fourteen
// more.
extern "C" void __cdecl Area07_SetCells(void) {
    static const unsigned char kCells[][3] = {
        {0x12, 0x5, 0x0},   {0x12, 0x6, 0x0},   {0x12, 0xC, 0x0},   {0x12, 0xD, 0x0},   {0xE, 0x21, 0x0},
        {0xE, 0x22, 0x0},   {0x1B, 0x16, 0x0},  {0x1C, 0x16, 0x0},  {0x20, 0x16, 0x0},  {0x21, 0x16, 0x0},
        {0x20, 0x1E, 0x0},  {0x20, 0x1F, 0x0},  {0x11, 0x5, 0x50},  {0x11, 0x6, 0x50},  {0x11, 0xC, 0x50},
        {0x11, 0xD, 0x50},  {0xD, 0x21, 0x50},  {0xD, 0x22, 0x50},  {0x1B, 0x15, 0x50}, {0x1C, 0x15, 0x50},
        {0x20, 0x15, 0x50}, {0x21, 0x15, 0x50}, {0x1F, 0x1E, 0x50}, {0x1F, 0x1F, 0x50}, {0x1, 0x15, 0x0},
        {0x1, 0x14, 0x0},   {0x1, 0x13, 0x0},   {0x2, 0x15, 0x0},   {0x2, 0x14, 0x0},   {0x2, 0x13, 0x0},
        {0x15, 0x2, 0x0},   {0x16, 0x2, 0x0},   {0x15, 0x1, 0x0},   {0x16, 0x1, 0x0},   {0x26, 0x18, 0x0},
        {0x26, 0x19, 0x0},  {0x27, 0x18, 0x0},  {0x27, 0x19, 0x0},
    };
    SetCells(kCells, 38);
}

// original 0x4014C0 / 0x4014D0 (area 7's handlers 6 / 7, area 131's 0 / 1;
// PSX 0x801F2F20 / 0x801F2F48 as area 7's): Camera_ShiftY 10 less / more
// (a 16-bit add), MapView_Redraw 2.
extern "C" void __cdecl Area07_CameraShiftYLess(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 0xA);
    MapView_Redraw = 2;
}

extern "C" void __cdecl Area07_CameraShiftYMore(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 0xA);
    MapView_Redraw = 2;
}

// original 0x4014E0 (area 8's choice 0; PSX 0x801F42EC): the message
// Area08_ChoiceMessages[row]; row 0 sets variable 3 to 1, row 1 to 0.
extern "C" void __cdecl Area08_ChoiceMessageVar3(void) {
    const int row = CursorRow();
    SetMessage(MessageAt(::bof3::Img(Area08_ChoiceMessages_addr), row));
    if (row == 0) Mem(::bof3::Img(at::kVar3.v))[0] = 1;
    else if (row == 1) Mem(::bof3::Img(at::kVar3.v))[0] = 0;
}

void Area07_Inject() {
    BOF3_INJECT(Area07_PlaceKind2At1);
    BOF3_INJECT(Area07_PlaceKind2At2);
    BOF3_INJECT(Area07_PlaceKind2At3);
    BOF3_INJECT(Area07_SetCells);
    BOF3_INJECT(Area07_CameraShiftYLess);
    BOF3_INJECT(Area07_CameraShiftYMore);
    BOF3_INJECT(Area08_ChoiceMessageVar3);
}
