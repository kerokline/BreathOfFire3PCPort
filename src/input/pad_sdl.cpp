#include "input/pad_sdl.h"

#include <SDL3/SDL.h>

#include <cstdio>

namespace bof3x::input {
namespace {

// Half travel, the original's 500 of 1000 on its DirectInput axes (Pad_Read
// 0x5A9700, docs/controls.md section 1). SDL's sticks run -32768..32767 and
// its triggers 0..32767, so 16384 is half of either.
constexpr Sint16 kStickThreshold = 16384;
constexpr Sint16 kTriggerThreshold = 16384;

bool g_started = false;
SDL_Gamepad* g_pad = nullptr;
SDL_JoystickID g_pad_id = 0;
Layout g_layout = Layout::kPositional;
bool g_nintendo = false;
PadLog g_log = nullptr;

// One line to the caller's log. `fmt` may use at most %s, %d, %s, in that
// order, fed from a, b, c; a caller with only a number passes "" for a and
// puts a bare %s before its %d.
void Log(const char* fmt, const char* a = "", int b = 0, const char* c = "") {
    if (!g_log) return;
    char line[256];
    std::snprintf(line, sizeof line, fmt, a, b, c);
    g_log(line);
}

void OpenPad(SDL_JoystickID id) {
    g_pad = SDL_OpenGamepad(id);
    if (!g_pad) {
        Log("pad: SDL_OpenGamepad(%s%d) failed: %s", "", static_cast<int>(id), SDL_GetError());
        return;
    }
    g_pad_id = id;
    g_nintendo = g_layout == Layout::kNintendo;
    // Auto: a pad whose lower face button is lettered B is Nintendo-lettered.
    if (g_layout == Layout::kAuto)
        g_nintendo = SDL_GetGamepadButtonLabel(g_pad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_B;
    const char* name = SDL_GetGamepadName(g_pad);
    Log("pad: opened %s (type %d, layout %s)", name ? name : "?", static_cast<int>(SDL_GetGamepadType(g_pad)),
        g_nintendo ? "nintendo" : "positional");
}

void ClosePad() {
    if (!g_pad) return;
    SDL_CloseGamepad(g_pad);
    g_pad = nullptr;
    g_pad_id = 0;
    Log("pad: closed");
}

void OpenAnyPad() {
    if (g_pad) return;
    int n = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&n);
    if (ids && n > 0) OpenPad(ids[0]);
    SDL_free(ids);
}

// The physical face button for a map position; `input` is one of the four
// face positions (the default case is north).
SDL_GamepadButton FaceButton(PadInput input) {
    switch (input) {
    case PadInput::kSouth: return g_nintendo ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_SOUTH;
    case PadInput::kEast: return g_nintendo ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST;
    case PadInput::kWest: return g_nintendo ? SDL_GAMEPAD_BUTTON_NORTH : SDL_GAMEPAD_BUTTON_WEST;
    default: return g_nintendo ? SDL_GAMEPAD_BUTTON_WEST : SDL_GAMEPAD_BUTTON_NORTH;
    }
}

}  // namespace

bool PadSdl_Start(Layout layout, PadLog log) {
    if (g_started) return true;
    g_log = log;
    g_layout = layout;
    // The DLL's keyboard is read whether or not the window is in front
    // (DISCL_BACKGROUND) and DIV-0033 decides what to do with the words; the
    // pad follows the same rule. The launcher's dialogs are in front anyway.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        Log("pad: SDL_Init(GAMEPAD) failed: %s - no pad", SDL_GetError());
        return false;
    }
    g_started = true;
    const int v = SDL_GetVersion();
    char ver[32];
    std::snprintf(ver, sizeof ver, "%d.%d.%d", SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v),
                  SDL_VERSIONNUM_MICRO(v));
    Log("pad: SDL %s, gamepad subsystem up", ver);
    OpenAnyPad();
    return true;
}

// SDL_Quit, not SDL_QuitSubSystem: in the launcher this file is SDL's only
// user, and in the DLL its stop is the last of SDL's (Game_WinMain's exit:
// the window's video, then the sound, then DInput_Shutdown, which calls this).
void PadSdl_Stop() {
    if (!g_started) return;
    ClosePad();
    SDL_Quit();
    g_started = false;
}

bool PadSdl_Started() { return g_started; }

// SDL_UpdateGamepads notices devices coming and going (SDL_UpdateJoysticks:
// each driver's Detect; on Windows the device-change window is SDL's own
// thread's) and queues the events; this takes the joystick and gamepad events
// out of SDL's queue (SDL_EVENT_JOYSTICK_AXIS_MOTION up to the touch events)
// and acts on the two that matter.
//
// It pumps only where nothing else does. With SDL's video up (the game's
// window, game_window.h) the window's owner pumps - game_window::Pump at the
// loop's message step, and the SDL keyboard's read at the latch (d_input.cpp) -
// and drops what no one takes, and runs this poll at each message step so
// that the pad's events leave the queue while Pad_Read does not run (a pause,
// the window not in front); a pump here would dispatch the thread's window
// messages from inside Pad_Read and the FMV player's skip test, and a title-bar
// drag's modal loop with them. Without SDL's video (the launcher, and the game
// under BOF3X_OFF=WindowSdl, whose Win32 pump is PeekMessage alone) this poll
// is SDL's one pump, as it always was, and it drops every other event so that
// SDL's queue never fills.
void PadSdl_Poll() {
    if (!g_started) return;
    const bool alone = SDL_WasInit(SDL_INIT_VIDEO) == 0;
    if (alone) SDL_PumpEvents();
    SDL_UpdateGamepads();
    SDL_Event events[16];
    int n = 0;
    while ((n = SDL_PeepEvents(events, 16, SDL_GETEVENT, SDL_EVENT_JOYSTICK_AXIS_MOTION, SDL_EVENT_FINGER_DOWN - 1)) > 0) {
        for (int i = 0; i < n; ++i) {
            const SDL_Event& e = events[i];
            if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
                if (!g_pad) OpenPad(e.gdevice.which);
            } else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
                if (g_pad && e.gdevice.which == g_pad_id) {
                    ClosePad();
                    OpenAnyPad();
                }
            }
        }
    }
    if (alone) SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
}

bool PadSdl_Open() { return g_pad != nullptr; }

void PadSdl_Find() {
    if (!g_started) return;
    if (g_pad && !SDL_GamepadConnected(g_pad)) ClosePad();
    OpenAnyPad();
}

bool PadSdl_InputDown(PadInput input) {
    if (!g_pad) return false;
    auto button = [&](SDL_GamepadButton b) { return SDL_GetGamepadButton(g_pad, b); };
    auto axis = [&](SDL_GamepadAxis a, bool positive) {
        const Sint16 v = SDL_GetGamepadAxis(g_pad, a);
        return positive ? v > kStickThreshold : v < -kStickThreshold;
    };
    switch (input) {
    case PadInput::kSouth: case PadInput::kEast: case PadInput::kWest: case PadInput::kNorth:
        return button(FaceButton(input));
    case PadInput::kLb: return button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    case PadInput::kRb: return button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    case PadInput::kLt: return SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > kTriggerThreshold;
    case PadInput::kRt: return SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > kTriggerThreshold;
    case PadInput::kStart: return button(SDL_GAMEPAD_BUTTON_START);
    case PadInput::kBack: return button(SDL_GAMEPAD_BUTTON_BACK);
    case PadInput::kGuide: return button(SDL_GAMEPAD_BUTTON_GUIDE);
    case PadInput::kLs: return button(SDL_GAMEPAD_BUTTON_LEFT_STICK);
    case PadInput::kRs: return button(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    case PadInput::kDpadUp: return button(SDL_GAMEPAD_BUTTON_DPAD_UP);
    case PadInput::kDpadDown: return button(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    case PadInput::kDpadLeft: return button(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    case PadInput::kDpadRight: return button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    case PadInput::kLsUp: return axis(SDL_GAMEPAD_AXIS_LEFTY, false);
    case PadInput::kLsDown: return axis(SDL_GAMEPAD_AXIS_LEFTY, true);
    case PadInput::kLsLeft: return axis(SDL_GAMEPAD_AXIS_LEFTX, false);
    case PadInput::kLsRight: return axis(SDL_GAMEPAD_AXIS_LEFTX, true);
    case PadInput::kRsUp: return axis(SDL_GAMEPAD_AXIS_RIGHTY, false);
    case PadInput::kRsDown: return axis(SDL_GAMEPAD_AXIS_RIGHTY, true);
    case PadInput::kRsLeft: return axis(SDL_GAMEPAD_AXIS_RIGHTX, false);
    case PadInput::kRsRight: return axis(SDL_GAMEPAD_AXIS_RIGHTX, true);
    case PadInput::kMisc1: return button(SDL_GAMEPAD_BUTTON_MISC1);
    case PadInput::kRightPaddle1: return button(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1);
    case PadInput::kLeftPaddle1: return button(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1);
    case PadInput::kRightPaddle2: return button(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2);
    case PadInput::kLeftPaddle2: return button(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2);
    case PadInput::kTouchpad: return button(SDL_GAMEPAD_BUTTON_TOUCHPAD);
    case PadInput::kMisc2: return button(SDL_GAMEPAD_BUTTON_MISC2);
    case PadInput::kMisc3: return button(SDL_GAMEPAD_BUTTON_MISC3);
    case PadInput::kMisc4: return button(SDL_GAMEPAD_BUTTON_MISC4);
    case PadInput::kMisc5: return button(SDL_GAMEPAD_BUTTON_MISC5);
    case PadInput::kMisc6: return button(SDL_GAMEPAD_BUTTON_MISC6);
    default: return false;
    }
}

unsigned PadSdl_Word(const std::vector<PadBinding>& map) {
    if (!g_pad) return 0;
    unsigned word = 0;
    for (const PadBinding& b : map)
        if (PadSdl_InputDown(b.input)) word |= b.bits;
    return word;
}

int PadSdl_FirstInputDown() {
    if (!g_pad) return -1;
    for (int i = 0; i < static_cast<int>(PadInput::kCount); ++i)
        if (PadSdl_InputDown(static_cast<PadInput>(i))) return i;
    return -1;
}

}  // namespace bof3x::input
