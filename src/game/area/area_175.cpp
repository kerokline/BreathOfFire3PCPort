// Moved here by tools/move_funcs.py from area_w4d.cpp (docs/source-layout-round.md).
#include "game/area/area_175.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4d.h"
#include "game/area_w4d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4d::at;

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

void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// The focus object's trigger id cleared (0xFF): the focus read before the
// message store, as the originals do.
void DropFocusTrigger(unsigned message) {
    unsigned char* const focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    SetMessage(message);
    focus[0x86] = 0xFF;
}

// Field_ActiveMember's word +0x8A less 2 (the script position back one op).
void MemberStep() { AddWord(Field_ActiveMember + 0x8A, -2); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// Areas 175..185's choice 27 (eleven copies): the message by the answer read
// signed - 0xFA, 0xFB, 0xFC, 0xFD for 0..3, 0xF9 for 4; anything else (a
// negative answer is above 4 unsigned) leaves the message word alone.
void ChoiceMessageByAnswer() {
    static const std::uint16_t kMessages[5] = {0xFA, 0xFB, 0xFC, 0xFD, 0xF9};
    const auto answer = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(Answer())));
    if (answer > 4) return;
    SetMessage(kMessages[answer]);
}

// Areas 175 and 184's Zenny question (choices 10 and 14): the answer and
// Party_Zenny at 500 or more (unsigned) pick a message and a byte.
bool HasZenny500() { return static_cast<U>(Long(Mem(::bof3::Img(Party_Zenny_addr)))) >= 0x1F4; }

// Area 175's handler 2 and its glide step: x += step << 11, z -= step << 11
// (the step a signed byte of `steps` by `index & 0xF`), each through
// `object`; the callers pass the object and the index read again for the z
// half, as the originals read them.
void SlideBy(unsigned char* object, U steps, unsigned index) {
    const std::int32_t step = static_cast<signed char>(B(steps + (index & 0xF)));
    AddLong(object + 0x34, static_cast<U>(step) << 11);
}

void SlideZBy(unsigned char* object, U steps, unsigned index) {
    const std::int32_t step = static_cast<signed char>(B(steps + (index & 0xF)));
    AddLong(object + 0x38, static_cast<U>(-step) << 11);
}

}  // namespace

// original 0x4292C0 and its ten copies 0x4296A0, 0x429730, 0x4297B0,
// 0x429840, 0x429930, 0x4299B0, 0x429A40, 0x429AA0, 0x429B40, 0x429D10
// (areas 175..185's choice 27, 0x5C bytes each, each with its own five-entry
// jump table at +0x48).
extern "C" void __cdecl Area175_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }

// original 0x429320 (choice 30 = handler 1 of all eleven; PSX 0x801F3348 /
// area 175's 0x801F3FDC): with Field_Kind2X at 0x2E0000 or less (signed),
// the running object's x is Field_Kind2X.
extern "C" void __cdecl Area175_ClampXToKind2(void) {
    const std::int32_t x = Field_Kind2X;
    if (x > 0x2E0000) return;
    rv::Current<records::SpriteObject>()->x = x;
}

// original 0x429340 (handler 2; PSX 0x801F400C): Field_Request 2: the
// running object slid by Area175_SlideSteps[Frame_Counter & 0xF] (x plus,
// z minus, << 11; the object and the counter read again for z) and the
// active member's script back one op; else the object placed at (0x2C0000,
// 0x5C0000) with its state +4 0 (Sprite_Current read again for each store).
extern "C" void __cdecl Area175_SlideOrPlace(void) {
    const unsigned char request = Field_Request;
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    if (request == 2) {
        SlideBy(cur, ::bof3::Img(Area175_SlideSteps_addr), static_cast<U>(Frame_Counter));
        SlideZBy(Sprite_Current, ::bof3::Img(Area175_SlideSteps_addr), static_cast<U>(Frame_Counter));
        MemberStep();
        return;
    }
    tCur->x = 0x2C0000;
    rv::Current<records::SpriteObject>()->z = 0x5C0000;
    Sprite_Current[4] = 0;
}

// original 0x4293C0 (handler 3; PSX 0x801F40AC): Effect_FindFree to the
// running object's +0xB (Sprite_Current read again after the call); none
// (0xFF): the active member's script back one op; else that record +0 1,
// kind +5 0x1F, at (0x2E0000, 0x5C0000) height 0x1A00000 (the slot read
// again from +0xB for each store).
extern "C" void __cdecl Area175_SpawnEffect1F(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = slot;
    unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 0xFF) {
        MemberStep();
        return;
    }
    EffectAt(cur[0xB])[0] = 1;
    EffectAt(cur[0xB])[5] = 0x1F;
    rv::As<records::EffectObject>(EffectAt(cur[0xB]))->x = 0x2E0000;
    rv::As<records::EffectObject>(EffectAt(cur[0xB]))->z = 0x5C0000;
    rv::As<records::EffectObject>(EffectAt(cur[0xB]))->height = 0x1A00000;
}

// original 0x429440 (handler 4; PSX 0x801F4204): jmp [Sprite_Current[4] * 4
// + Area175_GlideStates] (unchecked; ours aborts past its two entries).
extern "C" void __cdecl Area175_GlideRun(void) {
    StateEntry("Area175_GlideRun", ::bof3::Img(Area175_GlideStates_addr), at::kArea175GlideStateCount, Sprite_Current[4])();
}

// original 0x429460 (Area175_GlideStates[0]): the count +0xA 0x20, state +4
// 1 (Sprite_Current read again).
extern "C" void __cdecl Area175_GlideBegin20(void) {
    Sprite_Current[0xA] = 0x20;
    Sprite_Current[4] = 1;
}

// original 0x429480 (Area175_GlideStates[1]): the count less 1; at 0 the
// state 0; else x plus and z minus Area175_GlideSteps[count & 0xF] << 11
// (Sprite_Current read again for z) and the active member's script back one
// op.
extern "C" void __cdecl Area175_GlideStep(void) {
    Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] - 1);
    unsigned char* const cur = Sprite_Current;
    if (cur[0xA] == 0) {
        cur[4] = 0;
        return;
    }
    SlideBy(cur, ::bof3::Img(Area175_GlideSteps_addr), cur[0xA]);
    unsigned char* const again = Sprite_Current;
    SlideZBy(again, ::bof3::Img(Area175_GlideSteps_addr), again[0xA]);
    MemberStep();
}

// original 0x4294F0 (handler 5; PSX 0x801F431C): Effect_FindFree; a slot: +0
// 1, kind +5 0x39, its words +0x2E / +0x30 the running object's (Sprite_Current
// read again for each).
extern "C" void __cdecl Area175_SpawnEffect39(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x39;
    tE->screen_x = static_cast<std::uint16_t>(rv::Current<records::SpriteObject>()->screen_x);
    tE->screen_y = static_cast<std::uint16_t>(rv::Current<records::SpriteObject>()->screen_y);
}

// original 0x429540 (handler 6; PSX 0x801F439C): the operand byte two past
// the script object's position word +0xA in the running area's +0x10 script
// [object +3] picks a word of Area175_ScriptMessages (unchecked, in .data):
// Msg_OpenScript(it); then Field_Request 2, the script object's +0 bit 0x10
// cleared and 0x20 set, its position on 1 (the object read again for each).
// The script table and the script are indexed unchecked, as the
// movement-script engine indexes them; an area number past Area_Descriptors'
// 200 aborts where the original reads past the table.
extern "C" void __cdecl Area175_OpenScriptMessage(void) {
    const unsigned area = Game_AreaNumber;
    if (area >= at::kDescriptorCount)
        bof3::Fatal("Area175_OpenScriptMessage: Game_AreaNumber %u past Area_Descriptors' %u", area, at::kDescriptorCount);
    const U descriptor = static_cast<U>(Long(Mem(::bof3::Img(Area_Descriptors_addr) + area * 4)));
    unsigned char* const object = MoveScript_Object;
    const U scripts = static_cast<U>(Long(Mem(descriptor + 0x10)));
    const U script = static_cast<U>(Long(Mem(scripts + object[3] * 4u)));
    const unsigned char operand = B(script + rv::As<records::ScriptContext>(object)->position + 2);
    AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(Mem(::bof3::Img(Area175_ScriptMessages_addr) + operand * 2u))));
    Field_Request = 2;
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] & 0xEF);
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x20);
    AddWord(MoveScript_Object + 0xA, 1);
}

// original 0x4295B0 (handler 7; PSX 0x801F446C): the script object's +0 bit
// 0x40 set; the running object's +7 bit 8 and +0 bit 0x10 set
// (Sprite_Current read again); Sprite_SetAnimation(0xA).
extern "C" void __cdecl Area175_PoseAnimA(void) {
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x40);
    Sprite_Current[7] = static_cast<unsigned char>(Sprite_Current[7] | 8);
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x10);
    AH_CALL(Sprite_SetAnimation)(0xA);
}

// original 0x4295E0 (Field_ObjectTriggers id 64; a gap of the tool, area
// 175's block): the object +1 4, +0x84 2, +0x83 0xD, word +0x8A 0; story
// flag 0x95; Party_DropIn(4); counter 0 cleared; al 0.
extern "C" unsigned char __cdecl Area175_Trigger64(unsigned char* object, unsigned char*) {
    object[1] = 4;
    object[0x84] = 2;
    object[0x83] = 0xD;
    rv::Store<unsigned>(rv::As<records::SpriteObject>(object)->script_position, 0);
    AH_CALL(Flags_Set)(StoryFlags(), 0x95);
    AH_CALL(Party_DropIn)(4);
    B(::bof3::Img(at::kCounter0.v)) = 0;
    return 0;
}

// original 0x429630 (choice 10 of all eleven): answer 0: message 0x6F, the
// tail sub-kind 0; else Party_Zenny at 500 or more: message 0x7D, sub-kind
// 1; else message 0x7C and the focus object's trigger id cleared.
extern "C" void __cdecl Area175_ChoiceAskZenny6F(void) {
    if (Answer() == 0) {
        SetMessage(0x6F);
        B(::bof3::Img(AreaW0c_TailSub_addr)) = 0;
        return;
    }
    if (HasZenny500()) {
        SetMessage(0x7D);
        B(::bof3::Img(AreaW0c_TailSub_addr)) = 1;
        return;
    }
    DropFocusTrigger(0x7C);
}

// original 0x429680 (choice 19 of all eleven): message 0x9F, or 0xA0 for an
// answer not 0.
extern "C" void __cdecl Area175_ChoiceMessage9F(void) { SetMessage(0x9F + (Answer() != 0 ? 1u : 0u)); }

void Area175_Inject() {
    BOF3_INJECT(Area175_ChoiceMessageByAnswer);
    BOF3_INJECT(Area175_ClampXToKind2);
    BOF3_INJECT(Area175_SlideOrPlace);
    BOF3_INJECT(Area175_SpawnEffect1F);
    BOF3_INJECT(Area175_GlideRun);
    BOF3_INJECT(Area175_GlideBegin20);
    BOF3_INJECT(Area175_GlideStep);
    BOF3_INJECT(Area175_SpawnEffect39);
    BOF3_INJECT(Area175_OpenScriptMessage);
    BOF3_INJECT(Area175_PoseAnimA);
    BOF3_INJECT(Area175_Trigger64);
    BOF3_INJECT(Area175_ChoiceAskZenny6F);
    BOF3_INJECT(Area175_ChoiceMessage9F);
}
