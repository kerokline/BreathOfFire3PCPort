// Moved here by tools/move_funcs.py from area_w0a.cpp (docs/source-layout-round.md).
#include "game/area/area_13.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
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

// The script position of MoveScript_Object (u16 +0xA), moved as the
// original's 16-bit add.
void MoveScriptPos(unsigned delta) {
    unsigned char* const object = MoveScript_Object;
    rv::Store<unsigned>(rv::As<records::ScriptContext>(object)->position, rv::As<records::ScriptContext>(object)->position + delta);
}

void SetCells(const unsigned char (*cells)[3], unsigned n) {
    for (unsigned i = 0; i < n; ++i) AH_CALL(AreaMap_SetByte)(cells[i][0], cells[i][1], cells[i][2]);
}

// The walk of area 13's handlers 2 and 3: the distance (wrapping 32-bit)
// made positive as cdq / xor / sub does (0x80000000 stays itself), divided
// by 0x8000 truncated toward 0, its low byte the step count at +7.
unsigned char Steps(std::uint32_t distance) {
    const auto d = static_cast<std::int32_t>(distance);
    const auto magnitude = static_cast<std::int32_t>(d < 0 ? 0u - distance : distance);
    return static_cast<unsigned char>(magnitude / 0x8000);
}

}  // namespace

// original 0x401860 (area 13's choice 0; PSX 0x801F2C04): the message
// Area13_ChoiceMessages[row]; row 0 sets variable 3 to 0xA, row 1 to 0x14.
extern "C" void __cdecl Area13_ChoiceMessageVar3(void) {
    const int row = CursorRow();
    SetMessage(MessageAt(::bof3::Img(Area13_ChoiceMessages_addr), row));
    if (row == 0) Mem(::bof3::Img(at::kVar3.v))[0] = 0xA;
    else if (row == 1) Mem(::bof3::Img(at::kVar3.v))[0] = 0x14;
}

// original 0x401890 (area 13's handler 0 and choice 1; PSX 0x801F2C54): four
// map cells, (0x39 and 0x3A, 3 and 2), cleared.
extern "C" void __cdecl Area13_ClearCells(void) {
    static const unsigned char kCells[][3] = {{0x39, 0x3, 0x0}, {0x3A, 0x3, 0x0}, {0x39, 0x2, 0x0}, {0x3A, 0x2, 0x0}};
    SetCells(kCells, 4);
}

// original 0x4018C0 (area 13's handler 1 and choice 2; PSX 0x801F2CAC):
// unless the leader's byte +0x89 is 8, the script position 0xC on.
extern "C" void __cdecl Area13_SkipUnlessLeader8(void) {
    if (Field_State[0x89] == 8) return;
    MoveScriptPos(0xC);
}

// original 0x4018E0 (area 13's handler 2 and choice 3; PSX 0x801F2CEC): the
// object walks along x to 0x1C8000: direction 3 when 0x1C8000 - x is not
// negative (32-bit, wrapping), else 7; the step count +7 of MoveScript_Object
// the distance / 0x8000; with steps, MoveCmd_Move(MoveScript_Object,
// Sprite_Current[8]).
extern "C" void __cdecl Area13_WalkToX(void) {
    unsigned char* const object = Sprite_Current;
    const std::uint32_t distance = 0x1C8000u - static_cast<std::uint32_t>(rv::As<records::SpriteHead>(object)->x);
    object[8] = static_cast<std::int32_t>(distance) >= 0 ? 3 : 7;
    MoveScript_Object[7] = Steps(distance);
    unsigned char* const script = MoveScript_Object;
    if (script[7] == 0) return;
    AH_CALL(MoveCmd_Move)(script, Sprite_Current[8]);
}

// original 0x401940 (area 13's handler 3 and choice 4; PSX 0x801F2D88): the
// object walks along z to 0x20000 in direction 1; the step count +7 the
// distance / 0x8000; with steps, MoveCmd_Move(MoveScript_Object,
// Sprite_Current[8]).
extern "C" void __cdecl Area13_WalkToZ(void) {
    unsigned char* const object = Sprite_Current;
    const std::uint32_t z = static_cast<std::uint32_t>(rv::As<records::SpriteHead>(object)->z);
    object[8] = 1;
    MoveScript_Object[7] = Steps(0x20000u - z);
    unsigned char* const script = MoveScript_Object;
    if (script[7] == 0) return;
    AH_CALL(MoveCmd_Move)(script, Sprite_Current[8]);
}

// original 0x401990 (area 13's handler 4 and choice 5; PSX 0x801F2E18):
// Sprite_Current[8] one direction on, of eight.
extern "C" void __cdecl Area13_TurnDirection(void) {
    Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] + 1) & 7);
}

void Area13_Inject() {
    BOF3_INJECT(Area13_ChoiceMessageVar3);
    BOF3_INJECT(Area13_ClearCells);
    BOF3_INJECT(Area13_SkipUnlessLeader8);
    BOF3_INJECT(Area13_WalkToX);
    BOF3_INJECT(Area13_WalkToZ);
    BOF3_INJECT(Area13_TurnDirection);
}
