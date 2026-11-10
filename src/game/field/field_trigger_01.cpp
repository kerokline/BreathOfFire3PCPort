// Moved here by tools/move_funcs.py from rest_4a.cpp (docs/source-layout-round.md).
#include "game/field/field_trigger_01.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_4a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_4a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

U UL(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U UL(U a) { return UL(move_script::At(a)); }

// The community's records and buildings, and the traits table by record.
unsigned char* Rec(U i) { return At(::bof3::Img(Community_Records_addr) + 8u * i); }

// The tail-kind triggers: the kind to 0x9039F3, the object to 0x939A38, a
// sub-kind (when given) to 0x9039F5, ScriptFlags_Set40, al 0.
unsigned char TailTrigger(unsigned char* object, unsigned char kind) {
    AreaW0a_TailKind = kind;
    Rest4d_Pointer = Low32(object);
    SH_CALL(ScriptFlags_Set40)();
    return 0;
}

unsigned char TailTriggerSub(unsigned char* object, unsigned char kind, unsigned char sub) {
    AreaW0a_TailKind = kind;
    Rest4d_Pointer = Low32(object);
    AreaW0c_TailSub = sub;
    SH_CALL(ScriptFlags_Set40)();
    return 0;
}

char* Row(U a) { return reinterpret_cast<char*>(At(a)); }

const char* Format() { return reinterpret_cast<const char*>(At(::bof3::Img(at::kNumberFormat.v))); }

}  // namespace

// original 0x456B20 (Field_ObjectTriggers id 1; no PSX twin paired): tail kind
// 0xE for the object.
extern "C" unsigned char __cdecl FieldTrigger01(unsigned char* object, unsigned char*) { return TailTrigger(object, 0xE); }

// original 0x456B40 (id 9): the shop record by the object's dword +0x1C
// (23-byte Shop_Records, unchecked) takes at +0 the level +3 of the building
// (1-based) of the community record the object's +5 names; al 0.
extern "C" unsigned char __cdecl FieldTrigger09(unsigned char* object, unsigned char*) {
    const unsigned char building = Rec(object[5])[1];
    const U shop = UL(object + 0x1C);
    At(::bof3::Img(at::kShopRecords.v) + at::kShopStride * shop)[0] = At(::bof3::Img(at::kBuildingLevels.v) + 8u * building)[0];
    return 0;
}

// original 0x456B70 (id 2): four numbers into Text_Records rows 0..3 by the
// format Area08_MessageFormat - the bytes 0x9040C8 and 0x9040C9, the dwords
// 0x904134 (the community's clock) and 0x904138 - then Msg_OpenScript(0x55)
// and Field_Request 2.
extern "C" unsigned char __cdecl FieldTrigger02(unsigned char*, unsigned char*) {
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow0.v)), Format(), static_cast<U>(At(::bof3::Img(at::kCountA.v))[0]));
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow1.v)), Format(), static_cast<std::uint32_t>(MapFieldObjects_ClockMinutes));
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow2.v)), Format(), Game_BattleCount);
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow3.v)), Format(), Rest4a_CountC);
    SH_CALL(Msg_OpenScript)(0x55);
    Field_Request = 2;
    return 0;
}

// original 0x456BF0 (id 3): three numbers - the dwords 0x90413C, 0x904140,
// 0x904144 - into rows 0..2, Msg_OpenScript(0x56), Field_Request 2.
extern "C" unsigned char __cdecl FieldTrigger03(unsigned char*, unsigned char*) {
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow0.v)), Format(), Rest4a_CountD);
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow1.v)), Format(), FieldE1_ContentCount);
    SH_CALL(Crt_sprintf)(Row(::bof3::Img(at::kTextRow2.v)), Format(), Rest4a_CountF);
    SH_CALL(Msg_OpenScript)(0x56);
    Field_Request = 2;
    return 0;
}

// original 0x456C50 (id 4): tail kind 0x15, sub-kind the object's +5.
extern "C" unsigned char __cdecl FieldTrigger04(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x15, object[5]); }

// original 0x456C70 (id 5): tail kind 0x16, sub-kind the object's +5.
extern "C" unsigned char __cdecl FieldTrigger05(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x16, object[5]); }

// original 0x456C90 (id 6): tail kind 0x17 (the sub-kind left).
extern "C" unsigned char __cdecl FieldTrigger06(unsigned char* object, unsigned char*) { return TailTrigger(object, 0x17); }

// original 0x456CB0 (id 7): tail kind 0x17, sub-kind 2.
extern "C" unsigned char __cdecl FieldTrigger07(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x17, 2); }

// original 0x456CD0 (id 8): tail kind 0x17, sub-kind 3.
extern "C" unsigned char __cdecl FieldTrigger08(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x17, 3); }

// original 0x456CF0 (id 10): tail kind 0x18, sub-kind the object's +5.
extern "C" unsigned char __cdecl FieldTrigger10(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x18, object[5]); }

// original 0x456D10 (id 61): tail kind 0x3C, sub-kind the object's +5.
extern "C" unsigned char __cdecl FieldTrigger61(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x3C, object[5]); }

// original 0x456D30 (id 11): tail kind 0x19, sub-kind the object's +5.
extern "C" unsigned char __cdecl FieldTrigger11(unsigned char* object, unsigned char*) { return TailTriggerSub(object, 0x19, object[5]); }

void FieldTrigger01_Inject() {
    BOF3_INJECT(FieldTrigger01);
    BOF3_INJECT(FieldTrigger09);
    BOF3_INJECT(FieldTrigger02);
    BOF3_INJECT(FieldTrigger03);
    BOF3_INJECT(FieldTrigger04);
    BOF3_INJECT(FieldTrigger05);
    BOF3_INJECT(FieldTrigger06);
    BOF3_INJECT(FieldTrigger07);
    BOF3_INJECT(FieldTrigger08);
    BOF3_INJECT(FieldTrigger10);
    BOF3_INJECT(FieldTrigger61);
    BOF3_INJECT(FieldTrigger11);
}
