// Moved here by tools/move_funcs.py from area_w0a.cpp (docs/source-layout-round.md).
#include "game/area/area_10.h"

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

unsigned char* Effect(std::uint32_t slot_dword) { return Mem(::bof3::Img(Effect_Objects_addr) + (slot_dword & 0xFFu) * at::kEffectStride); }

}  // namespace

// original 0x4016A0 (area 5's choice 1, area 10's choice 1): no new message;
// row 0 sets variable 3 to 0xC8, row 1 to 0xC9.
extern "C" void __cdecl Area05_ChoiceVar3AtC8(void) {
    const int row = CursorRow();
    SetMessage(0xFFFF);
    if (row == 0) Mem(::bof3::Img(at::kVar3.v))[0] = 0xC8;
    else if (row == 1) Mem(::bof3::Img(at::kVar3.v))[0] = 0xC9;
}

// original 0x4016D0 / 0x4016F0 (area 10's choices 2 / 3): the message
// Area10_MessagesA / B[row].
extern "C" void __cdecl Area10_ChoiceMessageA(void) { SetMessage(MessageAt(::bof3::Img(Area10_MessagesA_addr), CursorRow())); }

extern "C" void __cdecl Area10_ChoiceMessageB(void) { SetMessage(MessageAt(::bof3::Img(Area10_MessagesB_addr), CursorRow())); }

// original 0x401710 (area 10's handler 0 and choice 4; PSX 0x801F2CDC): a
// free effect record of kind 0x1C (+0 1, +5 0x1C) at Sprite_Current's x and
// z (the dwords +0x34 and +0x38, Sprite_Current read for each).
extern "C" void __cdecl Area10_SpawnEffect1C(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = Effect(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x1C;
    tE->x = rv::Current<records::SpriteHead>()->x;
    tE->z = rv::Current<records::SpriteHead>()->z;
}

void Area10_Inject() {
    BOF3_INJECT(Area05_ChoiceVar3AtC8);
    BOF3_INJECT(Area10_ChoiceMessageA);
    BOF3_INJECT(Area10_ChoiceMessageB);
    BOF3_INJECT(Area10_SpawnEffect1C);
}
