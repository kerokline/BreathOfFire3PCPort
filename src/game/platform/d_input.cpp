// Moved here by tools/move_funcs.py from pad_read.cpp, rest_2h.cpp (docs/source-layout-round.md).
//
// The keyboard on SDL3 (docs/linux-round.md, wave 2, group R5; the owner's
// call of 2026-11-18): the 256 key bytes Key_State are filled from SDL's
// keyboard state through input/key_sdl.h's table, each byte as DirectInput
// filled it (0x80 down, 0 up, indexed by DIK number), and the game's own key
// table Key_Table is read over them as before. There is no launcher option.
//
// Which reader, decided once by DInput_Init:
//   * SDL's keyboard state, when the game's window is SDL's
//     (game_window::SdlOn) and our Pad_Read reads it - the default, and the
//     only reader off Windows.
//   * DirectInput 7's system keyboard, as the port had it (non-exclusive,
//     background), on Windows only: under BOF3X_OFF=WindowSdl (the Win32
//     window: SDL's video is not up, so SDL has no keyboard focus and no key
//     state), when Pad_Read is Capcom's (BOF3X_ORIGINAL=Pad_Read reads
//     DInput_Keyboard itself), and under BOF3X_OFF=KeyboardSdl - the proof
//     tool: with SDL's window up, each latch then also fills SDL's bytes and
//     logs every byte the two hold apart for two latches or more while SDL's
//     window has the keyboard (the shadow, below). Capcom's own DInput_Init
//     (BOF3X_ORIGINAL=DInput_Init) leaves the choice unmade, and Pad_Read
//     reads its DirectInput keyboard.
// Off Windows DirectInput's code is not compiled: KeyboardSdl off, Capcom's
// DInput_Init, Pad_Read or DInput_EnumJoystick are each a Fatal.
#include "game/platform/d_input.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstring>
#include <vector>

#ifdef _WIN32
#define DIRECTINPUT_VERSION 0x0700
#include <dinput.h>
#endif

#include "bof3/symbols.gen.h"
#include "common/image_base.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_frame.h"
#include "game/move_script_bytes.h"
#include "game/pad_read.h"
#include "game/platform/game_window.h"
#include "game/rdata_consts.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2h.h"
#include "game/rest_2h_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/bindings.h"
#include "input/key_sdl.h"
#include "input/pad_sdl.h"
#include "os/os.h"

namespace {

namespace at = rest_2h::at;

}  // namespace

namespace {

namespace input = bof3x::input;

constexpr unsigned kKeyTableEntries = 32;   // (u16 key, u16 bits) pairs; a key of 0 ends it

// The switch: 1 through PatchBytes("KeyboardSdl") in DInput_Inject; 0 when the
// code name is off (BOF3X_OFF=KeyboardSdl, or a hosted BOF3X_ORIGINAL).
std::uint8_t g_keyboard_sdl = 0;

enum class Reader : std::uint8_t {
    kUnset,         // DInput_Init not ours (BOF3X_ORIGINAL), or not yet run
    kSdl,           // SDL's keyboard state
    kDirectInput,   // DirectInput's keyboard (Windows only)
};
Reader g_reader = Reader::kUnset;

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

// SDL's keyboard state as the 256 bytes. The pump first, so that the bytes
// are the keyboard as of this latch, as DirectInput's GetDeviceState read it:
// the loop's own message step (game_window::Pump) comes after the latch, and
// state taken from it alone would be a frame late. With SDL's window, this
// is where the thread's window messages are dispatched during the latch, as
// the pad's poll dispatched them before (pad_sdl.cpp).
void ReadSdl(unsigned char* to) {
    SDL_PumpEvents();
    int count = 0;
    const bool* state = SDL_GetKeyboardState(&count);
    input::FillKeyState(state, count, to);
}

#ifdef _WIN32

constexpr HRESULT kInputLost = static_cast<HRESULT>(0x8007001E);   // DIERR_INPUTLOST

IDirectInputDeviceA* Keyboard() { return static_cast<IDirectInputDeviceA*>(DInput_Keyboard); }

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

// --- BOF3X_OFF=KeyboardSdl with SDL's window: the two fills side by side -------
// DirectInput reads the keyboard whatever window is in front; SDL's state
// holds keys only while SDL's window has the keyboard (and is emptied when it
// loses it). So the two are compared only while SDL has the focus, and a
// byte counts as apart when it stays apart for two latches: a key going down
// between SDL's pump and DirectInput's read is a one-latch difference.
struct Shadow {
    bool on = false;
    std::uint32_t compared = 0;      // latches compared, SDL's window in front
    std::uint32_t apart = 0;         // of those, latches with a byte apart
    std::uint32_t held = 0;          // differences held two latches or more
    std::uint32_t lines = 0;
    unsigned char run[256] = {};     // latches each byte has been apart
};
Shadow g_shadow;
constexpr std::uint32_t kShadowLines = 200;

void ShadowCompare(const unsigned char* sdl) {
    if (!SDL_GetKeyboardFocus()) {
        std::memset(g_shadow.run, 0, sizeof g_shadow.run);
        return;
    }
    ++g_shadow.compared;
    bool any = false;
    for (unsigned i = 0; i < 256; ++i) {
        const unsigned char di = Key_State[i] & 0x80;
        if (di == sdl[i]) {
            g_shadow.run[i] = 0;
            continue;
        }
        any = true;
        if (g_shadow.run[i] < 255) ++g_shadow.run[i];
        if (g_shadow.run[i] != 2) continue;
        ++g_shadow.held;
        if (++g_shadow.lines <= kShadowLines)
            bof3::Log("keyboard    shadow: DIK 0x%02X (%s) is %s to DirectInput and %s to SDL (Frame_Counter %u)", i,
                      input::KeyName(static_cast<int>(i)).c_str(), di ? "down" : "up", sdl[i] ? "down" : "up",
                      static_cast<std::uint32_t>(Frame_Counter));
    }
    if (any) ++g_shadow.apart;
}

void ShadowReport() {
    if (!g_shadow.on) return;
    bof3::Log("keyboard    shadow: %u latches compared with SDL's window in front, %u with a byte apart, %u differences "
              "held two latches or more%s",
              g_shadow.compared, g_shadow.apart, g_shadow.held, g_shadow.held > kShadowLines ? " (the first 200 logged)" : "");
}

#endif  // _WIN32

using input::Layout;

void PadLogLine(const char* line) { bof3::Log("%s", line); }

Layout LayoutFromEnv(Layout fallback) {
    char v[32] = {};
    const std::uint32_t n = bof3::os::GetEnv("BOF3X_PAD_LAYOUT", v, sizeof v);
    if (n == 0 || n >= sizeof v) return fallback;
    const int l = input::LayoutFromName(v);
    if (l < 0) {
        bof3::Log("pad: BOF3X_PAD_LAYOUT=%s not understood; %s", v, input::LayoutName(fallback));
        return fallback;
    }
    return static_cast<Layout>(l);
}

// BOF3X_PAD: `input=action,...,layout=NAME` (input/bindings.h). Unset, the
// defaults; set, only what it names.
void LoadPadMap() {
    input::Bindings defaults = input::Bindings::Defaults();
    pad_read::g_pad_map = defaults.pad;
    Layout layout = defaults.layout;
    char text[2048] = {};
    const std::uint32_t n = bof3::os::GetEnv("BOF3X_PAD", text, sizeof text);
    if (n > 0 && n < sizeof text) {
        std::vector<input::PadBinding> pad;
        const int understood = input::ParsePad(text, ',', pad, layout);
        pad_read::g_pad_map = pad;
        bof3::Log("pad: BOF3X_PAD - %d items understood, %u inputs bound, layout %s", understood,
                  static_cast<unsigned>(pad.size()), input::LayoutName(layout));
    } else if (n >= sizeof text) {
        bof3::Log("pad: BOF3X_PAD too long (%u); the default map", static_cast<std::uint32_t>(n));
    }
    pad_read::g_layout = LayoutFromEnv(layout);
}

unsigned PadWord() {
    if (!input::PadSdl_Started()) return 0;
    input::PadSdl_Poll();
    return input::PadSdl_Word(pad_read::g_pad_map);
}

// BOF3X_KEYS: `key=action,...` (input/bindings.h) written into the game's own
// table Key_Table, which Cfg_Load filled from BOF3.CFG or the default a
// moment before Game_Init - the same 32-entry (scancode, bits) shape, so
// KeyboardWord and the original's loop read it alike. Unset, that table
// stands as the original left it.
void LoadKeyTable() {
    char text[4096] = {};
    const std::uint32_t n = bof3::os::GetEnv("BOF3X_KEYS", text, sizeof text);
    if (n == 0) return;
    if (n >= sizeof text) {
        bof3::Log("pad: BOF3X_KEYS too long (%u); the game's own table", static_cast<std::uint32_t>(n));
        return;
    }
    std::vector<input::KeyBinding> keys;
    const int understood = input::ParseKeys(text, ',', keys);
    if (keys.size() > static_cast<size_t>(input::kKeyTableMax)) {
        bof3::Log("pad: BOF3X_KEYS names %u keys, the table holds %d; the first %d", static_cast<unsigned>(keys.size()),
                  input::kKeyTableMax, input::kKeyTableMax);
        keys.resize(input::kKeyTableMax);
    }
    std::memset(Key_Table, 0, Key_Table_count * sizeof Key_Table[0]);
    for (size_t i = 0; i < keys.size(); ++i) {
        Key_Table[i * 2] = keys[i].dik;
        Key_Table[i * 2 + 1] = keys[i].bits;
    }
    bof3::Log("pad: BOF3X_KEYS - %d items understood, %u keys in the table", understood,
              static_cast<unsigned>(keys.size()));
}

void StartSdl() {
    if (input::PadSdl_Started()) return;
    LoadPadMap();
    input::PadSdl_Start(pad_read::g_layout, &PadLogLine);
}

void StopSdl() { input::PadSdl_Stop(); }

#ifdef _WIN32
constexpr auto& AtRest2h = move_script::At;

// The port's DirectInput keyboard: DirectInputCreateA and the system keyboard
// opened exactly as the original opens it. False when DirectInput could not
// be had (the bytes then stay zero: no keyboard).
bool OpenDirectInput(void* hinstance, void* hwnd) {
    using CreateFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, LPDIRECTINPUTA*, LPUNKNOWN);
    HMODULE dinput = LoadLibraryA("dinput.dll");
    CreateFn create = dinput ? reinterpret_cast<CreateFn>(GetProcAddress(dinput, "DirectInputCreateA")) : nullptr;
    LPDIRECTINPUTA di = nullptr;
    const HRESULT created = create ? create(static_cast<HINSTANCE>(hinstance), DIRECTINPUT_VERSION, &di, nullptr)
                                   : static_cast<HRESULT>(E_FAIL);
    if (created != DI_OK || !di) {
        bof3::Log("pad: DirectInputCreateA failed (dinput.dll %p, entry %p, hinstance %p, hr %#x) - no keyboard through DirectInput",
                  static_cast<void*>(dinput), reinterpret_cast<void*>(create), hinstance, static_cast<std::uint32_t>(created));
        return false;
    }
    DInput_Object = di;
    LPDIRECTINPUTDEVICEA kb = nullptr;
    // The data format is the SDK's c_dfDIKeyboard, which rdata::Verify finds
    // equal to the one the original passes (0x5C4948: dwSize 0x18, dwObjSize
    // 0x10, DIDF_RELAXIS, 256 bytes, 256 objects, their table at 0x5ACBD0 in
    // .text - int3 in the exe host); the GUID (0x5C4828 = GUID_SysKeyboard)
    // is the engine's copy of the same 16 bytes (rdata_consts.h).
    constexpr rdata::Const kKeyboardGuid{0x5C4828};   // DInput_KeyboardGuid
    const GUID& guid = *reinterpret_cast<const GUID*>(static_cast<std::uintptr_t>(kKeyboardGuid));
    const DIDATAFORMAT* format = rdata::KeyboardFormat();
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
    return true;
}
#endif  // _WIN32

}  // namespace

// DInput_Init 0x5A94C0: the key bytes zeroed, the keyboard's reader chosen and
// opened (above), the key table from BOF3X_KEYS, and SDL's gamepad subsystem
// started. The original's joystick enumeration (0x5A94F5..0x5A95CB) is not
// run and its four globals stay null (DIV-0050); with SDL's keyboard,
// DirectInput is not created at all and DInput_Object and DInput_Keyboard
// stay null too.
// The arguments are (hinstance, hwnd): 0x5A94D2 pushes eax, then reads
// [esp+0x24] - the first argument - for DirectInputCreateA, and 0x5A950C
// reads the second for SetCooperativeLevel (symbols.toml had them reversed
// until 2026-09-24; the first live run passed the window as the instance,
// DIERR_INVALIDPARAM).
extern "C" void __cdecl DInput_Init(void* hinstance, void* hwnd) {
    std::memset(Key_State, 0, Key_State_count);
    const bool window_sdl = game_window::SdlOn();
    const bool pad_read_ours = bof3::IsEnabled(bof3::addr::Pad_Read);
    if (g_keyboard_sdl && window_sdl && pad_read_ours) {
        g_reader = Reader::kSdl;
        bof3::Log("keyboard    SDL3's keyboard state (KeyboardSdl on): Key_State by DIK number from SDL's scancodes");
        LoadKeyTable();
        StartSdl();
        return;
    }
#ifdef _WIN32
    g_reader = Reader::kDirectInput;
    bof3::Log("keyboard    DirectInput's system keyboard (%s)",
              !g_keyboard_sdl ? "KeyboardSdl off" : !window_sdl ? "the Win32 window: SDL has no keyboard focus" : "Pad_Read is Capcom's");
    if (!OpenDirectInput(hinstance, hwnd)) {
        StartSdl();
        return;
    }
    g_shadow.on = window_sdl && pad_read_ours;
    if (g_shadow.on)
        bof3::Log("keyboard    shadow: SDL's keyboard state is filled beside DirectInput's every latch and compared "
                  "(KeyboardSdl off with SDL's window)");
    LoadKeyTable();
    StartSdl();
#else
    (void)hinstance;
    (void)hwnd;
    bof3::Fatal("DInput_Init: DirectInput's keyboard was asked for (%s), and this system has no DirectInput; unset the "
                "switch (docs/linux-round.md, R5)",
                !g_keyboard_sdl ? "BOF3X_OFF=KeyboardSdl" : !window_sdl ? "the Win32 window" : "BOF3X_ORIGINAL=Pad_Read");
#endif
}

#ifdef _WIN32
// original 0x5A9620: the EnumDevices callback the original DInput_Init hands
// IDirectInput (stdcall (instance, context)). CreateDevice(DInput_Object,
// instance->guidInstance, &DInput_Joystick, 0): failed, answer 1 (go on); else
// QueryInterface(DInput_Joystick re-read, the IID 0x5C4718, &DInput_Joystick2),
// and when the instance's product name equals 0x66C7B0's case-blind (the C
// runtime's _stricmp), DInput_JoystickFound = 1; answer 0 (stop). Ours never
// enumerates (DIV-0050: DInput_Init is ours, pad_read.cpp), so only
// BOF3X_ORIGINAL=DInput_Init reaches it; faithful for that.
extern "C" int __stdcall DInput_EnumJoystick(const void* instance, void* context) {
    (void)context;
    auto* const di = static_cast<IDirectInputA*>(DInput_Object);
    const auto* const inst = static_cast<const unsigned char*>(instance);
    // The two objects land in native locals seeded from the slots and are
    // stored back (Slot32<void>, common/slot32.h): DirectInput writes a
    // native pointer, which the original's direct write into the 4-byte slot
    // cannot hold on x86-64. A slot reads as the original's once the call has
    // returned, whether the callee wrote its out parameter or not.
    auto joystick = static_cast<LPDIRECTINPUTDEVICEA>(DInput_Joystick.Native());
    const HRESULT created = di->CreateDevice(*reinterpret_cast<const GUID*>(inst + 4), &joystick, nullptr);
    DInput_Joystick = joystick;
    if (created != 0) return 1;
    auto* const joy = static_cast<IUnknown*>(DInput_Joystick);
    void* joystick2 = DInput_Joystick2;
    joy->QueryInterface(*reinterpret_cast<const IID*>(static_cast<std::uintptr_t>(rest_2h::at::kJoystickIid)), &joystick2);
    DInput_Joystick2 = joystick2;
    const int differs = SH_CALL(Crt_stricmp)(
        reinterpret_cast<const char*>(inst + 0x12C), reinterpret_cast<const char*>(AtRest2h(::bof3::Img(rest_2h::at::kProductName.v))));
    if (differs == 0) DInput_JoystickFound = 1;
    return 0;
}
#else
// Only Capcom's DInput_Init enumerates joysticks, through DirectInput, which
// this system has not.
extern "C" int __stdcall DInput_EnumJoystick(const void*, void*) {
    bof3::Fatal("DInput_EnumJoystick: DirectInput's joystick enumeration was called, and this system has no DirectInput "
                "(docs/linux-round.md, R5)");
}
#endif

// DInput_Shutdown 0x5A9690: the same releases, each slot zeroed; the joystick
// slots are null unless the original DInput_Init ran (BOF3X_ORIGINAL), in
// which case they are released the same way; with SDL's keyboard every slot
// is null. Then SDL is stopped.
extern "C" void __cdecl DInput_Shutdown(void) {
#ifdef _WIN32
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
    ShadowReport();
#endif
    StopSdl();
}

// Pad_Read 0x5A9700: the keyboard's bytes from the chosen reader, the table
// over them as the original, the pad from SDL. The high word is 0, as the
// original's always was (pad 2 never had a device).
extern "C" unsigned int __cdecl Pad_Read(void) {
    if (g_reader == Reader::kSdl) {
        ReadSdl(Key_State);
    } else {
#ifdef _WIN32
        if (g_shadow.on) {
            unsigned char sdl[256];
            ReadSdl(sdl);
            ReadKeyboard();
            ShadowCompare(sdl);
        } else {
            ReadKeyboard();
        }
#else
        bof3::Fatal("Pad_Read: no keyboard reader was chosen (Capcom's DInput_Init ran?), and this system has only SDL's "
                    "(docs/linux-round.md, R5)");
#endif
    }
    return (KeyboardWord() | PadWord()) & 0xFFFFu;
}

void DInput_Inject() {
    {
        // The keyboard on SDL3 (docs/linux-round.md, wave 2, R5).
        // BOF3X_OFF=KeyboardSdl keeps DirectInput's keyboard, on Windows, as
        // the proof tool: it compares the two fills while SDL's window is up.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("KeyboardSdl", Low32(&g_keyboard_sdl), &was, &is, 1);
#ifndef _WIN32
        if (!g_keyboard_sdl)
            bof3::Fatal("BOF3X_OFF=KeyboardSdl: DirectInput's keyboard is Windows only; unset the switch (docs/linux-round.md, R5)");
#endif
        bof3::Log("keyboard    SDL3's keyboard: %s", g_keyboard_sdl ? "on" : "OFF (DirectInput's, the proof tool)");
    }
    BOF3_INJECT(DInput_Init);
    BOF3_INJECT(DInput_EnumJoystick);
    BOF3_INJECT(DInput_Shutdown);
    BOF3_INJECT(Pad_Read);
}
