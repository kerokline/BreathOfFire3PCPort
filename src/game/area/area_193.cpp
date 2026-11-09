// Moved here by tools/move_funcs.py from area_w4f.cpp (docs/source-layout-round.md).
#include "game/area/area_193.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4f.h"
#include "game/area_w4f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4f::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The answer as the originals' movsx reads it.
int AnswerS8() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// Area 192's choice 2 and area 193's choice 3 (0x42BD60, 0x42C430: one body,
// two tables): the focus object's dwords +0x18 and +0x1C the pair of bytes
// the s8 answer picks (table + answer * 2, unchecked), the message word 0xFFFF
// stored between the first read and the first store; the focus pointer and
// the answer read again for the second.
void FocusPair(U table) {
    int a = AnswerS8();
    unsigned char* focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    const unsigned first = B(table + static_cast<U>(a * 2));
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, static_cast<std::int32_t>(first));
    focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    a = AnswerS8();
    SetLong(focus + 0x1C, static_cast<std::int32_t>(B(table + 1 + static_cast<U>(a * 2))));
}

// A choice's message: the word the s8 answer picks (table + answer * 2).
unsigned short MessageByAnswer(U table) { return Word(Mem(table + static_cast<U>(AnswerS8() * 2))); }

// The tail disarmed: kind 0, state 0.
void Disarm() {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// The tail armed after ScriptFlags_Set40: kind `kind`, state `state`.
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}

// The run armed after ScriptFlags_Set40: MoveScript_Var7 `var`, its step
// byte `step`.
void ArmRun(signed char var, unsigned char step) {
    AH_CALL(ScriptFlags_Set40)();
    MoveScript_Var7 = var;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = step;
}

}  // namespace

// original 0x42C390 (area 193 +0x34[0]): the message word
// Area193_Messages0[s8 answer] (unchecked).
extern "C" void __cdecl Area193_ChoiceMessage(void) { SetMessage(MessageByAnswer(::bof3::Img(Area193_Messages0_addr))); }

// original 0x42C3B0 (area 193 +0x34[1]): the message Area193_Messages1[s8
// answer]; the answer 1: ScriptFlags_Set40, MoveScript_Var7 1, its step 0.
extern "C" void __cdecl Area193_ChoiceRunOnYes(void) {
    const unsigned char a = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(MessageByAnswer(::bof3::Img(Area193_Messages1_addr)));
    if (a == 1) ArmRun(1, 0);
}

// original 0x42C3E0 (area 193 +0x34[2]): the message Area193_Messages2[s8
// answer]; the answer 1: n = Inventory_Count(0, 0x5B, 0), Inventory_Add(0,
// 0x5B, 0x10 - n as a byte - the original pushes a fourth word, 0, never
// read), Sound_PlayEffect(0x106).
extern "C" void __cdecl Area193_ChoiceFill5B(void) {
    const unsigned char a = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(MessageByAnswer(::bof3::Img(Area193_Messages2_addr)));
    if (a != 1) return;
    const unsigned char n = static_cast<unsigned char>(AH_CALL(Inventory_Count)(0, 0x5B, 0));
    AH_CALL(Inventory_Add)(0, 0x5B, static_cast<unsigned char>(0x10 - n));
    AH_CALL(Sound_PlayEffect)(0x106);
}

// original 0x42C430 (area 193 +0x34[3]): FocusPair(Area193_FocusPairs).
extern "C" void __cdecl Area193_ChoiceFocusPair(void) { FocusPair(::bof3::Img(Area193_FocusPairs_addr)); }

// original 0x42C470 (area 193 +0x34[4]): three rows of a stack table (a
// message word and a tail state byte each) by the s8 answer: 0 message 0x1D
// and state 0xC, 1 0x1E and 0xE, 2 0xFFFF and 0x10. The original reads past
// its twelve bytes for any other answer (its own frame, then the caller's);
// ours aborts there.
extern "C" void __cdecl Area193_ChoiceMessageState(void) {
    static const unsigned short kMessages[3] = {0x1D, 0x1E, 0xFFFF};
    static const unsigned char kStates[3] = {0xC, 0xE, 0x10};
    const int a = AnswerS8();
    if (a < 0 || a > 2) bof3::Fatal("Area193_ChoiceMessageState: answer %d is past its three-row stack table", a);
    SetMessage(kMessages[a]);
    B(::bof3::Img(AreaW0c_TailState_addr)) = kStates[a];
}

// original 0x42C4D0 (area 193 +0x34[5] = +0x3C[0]; PSX 0x801F2E38):
// Field_ActiveMember's +0x80 bit 0 cleared; the leader's +0x89 7:
// ScriptFlags_Set40 and Field_ActiveMember's (read again) word +0x8A + 1.
extern "C" void __cdecl Area193_MemberBit0Leader7(void) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(::bof3::Img(ObjTrio_addr) + 0x89) != 7) return;
    AH_CALL(ScriptFlags_Set40)();
    AddWord(Field_ActiveMember + 0x8A, 1);
}

// original 0x42C500 (Field_ModeTailKinds[57]): by the s8 state - 0xA through a
// jump table of 13 in its extent (the `ja` makes every other state nothing):
//   0xA: message 0x1C, Field_Request 2, state 0xB (0xB waits);
//   0xC: unless Field_Request is 2: story flag 0x77 cleared,
//      ScriptFlags_Clear40, Field_ChangeArea(0xBD, 0x12000000, 0x17FF0000, 2);
//      Field_ScriptFlags2 bit 6; the return point's byte +0xB 2, the word
//      0x90405C 0, 0x90405F 0xF0, 0x90405E, 0x929EC1, 0x9036D0 0;
//      Field_StatusBits (read after the call) bit 0 set; disarmed;
//   0xE: unless Field_Request is 2: flag 0x77 cleared, ScriptFlags_Clear40,
//      Field_ChangeArea(0x97, 0x160000, 0x150000, 3), Field_ScriptFlags2 bit
//      6, disarmed;
//   0x10: unless Field_Request is 2: ScriptFlags_Clear40, disarmed;
//   0x14: once Field_Request is 0: message 0x25 if Cond_Flags row 14's flag
//      3, else 0x20; Field_Request 2, state 0x15;
//   0x15: unless Field_Request is 2: Party_HealJoined, Sound_LoadStream(0),
//      state 0x16;
//   0x16: once Sound_StreamDone: counter 0 = 1, Music_Play(0x95, 0x10),
//      Transition_Start(1), Draw_PassFlags 0x1F, row 14's flag 3 and story
//      flag 0x8A cleared, disarmed.
extern "C" void __cdecl Area193_Tail57(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0xA:
        AH_CALL(Msg_OpenScript)(0x1C);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xB;
        return;
    case 0xC: {
        if (Field_Request == 2) return;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Field_ChangeArea)(0xBD, 0x12000000, 0x17FF0000, 2);
        const unsigned char bits = Field_StatusBits;
        B(::bof3::Img(Field_ScriptFlags2_addr)) |= 0x40;
        B(::bof3::Img(AreaW3g_HealByte_addr)) = 2;
        SetWord(Mem(::bof3::Img(Cond_Flags_addr) + 0xcc), 0);
        B(::bof3::Img(Cond_Flags_addr) + 0xcf) = 0xF0;
        B(::bof3::Img(Cond_Flags_addr) + 0xce) = 0;
        B(::bof3::Img(Area189_StepsSinceEvent_addr)) = 0;
        B(::bof3::Img(Area189_EventsInRow_addr)) = 0;
        Field_StatusBits = static_cast<unsigned char>(bits | 1);
        Disarm();
        return;
    }
    case 0xE:
        if (Field_Request == 2) return;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Field_ChangeArea)(0x97, 0x160000, 0x150000, 3);
        B(::bof3::Img(Field_ScriptFlags2_addr)) |= 0x40;
        Disarm();
        return;
    case 0x10:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        Disarm();
        return;
    case 0x14:
        if (Field_Request != 0) return;
        AH_CALL(Msg_OpenScript)(AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x70), 3) != 0 ? 0x25 : 0x20);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x15;
        return;
    case 0x15:
        if (Field_Request == 2) return;
        AH_CALL(Party_HealJoined)();
        AH_CALL(Sound_LoadStream)(0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x16;
        return;
    case 0x16:
        if (AH_CALL(Sound_StreamDone)() == 0) return;
        B(::bof3::Img(at::kCounter0.v)) = 1;
        AH_CALL(Music_Play)(0x95, 0x10);
        AH_CALL(Transition_Start)(1);
        Draw_PassFlags = 0x1F;
        AH_CALL(Flags_Clear)(Mem(::bof3::Img(Cond_Flags_addr) + 0x70), 3);
        AH_CALL(Flags_Clear)(StoryFlags(), 0x8A);
        Disarm();
        return;
    default:
        return;
    }
}

// original 0x42C700 (Area_StepHook's case for area 0xC1): x or z (whole
// dwords) exactly 0x18000, then either exactly 0x338000 (in that order):
// ScriptFlags_Set40, tail kind 0x39 at state 0xA, al 1; else al 0.
extern "C" unsigned char __cdecl Area193_StepHook(long x, long z) {
    const U ux = static_cast<U>(x), uz = static_cast<U>(z);
    if (ux != 0x18000 && uz != 0x18000 && ux != 0x338000 && uz != 0x338000) return 0;
    ArmTail(0x39, 0xA);
    return 1;
}

// original 0x42C740 (area 193's init, its descriptor +0x40; PSX
// 0x801F31C0): story flag 0x8A set: Draw_PassFlags 0, tail kind 0x39 at
// state 0x14; clear: the pending-area byte 0x937F98 1.
extern "C" void __cdecl Area193_Init(void) {
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x8A) != 0) {
        Draw_PassFlags = 0;
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x39;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x14;
        return;
    }
    B(::bof3::Img(Area_EnterTransition_addr)) = 1;
}

void Area193_Inject() {
    BOF3_INJECT(Area193_ChoiceMessage);
    BOF3_INJECT(Area193_ChoiceRunOnYes);
    BOF3_INJECT(Area193_ChoiceFill5B);
    BOF3_INJECT(Area193_ChoiceFocusPair);
    BOF3_INJECT(Area193_ChoiceMessageState);
    BOF3_INJECT(Area193_MemberBit0Leader7);
    BOF3_INJECT(Area193_Tail57);
    BOF3_INJECT(Area193_StepHook);
    BOF3_INJECT(Area193_Init);
}
