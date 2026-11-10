// Moved here by tools/move_funcs.py from effect_4b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_8c.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect_4b.h"
#include "game/effect_4b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/sprite_draw.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_4b::at;

}  // namespace

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char& Byte(U cell) { return At(cell)[0]; }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the
// dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4b.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

unsigned short Pad() { return Input_Pressed; }

void CountUp() { Effect4b_Count = Effect4b_Count + 1u; }

// The leader's x (+0x34) or z (+0x38) moved by `d`, Field_State the leader
// (made so before the store, as the original).
void Nudge(unsigned off, U d) {
    const U v = UL(rv::Bytes(ObjTrio) + off);
    Field_State = rv::Bytes(ObjTrio);
    SetUL(rv::Bytes(ObjTrio) + off, v + d);
}

// States 1 and 4: the first count-down down - at 0, `sound`, the count kept
// as the target, +1 = 0xA; else the pad's 0x40 pressed - counted, the frame
// count 0, +1 = 3; else +1 = `idle`.
void Kind8CWait(unsigned short sound, unsigned char pressed, unsigned char idle) {
    const unsigned char t = static_cast<unsigned char>(Effect4b_TimerA - 1);
    Effect4b_TimerA = t;
    if (t == 0) {
        SH_CALL(Sound_PlayEffect)(sound);
        const unsigned short c = Effect4b_Count;
        unsigned char* const s = S();
        Effect4b_Target = c;
        Effect4b_TimerA = 0;
        s[1] = 0xA;
        return;
    }
    if (Pad() == Sigma(0x40)) {
        unsigned char* const s = S();
        CountUp();
        Effect4b_Presses = 0;
        s[1] = pressed;
        return;
    }
    S()[1] = idle;
}

// States 3 and 0x11: the pad's `button` counted; the frame count up - at 0x1C
// or more +1 = 0x16, else +1 = `next`.
void Kind8CHold(unsigned short button, unsigned char next) {
    if (Pad() == Sigma(button)) CountUp();
    const unsigned char n = static_cast<unsigned char>(Effect4b_Presses + 1);
    Effect4b_Presses = n;
    S()[1] = n >= 0x1C ? 0x16 : next;
}

}  // namespace

// original 0x48AB30 (Effect_KindHandlers[0x8B], hidden after 0x48A8E0):
// jmp [EffectKind8B_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind8B_Run(void) {
    Dispatch("EffectKind8B_Run", AddressOf(EffectKind8B_States), EffectKind8B_States_count);
}

// original 0x48AB50 (state 0): +9 = 0xB4, +1 = 1.
extern "C" void __cdecl EffectKind8B_Start(void) {
    S()[9] = 0xB4;
    S()[1] = 1;
}

// original 0x48AB70 (state 1): the counter 0x903849 at 1 or more - the leader
// (ObjTrio, made Field_State and Sprite_Current) its +0x124 bit 6 set and
// animation 0x6F, its +7 kept to bits 0x11; Sprite_Current put back; +1 = 2.
extern "C" void __cdecl EffectKind8B_Wait(void) {
    const unsigned char n = Byte(::bof3::Img(at::kCounter2.v));
    unsigned char* const saved = Sprite_Current;
    if (n < 1) return;
    Field_State = rv::Bytes(ObjTrio);
    Sprite_Current = rv::Bytes(ObjTrio);
    rv::Bytes(ObjTrio)[0x124] = static_cast<unsigned char>(rv::Bytes(ObjTrio)[0x124] | 0x40);
    SH_CALL(Sprite_SetAnimation)(0x6F);
    const unsigned char b = static_cast<unsigned char>(rv::Bytes(ObjTrio)[0x7] & 0x11);
    Sprite_Current = saved;
    rv::Bytes(ObjTrio)[0x7] = b;
    saved[1] = 2;
}

// original 0x48ABC0 (state 2): the leader made Field_State and Sprite_Current;
// at its word +0x58 0xC and byte +0x4A 1 (the word read first) - animation
// 0x6E, its +7 kept to bits 0x11, Sprite_Current put back and +1 = 3. Else
// Sprite_Current is left on the leader (section 7).
extern "C" void __cdecl EffectKind8B_Pose(void) {
    const bool word = Word(rv::Bytes(ObjTrio) + 0x58) == 0xC;
    unsigned char* const saved = Sprite_Current;
    Field_State = rv::Bytes(ObjTrio);
    Sprite_Current = rv::Bytes(ObjTrio);
    if (!word || ObjTrio[0].script_hold != 1) return;
    SH_CALL(Sprite_SetAnimation)(0x6E);
    const unsigned char b = static_cast<unsigned char>(rv::Bytes(ObjTrio)[0x7] & 0x11);
    Sprite_Current = saved;
    rv::Bytes(ObjTrio)[0x7] = b;
    saved[1] = 3;
}

// original 0x48AC10 (state 3): the counter 0x903849 0; +1 = 1.
extern "C" void __cdecl EffectKind8B_Again(void) {
    unsigned char* const s = S();
    Byte(::bof3::Img(at::kCounter2.v)) = 0;
    s[1] = 1;
}

// original 0x48AC30 (Effect_KindHandlers[0x8C], hidden after 0x48A8E0):
// jmp [EffectKind8C_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind8C_Run(void) {
    Dispatch("EffectKind8C_Run", AddressOf(EffectKind8C_States), EffectKind8C_States_count);
}

// original 0x48AC50 (state 0): both count-downs a byte of kKind8CCounts by
// Rand & 0xF; the frame count, step count, direction, presses and target 0;
// +1 = 1.
extern "C" void __cdecl EffectKind8C_Start(void) {
    const unsigned r = static_cast<unsigned>(SH_CALL(Rand)()) & 0xFu;
    unsigned char* const s = S();
    const unsigned char v = At(::bof3::Img(at::kKind8CCounts.v) + r)[0];
    Effect4b_TimerA = v;
    Effect4b_TimerB = v;
    Effect4b_Presses = 0;
    Effect4b_Steps = 0;
    Effect4b_Direction = 0;
    Effect4b_Count = 0;
    Effect4b_Target = 0;
    s[1] = 1;
}

// original 0x48ACA0 (state 1): Kind8CWait(0x200, 3, 1).
extern "C" void __cdecl EffectKind8C_WaitA(void) { Kind8CWait(0x200, 3, 1); }

// original 0x48AD10 (state 3): Kind8CHold(0x40, 4).
extern "C" void __cdecl EffectKind8C_PressA(void) { Kind8CHold(0x40, 4); }

// original 0x48AD50 (state 4): Kind8CWait(0x201, 1, 3).
extern "C" void __cdecl EffectKind8C_WaitB(void) { Kind8CWait(0x201, 1, 3); }

// original 0x48ADC0 (state 0xA): the target 0 - +1 = 0x16; else the frame
// count 0 and +1 = 0xB.
extern "C" void __cdecl EffectKind8C_Check(void) {
    if (Effect4b_Target == 0) {
        S()[1] = 0x16;
        return;
    }
    unsigned char* const s = S();
    Effect4b_Presses = 0;
    s[1] = 0xB;
}

// original 0x48ADF0 (state 0xB): the pad's word one of 0x1000, 0x4000, 0x2000,
// 0x8000, 0x3000, 0x6000, 0x9000, 0xC000 - kept as the direction, sound 0x202,
// then by the word read again: 0x1000 / 0x3000 the leader's z down 0x800,
// 0x2000 / 0x6000 its x up, 0x4000 / 0xC000 z up, 0x8000 / 0x9000 x down (any
// other nothing); +1 = 0xC and +9 = 0xC. Any other word: nothing.
extern "C" void __cdecl EffectKind8C_Steer(void) {
    const unsigned short w = Pad();
    switch (w) {
    case 0x1000: case 0x4000: case 0x2000: case 0x8000:
    case 0x3000: case 0x6000: case 0x9000: case 0xC000: break;
    default: return;
    }
    Effect4b_Direction = w;
    SH_CALL(Sound_PlayEffect)(0x202);
    switch (Pad()) {
    case 0x1000: case 0x3000: Nudge(0x38, 0xFFFFF800u); break;
    case 0x2000: case 0x6000: Nudge(0x34, 0x800u); break;
    case 0x4000: case 0xC000: Nudge(0x38, 0x800u); break;
    case 0x8000: case 0x9000: Nudge(0x34, 0xFFFFF800u); break;
    default: break;
    }
    unsigned char* const s = S();
    s[1] = 0xC;
    S()[9] = 0xC;
}

// original 0x48AF10 (state 0xC): +9 down; it was 0 - +1 = 0xD.
extern "C" void __cdecl EffectKind8C_SteerWait(void) {
    unsigned char* const s = S();
    const unsigned char c = s[9];
    s[9] = static_cast<unsigned char>(c - 1);
    if (c == 0) S()[1] = 0xD;
}

// original 0x48AF30 (state 0xD): the step count up - at 8, the leader's x and
// z kept to multiples of 0x1000, +1 = 0xF, the count and the step count 0;
// else the leader moved back by the direction (0x1000 / 0x3000 z up, 0x2000 /
// 0x6000 x down, 0x4000 / 0xC000 z down, 0x8000 / 0x9000 x up) and +1 = 0xB.
extern "C" void __cdecl EffectKind8C_SteerBack(void) {
    const unsigned char n = static_cast<unsigned char>(Effect4b_Steps + 1);
    Effect4b_Steps = n;
    if (n >= 8) {
        const U z = UL(rv::Bytes(ObjTrio) + 0x38) & 0xFFFFF000u;
        Field_State = rv::Bytes(ObjTrio);
        SetUL(rv::Bytes(ObjTrio) + 0x38, z);
        const U x = UL(rv::Bytes(ObjTrio) + 0x34) & 0xFFFFF000u;
        unsigned char* const s = S();
        SetUL(rv::Bytes(ObjTrio) + 0x34, x);
        s[1] = 0xF;
        Effect4b_Count = 0;
        Effect4b_Steps = 0;
        return;
    }
    switch (Effect4b_Direction) {
    case 0x1000: case 0x3000: Nudge(0x38, 0x800u); break;
    case 0x2000: case 0x6000: Nudge(0x34, 0xFFFFF800u); break;
    case 0x4000: case 0xC000: Nudge(0x38, 0xFFFFF800u); break;
    case 0x8000: case 0x9000: Nudge(0x34, 0x800u); break;
    default: break;
    }
    S()[1] = 0xB;
}

// original 0x48B070 (state 0xF): the pad's 0x20 - counted, +1 = 0x10.
extern "C" void __cdecl EffectKind8C_Ready(void) {
    if (Pad() != Sigma(0x20)) return;
    unsigned char* const s = S();
    CountUp();
    s[1] = 0x10;
}

// original 0x48B090 (state 0x10): the second count-down down - at 0 +1 =
// 0x14; else the pad's 0x20 - counted, the frame count 0, +1 = 0x11 and the
// counter 0x903849 up; else Kind8CHold's count (0x1C: +1 = 0x16, else 0x10).
extern "C" void __cdecl EffectKind8C_CountA(void) {
    const unsigned char t = static_cast<unsigned char>(Effect4b_TimerB - 1);
    Effect4b_TimerB = t;
    if (t == 0) {
        S()[1] = 0x14;
        return;
    }
    if (Pad() == Sigma(0x20)) {
        unsigned char* const s = S();
        CountUp();
        Effect4b_Presses = 0;
        s[1] = 0x11;
        Byte(::bof3::Img(at::kCounter2.v)) = static_cast<unsigned char>(Byte(::bof3::Img(at::kCounter2.v)) + 1);
        return;
    }
    const unsigned char n = static_cast<unsigned char>(Effect4b_Presses + 1);
    Effect4b_Presses = n;
    S()[1] = n >= 0x1C ? 0x16 : 0x10;
}

// original 0x48B100 (state 0x11): Kind8CHold(0x20, 0x12).
extern "C" void __cdecl EffectKind8C_CountPress(void) { Kind8CHold(0x20, 0x12); }

// original 0x48B140 (state 0x12): the second count-down down - at 0 it is set
// 0 again and +1 = 0x14; else the pad's 0x20 - counted, the frame count 0, +1
// = 0x10; else +1 = 0x11.
extern "C" void __cdecl EffectKind8C_CountB(void) {
    const unsigned char t = static_cast<unsigned char>(Effect4b_TimerB - 1);
    Effect4b_TimerB = t;
    if (t == 0) {
        unsigned char* const s = S();
        Effect4b_TimerB = 0;
        s[1] = 0x14;
        return;
    }
    if (Pad() == Sigma(0x20)) {
        unsigned char* const s = S();
        CountUp();
        Effect4b_Presses = 0;
        s[1] = 0x10;
        return;
    }
    S()[1] = 0x11;
}

// original 0x48B190 (state 0x14): the count within one of the target - the
// frame count 0, +1 = 0x15; else +1 = 0x16.
extern "C" void __cdecl EffectKind8C_Judge(void) {
    const std::int32_t c = static_cast<std::int32_t>(UL(At(::bof3::Img(Effect4b_Count_addr))) & 0xFFFFu);
    const std::int32_t t = Effect4b_Target;
    if (c >= t - 1 && c <= t + 1) {
        unsigned char* const s = S();
        Effect4b_Presses = 0;
        s[1] = 0x15;
        return;
    }
    S()[1] = 0x16;
}

// original 0x48B1D0 (state 0x15): the counter 0x903848 0, the chapter's step
// byte 0xF; Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind8C_Pass(void) {
    Byte(::bof3::Img(at::kCounter.v)) = 0;
    MoveScript_PartyRecords[0x65] = 0xF;
    SH_CALL(Effect_Release)();
}

// original 0x48B1F0 (state 0x16): the chapter's step byte 8; Effect_Release (a
// tail jump). Every loss comes here: no press in the first window (the
// target 0), 0x1C frames without one in either window, or a count more than
// one off the target. DIVERGENCE DIV-0188: under BOF3X_NO_FAIL the state
// ends as a judged pass instead - the frame count 0 (EffectKind8C_Judge's
// store) and then state 0x15's writes, the counter 0x903848 0 and the step
// byte 0xF.
extern "C" void __cdecl EffectKind8C_Fail(void) {
    if (Cheats_MinigameNoFail()) {
        Effect4b_Presses = 0;
        EffectKind8C_Pass();
        return;
    }
    MoveScript_PartyRecords[0x65] = 8;
    SH_CALL(Effect_Release)();
}

void EffectKind8c_Inject() {
    BOF3_INJECT(EffectKind8B_Run);
    BOF3_INJECT(EffectKind8B_Start);
    BOF3_INJECT(EffectKind8B_Wait);
    BOF3_INJECT(EffectKind8B_Pose);
    BOF3_INJECT(EffectKind8B_Again);
    BOF3_INJECT(EffectKind8C_Run);
    BOF3_INJECT(EffectKind8C_Start);
    BOF3_INJECT(EffectKind8C_WaitA);
    BOF3_INJECT(EffectKind8C_PressA);
    BOF3_INJECT(EffectKind8C_WaitB);
    BOF3_INJECT(EffectKind8C_Check);
    BOF3_INJECT(EffectKind8C_Steer);
    BOF3_INJECT(EffectKind8C_SteerWait);
    BOF3_INJECT(EffectKind8C_SteerBack);
    BOF3_INJECT(EffectKind8C_Ready);
    BOF3_INJECT(EffectKind8C_CountA);
    BOF3_INJECT(EffectKind8C_CountPress);
    BOF3_INJECT(EffectKind8C_CountB);
    BOF3_INJECT(EffectKind8C_Judge);
    BOF3_INJECT(EffectKind8C_Pass);
    BOF3_INJECT(EffectKind8C_Fail);
    // DIV-0188: with the switch on, the well's losing state must be ours -
    // left as Capcom's (BOF3X_ORIGINAL / BOF3X_OFF), a loss would stand and
    // the cheat would be silently half off.
    if (Cheats_MinigameNoFail() && !bof3::IsEnabled(bof3::addr::EffectKind8C_Fail))
        bof3::Fatal("BOF3X_NO_FAIL=1 needs EffectKind8C_Fail ours, and it is disabled");
}
