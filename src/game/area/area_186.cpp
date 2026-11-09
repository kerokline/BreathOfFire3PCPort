// Moved here by tools/move_funcs.py from area_w4d.cpp (docs/source-layout-round.md).
#include "game/area/area_186.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4d.h"
#include "game/area_w4d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4d::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// Area 186's two walls (tail kind 39's states 0 and 0xA): with the map's
// focus at `focus`, the kind-2 field, the extra object 0 and Sprite_Kind2's z
// all moved by `dz`, MapView_Origin's second word by `dy`, the focus to
// `to`, and MapView_FillCells; then the timer down, and at 0 counter 3 0x1C,
// the timer 0xF and the next state.
void Area186Shift(std::int32_t focus, U dz, int dy, std::int32_t to, unsigned char next) {
    if (Long(Mem(::bof3::Img(MapView_FocusZ_addr))) == focus) {
        AddWord(Mem(::bof3::Img(MapView_Origin_addr) + 0x2), dy);
        AddLong(Mem(::bof3::Img(Field_Kind2Z_addr)), dz);
        AddLong(Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x38), dz);
        AddLong(Mem(::bof3::Img(Sprite_Kind2_addr) + 0x38), dz);
        SetLong(Mem(::bof3::Img(MapView_FocusZ_addr)), to);
        AH_CALL(MapView_FillCells)();
    }
    AddWord(Mem(::bof3::Img(at::kTailTimer.v)), -1);
    if (Word(Mem(::bof3::Img(at::kTailTimer.v))) != 0) return;
    B(::bof3::Img(at::kCounter3.v)) = 0x1C;
    SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0xF);
    B(::bof3::Img(AreaW0c_TailState_addr)) = next;
}

// Tail kind 39's states 1 and 0xB: the timer down, and at 0 area 0xAD at
// (x, z) with `flags`, the tail kind and state 0.
void Area186Leave(std::int32_t x, std::int32_t z, unsigned flags) {
    AddWord(Mem(::bof3::Img(at::kTailTimer.v)), -1);
    if (Word(Mem(::bof3::Img(at::kTailTimer.v))) != 0) return;
    AH_CALL(Field_ChangeArea)(0xAD, x, z, flags);
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

}  // namespace

// original 0x42A000 (Field_ModeTailKinds[39]): the s8 state through the byte
// table at 0x42A170 (12 entries) and the jump table at 0x42A15C (5), both in
// the body: 0 the wall shift down (focus 0x4400: z less 0x1E0000, the origin
// word less 0x1E, focus 0x6200) then state 1; 1 the leave to area 0xAD at
// (0x120000, 0x460000) flags 0x81; 0xA the shift up (focus 0x6C00: z plus,
// origin plus, focus 0x4E00) then state 0xB; 0xB the leave at (0x310000,
// 0x600000) flags 0x82; 2..9 and anything else (negative, above 0xB)
// nothing.
extern "C" void __cdecl Area186_TailShift(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0: Area186Shift(0x4400, static_cast<U>(-0x1E0000), -0x1E, 0x6200, 1); break;
    case 1: Area186Leave(0x120000, 0x460000, 0x81); break;
    case 0xA: Area186Shift(0x6C00, 0x1E0000, 0x1E, 0x4E00, 0xB); break;
    case 0xB: Area186Leave(0x310000, 0x600000, 0x82); break;
    default: break;
    }
}

// original 0x42A180 (init; PSX 0x801F2F24): story flag 0x58 set: the byte
// after Field_Kind2X 1: Area186_Start(0xA); 2: Area186_Start(0).
extern "C" void __cdecl Area186_Init(void) {
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x58) == 0) return;
    const unsigned char mode = B(::bof3::Img(AreaW1a_EntryZone_addr));
    if (mode == 1) AH_CALL(Area186_Start)(0xA);
    else if (mode == 2) AH_CALL(Area186_Start)(0);
}

// original 0x42A1C0 (called by Area186_Init; not in the PSX pairing): the
// camera angles (0x100, 0, 0) and distance 0x100; the extra object 0's x
// 0x48000; state 0: its z 0xA0000, +0x83 1, Kind2_Place(0); else z 0x640000,
// +0x83 2, Kind2_Place(1). Then the tail's state the argument, its timer
// 0x96, kind 39.
extern "C" void __cdecl Area186_Start(unsigned char state) {
    SetWord(Mem(::bof3::Img(Camera_Angles_addr)), 0x100);
    SetWord(Mem(::bof3::Img(Camera_Angles_addr) + 4), 0);
    SetWord(Mem(::bof3::Img(Camera_Angles_addr) + 2), 0);
    SetWord(Mem(::bof3::Img(Camera_Distance_addr)), 0x100);
    SetLong(Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x34), 0x48000);
    if (state == 0) {
        SetLong(Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x38), 0xA0000);
        B(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x83) = 1;
        AH_CALL(Kind2_Place)(0);
    } else {
        SetLong(Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x38), 0x640000);
        B(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x83) = 2;
        AH_CALL(Kind2_Place)(1);
    }
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
    SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x96);
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x27;
}

void Area186_Inject() {
    BOF3_INJECT(Area186_TailShift);
    BOF3_INJECT(Area186_Init);
    BOF3_INJECT(Area186_Start);
}
