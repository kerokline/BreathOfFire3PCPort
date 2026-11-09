// Moved here by tools/move_funcs.py from area_w0b.cpp (docs/source-layout-round.md).
#include "game/area/area_26.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0b.h"
#include "game/area_w0b_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char* Pointer(U cell) { return At(static_cast<U>(Long(At(cell)))); }

void SetMessage(unsigned id) { SetWord(At(::bof3::Img(at::kMessage.v)), id); }

signed char Choice() { return AreaW0c_ChoiceAnswer; }

// Areas 22's and 26's choice pattern: message 0xFFFF (none), then the script
// variable 0x903848 = `yes` for choice 0, `no` for choice 1, else unchanged.
void ChoiceSetVar(unsigned char yes, unsigned char no) {
    const signed char choice = Choice();
    SetMessage(0xFFFF);
    if (choice == 0) At(::bof3::Img(at::kScriptVar.v))[0] = yes;
    else if (choice == 1) At(::bof3::Img(at::kScriptVar.v))[0] = no;
}

unsigned char* FlagRow() { return Pointer(::bof3::Img(at::kFlagBase.v)); }

}  // namespace

// original 0x403120 (area 25's handler 0; PSX 0x801F2C04): on frames whose
// Frame_Counter & 3 is 0, Effect_FindFree to 0x903850; a slot (not 0xFF):
// its Effect_Objects record (the byte read back) +0 = 1, +5 = 0xB, the dword
// +0x1C = 6, +7 = 1 while the byte 0x90384A is below 0x14, else 3.
extern "C" void __cdecl Area25_SpawnEffect(void) {
    if ((Frame_Counter & 3) != 0) return;
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    at::kFoundSlot = slot;
    if (slot == 0xFF) return;
    const unsigned count = At(::bof3::Img(at::kEffectCount.v))[0];
    unsigned char* const record = rv::Bytes(Effect_Objects) + static_cast<std::uint32_t>(at::kFoundSlot) * 0x80u;
    record[0] = 1;
    record[5] = 0xB;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(record)->work_1c, 6);
    record[7] = count < 0x14 ? 1 : 3;
}

// original 0x403180 (area 26's choice 0, the descriptor 0x5EC5A0's +0x34; PSX 0x801F300C):
// message = Area26_Messages 0x5EC5E4 by the s8 choice (unchecked); the
// variable 0xBE (choice 0) or 0xBF (1).
extern "C" void __cdecl Area26_ChoiceMessage(void) {
    const signed char choice = Choice();
    SetMessage(Word(At(::bof3::Img(Area26_Messages_addr) + static_cast<U>(choice * 2))));
    if (choice == 0) At(::bof3::Img(at::kScriptVar.v))[0] = 0xBE;
    else if (choice == 1) At(::bof3::Img(at::kScriptVar.v))[0] = 0xBF;
}

// original 0x4031B0 (area 26's choice 1; PSX 0x801F305C): no message; the variable 8 or 0xF.
extern "C" void __cdecl Area26_ChoiceSetVar(void) { ChoiceSetVar(8, 0xF); }

// original 0x4031E0 (area 26's choice 2 and handler 0; PSX 0x801F309C):
// Flags_Set(row, 0x29), Flags_Clear(row read again, 0x28) - the row the
// pointer 0x929ED0 holds.
extern "C" void __cdecl Area26_Flag29Set28Clear(void) {
    AH_CALL(Flags_Set)(FlagRow(), 0x29);
    AH_CALL(Flags_Clear)(FlagRow(), 0x28);
}

// original 0x403200 (choice 3, handler 1; PSX 0x801F30D4): Flags_Clear 0x29.
extern "C" void __cdecl Area26_Flag29Clear(void) { AH_CALL(Flags_Clear)(FlagRow(), 0x29); }

// original 0x403220 (choice 4, handler 2; PSX 0x801F30FC): Flags_Set 0x28.
extern "C" void __cdecl Area26_Flag28Set(void) { AH_CALL(Flags_Set)(FlagRow(), 0x28); }

// original 0x403240 (choice 5, handler 3; PSX 0x801F3124): Flags_Clear 0x28.
extern "C" void __cdecl Area26_Flag28Clear(void) { AH_CALL(Flags_Clear)(FlagRow(), 0x28); }

// original 0x403260 (choice 6, handler 4; PSX 0x801F314C): Flags_Set 0x2B.
extern "C" void __cdecl Area26_Flag2BSet(void) { AH_CALL(Flags_Set)(FlagRow(), 0x2B); }

// original 0x403280 (choice 7, handler 5; PSX 0x801F3184): Flags_Clear 0x2B.
extern "C" void __cdecl Area26_Flag2BClear(void) { AH_CALL(Flags_Clear)(FlagRow(), 0x2B); }

// original 0x4032A0 (choice 8, handler 6; PSX 0x801F31AC): Flags_Set 0x2C.
extern "C" void __cdecl Area26_Flag2CSet(void) { AH_CALL(Flags_Set)(FlagRow(), 0x2C); }

// original 0x4032C0 (choice 9, handler 7; PSX 0x801F31D4): Flags_Clear 0x2C.
extern "C" void __cdecl Area26_Flag2CClear(void) { AH_CALL(Flags_Clear)(FlagRow(), 0x2C); }

// original 0x4032E0 (choice 10, handler 8; PSX 0x801F31FC): Inventory_Add(4,
// 3, 1) (a fourth word 0 pushed and not read), Sound_PlayEffect(0x106),
// Flags_Set(row, 0x33).
extern "C" void __cdecl Area26_GiveItem(void) {
    AH_CALL(Inventory_Add)(4, 3, 1);
    AH_CALL(Sound_PlayEffect)(0x106);
    AH_CALL(Flags_Set)(FlagRow(), 0x33);
}

// original 0x403310 (choice 13, handler 11; PSX 0x801F3290): Camera_ShiftY =
// 0, MapView_Redraw = 2, Music_FadeOutStop(0xA).
extern "C" void __cdecl Area26_ResetCamera(void) {
    Camera_ShiftY = 0;
    MapView_Redraw = 2;
    AH_CALL(Music_FadeOutStop)(0xA);
}

// original 0x403330 (choice 14, handler 12; PSX 0x801F32C8): as
// Area11_SpawnEffect with its own table - the leader's words +0x2E / +0x30
// read, Sprite_Current made the leader, Effect_Spawn(1, 0,
// Area26_EffectKinds[the first party list's first member], x, z); an answer
// other than 0xFF to Sprite_Current[0xB] (read again). The member id indexes
// the table unchecked.
extern "C" void __cdecl Area26_SpawnEffect(void) {
    const auto z = ObjTrio[0].screen_y;
    const auto x = ObjTrio[0].screen_x;
    const unsigned member = Cond_Flags[0xd2];
    Sprite_Current = At(::bof3::Img(ObjTrio_addr));
    const unsigned char kind = At(::bof3::Img(Area26_EffectKinds_addr) + member)[0];
    const unsigned char slot = AH_CALL(Effect_Spawn)(1, 0, static_cast<signed char>(kind), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// original 0x403380 (choice 15, handler 13; PSX 0x801F3370): Flags_Set 0x2E.
extern "C" void __cdecl Area26_Flag2ESet(void) { AH_CALL(Flags_Set)(FlagRow(), 0x2E); }

// original 0x4033A0 (choice 16, handler 14; PSX 0x801F3370): Effect_FindFree; a slot (not
// 0xFF): its Effect_Objects record +0x38 = 0x68000, +0x34 = 0x70000, +0 = 1,
// +5 = 0x33, then +0x3C = AreaMap_Elevation(0x70000, 0x68000) as a signed
// word << 16.
extern "C" void __cdecl Area26_PlaceEffect(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const record = rv::Bytes(Effect_Objects) + static_cast<U>(slot) * 0x80u;
    auto* const tRecord = rv::As<records::EffectObject>(record);
    tRecord->z = 0x68000;
    tRecord->x = 0x70000;
    record[0] = 1;
    record[5] = 0x33;
    const long height = AH_CALL(AreaMap_Elevation)(tRecord->x, tRecord->z);
    tRecord->height = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int16_t>(height)) << 16);
}

void Area26_Inject() {
    BOF3_INJECT(Area25_SpawnEffect);
    BOF3_INJECT(Area26_ChoiceMessage);
    BOF3_INJECT(Area26_ChoiceSetVar);
    BOF3_INJECT(Area26_Flag29Set28Clear);
    BOF3_INJECT(Area26_Flag29Clear);
    BOF3_INJECT(Area26_Flag28Set);
    BOF3_INJECT(Area26_Flag28Clear);
    BOF3_INJECT(Area26_Flag2BSet);
    BOF3_INJECT(Area26_Flag2BClear);
    BOF3_INJECT(Area26_Flag2CSet);
    BOF3_INJECT(Area26_Flag2CClear);
    BOF3_INJECT(Area26_GiveItem);
    BOF3_INJECT(Area26_ResetCamera);
    BOF3_INJECT(Area26_SpawnEffect);
    BOF3_INJECT(Area26_Flag2ESet);
    BOF3_INJECT(Area26_PlaceEffect);
}
