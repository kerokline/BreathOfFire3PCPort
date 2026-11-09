// Moved here by tools/move_funcs.py from area_w2d.cpp (docs/source-layout-round.md).
#include "game/area/area_98.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2d.h"
#include "game/area_w2d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2d::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(::bof3::Img(Chapter_FlagRow_addr)); }

// The map byte setter, as the originals pass it: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// The tail armed: ScriptFlags_Set40, kind `kind`, state 0.
void ArmTail(unsigned char kind) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// The tail disarmed: ScriptFlags_Clear40, kind and state 0.
void Disarm() {
    AH_CALL(ScriptFlags_Clear40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

}  // namespace

// original 0x413A00 (area 98 +0x34[0] = +0x3C[1]; PSX 0x801F59F4): message
// 0xFFFF, flag 9 of 0x904654 set; then the answer (read after the call) 0:
// counter 1 = 0; else the chapter row's flag 0x1A and counter 1 = 1.
extern "C" void __cdecl Area98_ChoiceFlag9(void) {
    SetMessage(0xFFFF);
    AH_CALL(Flags_Set)(Mem(::bof3::Img(at::kFlags904654.v)), 9);
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) == 0) {
        B(::bof3::Img(at::kCounter1.v)) = 0;
        return;
    }
    AH_CALL(Flags_Set)(FlagRow(), 0x1A);
    B(::bof3::Img(at::kCounter1.v)) = 1;
}

// original 0x413A50 (area 98 +0x34[3], [4] = +0x3C[4], [5]; PSX 0x801F5AC8,
// 0x801F5B0C): an answer: message 0x2F and the mark 0x9398CF 6; 0: message
// 0xFFFF.
extern "C" void __cdecl Area98_ChoiceAsk2F(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0x2F);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x413A80 (area 98 +0x34[5] = +0x3C[6]; PSX 0x801F5B50): message
// 0xFFFF; answer 0: tail kind 45 armed.
extern "C" void __cdecl Area98_ChoiceArmTail45(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer == 0) ArmTail(0x2D);
}

// original 0x413AB0 (area 98 +0x3C[0]; PSX 0x801F5BA0): Effect_FindFree to
// the running object's +0xB; a slot (read back from +0xB for every store):
// kind 0x37 at the object's +0x34 / +0x38 / +0x3C, +0x2C 0x11D, +6 0, +7 0,
// +0x29 2, +9 1; Sound_PlayEffect(0x206).
extern "C" void __cdecl Area98_SpawnEffect37(void) {
    Sprite_Current[0xB] = AH_CALL(Effect_FindFree)();
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
    if (cur[0xB] == 0xFF) return;
    EffectAt(cur[0xB])[0] = 1;
    EffectAt(cur[0xB])[5] = 0x37;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(cur[0xB]))->x, tCur->x);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(cur[0xB]))->z, tCur->z);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(EffectAt(cur[0xB]))->height, tCur->height);
    rv::Store<unsigned>(rv::As<records::EffectObject>(EffectAt(cur[0xB]))->column, 0x11D);
    EffectAt(cur[0xB])[6] = 0;
    EffectAt(cur[0xB])[7] = 0;
    EffectAt(cur[0xB])[0x29] = 2;
    EffectAt(cur[0xB])[9] = 1;
    AH_CALL(Sound_PlayEffect)(0x206);
}

// original 0x413B90 (area 98 +0x40; PSX 0x801F5DD0): cells (4..7, 5) set to
// 0x50.
extern "C" void __cdecl Area98_InitCells(void) {
    for (unsigned i = 0; i < 4; ++i) SetByte(4 + i, 5, 0x50);
}

// original 0x413BC0 (Field_ModeTailKinds[45]): state 0 and Field_Request not
// 2: the return point = the leader's x, z and the area number;
// Field_ChangeArea(0x62, 0x380000, 0x2C0000, 1); the tail disarmed.
extern "C" void __cdecl Area98_TailChangeArea62(void) {
    if (B(::bof3::Img(AreaW0c_TailState_addr)) != 0 || Field_Request == 2) return;
    const std::int32_t x = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x34));
    const std::int32_t z = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x38));
    const unsigned short area = Game_AreaNumber;
    SetLong(Mem(::bof3::Img(AreaW2d_ReturnPoint_addr)), x);
    SetLong(Mem(::bof3::Img(AreaW2d_ReturnPoint_addr) + 4), z);
    SetWord(Mem(::bof3::Img(AreaW2d_ReturnPoint_addr) + 8), area);
    AH_CALL(Field_ChangeArea)(0x62, 0x380000, 0x2C0000, 1);
    Disarm();
}

// original 0x413C20 (Field_ObjectTriggers id 44; a gap of the tool, area 98's
// block): ScriptFlags_Set40; the focus object 0x903804 (read again for each)
// +1 = 4, +0x83 = 0, word +0x8A = 0; tail kind 4 (engine, 0x56D930) with
// sub-kind 9 and counter 3 the focus object's index in Sprite_Objects (a
// signed division by 0xA4). al 0.
extern "C" unsigned char __cdecl Area98_Trigger44(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    Ptr(::bof3::Img(AreaW1a_FocusObject_addr))[1] = 4;
    Ptr(::bof3::Img(AreaW1a_FocusObject_addr))[0x83] = 0;
    SetWord(Ptr(::bof3::Img(AreaW1a_FocusObject_addr)) + 0x8A, 0);
    const auto offset = static_cast<std::int32_t>(static_cast<U>(Long(Mem(::bof3::Img(AreaW1a_FocusObject_addr)))) - ::bof3::Img(Sprite_Objects_addr));
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 9;
    B(::bof3::Img(at::kCounter3.v)) = static_cast<unsigned char>(offset / 0xA4);
    return 0;
}

// original 0x413C80 (areas 39 +0x3C[1], 41 +0x34[10] = +0x3C[0], 99
// +0x3C[0]; PSX 0x801F2C04 in area 99): Camera_Distance 0xFF00,
// MapView_Redraw 2.
extern "C" void __cdecl Area98_CameraDistanceFF00(void) {
    Camera_Distance = static_cast<short>(0xFF00);
    MapView_Redraw = 2;
}

void Area98_Inject() {
    BOF3_INJECT(Area98_ChoiceFlag9);
    BOF3_INJECT(Area98_ChoiceAsk2F);
    BOF3_INJECT(Area98_ChoiceArmTail45);
    BOF3_INJECT(Area98_SpawnEffect37);
    BOF3_INJECT(Area98_InitCells);
    BOF3_INJECT(Area98_TailChangeArea62);
    BOF3_INJECT(Area98_Trigger44);
    BOF3_INJECT(Area98_CameraDistanceFF00);
}
