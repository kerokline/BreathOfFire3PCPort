// Moved here by tools/move_funcs.py from area_w2c.cpp (docs/source-layout-round.md).
#include "game/area/area_94.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

// The dword at `cell` as a pointer.
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(::bof3::Img(Chapter_FlagRow_addr)); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// The choice's answer as the choices read it with movsx.
int Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

// A message word of a table indexed by the signed answer (unchecked).
unsigned MessageAt(U table, int answer) { return Word(Mem(table + static_cast<U>(answer) * 2u)); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The scene start the choices share: ScriptFlags_Set40, then the movement
// script's four counters 0, the run step byte after MoveScript_Var7 and
// MoveScript_Var7 itself.
void RunStep(unsigned char var7, unsigned char step) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(at::kCounter0.v)) = 0;
    B(::bof3::Img(at::kCounter1.v)) = 0;
    B(::bof3::Img(at::kCounter2.v)) = 0;
    B(::bof3::Img(at::kCounter3.v)) = 0;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = step;
    B(::bof3::Img(MoveScript_Var7_addr)) = var7;
}

// Area 94's spawns: Effect_Spawn(kind, 0, `args`[the list byte], the record's
// words +0x2E, +0x30) with Sprite_Current made the record (and left); a slot
// that is not 0xFF goes to its +0xB (Sprite_Current read again after the call).
void SpawnAt(U record, unsigned char kind, U args, U list) {
    const auto z = static_cast<short>(Word(Mem(record + 0x30)));
    const auto x = static_cast<short>(Word(Mem(record + 0x2E)));
    Sprite_Current = Mem(record);
    const auto b = static_cast<signed char>(B(args + B(list)));
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, b, x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

}  // namespace

// original 0x412FC0 (area 94 +0x34[0] = +0x3C[13], PSX 0x801F2C04): the
// message word of Area94_ChoiceMessages by the s8 answer.
extern "C" void __cdecl Area94_ChoiceMessage0(void) { SetMessage(MessageAt(::bof3::Img(Area94_ChoiceMessages_addr), Answer())); }

// original 0x412FE0 (+0x34[1] = +0x3C[14], PSX 0x801F2C30): the same, its
// second list.
extern "C" void __cdecl Area94_ChoiceMessage1(void) { SetMessage(MessageAt(::bof3::Img(Area94_ChoiceMessages_addr) + 0xc, Answer())); }

// original 0x413000 (+0x34[2] = +0x3C[15], PSX 0x801F2C5C): the message word;
// answer 1: the chapter row's flag 0x15 set.
extern "C" void __cdecl Area94_ChoiceSetFlag15(void) {
    const int answer = Answer();
    SetMessage(MessageAt(::bof3::Img(Area94_ChoiceMessages_addr) + 0x18, answer));
    if (answer == 1) AH_CALL(Flags_Set)(FlagRow(), 0x15);
}

// original 0x413030 (+0x34[3] = +0x3C[16], PSX 0x801F2CB4): the message word;
// answer 1: the row's flag 0x16 clear - set it (the row read again), the scene
// start with MoveScript_Var7 7, step 0; set - flag 0x1A clear: step 0x19, set:
// step 0x32.
extern "C" void __cdecl Area94_ChoiceRunByFlags16(void) {
    const int answer = Answer();
    SetMessage(MessageAt(::bof3::Img(Area94_ChoiceMessages_addr) + 0x1c, answer));
    if (answer != 1) return;
    if (AH_CALL(Flags_Test)(FlagRow(), 0x16) == 0) {
        AH_CALL(Flags_Set)(FlagRow(), 0x16);
        RunStep(7, 0);
    } else if (AH_CALL(Flags_Test)(FlagRow(), 0x1A) == 0) {
        RunStep(7, 0x19);
    } else {
        RunStep(7, 0x32);
    }
}

// original 0x413110 (+0x34[4] = +0x3C[17], PSX 0x801F2DC4): the message word;
// answer 1: the scene start with MoveScript_Var7 7, step 0x32; 2: the row's
// flag 0x20 clear - MoveScript_Var7 9, step 0; set - step 2.
extern "C" void __cdecl Area94_ChoiceRunByFlag20(void) {
    const int answer = Answer();
    SetMessage(MessageAt(::bof3::Img(Area94_ChoiceMessages_addr) + 0x20, answer));
    if (answer == 1) {
        RunStep(7, 0x32);
    } else if (answer == 2) {
        if (AH_CALL(Flags_Test)(FlagRow(), 0x20) == 0)
            RunStep(9, 0);
        else
            RunStep(9, 2);
    }
}

// original 0x4131D0 (+0x3C[0], PSX 0x801F2ED0): Effect_Spawn kind 2 at party
// record 0 by list 0, Area94_EffectArgsA.
extern "C" void __cdecl Area94_SpawnKind2AtMember0(void) { SpawnAt(::bof3::Img(ObjTrio_addr), 2, ::bof3::Img(Area94_EffectArgsA_addr), ::bof3::Img(Cond_Flags_addr) + 0xd2); }

// original 0x413220 (+0x3C[1], PSX 0x801F2F50): kind 3 at record 1, args A.
extern "C" void __cdecl Area94_SpawnKind3AtMember1(void) {
    SpawnAt(::bof3::Img(ObjTrio_addr) + at::kPartyStride, 3, ::bof3::Img(Area94_EffectArgsA_addr), ::bof3::Img(Cond_Flags_addr) + 0xd3);
}

// original 0x413270 (+0x3C[2], PSX 0x801F2FD0): kind 3 at record 2, args A.
extern "C" void __cdecl Area94_SpawnKind3AtMember2(void) {
    SpawnAt(::bof3::Img(ObjTrio_addr) + 2 * at::kPartyStride, 3, ::bof3::Img(Area94_EffectArgsA_addr), ::bof3::Img(Cond_Flags_addr) + 0xd4);
}

// original 0x4132C0 (+0x3C[3], PSX 0x801F3050): kind 4 at record 0, args B.
extern "C" void __cdecl Area94_SpawnKind4AtMember0(void) { SpawnAt(::bof3::Img(ObjTrio_addr), 4, ::bof3::Img(Area94_EffectArgsB_addr), ::bof3::Img(Cond_Flags_addr) + 0xd2); }

// original 0x413310 (+0x3C[4], PSX 0x801F30D0): the map cells (0x52, 0x32..0x34)
// zeroed.
extern "C" void __cdecl Area94_ClearCells(void) {
    AH_CALL(AreaMap_SetByte)(0x52, 0x32, 0);
    AH_CALL(AreaMap_SetByte)(0x52, 0x33, 0);
    AH_CALL(AreaMap_SetByte)(0x52, 0x34, 0);
}

// original 0x413340 (+0x3C[5], PSX 0x801F3118): kind 1 at record 1, args C.
extern "C" void __cdecl Area94_SpawnKind1AtMember1(void) {
    SpawnAt(::bof3::Img(ObjTrio_addr) + at::kPartyStride, 1, ::bof3::Img(Area94_EffectArgsC_addr), ::bof3::Img(Cond_Flags_addr) + 0xd3);
}

// original 0x413390 (+0x3C[6], PSX 0x801F3198): kind 1 at record 2, args C.
extern "C" void __cdecl Area94_SpawnKind1AtMember2(void) {
    SpawnAt(::bof3::Img(ObjTrio_addr) + 2 * at::kPartyStride, 1, ::bof3::Img(Area94_EffectArgsC_addr), ::bof3::Img(Cond_Flags_addr) + 0xd4);
}

// original 0x4133E0 (+0x3C[7], PSX 0x801F3218; also area 131 +0x3C[8] and area
// 133 +0x3C[3]): counter 1 = the leader's byte +8.
extern "C" void __cdecl Area94_Counter1FromLeaderPose(void) { B(::bof3::Img(at::kCounter1.v)) = B(::bof3::Img(ObjTrio_addr) + 0x8); }

// original 0x4133F0 (+0x3C[8], PSX 0x801F3230): kind 3 at record 0, args D.
extern "C" void __cdecl Area94_SpawnKind3AtMember0(void) { SpawnAt(::bof3::Img(ObjTrio_addr), 3, ::bof3::Img(Area94_EffectArgsD_addr), ::bof3::Img(Cond_Flags_addr) + 0xd2); }

// original 0x413440 (+0x3C[9], PSX 0x801F32B0): kind 4 at record 1, args B.
extern "C" void __cdecl Area94_SpawnKind4AtMember1(void) {
    SpawnAt(::bof3::Img(ObjTrio_addr) + at::kPartyStride, 4, ::bof3::Img(Area94_EffectArgsB_addr), ::bof3::Img(Cond_Flags_addr) + 0xd3);
}

// original 0x413490 (+0x3C[10], PSX 0x801F3330): kind 4 at record 2, args B.
extern "C" void __cdecl Area94_SpawnKind4AtMember2(void) {
    SpawnAt(::bof3::Img(ObjTrio_addr) + 2 * at::kPartyStride, 4, ::bof3::Img(Area94_EffectArgsB_addr), ::bof3::Img(Cond_Flags_addr) + 0xd4);
}

// original 0x4134E0 (+0x3C[11], PSX 0x801F33B0): KeyItem_Add(7), then the
// chapter row (read after the call) flag 0x1E.
extern "C" void __cdecl Area94_GiveKeyItem7(void) {
    AH_CALL(KeyItem_Add)(7);
    AH_CALL(Flags_Set)(FlagRow(), 0x1E);
}

// original 0x413500 (+0x3C[12], PSX 0x801F33E0): kind 4 at record 0, args E.
extern "C" void __cdecl Area94_SpawnKind4AtMember0E(void) { SpawnAt(::bof3::Img(ObjTrio_addr), 4, ::bof3::Img(Area94_EffectArgsE_addr), ::bof3::Img(Cond_Flags_addr) + 0xd2); }

// original 0x413550 (+0x40, PSX 0x801F3460): the previous area (u16 0x802290)
// 0x79: story flag 0x42 set, 0x43 cleared, then the patch chain applied - from
// AreaMap_Header dword AreaMap_PatchBase (the dword at 0x8CB5A8 masked to its
// word), each entry not 0: AreaMap_ApplyPatch(entry), then on by (its dword,
// read again, >> 16) + 1 dwords; a zero dword ends it (none: the walk runs on).
extern "C" void __cdecl Area94_InitPatches(void) {
    if (Word(Mem(::bof3::Img(AreaW2c_LastArea_addr))) != 0x79) return;
    AH_CALL(Flags_Set)(StoryFlags(), 0x42);
    AH_CALL(Flags_Clear)(StoryFlags(), 0x43);
    U entry = ::bof3::Img(AreaMap_Header_addr) + (static_cast<U>(Long(Mem(::bof3::Img(AreaMap_PatchBase_addr)))) & 0xFFFF) * 4u;
    while (Long(Mem(entry)) != 0) {
        AH_CALL(AreaMap_ApplyPatch)(Mem(entry));
        entry += (static_cast<U>(Long(Mem(entry))) >> 16) * 4u + 4u;
    }
}

void Area94_Inject() {
    BOF3_INJECT(Area94_ChoiceMessage0);
    BOF3_INJECT(Area94_ChoiceMessage1);
    BOF3_INJECT(Area94_ChoiceSetFlag15);
    BOF3_INJECT(Area94_ChoiceRunByFlags16);
    BOF3_INJECT(Area94_ChoiceRunByFlag20);
    BOF3_INJECT(Area94_SpawnKind2AtMember0);
    BOF3_INJECT(Area94_SpawnKind3AtMember1);
    BOF3_INJECT(Area94_SpawnKind3AtMember2);
    BOF3_INJECT(Area94_SpawnKind4AtMember0);
    BOF3_INJECT(Area94_ClearCells);
    BOF3_INJECT(Area94_SpawnKind1AtMember1);
    BOF3_INJECT(Area94_SpawnKind1AtMember2);
    BOF3_INJECT(Area94_Counter1FromLeaderPose);
    BOF3_INJECT(Area94_SpawnKind3AtMember0);
    BOF3_INJECT(Area94_SpawnKind4AtMember1);
    BOF3_INJECT(Area94_SpawnKind4AtMember2);
    BOF3_INJECT(Area94_GiveKeyItem7);
    BOF3_INJECT(Area94_SpawnKind4AtMember0E);
    BOF3_INJECT(Area94_InitPatches);
}
