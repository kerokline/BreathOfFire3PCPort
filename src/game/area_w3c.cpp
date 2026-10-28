// World 3's areas 124..125, 127..128 and 130..134: the PSX's
// BIN/WORLD03/AREA124..134.EMI compiled into the exe at 0x41C890..0x41DAD0
// (Area_Descriptors entries 124..134; areas 126 and 129 name no code). Round
// ten, group AR3C: the band's 56 functions, none ours before, each read to its
// last instruction with capstone (2026-09-28) and taken through the area
// harness (area_harness.h). docs/area_w3c.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept (they stay in
// .data); the band has no dispatch through a .data table of its own (its two
// jump tables are in .text, bounded). Every call goes through the harness
// (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for ours as
// for the originals' copies; the group's own callee (Area131_DisarmTail) is
// called the same way, so each function is fuzzed alone.
#include "game/area_w3c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3c::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }
void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }
// The dword at `cell` as a pointer.
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }
unsigned char* Focus() { return Ptr(::bof3::Img(AreaW1a_FocusObject_addr)); }
// Field object `index` (Sprite_Objects, 0xA4 bytes); effect record `slot`
// (Effect_Objects, 0x80 bytes); party record `member` (ObjTrio, 0x14C bytes).
unsigned char* ObjectAt(unsigned index) { return rv::Bytes(Sprite_Objects) + index * 0xA4u; }
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }
unsigned char* PartyAt(unsigned member) { return rv::Bytes(&ObjTrio[member]); }
// The choice's answer as the choices read it with movsx.
int Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }
// A float stored as the originals' `mov dword` of its bits.
void SetFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

// ---- areas 124 and 125 ----

// Areas 124 and 125's inits (0x41C890, 0x41C960, one body over two tables;
// areas 72 and 73's code compiled again): a weighted draw - Rand() & 0x3F
// walked down the eight weights, the first it falls below chosen (none: 8);
// field objects 0..7 but the chosen one get +0 = 0; the chosen one is put at
// one of the eight cells (Rand() & 7: x and z bytes << 16), its word +0x3E
// the ground there; Field_StepCount = the leader's dword +0x134 less 5 (either
// way).
void PlaceRandomObject(U cells, U weights) {
    auto roll = static_cast<unsigned char>(AH_CALL(Rand)() & 0x3F);
    unsigned chosen = 0;
    for (; chosen < 8; ++chosen) {
        const unsigned char weight = B(weights + chosen);
        if (roll < weight) break;
        roll = static_cast<unsigned char>(roll - weight);
    }
    for (unsigned k = 0; k < 8; ++k)
        if (k != chosen) ObjectAt(k)[0] = 0;
    if (chosen < 8) {
        const unsigned pick = static_cast<unsigned char>(AH_CALL(Rand)() & 7);
        unsigned char* const object = ObjectAt(chosen);
        auto* const tObject = rv::As<records::SpriteObject>(object);
        const auto x = static_cast<std::int32_t>(static_cast<U>(B(cells + pick * 2)) << 16);
        const auto z = static_cast<std::int32_t>(static_cast<U>(B(cells + pick * 2 + 1)) << 16);
        tObject->x = x;
        tObject->z = z;
        const long ground = AH_CALL(AreaMap_Elevation)(tObject->x, z);
        rv::Store<unsigned>(rv::As<records::SpriteObject>(object)->height_whole, static_cast<unsigned>(ground));
    }
    Field_StepCount = static_cast<unsigned short>(Long(Mem(::bof3::Img(ObjTrio_addr) + 0x134)) - 5);
}

// ---- the party-list spawns (areas 130, 133, 134) ----

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

// The patch chain the inits of areas 94 and 128 apply: from AreaMap_Header
// dword AreaMap_PatchBase (the dword at 0x8CB5A8 masked to its word), each
// entry not 0: AreaMap_ApplyPatch(entry), then on by (its dword, read again,
// >> 16) + 1 dwords; a zero dword ends it (none: the walk runs on).
void ApplyPatchChain() {
    U entry = ::bof3::Img(AreaMap_Header_addr) + (static_cast<U>(Long(Mem(::bof3::Img(AreaMap_PatchBase_addr)))) & 0xFFFF) * 4u;
    while (Long(Mem(entry)) != 0) {
        AH_CALL(AreaMap_ApplyPatch)(Mem(entry));
        entry += (static_cast<U>(Long(Mem(entry))) >> 16) * 4u + 4u;
    }
}

// Area 132's effects at a fixed cell: a slot from Effect_FindFree, none
// nothing; else its z +0x38 and x +0x34 set (and read back), +0 = 1, +5 =
// kind, the extra bytes `set` (+1 and +0xB, each when not negative), then
// +0x3C = (the ground at (x, z), sign-extended, + `lift`) << 16, and +0x34
// moved by `shift` (read again after the call; only when `shift` is not 0).
// Returns nothing; the caller's slot byte is not read after.
void EffectAtCell(unsigned char kind, std::int32_t x, std::int32_t z, int b1, int bB, std::int32_t lift, std::int32_t shift) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    tE->z = z;
    const std::int32_t zz = tE->z;
    tE->x = x;
    const std::int32_t xx = tE->x;
    e[0] = 1;
    e[5] = kind;
    if (b1 >= 0) e[1] = static_cast<unsigned char>(b1);
    if (bB >= 0) e[0xB] = static_cast<unsigned char>(bB);
    const long ground = AH_CALL(AreaMap_Elevation)(xx, zz);
    const U y = static_cast<U>(static_cast<std::int32_t>(static_cast<short>(ground)) + lift);
    if (shift != 0) {
        const U moved = static_cast<U>(tE->x) + static_cast<U>(shift);
        tE->height = static_cast<std::int32_t>(y << 16);
        tE->x = static_cast<std::int32_t>(moved);
    } else {
        tE->height = static_cast<std::int32_t>(y << 16);
    }
}

}  // namespace

// ===========================================================================
// Areas 124 and 125 (descriptors 0x6265C8, 0x626700; PSX 0x801F2DE0, 0x801F37F0)
// ===========================================================================

// original 0x41C890 (Area_Descriptors[124] +0x40; PSX 0x801F2D2C): a jmp over
// eleven nops to the body; PlaceRandomObject over Area124_Cells / _Weights.
extern "C" void __cdecl Area124_PlaceRandomObject(void) { PlaceRandomObject(::bof3::Img(Area124_Cells_addr), ::bof3::Img(Area124_Weights_addr)); }
// original 0x41C960 (Area_Descriptors[125] +0x40; PSX 0x801F362C): area 124's
// code over Area125_Cells / _Weights.
extern "C" void __cdecl Area125_PlaceRandomObject(void) { PlaceRandomObject(::bof3::Img(Area125_Cells_addr), ::bof3::Img(Area125_Weights_addr)); }

// ===========================================================================
// Area 127 (descriptor 0x626FE8; PSX 0x801F2D10)
// ===========================================================================

// original 0x41CA30 (+0x3C[0], PSX 0x801F2C04): Field_ActiveMember's byte
// +0x80 bit 0 cleared; the leader's byte +0x89 at 5: story flag 0x32,
// MoveCmd_TestFB(0x4E, 0x33), then Sprite_Current (read after the calls) +0 =
// 0.
extern "C" void __cdecl Area127_ClearActive80(void) {
    unsigned char* const member = Ptr(::bof3::Img(Field_ActiveMember_addr));
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(::bof3::Img(ObjTrio_addr) + 0x89) != 5) return;
    AH_CALL(Flags_Set)(StoryFlags(), 0x32);
    AH_CALL(MoveCmd_TestFB)(0x4E, 0x33);
    Sprite_Current[0] = 0;
}

// ===========================================================================
// Area 128 (descriptor 0x627A88; PSX 0x801F3DA8)
// ===========================================================================

// original 0x41CA70 (+0x34[0]): the answer read, the message 0xFFFF; answer 0:
// the focus object's word +0x8A = 0xB, counter 0 = 0xF and the step 3; else
// the step 8.
extern "C" void __cdecl Area128_ChoiceStep3(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer != 0) {
        B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 8;
        return;
    }
    SetWord(Focus() + 0x8A, 0xB);
    B(::bof3::Img(at::kCounter0.v)) = 0xF;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 3;
}

// original 0x41CAB0 (+0x34[1]): answer 0: ScriptFlags_Set40, tail kind 56
// (0x38) armed at state 0xA, message 0x49; else message 0xFFFF.
extern "C" void __cdecl Area128_ChoiceArmTail56(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0xFFFF);
        return;
    }
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x38;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0xA;
    SetMessage(0x49);
}

// original 0x41CAE0 (+0x34[2] = +0x3C[0], PSX 0x801F3030): the running
// object's x +0x34 = 0x4E8000, y +0x3C = 0; z +0x38 = 0x88000 when
// Field_State's byte +0x89 is 2, else 0x80000; +8 = 1. Sprite_Current is read
// again for every store.
extern "C" void __cdecl Area128_PlaceObject(void) {
    rv::Current<records::SpriteObject>()->x = 0x4E8000;
    rv::Current<records::SpriteObject>()->height = 0;
    if (Field_State[0x89] == 2)
        rv::Current<records::SpriteObject>()->z = 0x88000;
    else
        rv::Current<records::SpriteObject>()->z = 0x80000;
    Sprite_Current[8] = 1;
}

// original 0x41CB40 (Field_ModeTailKinds[56]): state 0 with Field_Request not
// 2: ScriptFlags_Clear40, Party_DropIn(6), story flag 0x88, the tail cleared
// (kind and state); state 0xA with the request not 2: ScriptFlags_Clear40,
// story flag 0x89, Field_ChangeArea(0x79, 0x2A0000, 0x1A0000, 7), the tail
// cleared; any other state nothing.
extern "C" void __cdecl Area128_TailLeave56(void) {
    const unsigned char state = B(::bof3::Img(AreaW0c_TailState_addr));
    if (state == 0) {
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Party_DropIn)(6);
        AH_CALL(Flags_Set)(StoryFlags(), 0x88);
    } else if (state == 0xA) {
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Flags_Set)(StoryFlags(), 0x89);
        AH_CALL(Field_ChangeArea)(0x79, 0x2A0000, 0x1A0000, 7);
    } else {
        return;
    }
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// original 0x41CBD0 (Field_ObjectTriggers id 61; object, flags): ScriptFlags_
// Set40, tail kind 56 at state 0, the object's word +0x8A + 1; al 0.
extern "C" unsigned char __cdecl Area128_Trigger61(unsigned char* object, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x38;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    AddWord(object + 0x8A, 1);
    return 0;
}

// original 0x41CC00 (+0x40, PSX 0x801F31D0): the previous area 0x79: story
// flag 0x43 set, 0x42 cleared, the patch chain applied. Then, the chapter
// (Cond_ByteFA, signed) above 10 and story flag 0x43 set: cells (0x1F, 0x39),
// (0x20, 0x39) = 0xC0 and (0x1F, 0x3A), (0x20, 0x3A) = 0xA1.
extern "C" void __cdecl Area128_InitPatches(void) {
    if (Word(Mem(::bof3::Img(AreaW2c_LastArea_addr))) == 0x79) {
        AH_CALL(Flags_Set)(StoryFlags(), 0x43);
        AH_CALL(Flags_Clear)(StoryFlags(), 0x42);
        ApplyPatchChain();
    }
    if (Cond_ByteFA <= 10) return;
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x43) == 0) return;
    AH_CALL(AreaMap_SetByte)(0x1F, 0x39, 0xC0);
    AH_CALL(AreaMap_SetByte)(0x20, 0x39, 0xC0);
    AH_CALL(AreaMap_SetByte)(0x1F, 0x3A, 0xA1);
    AH_CALL(AreaMap_SetByte)(0x20, 0x3A, 0xA1);
}

// ===========================================================================
// Area 130 (descriptor 0x628C80; PSX 0x801F500C)
// ===========================================================================

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

// ===========================================================================
// Area 131 (descriptor 0x629910; PSX 0x801F3CB8)
// ===========================================================================

// The talk scene the two choices open on the focus object: its +1 = 4, +0x84
// = 2, +0x83 = 4 / 2, word +0x8A = 0 (the focus pointer read again for each
// store after the first), with counter 0 = 0, MoveScript_Var7 7 and the step.
void FocusScene(unsigned char step, unsigned char b83) {
    unsigned char* const first = Focus();
    B(::bof3::Img(at::kCounter0.v)) = 0;
    B(::bof3::Img(MoveScript_Var7_addr)) = 7;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = step;
    first[1] = 4;
    Focus()[0x84] = 2;
    Focus()[0x83] = b83;
    SetWord(Focus() + 0x8A, 0);
}

// original 0x41D290 (+0x34[2]): answer 0: ScriptFlags_Set40, the focus scene
// at step 0xA (+0x83 = 4), message 0xC; else message 0xFFFF.
extern "C" void __cdecl Area131_ChoiceFocusStepA(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0xFFFF);
        return;
    }
    AH_CALL(ScriptFlags_Set40)();
    FocusScene(0xA, 4);
    SetMessage(0xC);
}

// original 0x41D300 (+0x34[3]): the answer read, the message 0xFFFF; answer 0:
// ScriptFlags_Set40, the focus scene at step 0 (+0x83 = 2).
extern "C" void __cdecl Area131_ChoiceFocusStep0(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    FocusScene(0, 2);
}

// original 0x41D360 (+0x3C[4], PSX 0x801F2E18): Camera_ShiftY - 0x1E, redraw.
extern "C" void __cdecl Area131_CameraShiftYLess1E(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 0x1E);
    MapView_Redraw = 2;
}
// original 0x41D370 (+0x3C[5], PSX 0x801F2E40): Camera_ShiftY + 0x1E, redraw.
extern "C" void __cdecl Area131_CameraShiftYMore1E(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 0x1E);
    MapView_Redraw = 2;
}

// original 0x41D380 (Field_ObjectTriggers id 15): tail kind 34 (0x22); al 0.
extern "C" unsigned char __cdecl Area131_Trigger15(unsigned char*, unsigned char*) {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x22;
    return 0;
}

// original 0x41D430 (called by tail kind 34 and by five of the engine's own
// tail phases, 0x456E29 .. 0x4575CE): ScriptFlags_Clear40, then the tail's
// kind, state and sub-kind 0.
extern "C" void __cdecl Area131_DisarmTail(void) {
    AH_CALL(ScriptFlags_Clear40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0;
}

// original 0x41D390 (Field_ModeTailKinds[34]), by the tail state (s8; a
// negative one is above 3): 0 - ScriptFlags_Set40, message 2, the request 2
// and the state (read after the call) + 1; 1 with the request not 2 - the
// tail disarmed; 2 with the request not 2 - state 3; 3 - Field_ScriptFlags ^
// 0x16, the tail disarmed, the byte 0x904152 0, story flag 0x77 cleared,
// Field_ChangeArea(0x79, 0x1A0000, 0x350000, 3).
extern "C" void __cdecl Area131_TailLeave34(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    switch (static_cast<U>(static_cast<std::int32_t>(state))) {
    case 0: {
        AH_CALL(ScriptFlags_Set40)();
        AH_CALL(Msg_OpenScript)(2);
        const auto next = static_cast<unsigned char>(B(::bof3::Img(AreaW0c_TailState_addr)) + 1);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = next;
        break;
    }
    case 1:
        if (Field_Request != 2) AH_CALL(Area131_DisarmTail)();
        break;
    case 2:
        if (Field_Request != 2) B(::bof3::Img(AreaW0c_TailState_addr)) = 3;
        break;
    case 3:
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ 0x16);
        AH_CALL(Area131_DisarmTail)();
        B(::bof3::Img(AreaW3c_CampFlag_addr)) = 0;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        AH_CALL(Field_ChangeArea)(0x79, 0x1A0000, 0x350000, 3);
        break;
    default: break;
    }
}

// ===========================================================================
// Area 132 (descriptor 0x62A598; PSX 0x801F4A44)
// ===========================================================================

// original 0x41D450 (+0x3C[0], PSX 0x801F3A24): the running object's +8 bit 4:
// animation 0x40, else 0x41.
extern "C" void __cdecl Area132_AnimByBit4(void) { AH_CALL(Sprite_SetAnimation)((Sprite_Current[8] & 4) ? 0x40 : 0x41); }

// original 0x41D470 (+0x3C[1], PSX 0x801F3A64): an effect record of kind 0x73
// at (0x2A0000, 0x1D0000), +1 = 0, on the ground.
extern "C" void __cdecl Area132_Effect73(void) { EffectAtCell(0x73, 0x2A0000, 0x1D0000, 0, -1, 0, 0); }

// original 0x41D4D0 (+0x3C[2], PSX 0x801F3AEC): two records of kind 0x73, at
// (0x2E0000, 0x1C0000) with +1 = 1, +0xB = 1 and at (0x2F0000, 0x1C0000) with
// +1 = 1, +0xB = 0; each 0x200 above the ground and moved 0x8000 back in x.
extern "C" void __cdecl Area132_Effect73Pair(void) {
    EffectAtCell(0x73, 0x2E0000, 0x1C0000, 1, 1, 0x200, -0x8000);
    EffectAtCell(0x73, 0x2F0000, 0x1C0000, 1, 0, 0x200, -0x8000);
}

// original 0x41D5B0 (+0x3C[3], PSX 0x801F3C1C): an effect record of kind 0x74
// at (0x2B0000, 0x280000), on the ground (+1 left as it was).
extern "C" void __cdecl Area132_Effect74(void) { EffectAtCell(0x74, 0x2B0000, 0x280000, -1, -1, 0, 0); }

// original 0x41D610 (+0x3C[4], PSX 0x801F3CA0): Cond_ByteFE = 0 (the gradient
// state below releases its record once it reads 0).
extern "C" void __cdecl Area132_ClearFE(void) { Cond_ByteFE = 0; }

// original 0x41D620 (EffectKind18_States[95], 0x6541E8; Sprite_Current the
// effect record): Cond_ByteFE 0: Effect_Release. Then, with Draw_PassFlags bit
// 2: Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, 0x95, 0), Gfx_CommitPrim(7, 0xC);
// at Gfx_PacketNext (read again) a POLY_G4: Gpu_SetPolyG4, Gpu_SetSemiTrans(0),
// corners (0, 0), (320, 0), (0, 200), (320, 200) as floats, the top two
// vertices' colour (0, 0xC8, 0xFF) and the bottom two's (0, 0, 0x20);
// Gfx_CommitPrim(7, 0x44). A screen-wide gradient.
extern "C" void __cdecl Area132_EffectGradient(void) {
    if (Cond_ByteFE == 0) AH_CALL(Effect_Release)();
    if ((Draw_PassFlags & 4) == 0) return;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    AH_CALL(Gfx_CommitPrim)(7, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyG4)(p);
    AH_CALL(Gpu_SetSemiTrans)(p, 0);
    SetFloat(p + 0x2C, 200.0f);
    SetFloat(p + 0x3C, 200.0f);
    p[0x15] = 0xC8;
    p[0x05] = 0xC8;
    p[0x16] = 0xFF;
    p[0x06] = 0xFF;
    // DIV-0041: the left corners at -53 and the right at 373 under the wide
    // picture (widescreen.h: 0 until every self-test has run).
    SetFloat(p + 0x08, Widescreen_FillX());
    SetLong(p + 0x0C, 0);
    SetFloat(p + 0x18, 320.0f + static_cast<float>(Widescreen_Fill()));
    SetLong(p + 0x1C, 0);
    SetFloat(p + 0x28, Widescreen_FillX());
    SetFloat(p + 0x38, 320.0f + static_cast<float>(Widescreen_Fill()));
    p[0x14] = 0;
    p[0x04] = 0;
    p[0x34] = 0;
    p[0x24] = 0;
    p[0x35] = 0;
    p[0x25] = 0;
    p[0x36] = 0x20;
    p[0x26] = 0x20;
    AH_CALL(Gfx_CommitPrim)(7, 0x44);
}

// ===========================================================================
// Area 133 (descriptor 0x62B5C0; PSX 0x801F3DAC)
// ===========================================================================

// original 0x41D6D0 (+0x34[0] = +0x3C[4], PSX 0x801F2C1C): the focus object's
// dwords +0x18 / +0x1C = the pair Area133_ChoicePairs[s8 answer] (bytes,
// unchecked; the focus pointer and the answer read again for each), the
// message 0xFFFF; answer 2: the byte 0x929F0F 0, ScriptFlags_Set40,
// Draw_PassFlags 0, the step 0x1E and MoveScript_Var7 4.
extern "C" void __cdecl Area133_ChoiceFocusPair(void) {
    unsigned char* const focus = Focus();
    const unsigned char first = B(::bof3::Img(Area133_ChoicePairs_addr) + static_cast<U>(Answer()) * 2u);
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, first);
    unsigned char* const again = Focus();
    SetLong(again + 0x1C, B(::bof3::Img(Area133_ChoicePairs_addr) + static_cast<U>(Answer()) * 2u + 1));
    if (Answer() != 2) return;
    B(::bof3::Img(AreaW3c_Load0F_addr)) = 0;
    AH_CALL(ScriptFlags_Set40)();
    Draw_PassFlags = 0;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0x1E;
    B(::bof3::Img(MoveScript_Var7_addr)) = 4;
}

// original 0x41D740 (+0x3C[0], PSX 0x801F2CEC): kind 3 at record 0,
// Area133_EffectArgs.
extern "C" void __cdecl Area133_SpawnKind3AtMember0(void) { SpawnAtMember(0, 3, ::bof3::Img(Area133_EffectArgs_addr)); }

// ===========================================================================
// Area 134 (descriptor 0x62C120; PSX 0x801F6374)
// ===========================================================================

// original 0x41D790 (+0x3C[0], PSX 0x801F5114): kind 4 at record 0, args B.
extern "C" void __cdecl Area134_SpawnKind4AtMember0(void) { SpawnAtMember(0, 4, ::bof3::Img(Area134_EffectArgsB_addr)); }

// original 0x41D7E0 (+0x3C[1], PSX 0x801F5194): Camera_ShiftX + 2, redraw.
extern "C" void __cdecl Area134_CameraShiftXMore2(void) {
    Camera_ShiftX = static_cast<short>(Camera_ShiftX + 2);
    MapView_Redraw = 2;
}
// original 0x41D800 (+0x3C[2], PSX 0x801F51BC): Camera_ShiftX - 2, redraw.
extern "C" void __cdecl Area134_CameraShiftXLess2(void) {
    Camera_ShiftX = static_cast<short>(Camera_ShiftX - 2);
    MapView_Redraw = 2;
}
// original 0x41D810 (+0x3C[3], PSX 0x801F51E4; also area 8's choice 7 =
// handler 6): Camera_ShiftX 0, redraw.
extern "C" void __cdecl Area134_CameraShiftXReset(void) {
    Camera_ShiftX = 0;
    MapView_Redraw = 2;
}

// original 0x41D830 (+0x3C[4], PSX 0x801F5200): kind 1 at record 0, args A.
extern "C" void __cdecl Area134_SpawnKind1AtMember0(void) { SpawnAtMember(0, 1, ::bof3::Img(Area134_EffectArgsA_addr)); }

// original 0x41D880 (+0x3C[5], PSX 0x801F5280): Camera_ShiftY + 8, redraw.
extern "C" void __cdecl Area134_CameraShiftYMore8(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 8);
    MapView_Redraw = 2;
}
// original 0x41D890 (+0x3C[6], PSX 0x801F52A8): Camera_ShiftY - 8, redraw.
extern "C" void __cdecl Area134_CameraShiftYLess8(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 8);
    MapView_Redraw = 2;
}

// original 0x41D8A0 (+0x3C[7], PSX 0x801F52D0): kind 3 at record 1, args A.
extern "C" void __cdecl Area134_SpawnKind3AtMember1(void) { SpawnAtMember(1, 3, ::bof3::Img(Area134_EffectArgsA_addr)); }
// original 0x41D8F0 (+0x3C[8], PSX 0x801F5350): kind 3 at record 2, args A.
extern "C" void __cdecl Area134_SpawnKind3AtMember2(void) { SpawnAtMember(2, 3, ::bof3::Img(Area134_EffectArgsA_addr)); }
// original 0x41D940 (+0x3C[10], PSX 0x801F53EC): kind 1 at record 1, args A.
extern "C" void __cdecl Area134_SpawnKind1AtMember1(void) { SpawnAtMember(1, 1, ::bof3::Img(Area134_EffectArgsA_addr)); }
// original 0x41D990 (+0x3C[11], PSX 0x801F546C): kind 1 at record 2, args A.
extern "C" void __cdecl Area134_SpawnKind1AtMember2(void) { SpawnAtMember(2, 1, ::bof3::Img(Area134_EffectArgsA_addr)); }

// original 0x41D9E0 (+0x3C[12] and [13], PSX 0x801F54EC and 0x801F557C): an
// effect record of kind 0x90 at the running object, +1 = 0.
extern "C" void __cdecl Area134_Effect90AtObject(void) { EffectAtObject(0x90, true); }
// original 0x41DA30 (+0x3C[14], PSX 0x801F560C): kind 0x93 at the running
// object.
extern "C" void __cdecl Area134_Effect93AtObject(void) { EffectAtObject(0x93, false); }
// original 0x41DA80 (+0x3C[15], PSX 0x801F5698): kind 0x99 at the running
// object.
extern "C" void __cdecl Area134_Effect99AtObject(void) { EffectAtObject(0x99, false); }

void AreaW3c_Inject() {
    if (bof3::WantsShadow("area_w3c")) area_w3c::SelfTest();
    BOF3_INJECT(Area124_PlaceRandomObject);
    BOF3_INJECT(Area125_PlaceRandomObject);
    BOF3_INJECT(Area127_ClearActive80);
    BOF3_INJECT(Area128_ChoiceStep3);
    BOF3_INJECT(Area128_ChoiceArmTail56);
    BOF3_INJECT(Area128_PlaceObject);
    BOF3_INJECT(Area128_TailLeave56);
    BOF3_INJECT(Area128_Trigger61);
    BOF3_INJECT(Area128_InitPatches);
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
    BOF3_INJECT(Area131_ChoiceFocusStepA);
    BOF3_INJECT(Area131_ChoiceFocusStep0);
    BOF3_INJECT(Area131_CameraShiftYLess1E);
    BOF3_INJECT(Area131_CameraShiftYMore1E);
    BOF3_INJECT(Area131_Trigger15);
    BOF3_INJECT(Area131_TailLeave34);
    BOF3_INJECT(Area131_DisarmTail);
    BOF3_INJECT(Area132_AnimByBit4);
    BOF3_INJECT(Area132_Effect73);
    BOF3_INJECT(Area132_Effect73Pair);
    BOF3_INJECT(Area132_Effect74);
    BOF3_INJECT(Area132_ClearFE);
    BOF3_INJECT(Area132_EffectGradient);
    BOF3_INJECT(Area133_ChoiceFocusPair);
    BOF3_INJECT(Area133_SpawnKind3AtMember0);
    BOF3_INJECT(Area134_SpawnKind4AtMember0);
    BOF3_INJECT(Area134_CameraShiftXMore2);
    BOF3_INJECT(Area134_CameraShiftXLess2);
    BOF3_INJECT(Area134_CameraShiftXReset);
    BOF3_INJECT(Area134_SpawnKind1AtMember0);
    BOF3_INJECT(Area134_CameraShiftYMore8);
    BOF3_INJECT(Area134_CameraShiftYLess8);
    BOF3_INJECT(Area134_SpawnKind3AtMember1);
    BOF3_INJECT(Area134_SpawnKind3AtMember2);
    BOF3_INJECT(Area134_SpawnKind1AtMember1);
    BOF3_INJECT(Area134_SpawnKind1AtMember2);
    BOF3_INJECT(Area134_Effect90AtObject);
    BOF3_INJECT(Area134_Effect93AtObject);
    BOF3_INJECT(Area134_Effect99AtObject);
}
