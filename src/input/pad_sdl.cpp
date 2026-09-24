#include "input/pad_sdl.h"

#include <SDL3/SDL.h>

#include <cstdio>

namespace bof3x::input {
namespace {

// Half travel, the original's 500 of 1000 on its DirectInput axes.
constexpr Sint16 kStickThreshold = 16384;
constexpr Sint16 kTriggerThreshold = 16384;

bool g_started = false;
SDL_Gamepad* g_pad = nullptr;
SDL_JoystickID g_pad_id = 0;
Layout g_layout = Layout::kPositional;
bool g_nintendo = false;
PadLog g_log = nullptr;

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

// The physical face button for a map position.
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

void PadSdl_Stop() {
    if (!g_started) return;
    ClosePad();
    SDL_Quit();
    g_started = false;
}

bool PadSdl_Started() { return g_started; }

// SDL_UpdateGamepads notices devices coming and going and queues the events;
// drain them so the queue never fills, and act on the two that matter.
void PadSdl_Poll() {
    if (!g_started) return;
    SDL_UpdateGamepads();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
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

bool PadSdl_Open() { return g_pad != nullptr; }

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
