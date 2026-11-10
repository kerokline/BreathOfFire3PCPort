// Moved here by tools/move_funcs.py from area_w1a.cpp (docs/source-layout-round.md).
#include "game/area/area_41.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1a_callees.h"
#include "game/draw_pool.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1a::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

using Handler = void (__cdecl*)();

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// Field_State's word +0x12E less 2 (area 41's states).
void LeaderStep() { AddWord(Field_State + 0x12E, -2); }

void SetByte(U x, U z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// Area 41's tint pair (0x406560 up, 0x4064C0 down): +0x34 moved by +0xC, the
// running script's tint record (MoveScript_TintRecords by Field_State +0x149)
// bytes +2..+4 moved by `d`, the index re-read for each.
void TintBy(unsigned char d) {
    unsigned char* const cur = Sprite_Current;
    AddLong(cur + 0x34, static_cast<U>(rv::As<records::SpriteHead>(cur)->step_x));
    unsigned char* const leader = Field_State;
    for (unsigned off = 2; off <= 4; ++off) MoveScript_TintRecords[leader[0x149] * 12u + off] += d;
}

// Area 41's countdown on +0xA (states 1 and 2): at 0, +0xA = 8 and state
// `next`; Field_State's word +0x12E less 2 either way.
void CountDown(unsigned char next) {
    unsigned char* cur = Sprite_Current;
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    cur = Sprite_Current;
    if (cur[0xA] == 0) {
        cur[0xA] = 8;
        Sprite_Current[4] = next;
    }
    LeaderStep();
}

}  // namespace

// original 0x406200 (Area41_Choices[0]): message 0xFFFF; answer 0:
// Party_DropIn(1), counter 1 = 0xA; else Party_DropIn(2), 0x14.
extern "C" void __cdecl Area41_ChoiceDropIn12(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer == 0) {
        AH_CALL(Party_DropIn)(1);
        B(::bof3::Img(at::kCounter1.v)) = 0xA;
        return;
    }
    AH_CALL(Party_DropIn)(2);
    B(::bof3::Img(at::kCounter1.v)) = 0x14;
}

// original 0x406240 (Area41_Choices[1]): answer 0: Party_DropIn(3), counter 1
// = 0x1E; else Party_DropIn(4), the byte 0x903804 points at (read after the
// call) 0, counter 1 = 0x28.
extern "C" void __cdecl Area41_ChoiceDropIn34(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer == 0) {
        AH_CALL(Party_DropIn)(3);
        B(::bof3::Img(at::kCounter1.v)) = 0x1E;
        return;
    }
    AH_CALL(Party_DropIn)(4);
    Ptr(::bof3::Img(AreaW1a_FocusObject_addr))[0] = 0;
    B(::bof3::Img(at::kCounter1.v)) = 0x28;
}

// original 0x406280 (Area41_Choices[3]): the message Area41_GiveMessages[the
// signed answer] (unchecked: any answer but 0 and 1 reads the words either
// side); answer 0 with flag 6 of the chapter's row clear: sound 0x106, item
// 0x28 of category 0 given, the flag set (the row pointer read again).
extern "C" void __cdecl Area41_ChoiceGiveItem(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    const auto index = static_cast<std::int32_t>(static_cast<signed char>(answer));
    SetMessage(Word(Mem(::bof3::Img(Area41_GiveMessages_addr) + static_cast<U>(index) * 2u)));
    if (answer != 0) return;
    if (AH_CALL(Flags_Test)(Ptr(::bof3::Img(Chapter_FlagRow_addr)), 6) != 0) return;
    AH_CALL(Sound_PlayEffect)(0x106);
    AH_CALL(Inventory_Add)(0, 0x28, 1);   // the original pushes a fourth word, 0, never read
    AH_CALL(Flags_Set)(Ptr(::bof3::Img(Chapter_FlagRow_addr)), 6);
}

// original 0x4062E0 (Area41_Choices[4]): answer 0 with items 0x49, 0x3F, 0x27
// and 0x1D of category 0 all held: each taken (0x591B60), message 0x62; any
// missing: message 0x63 and the mark. Answer not 0: 0x64 and the mark.
extern "C" void __cdecl Area41_ChoiceTrade(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0x64);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    static constexpr unsigned kItems[4] = {0x49, 0x3F, 0x27, 0x1D};
    for (unsigned item : kItems)
        if (AH_CALL(Inventory_Count)(0, item, 0) == 0) {
            SetMessage(0x63);
            B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
            return;
        }
    for (unsigned item : kItems) AH_AT(void (__cdecl*)(unsigned, unsigned, unsigned, unsigned), at::kInventoryTake.v)(0, item, 1, 0);
    SetMessage(0x62);
}

// original 0x4063A0 (Area41_Choices[7] and [8]): answer not 0: message 0x64
// and the mark; else 0xFFFF.
extern "C" void __cdecl Area41_ChoiceConfirm64(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(0x64);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x4063D0 (Area41_Choices[12] = Area41_Handlers[2]; PSX
// 0x801F3E3C): Area41_States by Sprite_Current[4].
extern "C" void __cdecl Area41_Run(void) {
    StateEntry("Area41_Run", ::bof3::Img(Area41_States_addr), at::kArea41StateCount, Sprite_Current[4])();
}

// original 0x4063F0 (Area41_States[0]): the step +0xC = 0x4000, a tint step
// up (Area41_TintUp), then +0xA = 7 and state 1 (Sprite_Current read again
// for each); the leader's word +0x12E less 2.
extern "C" void __cdecl Area41_TintStart(void) {
    rv::Current<records::SpriteObject>()->step_x = 0x4000;
    AH_CALL(Area41_TintUp)();
    Sprite_Current[0xA] = 7;
    Sprite_Current[4] = 1;
    LeaderStep();
}

// original 0x406430 (Area41_States[1]): a tint step up; +0xA down, at 0 the
// slide (state 2) for 8.
extern "C" void __cdecl Area41_TintRise(void) {
    AH_CALL(Area41_TintUp)();
    CountDown(2);
}

// original 0x406470 (Area41_States[2]): +0x34 by +0xC; +0xA down, at 0 the
// fall (state 3) for 8.
extern "C" void __cdecl Area41_Slide(void) {
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    AddLong(cur + 0x34, static_cast<U>(tCur->step_x));
    CountDown(3);
}

// original 0x4064C0 (Area41_States[3]): a tint step down; +0xA down, at 0
// state 0 (and the leader's word left); else the leader's word +0x12E less 2.
extern "C" void __cdecl Area41_TintFall(void) {
    TintBy(0xFE);
    unsigned char* cur = Sprite_Current;
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    cur = Sprite_Current;
    if (cur[0xA] == 0) {
        cur[4] = 0;
        return;
    }
    LeaderStep();
}

// original 0x406560 (called by TintStart and TintRise): +0x34 by +0xC, the
// tint record's three bytes up 2.
extern "C" void __cdecl Area41_TintUp(void) { TintBy(2); }

// original 0x4065D0 (Area41_Choices[13] = Area41_Handlers[3]; PSX 0x801F41F8).
extern "C" void __cdecl Area41_ClearCells(void) {
    SetByte(0x40, 0x28, 0);
    SetByte(0x40, 0x27, 0);
}

// original 0x4065F0 (Area41_Choices[14] = Area41_Handlers[4]; PSX 0x801F4230).
extern "C" void __cdecl Area41_PlaceKind2(void) { AH_CALL(Kind2_Place)(8); }

// original 0x406600 (Field_ObjectTriggers id 29, (object, flags) ignored):
// ScriptFlags_Set40, the mode tail kind 4 with sub-kind 0xA; al 0.
extern "C" unsigned char __cdecl Area41_Trigger29(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0xA;
    return 0;
}

// original 0x406620 (Field_ObjectTriggers ids 59 and 60): the story flag the
// byte 0x5F5F2D + the object's +0x86 names (Area41_TriggerFlags for 59, 60;
// the flags argument is not read), sound 0x201; al 0.
extern "C" unsigned char __cdecl Area41_TriggerFlag(unsigned char* object, unsigned char*) {
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), B(::bof3::Img(at::kArea41TriggerBase.v) + object[0x86]));
    AH_CALL(Sound_PlayEffect)(0x201);
    return 0;
}

void Area41_Inject() {
    BOF3_INJECT(Area41_ChoiceDropIn12);
    BOF3_INJECT(Area41_ChoiceDropIn34);
    BOF3_INJECT(Area41_ChoiceGiveItem);
    BOF3_INJECT(Area41_ChoiceTrade);
    BOF3_INJECT(Area41_ChoiceConfirm64);
    BOF3_INJECT(Area41_Run);
    BOF3_INJECT(Area41_TintStart);
    BOF3_INJECT(Area41_TintRise);
    BOF3_INJECT(Area41_Slide);
    BOF3_INJECT(Area41_TintFall);
    BOF3_INJECT(Area41_TintUp);
    BOF3_INJECT(Area41_ClearCells);
    BOF3_INJECT(Area41_PlaceKind2);
    BOF3_INJECT(Area41_Trigger29);
    BOF3_INJECT(Area41_TriggerFlag);
}
