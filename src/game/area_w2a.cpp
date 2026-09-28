// World 2's areas 76..82 and 84: the PSX's BIN/WORLD02/AREA076..084.EMI
// compiled into the exe at 0x40EB90..0x40F720 (Area_Descriptors entries
// 76..84; area 83's descriptor names no code). Round ten, group AR2A: the
// band's 50 functions, none ours before, each read to its last instruction
// with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). docs/area_w2a.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept (they stay in
// .data); two indexes past a table abort where the original would jump
// through its neighbour or read past Area_Descriptors - area 79's object
// state and area 77's leap step (the project's rule, round9 doc section 6;
// docs/area_w2a.md section 6). Every call goes through the harness
// (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for ours as
// for the originals' copies; the callees of the group's own are called the
// same way, so each function is fuzzed alone.
#include "game/area_w2a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2a_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2a::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
// Party record `member` (ObjTrio, 0x14C bytes).
unsigned char* PartyAt(unsigned member) { return ObjTrio + member * at::kPartyStride; }
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(at::kFlagRow)))));
}

// The choice answer, as every choice of the band reads it: a signed byte.
signed char Answer() { return static_cast<signed char>(B(at::kChoiceAnswer)); }
void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
// A message word from a choice's table by the signed answer (unchecked: a
// negative answer reads before the table, a large one past it - in .data).
unsigned MessageAt(std::uint32_t table, signed char answer) {
    return Word(Mem(table + static_cast<std::uint32_t>(static_cast<int>(answer) * 2)));
}

// The band's commonest choice: no new message; answer 0 sets counter 0 to
// `on0`, answer 1 to `on1`, any other nothing.
void CountersByAnswer(signed char answer, unsigned char on0, unsigned char on1) {
    if (answer == 0) B(at::kCounter0) = on0;
    else if (answer == 1) B(at::kCounter0) = on1;
}
void ChoiceCounter0(unsigned char on0, unsigned char on1) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    CountersByAnswer(answer, on0, on1);
}
// The same with the message word from `table` by the answer.
void ChoiceMessageCounter0(std::uint32_t table, unsigned char on0, unsigned char on1) {
    const signed char answer = Answer();
    SetMessage(MessageAt(table, answer));
    CountersByAnswer(answer, on0, on1);
}

// The story-flag toggle nobody owns (area_w2a_callees.h).
void FlagsToggle(unsigned char* bits, unsigned index) {
    AH_AT(void (__cdecl*)(unsigned char*, unsigned), at::kFlagsToggle)(bits, index);
}

// Area 77's ten spawns (as area 49's SpawnAtMember, docs/area_w1c.md):
// Effect_Spawn(kind, 0, kinds[party list `member`'s first byte], party record
// `member`'s words +0x2E and +0x30) with Sprite_Current made that record (and
// left so); an answer other than 0xFF stored to Sprite_Current[0xB], read
// again after the call. The member byte indexes the list unchecked, as the
// original's (a list byte of 8 or more reads the next list).
void SpawnAtMember(unsigned member, unsigned char kind, std::uint32_t kinds) {
    const unsigned char* const record = PartyAt(member);
    const auto z = static_cast<short>(Word(record + 0x30));
    const auto x = static_cast<short>(Word(record + 0x2E));
    const unsigned char effect = B(kinds + B(at::kPartyList0 + member));
    Sprite_Current = PartyAt(member);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(effect), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// A free effect record of kind `kind` (Effect_FindFree; none: nothing), its
// +0 set 1 and +5 the kind; answers the record, or null for none.
unsigned char* SpawnEffectRecord(unsigned char kind) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return nullptr;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = kind;
    return e;
}

// Camera_ShiftY moved by `by`, MapView_Redraw 2.
void ShiftCamera(int by) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + by);
    MapView_Redraw = 2;
}

}  // namespace

// ===========================================================================
// Area 76 (descriptor 0x60AC78): no choices, handlers or init; one step hook.
// ===========================================================================

// original 0x40EB90 (Area_StepHook's case for area 0x4C, called with no
// arguments: it reads none, event_ops.cpp): with the mode tail kind at 5,
// the kind, its state and the byte 0x9039F1 cleared. Answers 0 in al.
extern "C" unsigned char __cdecl Area76_StepDisarmTail5(void) {
    if (B(at::kTailKind) == 5) {
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        B(at::kModeByte) = 0;
    }
    return 0;
}

// ===========================================================================
// Area 77 (descriptor 0x60BB88; PSX 0x801F6AD0): 23 choices, 21 handlers
// (choice k + 2 is handler k), a cell hook, an object trigger.
// ===========================================================================

// original 0x40EBB0 (Area_CellHooks' entry for area 0x4D, `(x, z)` answering
// in al): Cond_Flags row 6's flag 0x2C set: 0. The first of
// Area77_CellSwitches' two records whose x and z bytes are the cell's and
// whose pose is the leader's byte +8 (read after the flag test); none: 0.
// Story flag 0x1C set: 0. Else its story flag toggled (0x57C160),
// Sound_PlayEffect(0x201), 0x469FE0(0xF), answer 1.
extern "C" int __cdecl Area77_CellHook(long x, long z) {
    if (AH_CALL(Flags_Test)(Mem(at::kCondRow6), 0x2C)) return 0;
    const unsigned char pose = B(at::kLeaderPose);
    unsigned i = 0;
    for (; i < at::kArea77CellSwitchCount; ++i) {
        const unsigned char* const r = Mem(at::kArea77CellSwitches + i * 4u);
        if (static_cast<unsigned char>(x) == r[0] && static_cast<unsigned char>(z) == r[1] && r[2] == pose) break;
    }
    if (i == at::kArea77CellSwitchCount) return 0;
    if (AH_CALL(Flags_Test)(Mem(at::kStoryFlags), 0x1C)) return 0;
    FlagsToggle(Mem(at::kStoryFlags), B(at::kArea77CellSwitches + i * 4u + 3));
    AH_CALL(Sound_PlayEffect)(0x201);
    AH_AT(void (__cdecl*)(unsigned), at::kSpawnKind4)(0xF);
    return 1;
}

// original 0x40EC50 (Field_ObjectTriggers id 34, (object, flags) ignored):
// ScriptFlags_Set40, then the mode tail kind 0x2C with state 0 and sub-kind
// 7; answers 0 in al.
extern "C" unsigned char __cdecl Area77_Trigger34(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x2C;
    B(at::kTailState) = 0;
    B(at::kTailSub) = 7;
    return 0;
}

// original 0x40EC70 (Area77_Choices[0]): the message word from
// Area77_ChoiceMessages by the signed answer, unchecked.
extern "C" void __cdecl Area77_ChoiceMessage(void) { SetMessage(MessageAt(at::kArea77ChoiceMessages, Answer())); }

// original 0x40EC90 (Area77_Choices[1]): no new message; answer 0 sets
// counter 0 to 1, answer 1 to 0.
extern "C" void __cdecl Area77_ChoiceCounter0(void) { ChoiceCounter0(1, 0); }

// original 0x40ECC0 (Area77_Choices[2] = Area77_Handlers[0]; PSX
// 0x801F5A58): Cond_ByteFE 1, Sound_PlayEffect(0x202).
extern "C" void __cdecl Area77_SetByteFE(void) {
    Cond_ByteFE = 1;
    AH_CALL(Sound_PlayEffect)(0x202);
}

// original 0x40ECE0 (Area77_Handlers[1]; PSX 0x801F5A84): Cond_ByteFE 0,
// Sound_PlayEffect(0x202).
extern "C" void __cdecl Area77_ClearByteFE(void) {
    Cond_ByteFE = 0;
    AH_CALL(Sound_PlayEffect)(0x202);
}

// originals 0x40ED00, 0x40ED50, 0x40EDD0, 0x40EE20, 0x40EE70 (handlers 2, 3,
// 6, 7, 8; PSX 0x801F5AAC, 0x801F5B2C, 0x801F5C00, 0x801F5C80, 0x801F5D00):
// SpawnAtMember with lists A..E.
extern "C" void __cdecl Area77_Spawn3AtMember0ListA(void) { SpawnAtMember(0, 3, at::kArea77EffectKindsA); }
extern "C" void __cdecl Area77_Spawn4AtMember0ListB(void) { SpawnAtMember(0, 4, at::kArea77EffectKindsB); }
extern "C" void __cdecl Area77_Spawn2AtMember0ListC(void) { SpawnAtMember(0, 2, at::kArea77EffectKindsC); }
extern "C" void __cdecl Area77_Spawn3AtMember1ListD(void) { SpawnAtMember(1, 3, at::kArea77EffectKindsD); }
extern "C" void __cdecl Area77_Spawn3AtMember2ListE(void) { SpawnAtMember(2, 3, at::kArea77EffectKindsE); }

// original 0x40EDA0 (Area77_Handlers[4]; PSX 0x801F5BAC): Inventory_Remove(1,
// 0x47, 1) - the original pushes a fourth word, 0, that the callee never reads.
extern "C" void __cdecl Area77_RemoveItem47(void) { AH_CALL(Inventory_Remove)(1, 0x47, 1); }

// original 0x40EDC0 (Area77_Handlers[5]; PSX 0x801F5BD8): story flag 4 set.
extern "C" void __cdecl Area77_SetFlag4(void) { AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 4); }

// original 0x40EEC0 (Area77_Handlers[9]; PSX 0x801F5D80): Camera_ShiftY -
// 0x14, MapView_Redraw 2, Sound_PlayEffect(0x200).
extern "C" void __cdecl Area77_ShiftCameraDownSound(void) {
    ShiftCamera(-0x14);
    AH_CALL(Sound_PlayEffect)(0x200);
}

// originals 0x40EEE0, 0x40EF30, 0x40EF80 (handlers 12, 13, 14; PSX
// 0x801F5E04, 0x801F5E84, 0x801F5F04): SpawnAtMember kind 4 for members 1,
// 2, 0, all three by list 0.
extern "C" void __cdecl Area77_Spawn4AtMember1List0(void) { SpawnAtMember(1, 4, at::kArea77EffectKinds0); }
extern "C" void __cdecl Area77_Spawn4AtMember2List0(void) { SpawnAtMember(2, 4, at::kArea77EffectKinds0); }
extern "C" void __cdecl Area77_Spawn4AtMember0List0(void) { SpawnAtMember(0, 4, at::kArea77EffectKinds0); }

// originals 0x40EFD0, 0x40F020 (handlers 15, 16; PSX 0x801F5F84,
// 0x801F6004): SpawnAtMember for member 0 by lists F and G.
extern "C" void __cdecl Area77_Spawn1AtMember0ListF(void) { SpawnAtMember(0, 1, at::kArea77EffectKindsF); }
extern "C" void __cdecl Area77_Spawn3AtMember0ListG(void) { SpawnAtMember(0, 3, at::kArea77EffectKindsG); }

// original 0x40F140 (called by handlers 17 and 18): a free effect record of
// kind `kind` (its low byte), +0 = 1. The answer is unread.
extern "C" void __cdecl Area77_SpawnEffectKind(unsigned kind) { SpawnEffectRecord(static_cast<unsigned char>(kind)); }

// originals 0x40F070, 0x40F080 (handlers 17, 18; PSX 0x801F6084,
// 0x801F60A4): Area77_SpawnEffectKind(0x4A), (0x4C).
extern "C" void __cdecl Area77_SpawnEffect4A(void) { AH_CALL(Area77_SpawnEffectKind)(0x4A); }
extern "C" void __cdecl Area77_SpawnEffect4C(void) { AH_CALL(Area77_SpawnEffectKind)(0x4C); }

// original 0x40F090 (Area77_Handlers[19]; PSX 0x801F60C4): one step of a
// leap by the movement script. The script's operand byte - the byte two past
// the object's position word +0xA in the running area's +0x10 script
// [object +3] - picks a (dx, dz) pair of Area77_Leaps; Scena06_Leap(the
// script object, dx, dz, 0x40, -0x400, 4, 0). Not 0: the script's position
// back 2 (the op runs again next frame). 0: at operand 4 the running
// object's +0x3E is the ground under it (AreaMap_Elevation) and
// Sprite_SetAnimation(0); then the position on 1. The dx and dz dwords carry
// what the original's registers held above the byte (dx: the pair's offset,
// operand * 2; dz: the script table's address), which Scena06_Leap reads as
// s8 only. The script table and the script are indexed unchecked, as the
// movement-script engine indexes them; a running area past Area_Descriptors'
// 200 aborts where the original reads past the table.
extern "C" void __cdecl Area77_LeapStep(void) {
    unsigned char* const object = MoveScript_Object;
    const unsigned area = Game_AreaNumber;
    if (area >= at::kDescriptorCount)
        bof3::Fatal("Area77_LeapStep: Game_AreaNumber %u past Area_Descriptors' %u", area, at::kDescriptorCount);
    const std::uint32_t descriptor = static_cast<std::uint32_t>(Long(Mem(at::kDescriptors + area * 4)));
    const unsigned position = Word(object + 0xA);
    const std::uint32_t scripts = static_cast<std::uint32_t>(Long(Mem(descriptor + 0x10)));
    const std::uint32_t script = static_cast<std::uint32_t>(Long(Mem(scripts + object[3] * 4u)));
    const unsigned char operand = B(script + position + 2);
    const std::uint32_t pair = operand * 2u;
    const std::uint32_t dx = (pair & 0xFFFFFF00u) | B(at::kArea77Leaps + pair);
    const std::uint32_t dz = (scripts & 0xFFFFFF00u) | B(at::kArea77Leaps + pair + 1);
    if (AH_CALL(Scena06_Leap)(object, dx, dz, 0x40, 0xFFFFFC00u, 4, 0)) {
        AddWord(MoveScript_Object + 0xA, -2);
        return;
    }
    if (operand == 4) {
        const unsigned char* const cur = Sprite_Current;
        const long ground = AH_CALL(AreaMap_Elevation)(Long(cur + 0x34), Long(cur + 0x38));
        SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground));
        AH_CALL(Sprite_SetAnimation)(0);
    }
    AddWord(MoveScript_Object + 0xA, 1);
}

// ===========================================================================
// Area 78 (descriptor 0x60CEF0; PSX 0x801F4140): nine choices, one handler
// (choice 8).
// ===========================================================================

// original 0x40F170 (Area78_Choices[0]): no new message; answer 0: counter 0
// 0x1E and flag 0x16 of the chapter's row; answer 1: counter 0 0x1E and flag
// 0x17.
extern "C" void __cdecl Area78_ChoiceRowFlag16or17(void) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    if (answer != 0 && answer != 1) return;
    unsigned char* const row = FlagRow();
    B(at::kCounter0) = 0x1E;
    AH_CALL(Flags_Set)(row, answer == 0 ? 0x16 : 0x17);
}

// original 0x40F1C0 (Area78_Choices[3], [4]): counter 0 0x2B for answer 0,
// 0x2C for 1.
extern "C" void __cdecl Area78_ChoiceCounter2Bor2C(void) { ChoiceCounter0(0x2B, 0x2C); }

// original 0x40F1F0 (Area78_Choices[5], [6]): counter 0 0xA for answer 0, 2
// for 1.
extern "C" void __cdecl Area78_ChoiceCounterAor2(void) { ChoiceCounter0(0xA, 2); }

// original 0x40F220 (Area78_Choices[7]): the message word from
// Area78_ChoiceMessages by the signed answer (unchecked); answer 0:
// ScriptFlags_Set40, the four counters 0, the byte and word after
// MoveScript_Var7 0, MoveScript_Var7 0xE; answer 1: counter 0 0.
extern "C" void __cdecl Area78_ChoiceResetScene(void) {
    const signed char answer = Answer();
    SetMessage(MessageAt(at::kArea78ChoiceMessages, answer));
    if (answer == 0) {
        AH_CALL(ScriptFlags_Set40)();
        B(at::kCounter0) = 0;
        B(at::kCounter1) = 0;
        B(at::kCounter2) = 0;
        B(at::kCounter3) = 0;
        B(at::kVar7Step) = 0;
        SetWord(Mem(at::kVar7Word), 0);
        MoveScript_Var7 = 0xE;
    } else if (answer == 1) {
        B(at::kCounter0) = 0;
    }
}

// original 0x40F280 (Area78_Choices[8] = Area78_Handlers[0], PSX 0x801F2E1C;
// area 80's choice 6 = handler 3, PSX 0x801F3D50): a free effect record of
// kind 0x44.
extern "C" void __cdecl Area78_SpawnEffect44(void) { SpawnEffectRecord(0x44); }

// ===========================================================================
// Area 79 (descriptor 0x60EC30; PSX 0x801F4C2C): six choices, one handler,
// whose state table's second entry is the band's.
// ===========================================================================

// original 0x40F2B0 (Area79_Choices[0], [1]): counter 0 1 for answer 0,
// 0x14 for 1.
extern "C" void __cdecl Area79_ChoiceCounter1or14(void) { ChoiceCounter0(1, 0x14); }

// original 0x40F2E0 (Area79_Choices[2]): the message word left as it is;
// an answer 0..5 (unsigned: a negative one is none) sets counter 3 to the
// answer + 1 (a jump table of six in .text).
extern "C" void __cdecl Area79_ChoiceCounter3(void) {
    const int answer = Answer();
    if (static_cast<unsigned>(answer) > 5) return;
    B(at::kCounter3) = static_cast<unsigned char>(answer + 1);
}

// original 0x40F340 (Area79_Choices[3]): the message word from
// Area79_ChoiceMessages by the signed answer (read before the bound, so
// unchecked); an answer 0..5 sets counter 3 to 0xB + the answer.
extern "C" void __cdecl Area79_ChoiceMessageCounter3(void) {
    const signed char answer = Answer();
    SetMessage(MessageAt(at::kArea79ChoiceMessages, answer));
    if (static_cast<unsigned>(static_cast<int>(answer)) > 5) return;
    B(at::kCounter3) = static_cast<unsigned char>(0xB + answer);
}

// original 0x40F3B0 (Area79_Choices[4]): counter 0 8 for answer 0, 9 for 1.
extern "C" void __cdecl Area79_ChoiceCounter8or9(void) { ChoiceCounter0(8, 9); }

// original 0x40F3E0 (Area79_Choices[5]): counter 0 5 for answer 0, 0xA for 1.
extern "C" void __cdecl Area79_ChoiceCounter5orA(void) { ChoiceCounter0(5, 0xA); }

// original 0x40F410 (Area79_Handlers[0]; PSX 0x801F2E24): jumps through
// Area79_States[Sprite_Current +4] (two entries: 0x40D380, another block's,
// and Area79_StateSlide). Read in place and called directly; a state of 2 or
// more aborts where the original jumps through the dwords after the table.
extern "C" void __cdecl Area79_ObjectState(void) {
    const unsigned state = Sprite_Current[4];
    if (state >= at::kArea79StateCount)
        bof3::Fatal("Area79_ObjectState: state %u outside Area79_States' %u at 0x%X", state, at::kArea79StateCount,
                    static_cast<unsigned>(at::kArea79States));
    reinterpret_cast<area_harness::Handler>(static_cast<std::uintptr_t>(
        static_cast<std::uint32_t>(Long(Mem(at::kArea79States + state * 4)))))();
}

// original 0x40F430 (Area79_States[1]): with a count at Sprite_Current +0xA,
// the object's x moved by Area79_SlideSteps[count & 0xF] << 11 and its z by
// minus that, the count less 1, Field_State's word +0x12E less 2 (Sprite_Current
// and the count read again for each, as the original); a count of 0: the
// state +4 back to 0.
extern "C" void __cdecl Area79_StateSlide(void) {
    unsigned char* s = Sprite_Current;
    if (s[0xA] == 0) {
        s[4] = 0;
        return;
    }
    const auto dx = static_cast<std::uint32_t>(static_cast<int>(static_cast<signed char>(B(at::kArea79Steps + (s[0xA] & 0xFu)))));
    SetLong(s + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x34)) + (dx << 11)));
    s = Sprite_Current;
    const auto dz = static_cast<std::uint32_t>(-static_cast<int>(static_cast<signed char>(B(at::kArea79Steps + (s[0xA] & 0xFu)))));
    SetLong(s + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(s + 0x38)) + (dz << 11)));
    s = Sprite_Current;
    s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
    AddWord(Field_State + 0x12E, -2);
}

// ===========================================================================
// Area 80 (descriptor 0x60F308; PSX 0x801F4450): seven choices, four
// handlers (choice k + 3 is handler k), two of them area 49's camera shifts.
// ===========================================================================

// originals 0x40F4A0, 0x40F4D0, 0x40F500 (Area80_Choices[0..2]): the message
// word from the choice's pair in Area80_ChoiceMessages by the signed answer
// (unchecked); counter 0 1 for answer 0, 0xA for 1.
extern "C" void __cdecl Area80_ChoiceMessage0(void) { ChoiceMessageCounter0(at::kArea80ChoiceMessages, 1, 0xA); }
extern "C" void __cdecl Area80_ChoiceMessage1(void) { ChoiceMessageCounter0(at::kArea80ChoiceMessages + 4, 1, 0xA); }
extern "C" void __cdecl Area80_ChoiceMessage2(void) { ChoiceMessageCounter0(at::kArea80ChoiceMessages + 8, 1, 0xA); }

// original 0x40F530 (Area80_Handlers[2], PSX 0x801F3D34; named by twelve
// areas' tables: 7, 8, 15, 23, 49, 77, 80, 81, 100, 119, 134, 188):
// Camera_ShiftY 0, MapView_Redraw 2.
extern "C" void __cdecl Area80_ResetCameraShift(void) {
    Camera_ShiftY = 0;
    MapView_Redraw = 2;
}

// ===========================================================================
// Area 81 (descriptor 0x60FD90; PSX 0x801F37B8): two choices, four handlers
// (the fourth Area80_ResetCameraShift).
// ===========================================================================

// original 0x40F550 (Area81_Choices[0]): counter 0 1 for answer 0, 2 for 1.
extern "C" void __cdecl Area81_ChoiceCounter1or2(void) { ChoiceCounter0(1, 2); }

// original 0x40F580 (Area81_Choices[1]): the message word from
// Area81_ChoiceMessages by the signed answer (unchecked); counter 0 3 for
// answer 0, 4 for 1.
extern "C" void __cdecl Area81_ChoiceMessageCounter3or4(void) { ChoiceMessageCounter0(at::kArea81ChoiceMessages, 3, 4); }

// original 0x40F5B0 (Area81_Handlers[0]; PSX 0x801F2C94): a free effect
// record of kind 0x27 put at the running object's x and z (Sprite_Current
// read again for each, after the record's +0 and +5 are written).
extern "C" void __cdecl Area81_SpawnEffect27AtObject(void) {
    unsigned char* const e = SpawnEffectRecord(0x27);
    if (e == nullptr) return;
    SetLong(e + 0x34, Long(Sprite_Current + 0x34));
    SetLong(e + 0x38, Long(Sprite_Current + 0x38));
}

// original 0x40F5F0 (Area81_Handlers[1]; PSX 0x801F2D14): Camera_ShiftY -
// 0xF, MapView_Redraw 2.
extern "C" void __cdecl Area81_ShiftCameraDown(void) { ShiftCamera(-0xF); }

// original 0x40F600 (Area81_Handlers[2]; PSX 0x801F2D3C): Camera_ShiftY +
// 0xF, MapView_Redraw 2.
extern "C" void __cdecl Area81_ShiftCameraUp(void) { ShiftCamera(0xF); }

// ===========================================================================
// Area 82 (descriptor 0x610870; PSX 0x801F62A4): four handlers.
// ===========================================================================

// original 0x40F610 (Area82_Handlers[0]; PSX 0x801F5590): by Field_State's
// byte +0x89: 8 nothing; 4 the script's position word +0xA on 0xA; any other
// on 0x16.
extern "C" void __cdecl Area82_SkipByPose(void) {
    const unsigned char pose = Field_State[0x89];
    if (pose == 8) return;
    AddWord(MoveScript_Object + 0xA, pose == 4 ? 0xA : 0x16);
}

// original 0x40F640 (Area82_Handlers[1]; PSX 0x801F55F0): the running
// object's dword +0x70 (the margin Party_MemberAt reads, docs/area_w1c.md) 5.
extern "C" void __cdecl Area82_SetMargin5(void) { SetLong(Sprite_Current + 0x70, 5); }

// original 0x40F650 (Area82_Handlers[2]; PSX 0x801F5604): Field_State's byte
// +0x89 tested three times, read again each time: 5 the script's position on
// 3, 4 on 6, 6 on 9.
extern "C" void __cdecl Area82_SkipByPose456(void) {
    if (Field_State[0x89] == 5) AddWord(MoveScript_Object + 0xA, 3);
    if (Field_State[0x89] == 4) AddWord(MoveScript_Object + 0xA, 6);
    if (Field_State[0x89] == 6) AddWord(MoveScript_Object + 0xA, 9);
}

// original 0x40F6A0 (Area82_Handlers[3]; PSX 0x801F56E8): Msg_OpenScript(8)
// with flag 4 of the chapter's row set, else (3); Field_Request 2.
extern "C" void __cdecl Area82_OpenMessageByRowFlag4(void) {
    AH_CALL(Msg_OpenScript)(AH_CALL(Flags_Test)(FlagRow(), 4) ? 8 : 3);
    Field_Request = 2;
}

// ===========================================================================
// Area 84 (descriptor 0x611218; PSX 0x801F3130): one handler. (Area 83's
// descriptor names no code.)
// ===========================================================================

// original 0x40F6E0 (Area84_Handlers[0]; PSX 0x801F2C04): the active
// member's +0x80 bit 0 cleared; with the leader's byte +0x89 at 5, story flag
// 0x31 set, MoveCmd_TestFB(9, 0x10) (its answer unread) and the running
// object's byte +0 cleared.
extern "C" void __cdecl Area84_SetFlag31At5(void) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(at::kLeaderByte89) != 5) return;
    AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0x31);
    AH_CALL(MoveCmd_TestFB)(9, 0x10);
    Sprite_Current[0] = 0;
}

void AreaW2a_Inject() {
    if (bof3::WantsShadow("area_w2a")) area_w2a::SelfTest();
    BOF3_INJECT(Area76_StepDisarmTail5);
    BOF3_INJECT(Area77_CellHook);
    BOF3_INJECT(Area77_Trigger34);
    BOF3_INJECT(Area77_ChoiceMessage);
    BOF3_INJECT(Area77_ChoiceCounter0);
    BOF3_INJECT(Area77_SetByteFE);
    BOF3_INJECT(Area77_ClearByteFE);
    BOF3_INJECT(Area77_Spawn3AtMember0ListA);
    BOF3_INJECT(Area77_Spawn4AtMember0ListB);
    BOF3_INJECT(Area77_RemoveItem47);
    BOF3_INJECT(Area77_SetFlag4);
    BOF3_INJECT(Area77_Spawn2AtMember0ListC);
    BOF3_INJECT(Area77_Spawn3AtMember1ListD);
    BOF3_INJECT(Area77_Spawn3AtMember2ListE);
    BOF3_INJECT(Area77_ShiftCameraDownSound);
    BOF3_INJECT(Area77_Spawn4AtMember1List0);
    BOF3_INJECT(Area77_Spawn4AtMember2List0);
    BOF3_INJECT(Area77_Spawn4AtMember0List0);
    BOF3_INJECT(Area77_Spawn1AtMember0ListF);
    BOF3_INJECT(Area77_Spawn3AtMember0ListG);
    BOF3_INJECT(Area77_SpawnEffect4A);
    BOF3_INJECT(Area77_SpawnEffect4C);
    BOF3_INJECT(Area77_LeapStep);
    BOF3_INJECT(Area77_SpawnEffectKind);
    BOF3_INJECT(Area78_ChoiceRowFlag16or17);
    BOF3_INJECT(Area78_ChoiceCounter2Bor2C);
    BOF3_INJECT(Area78_ChoiceCounterAor2);
    BOF3_INJECT(Area78_ChoiceResetScene);
    BOF3_INJECT(Area78_SpawnEffect44);
    BOF3_INJECT(Area79_ChoiceCounter1or14);
    BOF3_INJECT(Area79_ChoiceCounter3);
    BOF3_INJECT(Area79_ChoiceMessageCounter3);
    BOF3_INJECT(Area79_ChoiceCounter8or9);
    BOF3_INJECT(Area79_ChoiceCounter5orA);
    BOF3_INJECT(Area79_ObjectState);
    BOF3_INJECT(Area79_StateSlide);
    BOF3_INJECT(Area80_ChoiceMessage0);
    BOF3_INJECT(Area80_ChoiceMessage1);
    BOF3_INJECT(Area80_ChoiceMessage2);
    BOF3_INJECT(Area80_ResetCameraShift);
    BOF3_INJECT(Area81_ChoiceCounter1or2);
    BOF3_INJECT(Area81_ChoiceMessageCounter3or4);
    BOF3_INJECT(Area81_SpawnEffect27AtObject);
    BOF3_INJECT(Area81_ShiftCameraDown);
    BOF3_INJECT(Area81_ShiftCameraUp);
    BOF3_INJECT(Area82_SkipByPose);
    BOF3_INJECT(Area82_SetMargin5);
    BOF3_INJECT(Area82_SkipByPose456);
    BOF3_INJECT(Area82_OpenMessageByRowFlag4);
    BOF3_INJECT(Area84_SetFlag31At5);
}
