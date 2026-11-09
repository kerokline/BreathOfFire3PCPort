// Moved here by tools/move_funcs.py from area_w3c.cpp (docs/source-layout-round.md).
#include "game/area/area_130.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3c.h"
#include "game/area_w3c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

unsigned char* PartyAt(unsigned member) { return rv::Bytes(&ObjTrio[member]); }

// The choice's answer as the choices read it with movsx.
int Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

// Effect_Spawn(kind, 0, args[party list `member`'s byte], party record
// `member`'s words +0x2E and +0x30) with Sprite_Current made that record (and
// left so); an answer other than 0xFF stored to Sprite_Current[0xB], read
// again after the call. The list byte indexes the table unchecked (it stays in
// .data). Area 94's handlers' shape (area_w2c.cpp's SpawnAt).
void SpawnAtMember(unsigned member, unsigned char kind, U args) {
    const unsigned char* const record = PartyAt(member);
    const auto* const tRecord = rv::AsConst<records::PartyMember>(record);
    const auto z = static_cast<short>(static_cast<std::uint16_t>(tRecord->screen_y));
    const auto x = static_cast<short>(static_cast<std::uint16_t>(tRecord->screen_x));
    const unsigned char effect = B(args + B(::bof3::Img(Cond_Flags_addr) + 0xd2 + member));
    Sprite_Current = PartyAt(member);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(effect), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// An effect record of `kind` at the running object (areas 130 and 134): a slot
// from Effect_FindFree, none (0xFF) nothing; else its +0 = 1, +5 = kind, +1 =
// 0 when `clear1`, and its x, z, y dwords (+0x34, +0x38, +0x3C) the running
// object's, Sprite_Current read again for each.
void EffectAtObject(unsigned char kind, bool clear1) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = kind;
    if (clear1) e[1] = 0;
    tE->x = rv::Current<records::SpriteHead>()->x;
    tE->z = rv::Current<records::SpriteHead>()->z;
    tE->height = rv::Current<records::SpriteHead>()->height;
}

}  // namespace

// original 0x41CCC0 (+0x34[0] = +0x3C[16], PSX 0x801F35B0): the message word
// Area130_ChoiceMessages[s8 answer] (unchecked); answer 0: the four counters
// 0, the step 0xA, MoveScript_Var7 6; answer 1: counter 0 = 0.
extern "C" void __cdecl Area130_ChoiceRunStep(void) {
    const int answer = Answer();
    SetMessage(Word(Mem(::bof3::Img(Area130_ChoiceMessages_addr) + static_cast<U>(answer) * 2u)));
    if (answer == 0) {
        B(::bof3::Img(at::kCounter0.v)) = 0;
        B(::bof3::Img(at::kCounter1.v)) = 0;
        B(::bof3::Img(at::kCounter2.v)) = 0;
        B(::bof3::Img(at::kCounter3.v)) = 0;
        B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0xA;
        B(::bof3::Img(MoveScript_Var7_addr)) = 6;
    } else if (answer == 1) {
        B(::bof3::Img(at::kCounter0.v)) = 0;
    }
}

// original 0x41CD10 (+0x3C[0], PSX 0x801F363C): kind 4 at record 0, args A.
extern "C" void __cdecl Area130_SpawnKind4AtMember0(void) { SpawnAtMember(0, 4, ::bof3::Img(Area130_EffectArgsA_addr)); }

// original 0x41CD60 (+0x3C[1], PSX 0x801F36BC): kind 3 at record 0, args B.
extern "C" void __cdecl Area130_SpawnKind3AtMember0(void) { SpawnAtMember(0, 3, ::bof3::Img(Area130_EffectArgsB_addr)); }

// original 0x41CDB0 (+0x3C[3], PSX 0x801F3760): Port_DroppedCall(1) (the PSX
// call the port dropped).
extern "C" void __cdecl Area130_DroppedCall1(void) { AH_CALL(Port_DroppedCall)(1); }

// original 0x41CDC0 (+0x3C[5], PSX 0x801F37B8): Port_DroppedCall(0), then
// Music_Track = 0x83.
extern "C" void __cdecl Area130_DroppedCallTrack83(void) {
    AH_CALL(Port_DroppedCall)(0);
    Music_Track = 0x83;
}

// original 0x41CDE0 (+0x3C[6], PSX 0x801F37F0): kind 3 at record 1, args B.
extern "C" void __cdecl Area130_SpawnKind3AtMember1(void) { SpawnAtMember(1, 3, ::bof3::Img(Area130_EffectArgsB_addr)); }

// original 0x41CE30 (+0x3C[7], PSX 0x801F3870): kind 3 at record 2, args B.
extern "C" void __cdecl Area130_SpawnKind3AtMember2(void) { SpawnAtMember(2, 3, ::bof3::Img(Area130_EffectArgsB_addr)); }

// original 0x41CE80 (+0x3C[8], PSX 0x801F38F0): an effect record of kind 0x78
// at the running object.
extern "C" void __cdecl Area130_Effect78AtObject(void) { EffectAtObject(0x78, false); }

// original 0x41CED0 (+0x3C[9], PSX 0x801F397C): kind 1 at record 1, args B.
extern "C" void __cdecl Area130_SpawnKind1AtMember1(void) { SpawnAtMember(1, 1, ::bof3::Img(Area130_EffectArgsB_addr)); }

// original 0x41CF20 (+0x3C[10], PSX 0x801F39FC): kind 1 at record 2, args B.
extern "C" void __cdecl Area130_SpawnKind1AtMember2(void) { SpawnAtMember(2, 1, ::bof3::Img(Area130_EffectArgsB_addr)); }

// original 0x41CF70 (+0x3C[11], PSX 0x801F3A7C): kind 4 at record 1, args C.
extern "C" void __cdecl Area130_SpawnKind4AtMember1(void) { SpawnAtMember(1, 4, ::bof3::Img(Area130_EffectArgsC_addr)); }

// original 0x41CFC0 (+0x3C[12], PSX 0x801F3AFC): kind 4 at record 2, args C.
extern "C" void __cdecl Area130_SpawnKind4AtMember2(void) { SpawnAtMember(2, 4, ::bof3::Img(Area130_EffectArgsC_addr)); }

// original 0x41D010 (+0x3C[13], PSX 0x801F3B7C): kind 5 at record 1, args C.
extern "C" void __cdecl Area130_SpawnKind5AtMember1(void) { SpawnAtMember(1, 5, ::bof3::Img(Area130_EffectArgsC_addr)); }

// original 0x41D060 (+0x3C[14], PSX 0x801F3BFC): kind 5 at record 2, args C.
extern "C" void __cdecl Area130_SpawnKind5AtMember2(void) { SpawnAtMember(2, 5, ::bof3::Img(Area130_EffectArgsC_addr)); }

// original 0x41D0B0 (+0x3C[15], PSX 0x801F3C7C): KeyItem_Add(0xA).
extern "C" void __cdecl Area130_GiveKeyItem10(void) { AH_CALL(KeyItem_Add)(0xA); }

// original 0x41D0C0 (Field_ObjectTriggers id 65): ScriptFlags_Set40, tail kind
// 63 (0x3F) with sub-kind 0; al 0. The tail's state is the step byte.
extern "C" unsigned char __cdecl Area130_Trigger65(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x3F;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0;
    return 0;
}

// original 0x41D0E0 (Field_ModeTailKinds[63]), by the step byte after
// MoveScript_Var7: 0 with Field_Request not 2 - the item at 0x903F6A's pick
// (0..5; anything else none): its name copied into Text_Records
// (strncpy(Text_Records, Item_NamePtr(category, item), 0x10)) and
// Inventory_Add(category, item, 1); then Text_Records +0x2F = 0, message 0x49,
// the request 2, the step 1. 1 with the request not 2: sound 0x106, story flag
// 0x98, ScriptFlags_Clear40, the tail kind and sub-kind 0. Else nothing.
extern "C" void __cdecl Area130_TailGiveItem(void) {
    static const unsigned char kItems[6][2] = {{0, 0xE}, {0, 7}, {0, 8}, {2, 0x15}, {1, 0x11}, {3, 0x19}};
    const unsigned char step = B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65);
    if (step == 0) {
        if (Field_Request == 2) return;
        const unsigned pick = B(::bof3::Img(AreaW3c_ItemPick_addr));
        if (pick <= 5) {
            const unsigned category = kItems[pick][0], item = kItems[pick][1];
            unsigned char* const name = AH_CALL(Item_NamePtr)(category, item);
            AH_CALL(Crt_strncpy)(
                reinterpret_cast<char*>(Mem(::bof3::Img(at::kTextRecords.v))), reinterpret_cast<const char*>(name), 0x10);
            AH_CALL(Inventory_Add)(category, item, 1);
        }
        B(::bof3::Img(at::kTextRecords2F.v)) = 0;
        AH_CALL(Msg_OpenScript)(0x49);
        Field_Request = 2;
        B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 1;
    } else if (step == 1) {
        if (Field_Request == 2) return;
        AH_CALL(Sound_PlayEffect)(0x106);
        AH_CALL(Flags_Set)(StoryFlags(), 0x98);
        AH_CALL(ScriptFlags_Clear40)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailSub_addr)) = 0;
    }
}

// original 0x41D270 (area 131 +0x34[1]; the choice 0 of world-map areas 16,
// 33, 45, 65, 87, 88, 115, 121, 151, 152, area 33's and 121's handlers 2 / 3,
// area 187's choice 1): answer 0: the tail state 2; the message 0xFFFF.
extern "C" void __cdecl Area130_ChoiceTailState2(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) == 0) B(::bof3::Img(AreaW0c_TailState_addr)) = 2;
    SetMessage(0xFFFF);
}

void Area130_Inject() {
    BOF3_INJECT(Area130_ChoiceRunStep);
    BOF3_INJECT(Area130_SpawnKind4AtMember0);
    BOF3_INJECT(Area130_SpawnKind3AtMember0);
    BOF3_INJECT(Area130_DroppedCall1);
    BOF3_INJECT(Area130_DroppedCallTrack83);
    BOF3_INJECT(Area130_SpawnKind3AtMember1);
    BOF3_INJECT(Area130_SpawnKind3AtMember2);
    BOF3_INJECT(Area130_Effect78AtObject);
    BOF3_INJECT(Area130_SpawnKind1AtMember1);
    BOF3_INJECT(Area130_SpawnKind1AtMember2);
    BOF3_INJECT(Area130_SpawnKind4AtMember1);
    BOF3_INJECT(Area130_SpawnKind4AtMember2);
    BOF3_INJECT(Area130_SpawnKind5AtMember1);
    BOF3_INJECT(Area130_SpawnKind5AtMember2);
    BOF3_INJECT(Area130_GiveKeyItem10);
    BOF3_INJECT(Area130_Trigger65);
    BOF3_INJECT(Area130_TailGiveItem);
    BOF3_INJECT(Area130_ChoiceTailState2);
}
