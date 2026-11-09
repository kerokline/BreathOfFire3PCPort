// Moved here by tools/move_funcs.py from mode_flow.cpp, mode_rest.cpp, mode_states.cpp, mode_tasks.cpp, rest_3g.cpp (docs/source-layout-round.md).
#include "game/core/transition.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_data_reload.h"
#include "game/cheats.h"
#include "game/core/transition_callees.h"
#include "game/draw_pool.h"
#include "game/labels.h"
#include "game/map_rim.h"
#include "game/mode_flow.h"
#include "game/mode_flow_callees.h"
#include "game/mode_rest.h"
#include "game/mode_rest_callees.h"
#include "game/mode_states.h"
#include "game/mode_states_callees.h"
#include "game/mode_tasks.h"
#include "game/move_script_bytes.h"
#include "game/music_resume.h"
#include "game/psp_code.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3g.h"
#include "game/rest_3g_callees.h"
#include "game/scenario_harness.h"
#include "game/sound_rest.h"
#include "game/widescreen.h"
#include "game/window_task.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = transition::at;

}  // namespace

using namespace mode_flow;

using namespace mode_states;

namespace mode_flow {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

}  // namespace mode_flow

namespace {

// The fades' six bytes (mode_flow_callees.h): all to one value. The original
// stores them 0x90393F, 3E, 3D, 0x9038AF, AE, AD with nothing in between.
void SetFadeHold(unsigned char v) {
    for (unsigned i = 0; i < 3; ++i) {
        mode_flow::At(::bof3::Img(ModeFlow_FadeHoldA_addr))[i] = v;
        mode_flow::At(::bof3::Img(ModeFlow_FadeHoldB_addr))[i] = v;
    }
}

// The 16-bit words the camera transitions add to in memory: `add word`, so
// wrapping at 0x10000 and never carrying into the next word.
void AddWord(std::uint32_t address, int delta) { mode_flow::SetWord(mode_flow::At(address), static_cast<unsigned>(mode_flow::Word(mode_flow::At(address)) + delta)); }

constexpr std::uint32_t kCameraYaw = 0x929EC8;     // Camera_Angles[0]

constexpr std::uint32_t kCameraAngle = 0x929ECC;   // Cond_AngleFB's low word

// An argument the original reads as a byte from its stack slot. Capcom's
// callers push whole registers (Transition_Start has 170 call sites, some
// `push eax` / `push ecx` with a byte loaded into the low 8 bits), and clang
// is entitled to assume an `unsigned char` argument arrives extended - so the
// byte is read from the slot itself, as the original's `movzx` does.
unsigned char SlotByte(const unsigned char& argument) { return *reinterpret_cast<const volatile unsigned char*>(&argument); }

// Kinds 10 and 17: the camera swings away while the screen fades to black
// (subtractive, slot 0). Each frame the tile is not yet done, the yaw and the
// angle word turn by 11 and the distance shrinks by 50, the map view asked to
// redraw; the frame the fade finishes, nothing moves.
void SwingOut(int step) {
    std::int32_t level = 0;
    while (mode_flow::g.draw_tile(reinterpret_cast<short*>(&level), step, 1, 0, 2) == 0) {
        AddWord(::bof3::Img(kCameraYaw), 0xB);
        AddWord(::bof3::Img(kCameraAngle), 0xB);
        Camera_Distance = static_cast<short>(Camera_Distance - 0x32);
        MapView_Redraw = 2;
        mode_flow::g.sleep(1);
    }
    mode_flow::g.task_exit();
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
    Camera_Angles[0] = 0xFEAB;
    mode_flow::SetWord(mode_flow::At(::bof3::Img(Cond_AngleFB_addr)), 0x355);
    MapView_Redraw = 2;
    for (int i = 0; i < 5; ++i) {
        mode_flow::g.draw_tile(reinterpret_cast<short*>(&level), -0x400, 1, 0, 2);
        mode_flow::g.sleep(1);
    }
    unsigned char done = mode_flow::g.draw_tile(reinterpret_cast<short*>(&level), -0x400, 1, 0, 2);
    for (;;) {
        const auto distance = static_cast<std::uint16_t>(Camera_Distance + 0x32);
        AddWord(::bof3::Img(kCameraYaw), -0xB);
        AddWord(::bof3::Img(kCameraAngle), -0xB);
        Camera_Distance = static_cast<short>(distance);
        MapView_Redraw = 2;
        if (distance == 0x5DC) break;
        mode_flow::g.sleep(1);
        if (done == 0) done = mode_flow::g.draw_tile(reinterpret_cast<short*>(&level), -0x400, 1, 0, 2);
    }
    Camera_Angles[0] = 0xFD56;
    mode_flow::SetWord(mode_flow::At(::bof3::Img(Cond_AngleFB_addr)), 0x200);
    MapView_Redraw = 2;
    mode_flow::g.task_exit();
}

// A countdown: frames 29..0, then seconds and minutes 59..0, then hours.
//
// As the original has it: all four bytes zero means stopped; each byte is
// decremented and tested as s8 (so 0x80 and above count as below zero); when
// the minutes underflow at hour 0 the minutes and seconds are zeroed but the
// frames byte was already reloaded with 29 - so a countdown never reaches
// all-zero by itself, it runs 0:00:00 round every 30 frames (the PSX's
// 0x80199D9C is the same).
void CountDown(std::uint32_t address) {
    unsigned char* const t = mode_flow::At(address);
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

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using HandlerModeRest = scenario_harness::Handler;

U UL(U a) { return static_cast<U>(move_script::Long(move_script::At(a))); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

void StepUp() { Game_Step = static_cast<unsigned short>(Game_Step + 1); }

// xor eax, eax; mov ax, [Game_Step]; jmp [table + eax * 4]: the table's
// `entries` steps read in place (the fuzz swaps the cells for recorders); a
// Fatal past them, where the original jumps through the dword after - for mode
// 3 the facing jitter's four bytes, for mode 5 the approach table's.
void StepDispatch(const char* who, U table, unsigned entries) {
    const unsigned step = Game_Step;
    if (step >= entries)
        bof3::Fatal("%s: Game_Step is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/mode-rest.md section 6)",
                    who, step, entries, (unsigned)table);
    reinterpret_cast<HandlerModeRest>(static_cast<std::uintptr_t>(UL(table + 4 * step)))();
}

// call File_LoadDone; test eax, eax; jne done; loop: Task_Sleep(1);
// File_LoadDone; test; je loop.
void WaitLoad() {
    if (SH_CALL(File_LoadDone)() != 0) return;
    do SH_CALL(Task_Sleep)(1);
    while (SH_CALL(File_LoadDone)() == 0);
}

// cmp word [0x7E0678], 0x6E; jne; call 0x587C20; test eax, eax; jne: the word
// first, the whole of eax.
bool MusicWordAndSilent() { return ModeRest_MusicWord == 0x6E && SH_CALL(Sound_MusicPlaying)() == 0; }

// The battle steps' tail: the field objects' frame and a loading frame.
void BattleFrames() {
    SH_CALL(Field_ObjectsFrame)();
    SH_CALL(Field_LoadingFrame)();
}

}  // namespace

namespace mode_states {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

}  // namespace mode_states

namespace {

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
mode_states::Handler Entry(std::uint32_t table, int index) {
    return reinterpret_cast<mode_states::Handler>(static_cast<std::uintptr_t>(
        static_cast<std::uint32_t>(mode_states::Long(mode_states::At(table + 4u * static_cast<std::uint32_t>(index))))));
}

// The camera words: yaw Camera_Angles[0] and the pitch word Camera_Angles[2]
// (Cond_AngleFB's low half), and their copies Light_AnglesCopy[0] / [2].
constexpr std::uint32_t kYaw = 0x929EC8, kPitch = 0x929ECC, kYawCopy = 0x7E0680, kPitchCopy = 0x7E0684;

short Signed16(std::uint32_t address) { return static_cast<short>(mode_states::Word(mode_states::At(address))); }

void AddWordModeStates(std::uint32_t address, int delta) { mode_states::SetWord(mode_states::At(address), mode_states::Word(mode_states::At(address)) + static_cast<unsigned>(delta)); }

// The same with a loading frame before each sleep (the mode handlers').
void WaitLoadFramed() {
    while (mode_states::g.load_done() == 0) {
        mode_states::g.loading_frame();
        mode_states::g.sleep(1);
    }
}

// GameMode_Field's bytes, unnamed (docs/mode-tasks.md section 3 has the PSX
// twin of each).
// kBattleCount: now Game_BattleCount (symbols.toml). dword, +1 entering mode 5 (PSX 0x80145028)
constexpr std::uint32_t kBattleBytes = 0x904AA0;     // three bytes zeroed entering mode 5 (PSX 0x801462DC)

// kFieldFlags: now Field_ScriptFlags2 (symbols.toml). byte |= 0x40 on the area change (PSX 0x80146256)
// kMusicTrack: now Music_Track (symbols.toml). the track Music_Play last started, 0xFF none
// kAreaTrack: now AreaW1f_MusicCurrent (symbols.toml). the track the next area wants (PSX 0x80143F1F)
// kAreaTransition: now ModeFlow_AreaTransition (symbols.toml). Transition_Start's kind for the area change, 0xFF none
// kMenuBlock: now BattleE1_Mode (symbols.toml). the menu's state block (docs/menu-screens.md)
constexpr std::uint32_t kMode11Bytes = 0x905DA0;     // five bytes set entering mode 11 (PSX 0x80146268)

using HandlerRest3g = scenario_harness::Handler;

U ULRest3g(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U ULRest3g(U a) { return ULRest3g(move_script::At(a)); }

// xor eax, eax; mov ax, [Game_Step]; jmp [table + eax * 4]: the table's
// `entries` steps read in place (the fuzz swaps the cells for recorders); a
// Fatal past them, where the original jumps through the dword after - the next
// mode's table or data.
void StepDispatchRest3g(const char* who, U table, unsigned entries) {
    const unsigned step = Game_Step;
    if (step >= entries)
        bof3::Fatal("%s: Game_Step is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3g.md section 7)",
                    who, step, entries, (unsigned)table);
    reinterpret_cast<HandlerRest3g>(static_cast<std::uintptr_t>(ULRest3g(table + 4 * step)))();
}

// The two framed waits of the mode steps: while the wait word is not 0, a frame
// and a sleep; while File_LoadDone answers 0, `frame` (if any) and a sleep.
void WaitWord(void (*frame)()) {
    while (Transition_Busy != 0) {
        frame();
        SH_CALL(Task_Sleep)(1);
    }
}

void LoadingFrame() { SH_CALL(Field_LoadingFrame)(); }

void Mode8Frame() { SH_CALL(GameMode8_WaitFrame)(); }

// call File_LoadDone; test eax, eax; jne done; loop: [frame;] Task_Sleep(1);
// File_LoadDone; test; je loop.
void WaitLoadRest3g(void (*frame)()) {
    if (SH_CALL(File_LoadDone)() != 0) return;
    do {
        if (frame) frame();
        SH_CALL(Task_Sleep)(1);
    } while (SH_CALL(File_LoadDone)() == 0);
}

// Music_Track not 0xFF and not the byte 0x904CD0: the fade.
bool MusicChanges() {
    const unsigned char track = Music_Track;
    return track != 0xFF && AreaW1f_MusicCurrent != track;
}

// 0x90412C |= 0x80 and the sound bank of its low seven bits + 0x2C2 loaded.
void LoadBank() {
    const unsigned char b = static_cast<unsigned char>(AreaW0b_PartySet | 0x80);
    AreaW0b_PartySet = b;
    SH_CALL(Snd_LoadBankFile)((b & 0x7Fu) + 0x2C2u);
}

// Modes 9 and 10's step 0 (0x4965D0, 0x4966F0, byte for byte but the file):
// Transition_Start(2) and its framed wait (Field_LoadingFrame); two sleeps; the
// file loaded and waited for (a sleep a try); the CLUT strip's rows 1 and 2
// copied; Game_Step up; Gfx_ClutStripDirty 1.
void MenuModeEnter(int file) {
    SH_CALL(Transition_Start)(2);
    WaitWord(&LoadingFrame);
    SH_CALL(Task_Sleep)(1);
    SH_CALL(Task_Sleep)(1);
    SH_CALL(LoadDatFile)(file);
    WaitLoadRest3g(nullptr);
    SH_CALL(Gfx_ClutStripCopyRow)(1);
    SH_CALL(Gfx_ClutStripCopyRow)(2);
    Game_Step = static_cast<unsigned short>(Game_Step + 1);
    Gfx_ClutStripDirty = 1;
}

}  // namespace

// original 0x495040 (PSX 0x8014EB68): unless Transition_Busy is set (a
// transition already running), the kind to the word 0x66C828 and task 2
// started at 0x495070.
//
// As the original has it: the kind is the argument's low byte, zero-extended
// to the word; the wait word is tested as 16 bits; a running transition makes
// the call a no-op (the kind is not stored).
extern "C" void __cdecl Transition_Start(unsigned char kind) {
    if (Transition_Busy != 0) return;
    ModeFlow_TransitionKind = SlotByte(kind);
    mode_flow::g.task_create(2, reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(kTransitionEntry.v))));
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
    const unsigned kind = ModeFlow_TransitionKind;
    if (kind >= 21) bof3::Fatal("Transition_Task: kind %u, past the 21-entry table", kind);
    mode_flow::g.transitions[kind]();
}

// The 21 entries of Transition_Task's table (originals 0x495130..0x4955A0).
// Unless the line says otherwise, each is one call: FadeSub (subtractive, to
// or from black) or FadeAdd (additive, to or from white) with a step - 0x800
// is 16 frames, 0x400 32, 0x200 64, 0x100 128, 0x2000 4 - semi 1 and slot 0.
// None of them is reached by the attract sequence except kinds 0 and 1.
extern "C" void __cdecl Transition_Kind00(void) { mode_flow::g.fade_sub(0x800, 1, 0); }     // 0x495130: to black

extern "C" void __cdecl Transition_Kind01(void) { mode_flow::g.fade_sub(-0x800, 1, 0); }    // 0x495150: from black

extern "C" void __cdecl Transition_Kind02(void) { mode_flow::g.fade_sub(0x2000, 1, 0); }    // 0x495170

extern "C" void __cdecl Transition_Kind03(void) { mode_flow::g.fade_sub(-0x2000, 1, 0); }   // 0x495190

extern "C" void __cdecl Transition_Kind04(void) { mode_flow::g.fade_sub(0x400, 1, 0); }     // 0x4951B0

extern "C" void __cdecl Transition_Kind05(void) { mode_flow::g.fade_sub(-0x400, 1, 0); }    // 0x4951D0

extern "C" void __cdecl Transition_Kind06(void) { mode_flow::g.fade_sub(0x800, 1, 2); }     // 0x4951F0: into OT slot 2

extern "C" void __cdecl Transition_Kind07(void) { mode_flow::g.fade_sub(-0x800, 1, 2); }    // 0x495210

extern "C" void __cdecl Transition_Kind08(void) { mode_flow::g.fade_add(0x800, 1, 0); }     // 0x495230: to white

extern "C" void __cdecl Transition_Kind09(void) { mode_flow::g.fade_add(-0x800, 1, 0); }    // 0x495250: from white

extern "C" void __cdecl Transition_Kind10(void) { SwingOut(0x400); }             // 0x495270

extern "C" void __cdecl Transition_Kind11(void) { SwingIn(); }                   // 0x4952D0

// 0x4953A0: the camera distance put at 0x5DC and the view asked to redraw
// (before the call), then a 16-frame fade in from black.
extern "C" void __cdecl Transition_Kind12(void) {
    Camera_Distance = 0x5DC;
    MapView_Redraw = 2;
    mode_flow::g.fade_sub(-0x800, 1, 0);
}

extern "C" void __cdecl Transition_Kind13(void) { mode_flow::g.fade_sub(0x200, 1, 0); }     // 0x4953D0

extern "C" void __cdecl Transition_Kind14(void) { mode_flow::g.fade_sub(-0x200, 1, 0); }    // 0x4953F0

extern "C" void __cdecl Transition_Kind15(void) { mode_flow::g.fade_add(0x200, 1, 0); }     // 0x495410

extern "C" void __cdecl Transition_Kind16(void) { mode_flow::g.fade_add(-0x200, 1, 0); }    // 0x495430

extern "C" void __cdecl Transition_Kind17(void) { SwingOut(0x200); }             // 0x495450

extern "C" void __cdecl Transition_Kind18(void) { SwingIn(); }                   // 0x4954B0, byte for byte kind 11's

extern "C" void __cdecl Transition_Kind19(void) { mode_flow::g.fade_sub(0x100, 1, 0); }     // 0x495580

// 0x4955A0: a 16-frame fade to black (subtractive) while the angle word spins
// by 0x20 a frame, kept inside 0..0xFFF; then the hold bytes zeroed.
//
// As the original has it: the angle is read as a dword, masked, and stored as
// a word - its upper word is left alone.
extern "C" void __cdecl Transition_Kind20(void) {
    std::int32_t level = 0;
    while (mode_flow::g.draw_tile(reinterpret_cast<short*>(&level), 0x800, 1, 0, 2) == 0) {
        mode_flow::SetWord(mode_flow::At(::bof3::Img(Cond_AngleFB_addr)), (Cond_AngleFB + 0x20u) & 0xFFFu);
        mode_flow::g.sleep(1);
    }
    SetFadeHold(0);
    mode_flow::g.task_exit();
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
    while (mode_flow::g.draw_tile(reinterpret_cast<short*>(&level), step, semi, slot, 2) == 0) mode_flow::g.sleep(1);
    if (out) SetFadeHold(0);
    mode_flow::g.task_exit();
}

// original 0x4956A0: the same through an additive tile (blend mode 1), so
// through white. A fade in (step <= 0) zeroes the hold bytes before the
// first frame; a fade out sets them to 0xFF after the last. A tail jump to
// Task_Exit.
extern "C" void __cdecl Transition_FadeAdd(int step, unsigned semi, unsigned slot) {
    const bool out = static_cast<short>(step) > 0;
    std::int32_t level = out ? 0 : 0x7FFF;
    if (!out) SetFadeHold(0);
    while (mode_flow::g.draw_tile(reinterpret_cast<short*>(&level), step, semi, slot, 1) == 0) mode_flow::g.sleep(1);
    if (out) SetFadeHold(0xFF);
    mode_flow::g.task_exit();
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
    const unsigned tpage = mode_flow::g.get_tpage(1, abr & 0xFF, 0x140, 0);
    mode_flow::g.draw_mode(Gfx_PacketNext, 0, 0, tpage & 0xFFFF, 0);
    mode_flow::g.commit(slot, 0xC);
    unsigned char* const tile = Gfx_PacketNext;
    mode_flow::g.set_tile(tile);
    const auto grey = static_cast<unsigned char>(static_cast<short>(*level) >> 7);
    tile[6] = grey;
    tile[5] = grey;
    tile[4] = grey;
    // DIV-0041: under a wide picture the fade covers it whole, (-53, 0) 426 x 240.
    const float wide = static_cast<float>(Widescreen_Live());
    mode_flow::SetLong(tile + 8, std::bit_cast<std::uint32_t>(0.0f - wide));   // x; 0.0f - wide, not -wide, which is -0.0f when wide is 0
    mode_flow::SetLong(tile + 0xC, 0);
    mode_flow::SetLong(tile + 0x14, std::bit_cast<std::uint32_t>(320.0f + 2 * wide));   // 0x43A00000, 320.0f
    mode_flow::SetLong(tile + 0x18, 0x43700000);   // 240.0f
    mode_flow::g.set_semi(tile, semi & 0xFF);
    mode_flow::g.commit(slot, 0x1C);
    *level = static_cast<short>(static_cast<std::uint16_t>(*level) + static_cast<std::uint16_t>(step));
    return static_cast<short>(*level) < 0 ? 1 : 0;
}

// original 0x495800 (PSX 0x80198068): task 0's body for a game - the demo (Title_StateStartDemo)
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
    mode_flow::g.clear_private();
    mode_flow::g.window_reset();
    for (;;) {
        reinterpret_cast<mode_flow::Handler>(GameMode_Handlers[Game_Mode].Native())();
        mode_flow::g.clock_tick();
        mode_flow::g.sleep(1);
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
    mode_flow::g.party_load(0);
    const unsigned char flags = mode_flow::At(::bof3::Img(FieldModes_Pending_addr))[3];
    const std::int32_t z = mode_flow::Long(mode_flow::At(::bof3::Img(FieldModes_Pending_addr) + 8));
    const std::int32_t x = mode_flow::Long(mode_flow::At(::bof3::Img(FieldModes_Pending_addr) + 4));
    Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 | 0x4040);
    const std::uint16_t area = FieldModes_Pending;
    mode_flow::At(::bof3::Img(0x905B82))[0] = 0;   // Field_StepCount + 2
    Sprite_Kind2.state = 0;                                 // Sprite_Kind2 + 1..3
    Sprite_Kind2.step = 0;
    Sprite_Kind2.state3 = 0;
    EventBattle_Flags = 0;
    EventBattle_Id = 0;
    Area189_StepsSinceEvent = 0;   // the byte after Field_MemberCount
    Area189_EventsInRow = 0;
    Game_AreaNumber = 0xFFFF;
    mode_flow::g.change_area(area, x, z, flags);
    const unsigned char* const clock = mode_flow::At(::bof3::Img(mode_flow::at::kClock));
    if (clock[3] != 0 || clock[2] != 0 || clock[1] != 0 || clock[0] != 0) {
        // DIV-0092: a loaded game plays the track the save holds; a save with no
        // track (0xFF - the port's F12 save, written while the music was off)
        // keeps the one Field_ChangeArea just chose from the area's zone instead
        // of silence. Capcom's rule copies the 0xFF.
        const unsigned char saved = Music_Track;
        if (!(mode_flow::g_area_track_fallback && saved == 0xFF)) AreaW1f_MusicCurrent = saved;
    }
    Music_Track = 0xFF;
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
    const unsigned char flags = AreaEntry_PendingFlags;
    const std::int32_t z = static_cast<std::int32_t>(AreaEntry_PendingZ);
    const std::int32_t x = mode_flow::at::kPendingX;
    Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0xFFF7);
    mode_flow::g.area_enter(AreaW0b_Place, x, z, flags);
    if (Field_InputFlags & 8) {
        Game_Mode = 8;
        return;
    }
    const unsigned char kind = Area_EnterTransition;
    if (kind == 0xFE) {
        Draw_PassFlags = 0;
    } else if (kind != 0xFF) {
        mode_flow::g.transition(kind);
        mode_flow::g.wait_transition(1);
        if (Field_InputFlags & 1) mode_flow::g.entry_point(mode_flow::Word(mode_flow::At(::bof3::Img(mode_flow::at::kKind2XHigh))), mode_flow::Word(mode_flow::At(::bof3::Img(mode_flow::at::kKind2ZHigh))));
    }
    const bool bit0 = (Field_InputFlags & 1) != 0;
    const std::uint16_t area = Game_AreaNumber;
    if (bit0) ModeFlow_AreaTransition = 0xA;
    else if (area == 0xBD) ModeFlow_AreaTransition = 0x14;
    else ModeFlow_AreaTransition = 0;
    if (area != 0xBD) mode_flow::g.zone_roll(1);
    Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0xFFBF);
    Field_Request = 0;
    Game_Mode = 2;
}

// original 0x4959F0 (PSX 0x80198378): Game_Mode 2, the field (PSX GAME.EMI section 0,
// FUN_80198378): the event script and the field's frame, then whatever
// Field_Request asks for becomes the next mode.
//
// As the original has it: Field_Request is read (as a byte) only after the
// two calls, which may set it; 0 and anything above 9 change nothing. On the
// area change (5) the music track byte is read again after each call that
// may have changed it, and Transition_Start gets the area's kind as a byte
// (the original passes ecx with whatever its upper bytes held; the callee
// reads the byte). The PSX's case 5 also started the next area's music; the
// port fades instead (docs/mode-tasks.md section 3).
extern "C" void __cdecl GameMode_Field(void) {
    mode_tasks::g.event_dispatch();
    mode_tasks::g.field_frame();
    switch (Field_Request) {
    case 1:   // the menu
        mode_tasks::g.transition(2);
        mode_tasks::g.wait_transition(0);
        Game_Step = 0;
        Game_Mode = 3;
        return;
    case 2:
        Game_Mode = 4;
        return;
    case 3: {   // battle
        const std::int32_t count = static_cast<std::int32_t>(Game_BattleCount);
        Game_Mode = 5;
        move_script::At(::bof3::Img(kBattleBytes))[0] = 0;
        move_script::At(::bof3::Img(kBattleBytes))[1] = 0;
        move_script::At(::bof3::Img(kBattleBytes))[2] = 0;
        rv::Bytes(ObjTrio)[0xb] = 0;
        rv::Bytes(&ObjTrio[1])[0xb] = 0;
        rv::Bytes(&ObjTrio[2])[0xb] = 0;
        Game_Step = 0;
        Game_BattleCount = static_cast<std::int32_t>(static_cast<std::uint32_t>(count) + 1u);
        return;
    }
    case 4:
        Game_Mode = 6;
        return;
    case 5: {   // the area change
        unsigned char track = Music_Track;
        move_script::At(::bof3::Img(Field_ScriptFlags2_addr))[0] |= 0x40;
        if (track != 0xFF && AreaW1f_MusicCurrent != track) {
            mode_tasks::g.music_fade(0x10);
            track = Music_Track;
        }
        const unsigned char kind = ModeFlow_AreaTransition;
        if (kind != 0xFF) {
            mode_tasks::g.transition(kind);
            mode_tasks::g.wait_transition(0);
            track = Music_Track;
        }
        if (track != 0xFF && AreaW1f_MusicCurrent != track) mode_tasks::g.music_stop(0xA);
        Game_Mode = 1;
        return;
    }
    case 6:
        BattleE1_Mode = 6;
        move_script::At(::bof3::Img(BattleE1_Mode_addr))[1] = 0;
        move_script::At(::bof3::Img(BattleE1_Mode_addr))[2] = 0;
        Game_Step = 0;
        move_script::At(::bof3::Img(BattleE1_Mode_addr))[0xC] = 0xFF;
        Game_Mode = 7;
        return;
    case 7:
    case 8:
        Game_Mode = Field_Request == 7 ? 9 : 0xA;
        Game_Step = 0;
        BattleE1_Mode = 0;
        move_script::At(::bof3::Img(BattleE1_Mode_addr))[1] = 0;
        move_script::At(::bof3::Img(BattleE1_Mode_addr))[2] = 0;
        return;
    case 9:
        FieldEvent_Object905DA0 = 1;
        move_script::At(::bof3::Img(kMode11Bytes))[1] = 0;
        move_script::At(::bof3::Img(kMode11Bytes))[2] = 0;
        move_script::At(::bof3::Img(kMode11Bytes))[3] = 0;
        move_script::At(::bof3::Img(kMode11Bytes))[4] = 0;
        Game_Mode = 0xB;
        return;
    default:
        return;
    }
}

// original 0x495BB0 (GameMode_Handlers[3]; PSX 0x801985E4): jmp
// [GameMode3_Steps + Game_Step * 4], unchecked.
extern "C" void __cdecl GameMode3_Run(void) {
    StepDispatch("GameMode3_Run", AddressOf(GameMode3_Steps), GameMode3_Steps_count);
}

// original 0x495BC0 (GameMode3_Steps[0]; PSX 0x80198620, which loads 0x269):
// the menu's DAT 0x31E and its wait (a sleep a try); CLUT strip rows 1 and 2;
// Gfx_ClutStripDirty 1, the menu block's mode byte 0 and byte +4 3 - stored
// before Transition_Start(3) (pushed first); Menu_WaitTransition(1); with the
// word 0x7E0678 0x6E and the music not playing, Music_FadeOut(0x10); Game_Step
// up.
extern "C" void __cdecl GameMode3_Enter(void) {
    SH_CALL(LoadDatFile)(static_cast<int>(mode_rest::at::kMenuFile));
    WaitLoad();
    SH_CALL(Gfx_ClutStripCopyRow)(1);
    SH_CALL(Gfx_ClutStripCopyRow)(2);
    Gfx_ClutStripDirty = 1;
    BattleE1_Mode = 0;
    move_script::At(::bof3::Img(mode_rest::at::kMenuTimer))[0] = 3;
    SH_CALL(Transition_Start)(3);
    SH_CALL(Menu_WaitTransition)(1);
    if (MusicWordAndSilent()) SH_CALL(Music_FadeOut)(0x10);
    StepUp();
}

// original 0x495C50 (GameMode3_Steps[2]; PSX 0x801986EC): the menu's last
// frame and its fade out, then the field back:
//   - Menu_Frame; Transition_Start(2); Menu_WaitTransition(0); a sleep;
//   - 0x929F11 set: PartySet_Load(the list 0x904062..64, 0) and its wait,
//     0x929F11 0;
//   - the party set 0x90412C |= 0x80, Snd_LoadBankFile(its low seven bits +
//     0x2C2), not waited for;
//   - Field_InputFlags bit 4: DAT 0x12A and its wait, CommuSim_RollOffers;
//   - Transition_Start(3); Field_WaitTransition(1); the word 0x7E0678 0x6E and
//     the music not playing: Music_FadeIn(0x10);
//   - 0x905B60 set (cleared): with 0x904152 0 and (Field_InputFlags bit 0 or
//     area 0xBD), out - the leader's x, z and the area saved to 0x904148 /
//     0x90414C / 0x904150, 0x904152 1, the facing (0x656A80[Rand & 3] + the
//     leader's) & 0xF to 0x904153, Field_ChangeArea(0x937F82, 0x903860,
//     0x90384C, 0x905B88); otherwise home - 0x904152 0, story flag 0x77
//     cleared, Field_ChangeArea(the saved area, x, z, 4);
//   - else 0x905B61 set: Field_ChangeArea(it, 0x430000, 0x160000, 4), cleared;
//   - else Field_Request 0;
//   - Window_ResetAll; Game_Mode 2, Game_Step 0.
// As the original has it: the arguments with stale upper bytes (the list, the
// areas, the flags) are read by their callees as bytes and words (masked in
// the fuzz, scenario_harness.cpp's rows); the leader's facing, the pending
// area's cells and the jitter are read after Rand, the saved trip after
// Flags_Clear.
extern "C" void __cdecl GameMode3_Leave(void) {
    SH_CALL(Menu_Frame)();
    SH_CALL(Transition_Start)(2);
    SH_CALL(Menu_WaitTransition)(0);
    SH_CALL(Task_Sleep)(1);
    if (MenuLists_PartyChanged != 0) {
        const unsigned c = Cond_Flags[0xd4], b = Cond_Flags[0xd3], a = Cond_Flags[0xd2];
        SH_CALL(PartySet_Load)(a, b, c, 0);
        WaitLoad();
        MenuLists_PartyChanged = 0;
    }
    {
        const unsigned char set = static_cast<unsigned char>(AreaW0b_PartySet | 0x80);
        AreaW0b_PartySet = set;
        SH_CALL(Snd_LoadBankFile)((set & 0x7Fu) + mode_rest::at::kPartySetFiles);
    }
    if (Field_InputFlags & 0x10) {
        SH_CALL(LoadDatFile)(static_cast<int>(mode_rest::at::kCommuFile));
        WaitLoad();
        SH_CALL(CommuSim_RollOffers)();
    }
    SH_CALL(Transition_Start)(3);
    SH_CALL(Field_WaitTransition)(1);
    if (MusicWordAndSilent()) SH_CALL(Music_FadeIn)(0x10);
    if (MenuLists_CampChosen != 0) {
        const unsigned char out = AreaW3c_CampFlag;
        MenuLists_CampChosen = 0;
        if (out == 0 && ((Field_InputFlags & 1) != 0 || Game_AreaNumber == mode_rest::at::kTripAnyArea)) {
            const U x = static_cast<U>(ObjTrio[0].x), z = static_cast<U>(ObjTrio[0].z);
            const unsigned area = Game_AreaNumber;
            AreaW3c_CampFlag = 1;
            AreaW2d_ReturnPoint = x;
            AreaW4d_ReturnZ = z;
            AreaW4d_ReturnArea = area;
            const U r = static_cast<U>(SH_CALL(Rand)());
            AreaW3g_HealByte =
                static_cast<unsigned char>((move_script::At(::bof3::Img(mode_rest::at::kFacingJitter))[r & 3] + ObjTrio[0].direction) & 0xF);
            const unsigned place = AreaW0b_Place;
            const U px = static_cast<std::uint32_t>(mode_rest::at::kPendingX), pz = AreaEntry_PendingZ;
            SH_CALL(Field_ChangeArea)(place, I(px), I(pz), AreaEntry_PendingFlags);
        } else {
            AreaW3c_CampFlag = 0;
            SH_CALL(Flags_Clear)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), mode_rest::at::kTripFlag);
            const unsigned area = AreaW4d_ReturnArea;
            const U x = AreaW2d_ReturnPoint, z = AreaW4d_ReturnZ;
            SH_CALL(Field_ChangeArea)(area, I(x), I(z), 4);
        }
    } else if (ModeRest_AreaAsked != 0) {
        SH_CALL(Field_ChangeArea)(ModeRest_AreaAsked, I(mode_rest::at::kAreaAskedX), I(mode_rest::at::kAreaAskedZ), 4);
        ModeRest_AreaAsked = 0;
    } else {
        Field_Request = 0;
    }
    SH_CALL(Window_ResetAll)();
    Game_Mode = 2;
    Game_Step = 0;
}

// original 0x495E60 (GameMode_Handlers[4]; PSX 0x801989D4): with the message
// cells' 0x7DEE44 bit 1, 0x905B82 = 0x10, Field_Request 0, Game_Mode 2; a tail
// jump to Field_FrameScripted either way.
extern "C" void __cdecl GameMode4_Run(void) {
    if (move_script::At(::bof3::Img(mode_rest::at::kMessageBits))[0] & 2) {
        EventOps_BlockByte = 0x10;
        Field_Request = 0;
        Game_Mode = 2;
    }
    SH_CALL(Field_FrameScripted)();
}

// original 0x495E90 (GameMode_Handlers[5]; PSX 0x80198A24): jmp
// [GameMode5_Steps + Game_Step * 4], unchecked.
extern "C" void __cdecl GameMode5_Run(void) {
    StepDispatch("GameMode5_Run", AddressOf(GameMode5_Steps), GameMode5_Steps_count);
}

// original 0x495EA0 (GameMode5_Steps[0]; PSX 0x80198A60):
// Encounter_PartyTurnSense, the two frames, Game_Step up.
extern "C" void __cdecl GameMode5_TurnSense(void) {
    SH_CALL(Encounter_PartyTurnSense)();
    BattleFrames();
    StepUp();
}

// original 0x495EC0 (GameMode5_Steps[1]; PSX 0x80198AA4): Encounter_PartyTurn
// (al), the two frames; once it answers non-zero: Port_DroppedCall(1); then by
// the flags 0x904AE5 and the dword 0x904AAC (both read once, after it):
//   - bit 0 clear: MoveScript_F3Divisor 0x40; the upper words of Field_Kind2Z
//     and _Kind2X stepped by the s8 pair 0x656AA4[formation byte * 2] (+1 to
//     z, +0 to x); MoveScript_FAWord = (the dword 0x939860 - MapView_Elevation)
//     sar 4;
//   - bit 4 clear: PartySet_Select(the party set's low seven bits, formation
//     bit 1 ? 2 : 1) and the formation read again;
// Game_Step up, Draw_OtSlot 4, Draw_SortOnX = formation bit 0, MapView_Redraw 2.
// As the original has it: the formation byte indexes the pairs unbounded
// (docs/mode-rest.md section 6); the elevation is read before the stores.
extern "C" void __cdecl GameMode5_Turn(void) {
    const unsigned char turned = SH_CALL(Encounter_PartyTurn)();
    BattleFrames();
    if (turned == 0) return;
    SH_CALL(Port_DroppedCall)(1);
    const unsigned char flags = EventBattle_Flags;
    U formation = UL(::bof3::Img(mode_rest::at::kFormation));
    if (!(flags & 1)) {
        const U i = (formation & 0xFF) * 2;
        const U elevation = static_cast<U>(MapView_Elevation);
        MoveScript_F3Divisor = 0x40;
        const unsigned dx = static_cast<std::uint16_t>(static_cast<signed char>(move_script::At(::bof3::Img(mode_rest::at::kApproach))[i]));
        const unsigned dz = static_cast<std::uint16_t>(static_cast<signed char>(move_script::At(::bof3::Img(mode_rest::at::kApproach))[i + 1]));
        move_script::SetWord(move_script::At(::bof3::Img(mode_rest::at::kKind2ZHigh)), move_script::Word(move_script::At(::bof3::Img(mode_rest::at::kKind2ZHigh))) + dz);
        const U base = ModeRest_ElevationBase;
        move_script::SetWord(move_script::At(::bof3::Img(mode_rest::at::kKind2XHigh)), move_script::Word(move_script::At(::bof3::Img(mode_rest::at::kKind2XHigh))) + dx);
        MoveScript_FAWord = static_cast<unsigned short>(static_cast<U>(I(base - elevation) >> 4));
    }
    if (!(flags & 0x10)) {
        const unsigned set = AreaW0b_PartySet & 0x7Fu;
        SH_CALL(PartySet_Select)(set, (formation & 2) ? 2u : 1u);
        formation = UL(::bof3::Img(mode_rest::at::kFormation));
    }
    StepUp();
    Draw_OtSlot = 4;
    Draw_SortOnX = static_cast<unsigned char>(formation & 1);
    MapView_Redraw = 2;
}

// original 0x495F90 (GameMode5_Steps[2]; PSX 0x80198C1C):
// Encounter_PartyToPlaces (al), the two frames; Game_Step up when File_LoadDone
// (asked first), that answer and Field_Kind2Hold 0.
extern "C" void __cdecl GameMode5_ToPlaces(void) {
    const unsigned char placed = SH_CALL(Encounter_PartyToPlaces)();
    BattleFrames();
    if (SH_CALL(File_LoadDone)() != 0 && placed != 0 && Field_Kind2Hold == 0) StepUp();
}

// original 0x495FD0 (GameMode5_Steps[3]; PSX 0x80198C94): Field_PartyLoad(1),
// Encounter_PartyAtPlaces, the two frames; Music_FadeOutStop(0xA) unless
// 0x904AE5 bit 6 or Music_Track 0xFF; the battle's file and track - an event
// battle's from its record (EventBattle_Records +3 indexing the word pairs
// 0x64DECC, the record read again after the load), else DAT 0xD2 and track
// 0x97 below chapter 8 (Cond_ByteFA, s8), DAT 0xD3 and 0x99 from it;
// Music_Play(track, 0xA); Window_ResetAll; Game_Step up; 0x904AE9 0, the word
// 0x904AA8 0, the bytes 0x904AA0..A4 0.
extern "C" void __cdecl GameMode5_Load(void) {
    SH_CALL(Field_PartyLoad)(1);
    SH_CALL(Encounter_PartyAtPlaces)();
    BattleFrames();
    // DIV-0092: the field's song and where it is heard, before the battle's
    // music takes over - our DLL's state, nothing written to the game's;
    // nothing remembered with the switch off (Capcom's rule, and the fuzz).
    music_resume::Remember();
    // DIVERGENCE DIV-0149: an event battle whose file has no music of its own
    // (its track word 0xFF) leaves the scene's song playing, as the
    // PlayStation does. Capcom's PC faded it out here and then asked for song
    // 0xFF, which has no file, so D24's path started the old song again from
    // its top.
    bool keep = false;
    if (mode_rest::g_event_keeps_music && EventBattle_Id != 0) {
        const unsigned asset = move_script::At(::bof3::Img(mode_rest::at::kEventAsset))[EventBattle_Id * 4u];
        keep = move_script::Word(move_script::At(::bof3::Img(mode_rest::at::kEventTrack) + asset * 4u)) == 0xFF;
    }
    if (!keep && !(EventBattle_Flags & 0x40) && Music_Track != 0xFF) SH_CALL(Music_FadeOutStop)(0xA);
    unsigned track;
    const unsigned char event = EventBattle_Id;
    if (event != 0) {
        const unsigned asset = move_script::At(::bof3::Img(mode_rest::at::kEventAsset))[event * 4u];
        SH_CALL(LoadDatFile)(static_cast<int>(move_script::Word(move_script::At(::bof3::Img(mode_rest::at::kEventFile) + asset * 4u))));
        const unsigned again = move_script::At(::bof3::Img(mode_rest::at::kEventAsset))[EventBattle_Id * 4u];
        track = move_script::Word(move_script::At(::bof3::Img(mode_rest::at::kEventTrack) + again * 4u));
    } else if (Cond_ByteFA < static_cast<int>(mode_rest::at::kChapterSplit)) {
        SH_CALL(LoadDatFile)(static_cast<int>(mode_rest::at::kBattleFileA));
        track = mode_rest::at::kBattleTrackA;
    } else {
        SH_CALL(LoadDatFile)(static_cast<int>(mode_rest::at::kBattleFileB));
        track = mode_rest::at::kBattleTrackB;
    }
    if (keep)
        bof3::Log("DIV-0149    event battle %u has no music of its own: track %u plays on", event,
                  static_cast<unsigned>(Music_Track));
    else
        SH_CALL(Music_Play)(track, 0xA);
    SH_CALL(Window_ResetAll)();
    StepUp();
    BattleMisc_BannerKinds = 0;
    move_script::SetWord(move_script::At(::bof3::Img(mode_rest::at::kBattleWord)), 0);
    for (unsigned k = 0; k < 5; ++k) move_script::At(::bof3::Img(mode_rest::at::kBattleBytes))[k] = 0;
}

// original 0x4960D0 (GameMode5_Steps[4]; PSX 0x80198E54):
// Encounter_PartyScriptOnce (al), the two frames; when File_LoadDone (asked
// first) and that answer: MoveScript_F3Divisor 0, 0x904AE9 0, 0x904AA0..A4 0,
// Party_Count(1), Game_Step up, its al to 0x904AB0.
extern "C" void __cdecl GameMode5_Script(void) {
    const unsigned char done = SH_CALL(Encounter_PartyScriptOnce)();
    BattleFrames();
    if (SH_CALL(File_LoadDone)() == 0 || done == 0) return;
    MoveScript_F3Divisor = 0;
    BattleMisc_BannerKinds = 0;
    for (unsigned k = 0; k < 5; ++k) move_script::At(::bof3::Img(mode_rest::at::kBattleBytes))[k] = 0;
    const int count = SH_CALL(Party_Count)(1);
    StepUp();
    move_script::At(::bof3::Img(mode_rest::at::kMemberCount))[0] = static_cast<unsigned char>(count);
}

// original 0x496130 (GameMode5_Steps[6]; PSX 0x80198EFC): Party_PlaceAtSlots,
// Party_ScriptTicks, the two frames, Game_Step up.
extern "C" void __cdecl GameMode5_Place(void) {
    SH_CALL(Party_PlaceAtSlots)();
    SH_CALL(Party_ScriptTicks)();
    BattleFrames();
    StepUp();
}

// original 0x496150 (GameMode5_Steps[7]; PSX 0x80198F48): with Field_Kind2Hold
// 0 and File_LoadDone (in that order): Draw_OtSlot 6; Field_PartyLoad(0);
// Party_PlacesByList; Field_StepCount 0; unless 0x904AE5 bit 1,
// MapView_SetElevation(AreaMap_Elevation(Field_Kind2X, Field_Kind2Z)); the
// field's track again (Music_Play(Music_Track, 0xA)) unless 0x904AE5 bit 6,
// 0x904AE8 bit 3 or the track 0xFF; 0x904AE5 and the event battle 0;
// Field_EncounterThresholdRoll(0); Field_AfterBattleTally. Then, either way,
// Party_ScriptTicks and the two frames; and after the first branch Game_Mode 2,
// Game_Step 0, Field_Request 0.
// As the original has it: 0x904AE5 is read twice, before and after the
// elevation's calls.
extern "C" void __cdecl GameMode5_Leave(void) {
    bool back = false;
    if (Field_Kind2Hold == 0 && SH_CALL(File_LoadDone)() != 0) {
        Draw_OtSlot = 6;
        SH_CALL(Field_PartyLoad)(0);
        SH_CALL(Party_PlacesByList)();
        const unsigned char flags = EventBattle_Flags;
        Field_StepCount = 0;
        if (!(flags & 2)) {
            const long z = Field_Kind2Z, x = Field_Kind2X;
            const long height = SH_CALL(AreaMap_Elevation)(x, z);
            SH_CALL(MapView_SetElevation)(static_cast<int>(height));
        }
        if (!(EventBattle_Flags & 0x40) && !(Battle_EndFlags & 8)) {
            unsigned char track = Music_Track;
            // DIV-0092: by now the fanfare's stopping fade (BattleEnd_ExitHook)
            // has left the track 0xFF, so Capcom's Music_Play(Music_Track) never
            // ran and the field came back silent; the PSX restarts the area's
            // song from its table. Ours plays the area's own track.
            // When that is the song the battle took over, it resumes where it
            // was heard (GameMode5_Load's Remember), as the PlayStation's does,
            // over music_resume's own fade.
            bool resume = false;
            if (track == 0xFF && mode_flow::g_area_track_fallback) {
                track = AreaW1f_MusicCurrent;
                // DIV-0092, second amendment (2026-10-11): after an event battle
                // (Field_StartEventBattle's byte, cleared below) the PlayStation
                // restarts the area's own song from its table - the owner heard
                // track 27 after the Nue fight on mednafen, where the current-track
                // byte held the script's 29. So: the zone's table track for the
                // leader's cell (what Area_PickMusic derives on an area change),
                // from its top, with Capcom's fade. Only a random encounter resumes.
                const unsigned char event = EventBattle_Id;
                if (event != 0) {
                    const unsigned char current = track;
                    track = Area_ZoneTrack(Area_ZoneAt(rv::Bytes(ObjTrio)[0x36], rv::Bytes(ObjTrio)[0x3a]));
                    AreaW1f_MusicCurrent = track;
                    bof3::Log("DIV-0092    event battle %u over: the zone's track %u from its top (the current byte held %u), no resume",
                              event, track, current);
                } else {
                    resume = track != 0xFF && music_resume::Arm(track);
                }
            }
            if (track != 0xFF) SH_CALL(Music_Play)(track, resume ? music_resume::kResumeFadeFrames : 0xA);
        }
        music_resume::Forget();  // a remembered position is used here or not at all
        EventBattle_Flags = 0;
        EventBattle_Id = 0;
        SH_CALL(Field_EncounterThresholdRoll)(0);
        SH_CALL(Field_AfterBattleTally)();
        back = true;
    }
    SH_CALL(Party_ScriptTicks)();
    BattleFrames();
    if (back) {
        Game_Mode = 2;
        Game_Step = 0;
        Field_Request = 0;
    }
}

// original 0x496230 (GameMode_Handlers[6]; PSX 0x80199170): Look_PadControl at
// Game_Step 0, GameMode_LookEnd at any other; a tail jump to
// Field_LoadingFrame.
extern "C" void __cdecl GameMode6_Run(void) {
    if (Game_Step == 0) SH_CALL(Look_PadControl)();
    else SH_CALL(GameMode_LookEnd)();
    SH_CALL(Field_LoadingFrame)();
}

// original 0x496250 (no PSX twin paired): mode 6's step 1 - Look_Return, and
// with the camera back at yaw 0xFD56 and pitch 0x200, mode 2 (the field)
// with Field_Request and Game_Step 0.
extern "C" void __cdecl GameMode_LookEnd(void) {
    mode_states::g.look_return();
    if (static_cast<std::uint16_t>(Camera_Angles[0]) == 0xFD56 && mode_states::Word(mode_states::At(::bof3::Img(Cond_AngleFB_addr))) == 0x200) {
        Game_Mode = 2;
        Field_Request = 0;
        Game_Step = 0;
    }
}

// original 0x496290 (no PSX twin paired): Game_Mode 7 (GameMode_Handlers
// entry 7) - a tail jump through GameMode_ShopSteps 0x656AAC on Game_Step:
// 0 Shop_Open, 1 Shop_Frame, 2 Shop_Close. Unchecked: step 3 would run mode
// 8's first step, GameMode8_Enter 0x496440, the word after the table.
extern "C" void __cdecl GameMode_Shop(void) { Entry(::bof3::Img(GameMode_ShopSteps_addr), Game_Step)(); }

// original 0x4962A0 (no PSX twin paired; PSX 0x80199270): the shop's step 0. The object the
// shop was opened on is the s8 0x929F0C (Field_ObjectIdle sets it with
// Game_Mode 7). When it is an object and its dword +0x18 is 9, there is no
// shop: back to mode 2 at once - the object turned to face +0x85 unless its
// +7 bit 3 (and then its +0x80 bit 3 set), its +0x80 bit 5 cleared,
// Field_ScriptFlags bit 8 cleared, a field frame. Otherwise the shop's file
// DAT 0x31B is loaded with a loading frame and a sleep each frame of the
// wait, CLUT strip rows 1 and 2 kept, a field frame, and step 1.
//
// As the original has it: Sprite_Current is set to the object before the
// +0x18 test either way; the facing is read through Sprite_Current, the rest
// through the object; Game_Step goes up after the field frame, read afresh.
extern "C" void __cdecl Shop_Open(void) {
    const signed char index = static_cast<signed char>(ModeStates_ShopObject);
    if (index >= 0) {
        unsigned char* const o = rv::Bytes(Sprite_Objects) + static_cast<std::uint32_t>(static_cast<int>(index)) * 0xA4u;
        Sprite_Current = o;
        if (mode_states::Long(o + 0x18) == 9) {
            Game_Mode = 2;
            Game_Step = 0;
            Field_Request = 0;
            if ((o[7] & 8) == 0) {
                o[8] = o[0x85];
                mode_states::g.face_direction(Sprite_Current[8]);
                o[0x80] = static_cast<unsigned char>(o[0x80] | 8);
            }
            o[0x80] = static_cast<unsigned char>(o[0x80] & ~0x20u);
            Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFEFF);
            mode_states::g.field_frame();
            return;
        }
    }
    mode_states::g.load_dat(0x31B);
    WaitLoadFramed();
    mode_states::g.clut_copy_row(1);
    mode_states::g.clut_copy_row(2);
    Gfx_ClutStripDirty = 1;
    mode_states::g.field_frame();
    Game_Step = static_cast<unsigned short>(Game_Step + 1);
}

// original 0x496390 (no PSX twin paired; PSX 0x801993D8): the shop's step 2. The party
// combination byte 0x90412C gets bit 7, and the sound bank 0x2C2 + its low
// seven bits is loaded (the area's own, which the shop's file replaced) with
// the framed wait; with Field_InputFlags bit 4, DAT 0x12A too, with its wait,
// and CommuSim_RollOffers 0x4560D0 (R4A's); then mode 2 with Game_Step and Field_Request 0
// and a tail jump to Field_Frame.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Shop_Close(void) {
    const unsigned char combo = static_cast<unsigned char>(AreaW0b_PartySet | 0x80);
    AreaW0b_PartySet = combo;
    mode_states::g.bank_file((combo & 0x7Fu) + 0x2C2);
    WaitLoadFramed();
    if ((Field_InputFlags & 0x10) != 0) {
        mode_states::g.load_dat(0x12A);
        WaitLoadFramed();
        mode_states::g.after_shop();
    }
    Game_Mode = 2;
    Game_Step = 0;
    Field_Request = 0;
    mode_states::g.field_frame();
}

// original 0x496430 (GameMode_Handlers[8]): jmp [GameMode8_Steps + Game_Step *
// 4], unchecked.
extern "C" void __cdecl GameMode8_Run(void) {
    StepDispatchRest3g("GameMode8_Run", AddressOf(GameMode8_Steps), GameMode8_Steps_count);
}

// original 0x496440 (GameMode8_Steps[0]; PSX 0x801994E8): the dword 0x904144 up; Fish_Spawn;
// Transition_Start(1); AreaMap_Frame; Field_DrawFrame; a draw-area move of the
// rectangle (0, buffer * 0xF0 + 0x50, 0x40, 0x78) to (0x340, 0x100) committed
// at slot 5 (0x18); GameMode8_Frame; Field_ScriptFlags2 bit 6 clear, Game_Step
// up, Field_StepCount 0, Field_Request 0.
extern "C" void __cdecl GameMode8_Enter(void) {
    Rest4a_CountF = Rest4a_CountF + 1u;
    SH_CALL(Fish_Spawn)();
    SH_CALL(Transition_Start)(1);
    SH_CALL(AreaMap_Frame)();
    SH_CALL(Field_DrawFrame)();
    alignas(4) unsigned char rect[8];
    move_script::SetWord(rect, 0);
    move_script::SetWord(rect + 2, static_cast<U>(Gfx_BufferIndex) * 0xF0u + 0x50u);
    move_script::SetWord(rect + 4, 0x40);
    move_script::SetWord(rect + 6, 0x78);
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, 0x340, 0x100);
    SH_CALL(Gfx_CommitPrim)(5, 0x18);
    SH_CALL(GameMode8_Frame)();
    Field_ScriptFlags2 = Field_ScriptFlags2 & 0xFFBFu;
    Game_Step = static_cast<unsigned short>(Game_Step + 1);
    Field_StepCount = 0;
    Field_Request = 0;
}

// original 0x4964E0 (GameMode8_Steps[2]; PSX 0x801995C8): Field_ScriptFlags2 bit 6 set; the
// sound bank (0x90412C) and its framed wait (GameMode8_WaitFrame, a sleep);
// Music_FadeOut(0x10) if the track changes; Transition_Start(0) and its framed
// wait on the wait word; Field_ChangeArea(the word 0x802290, the dwords
// 0x7E091C, 0x7E0920, 4); Music_FadeOutStop(0xA) if the track changes; Game_Mode
// 1, Game_Step 0.
extern "C" void __cdecl GameMode8_Leave(void) {
    {
        const unsigned char was = AreaW0b_PartySet;
        move_script::At(::bof3::Img(Field_ScriptFlags2_addr))[0] = static_cast<unsigned char>(move_script::At(::bof3::Img(Field_ScriptFlags2_addr))[0] | 0x40);
        const unsigned char b = static_cast<unsigned char>(was | 0x80);
        AreaW0b_PartySet = b;
        SH_CALL(Snd_LoadBankFile)((b & 0x7Fu) + 0x2C2u);
    }
    WaitLoadRest3g(&Mode8Frame);
    if (MusicChanges()) SH_CALL(Music_FadeOut)(0x10);
    SH_CALL(Transition_Start)(0);
    WaitWord(&Mode8Frame);
    {
        const U z = ULRest3g(::bof3::Img(rest_3g::at::kReturnZ.v));
        const U x = ULRest3g(::bof3::Img(rest_3g::at::kReturnX.v));
        const unsigned area = AreaW2c_LastArea;
        SH_CALL(Field_ChangeArea)(area, I(x), I(z), 4);
    }
    if (MusicChanges()) SH_CALL(Music_FadeOutStop)(0xA);
    Game_Mode = 1;
    Game_Step = 0;
}

// original 0x4965C0 (GameMode_Handlers[9]): jmp [GameMode9_Steps + Game_Step *
// 4], unchecked.
extern "C" void __cdecl GameMode9_Run(void) {
    StepDispatchRest3g("GameMode9_Run", AddressOf(GameMode9_Steps), GameMode9_Steps_count);
}

// original 0x4965D0 (GameMode9_Steps[0]; PSX 0x80199780): the file 0xCB.
extern "C" void __cdecl GameMode9_Enter(void) { MenuModeEnter(0xCB); }

// original 0x496660 (GameMode9_Steps[2] and GameMode10_Steps[2]; PSX 0x8019983C): two sleeps;
// the sound bank (0x90412C) loaded, not waited for; Transition_Start(3) and its
// framed wait (Field_LoadingFrame); Game_Mode 2, Game_Step 0, Field_Request 0;
// a tail jump to Field_Frame.
extern "C" void __cdecl GameMode9_Leave(void) {
    SH_CALL(Task_Sleep)(1);
    SH_CALL(Task_Sleep)(1);
    LoadBank();
    SH_CALL(Transition_Start)(3);
    WaitWord(&LoadingFrame);
    Game_Mode = 2;
    Game_Step = 0;
    Field_Request = 0;
    SH_CALL(Field_Frame)();
}

// original 0x4966E0 (GameMode_Handlers[10]): jmp [GameMode10_Steps + Game_Step
// * 4], unchecked.
extern "C" void __cdecl GameMode10_Run(void) {
    StepDispatchRest3g("GameMode10_Run", AddressOf(GameMode10_Steps), GameMode10_Steps_count);
}

// original 0x4966F0 (GameMode10_Steps[0]; PSX 0x80199940): the file 0x31A.
extern "C" void __cdecl GameMode10_Enter(void) { MenuModeEnter(0x31A); }

// original 0x496780 (GameMode_Handlers[11]): jmp [GameMode11_Steps + Game_Step
// * 4], unchecked.
extern "C" void __cdecl GameMode11_Run(void) {
    StepDispatchRest3g("GameMode11_Run", AddressOf(GameMode11_Steps), GameMode11_Steps_count);
}

// original 0x496790 (GameMode11_Steps[0]; PSX 0x80199B00): Mode11_ObjectFrame, Mode11_FieldFrame;
// with Field_Request 0, Game_Mode 2.
extern "C" void __cdecl GameMode11_Frame(void) {
    SH_CALL(Mode11_ObjectFrame)();
    SH_CALL(Mode11_FieldFrame)();
    if (Field_Request == 0) Game_Mode = 2;
}

// original 0x4967B0 (GameMode11_Steps[1]; PSX 0x80199B44): Look_PadControl; a tail jump to
// Mode11_FieldFrame.
extern "C" void __cdecl GameMode11_Look(void) {
    SH_CALL(Look_PadControl)();
    SH_CALL(Mode11_FieldFrame)();
}

// original 0x4967C0 (GameMode11_Steps[2]; PSX 0x80199B6C): Look_Return, Mode11_FieldFrame; with
// the yaw word back at 0xFD56 and the pitch word at 0x200, Game_Step 0.
extern "C" void __cdecl GameMode11_LookEnd(void) {
    SH_CALL(Look_Return)();
    SH_CALL(Mode11_FieldFrame)();
    if (static_cast<std::uint16_t>(Camera_Angles[0]) == 0xFD56 && move_script::Word(move_script::At(::bof3::Img(Cond_AngleFB_addr))) == 0x200) Game_Step = 0;
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
        mode_flow::g.sleep(1);
        if (Transition_Busy == 0 && run == 0) {
            mode_flow::g.sleep(1);
            return;
        }
        mode_flow::g.field_frame();
        if (Transition_Busy == 0) return;
    }
}

// original 0x496830 (no PSX twin paired; PSX 0x80199C38): Field_WaitTransition with the menu's
// frame - called by mode 3's steps 0 and 2 after Transition_Start(3) / (2).
// Loop: a frame's sleep; with Transition_Busy 0 and `run` 0, one more
// sleep and return; else Menu_Frame, and return once the word is 0.
//
// As the original has it: `run` is the argument's byte, read once, and
// tested every pass; the wait word is read afresh at both tests.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Menu_WaitTransition(unsigned char run_argument) {
    const unsigned char run = SlotByte(run_argument);
    for (;;) {
        mode_states::g.sleep(1);
        if (Transition_Busy == 0 && run == 0) {
            mode_states::g.sleep(1);
            return;
        }
        mode_states::g.menu_frame();
        if (Transition_Busy == 0) return;
    }
}

extern "C" void __cdecl Game_ClockTick(void) {
    if (Field_Request == 9) return;
    unsigned char* const c = mode_flow::At(::bof3::Img(mode_flow::at::kClock));
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
    CountDown(::bof3::Img(ModeFlow_CountdownA_addr));
    CountDown(::bof3::Img(Rest3c_Byte660_addr));
}

// original 0x496A00 (no PSX twin paired; PSX 0x80199F1C): mode 6's step 0, and mode 11's
// step 1. While none of the buttons of the map word 0x903586 is held in
// Input_Held, the look ends: the pitch word above 0x200 leaves the turn byte
// 0x66C7D9 0, below it sets it 1 (equal leaves it), and the step moves on.
// Otherwise bits 8..15 of the held word turn the camera 22 a frame, each
// word and its Light_AnglesCopy word together: 0x40 yaw down to 0xFD56,
// 0x10 yaw up to 0xFE39, 0x20 pitch up to 0x355, 0x80 pitch down to 0xAA.
//
// As the original has it: every limit is a signed 16-bit compare of the word
// read afresh, and the step past a limit is not clamped (the word may end up
// to 21 past it); the held word is read once.
extern "C" void __cdecl Look_PadControl(void) {
    const unsigned held = Input_Held;
    if ((mode_states::Word(mode_states::At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f006)) & held & 0xFFFF) == 0) {
        const short pitch = Signed16(::bof3::Img(kPitch));
        if (pitch > 0x200) {
            ModeStates_LookTurn = 0;
        } else if (pitch < 0x200) {
            ModeStates_LookTurn = 1;
        }
        Game_Step = static_cast<unsigned short>(Game_Step + 1);
        return;
    }
    const unsigned buttons = (held >> 8) & 0xFF;
    if ((buttons & 0x40) != 0 && Camera_Angles[0] > static_cast<short>(0xFD56)) {
        AddWordModeStates(::bof3::Img(kYaw), -0x16);
        AddWordModeStates(::bof3::Img(kYawCopy), -0x16);
    }
    if ((buttons & 0x10) != 0 && Camera_Angles[0] < static_cast<short>(0xFE39)) {
        AddWordModeStates(::bof3::Img(kYaw), 0x16);
        AddWordModeStates(::bof3::Img(kYawCopy), 0x16);
    }
    if ((buttons & 0x20) != 0 && Signed16(::bof3::Img(kPitch)) < 0x355) {
        AddWordModeStates(::bof3::Img(kPitch), 0x16);
        AddWordModeStates(::bof3::Img(kPitchCopy), 0x16);
    }
    if ((buttons & 0x80) != 0 && Signed16(::bof3::Img(kPitch)) > 0xAA) {
        AddWordModeStates(::bof3::Img(kPitch), -0x16);
        AddWordModeStates(::bof3::Img(kPitchCopy), -0x16);
    }
}

// original 0x496AD0 (no PSX twin paired; PSX 0x8019A0D0): one frame of the camera's way back
// from a look - yaw 22 down to 0xFD56 (below it, snapped there), and the
// pitch word 22 towards 0x200, down with the turn byte 0x66C7D9 at 0 and up
// otherwise, snapped to 0x200 once there or past it; each word with its
// Light_AnglesCopy word. Nothing moves once the pitch is 0x200.
//
// As the original has it: the yaw copy is stepped by 22 or set to 0xFD56 with
// the yaw, never compared; the pitch is read once and its copy stepped where
// it lies.
extern "C" void __cdecl Look_Return(void) {
    if (Camera_Angles[0] > static_cast<short>(0xFD56)) {
        AddWordModeStates(::bof3::Img(kYaw), -0x16);
        AddWordModeStates(::bof3::Img(kYawCopy), -0x16);
    } else {
        Camera_Angles[0] = 0xFD56;
        Light_AnglesCopy[0] = 0xFD56;
    }
    const short pitch = Signed16(::bof3::Img(kPitch));
    if (pitch == 0x200) return;
    if (ModeStates_LookTurn == 0) {
        if (pitch <= 0x200) {
            mode_states::SetWord(mode_states::At(::bof3::Img(Cond_AngleFB_addr)), 0x200);
            Light_AnglesCopy[2] = 0x200;
            return;
        }
        AddWordModeStates(::bof3::Img(kPitchCopy), -0x16);
        mode_states::SetWord(mode_states::At(::bof3::Img(Cond_AngleFB_addr)), static_cast<unsigned>(pitch - 0x16));
        return;
    }
    if (pitch >= 0x200) {
        mode_states::SetWord(mode_states::At(::bof3::Img(Cond_AngleFB_addr)), 0x200);
        Light_AnglesCopy[2] = 0x200;
        return;
    }
    AddWordModeStates(::bof3::Img(kPitchCopy), 0x16);
    mode_states::SetWord(mode_states::At(::bof3::Img(Cond_AngleFB_addr)), static_cast<unsigned>(pitch + 0x16));
}

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
    mode_flow::g.clear_rect(0, 0, 0x400, 0x200);
    mode_flow::g.dropped(0);
    mode_flow::g.load_dat(0x225);
    while (mode_flow::g.load_done() == 0) mode_flow::g.sleep(1);
    std::memset(mode_flow::At(::bof3::Img(mode_flow::at::kBootClear)), 0, mode_flow::at::kBootClearBytes);
    std::memcpy(mode_flow::At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f000), mode_flow::At(::bof3::Img(ModeFlow_ButtonDefaults_addr)), 0x12);
    // DIV-0133 and DIV-0134 (docs/button-sigma.md): the release's starting
    // words by source, then words 7 / 8 as Speak's and Action's and the
    // table - nothing until sigma::Arm, after every module's self-test.
    bof3x::input::sigma::BootDefaultsWritten();
    AreaW0b_PartySet = 0xFF;
    Field_ScriptFlags = 0;
    Field_ScriptFlags2 = 0;
    unsigned char* const config = mode_flow::At(::bof3::Img(ModeFlow_ConfigBytes_addr));
    config[0] = 1;
    for (unsigned i = 1; i < 7; ++i) config[i] = 0;
    mode_flow::g.window_reset();
    mode_flow::g.tint_reset();
    mode_flow::g.slots_release();
    Draw_PassFlags = 0x1F;
    Draw_OtSlot = 6;
    BattleE2_AnimSet = ::bof3::Img(0x8B3580);
    mode_flow::SetLong(mode_flow::At(::bof3::Img(MoveScript_TintRecords_addr) + 0x180), ::bof3::Img(0x8C3584));
    mode_flow::g.clut_restore();
    Gfx_ClutStripDirty = static_cast<unsigned char>(Gfx_ClutStripDirty + 1);
    mode_flow::g.task_create(1, reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(kTitleLoadEntry.v))));
    mode_flow::g.task_exit();
}

// original 0x496C90: task 1's first body - DAT 0x226 loaded and waited for,
// then a tail jump into Title_Task (which never returns).
extern "C" void __cdecl Title_LoadTask(void) {
    mode_flow::g.load_dat(0x226);
    while (mode_flow::g.load_done() == 0) mode_flow::g.sleep(1);
    mode_flow::g.title_task();
}

void TransitionFromModeFlow_Inject() {
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
    BOF3_INJECT(Boot_Task);
    BOF3_INJECT(Title_LoadTask);
}

void TransitionFromModeRest_Inject() {
    BOF3_INJECT(GameMode3_Run);
    BOF3_INJECT(GameMode3_Enter);
    BOF3_INJECT(GameMode3_Leave);
    BOF3_INJECT(GameMode4_Run);
    BOF3_INJECT(GameMode5_Run);
    BOF3_INJECT(GameMode5_TurnSense);
    BOF3_INJECT(GameMode5_Turn);
    BOF3_INJECT(GameMode5_ToPlaces);
    BOF3_INJECT(GameMode5_Load);
    BOF3_INJECT(GameMode5_Script);
    BOF3_INJECT(GameMode5_Place);
    BOF3_INJECT(GameMode5_Leave);
    BOF3_INJECT(GameMode6_Run);
}

void TransitionFromModeStates_Inject() {
    BOF3_INJECT(GameMode_LookEnd);
    BOF3_INJECT(GameMode_Shop);
    BOF3_INJECT(Shop_Open);
    BOF3_INJECT(Shop_Close);
    BOF3_INJECT(Menu_WaitTransition);
    BOF3_INJECT(Look_PadControl);
    BOF3_INJECT(Look_Return);
}

void TransitionFromModeTasks_Inject() {
    BOF3_INJECT(GameMode_Field);
}

void TransitionFromRest3g_Inject() {
    BOF3_INJECT(GameMode8_Run);
    BOF3_INJECT(GameMode8_Enter);
    BOF3_INJECT(GameMode8_Leave);
    BOF3_INJECT(GameMode9_Run);
    BOF3_INJECT(GameMode9_Enter);
    BOF3_INJECT(GameMode9_Leave);
    BOF3_INJECT(GameMode10_Run);
    BOF3_INJECT(GameMode10_Enter);
    BOF3_INJECT(GameMode11_Run);
    BOF3_INJECT(GameMode11_Frame);
    BOF3_INJECT(GameMode11_Look);
    BOF3_INJECT(GameMode11_LookEnd);
}
