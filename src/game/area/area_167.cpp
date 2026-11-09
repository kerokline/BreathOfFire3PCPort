// Moved here by tools/move_funcs.py from area_w4a.cpp (docs/source-layout-round.md).
#include "game/area/area_167.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4a.h"
#include "game/area_w4a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

bool Test(unsigned index) { return AH_CALL(Flags_Test)(Bank(), index) != 0; }

void Set(unsigned index) { AH_CALL(Flags_Set)(Bank(), index); }

void Clear(unsigned index) { AH_CALL(Flags_Clear)(Bank(), index); }

void SetMessage(unsigned v) { SetWord(At(::bof3::Img(at::kMessage.v)), v); }

// MoveScript_Object's script word +0xA, moved by n (the op's position).
void ScriptSkip(unsigned n) { rv::Store<unsigned>(rv::As<records::ScriptContext>(MoveScript_Object)->position, rv::As<records::ScriptContext>(MoveScript_Object)->position + n); }

void ArmTail(unsigned kind, unsigned state) {
    AreaW0a_TailKind = static_cast<unsigned char>(kind);
    AreaW0c_TailState = static_cast<unsigned char>(state);
}

// Tail kind 32's case by state: the switch the compiler turned into the
// 33-byte index at at::kTail32Index - this function's logic, ours since the
// function is (the brief of the disc-only round, docs/operand-store.md). 13
// is the cases that do nothing. AreaW4a_Inject registers the bytes this
// gives against the index for the hosted check.
unsigned Tail32Case(std::int32_t state) {
    switch (state) {
    case 0x00: return 0;
    case 0x01: return 1;
    case 0x05: return 2;
    case 0x06: return 3;
    case 0x0A: return 4;
    case 0x0B: return 5;
    case 0x0C: return 6;
    case 0x14: return 7;
    case 0x15: return 8;
    case 0x18: return 9;
    case 0x1E: return 10;
    case 0x1F: return 11;
    case 0x20: return 12;
    default: return 13;
    }
}

}  // namespace

// original 0x425CC0 (area 167 +0x34[0]): the message 0xFFFF; answer (byte) 2
// nothing more; else movement-script variable 5 = (answer != 0) + 1,
// ScriptFlags_Set40, variable 6 = 0x40, tail kind 0x20 (Area167_Tail32) at
// state 0x15.
extern "C" void __cdecl Area167_ChoiceTail32At15(void) {
    const unsigned char answer = static_cast<unsigned char>(AreaW0c_ChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer == 2) return;
    B(::bof3::Img(at::kScriptVar5.v)) = static_cast<unsigned char>((answer != 0 ? 1 : 0) + 1);
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(at::kScriptVar6.v)) = 0x40;
    ArmTail(0x20, 0x15);
}

// original 0x425D00 (+0x34[1]): the same with variable 5 = 2 or 0 (answer not
// 0 or 0) and state 0x14.
extern "C" void __cdecl Area167_ChoiceTail32At14Even(void) {
    const unsigned char answer = static_cast<unsigned char>(AreaW0c_ChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer == 2) return;
    B(::bof3::Img(at::kScriptVar5.v)) = static_cast<unsigned char>(answer != 0 ? 2 : 0);
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(at::kScriptVar6.v)) = 0x40;
    ArmTail(0x20, 0x14);
}

// original 0x425D40 (+0x34[2]): the same with variable 5 = 1 or 0 and state
// 0x14.
extern "C" void __cdecl Area167_ChoiceTail32At14(void) {
    const unsigned char answer = static_cast<unsigned char>(AreaW0c_ChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer == 2) return;
    B(::bof3::Img(at::kScriptVar5.v)) = static_cast<unsigned char>(answer != 0 ? 1 : 0);
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(at::kScriptVar6.v)) = 0x40;
    ArmTail(0x20, 0x14);
}

// original 0x425D80 (+0x34[3] and [4]): the message 0xFFFF; answer (byte) 0:
// ScriptFlags_Set40, variable 6 = 0x60, tail kind 0x20 at state 0xA.
extern "C" void __cdecl Area167_ChoiceTail32AtA(void) {
    const unsigned char answer = static_cast<unsigned char>(AreaW0c_ChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(at::kScriptVar6.v)) = 0x60;
    ArmTail(0x20, 0xA);
}

// original 0x425DB0 (handler 0 = choice 5; also area 77's handler 20 / choice
// 22 and area 148's handler 3 / choice 5; PSX 0x801F3F14): Cond_ByteFE = 2.
extern "C" void __cdecl Area167_SetByteFE2(void) { Cond_ByteFE = 2; }

// original 0x425DC0 (handler 2; PSX 0x801F3F58): Cond_ByteFD 2 - story flag
// 0x4C set: cleared, MoveScript_Object +3 = 5 and its script word +0xA =
// 0xFFFE, done; flag 0x4B clear: Sprite_Current's word +0x3E = 0xFDC0 and +0
// |= 0x40, done. Then (read again) 1 - flag 0x4C set: cleared, +3 = 4 and the
// word 0xFFFE, done; flag 0x4B set: +0x3E = 0x9C0, +0 |= 0x40, done. Else
// +0x3E = 0x3C0. Every pointer read again for each store.
extern "C" void __cdecl Area167_HeightBy4B4C(void) {
    if (Cond_ByteFD == 2) {
        if (Test(0x4C)) {
            Clear(0x4C);
            MoveScript_Object[3] = 5;
            rv::Store<unsigned>(rv::As<records::ScriptContext>(MoveScript_Object)->position, 0xFFFE);
            return;
        }
        if (!Test(0x4B)) {
            rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0xFDC0);
            Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
            return;
        }
    }
    if (Cond_ByteFD == 1) {
        if (Test(0x4C)) {
            Clear(0x4C);
            MoveScript_Object[3] = 4;
            rv::Store<unsigned>(rv::As<records::ScriptContext>(MoveScript_Object)->position, 0xFFFE);
            return;
        }
        if (Test(0x4B)) {
            rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0x9C0);
            Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
            return;
        }
    }
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0x3C0);
}

// original 0x425EA0 (handler 3; PSX 0x801F4080): Sprite_Current's word +0x3E
// 0x3C0: MoveScript_Object's script word + 8. Else +0 bit 6 cleared and
// story flag 0x4B toggled.
extern "C" void __cdecl Area167_Toggle4BUnlessLow(void) {
    unsigned char* const o = Sprite_Current;
    if (rv::As<records::SpriteObject>(o)->height_whole == 0x3C0) {
        ScriptSkip(8);
        return;
    }
    o[0] = static_cast<unsigned char>(o[0] & 0xBF);
    AH_CALL(Flags_Toggle)(Bank(), 0x4B);
}

// original 0x425ED0 (handler 4; PSX 0x801F40F4): story flag 0x4B clear:
// MoveScript_Object's script word + 3.
extern "C" void __cdecl Area167_Skip3Unless4B(void) {
    if (!Test(0x4B)) ScriptSkip(3);
}

// original 0x425EF0 (handler 5; PSX 0x801F4140): Cond_ByteFD 2 - story flag
// 0x4A set: cleared, MoveScript_Object's script word = 0xFFFE, its +3 = 7
// when the byte 0x905E68 is 0 else 8, done; flag 0x48 clear: +0x3E = 0x9C0
// (flag 0x49 clear) or 0xFDC0 (set) and +0 |= 0x40, done. Then (read again) 1
// - flag 0x4A set: cleared, +3 = 7 and the word 0xFFFE, done; flag 0x49
// clear: +0x3E = 0x9C0, +0 |= 0x40, done. Else +0x3E = 0x3C0.
extern "C" void __cdecl Area167_HeightBy48To4A(void) {
    if (Cond_ByteFD == 2) {
        if (Test(0x4A)) {
            Clear(0x4A);
            rv::Store<unsigned>(rv::As<records::ScriptContext>(MoveScript_Object)->position, 0xFFFE);
            if (AreaW1a_EntryZone == 0) MoveScript_Object[3] = 7;
            else MoveScript_Object[3] = 8;
            return;
        }
        if (!Test(0x48)) {
            const unsigned height = Test(0x49) ? 0xFDC0u : 0x9C0u;
            rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, height);
            Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
            return;
        }
    }
    if (Cond_ByteFD == 1) {
        if (Test(0x4A)) {
            Clear(0x4A);
            MoveScript_Object[3] = 7;
            rv::Store<unsigned>(rv::As<records::ScriptContext>(MoveScript_Object)->position, 0xFFFE);
            return;
        }
        if (!Test(0x49)) {
            rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0x9C0);
            Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
            return;
        }
    }
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0x3C0);
}

// original 0x426010 (handler 6; PSX 0x801F42C4): Sprite_Current's word +0x3E
// 0x3C0: MoveScript_Object's script word + 8. Else +0 bit 6 cleared, and by
// Cond_ByteFD (read once): 1 - flag 0x48 set, 0x49 cleared; 2 - 0x48 cleared,
// 0x49 set; 3 - both cleared; else nothing more.
extern "C" void __cdecl Area167_Flags48By49(void) {
    unsigned char* const o = Sprite_Current;
    if (rv::As<records::SpriteObject>(o)->height_whole == 0x3C0) {
        ScriptSkip(8);
        return;
    }
    o[0] = static_cast<unsigned char>(o[0] & 0xBF);
    const unsigned char chapter = Cond_ByteFD;
    if (chapter == 1) {
        Set(0x48);
        Clear(0x49);
    } else if (chapter == 2) {
        Clear(0x48);
        Set(0x49);
    } else if (chapter == 3) {
        Clear(0x48);
        Clear(0x49);
    }
}

// original 0x4260A0 (handler 7; PSX 0x801F43B8): Cond_ByteFD 2 - variable 5
// not 0: done; else MoveScript_Object's script word + 3. Then (read again)
// Cond_ByteFD 1: the script word + 3.
extern "C" void __cdecl Area167_Skip3ByChapter(void) {
    if (Cond_ByteFD == 2) {
        if (B(::bof3::Img(at::kScriptVar5.v)) != 0) return;
        ScriptSkip(3);
    }
    if (Cond_ByteFD == 1) ScriptSkip(3);
}

// original 0x4260E0 (handler 10 = choice 15; PSX 0x801F4480): Cond_ByteFE =
// 0x10.
extern "C" void __cdecl Area167_SetByteFE10(void) { Cond_ByteFE = 0x10; }

extern "C" void __cdecl Area167_Tail32(void) {
    const auto state = AreaW0c_TailState;
    if (static_cast<U>(state) > 0x20) return;
    const unsigned which = Tail32Case(state);
    switch (which) {
    case 0:
        AH_CALL(Party_DropIn)(0);
        AreaW0c_TailState = 1;
        return;
    case 1:
        if (B(::bof3::Img(at::kScriptVar6.v)) != 0x18) return;
        ArmTail(0, 0);
        Set(0x46);
        AH_CALL(Field_ChangeArea)(0x95, 0x80000, 0xC0000, 0x81);
        return;
    case 2:
        AH_CALL(Sound_PlayEffect)(0x204);
        AreaW0c_TailState = 6;
        SetWord(At(::bof3::Img(at::kTailTimer.v)), 0xA);
        return;
    case 3:
        SetWord(At(::bof3::Img(at::kTailTimer.v)), Word(At(::bof3::Img(at::kTailTimer.v))) - 1u);
        if (Word(At(::bof3::Img(at::kTailTimer.v))) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        ArmTail(0, 0);
        return;
    case 4:
        if (B(::bof3::Img(at::kScriptVar6.v)) != 0x64) return;
        AH_CALL(Party_DropIn)(5);
        AreaW0c_TailState = 0xB;
        return;
    case 5:
        if (B(::bof3::Img(at::kScriptVar6.v)) != 0x74) return;
        if (Cond_ByteFD == 2) AH_CALL(Field_ChangeArea)(0xA7, 0x130000, 0x9A0000, 0x87);
        else AH_CALL(Field_ChangeArea)(0xA7, 0xA0000, 0x1E0000, 0x86);
        AH_CALL(Flags_Toggle)(Bank(), 0x4B);
        Set(0x4C);
        AreaW0c_TailState = 0xC;
        return;
    case 6:
        if (B(::bof3::Img(at::kScriptVar6.v)) != 0) return;
        if (Cond_ByteFD == 0) AH_CALL(Sound_PlayEffect)(0x206);
        B(::bof3::Img(at::kScriptVar5.v)) = 0;
        ArmTail(0, 0);
        return;
    case 7:
        if (B(::bof3::Img(at::kScriptVar6.v)) != 0x44) return;
        AH_CALL(Party_DropIn)(2);
        AreaW0c_TailState = 0x18;
        return;
    case 8:
        AH_CALL(Party_DropIn)(8);
        AreaW0c_TailState = 0x18;
        return;
    case 9: {
        if (B(::bof3::Img(at::kScriptVar6.v)) != 0x54) return;
        const unsigned char way = B(::bof3::Img(at::kScriptVar5.v));
        if (way == 0) {
            B(::bof3::Img(at::kScriptVar6.v)) = 0;
            AH_CALL(ScriptFlags_Clear40)();
            AH_CALL(Field_ChangeArea)(0xA7, 0x1A8000, 0x560000, 5);
            Clear(0x48);
            Clear(0x49);
        } else if (way == 1) {
            AH_CALL(Field_ChangeArea)(0xA7, 0x200000, 0xF0000, 0x83);
            Set(0x48);
            Clear(0x49);
        } else if (way == 2) {
            AH_CALL(Field_ChangeArea)(0xA7, 0x290000, 0x8B0000, 0x84);
            Clear(0x48);
            Set(0x49);
        }
        if (Cond_ByteFD == 0) AH_CALL(Sound_PlayEffect)(0x205);
        Set(0x4A);
        AreaW0c_TailState = 0xC;
        return;
    }
    case 10:
        AreaW0c_TailState = 0x1F;
        return;
    case 11:
        if (Transition_Busy != 0) return;
        Cond_ByteFE = 0x20;
        SetWord(At(::bof3::Img(at::kTailTimer.v)), 0x1E);
        AreaW0c_TailState = 0x20;
        return;
    case 12:
        SetWord(At(::bof3::Img(at::kTailTimer.v)), Word(At(::bof3::Img(at::kTailTimer.v))) - 1u);
        if (Word(At(::bof3::Img(at::kTailTimer.v))) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        Set(0x90);
        AH_CALL(Field_ChangeArea)(0xA8, 0x150000, 0x40000, 7);
        B(::bof3::Img(Field_ScriptFlags2_addr)) = static_cast<unsigned char>(B(::bof3::Img(Field_ScriptFlags2_addr)) | 0x40);   // Field_ScriptFlags2's low byte
        ArmTail(0, 0);
        return;
    case 13:
        return;
    default:
        bof3::Fatal("Area167_Tail32: case %u (state %d) is past the fourteen-entry jump table 0x426408", which, state);
    }
}

// original 0x426470 (Area_ArriveHook's case for area 167): (x, z) 16.16; z's
// high word 0x66 and x's high word 0x14..0x16 (u16): ScriptFlags_Set40, tail
// kind 0x20 at state 0, al 1. Else al 0.
extern "C" unsigned char __cdecl Area167_ArriveHook(long x, long z) {
    if (static_cast<std::uint16_t>(static_cast<U>(z) >> 16) != 0x66) return 0;
    if (static_cast<std::uint16_t>((static_cast<U>(x) >> 16) - 0x14u) >= 3) return 0;
    AH_CALL(ScriptFlags_Set40)();
    ArmTail(0x20, 0);
    return 1;
}

// original 0x4264A0 (Area_CellHooks, area 167's): (x, z) as bytes; the
// switch records 0x63C594 (one) searched for x, z and a facing nibble (& 0xF)
// equal to the leader's whole facing byte +8 (read once, before the search):
// none al 0. Else its flag toggled in the story bank, ScriptFlags_Set40, tail
// kind 0x20 at state 5, al 1.
extern "C" unsigned char __cdecl Area167_CellHook(unsigned x, unsigned z) {
    const unsigned char facing = ObjTrio[0].direction;
    const auto bx = static_cast<unsigned char>(x);
    const auto bz = static_cast<unsigned char>(z);
    U found = 0;
    for (U a = ::bof3::Img(Area167_Switch_addr); a < ::bof3::Img(at::kA167SwitchEnd.v); a += 4, ++found) {
        const unsigned char* const r = At(a);
        if (r[0] == bx && r[1] == bz && (r[2] & 0xF) == facing) break;
    }
    if (found == 1) return 0;
    AH_CALL(Flags_Toggle)(Bank(), B(::bof3::Img(at::kA167Switch.v) + 3 + found * 4));
    AH_CALL(ScriptFlags_Set40)();
    ArmTail(0x20, 5);
    return 1;
}

// original 0x426510 (area 167 +0x40, the init; PSX 0x801F49F0): with
// Cond_ByteFD 0, story flag 0x8F set and 0x90 clear: Camera_Distance = 0x980,
// tail kind 0x20 at state 0x1E.
extern "C" void __cdecl Area167_Init(void) {
    if (Cond_ByteFD != 0) return;
    if (!Test(0x8F)) return;
    if (Test(0x90)) return;
    Camera_Distance = 0x980;
    ArmTail(0x20, 0x1E);
}

void Area167_Inject() {
    BOF3_INJECT(Area167_ChoiceTail32At15);
    BOF3_INJECT(Area167_ChoiceTail32At14Even);
    BOF3_INJECT(Area167_ChoiceTail32At14);
    BOF3_INJECT(Area167_ChoiceTail32AtA);
    BOF3_INJECT(Area167_SetByteFE2);
    BOF3_INJECT(Area167_HeightBy4B4C);
    BOF3_INJECT(Area167_Toggle4BUnlessLow);
    BOF3_INJECT(Area167_Skip3Unless4B);
    BOF3_INJECT(Area167_HeightBy48To4A);
    BOF3_INJECT(Area167_Flags48By49);
    BOF3_INJECT(Area167_Skip3ByChapter);
    BOF3_INJECT(Area167_SetByteFE10);
    BOF3_INJECT(Area167_Tail32);
    BOF3_INJECT(Area167_ArriveHook);
    BOF3_INJECT(Area167_CellHook);
    BOF3_INJECT(Area167_Init);
}
