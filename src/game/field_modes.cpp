// The field's mode handlers (docs/field-modes.md). Field_ModeDispatch
// 0x56D690 is slot 0 of the current scenario chapter's vtable - the PSX's
// thunk 0x801A8834 - and the two tail dispatchers after it; scenario 16, the
// attract sequence's demo (PSX SCENA16.EMI), is a state machine of eleven
// handlers under its slot 0; Scenario_CallA 0x5341A0 is the chapter's call
// table's thunk (PSX 0x801C2DE8). Every call goes through field_modes::g, so
// that the start-up fuzz (field_modes_fuzz.cpp) tests each function alone.
#include "game/field_modes.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/field_modes_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_modes {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

const Callees kOriginals = {
    Scenario_CallA,
    reinterpret_cast<void (__cdecl*)(int, int, int, int)>(kChangeArea),
    reinterpret_cast<void (__cdecl*)(int)>(kStartMusic),
    File_LoadDone,
    Task_Sleep,
    reinterpret_cast<void (__cdecl*)()>(kTaskEnd),
    ObjTrio_SetBit40,
    Flags_Test,
    Flags_Set,
    ScriptFlags_Set40,
    MapView_SetElevation,
    Kind2_Place,
    reinterpret_cast<unsigned char (__cdecl*)()>(kEffectAlloc),
    reinterpret_cast<void (__cdecl*)(int)>(kPlaceParty),
    Sound_PlayEffect,
    Music_FadeOutStop,
    Transition_Start,
    Music_Play,
    Text_DrawAt,
    reinterpret_cast<void (__cdecl*)(unsigned short)>(kOpenScript),
    reinterpret_cast<void (__cdecl*)()>(kViewShift),
    ClutStrip_FadeTo,
    ClutStrip_Restore,
    Scena16_Area1F,
    Scena16_Area04,
    Scena16_Area02,
    Field_ModeTail,
};
Callees g = kOriginals;

}  // namespace field_modes

namespace {

using namespace field_modes;
using VoidFn = void (__cdecl*)();

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
VoidFn Entry(std::uint32_t table, int index) {
    return reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(table + 4u * static_cast<std::uint32_t>(index))))));
}
signed char Signed(std::uint32_t address) { return static_cast<signed char>(At(address)[0]); }

unsigned char& Step() { return At(kStep)[0]; }
std::uint16_t Timer() { return Word(At(kTimer)); }
void SetTimer(unsigned v) { SetWord(At(kTimer), v); }
unsigned char* FlagBits() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(kFlagsPtr))))); }
// The three camera angles, Camera_Angles[0..2].
unsigned char* AngleAt(unsigned i) { return reinterpret_cast<unsigned char*>(Camera_Angles) + 2 * i; }
short Angle(unsigned i) { return static_cast<short>(Word(AngleAt(i))); }

// A text line of the script pool: entry `id`'s u16 offset from the pool's
// base, drawn at (x, y) in colour 0, whole (count 0xFF).
void Line(int x, int y, unsigned id) {
    const std::uint32_t text = bof3::addr::MessagePools + Word(At(bof3::addr::MessagePools + 2 * id));
    g.text(x, y, 0, 0xFF, reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(text)));
}

// Takes an effect slot and, when there is one, sets its record up: kind 0x13
// at (x, the second and the third camera angle) with +9 = `mode`. As the
// original has it: the slot byte is stored first and the record indexed by
// the byte read back; x is computed by the caller after the slot is taken
// (the camera read after the call); +0 is set to 1.
void PlaceEffect(int (*x)(), unsigned char mode) {
    const unsigned char slot = g.effect_alloc();
    At(kSlot)[0] = slot;
    if (slot == 0xFF) return;
    unsigned char* const e = Effect_Objects + (static_cast<std::uint32_t>(At(kSlot)[0]) << 7);
    const int ex = x();
    const int ey = Angle(1);
    e[0] = 1;
    e[5] = 0x13;
    SetLong(e + 0x64, ex);
    SetLong(e + 0x68, ey);
    SetLong(e + 0x6C, Angle(2));
    e[9] = mode;
}

}  // namespace

// original 0x56D690: slot 0 of the current chapter's vtable, then the tail
// dispatcher (PSX thunk 0x801A8834, which calls slot 0 and 0x801A8BF8).
// As the original has it: the chapter Cond_ByteFA is signed and unchecked;
// the table is read afresh; the tail is a jump, here a call.
extern "C" void __cdecl Field_ModeDispatch(void) {
    const auto vtable = static_cast<std::uint32_t>(Long(At(kScenarioTables + 4u * static_cast<std::uint32_t>(static_cast<int>(Cond_ByteFA)))));
    Entry(vtable, 0)();
    g.mode_tail();
}

// original 0x56D8B0: a tail jump through the two-entry table 0x662CD0 on the
// s8 at 0x9039F2 (PSX 0x801A8BF8 on 0x801448E6): 0 is the set-up 0x56D8C0,
// which sets it to 1; 1 is Field_ModeTailRun. Unchecked.
extern "C" void __cdecl Field_ModeTail(void) { Entry(kTailTable, Signed(kTailPhase))(); }

// original 0x56D920: a tail jump through 0x662CE8 on the s8 at 0x9039F3 (PSX
// 0x801A8CB4 on 0x801448E7); entry 0 is a bare ret. Unchecked.
extern "C" void __cdecl Field_ModeTailRun(void) { Entry(kTailRunTable, Signed(kTailKind))(); }

// original 0x5341A0: entry (n & 0xFF) of the current chapter's call table
// (0x660B84[chapter], PSX Scenario_CallA 0x801C2DE8 through 0x801CDC4C).
// Kept a tail jump: the entries take the caller's arguments where they lie,
// and 153 call sites push anything from one argument to several (cleaned up
// later, some of them together), so no C++ signature forwards them all. None
// of the 153 reads eax after the call (capstone, to the first branch or
// write of eax); the jump passes the entry's through regardless.
extern "C" __attribute__((naked)) void __cdecl Scenario_CallA(unsigned) {
    asm("movsbl 0x8034E0, %eax\n\t"          // Cond_ByteFA, the chapter
        "movl 4(%esp), %ecx\n\t"
        "movl 0x660B84(,%eax,4), %edx\n\t"   // field_modes::kCallATables
        "andl $0xFF, %ecx\n\t"
        "jmp *(%edx,%ecx,4)");
}

// original 0x56B2A0: scenario 16's vtable slot 0 (PSX 0x801F6C90): a tail
// jump through 0x6619FC on its state, the s8 0x8034E2 - 0 Scena16_Start,
// 1 Scena16_EnterArea, 2 Scena16_Run. Unchecked; entries 3.. of the same
// words are Scena16_Run's table.
extern "C" void __cdecl Scena16_Frame(void) { Entry(kStateTable, Signed(kState))(); }

// original 0x56B2B0: the demo's first frame (PSX 0x801F6CCC): the chapter's
// call-table entry 0, the pass flags 0x1F, the area change to area 4 at
// (0x1A0000, 0x88000) with facing 5 - and the same four written into the
// pending change -, the script flags 0x240, the call 0x587A20(6), then a
// frame at a time until File_LoadDone; the counters cleared and state 1.
// As the original has it: the pass flags are stored after Scenario_CallA and
// before the area change; the wait asks File_LoadDone before its first sleep.
extern "C" void __cdecl Scena16_Start(void) {
    g.call_a(0);
    Draw_PassFlags = 0x1F;
    g.change_area(4, 0x1A0000, 0x88000, 5);
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x240);
    SetWord(At(kPending), 4);
    SetLong(At(kPending + 4), 0x1A0000);
    SetLong(At(kPending + 8), 0x88000);
    At(kPending + 3)[0] = 5;
    g.start_music(6);
    while (g.load_done() == 0) g.sleep(1);
    SetLong(At(kCounters), 0);
    At(kState)[0] = 1;
}

// original 0x56B340: scenario 16's state 1 (PSX 0x801F6D90): the set-up of
// the area just entered - 0x1F, 4 and 2 each have one, the badge off in area
// 0x1F and on in 4 and 2 - then state 2 whatever the area.
extern "C" void __cdecl Scena16_EnterArea(void) {
    switch (Game_AreaNumber) {
    case 0x1F:
        g.area_1f();
        At(kState)[0] = 2;
        At(kBadge)[0] = 0;
        return;
    case 4:
        g.area_04();
        At(kState)[0] = 2;
        At(kBadge)[0] = 1;
        return;
    case 2:
        g.area_02();
        At(kBadge)[0] = 1;
        At(kState)[0] = 2;
        return;
    default:
        At(kState)[0] = 2;
        return;
    }
}

// original 0x56B3A0: area 0x1F's set-up (PSX 0x801F6E30): the camera to
// angles (0x100, 0, 0) and distance 0x100, ObjTrio_SetBit40, and once - on
// flag 1 of the bits at 0x929ED0 - the pass flags 0, the flag set, run 3.
// As the original has it: the flag pointer is read afresh for each call; the
// pass flags get the test's result byte (0 on that path).
extern "C" void __cdecl Scena16_Area1F(void) {
    SetWord(AngleAt(2), 0);
    SetWord(AngleAt(0), 0x100);
    SetWord(AngleAt(1), 0);
    Camera_Distance = 0x100;
    g.obj_trio();
    const unsigned char seen = g.flags_test(FlagBits(), 1);
    if (seen != 0) return;
    Draw_PassFlags = seen;
    g.flags_set(FlagBits(), 1);
    MoveScript_Var7 = 3;
}

// original 0x56B400: area 4's set-up (PSX 0x801F6EB0): once, on flag 0, the
// pass flags 0, the flag set, ScriptFlags_Set40 and run 2; then, while
// Cond_ByteFD is 2, the pass flags 0x1F and ObjTrio_SetBit40 (a tail jump).
extern "C" void __cdecl Scena16_Area04(void) {
    const unsigned char seen = g.flags_test(FlagBits(), 0);
    if (seen == 0) {
        Draw_PassFlags = seen;
        g.flags_set(FlagBits(), 0);
        g.script_flags();
        MoveScript_Var7 = 2;
    }
    if (Cond_ByteFD != 2) return;
    Draw_PassFlags = 0x1F;
    g.obj_trio();
}

// original 0x56B450: area 2's set-up (PSX 0x801F6F30). Once, on flag 2: the
// counters cleared, elevation 0x300, Kind2_Place(0), the first angle + 0xAA,
// an effect of mode 0x40 at the angle before that step, ObjTrio_SetBit40,
// the dwords 0x802D74 / 0x802D78 = 0x330000 / 0x400000, flag 2 set. Else,
// once, on flag 10: flag 10 set, 0x531F90(0), and Field_Kind2X / Z =
// 0x260000 / 0x1B8000.
// As the original has it: the effect's x is the angle read after the slot is
// taken, less 0xAA; the flag pointer is read afresh for each call.
extern "C" void __cdecl Scena16_Area02(void) {
    if (g.flags_test(FlagBits(), 2) == 0) {
        SetLong(At(kCounters), 0);
        g.set_elevation(0x300);
        g.kind2_place(0);
        SetWord(AngleAt(0), static_cast<unsigned>(Word(AngleAt(0))) + 0xAA);
        PlaceEffect([] { return Angle(0) - 0xAA; }, 0x40);
        g.obj_trio();
        SetLong(At(0x802D74), 0x330000);
        SetLong(At(0x802D78), 0x400000);
        g.flags_set(FlagBits(), 2);
        return;
    }
    if (g.flags_test(FlagBits(), 0xA) != 0) return;
    g.flags_set(FlagBits(), 0xA);
    g.place_party(0);
    Field_Kind2X = 0x260000;
    Field_Kind2Z = 0x1B8000;
}

// original 0x56B560: scenario 16's state 2 (PSX 0x801F7144): a tail jump
// through 0x661A08 on MoveScript_Var7 (s8, unchecked) - 0 a bare ret,
// 1 Scena16_End, 2..4 the three scenes, 5 0x43C9F0.
extern "C" void __cdecl Scena16_Run(void) { Entry(kRunTable, MoveScript_Var7)(); }

// original 0x56B570: the demo's end, run 1 (PSX 0x801F7188). Once the wait
// word is 0: the badge and the pass flags off, the CLUT strip restored, sound
// effects 0x213 and 0x214, the music stopped over 10 frames unless none
// plays (then marked none), and the task ended (a tail jump to 0x5A99AD,
// which does not return). The PSX restarts the area's music from its table
// and sets the volume where the port stops it (docs/field-modes.md 3.4).
extern "C" void __cdecl Scena16_End(void) {
    if (MoveScript_WaitWordDA != 0) return;
    At(kBadge)[0] = 0;
    Draw_PassFlags = 0;
    g.clut_restore();
    g.sound(0x213);
    g.sound(0x214);
    if (At(kMusicTrack)[0] != 0xFF) {
        g.music_stop(0xA);
        At(kMusicTrack)[0] = 0xFF;
    }
    g.task_end();
}

// original 0x56B5D0: run 2, the first scene (PSX 0x801F7230), a step machine
// on the u8 0x8034E5 - twelve steps, anything above does nothing. Two
// captions, pool entries 0x10 at (0x82, 0x64) and 0x11 at (0x44, 0x50), each
// held while the wait word is set and timed on the u16 0x8034E6; the
// transitions around them; the area change to area 4 at (0x440000, 0x80000);
// an effect when the counter byte 0x90384A reaches 0x23; and, when 0x90384B
// reaches 0x60, the change to area 0x1F and run 3.
// As the original has it: the step is read afresh after every call; the
// timer's increment (step 2) precedes the caption; steps 4 and 7 share the
// original's tail (Transition_Start(0), step + 1).
extern "C" void __cdecl Scena16_Scene2(void) {
    switch (Step()) {
    case 0:
        if (MoveScript_WaitWordDA != 0) return;
        g.transition(1);
        g.music_play(6, 8);
        At(0x9039A2)[0] |= 0x80;   // Field_ScriptFlags' low byte
        ++Step();
        return;
    case 1:
        Line(0x82, 0x64, 0x10);
        if (MoveScript_WaitWordDA != 0) return;
        SetTimer(0);
        ++Step();
        return;
    case 2:
        SetTimer(Timer() + 1u);
        Line(0x82, 0x64, 0x10);
        if (Timer() != 0x7F) return;
        g.transition(0);
        ++Step();
        SetTimer(0);
        return;
    case 3:
        if (MoveScript_WaitWordDA != 0) {
            Line(0x82, 0x64, 0x10);
            return;
        }
        Draw_PassFlags = 0x1F;
        g.script_flags();
        g.obj_trio();
        g.transition(1);
        ++Step();
        At(kCounters + 3)[0] = 1;
        SetTimer(0);
        return;
    case 4:
        if (At(kCounters + 3)[0] != 0x54) return;
        g.transition(0);
        ++Step();
        return;
    case 5:
        if (MoveScript_WaitWordDA != 0) return;
        Draw_PassFlags = 0;
        g.transition(1);
        ++Step();
        return;
    case 6:
        Line(0x44, 0x50, 0x11);
        if (MoveScript_WaitWordDA != 0) return;
        SetTimer(0x7F);
        ++Step();
        return;
    case 7:
        Line(0x44, 0x50, 0x11);
        SetTimer(Timer() - 1u);
        if (Timer() != 0) return;
        g.transition(0);
        ++Step();
        return;
    case 8:
        if (MoveScript_WaitWordDA != 0) {
            Line(0x44, 0x50, 0x11);
            return;
        }
        g.change_area(4, 0x440000, 0x80000, 5);
        ++Step();
        return;
    case 9:
        if (Cond_ByteFD != 2) return;
        Draw_PassFlags = 0x1F;
        ++Step();
        return;
    case 10:
        if (At(kCounters + 2)[0] != 0x23) return;
        PlaceEffect([] { return Angle(0) + 0x180; }, 0x60);
        ++Step();
        return;
    case 11:
        if (At(kCounters + 3)[0] != 0x60) return;
        g.change_area(0x1F, 0x70000, 0x140000, 0);
        At(0x937F98)[0] = 0xFF;
        g.music_stop(0x40);
        At(kCounters + 2)[0] = 0;
        MoveScript_Var7 = 3;
        Step() = 0;
        return;
    default:
        return;
    }
}

namespace {

// Scena16_Scene3's tail, after every step and after none: once the map
// view's focus is 0x4400, it steps the view back 0x1E0000 in three places,
// the word 0x7E068A back 0x1E, the focus to 0x6200, and runs 0x56FCA0 (a
// tail jump).
void Scene3Tail() {
    if (MapView_FocusZ != 0x4400) return;
    const std::int32_t kind2_z = Field_Kind2Z;
    const std::int32_t d = Long(At(0x802038));
    SetWord(At(0x7E068A), static_cast<unsigned>(Word(At(0x7E068A))) - 0x1Eu);
    Field_Kind2Z = static_cast<long>(static_cast<std::uint32_t>(kind2_z) - 0x1E0000u);
    const std::int32_t e = Long(At(0x7E0978));
    MapView_FocusZ = 0x6200;
    SetLong(At(0x802038), static_cast<std::int32_t>(static_cast<std::uint32_t>(d) - 0x1E0000u));
    SetLong(At(0x7E0978), static_cast<std::int32_t>(static_cast<std::uint32_t>(e) - 0x1E0000u));
    g.view_shift();
}

}  // namespace

// original 0x56B950: run 3, the second scene (PSX 0x801F7790): ten steps on
// the same step byte. Caption 0x02 at (0x4C, 0x50) held and timed out;
// the change's transition, the counter 0x90384A to 0x30 and an effect of
// mode 0xFF; then caption 0x03 with the CLUT strip faded in over 0x20 frames,
// held, faded out over 0x1E and restored; then the music, the change to area
// 2 at (0x340000, 0x430000) with facing 3, and run 4. Every step ends in
// Scene3Tail, and so does a step above 9.
// As the original has it: the fade level passed is the timer's low byte read
// back from memory; step 5 restores the strip on the frame the timer reaches
// 0x20 and fades on the others (0x20 itself is never a fade level); step 7
// counts down before it draws, restoring instead of drawing at 0.
extern "C" void __cdecl Scena16_Scene3(void) {
    switch (Step()) {
    case 0:
        if (MoveScript_WaitWordDA != 0) break;
        g.transition(1);
        ++Step();
        break;
    case 1:
        Line(0x4C, 0x50, 0x02);
        if (MoveScript_WaitWordDA != 0) break;
        ++Step();
        SetTimer(0x7F);
        break;
    case 2:
        Line(0x4C, 0x50, 0x02);
        SetTimer(Timer() - 1u);
        if (Timer() != 0) break;
        g.transition(0);
        ++Step();
        break;
    case 3:
        if (MoveScript_WaitWordDA != 0) {
            Line(0x4C, 0x50, 0x02);
            break;
        }
        g.transition(1);
        At(kCounters + 2)[0] = 0x30;
        Draw_PassFlags = 0x1F;
        PlaceEffect([] { return Angle(0) + 0x1C0; }, 0xFF);
        ++Step();
        break;
    case 4:
        if (MoveScript_WaitWordDA != 0) break;
        ++Step();
        SetTimer(0);
        break;
    case 5: {
        Line(0x4C, 0x50, 0x03);
        const auto t = static_cast<std::uint16_t>(Timer() + 1u);
        SetTimer(t);
        if (t == 0x20) {
            g.clut_restore();
            ++Step();
            SetTimer(0x7F);
        } else {
            g.clut_fade(At(kTimer)[0]);
        }
        break;
    }
    case 6:
        Line(0x4C, 0x50, 0x03);
        SetTimer(Timer() - 1u);
        if (Timer() != 0) break;
        ++Step();
        SetTimer(0x20);
        break;
    case 7:
        SetTimer(Timer() - 1u);
        if (Timer() == 0) {
            g.clut_restore();
            ++Step();
            SetTimer(0x1E);
            break;
        }
        Line(0x4C, 0x50, 0x03);
        g.clut_fade(At(kTimer)[0]);
        break;
    case 8:
        SetTimer(Timer() - 1u);
        if (Timer() == 0) ++Step();
        break;
    case 9:
        g.music_play(2, 0x20);
        g.change_area(2, 0x340000, 0x430000, 3);
        MoveScript_Var7 = 4;
        Step() = 0;
        break;
    default:
        break;
    }
    Scene3Tail();
}

namespace {

// Scena16_Scene4's zoom, steps 4 and 7: while the timer runs, one down and
// the camera distance from it.
void DistanceFromTimer(unsigned (*distance)(std::uint16_t)) {
    const std::uint16_t t = Timer();
    if (t == 0) {
        ++Step();
        return;
    }
    Camera_Distance = static_cast<short>(distance(t));
    SetTimer(t - 1u);
}

}  // namespace

// original 0x56BCC0: run 4, the third scene (PSX 0x801F7CC4): thirteen steps.
// Script message 1 opened (Field_Request 2) and waited out; on counter 3,
// message 2; on counter 4, 0x130 frames of caption 0x03 at (0x64, 0x50)
// with the CLUT strip faded in over the first 0x20 and out over the last
// 0x20, and the camera pulled in; on counters 5 and 8, two effects; a second
// zoom over 0x200 frames; on counter 0xB, the transition out, the music
// stopped, caption 0x04 at (0x5E, 0x64) held and timed out; then run 1.
// As the original has it: step 4 computes 0x130 - timer as a u16 into the
// word 0x903850 and reads it back after the caption; below 0x20 it fades to
// that level, at 0x20 restores, above 0x9F fades to 0xBF - it (as a byte),
// at 0xBF restores without drawing, above draws nothing; the distance is
// (0x67FFF90 - (timer - 1) * 0x57943) >> 16 (unsigned) in step 4 and
// ((13 * timer) << 14) >> 16 (unsigned, wrapping) in step 7, both stored as
// words, and a zero timer steps on instead.
extern "C" void __cdecl Scena16_Scene4(void) {
    switch (Step()) {
    case 0:
        if (MoveScript_WaitWordDA != 0) return;
        g.open_script(1);
        Field_Request = 2;
        ++Step();
        return;
    case 1:
        if (Field_Request == 2) return;
        ++Step();
        return;
    case 2:
        if (At(kCounters)[0] != 3) return;
        g.open_script(2);
        ++Step();
        Field_Request = 2;
        return;
    case 3:
        if (At(kCounters)[0] != 4) return;
        SetTimer(0x130);
        ++Step();
        return;
    case 4: {
        const auto into = static_cast<std::uint16_t>(0x130u - Timer());
        SetWord(At(kSlot), into);
        if (into < 0xBF) {
            Line(0x64, 0x50, 0x03);
            const std::uint16_t back = Word(At(kSlot));
            if (back < 0x20) g.clut_fade(static_cast<unsigned char>(back));
            else if (back == 0x20) g.clut_restore();
            else if (back > 0x9F) g.clut_fade(static_cast<unsigned char>(0xBF - static_cast<unsigned char>(back)));
        } else if (into == 0xBF) {
            g.clut_restore();
        }
        DistanceFromTimer([](std::uint16_t t) {
            return (0x67FFF90u - static_cast<std::uint32_t>(static_cast<std::uint16_t>(t - 1u)) * 0x57943u) >> 16;
        });
        return;
    }
    case 5:
        if (At(kCounters)[0] != 5) return;
        PlaceEffect([] { return -0x34A; }, 0x60);
        ++Step();
        return;
    case 6:
        if (At(kCounters)[0] != 8) return;
        PlaceEffect([] { return -0x2AC; }, 0x20);
        ++Step();
        SetTimer(0x200);
        return;
    case 7:
        DistanceFromTimer([](std::uint16_t t) { return (static_cast<std::uint32_t>(t) * 13u << 14) >> 16; });
        return;
    case 8:
        if (At(kCounters)[0] != 0xB) return;
        g.transition(0);
        ++Step();
        SetTimer(0);
        return;
    case 9:
        if (MoveScript_WaitWordDA != 0) return;
        Draw_PassFlags = 0;
        g.transition(1);
        g.music_stop(0x20);
        ++Step();
        return;
    case 10:
        Line(0x5E, 0x64, 0x04);
        if (MoveScript_WaitWordDA != 0) return;
        ++Step();
        SetTimer(0x7F);
        return;
    case 11:
        Line(0x5E, 0x64, 0x04);
        SetTimer(Timer() - 1u);
        if (Timer() != 0) return;
        g.transition(0);
        ++Step();
        return;
    case 12:
        if (MoveScript_WaitWordDA == 0) {
            MoveScript_Var7 = 1;
            Step() = 0;
            return;
        }
        Line(0x5E, 0x64, 0x04);
        return;
    default:
        return;
    }
}

// original 0x56C0A0: the first 16 colours of Gfx_ClutStrip from
// Gfx_ClutStripSource with every 5-bit channel clamped to `level` (a byte),
// then Gfx_ClutStripDirty + 1 (PSX 0x801F839C, the same loop).
// As the original has it, on both platforms: the channels are taken from bit
// 0 up and shifted in from the bottom, so the source's bits 0-4 land in
// 10-14 and 10-14 in 0-4 - red and blue trade places (docs/field-modes.md
// section 7); bit 15 is set when the result is not 0, whatever the source's.
extern "C" void __cdecl ClutStrip_FadeTo(unsigned level) {
    const unsigned cap = level & 0xFF;
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned source = Gfx_ClutStripSource[i];
        unsigned out = 0;
        for (unsigned shift = 0; shift < 15; shift += 5) {
            unsigned channel = (source >> shift) & 0x1F;
            if (cap < channel) channel = cap;
            out = out << 5 | channel;
        }
        const auto word = static_cast<std::uint16_t>(out);
        Gfx_ClutStrip[i] = word != 0 ? static_cast<std::uint16_t>(word | 0x8000) : word;
    }
    ++Gfx_ClutStripDirty;
}

// original 0x56C110: the first 16 colours of Gfx_ClutStrip copied back from
// Gfx_ClutStripSource, then Gfx_ClutStripDirty + 1 (PSX 0x801F8448).
extern "C" void __cdecl ClutStrip_Restore(void) {
    for (unsigned i = 0; i < 16; ++i) Gfx_ClutStrip[i] = Gfx_ClutStripSource[i];
    ++Gfx_ClutStripDirty;
}

void FieldModes_Inject() {
    if (bof3::WantsShadow("field_modes")) field_modes::SelfTest();
    BOF3_INJECT(Field_ModeDispatch);
    BOF3_INJECT(Field_ModeTail);
    BOF3_INJECT(Field_ModeTailRun);
    BOF3_INJECT(Scenario_CallA);
    BOF3_INJECT(Scena16_Frame);
    BOF3_INJECT(Scena16_Start);
    BOF3_INJECT(Scena16_EnterArea);
    BOF3_INJECT(Scena16_Area1F);
    BOF3_INJECT(Scena16_Area04);
    BOF3_INJECT(Scena16_Area02);
    BOF3_INJECT(Scena16_Run);
    BOF3_INJECT(Scena16_End);
    BOF3_INJECT(Scena16_Scene2);
    BOF3_INJECT(Scena16_Scene3);
    BOF3_INJECT(Scena16_Scene4);
    BOF3_INJECT(ClutStrip_FadeTo);
    BOF3_INJECT(ClutStrip_Restore);
}
