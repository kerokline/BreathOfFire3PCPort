// Moved here by tools/move_funcs.py from area_w2d.cpp (docs/source-layout-round.md).
#include "game/area/area_103.h"

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

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// The tail armed: ScriptFlags_Set40, kind `kind`, state 0.
void ArmTail(unsigned char kind) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

}  // namespace

// original 0x4144E0 (area 103 +0x3C[0]; PSX 0x801F560C): the script object's
// word +8 = Area103_FirstListedMember + 2 (16 bits: none makes 0x101),
// Field_ObjectTalk(it), its bit 0x10 cleared; then its bit 0x20
// cleared when the running object's +7 has bit 4, else set.
extern "C" void __cdecl Area103_TalkByMember(void) {
    const unsigned char row = AH_CALL(Area103_FirstListedMember)();
    SetWord(MoveScript_Object + 8, static_cast<unsigned>(row) + 2);
    AH_CALL(Field_ObjectTalk)(MoveScript_Object);
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] & 0xEF);
    const unsigned char bits = Sprite_Current[7];
    unsigned char* const obj = MoveScript_Object;
    obj[0] = static_cast<unsigned char>((bits & 4) != 0 ? obj[0] & 0xDF : obj[0] | 0x20);
}

// original 0x414540 (called by Area103_TalkByMember): the first i of 0..2
// whose Area103_MemberKeys[i] is some member's byte +0x89 (members 0..
// Field_MemberCount - 1, the count read once), or 0xFF.
extern "C" unsigned char __cdecl Area103_FirstListedMember(void) {
    const unsigned char count = Field_MemberCount;
    for (unsigned i = 0; i < at::kArea103MemberKeyCount; ++i) {
        const unsigned char key = B(::bof3::Img(Area103_MemberKeys_addr) + i);
        for (unsigned m = 0; m < count; ++m)
            if (key == B(::bof3::Img(ObjTrio_addr) + m * at::kPartyStride + 0x89)) return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x4145B0 (area 103 +0x3C[1]; PSX 0x801F5738): Field_State +0x89
// not 0: the script position + 0x17.
extern "C" void __cdecl Area103_SkipIfLeader89(void) {
    if (Field_State[0x89] != 0) ScriptStep(0x17);
}

// original 0x4145D0 (area 103 +0x3C[2]; PSX 0x801F5778): Field_State +0x89
// 0: counter 0 + 1.
extern "C" void __cdecl Area103_CountIfLeader89Zero(void) {
    if (Field_State[0x89] == 0) B(::bof3::Img(at::kCounter0.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter0.v)) + 1);
}

// original 0x4145F0 (area 103 +0x3C[3]; PSX 0x801F57B4): MapView_Redraw 2,
// MapView_Elevation + Area103_Shake[Frame_Counter & 3] (signed) * 8.
extern "C" void __cdecl Area103_ShakeElevation(void) {
    const unsigned f = Frame_Counter & 3;
    const long now = MapView_Elevation;
    MapView_Redraw = 2;
    const auto step = static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(Area103_Shake_addr) + f)));
    MapView_Elevation = static_cast<long>(static_cast<U>(now) + static_cast<U>(step) * 8u);
}

// original 0x414620 (area 103 +0x3C[4]; PSX 0x801F57FC): the running object's
// word +0x3E = the ground at its (x, z) (the object read again).
extern "C" void __cdecl Area103_Ground(void) {
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
    const long ground = AH_CALL(AreaMap_Elevation)(tCur->x, tCur->z);
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, static_cast<unsigned>(ground));
}

// original 0x414640 (area 103 +0x3C[5]; PSX 0x801F5834): Effect_FindFree; a
// slot: kind 0x4D at the running object's +0x34 / +0x38 / +0x3C; none: the
// script position - 2.
extern "C" void __cdecl Area103_SpawnEffect4D(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) {
        ScriptStep(-2);
        return;
    }
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x4D;
    tE->x = tCur->x;
    tE->z = tCur->z;
    tE->height = tCur->height;
}

// original 0x4146A0 (Field_ObjectTriggers id 35; a gap of the tool, area
// 103's block): tail kind 44 (engine) armed with sub-kind 8. al 0.
extern "C" unsigned char __cdecl Area103_Trigger35(unsigned char*, unsigned char*) {
    ArmTail(0x2C);
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 8;
    return 0;
}

void Area103_Inject() {
    BOF3_INJECT(Area103_TalkByMember);
    BOF3_INJECT(Area103_FirstListedMember);
    BOF3_INJECT(Area103_SkipIfLeader89);
    BOF3_INJECT(Area103_CountIfLeader89Zero);
    BOF3_INJECT(Area103_ShakeElevation);
    BOF3_INJECT(Area103_Ground);
    BOF3_INJECT(Area103_SpawnEffect4D);
    BOF3_INJECT(Area103_Trigger35);
}
