// The top-level task flow: the boot task WinMain starts (0x496B60) and the
// title's loader it hands over to (0x496C90); the field task 0x495800 with
// its mode-0 and mode-1 handlers, the play clock it ticks every frame, and
// the area-entry hub 0x594E60 under mode 1; the screen-transition task
// 0x495070 with its 21 kinds and the fade loops under them; and the small
// resets they share. docs/mode-flow.md.
//
// Every call goes through mode_flow::g (mode_flow_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike.
#include "game/mode_flow.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/mode_flow_callees.h"
#include "game/widescreen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace mode_flow {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Task_Sleep, Task_Exit, Task_Create, Task_ClearPrivate,
    Transition_FadeSub, Transition_FadeAdd, Transition_DrawTile, Window_ResetAll, Game_ClockTick,
    Transition_Start, Field_WaitTransition, Area_Enter, Area_EntryWalk, MoveScript_TintReset,
    Field_SlotRelease, Field_SlotsReleaseAll, Gfx_ClutStripRestore, File_LoadDone, Gfx_ClearRect,
    Gpu_GetTPage, Gpu_SetDrawMode, Gfx_CommitPrim, Gpu_SetTile, Gpu_SetSemiTrans,
    Field_PartyLoad, Field_ChangeArea, Fn<void (__cdecl*)(unsigned, unsigned)>(kEntryPoint), Field_ZoneCounterRoll,
    Field_Frame, Effect_ClearAll, Music_Play, LoadDatFile, Field_PartySetUp, Area_ZoneIdAt, Field_ViewReset,
    Gte_SetColorMatrix, Scenario_Start, Area_RunPlacement, Flags_Clear, Field_ModeDispatch, Field_PartyFirstFrame,
    MoveCmd_TestFB, Fn<unsigned char (__cdecl*)(unsigned)>(kPlaceParty), Field_MembersFrame, Port_DroppedCall,
    Title_Task, Gfx_ClearImage,
    {Transition_Kind00, Transition_Kind01, Transition_Kind02, Transition_Kind03, Transition_Kind04,
     Transition_Kind05, Transition_Kind06, Transition_Kind07, Transition_Kind08, Transition_Kind09,
     Transition_Kind10, Transition_Kind11, Transition_Kind12, Transition_Kind13, Transition_Kind14,
     Transition_Kind15, Transition_Kind16, Transition_Kind17, Transition_Kind18, Transition_Kind19,
     Transition_Kind20},
};
Callees g = kOriginals;

}  // namespace mode_flow

using namespace mode_flow;

namespace {

// The fades' six bytes (mode_flow_callees.h): all to one value. The original
// stores them 0x90393F, 3E, 3D, 0x9038AF, AE, AD with nothing in between.
void SetFadeHold(unsigned char v) {
    for (unsigned i = 0; i < 3; ++i) {
        At(at::kFadeHoldA)[i] = v;
        At(at::kFadeHoldB)[i] = v;
    }
}

// The 16-bit words the camera transitions add to in memory: `add word`, so
// wrapping at 0x10000 and never carrying into the next word.
void AddWord(std::uint32_t address, int delta) { SetWord(At(address), static_cast<unsigned>(Word(At(address)) + delta)); }
constexpr std::uint32_t kCameraYaw = 0x929EC8;     // Camera_Angles[0]
constexpr std::uint32_t kCameraAngle = 0x929ECC;   // Cond_AngleFB's low word

// An argument the original reads as a byte from its stack slot. Capcom's
// callers push whole registers (Transition_Start has 170 call sites, some
// `push eax` / `push ecx` with a byte loaded into the low 8 bits), and clang
// is entitled to assume an `unsigned char` argument arrives extended - so the
// byte is read from the slot itself, as the original's `movzx` does.
unsigned char SlotByte(const unsigned char& argument) { return *reinterpret_cast<const volatile unsigned char*>(&argument); }

}  // namespace

// ===========================================================================
// The screen transition (0x495040..0x495800)
// ===========================================================================

// original 0x495040 (PSX 0x8014EB68): unless MoveScript_WaitWordDA is set (a
// transition already running), the kind to the word 0x66C828 and task 2
// started at 0x495070.
//
// As the original has it: the kind is the argument's low byte, zero-extended
// to the word; the wait word is tested as 16 bits; a running transition makes
// the call a no-op (the kind is not stored).
extern "C" void __cdecl Transition_Start(unsigned char kind) {
    if (MoveScript_WaitWordDA != 0) return;
    SetWord(At(at::kTransitionKind), SlotByte(kind));
    g.task_create(2, reinterpret_cast<void*>(static_cast<std::uintptr_t>(kTransitionEntry)));
}

// original 0x495070: task 2's body (PSX 0x8014EC28). Builds a 21-entry table
// on its own stack and calls entry [0x66C828] - the kinds below, every one of
// which ends the task through Task_Exit.
//
// As the original has it: the word is read as 16 bits and not checked. Index
// 21 and above would call through the words above the table on its stack -
// its own return address first; ours aborts instead (CLAUDE.md rule 4). The
// callers pass constants 0..0x13 and GameMode_Field's area kinds (0xA, 0x14,
// 0 and the pending kind byte).
extern "C" void __cdecl Transition_Task(void) {
    const unsigned kind = Word(At(at::kTransitionKind));
    if (kind >= 21) bof3::Fatal("Transition_Task: kind %u, past the 21-entry table", kind);
    g.transitions[kind]();
}

// original 0x495620: a fade through a subtractive full-screen tile (the
// tpage's blend mode 2). The level starts at 0 for a positive step (fading to
// black) and at 0x7FFF for a negative one (fading in from black), and the
// tile is drawn a frame at a time until the level passes below zero. After a
// fade to black the six hold bytes are zeroed. Then the task ends.
//
// As the original has it: the step's sign is its low 16 bits' (the callers
// push dwords); the level lives in the argument slot of the step and only its
// low word is read or written by Transition_DrawTile; the task ends with a
// call to Task_Exit (which never returns in game).
extern "C" void __cdecl Transition_FadeSub(int step, unsigned semi, unsigned slot) {
    const bool out = static_cast<short>(step) > 0;
    std::int32_t level = out ? 0 : 0x7FFF;
    while (g.draw_tile(reinterpret_cast<short*>(&level), step, semi, slot, 2) == 0) g.sleep(1);
    if (out) SetFadeHold(0);
    g.task_exit();
}

// original 0x4956A0: the same through an additive tile (blend mode 1), so
// through white. A fade in (step <= 0) zeroes the hold bytes before the
// first frame; a fade out sets them to 0xFF after the last. A tail jump to
// Task_Exit.
extern "C" void __cdecl Transition_FadeAdd(int step, unsigned semi, unsigned slot) {
    const bool out = static_cast<short>(step) > 0;
    std::int32_t level = out ? 0 : 0x7FFF;
    if (!out) SetFadeHold(0);
    while (g.draw_tile(reinterpret_cast<short*>(&level), step, semi, slot, 1) == 0) g.sleep(1);
    if (out) SetFadeHold(0xFF);
    g.task_exit();
}

// original 0x495750: one frame of a fade - a draw mode under tpage
// (1, abr, 0x140, 0) committed to `slot`, then a 320 x 240 TILE at (0, 0) in
// the grey level >> 7, semi-transparent by `semi`, committed to `slot` too;
// then the step added to the level. Returns 1 once the level has gone
// negative (s16), else 0.
//
// As the original has it: the draw mode's texture window is 0 (a push left
// under the tpage call); the packet cursor is read after the tpage call for
// the draw mode and after the first commit for the tile; Gpu_SetTile runs
// before the colour and the corners are stored; the colour is the s16 level
// shifted right 7, its low byte, in all three channels; x, y 0.0 and w, h
// 320.0 and 240.0 (the PC tile's floats); the level is read before the
// primitive is built and added to after the second commit, as a 16-bit word.
extern "C" unsigned char __cdecl Transition_DrawTile(short* level, int step, unsigned semi, unsigned slot, unsigned abr) {
    const unsigned tpage = g.get_tpage(1, abr & 0xFF, 0x140, 0);
    g.draw_mode(Gfx_PacketNext, 0, 0, tpage & 0xFFFF, 0);
    g.commit(slot, 0xC);
    unsigned char* const tile = Gfx_PacketNext;
    g.set_tile(tile);
    const auto grey = static_cast<unsigned char>(static_cast<short>(*level) >> 7);
    tile[6] = grey;
    tile[5] = grey;
    tile[4] = grey;
    // DIV-0041: under a wide picture the fade covers it whole, (-53, 0) 426 x 240.
    const float wide = static_cast<float>(Widescreen_Live());
    SetLong(tile + 8, std::bit_cast<std::uint32_t>(0.0f - wide));   // x; 0.0f - wide, not -wide, which is -0.0f when wide is 0
    SetLong(tile + 0xC, 0);
    SetLong(tile + 0x14, std::bit_cast<std::uint32_t>(320.0f + 2 * wide));   // 0x43A00000, 320.0f
    SetLong(tile + 0x18, 0x43700000);   // 240.0f
    g.set_semi(tile, semi & 0xFF);
    g.commit(slot, 0x1C);
    *level = static_cast<short>(static_cast<std::uint16_t>(*level) + static_cast<std::uint16_t>(step));
    return static_cast<short>(*level) < 0 ? 1 : 0;
}

namespace {

// Kinds 10 and 17: the camera swings away while the screen fades to black
// (subtractive, slot 0). Each frame the tile is not yet done, the yaw and the
// angle word turn by 11 and the distance shrinks by 50, the map view asked to
// redraw; the frame the fade finishes, nothing moves.
void SwingOut(int step) {
    std::int32_t level = 0;
    while (g.draw_tile(reinterpret_cast<short*>(&level), step, 1, 0, 2) == 0) {
        AddWord(kCameraYaw, 0xB);
        AddWord(kCameraAngle, 0xB);
        Camera_Distance = static_cast<short>(Camera_Distance - 0x32);
        MapView_Redraw = 2;
        g.sleep(1);
    }
    g.task_exit();
}

// Kinds 11 and 18: the mirror. The camera is put at yaw 0xFEAB, angle 0x355;
// five frames of fade-in from black whatever the tile says; then every frame
// the camera turns back by 11 and the distance grows by 50 until it reaches
// 0x5DC exactly, the tile drawn until it reports done and not after. Then
// yaw 0xFD56, angle 0x200.
//
// As the original has it: the distance is compared for equality after the
// add, as 16 bits - a distance that does not come to 0x5DC in steps of 50
// turns the camera for ever; the stop is tested before the frame's sleep, so
// the last frame's turn is not slept on.
void SwingIn() {
    std::int32_t level = 0x7FFF;
    SetWord(At(kCameraYaw), 0xFEAB);
    SetWord(At(kCameraAngle), 0x355);
    MapView_Redraw = 2;
    for (int i = 0; i < 5; ++i) {
        g.draw_tile(reinterpret_cast<short*>(&level), -0x400, 1, 0, 2);
        g.sleep(1);
    }
    unsigned char done = g.draw_tile(reinterpret_cast<short*>(&level), -0x400, 1, 0, 2);
    for (;;) {
        const auto distance = static_cast<std::uint16_t>(Camera_Distance + 0x32);
        AddWord(kCameraYaw, -0xB);
        AddWord(kCameraAngle, -0xB);
        Camera_Distance = static_cast<short>(distance);
        MapView_Redraw = 2;
        if (distance == 0x5DC) break;
        g.sleep(1);
        if (done == 0) done = g.draw_tile(reinterpret_cast<short*>(&level), -0x400, 1, 0, 2);
    }
    SetWord(At(kCameraYaw), 0xFD56);
    SetWord(At(kCameraAngle), 0x200);
    MapView_Redraw = 2;
    g.task_exit();
}

}  // namespace

// The 21 entries of Transition_Task's table (originals 0x495130..0x4955A0).
// Unless the line says otherwise, each is one call: FadeSub (subtractive, to
// or from black) or FadeAdd (additive, to or from white) with a step - 0x800
// is 16 frames, 0x400 32, 0x200 64, 0x100 128, 0x2000 4 - semi 1 and slot 0.
// None of them is reached by the attract sequence except kinds 0 and 1.
extern "C" void __cdecl Transition_Kind00(void) { g.fade_sub(0x800, 1, 0); }     // 0x495130: to black
extern "C" void __cdecl Transition_Kind01(void) { g.fade_sub(-0x800, 1, 0); }    // 0x495150: from black
extern "C" void __cdecl Transition_Kind02(void) { g.fade_sub(0x2000, 1, 0); }    // 0x495170
extern "C" void __cdecl Transition_Kind03(void) { g.fade_sub(-0x2000, 1, 0); }   // 0x495190
extern "C" void __cdecl Transition_Kind04(void) { g.fade_sub(0x400, 1, 0); }     // 0x4951B0
extern "C" void __cdecl Transition_Kind05(void) { g.fade_sub(-0x400, 1, 0); }    // 0x4951D0
extern "C" void __cdecl Transition_Kind06(void) { g.fade_sub(0x800, 1, 2); }     // 0x4951F0: into OT slot 2
extern "C" void __cdecl Transition_Kind07(void) { g.fade_sub(-0x800, 1, 2); }    // 0x495210
extern "C" void __cdecl Transition_Kind08(void) { g.fade_add(0x800, 1, 0); }     // 0x495230: to white
extern "C" void __cdecl Transition_Kind09(void) { g.fade_add(-0x800, 1, 0); }    // 0x495250: from white
extern "C" void __cdecl Transition_Kind10(void) { SwingOut(0x400); }             // 0x495270
extern "C" void __cdecl Transition_Kind11(void) { SwingIn(); }                   // 0x4952D0
// 0x4953A0: the camera distance put at 0x5DC and the view asked to redraw
// (before the call), then a 16-frame fade in from black.
extern "C" void __cdecl Transition_Kind12(void) {
    Camera_Distance = 0x5DC;
    MapView_Redraw = 2;
    g.fade_sub(-0x800, 1, 0);
}
extern "C" void __cdecl Transition_Kind13(void) { g.fade_sub(0x200, 1, 0); }     // 0x4953D0
extern "C" void __cdecl Transition_Kind14(void) { g.fade_sub(-0x200, 1, 0); }    // 0x4953F0
extern "C" void __cdecl Transition_Kind15(void) { g.fade_add(0x200, 1, 0); }     // 0x495410
extern "C" void __cdecl Transition_Kind16(void) { g.fade_add(-0x200, 1, 0); }    // 0x495430
extern "C" void __cdecl Transition_Kind17(void) { SwingOut(0x200); }             // 0x495450
extern "C" void __cdecl Transition_Kind18(void) { SwingIn(); }                   // 0x4954B0, byte for byte kind 11's
extern "C" void __cdecl Transition_Kind19(void) { g.fade_sub(0x100, 1, 0); }     // 0x495580
// 0x4955A0: a 16-frame fade to black (subtractive) while the angle word spins
// by 0x20 a frame, kept inside 0..0xFFF; then the hold bytes zeroed.
//
// As the original has it: the angle is read as a dword, masked, and stored as
// a word - its upper word is left alone.
extern "C" void __cdecl Transition_Kind20(void) {
    std::int32_t level = 0;
    while (g.draw_tile(reinterpret_cast<short*>(&level), 0x800, 1, 0, 2) == 0) {
        SetWord(At(kCameraAngle), (static_cast<std::uint32_t>(Long(At(kCameraAngle))) + 0x20u) & 0xFFFu);
        g.sleep(1);
    }
    SetFadeHold(0);
    g.task_exit();
}

// ===========================================================================
// The field task (0x495800) and its modes 0 and 1
// ===========================================================================

// original 0x495800: task 0's body for a game - the demo (Title_StateStartDemo)
// and every started or loaded game (PSX GAME.EMI section 0 0x80198068). Mode
// and step 0, the task's private words and every window record cleared; then
// for ever: the handler of GameMode_Handlers[Game_Mode], the play clock, a
// frame's sleep.
//
// As the original has it: Game_Mode indexes the table unchecked, read afresh
// every frame (the handlers change it); the table is read from memory each
// time.
extern "C" void __cdecl Field_Task(void) {
    Game_Mode = 0;
    Game_Step = 0;
    g.clear_private();
    g.window_reset();
    for (;;) {
        reinterpret_cast<Handler>(GameMode_Handlers[Game_Mode])();
        g.clock_tick();
        g.sleep(1);
    }
}

// original 0x495840: Game_Mode 0, the start of a game (PSX 0x801980EC). The
// party loaded from slot 0; the area change requested to the saved position
// (area, flags, x, z at 0x903A04..0x903A0F); with the play clock not at zero
// - a loaded game - the track that was playing is the one the area wants;
// then mode 1.
//
// As the original has it: Draw_PassFlags and Draw_OtSlot are set before the
// party load; the saved position is read after it; bit 0x4040 of
// Field_ScriptFlags2 is set; eight bytes around the field zeroed and the
// current area made 0xFFFF (none) before Field_ChangeArea, so that the area
// is always "changed"; the clock read as four bytes after the change.
extern "C" void __cdecl GameMode_Start(void) {
    Draw_PassFlags = 0x1F;
    Draw_OtSlot = 6;
    g.party_load(0);
    const unsigned char flags = At(at::kStartArea)[3];
    const std::int32_t z = Long(At(at::kStartArea + 8));
    const std::int32_t x = Long(At(at::kStartArea + 4));
    Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 | 0x4040);
    const std::uint16_t area = Word(At(at::kStartArea));
    At(0x905B82)[0] = 0;   // Field_EdgeBits + 2
    At(0x7E0941)[0] = 0;   // Sprite_Kind2 + 1..3
    At(0x7E0942)[0] = 0;
    At(0x7E0943)[0] = 0;
    At(0x904AE5)[0] = 0;
    At(0x904AAA)[0] = 0;
    At(0x929EC1)[0] = 0;   // the byte after Field_MemberCount
    At(0x9036D0)[0] = 0;
    Game_AreaNumber = 0xFFFF;
    g.change_area(area, x, z, flags);
    const unsigned char* const clock = At(at::kClock);
    if (clock[3] != 0 || clock[2] != 0 || clock[1] != 0 || clock[0] != 0) At(at::kAreaTrack)[0] = At(at::kMusicTrack)[0];
    At(at::kMusicTrack)[0] = 0xFF;
    Game_Mode = 1;
}

// original 0x495900: Game_Mode 1, entering the pending area (PSX 0x801981E8).
// Area_Enter with Field_ChangeArea's cells; then, unless the area's input
// flags have bit 8 (mode 8 at once): the pending kind's transition - none for
// 0xFF, only Draw_PassFlags off for 0xFE, else Transition_Start(kind) and
// Field_WaitTransition(1), then with input bit 1 the area's entry list
// (0x5951D0) at the leader's position. Then the kind GameMode_Field will hand
// Transition_Start on the next area change - 0xA with input bit 1, 0x14 for
// area 0xBD, else 0 - the zone counter rolled except in area 0xBD, bit 0x40
// of Field_ScriptFlags2 off, Field_Request 0 and mode 2, the field.
//
// As the original has it: bit 3 of Field_ScriptFlags2 is cleared after the
// four cells are read and before the call; the flags go to Area_Enter as a
// byte (the original's eax holds Field_Task's mode index above it, 1); the
// input flags are read afresh after every call; the area number is read once
// for both 0xBD tests, after the last call before them.
extern "C" void __cdecl GameMode_Enter(void) {
    const unsigned char flags = At(at::kPendingFlags)[0];
    const std::int32_t z = Long(At(at::kPendingZ));
    const std::int32_t x = Long(At(at::kPendingX));
    Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0xFFF7);
    g.area_enter(Word(At(at::kPendingArea)), x, z, flags);
    if (Field_InputFlags & 8) {
        Game_Mode = 8;
        return;
    }
    const unsigned char kind = At(at::kPendingKind)[0];
    if (kind == 0xFE) {
        Draw_PassFlags = 0;
    } else if (kind != 0xFF) {
        g.transition(kind);
        g.wait_transition(1);
        if (Field_InputFlags & 1) g.entry_point(Word(At(at::kKind2XHigh)), Word(At(at::kKind2ZHigh)));
    }
    const bool bit0 = (Field_InputFlags & 1) != 0;
    const std::uint16_t area = Game_AreaNumber;
    if (bit0) At(at::kAreaTransition)[0] = 0xA;
    else if (area == 0xBD) At(at::kAreaTransition)[0] = 0x14;
    else At(at::kAreaTransition)[0] = 0;
    if (area != 0xBD) g.zone_roll(1);
    Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0xFFBF);
    Field_Request = 0;
    Game_Mode = 2;
}

// original 0x4967F0 (PSX 0x80199BC4, call-disputed): a frame's sleep, then
// while the transition task's wait word is set - or, with `run`, always - the
// field frame and another sleep; with neither, one more sleep and return.
//
// As the original has it: `run` is tested every time round, not only on the
// first pass; with `run` set the loop ends only on a frame the wait word is
// 0 after the field frame (never on the first test); the word is read afresh
// after each call.
extern "C" void __cdecl Field_WaitTransition(unsigned char run_argument) {
    const unsigned char run = SlotByte(run_argument);
    for (;;) {
        g.sleep(1);
        if (MoveScript_WaitWordDA == 0 && run == 0) {
            g.sleep(1);
            return;
        }
        g.field_frame();
        if (MoveScript_WaitWordDA == 0) return;
    }
}

// original 0x496870: the play clock and two countdowns, once a frame from
// Field_Task (PSX 0x80199CAC, gap pairing - the same code). Unless
// Field_Request is 9: the clock's frames count to 30, seconds and minutes to
// 60, hours to 99, and at 99:59:59 it stops; then each countdown ticks.
//
// As the original has it: the stop is hours == 0x63 && minutes == 0x3B &&
// seconds == minutes, exactly (hours above 99 keep counting); the rollover
// tests are unsigned (>= 30, >= 60); at 99 hours a rollover of the minutes
// holds the clock at 99:59:59 rather than counting the hour.
namespace {
// A countdown: frames 29..0, then seconds and minutes 59..0, then hours.
//
// As the original has it: all four bytes zero means stopped; each byte is
// decremented and tested as s8 (so 0x80 and above count as below zero); when
// the minutes underflow at hour 0 the minutes and seconds are zeroed but the
// frames byte was already reloaded with 29 - so a countdown never reaches
// all-zero by itself, it runs 0:00:00 round every 30 frames (the PSX's
// 0x80199D9C is the same).
void CountDown(std::uint32_t address) {
    unsigned char* const t = At(address);
    const unsigned char hours = t[0];
    if (hours == 0 && t[1] == 0 && t[2] == 0 && t[3] == 0) return;
    t[3] = static_cast<unsigned char>(t[3] - 1);
    if (static_cast<signed char>(t[3]) >= 0) return;
    t[3] = 0x1D;
    t[2] = static_cast<unsigned char>(t[2] - 1);
    if (static_cast<signed char>(t[2]) >= 0) return;
    t[2] = 0x3B;
    t[1] = static_cast<unsigned char>(t[1] - 1);
    if (static_cast<signed char>(t[1]) >= 0) return;
    if (hours == 0) {
        t[1] = 0;
        t[2] = 0;
    } else {
        t[1] = 0x3B;
        t[0] = static_cast<unsigned char>(hours - 1);
    }
}
}  // namespace
extern "C" void __cdecl Game_ClockTick(void) {
    if (Field_Request == 9) return;
    unsigned char* const c = At(at::kClock);
    const unsigned char hours = c[0], minutes = c[1], seconds = c[2];
    if (!(hours == 0x63 && minutes == 0x3B && seconds == minutes)) {
        const auto frames = static_cast<unsigned char>(c[3] + 1);
        c[3] = frames;
        if (frames >= 0x1E) {
            const auto s = static_cast<unsigned char>(seconds + 1);
            c[3] = 0;
            c[2] = s;
            if (s >= 0x3C) {
                const auto m = static_cast<unsigned char>(minutes + 1);
                c[2] = 0;
                c[1] = m;
                if (m >= 0x3C) {
                    if (hours < 0x63) {
                        c[1] = 0;
                        c[0] = static_cast<unsigned char>(hours + 1);
                    } else {
                        c[1] = 0x3B;
                        c[2] = 0x3B;
                    }
                }
            }
        }
    }
    CountDown(at::kCountdownA);
    CountDown(at::kCountdownB);
}

// original 0x594E60: entering an area, once per area change from
// GameMode_Enter (PSX 0x801A0AA8, callers pairing). docs/mode-flow.md
// section 4 has it step by step.
//
// As the original has it: flag 0x80 of the entry flags sets or clears bit 15
// of Field_ScriptFlags2 at once and is kept (the original writes it over the
// flags' own argument slot) for the end; the area's DAT is loaded only when
// the area number differs AND Field_Request is 5; the flags go on whole to
// Field_PartySetUp; Area_Descriptors[Game_AreaNumber] is read afresh after
// every call; an area whose descriptor has no colour matrix at +0x38 gets
// the default one at 0x6BE090 and the light pair at 0x663B20; the scenario
// chapter steps by one with 0x10 skipped (to 0x11) when Field_StatusBits has
// bit 7; the descriptor's +0x40, when not null, is called with nothing.
extern "C" void __cdecl Area_Enter(unsigned area, int x, int z, unsigned flags) {
    const bool drop_in = (flags & 0x80) != 0;
    if (drop_in) Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 | 0x8000);
    else Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0x7FFF);
    g.effect_clear();
    g.tint_reset();
    const std::uint16_t previous = Game_AreaNumber;
    const unsigned char zone = Cond_ByteFD;
    const long kind2_x = Field_Kind2X, kind2_z = Field_Kind2Z;
    SetWord(At(at::kLastArea), previous);
    At(at::kLastZone)[0] = zone;
    SetLong(At(at::kLastKind2Z), kind2_z);
    SetLong(At(at::kLastKind2X), kind2_x);
    const unsigned char track = At(at::kAreaTrack)[0];
    if (track != 0xFF) g.music_play(track, 8);
    else At(at::kMusicTrack)[0] = 0xFF;
    if (Game_AreaNumber != static_cast<std::uint16_t>(area) && Field_Request == 5) {
        Game_AreaNumber = static_cast<std::uint16_t>(area);
        g.load_dat(static_cast<int>((area & 0xFFFF) + 3));
        while (g.load_done() == 0) g.sleep(1);
        Field_InputFlags = Area_Descriptors[Game_AreaNumber][0x32];
        g.clut_restore();
        Gfx_ClutStripDirty = 1;
    }
    g.party_set_up(x, z, flags);
    Cond_ByteFD = g.zone_id(Word(At(at::kKind2XHigh)), Word(At(at::kKind2ZHigh)));
    g.view_reset();
    const auto colour = static_cast<std::uint32_t>(Long(Area_Descriptors[Game_AreaNumber] + 0x38));
    if (colour != 0) {
        g.colour_matrix(Fn<const unsigned long*>(colour));
        const unsigned char* const own = Fn<const unsigned char*>(
            static_cast<std::uint32_t>(Long(Area_Descriptors[Game_AreaNumber] + 0x38)));
        SetLong(At(0x903598), Long(own + 0x20));   // Light_Angles
        SetLong(At(0x90359C), Long(own + 0x24));
    } else {
        g.colour_matrix(Fn<const unsigned long*>(at::kDefaultColour));
        SetLong(At(0x903598), Long(At(at::kDefaultLight)));
        SetLong(At(0x90359C), Long(At(at::kDefaultLight + 4)));
    }
    if (Field_StatusBits & 0x80) {
        auto chapter = static_cast<unsigned char>(Cond_ByteFA + 1);
        Cond_ByteFA = static_cast<signed char>(chapter);
        if (chapter == 0x10) {
            chapter = 0x11;
            Cond_ByteFA = 0x11;
        }
        g.scenario_start(chapter);
    }
    g.run_placement(Fn<const unsigned char*>(static_cast<std::uint32_t>(Long(Area_Descriptors[Game_AreaNumber]))));
    const auto hook = static_cast<std::uint32_t>(Long(Area_Descriptors[Game_AreaNumber] + 0x40));
    if (hook != 0) Fn<Handler>(hook)();
    g.flags_clear(At(at::kStoryFlags), 0x1C);
    At(0x8034E2)[0] = 1;   // Field_StatusBits + 1
    g.mode_dispatch();
    g.first_frame();

    const std::uint16_t bits = Field_ScriptFlags2;
    if (bits & 0x8000) {
        if (drop_in) {
            At(at::kWalk)[0] = 0;
            g.place_party(flags & ~0x80u);
            Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0xFFEF);
            g.members_frame();
        }
    } else if (!(Field_ScriptFlags & 0x800)) {
        bool walked = true;
        if (bits & 0x80) g.entry_walk(1, x, z);
        else if (bits & 0x100) g.entry_walk(2, x, z);
        else if (bits & 0x200) g.entry_walk(3, x, z);
        else if (Field_MemberCount > 1) {
            g.entry_walk(0, x, z);
            if (!(Field_ScriptFlags & 0x800) && !(Field_ScriptFlags2 & 0x8000))
                g.test_fb(static_cast<short>(Word(At(at::kKind2XHigh))), static_cast<short>(Word(At(at::kKind2ZHigh))));
        } else {
            walked = false;
        }
        if (walked) {
            const unsigned char members = Field_MemberCount;
            if (members > 1) {
                auto v = static_cast<std::uint16_t>(Field_ScriptFlags2 | 0x12);
                if (members == 3) v = static_cast<std::uint16_t>(v | 4);
                Field_ScriptFlags2 = static_cast<std::uint16_t>(v & 0xEE7F);
                return;
            }
        }
    }
    Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0xEE7F);
}

// original 0x595160 (PSX 0x801A0F90, gap pairing): the party walks in from
// the area's edge - the walk on, its direction, a zero, the point (x, z);
// with a direction bit 3 of Field_ScriptFlags2 set; each of Field_MemberCount
// members' two walk bytes: the direction's pose from 0x66ADBC, and bit 1.
//
// As the original has it: the direction is the argument's low byte; the pose
// table is indexed by it unchecked (callers pass 0..3); the member count is
// read once, as a byte.
extern "C" void __cdecl Area_EntryWalk(unsigned direction, int x, int z) {
    const auto dir = static_cast<unsigned char>(direction);
    if (dir != 0) Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 | 8);
    SetLong(At(at::kWalkX), x);
    const unsigned members = Field_MemberCount;
    At(at::kWalk)[0] = 1;
    At(at::kWalk)[1] = dir;
    At(at::kWalk)[2] = 0;
    SetLong(At(at::kWalkZ), z);
    if (members == 0) return;
    const unsigned char pose = At(at::kWalkPoses)[dir];
    for (unsigned i = 0; i < members; ++i) {
        unsigned char* const m = At(at::kMemberWalk + i * 0x14C);
        m[0] = pose;
        m[1] = static_cast<unsigned char>(m[1] | 2);
    }
}

// original 0x59E330 (PSX 0x8015990C): every window record's in-use, handler,
// kind, state and pass bytes (+0..+3, +0xF) zeroed, and the pass the walk
// starts from. No calls.
extern "C" void __cdecl Window_ResetAll(void) {
    for (unsigned i = 0; i < 22; ++i) {
        unsigned char* const r = At(at::kWindowRecords + i * 0x24);
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
        r[3] = 0;
        r[0xF] = 0;
    }
    At(at::kWindowPass)[0] = 0;
}

// ===========================================================================
// Boot (0x496B60, 0x496C90) and the small resets
// ===========================================================================

// original 0x496B60: task 0's first body, created by WinMain (PSX 0x8014E974 /
// 0x8014EBA8 by callers). The whole of VRAM's shadow cleared; DAT 0x225
// loaded and waited for; 0x10B0 bytes from 0x9039E0 zeroed (the game state -
// Cond_Flags, the clock, the party words); the button map from its
// defaults; the party combination 0xFF, both script flag words 0, the
// config bytes 1 then six zeros; the windows, the tint records and the slots
// reset; the draw pass flags, OT slot and two pool pointers set; the CLUT
// strip restored and counted dirty; then task 1 at 0x496C90 and this task
// ended.
//
// As the original has it: Port_DroppedCall(0) where the PSX called something
// (its argument left on the stack under LoadDatFile's); File_LoadDone asked
// before the first sleep; Gfx_ClutStripDirty counted up, not set.
//
// Task_Exit is a `call` in the original, not a tail jump, and this is a task's
// top frame: a tail jump would leave the task stack's 0 as Task_Exit's return
// address where the original leaves one inside this function. Nothing reads
// it in game, but the frame hash does - batch ab24's one differing frame,
// frame 1 - so the compiler's tail call is turned off here.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Boot_Task(void) {
    g.clear_rect(0, 0, 0x400, 0x200);
    g.dropped(0);
    g.load_dat(0x225);
    while (g.load_done() == 0) g.sleep(1);
    std::memset(At(at::kBootClear), 0, at::kBootClearBytes);
    std::memcpy(At(at::kButtons), At(at::kButtonDefaults), 0x12);
    At(at::kPartyCombination)[0] = 0xFF;
    Field_ScriptFlags = 0;
    Field_ScriptFlags2 = 0;
    unsigned char* const config = At(at::kConfigBytes);
    config[0] = 1;
    for (unsigned i = 1; i < 7; ++i) config[i] = 0;
    g.window_reset();
    g.tint_reset();
    g.slots_release();
    Draw_PassFlags = 0x1F;
    Draw_OtSlot = 6;
    SetLong(At(at::kPoolA), 0x8B3580);
    SetLong(At(at::kPoolB), 0x8C3584);
    g.clut_restore();
    Gfx_ClutStripDirty = static_cast<unsigned char>(Gfx_ClutStripDirty + 1);
    g.task_create(1, reinterpret_cast<void*>(static_cast<std::uintptr_t>(kTitleLoadEntry)));
    g.task_exit();
}

// original 0x496C90: task 1's first body - DAT 0x226 loaded and waited for,
// then a tail jump into Title_Task (which never returns).
extern "C" void __cdecl Title_LoadTask(void) {
    g.load_dat(0x226);
    while (g.load_done() == 0) g.sleep(1);
    g.title_task();
}

// original 0x454810: `mov eax, 1; ret`. Where the PSX polled its CD load
// (File_LoadDone 0x801636F0), the PC's loads are synchronous, so every wait
// loop's first question is answered "done".
extern "C" int __cdecl File_LoadDone(void) { return 1; }

// original 0x4549B0 (PSX 0x8014E178, the same loops): the last 0x800 bytes of
// Gfx_ClutStripSource - its rows 28..31 - zeroed, then all 32 rows of 0x200
// bytes copied to Gfx_ClutStrip. So the copy carries the zeros: the strip's
// rows 28..31 come out zero too. The PSX zeroes the same four rows first.
// Gfx_ClutStripDirty is not touched.
extern "C" void __cdecl Gfx_ClutStripRestore(void) {
    std::memset(At(at::kClutStripZero), 0, 0x800);
    std::memcpy(Gfx_ClutStrip, Gfx_ClutStripSource, 0x4000);
}

// original 0x454A20 (PSX 0x8014E2E8): byte +0 of the 32 MoveScript_TintRecords
// zeroed, then 16 dwords from 0x7E06A0 set to -1.
extern "C" void __cdecl MoveScript_TintReset(void) {
    for (unsigned i = 0; i < 32; ++i) MoveScript_TintRecords[i * 12] = 0;
    for (unsigned i = 0; i < 16; ++i) SetLong(At(at::kTintMarks + i * 4), -1);
}

// original 0x454A50 (PSX 0x8014E378): the Field_Slots record `slot` (its low
// byte) freed - byte +0 zeroed - if its bit 0 is set. As the original has it:
// the index is not checked (callers pass 0..7).
extern "C" void __cdecl Field_SlotRelease(unsigned slot) {
    unsigned char* const r = Field_Slots + (slot & 0xFF) * 16u;
    if (r[0] & 1) r[0] = 0;
}

// original 0x454AB0: Field_SlotRelease for each of the eight slots.
extern "C" void __cdecl Field_SlotsReleaseAll(void) {
    for (unsigned i = 0; i < 8; ++i) g.slot_release(i);
}

// original 0x461E10 (PSX 0x8014E458): a RECT {x, y, w, h} of 16-bit words on
// the stack, cleared through Gfx_ClearImage with colour 0.
//
// As the original has it: each argument's low 16 bits; a third zero is
// pushed that Gfx_ClearImage does not read (the PSX ClearImage's blue).
extern "C" void __cdecl Gfx_ClearRect(int x, int y, int w, int h) {
    short rect[4] = {static_cast<short>(x), static_cast<short>(y), static_cast<short>(w), static_cast<short>(h)};
    g.clear_image(rect, 0, 0);
}

// ===========================================================================

void ModeFlow_Inject() {
    if (bof3::WantsShadow("mode_flow")) mode_flow::SelfTest();
    BOF3_INJECT(Transition_Start);
    BOF3_INJECT(Transition_Task);
    BOF3_INJECT(Transition_Kind00);
    BOF3_INJECT(Transition_Kind01);
    BOF3_INJECT(Transition_Kind02);
    BOF3_INJECT(Transition_Kind03);
    BOF3_INJECT(Transition_Kind04);
    BOF3_INJECT(Transition_Kind05);
    BOF3_INJECT(Transition_Kind06);
    BOF3_INJECT(Transition_Kind07);
    BOF3_INJECT(Transition_Kind08);
    BOF3_INJECT(Transition_Kind09);
    BOF3_INJECT(Transition_Kind10);
    BOF3_INJECT(Transition_Kind11);
    BOF3_INJECT(Transition_Kind12);
    BOF3_INJECT(Transition_Kind13);
    BOF3_INJECT(Transition_Kind14);
    BOF3_INJECT(Transition_Kind15);
    BOF3_INJECT(Transition_Kind16);
    BOF3_INJECT(Transition_Kind17);
    BOF3_INJECT(Transition_Kind18);
    BOF3_INJECT(Transition_Kind19);
    BOF3_INJECT(Transition_Kind20);
    BOF3_INJECT(Transition_FadeSub);
    BOF3_INJECT(Transition_FadeAdd);
    BOF3_INJECT(Transition_DrawTile);
    BOF3_INJECT(Field_Task);
    BOF3_INJECT(GameMode_Start);
    BOF3_INJECT(GameMode_Enter);
    BOF3_INJECT(Field_WaitTransition);
    BOF3_INJECT(Game_ClockTick);
    BOF3_INJECT(Area_Enter);
    BOF3_INJECT(Area_EntryWalk);
    BOF3_INJECT(Window_ResetAll);
    BOF3_INJECT(Boot_Task);
    BOF3_INJECT(Title_LoadTask);
    BOF3_INJECT(File_LoadDone);
    BOF3_INJECT(Gfx_ClutStripRestore);
    BOF3_INJECT(MoveScript_TintReset);
    BOF3_INJECT(Field_SlotRelease);
    BOF3_INJECT(Field_SlotsReleaseAll);
    BOF3_INJECT(Gfx_ClearRect);
}
