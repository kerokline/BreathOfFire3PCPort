// The input devices, ours (DIV-0050, docs/controls.md).
//
// Three functions of the port's input layer are replaced together:
//
//   DInput_Init      0x5A94C0  opens the devices at Game_Init
//   Pad_Read         0x5A9700  builds the PlayStation pad word each latch
//   DInput_Shutdown  0x5A9690  releases them at WinMain's exit
//
// The keyboard is Capcom's, unchanged: the DirectInput 7 system keyboard,
// non-exclusive and background, read into the 256 key bytes at Key_State and
// mapped through the 32-entry (DIK scancode, pad bits) table at Key_Table -
// BOF3.CFG's lines 3 and up, or the default at Key_TableDefault. That path is
// reproduced instruction for instruction (the shadow check below compares it
// with a clone of the original over random key states and tables).
//
// The pad is not Capcom's. The original enumerated the first DirectInput
// joystick and read it digitally with a fixed button order that matches no
// modern pad - the POV hat was never read, no button reached L2, and an
// Xbox-class pad came out as A = cross, B = square, X = triangle, Y = circle
// (docs/controls.md section 1). The joystick enumeration is dropped, and the
// pad word comes from SDL3's gamepad layer instead: one pad, the first
// connected, re-opened on hot-plug; the d-pad and the left stick both the
// directions, the triggers L2 / R2, the face buttons by position or by the
// Nintendo layout (BOF3X_PAD_LAYOUT=nintendo swaps them; auto follows the
// pad's own labels). The stick threshold is the original's: half travel.
//
// Pad_Read runs from WinMain's loop on the main stack, so SDL's calls have
// room; nothing here runs on a task's 16 KB stack.
#include "game/pad_read.h"

#include <SDL3/SDL.h>
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0700
#include <dinput.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

// --- The PlayStation pad's bits (docs/input-script.md section 2) -----------
constexpr unsigned kL2 = 0x1, kR2 = 0x2, kL1 = 0x4, kR1 = 0x8, kTriangle = 0x10, kCircle = 0x20,
                   kCross = 0x40, kSquare = 0x80, kSelect = 0x100, kStart = 0x800, kUp = 0x1000,
                   kRight = 0x2000, kDown = 0x4000, kLeft = 0x8000;

constexpr unsigned kKeyTableEntries = 32;   // (u16 key, u16 bits) pairs; a key of 0 ends it
constexpr HRESULT kInputLost = static_cast<HRESULT>(0x8007001E);   // DIERR_INPUTLOST

// --- The keyboard, as the original reads it ----------------------------------

IDirectInputDeviceA* Keyboard() { return static_cast<IDirectInputDeviceA*>(DInput_Keyboard); }

// The table over the key bytes: the loop at 0x5A981F..0x5A984E.
unsigned KeyboardWord() {
    unsigned word = 0;
    for (unsigned i = 0; i < kKeyTableEntries; ++i) {
        const unsigned short key = Key_Table[i * 2];
        if (key == 0) break;
        if (Key_State[key]) word |= Key_Table[i * 2 + 1];
    }
    return word;
}

// GetDeviceState into Key_State, re-acquiring on DIERR_INPUTLOST as the
// original does (0x5A97ED..0x5A981D); any other failure leaves the bytes as
// they were, and the table is applied over them regardless - also the
// original's behaviour.
void ReadKeyboard() {
    IDirectInputDeviceA* kb = Keyboard();
    if (!kb) return;
    for (;;) {
        const HRESULT hr = kb->GetDeviceState(Key_State_count, Key_State);
        if (hr != kInputLost) return;
        kb->Acquire();
    }
}

// --- The pad, through SDL3 -----------------------------------------------------

enum class Layout { kPositional, kNintendo, kAuto };

struct PadButton {
    SDL_GamepadButton button;
    unsigned bits;
};

// The face buttons by position (the owner's default): south cross, east
// circle, west square, north triangle. The Nintendo layout swaps each pair.
constexpr PadButton kFacePositional[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH, kCross}, {SDL_GAMEPAD_BUTTON_EAST, kCircle},
    {SDL_GAMEPAD_BUTTON_WEST, kSquare}, {SDL_GAMEPAD_BUTTON_NORTH, kTriangle}};
constexpr PadButton kFaceNintendo[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH, kCircle}, {SDL_GAMEPAD_BUTTON_EAST, kCross},
    {SDL_GAMEPAD_BUTTON_WEST, kTriangle}, {SDL_GAMEPAD_BUTTON_NORTH, kSquare}};
constexpr PadButton kOtherButtons[] = {
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, kL1},  {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, kR1},
    {SDL_GAMEPAD_BUTTON_START, kStart},       {SDL_GAMEPAD_BUTTON_BACK, kSelect},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, kUp},        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, kDown},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, kLeft},    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, kRight}};

// Half travel, the original's 500 of 1000 on the DirectInput axes.
constexpr Sint16 kStickThreshold = 16384;
constexpr Sint16 kTriggerThreshold = 16384;

bool g_sdl = false;
SDL_Gamepad* g_pad = nullptr;
SDL_JoystickID g_pad_id = 0;
Layout g_layout = Layout::kPositional;
bool g_pad_nintendo = false;   // the layout in force for the open pad

Layout LayoutFromEnv() {
    char v[32] = {};
    const DWORD n = GetEnvironmentVariableA("BOF3X_PAD_LAYOUT", v, sizeof v);
    if (n == 0 || n >= sizeof v) return Layout::kPositional;
    if (std::strcmp(v, "nintendo") == 0) return Layout::kNintendo;
    if (std::strcmp(v, "auto") == 0) return Layout::kAuto;
    if (std::strcmp(v, "positional") != 0)
        bof3::Log("pad: BOF3X_PAD_LAYOUT=%s not understood; positional", v);
    return Layout::kPositional;
}

void OpenPad(SDL_JoystickID id) {
    g_pad = SDL_OpenGamepad(id);
    if (!g_pad) {
        bof3::Log("pad: SDL_OpenGamepad(%u) failed: %s", static_cast<unsigned>(id), SDL_GetError());
        return;
    }
    g_pad_id = id;
    bool nintendo = g_layout == Layout::kNintendo;
    if (g_layout == Layout::kAuto)
        nintendo = SDL_GetGamepadButtonLabel(g_pad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_B;
    g_pad_nintendo = nintendo;
    const char* name = SDL_GetGamepadName(g_pad);
    bof3::Log("pad: opened %s (type %d, layout %s)", name ? name : "?", static_cast<int>(SDL_GetGamepadType(g_pad)),
              nintendo ? "nintendo" : "positional");
}

void ClosePad() {
    if (!g_pad) return;
    SDL_CloseGamepad(g_pad);
    g_pad = nullptr;
    g_pad_id = 0;
    bof3::Log("pad: closed");
}

// The first connected pad, if none is open.
void OpenAnyPad() {
    if (g_pad) return;
    int n = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&n);
    if (ids && n > 0) OpenPad(ids[0]);
    SDL_free(ids);
}

// Hot-plug: SDL_UpdateGamepads notices devices coming and going and queues the
// events; drain them so the queue never fills, and act on the two that matter.
void PollPadEvents() {
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

unsigned PadWord() {
    if (!g_sdl) return 0;
    PollPadEvents();
    if (!g_pad) return 0;
    unsigned word = 0;
    for (const PadButton& b : g_pad_nintendo ? kFaceNintendo : kFacePositional)
        if (SDL_GetGamepadButton(g_pad, b.button)) word |= b.bits;
    for (const PadButton& b : kOtherButtons)
        if (SDL_GetGamepadButton(g_pad, b.button)) word |= b.bits;
    const Sint16 x = SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFTX);
    const Sint16 y = SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFTY);
    if (x > kStickThreshold) word |= kRight;
    if (x < -kStickThreshold) word |= kLeft;
    if (y > kStickThreshold) word |= kDown;
    if (y < -kStickThreshold) word |= kUp;
    if (SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > kTriggerThreshold) word |= kL2;
    if (SDL_GetGamepadAxis(g_pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > kTriggerThreshold) word |= kR2;
    return word;
}

void StartSdl() {
    if (g_sdl) return;
    g_layout = LayoutFromEnv();
    // The keyboard is read whether or not the window is in front (DISCL_BACKGROUND),
    // and DIV-0033 decides what to do with the words; the pad follows the same rule.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        bof3::Log("pad: SDL_Init(GAMEPAD) failed: %s - no pad", SDL_GetError());
        return;
    }
    g_sdl = true;
    const int v = SDL_GetVersion();
    bof3::Log("pad: SDL %d.%d.%d, gamepad subsystem up", SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v),
              SDL_VERSIONNUM_MICRO(v));
    OpenAnyPad();
}

void StopSdl() {
    if (!g_sdl) return;
    ClosePad();
    SDL_Quit();
    g_sdl = false;
}

// --- BOF3X_SHADOW=pad_read: the keyboard path against a clone --------------------
// With both device pointers null the original is the table loop and nothing
// else (0x5A9700: je on DInput_Joystick, je on DInput_Keyboard). The clone
// runs that way over random key bytes and random tables - including tables
// that end early, and bit patterns across both bytes - against KeyboardWord.
// The function has no direct calls (only COM calls through vtables, which
// the nulls skip), so a byte copy runs in place.

using WordFn = unsigned (__cdecl*)();

void SelfTest(WordFn theirs) {
    constexpr unsigned kRounds = 20000;
    unsigned char saved_state[256];
    unsigned short saved_table[kKeyTableEntries * 2];
    std::memcpy(saved_state, Key_State, sizeof saved_state);
    std::memcpy(saved_table, Key_Table, sizeof saved_table);
    void* const saved_joy = DInput_Joystick;
    void* const saved_kb = DInput_Keyboard;
    DInput_Joystick = nullptr;
    DInput_Keyboard = nullptr;

    std::uint32_t rng = 0x2545F491u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };

    unsigned bad = 0, nonzero = 0, short_tables = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        for (unsigned i = 0; i < 256; ++i) Key_State[i] = static_cast<unsigned char>(next() % 4 == 0 ? next() : 0);
        const unsigned len = next() % (kKeyTableEntries + 1);
        if (len < kKeyTableEntries) ++short_tables;
        for (unsigned i = 0; i < kKeyTableEntries; ++i) {
            // keys are scancodes 0..255; a 0 key ends the table, so live entries avoid it
            Key_Table[i * 2] = i < len ? static_cast<unsigned short>(1 + next() % 255) : 0;
            Key_Table[i * 2 + 1] = static_cast<unsigned short>(next());
        }
        if (round % 7 == 0) Key_Table[(next() % kKeyTableEntries) * 2] = 0;   // an early end at random
        const unsigned t = theirs();
        const unsigned o = KeyboardWord();
        if (o) ++nonzero;
        if (t != o) {
            if (bad < 5) bof3::Log("pad_read shadow: round %u theirs %#x ours %#x", round, t, o);
            ++bad;
        }
    }

    DInput_Joystick = saved_joy;
    DInput_Keyboard = saved_kb;
    std::memcpy(Key_State, saved_state, sizeof saved_state);
    std::memcpy(Key_Table, saved_table, sizeof saved_table);
    bof3::Log("pad_read shadow: %u rounds, %u differ, %u non-zero words, %u short tables%s", kRounds, bad, nonzero,
              short_tables, bad ? " - FAIL" : "");
}

}  // namespace

// --- The three originals ------------------------------------------------------

// DInput_Init 0x5A94C0: the key bytes zeroed, DirectInputCreateA, and the
// system keyboard opened exactly as the original opens it; the joystick
// enumeration (0x5A94F5..0x5A95CB) is not run, its four globals stay null,
// and SDL's gamepad subsystem is started instead.
// The arguments are (hinstance, hwnd): 0x5A94D2 pushes eax, then reads
// [esp+0x24] - the first argument - for DirectInputCreateA, and 0x5A950C
// reads the second for SetCooperativeLevel (symbols.toml had them reversed
// until 2026-09-24; the first live run passed the window as the instance,
// DIERR_INVALIDPARAM).
extern "C" void __cdecl DInput_Init(void* hinstance, void* hwnd) {
    std::memset(Key_State, 0, Key_State_count);
    using CreateFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, LPDIRECTINPUTA*, LPUNKNOWN);
    HMODULE dinput = LoadLibraryA("dinput.dll");
    CreateFn create = dinput ? reinterpret_cast<CreateFn>(GetProcAddress(dinput, "DirectInputCreateA")) : nullptr;
    LPDIRECTINPUTA di = nullptr;
    const HRESULT created = create ? create(static_cast<HINSTANCE>(hinstance), DIRECTINPUT_VERSION, &di, nullptr)
                                   : static_cast<HRESULT>(E_FAIL);
    if (created != DI_OK || !di) {
        bof3::Log("pad: DirectInputCreateA failed (dinput.dll %p, entry %p, hinstance %p, hr %#lx) - no keyboard through DirectInput",
                  static_cast<void*>(dinput), reinterpret_cast<void*>(create), hinstance, static_cast<unsigned long>(created));
        StartSdl();
        return;
    }
    DInput_Object = di;
    LPDIRECTINPUTDEVICEA kb = nullptr;
    // The GUID and the data format are the exe's own copies, the ones the
    // original passes (0x5C4828 = GUID_SysKeyboard, 0x5C4948 = c_dfDIKeyboard:
    // dwSize 0x18, dwObjSize 0x10, DIDF_RELAXIS, 256 bytes, 256 objects).
    const GUID& guid = *reinterpret_cast<const GUID*>(DInput_KeyboardGuid);
    const DIDATAFORMAT* format = reinterpret_cast<const DIDATAFORMAT*>(DInput_KeyboardFormat);
    if (di->CreateDevice(guid, &kb, nullptr) == DI_OK && kb) {
        // CreateDevice (vtable +0xC), then SetDataFormat (+0x2C),
        // SetCooperativeLevel (+0x34) and Acquire (+0x1C): 0x5A95CC..0x5A960C.
        DInput_Keyboard = kb;
        kb->SetDataFormat(format);
        kb->SetCooperativeLevel(static_cast<HWND>(hwnd), DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
        kb->Acquire();
    } else {
        bof3::Log("pad: CreateDevice(GUID_SysKeyboard) failed");
    }
    StartSdl();
}

// Pad_Read 0x5A9700: the keyboard as the original, the pad from SDL. The
// high word is 0, as the original's always was (pad 2 never had a device).
extern "C" unsigned int __cdecl Pad_Read(void) {
    ReadKeyboard();
    return (KeyboardWord() | PadWord()) & 0xFFFFu;
}

// DInput_Shutdown 0x5A9690: the same releases, each slot zeroed; the joystick
// slots are null unless the original DInput_Init ran (BOF3X_ORIGINAL), in
// which case they are released the same way. Then SDL is stopped.
extern "C" void __cdecl DInput_Shutdown(void) {
    if (auto* joy = static_cast<IDirectInputDeviceA*>(DInput_Joystick)) {
        joy->Unacquire();
        joy->Release();
        DInput_Joystick = nullptr;
    }
    if (auto* joy2 = static_cast<IUnknown*>(DInput_Joystick2)) {
        joy2->Release();
        DInput_Joystick2 = nullptr;
    }
    if (auto* kb = Keyboard()) {
        kb->Unacquire();
        kb->Release();
        DInput_Keyboard = nullptr;
    }
    if (auto* di = static_cast<IUnknown*>(DInput_Object)) {
        di->Release();
        DInput_Object = nullptr;
    }
    StopSdl();
}

void PadRead_Inject() {
    static_assert(Key_State_count == 256);
    static_assert(Key_Table_count == kKeyTableEntries * 2);
    // 0x5A9700..0x5A9855, 0x155 bytes: no direct calls, every jump internal (disasm 2026-09-24).
    if (bof3::WantsShadow("pad_read"))
        SelfTest(reinterpret_cast<WordFn>(bof3::CloneOriginal("Pad_Read", bof3::addr::Pad_Read, 0x155)));
    BOF3_INJECT(DInput_Init);
    BOF3_INJECT(Pad_Read);
    BOF3_INJECT(DInput_Shutdown);
}
