// Moved here by tools/move_funcs.py from pad_read.cpp, rest_2h.cpp (docs/source-layout-round.md).
#include "game/platform/d_input.h"

#include <cstdint>
#include <cstring>
#include <dinput.h>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_frame.h"
#include "game/move_script_bytes.h"
#include "game/pad_read.h"
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
#include "input/pad_sdl.h"
#include "os/os.h"

namespace {

namespace at = rest_2h::at;

}  // namespace

#define DIRECTINPUT_VERSION 0x0700

namespace {

namespace input = bof3x::input;

constexpr unsigned kKeyTableEntries = 32;   // (u16 key, u16 bits) pairs; a key of 0 ends it

constexpr HRESULT kInputLost = static_cast<HRESULT>(0x8007001E);   // DIERR_INPUTLOST

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

using input::Layout;

void PadLogLine(const char* line) { bof3::Log("%s", line); }

Layout LayoutFromEnv(Layout fallback) {
    char v[32] = {};
    const DWORD n = bof3::os::GetEnv("BOF3X_PAD_LAYOUT", v, sizeof v);
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
    const DWORD n = bof3::os::GetEnv("BOF3X_PAD", text, sizeof text);
    if (n > 0 && n < sizeof text) {
        std::vector<input::PadBinding> pad;
        const int understood = input::ParsePad(text, ',', pad, layout);
        pad_read::g_pad_map = pad;
        bof3::Log("pad: BOF3X_PAD - %d items understood, %u inputs bound, layout %s", understood,
                  static_cast<unsigned>(pad.size()), input::LayoutName(layout));
    } else if (n >= sizeof text) {
        bof3::Log("pad: BOF3X_PAD too long (%lu); the default map", static_cast<unsigned long>(n));
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
    const DWORD n = bof3::os::GetEnv("BOF3X_KEYS", text, sizeof text);
    if (n == 0) return;
    if (n >= sizeof text) {
        bof3::Log("pad: BOF3X_KEYS too long (%lu); the game's own table", static_cast<unsigned long>(n));
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

constexpr auto& AtRest2h = move_script::At;

}  // namespace

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
    LoadKeyTable();
    StartSdl();
}

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

// Pad_Read 0x5A9700: the keyboard as the original, the pad from SDL. The
// high word is 0, as the original's always was (pad 2 never had a device).
extern "C" unsigned int __cdecl Pad_Read(void) {
    ReadKeyboard();
    return (KeyboardWord() | PadWord()) & 0xFFFFu;
}

void DInput_Inject() {
    BOF3_INJECT(DInput_Init);
    BOF3_INJECT(DInput_EnumJoystick);
    BOF3_INJECT(DInput_Shutdown);
    BOF3_INJECT(Pad_Read);
}
