// Moved here by tools/move_funcs.py from area_w0a.cpp (docs/source-layout-round.md).
#include "game/area/area_08.h"

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

using move_script::Long;

using move_script::SetLong;

std::uint32_t Key(const void* p) { return Low32(p); }

// The index of an object from Sprite_Objects, as the original's magic
// multiply computes it: a signed 32-bit difference divided by 0xA4,
// truncated toward 0.
unsigned char ObjectIndex(const unsigned char* object) {
    const auto d = static_cast<std::int32_t>(Key(object) - ::bof3::Img(Sprite_Objects_addr));
    return static_cast<unsigned char>(d / 0xA4);
}

unsigned char* Effect(std::uint32_t slot_dword) { return Mem(::bof3::Img(Effect_Objects_addr) + (slot_dword & 0xFFu) * at::kEffectStride); }

void SetCells(const unsigned char (*cells)[3], unsigned n) {
    for (unsigned i = 0; i < n; ++i) AH_CALL(AreaMap_SetByte)(cells[i][0], cells[i][1], cells[i][2]);
}

}  // namespace

// original 0x401510 (area 8's handler 0 and choice 1; PSX 0x801F434C): three
// map cells set.
extern "C" void __cdecl Area08_SetCells(void) {
    static const unsigned char kCells[][3] = {{0x33, 0x15, 0x0}, {0x33, 0x16, 0x0}, {0x32, 0x16, 0x51}};
    SetCells(kCells, 3);
}

// original 0x401540 (PSX 0x801F4394) / 0x401570 (area 8's handlers 1 / 2 and choices 2 / 3;
// PSX 0x801F4394 / 0x801F43E0): Crt_sprintf(Text_Records,
// Area08_MessageFormat, variable 6 & 0x7F), then Msg_OpenScript 0x54 / 0x56
// and Field_Request 2; the second then Music_Play(0xA, 8).
extern "C" void __cdecl Area08_OpenMessage54(void) {
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(Mem(::bof3::Img(at::kTextRecords.v))),
                         reinterpret_cast<const char*>(Mem(::bof3::Img(at::kArea08MessageFormat.v))), Mem(::bof3::Img(at::kVar6.v))[0] & 0x7F);
    AH_CALL(Msg_OpenScript)(0x54);
    Field_Request = 2;
}

extern "C" void __cdecl Area08_OpenMessage56(void) {
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(Mem(::bof3::Img(at::kTextRecords.v))),
                         reinterpret_cast<const char*>(Mem(::bof3::Img(at::kArea08MessageFormat.v))), Mem(::bof3::Img(at::kVar6.v))[0] & 0x7F);
    AH_CALL(Msg_OpenScript)(0x56);
    Field_Request = 2;
    AH_CALL(Music_Play)(0xA, 8);
}

// original 0x4015B0 (area 8's handler 3 and choice 4; PSX 0x801F443C): a
// free effect record of kind 0x54 (+0 1, +5 0x54) whose +0xB is the index of
// Field_ActiveMember from Sprite_Objects, read after the call. The slot is
// kept at 0x903850 and read back from it.
extern "C" void __cdecl Area08_SpawnEffect54(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Mem(::bof3::Img(at::kEffectSlot.v))[0] = slot;
    if (slot == 0xFF) return;
    const unsigned char member = ObjectIndex(Field_ActiveMember);
    unsigned char* const e = Effect(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(at::kEffectSlot.v)))));
    e[0] = 1;
    e[5] = 0x54;
    e[0xB] = member;
}

// original 0x401600 / 0x401610 (area 8's handlers 4 / 5; PSX 0x801F4524 /
// 0x801F454C): Camera_ShiftX 0x14 more / less (a 16-bit add), MapView_Redraw 2.
extern "C" void __cdecl Area08_CameraShiftXMore(void) {
    Camera_ShiftX = static_cast<short>(Camera_ShiftX + 0x14);
    MapView_Redraw = 2;
}

extern "C" void __cdecl Area08_CameraShiftXLess(void) {
    Camera_ShiftX = static_cast<short>(Camera_ShiftX - 0x14);
    MapView_Redraw = 2;
}

// original 0x401620 / 0x401630 (area 8's handlers 10 / 11; PSX 0x801F45FC /
// 0x801F4618): field object 6's x (the dword Sprite_Objects + 6 * 0xA4 +
// 0x34) 0x800 more / less.
extern "C" void __cdecl Area08_Object6XMore(void) { SetLong(Mem(::bof3::Img(Sprite_Objects_addr) + 0x40c), static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Sprite_Objects_addr) + 0x40c))) + 0x800u)); }

extern "C" void __cdecl Area08_Object6XLess(void) { SetLong(Mem(::bof3::Img(Sprite_Objects_addr) + 0x40c), static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Sprite_Objects_addr) + 0x40c))) - 0x800u)); }

// original 0x401640 (PSX 0x801F4634; area 8's handler 12 and choice 13, area 3's handler 0;
// PSX 0x801F4634 as area 8's, 0x801F2D30 as area 3's): the four cells
// Area03_SetCells sets, cleared.
extern "C" void __cdecl Area08_ClearCells(void) {
    static const unsigned char kCells[][3] = {{0x55, 0x1C, 0x0}, {0x55, 0x1D, 0x0}, {0x54, 0x1C, 0x0}, {0x54, 0x1D, 0x0}};
    SetCells(kCells, 4);
}

void Area08_Inject() {
    BOF3_INJECT(Area08_SetCells);
    BOF3_INJECT(Area08_OpenMessage54);
    BOF3_INJECT(Area08_OpenMessage56);
    BOF3_INJECT(Area08_SpawnEffect54);
    BOF3_INJECT(Area08_CameraShiftXMore);
    BOF3_INJECT(Area08_CameraShiftXLess);
    BOF3_INJECT(Area08_Object6XMore);
    BOF3_INJECT(Area08_Object6XLess);
    BOF3_INJECT(Area08_ClearCells);
}
