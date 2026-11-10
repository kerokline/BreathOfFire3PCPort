// Moved here by tools/move_funcs.py from area_w4e.cpp (docs/source-layout-round.md).
#include "game/area/area_191.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4e::at;

}  // namespace

namespace {

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

unsigned char* Cur() { return Sprite_Current; }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char* Row14() { return Mem(::bof3::Img(Cond_Flags_addr) + 0x70); }

// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(::bof3::Img(Chapter_FlagRow_addr)); }

bool Test(const unsigned char* bits, unsigned index) { return AH_CALL(Flags_Test)(bits, index) != 0; }

// The movement script's position (MoveScript_Object's word +0xA) back by 2.
void ScriptBack2() { AddWord(MoveScript_Object + 0xA, -2); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count) bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The tail armed from a choice: ScriptFlags_Set40, then kind and state (the
// sub-kind left as it is).
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}

// A run the script starts: MoveScript_Var7 and the step byte after it.
void SetRun(signed char var7, unsigned char step) {
    MoveScript_Var7 = var7;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = step;
}

// Areas 188 and 191's choice that stores a byte pair at the focus object
// (0x42A3E0's first half, 0x42B570): the answer read as s8 indexes the pairs
// (unchecked: a negative answer reads before the table, in .data); +0x18 the
// first byte, +0x1C the second, both dwords; the focus pointer and the answer
// read again between; the message word 0xFFFF.
void FocusPair(U pairs) {
    const auto a = static_cast<signed char>(Answer());
    unsigned char* focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    const unsigned char first = B(pairs + static_cast<U>(a * 2));
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, first);
    focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    const auto a2 = static_cast<signed char>(Answer());
    SetLong(focus + 0x1C, B(pairs + 1 + static_cast<U>(a2 * 2)));
}

}  // namespace

// Area 191's scale states 1 and 2: x += +0xC; +0x40 and +0x44 += step; +0xA -
// 1; its low two bits 0: +4 = `flip`; +0xA at 0: +4 = 0 (done); else the
// script back 2.
static void ScaleTick(std::int32_t step, unsigned char flip) {
    unsigned char* const cur = Cur();
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->x, static_cast<std::int32_t>(static_cast<U>(rv::As<records::SpriteHead>(cur)->x) + static_cast<U>(rv::As<records::SpriteHead>(cur)->step_x)));
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->scale_x, static_cast<std::int32_t>(static_cast<U>(rv::As<records::SpriteHead>(cur)->scale_x) + static_cast<U>(step)));
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->scale_y, static_cast<std::int32_t>(static_cast<U>(rv::As<records::SpriteHead>(cur)->scale_y) + static_cast<U>(step)));
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    if ((cur[0xA] & 3) == 0) cur[4] = flip;
    if (cur[0xA] == 0) {
        cur[4] = 0;
        return;
    }
    ScriptBack2();
}

// original 0x42B570 (area 191 +0x34[2]): FocusPair over Area191_FocusPairs.
extern "C" void __cdecl Area191_ChoiceFocusPair(void) { FocusPair(::bof3::Img(Area191_FocusPairs_addr)); }

// original 0x42B5B0 (areas 191 and 192 +0x34[3]): message 0x6C for answer 0,
// else 0xFFFF.
extern "C" void __cdecl Area191_ChoiceMessage6C(void) { SetMessage(Answer() != 0 ? 0xFFFF : 0x6C); }

// original 0x42B5D0 (area 191 +0x34[5]): message 0xFFFF; answer 1:
// ScriptFlags_Set40, tail kind 53 (Area191_Tail53) at state 0x1E; 2:
// ScriptFlags_Set40, the run: MoveScript_Var7 1, step 0x14.
extern "C" void __cdecl Area191_ChoiceTail53(void) {
    const unsigned char a = Answer();
    SetMessage(0xFFFF);
    if (a == 1) {
        ArmTail(0x35, 0x1E);
        return;
    }
    if (a != 2) return;
    AH_CALL(ScriptFlags_Set40)();
    SetRun(1, 0x14);
}

// original 0x42B610 (areas 191 and 192 +0x3C[0] = +0x34[6]; PSX 0x801F30DC):
// ScriptFlags_Set40; the active member's +0x80 bit 0 cleared and its word
// +0x8A + 1 (Field_ActiveMember read after the call and again).
extern "C" void __cdecl Area191_MemberNext(void) {
    AH_CALL(ScriptFlags_Set40)();
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    AddWord(Field_ActiveMember + 0x8A, 1);
}

// original 0x42B640 (area 191 +0x3C[1] = +0x34[7]; PSX 0x801F3130):
// Area191_MemberNext's body, then the run: MoveScript_Var7 2, step 0x19.
extern "C" void __cdecl Area191_MemberNextRun(void) {
    AH_CALL(ScriptFlags_Set40)();
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    AddWord(Field_ActiveMember + 0x8A, 1);
    SetRun(2, 0x19);
}

// original 0x42B680 (area 191 +0x3C[2] = +0x34[8]; PSX 0x801F31A0):
// Area191_ScaleStates by Sprite_Current +4 (unchecked there; ours aborts past
// its three).
extern "C" void __cdecl Area191_RunScale(void) {
    StateEntry("Area191_RunScale", ::bof3::Img(Area191_ScaleStates_addr), at::kArea191ScaleStateCount, Cur()[4])();
}

// original 0x42B6A0 (Area191_ScaleStates 0): +0x48 = 2; +0x40, +0x44 =
// 0x10000; +0xA = 0xFF; the dword +0x70 = 1; +0x5D..+0x5F = 0x50, 0x3C, 0; +4
// = 1; the script back 2.
extern "C" void __cdecl Area191_ScaleStart(void) {
    unsigned char* const cur = Cur();
    cur[0x48] = 2;
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->scale_x, 0x10000);
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->scale_y, 0x10000);
    cur[0xA] = 0xFF;
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->reach, 1);
    cur[0x5D] = 0x50;
    cur[0x5E] = 0x3C;
    cur[0x5F] = 0;
    cur[4] = 1;
    ScriptBack2();
}

// original 0x42B710 (Area191_ScaleStates 1): ScaleTick(+0x2000, to state 2).
extern "C" void __cdecl Area191_ScaleGrow(void) { ScaleTick(0x2000, 2); }

// original 0x42B780 (Area191_ScaleStates 2): ScaleTick(-0x2000, to state 1).
extern "C" void __cdecl Area191_ScaleShrink(void) { ScaleTick(-0x2000, 1); }

// original 0x42B7F0 (Field_ModeTailKinds[53]): by the s8 state (a byte table
// of 31 at +0x1D8 into a jump table of 8 at +0x1B8, both inside the extent;
// the ja bounds it unsigned): 0 - message 0x6D, request 2, state 1 (which
// does nothing: the message's choice moves it on). 2 - once Field_Request is
// not 2: Transition_Start(0), state 3. 3 - once Transition_Busy is 0:
// Draw_PassFlags 0, Sound_StopMusic, 0x42C2D0, Sound_LoadStream(0), state 4.
// 4 - once Sound_StreamDone answers (all of eax): story flag 0x82, the walk's
// word 0x90405C, 0x929EC1, 0x9036D0 0, its count + 1, Field_StatusBits bit 0,
// state 0xA. 0xA - once not 2: ScriptFlags_Clear40, Field_ChangeArea(the
// saved area, x, z, 4), Field_ScriptFlags2 bit 6, the byte 0x904152 0, story
// flag 0x77 cleared, kind and state 0. 0x14 - once not 2: ScriptFlags_Clear40,
// kind and state 0. 0x1E - once not 2: ScriptFlags_Clear40,
// Field_ChangeArea(0x96, 0x180000, 0x300000, 1), the same three, then
// Field_StatusBits bit 0 cleared, kind and state 0. Every other state:
// nothing.
extern "C" void __cdecl Area191_Tail53(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0:
        AH_CALL(Msg_OpenScript)(0x6D);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    case 2:
        if (Field_Request == 2) return;
        AH_CALL(Transition_Start)(0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 3;
        return;
    case 3:
        if (Transition_Busy != 0) return;
        Draw_PassFlags = 0;
        AH_CALL(Sound_StopMusic)();
        AH_AT(void (__cdecl*)(), at::kRestoreRecords.v)();
        AH_CALL(Sound_LoadStream)(0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 4;
        return;
    case 4: {
        if (AH_CALL(Sound_StreamDone)() == 0) return;
        AH_CALL(Flags_Set)(StoryFlags(), 0x82);
        const unsigned char count = B(::bof3::Img(at::kWalkCount.v));
        const unsigned char bits = Field_StatusBits;
        SetWord(Mem(::bof3::Img(Cond_Flags_addr) + 0xcc), 0);
        B(::bof3::Img(Area189_StepsSinceEvent_addr)) = 0;
        B(::bof3::Img(Area189_EventsInRow_addr)) = 0;
        B(::bof3::Img(at::kWalkCount.v)) = static_cast<unsigned char>(count + 1);
        Field_StatusBits = static_cast<unsigned char>(bits | 1);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xA;
        return;
    }
    case 0xA:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Field_ChangeArea)(Word(Mem(::bof3::Img(AreaW4d_ReturnArea_addr))), Long(Mem(::bof3::Img(AreaW2d_ReturnPoint_addr))), Long(Mem(::bof3::Img(AreaW4d_ReturnZ_addr))), 4);
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | 0x40);
        B(::bof3::Img(at::kSavedCount.v)) = 0;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return;
    case 0x14:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return;
    case 0x1E: {
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Field_ChangeArea)(0x96, 0x180000, 0x300000, 1);
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | 0x40);
        B(::bof3::Img(at::kSavedCount.v)) = 0;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        const unsigned char bits = Field_StatusBits;
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        Field_StatusBits = static_cast<unsigned char>(bits & 0xFE);
        return;
    }
    default: return;
    }
}

// original 0x42B9F0 (Area_StepHook's case for area 0xBF): the cells the step
// covers - (x, z) by their high words, and the next cell in x when x's low
// word is not 0, in z when z's is not, and the diagonal when both - counted
// where AreaMap_ByteAt answers 0xA6; any: ScriptFlags_Set40, tail kind 53 at
// state 0, al 1. Else al 0.
extern "C" unsigned char __cdecl Area191_StepHook(long x, long z) {
    const U xv = static_cast<U>(x), zv = static_cast<U>(z);
    const auto cx = static_cast<short>(xv >> 16), cz = static_cast<short>(zv >> 16);
    unsigned char n = 0;
    if (AH_CALL(AreaMap_ByteAt)(cx, cz) == 0xA6) n = 1;
    if ((xv & 0xFFFF) != 0 && AH_CALL(AreaMap_ByteAt)(static_cast<short>(cx + 1), cz) == 0xA6) ++n;
    if ((zv & 0xFFFF) != 0 && AH_CALL(AreaMap_ByteAt)(cx, static_cast<short>(cz + 1)) == 0xA6) ++n;
    if ((xv & 0xFFFF) != 0 && (zv & 0xFFFF) != 0 && AH_CALL(AreaMap_ByteAt)(static_cast<short>(cx + 1), static_cast<short>(cz + 1)) == 0xA6) ++n;
    if (n == 0) return 0;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x35;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    return 1;
}

// original 0x42BA90 (called raw by chapter code: 0x5677F3, 0x567833,
// 0x567873, 0x5678B3, 0x5678F3 (SC13) and 0x56AEE1 (SC15)): the message a
// member's talk opens. i = the index of the member byte (`who`'s low byte) in
// Area191_TalkKeys (5 when none); a = i * 9; b = the walk count, at most 8; c
// = the byte +0x1E of the actor record Character_IndexById[who] names (both
// unchecked). Cond_Flags row 14 bit 0xA set: Area191_TalkMessageB(a, who, b,
// c). Else a + 8 for c from 8, + 7 for c above 4 with b below 7, else +
// Area191_TalkSteps[b]; the word from Area191_TalkMessagesB (the chapter
// row's flag 2) or _A by a.
extern "C" unsigned short __cdecl Area191_TalkMessage(unsigned who) {
    const auto key = static_cast<unsigned char>(who);
    unsigned char i = 0;
    while (B(::bof3::Img(Area191_TalkKeys_addr) + i) != key) {
        ++i;
        if (i >= 5) break;
    }
    auto a = static_cast<unsigned char>(i * 9);
    unsigned char b = B(::bof3::Img(at::kWalkCount.v));
    if (b > 8) b = 8;
    const unsigned char c = B(::bof3::Img(Field_ActorStates_addr) + 0xe + Character_IndexById[key] * at::kCharStride);
    if (Test(Row14(), 0xA)) return AH_CALL(Area191_TalkMessageB)(a, who, b, c);
    if (c >= 8) {
        a = static_cast<unsigned char>(a + 8);
    } else if (c > 4 && b < 7) {
        a = static_cast<unsigned char>(a + 7);
    } else {
        a = static_cast<unsigned char>(a + B(::bof3::Img(Area191_TalkSteps_addr) + b));
    }
    if (Test(FlagRow(), 2)) return B(::bof3::Img(Area191_TalkMessagesB_addr) + a);
    return B(::bof3::Img(Area191_TalkMessagesA_addr) + a);
}

// original 0x42BBB0 (a, who, b, c; called by Area191_TalkMessage): a +
// Area191_TalkStepsC[b] for c below 5, else a + 8; the word from
// Area191_TalkMessagesC by it. `who` is not read.
extern "C" unsigned short __cdecl Area191_TalkMessageB(unsigned a, unsigned who, unsigned b, unsigned c) {
    (void)who;
    const auto r = static_cast<unsigned char>((c & 0xFF) < 5 ? (a & 0xFF) + B(::bof3::Img(Area191_TalkStepsC_addr) + (b & 0xFF)) : (a & 0xFF) + 8);
    return B(::bof3::Img(Area191_TalkMessagesC_addr) + r);
}

// original 0x42BBF0 (area 191 +0x40; PSX 0x801F3964), with 0x42BC20, the body
// it tail-jumps to (reached by nothing else): nothing with Cond_ByteFA 0xE and
// the chapter row's flag 5, nor with story flag 0x77. Else a list k by actor
// record 0's byte +0x1E (read first): row 14 bit 0xA clear - 2 from 9, 1 from
// 5, else the walk count at least 5; set - 2 from 5, else the count at least
// 5. Six times: Sprite_FindFree to the word SharedScratch; not 0xFF:
// EventOp_0x(Area191_ObjectLists[k] + 0x11 * n).
extern "C" void __cdecl Area191_Init(void) {
    if (Cond_ByteFA == 0xE && Test(FlagRow(), 5)) return;
    const unsigned char level = B(::bof3::Img(Field_ActorStates_addr) + 0xe);
    if (Test(StoryFlags(), 0x77)) return;
    unsigned char k;
    if (!Test(Row14(), 0xA)) {
        k = level >= 9 ? 2 : level >= 5 ? 1 : static_cast<unsigned char>(B(::bof3::Img(at::kWalkCount.v)) >= 5);
    } else {
        k = level >= 5 ? 2 : static_cast<unsigned char>(B(::bof3::Img(at::kWalkCount.v)) >= 5);
    }
    for (unsigned n = 0; n < 6; ++n) {
        const unsigned char slot = AH_CALL(Sprite_FindFree)();
        SetWord(Mem(::bof3::Img(at::kDamageScratch.v)), slot);
        if (slot == 0xFF) continue;
        const U list = static_cast<U>(Long(Mem(::bof3::Img(Area191_ObjectLists_addr) + k * 4u)));
        AH_CALL(EventOp_0x)(Mem(list + n * at::kEventOpRecord));
    }
}

// original 0x42BCE0 (EffectKind18_States[77], 0x6541A0): story flag 0x77 set:
// the walk's word 0x90405C, 0x929EC1, 0x9036D0 0, its count + 1,
// Field_StatusBits bit 0; a tail jump to Effect_Release.
extern "C" void __cdecl Area191_Kind18Flag77(void) {
    if (!Test(StoryFlags(), 0x77)) return;
    const unsigned char count = B(::bof3::Img(at::kWalkCount.v));
    SetWord(Mem(::bof3::Img(Cond_Flags_addr) + 0xcc), 0);
    B(::bof3::Img(Area189_StepsSinceEvent_addr)) = 0;
    B(::bof3::Img(Area189_EventsInRow_addr)) = 0;
    const unsigned char bits = Field_StatusBits;
    B(::bof3::Img(at::kWalkCount.v)) = static_cast<unsigned char>(count + 1);
    Field_StatusBits = static_cast<unsigned char>(bits | 1);
    AH_CALL(Effect_Release)();
}

// original 0x42BD30 (areas 191 and 192 +0x34[1]): message 0xFFFF; answer 0:
// ScriptFlags_Set40, the run: MoveScript_Var7 1, step 0x14.
extern "C" void __cdecl Area191_ChoiceRun1(void) {
    const unsigned char a = Answer();
    SetMessage(0xFFFF);
    if (a != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    SetRun(1, 0x14);
}

void Area191_Inject() {
    BOF3_INJECT(Area191_ChoiceFocusPair);
    BOF3_INJECT(Area191_ChoiceMessage6C);
    BOF3_INJECT(Area191_ChoiceTail53);
    BOF3_INJECT(Area191_MemberNext);
    BOF3_INJECT(Area191_MemberNextRun);
    BOF3_INJECT(Area191_RunScale);
    BOF3_INJECT(Area191_ScaleStart);
    BOF3_INJECT(Area191_ScaleGrow);
    BOF3_INJECT(Area191_ScaleShrink);
    BOF3_INJECT(Area191_Tail53);
    BOF3_INJECT(Area191_StepHook);
    BOF3_INJECT(Area191_TalkMessage);
    BOF3_INJECT(Area191_TalkMessageB);
    BOF3_INJECT(Area191_Init);
    BOF3_INJECT(Area191_Kind18Flag77);
    BOF3_INJECT(Area191_ChoiceRun1);
}
