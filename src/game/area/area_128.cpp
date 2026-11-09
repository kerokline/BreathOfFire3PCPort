// Moved here by tools/move_funcs.py from area_w3c.cpp (docs/source-layout-round.md).
#include "game/area/area_128.h"

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

using move_script::Long;

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

}  // namespace

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

void Area128_Inject() {
    BOF3_INJECT(Area128_ChoiceStep3);
    BOF3_INJECT(Area128_ChoiceArmTail56);
    BOF3_INJECT(Area128_PlaceObject);
    BOF3_INJECT(Area128_TailLeave56);
    BOF3_INJECT(Area128_Trigger61);
    BOF3_INJECT(Area128_InitPatches);
}
