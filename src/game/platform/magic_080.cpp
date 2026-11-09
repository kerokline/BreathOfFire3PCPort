// Moved here by tools/move_funcs.py from area_backdrop.cpp, effect_5a.cpp, gfx_frame.cpp, magic_c1.cpp, shell.cpp, win_main.cpp (docs/source-layout-round.md).
#include "game/platform/magic_080.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <windows.h>

#include "audio/mp3.h"
#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/area_backdrop_callees.h"
#include "game/display_setup.h"
#include "game/effect_5a_callees.h"
#include "game/game_clock.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/msgbox.h"
#include "game/pause_text.h"
#include "game/platform/magic_080_callees.h"
#include "game/quick_menu.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/scenario_harness.h"
#include "game/shell.h"
#include "game/shell_callees.h"
#include "game/widescreen.h"
#include "game/win_main.h"
#include "hook/cpu_context.h"
#include "hook/detour.h"
#include "hook/exe_host.h"
#include "hook/exe_icon.h"
#include "hook/exe_image.h"
#include "hook/host.h"
#include "hook/input_script.h"
#include "hook/log.h"
#include "hook/run_speed.h"
#include "hook/snapshot.h"
#include "hook/stallwatch.h"
#include "input/bindings.h"
#include "render/render_d3d11.h"
#include "render/render_shim.h"

namespace {

namespace at = magic_080::at;

}  // namespace

using namespace shell;

namespace {

using area_backdrop::g;

void SetDword(unsigned char* p, std::uint32_t v) { std::memcpy(p, &v, sizeof v); }

void SetFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

using U = std::uint32_t;

unsigned char* S() { return Sprite_Current; }

using magic_harness::Mem;

using move_script::Word;

unsigned char* Sc() { return Sprite_Current; }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

// This group's functions and the other units' called by address, as the
// originals call them: in the game Capcom's code or the jmp Inject put there,
// in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

}  // namespace

#define C1_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace shell {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

unsigned long* Longs(U address) { return reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(address)); }

U Addr(const volatile void* p) { return Low32(p); }

U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

}  // namespace

}  // namespace shell

namespace {

// Strings in the image (.data, read 2026-09-23).
constexpr ::bof3::image::Va kStrSaveOk{0x65DA54};       // "Save OK"

constexpr ::bof3::image::Va kStrFrameRate{0x65DA5C};    // "Frame Rate = %d"

constexpr ::bof3::image::Va kStrLogosAvi{0x65DA6C};     // "LOGOS.AVI"

constexpr ::bof3::image::Va kStrClass{0x65DA84};        // "Bof3"

constexpr ::bof3::image::Va kStrIcon{0x65DA8C};         // "BOF3_ICON"

// DIVERGENCE DIV-0039: the window's title and the missing-disc box in English,
// in place of the GBK 0x65DA78 龙战士Ⅲ ("Breath of Fire III"), 0x65DA98
// 请插入龙战士Ⅲ光盘！ ("insert the disc!") and 0x65DAB0 错误 ("error").
// The originals are GBK bytes handed to the ANSI calls, so outside a Chinese
// locale Windows shows them as mojibake (the title as "ÁúÕ½Ê¿¢ó"). This is
// Windows' chrome, not the game's text, so it does not follow BOF3X_LANG.
constexpr const char* kTitle = "Breath of Fire III";

constexpr const char* kInsertDisc = "Please insert the Breath of Fire III disc.";

constexpr const char* kError = "Breath of Fire III";

constexpr ::bof3::image::Va kStrCapcomAvi{0x65DAB8};    // "CAPCOM.AVI"

constexpr ::bof3::image::Va kStrBof3Exe{0x65DAC4};      // "BOF3.EXE"

const char* StrWinMain(U address) { return reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address)); }

// The pacing constants (.rdata): the first deadline's offset, the frame
// period, and the wrap the float deadline is held under.
constexpr double kFirstFrameMs = 33.34;        // double at 0x5C4220

// DIV-0047: the period is the PlayStation's NTSC frame, 1001 / 30 ms
// (29.970 a second), not the port's 33.334 (29.999); and the deadline is
// kept as a base plus a count of frames times the period, in a double -
// never in the float at 0x6BC628 (Frame_Deadline), whose spacing past
// 2^24 ms made the pace depend on the clock's size (known-defects D5;
// DIV-0022 keeps the clock small, this makes the size not matter). The
// float at 0x5C4214, 2^32, was the original's tick wrap; the clock is
// unwrapped into 64 bits below instead. BOF3X_FRAME_MS=n (tooling, 1..1000)
// sets another period: 33.334 is the port's own.
constexpr double kFrameMs = 1001.0 / 30.0;     // the original's: double 33.334 at 0x5C4218

double g_frame_ms = kFrameMs;

// DIV-0048: F1 toggles double speed - the period halved, so two logic
// frames run per frame of wall time. Logic counts frames and reads no
// clock, so what the game computes is the same; when drawing cannot keep
// up the loop skips presents as it does catching up after a stall. Read by
// the loop, which rebases the deadline when it changes. BOF3X_SPEED=n
// (tooling, 1..64) starts a run at that speed - for scripted runs nobody
// watches (input_run.py --speed); F1 then toggles between 1 and 2 as ever.
int g_speed = 1;

// True while the speed is the one BOF3X_SPEED started the run at: only then
// does the loop hold x1 for a playing stream (run_speed.h - tooling, for runs
// nobody hears). F1 clears it, so a player's x2 is never held (round
// thirteen's review, item 1).
bool g_speed_scripted = false;

bool g_fps_log = false;   // BOF3X_FPS_LOG=1 (tooling): drawn and logic frames a second, to the log

constexpr const char* kStrSpeed2 = "Speed x2";

constexpr const char* kStrSpeed1 = "Speed x1";

constexpr const char* kStrFrameSaved = "Frame saved";

constexpr const char* kStrFrameNotSaved = "Frame not saved";

constexpr ::bof3::image::Va kEnvBase{0x903880};   // the two display-environment pairs

constexpr U kEnvStride = 0x90;

constexpr int kOverlayFrames = 0x78;

// DIV-0034: how far behind the clock the deadline may fall before it restarts.
constexpr double kMaxDebtMs = 500.0;

// DIV-0032: the two styles. The original's windowed style was 0xCA0000
// (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX), not resizable.
constexpr DWORD kWindowedStyle = WS_OVERLAPPEDWINDOW;

constexpr DWORD kBorderlessStyle = WS_POPUP;

RECT g_windowed_rect = {};    // the last windowed placement, for F8 back from borderless

bool g_have_windowed_rect = false;

// GetTickCount through the import slot, as the original reads it: DIV-0022's
// clock lives in that slot.
DWORD Tick() { return reinterpret_cast<DWORD(WINAPI*)(void)>(Imp_GetTickCount.Native())(); }

// The original's windowed placement: a 640 x 480 client, the frame added by
// AdjustWindowRect, centred on the desktop by (desktop - outer) / 2 - the
// cdq / sub / sar sequence at 0x4FCCB1, a signed division toward zero, which
// C's integer division also is. DIV-0036: the client is the render target's
// size, 320k x 240k (426k x 240k wide, DIV-0041) - the target's own k once the display is up, the window
// size setting before - with k lowered while the frame would not fit the
// work area. At k = 2 that is the original's 640 x 480.
// DIV-0042: the last windowed placement, kept in bof3x.window beside this
// dll ("left top right bottom" of the window rectangle), so that a resized
// window comes back at its size. Used when its frame still fits the work
// area and its top-left is on a monitor; else the computed placement.
std::wstring PlacementPath() {
    HMODULE self = nullptr;
    wchar_t path[MAX_PATH];
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&PlacementPath), &self) ||
        GetModuleFileNameW(self, path, MAX_PATH) == 0)
        return L"";
    std::wstring p = path;
    const size_t slash = p.find_last_of(L"\\/");
    return (slash == std::wstring::npos ? p : p.substr(0, slash)) + L"\\bof3x.window";
}

bool LoadPlacement(RECT& r) {
    const std::wstring path = PlacementPath();
    if (path.empty()) return false;
    FILE* f = _wfopen(path.c_str(), L"r");
    if (!f) return false;
    long v[4];
    const bool ok = fscanf(f, "%ld %ld %ld %ld", &v[0], &v[1], &v[2], &v[3]) == 4;
    fclose(f);
    if (!ok || v[2] - v[0] < 64 || v[3] - v[1] < 64) return false;
    r = {v[0], v[1], v[2], v[3]};
    if (!MonitorFromPoint({r.left + 8, r.top + 8}, MONITOR_DEFAULTTONULL)) return false;
    return true;
}

void SavePlacement(const RECT& r) {
    const std::wstring path = PlacementPath();
    if (path.empty()) return;
    FILE* f = _wfopen(path.c_str(), L"w");
    if (!f) return;
    fprintf(f, "%ld %ld %ld %ld\n", r.left, r.top, r.right, r.bottom);
    fclose(f);
}

RECT WindowedRect() {
    RECT saved;
    if (LoadPlacement(saved)) return saved;
    unsigned k = DisplaySetup_TargetScale();
    if (k == 0) k = DisplaySetup_WindowedScale();
    RECT work = {0, 0, Desktop_Width, Desktop_Height};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    RECT rc;
    for (;; --k) {
        rc = {0, 0, static_cast<LONG>(DisplaySetup_ViewWidth() * k), static_cast<LONG>(240 * k)};   // 426k wide under DIV-0041
        AdjustWindowRect(&rc, kWindowedStyle, FALSE);
        if (k <= 2 || (rc.right - rc.left <= work.right - work.left && rc.bottom - rc.top <= work.bottom - work.top)) break;
    }
    const int w = rc.right - rc.left, h = rc.bottom - rc.top;
    const int x = (Desktop_Width - w) / 2, y = (Desktop_Height - h) / 2;
    return {x, y, x + w, y + h};
}

// DIV-0032: the borderless window covers the monitor the window is on, or
// the primary one before there is a window (the original's fullscreen window
// was the desktop's size at 0, 0, which on one monitor is the same thing).
RECT BorderlessRect(HWND hwnd) {
    HMONITOR mon = hwnd ? MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) : MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info = {};
    info.cbSize = sizeof info;
    if (mon && GetMonitorInfoA(mon, &info)) return info.rcMonitor;
    return {0, 0, Desktop_Width, Desktop_Height};
}

// DIV-0032: F8's half - the window's style and placement follow
// Cfg_Fullscreen; the display objects stay.
void ApplyMode(HWND hwnd) {
    RECT r;
    if (Cfg_Fullscreen) {
        if (!(GetWindowLongA(hwnd, GWL_STYLE) & WS_POPUP)) {
            GetWindowRect(hwnd, &g_windowed_rect);
            g_have_windowed_rect = true;
        }
        r = BorderlessRect(hwnd);
        SetWindowLongA(hwnd, GWL_STYLE, static_cast<LONG>(kBorderlessStyle | WS_VISIBLE));
    } else {
        r = g_have_windowed_rect ? g_windowed_rect : WindowedRect();
        SetWindowLongA(hwnd, GWL_STYLE, static_cast<LONG>(kWindowedStyle | WS_VISIBLE));
    }
    SetWindowPos(hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOZORDER);
    bof3::Log("DIV-0032    window %s: %ld,%ld %ld x %ld", Cfg_Fullscreen ? "borderless" : "windowed", r.left, r.top,
              r.right - r.left, r.bottom - r.top);
}

// DIV-0032: the overlays. Display_TextOut needs the back buffer's GDI
// device context; the backend's surface has none, so with the backend's
// back buffer in the slot the text is logged, and with Capcom's
// (BOF3X_ORIGINAL=Display_Setup) it is drawn as it was.
bool BackBufferIsOurs() {
    U n = 0;
    render::Surface* const* all = render::AllSurfaces(&n);
    for (U i = 0; i < n; ++i)
        if (static_cast<const void*>(all[i]) == DDraw_BackBuffer) return true;
    return false;
}

void Overlay(const char* text) {
    if (BackBufferIsOurs()) bof3::Log("overlay     %s", text);
    else Display_TextOut(0, 0, text);
}

unsigned g_pad_reset_held = 0;

void PadReset_Frame() {
    if ((Input_Held & 0x900) != 0x900) {
        g_pad_reset_held = 0;
        return;
    }
    if (g_pad_reset_held != 60) {
        ++g_pad_reset_held;
        return;
    }
    g_pad_reset_held = 0;
    Game_RestartFlag = 1;
    bof3::Log("DIV-0152    Start + Select held 60 frames: back to the title at Frame_Counter %lu",
              static_cast<unsigned long>(Frame_Counter));
}

}  // namespace

// F11: the last frame presented, written beside the DLL as
// bof3x-frame-<Frame_Counter>.bmp (render::SaveFrame).
void SaveFrameBesideDll() {
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(GetModuleHandleW(L"bof3x.dll"), path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return;
    const size_t room = MAX_PATH - static_cast<size_t>(slash + 1 - path);
    if (std::swprintf(slash + 1, room, L"bof3x-frame-%lu.bmp", static_cast<unsigned long>(Frame_Counter)) < 0) return;
    const bool ok = render::SaveFrame(path);
    bof3::Log("F11         frame %lu %s %s", static_cast<unsigned long>(Frame_Counter), ok ? "saved to" : "NOT saved:",
              bof3::Utf8Path(path).text);
    Overlay_Frames = kOverlayFrames;
    Overlay_Text = ok ? kStrFrameSaved : kStrFrameNotSaved;
}

// original 0x4FC330: the kind-2 task. A two-entry stack table by +1:
// Magic080_Start, Magic080_Run.
C1_EXPORT void __cdecl Magic080_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Magic080_Start, bof3::addr::Magic080_Run};
    StackCall(kPhases, 2, Sc()[1], "Magic080_Task");
}

// original 0x4FC360: the task at (Field_Kind2X, Field_Kind2Z) and the map's
// elevation there; +0xB Rand & 0x3F (0 picks the text); +9 0x30; +1 on.
C1_EXPORT void __cdecl Magic080_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(Field_Kind2X));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(Field_Kind2Z));
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    const long h = MH_CALL(AreaMap_Elevation)(ts->x, ts->z);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, static_cast<unsigned>(h));
    const std::uint32_t r = RandCall();
    Sc()[0xB] = static_cast<unsigned char>(r & 0x3F);
    Sc()[9] = 0x30;
    Inc(Sc()[1]);
}

// original 0x4FC3C0: the screen point, its x 0x24 to the left; the glyph row
// (+0xB not 0) or the text (0); +9 down, at 0 the target's flag 0x40, the
// done flag, the task freed.
C1_EXPORT void __cdecl Magic080_Run(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) - 0x24u);
    if (Sc()[0xB] != 0)
        Call0(bof3::addr::Magic080_DrawGlyphs);
    else
        Call0(bof3::addr::Magic080_DrawText);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Mem(::bof3::Img(at::kFlags))[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4FC420: six characters, one from the head of each string the
// six pointers at 0x66A3C8 name (its first byte & 0x7F, its second: the
// character's code at +0x16), each a code-0x6C primitive 12 wide and 12 high
// from the screen point (+0x2E, +0x30) on, grey 0x80, CLUT (0, 0x1E0),
// committed to slot 2.
C1_EXPORT void __cdecl Magic080_DrawText(void) {
    for (unsigned i = 0; i < 6; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetCode6C)(p);
        const unsigned x = i * 12;
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x0, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + x);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y0, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x1, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + x + 12);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y1, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x2, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + x);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y2, Word(Sc() + 0x30) + 12u);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x3, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + x + 12);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y3, Word(Sc() + 0x30) + 12u);
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
        const unsigned char* const str = Magic080_TextPointers[i];
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->glyph, ((str[0] & 0x7Fu) << 8) + str[1]);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E0));
        p[0xC] = 0;
        p[0xD] = 0;
        p[0x14] = 12;
        p[0x15] = 0;
        p[0x1C] = 0;
        p[0x1D] = 12;
        p[0x24] = 12;
        p[0x25] = 12;
        MH_CALL(Gfx_CommitPrim)(2, 0x28);
    }
}

// original 0x4FC540: seven glyphs, textured quads 12 wide and 12 high from
// the screen point on, grey 0x80, page (0x3C0, 0) with blend 1, CLUT (0,
// 0x1E0), each cell's u / v the byte pair at 0x65DA28 + 4 i / + 2; slot 2.
C1_EXPORT void __cdecl Magic080_DrawGlyphs(void) {
    for (unsigned i = 0; i < 7; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        const int x0 = static_cast<int>(i * 12), x1 = x0 + 12;
        PutFloat(p + 8, S16(Sc() + 0x2E) + x0);
        PutFloat(p + 0xC, S16(Sc() + 0x30));
        PutFloat(p + 0x18, S16(Sc() + 0x2E) + x1);
        PutFloat(p + 0x1C, S16(Sc() + 0x30));
        PutFloat(p + 0x28, S16(Sc() + 0x2E) + x0);
        PutFloat(p + 0x2C, S16(Sc() + 0x30) + 12);
        PutFloat(p + 0x38, S16(Sc() + 0x2E) + x1);
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
        PutFloat(p + 0x3C, S16(Sc() + 0x30) + 12);
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0));
        rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E0));
        const unsigned char* const uv = Magic080_GlyphCells + 4 * i;
        p[0x14] = uv[0];
        p[0x15] = uv[2];
        p[0x24] = static_cast<unsigned char>(uv[0] + 12);
        p[0x25] = uv[2];
        p[0x34] = uv[0];
        p[0x35] = static_cast<unsigned char>(uv[2] + 12);
        p[0x44] = static_cast<unsigned char>(uv[0] + 12);
        p[0x45] = static_cast<unsigned char>(uv[2] + 12);
        MH_CALL(Gfx_CommitPrim)(2, 0x48);
    }
}

// original 0x4FC6A0; called once a pass of WinMain's message loop
// (InputScript_Latch's DeviceLatch, or Capcom's WinMain at 0x4FCDDE). Pad_Read's
// low word is pad 1: Input_Previous gets Input_Held as it stood after the
// read, Input_Held the new word, Input_Pressed the bits that went down. The
// high word - which Pad_Read never sets - the same for pad 2 at 0x7E1BF0.
extern "C" void __cdecl Input_Latch(void) {
    const U word = shell::g.pad_read();
    const std::uint16_t now1 = static_cast<std::uint16_t>(word);
    const std::uint16_t was1 = Input_Held;
    Input_Held = now1;
    Input_Previous = was1;
    Input_Pressed = static_cast<std::uint16_t>((was1 ^ now1) & now1);
    const std::uint16_t now2 = static_cast<std::uint16_t>(word >> 16);
    const std::uint16_t was2 = Input2_Held;
    Input2_Previous = was2;
    Input2_Held = now2;
    Input2_Pressed = static_cast<std::uint16_t>((was2 ^ now2) & now2);
}

extern "C" long __stdcall Game_WndProc(void* hwnd_, unsigned int msg, unsigned int wparam, long lparam) {
    HWND hwnd = static_cast<HWND>(hwnd_);
    switch (msg) {
    case WM_DESTROY:
        bof3::Log("WndProc: WM_DESTROY - quitting");
        // The loop leaves on Game_QuitFlag as well as on WM_QUIT. With SDL's
        // gamepad layer in the process (DIV-0050) the WM_QUIT that
        // PostQuitMessage sets can be taken by SDL's HIDAPI device discovery,
        // which pumps its message window from our thread on every update
        // (SDL_hidapi.c, PeekMessage / GetMessage on m_hwndMsg); the game then
        // ran on without a window (2026-09-24, closing at the title, 3 of 3).
        // The flag is what the original's Fmv_WndProc sets on WM_DESTROY too.
        Game_QuitFlag = 1;
        // DIV-0042: the windowed placement outlives the window.
        if (!Cfg_Fullscreen) {
            RECT r;
            if (GetWindowRect(hwnd, &r)) SavePlacement(r);
        } else if (g_have_windowed_rect) {
            SavePlacement(g_windowed_rect);
        }
        PostQuitMessage(0);
        return 0;
    case WM_MOVE:
        Display_WindowMoved(static_cast<int>(static_cast<unsigned long>(lparam) & 0xFFFF),
                            static_cast<int>(static_cast<unsigned long>(lparam) >> 16));
        return 0;
    case WM_SIZING: {
        // DIV-0042: a windowed drag keeps the picture's shape. With snap the
        // client lands on a whole multiple of the picture (the nearest to the
        // dragged height, 1..8); without, on the picture's aspect at the
        // dragged height (a side edge: at the dragged width).
        if (Cfg_Fullscreen) break;
        RECT* r = reinterpret_cast<RECT*>(lparam);
        RECT frame = {0, 0, 0, 0};
        AdjustWindowRect(&frame, kWindowedStyle, FALSE);
        const LONG fw = frame.right - frame.left, fh = frame.bottom - frame.top;
        const LONG view_w = static_cast<LONG>(DisplaySetup_ViewWidth());
        LONG cw = (r->right - r->left) - fw, ch = (r->bottom - r->top) - fh;
        const bool by_width = wparam == WMSZ_LEFT || wparam == WMSZ_RIGHT;
        if (DisplaySetup_Snap()) {
            LONG k = by_width ? (cw + view_w / 2) / view_w : (ch + 120) / 240;
            if (k < 1) k = 1;
            if (k > 8) k = 8;
            cw = view_w * k;
            ch = 240 * k;
        } else {
            if (by_width) ch = cw * 240 / view_w;
            else cw = ch * view_w / 240;
            if (cw < view_w) cw = view_w, ch = 240;
        }
        const LONG w = cw + fw, h = ch + fh;
        // Anchor the edge the user is not dragging.
        if (wparam == WMSZ_LEFT || wparam == WMSZ_TOPLEFT || wparam == WMSZ_BOTTOMLEFT) r->left = r->right - w;
        else r->right = r->left + w;
        if (wparam == WMSZ_TOP || wparam == WMSZ_TOPLEFT || wparam == WMSZ_TOPRIGHT) r->top = r->bottom - h;
        else r->bottom = r->top + h;
        return 1;
    }
    case WM_SIZE:
        // DIV-0042: the target follows the client, between frames.
        if (wparam != SIZE_MINIMIZED && DisplaySetup_TargetScale() != 0) {
            const unsigned cw = static_cast<unsigned long>(lparam) & 0xFFFF, ch = static_cast<unsigned long>(lparam) >> 16;
            if (cw && ch) render::RequestScale(DisplaySetup_ScaleForClient(cw, ch));
        }
        return 0;
    case WM_EXITSIZEMOVE:
        if (!Cfg_Fullscreen) {
            RECT r;
            if (GetWindowRect(hwnd, &r)) {
                g_windowed_rect = r;
                g_have_windowed_rect = true;
                SavePlacement(r);
            }
        }
        return 0;
    case WM_ACTIVATEAPP:
        if (wparam) {
            if (App_WasInactive) Sound_ResumeAll();
            SetFocus(hwnd);
            App_Active = 1;
            win_main::g_foreground = true;
        } else {
            win_main::g_foreground = false;
            if (win_main::g_background) {
                // DIV-0033: nothing stops. The pads read zero from here on
                // (WinMain_InputAllowed) until the next activation.
            } else {
                App_WasInactive = 1;
                App_Active = 0;
                Sound_PauseAll();
            }
        }
        return 0;
    case WM_KEYDOWN: {
        bof3::InputScript_RecordKey(static_cast<unsigned>(wparam), static_cast<long>(lparam));   // BOF3X_RECORD only
        // DIVERGENCE (the quicksave round's draft D, docs/quicksave.md, section QM): F5
        // opens the quick menu, pausing; while it is open every key is its own.
        if (bof3::QuickMenu_Key(static_cast<unsigned>(wparam), static_cast<long>(lparam))) return 0;
        const bool was_paused = Game_Paused != 0;
        if (was_paused && wparam != VK_F9) {
            Game_Paused = 0;
            Sound_ResumeAll();
        }
        if (wparam == VK_F9) {
            if (!was_paused) {
                Game_Paused = 1;
                Sound_PauseAll();
            } else {
                Game_Paused = 0;
                if (Title_LogoState == 0 && Title_Fade == 0) Game_RestartFlag = 1;
                else Game_QuitFlag = 1;
            }
            return 0;
        }
        if (wparam == VK_F8) {
            if (Desktop_Width > 640 && Desktop_Height > 480) {
                Display_Changed = 1;
                Cfg_Fullscreen ^= 1;
                ApplyMode(hwnd);   // DIV-0032: no teardown, no set-up
                Cursor_Sync();
            }
            return 0;
        }
        // DIVERGENCE DIV-0040: F7 (the next renderer device, re-made, and its
        // name on screen) and F11 (the frame-rate readout) do nothing - the
        // owner, 2026-09-23: not needed as keys. With the backend there is
        // one device, and neither readout could be drawn (DIV-0032). Like
        // any other key they still end a pause, above.
        // DIVERGENCE (the quicksave round's draft A, docs/quicksave.md 6): F6
        // quicksaves and F7 quick-loads - asked here, run by the loop between
        // two logic frames (Snapshot_Pending), never mid-frame. A held key's
        // repeats ask nothing more.
        // The reading row's study key (docs/reading-row.md 3.5; BOF3X_RUBY_KEY,
        // F4 by default): shows and hides the reading rows' glyphs. Ours; a
        // key Capcom left unbound.
        if (MsgBox_ReadingRowKey(static_cast<unsigned>(wparam), (lparam & (1 << 30)) != 0)) return 0;
        if (wparam == VK_F6) {
            if ((lparam & (1 << 30)) == 0) bof3::Snapshot_RequestTake();
            return 0;
        }
        if (wparam == VK_F7) {
            if ((lparam & (1 << 30)) == 0) bof3::Snapshot_RequestLoad(0x10);
            return 0;
        }
        if (wparam == VK_F12) {
            Save_QuickWrite();
            Overlay_Frames = kOverlayFrames;
            Overlay_Text = StrWinMain(::bof3::Img(kStrSaveOk.v));
            return 0;
        }
        // F11: the frame to a file (render::SaveFrame). Tooling, not a
        // divergence: DIV-0040 made the key nothing, and nothing of the
        // game's changes.
        if (wparam == VK_F11) {
            if ((lparam & (1 << 30)) == 0) SaveFrameBesideDll();
            return 0;
        }
        // DIV-0048: F1 toggles double speed; a held key's repeats (bit 30 of
        // lparam, the previous state) do not toggle it back.
        if (wparam == VK_F1) {
            if ((lparam & (1 << 30)) == 0) {
                g_speed = g_speed == 1 ? 2 : 1;
                g_speed_scripted = false;
                Overlay_Frames = kOverlayFrames;
                Overlay_Text = g_speed == 2 ? kStrSpeed2 : kStrSpeed1;
                bof3::Log("DIV-0048    speed x%d (F1) at Frame_Counter %lu", g_speed, static_cast<unsigned long>(Frame_Counter));
            }
            return 0;
        }
        return 0;
    }
    case WM_SYSKEYDOWN:
        if (wparam == VK_F10 || wparam == VK_MENU) return 1;
        break;
    case WM_SYSCOMMAND: {
        const unsigned int command = wparam & 0xFFF0;
        if (command == SC_SCREENSAVE || command == SC_MONITORPOWER) return 1;
        break;
    }
    case WM_POWERBROADCAST:
        if (wparam == 0) return static_cast<long>(BROADCAST_QUERY_DENY);   // PBT_APMQUERYSUSPEND
        break;
    default:
        break;
    }
    return static_cast<long>(DefWindowProcA(hwnd, msg, wparam, lparam));
}

extern "C" void __cdecl Cursor_Sync(void) {
    if (Cfg_Fullscreen) {
        if (Cursor_Hidden == 1) {
            ShowCursor(FALSE);
            Cursor_Hidden = 0;
        }
    } else if (Cursor_Hidden == 0) {
        ShowCursor(TRUE);
        Cursor_Hidden = 1;
    }
}

extern "C" int __stdcall Game_WinMain(void* hinstance_, void* /*hprev*/, char* /*cmdline*/, int /*show*/) {
    HINSTANCE hinstance = static_cast<HINSTANCE>(hinstance_);
    mp3dec::CompareAtStart();  // DIV-0091: BOF3X_MP3_COMPARE - the runtime's heap exists from here
    win_main::g_loop_ours = true;
    {
        char text[32];
        if (GetEnvironmentVariableA("BOF3X_BACKGROUND", text, sizeof text) > 0 && text[0] == '0') win_main::g_background = false;
        const DWORD n = GetEnvironmentVariableA("BOF3X_FRAME_MS", text, sizeof text);
        if (n > 0) {
            char* end = nullptr;
            const double v = n < sizeof text ? std::strtod(text, &end) : 0.0;
            if (end == nullptr || *end != '\0' || !(v >= 1.0 && v <= 1000.0)) bof3::Fatal("BOF3X_FRAME_MS must be 1..1000");
            g_frame_ms = v;
        }
        if (GetEnvironmentVariableA("BOF3X_FPS_LOG", text, sizeof text) > 0 && text[0] == '1') g_fps_log = true;
        const DWORD sn = GetEnvironmentVariableA("BOF3X_SPEED", text, sizeof text);
        if (sn > 0) {
            char* end = nullptr;
            const long v = sn < sizeof text ? std::strtol(text, &end, 10) : 0;
            if (end == nullptr || *end != '\0' || v < 1 || v > 64) bof3::Fatal("BOF3X_SPEED must be 1..64");
            g_speed = static_cast<int>(v);
            g_speed_scripted = g_speed != 1;
            if (g_speed != 1) bof3::Log("DIV-0048    speed x%d from the start (BOF3X_SPEED)", g_speed);
        }
    }

    if (!bof3::orig::Disc_Probe(StrWinMain(::bof3::Img(kStrCapcomAvi.v)), StrWinMain(::bof3::Img(kStrBof3Exe.v)))) {
        // DIVERGENCE DIV-0100: the probe's local test is CAPCOM.AVI in the
        // game directory; a disc-only install (.data from a disc-built cache,
        // docs/disc-only-install.md) has the movie only when the importer
        // could convert it. Such an install is local whatever the probe says:
        // the root left empty, as the probe's local branch leaves it. With a
        // BOF3.exe beside the game the install is the PC's and the original
        // probe decides (DIV-0100's "Reversible?"; review item 2).
        if (!bof3::ExeImage_DiscMapped() || bof3::ExeFile_Present()) {
            MessageBoxA(nullptr, kInsertDisc, kError, MB_ICONHAND);   // DIV-0039
            return 1;
        }
        File_CdRoot()[0] = 0;
        bof3::Log("DIV-0100    no CAPCOM.AVI and no PC disc: a disc-only install (BOF3X_EXEIMAGE's disc-built image) "
                  "starts as a local install, the CD root empty");
    }
    Game_HInstance = hinstance;

    // The window procedure, the FMV player and the set-up are reached through
    // Capcom's addresses, as the original reached them, so that each one's
    // BOF3X_ORIGINAL switch holds on its own under our WinMain: the address
    // is ours when injected and Capcom's when not.
    WNDCLASSA wc = {};
    wc.style = 0;
    wc.lpfnWndProc = reinterpret_cast<WNDPROC>(static_cast<std::uintptr_t>(::bof3::Code(bof3::addr::Game_WndProc)));
    wc.hInstance = hinstance;
    wc.hIcon = LoadIconA(hinstance, StrWinMain(::bof3::Img(kStrIcon.v)));
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = StrWinMain(::bof3::Img(kStrClass.v));
    if (!RegisterClassA(&wc)) return -1;

    bof3::orig::Cfg_Load();
    {
        HDC dc = GetDC(nullptr);
        Desktop_Width = GetDeviceCaps(dc, HORZRES);
        Desktop_Height = GetDeviceCaps(dc, VERTRES);
        Desktop_Bpp = GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES);
        ReleaseDC(nullptr, dc);
    }
    if (Desktop_Width == 640 && Desktop_Height == 480) Cfg_Fullscreen = 1;

    // DIV-0032: the window in its mode's style and placement.
    RECT r = Cfg_Fullscreen ? BorderlessRect(nullptr) : WindowedRect();
    const DWORD style = Cfg_Fullscreen ? kBorderlessStyle : kWindowedStyle;
    HWND hwnd = CreateWindowExA(0, StrWinMain(::bof3::Img(kStrClass.v)), kTitle, style, r.left, r.top, r.right - r.left,
                                r.bottom - r.top, nullptr, nullptr, hinstance, nullptr);
    Game_Hwnd = hwnd;
    bof3::Log("DIV-0032    window %s: %ld,%ld %ld x %ld%s", Cfg_Fullscreen ? "borderless" : "windowed", r.left, r.top,
              r.right - r.left, r.bottom - r.top, win_main::g_background ? "; DIV-0033 runs unfocused" : "");
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    Cursor_Sync();

    bof3::orig::Fmv_Play(StrWinMain(::bof3::Img(kStrCapcomAvi.v)), hwnd, Cfg_Fullscreen);
    if (!win_main::g_capcom_movie_only) bof3::orig::Fmv_Play(StrWinMain(::bof3::Img(kStrLogosAvi.v)), hwnd, Cfg_Fullscreen);
    bof3::Log("WinMain: FMVs done, quit flag %d", Game_QuitFlag);

    // A quit during the FMVs returns at once (0x4FCD3A): nothing is set up,
    // so nothing is torn down.
    if (Game_QuitFlag) return 1;
    {
        const int error = bof3::orig::Display_Setup(hwnd, &Cfg_Fullscreen, &Cfg_RenderMode, nullptr);
        if (error) {
            bof3::Log("WinMain: Display_Setup returned %d - the error box, then exit", error);
            Display_Teardown(hwnd);
            Display_ErrorBox(error);
            return 1;
        }
        Cursor_Sync();
        bof3::orig::Game_Init();   // ends in Task_SetStackBase: the task stacks hang from this frame
        bof3::Log("WinMain: display up, Game_Init done, entering the loop");
        bof3::StallWatch_Start(&Frame_Counter);   // BOF3X_STALLWATCH (stallwatch.h)

        // The loop. Small locals on purpose (see the header comment).
        char fps[0x50] = {};
        int frames_drawn = 0;
        DWORD last_fps_tick = 0;
        unsigned long logic_at_fps_tick = 0;
        // BOF3X_FPS_LOG: time inside the draw branch (present included) and
        // inside the logic, QueryPerformanceCounter ticks, per second.
        LARGE_INTEGER qpf = {}, q0 = {}, q1 = {}, q2 = {};
        QueryPerformanceFrequency(&qpf);
        long long draw_ticks = 0, logic_ticks = 0;
        MSG msg;
        bool quit = false;
        // DIV-0047: the deadline as base + frames * period against the
        // high-resolution clock (GameClock_NowMs: the tick slot steps 15.6 ms
        // at a time, too coarse for a period under that). The tick is still
        // read for the once-a-second frame-rate text, as the original did.
        DWORD last_tick = Tick();
        const auto Now = [&]() {
            last_tick = Tick();
            return GameClock_NowMs();
        };
        double deadline_base = 0.0;
        std::uint64_t deadline_frames = 0;
        int speed = 1;   // the period in force: g_frame_ms / speed (DIV-0048)
        unsigned stream_hold_frames = 0;   // BOF3X_SPEED: frames held at speed 1 for a playing stream
        while (!quit) {
            Task_Create(0, reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(bof3::addr::Boot_Task))));
            deadline_base = Now() + kFirstFrameMs;
            deadline_frames = 0;
            for (;;) {
                bof3::StallWatch_Beat();
                if (Game_QuitFlag) {
                    quit = true;
                    break;
                }
                if (!Game_Paused) bof3::InputScript_Latch();
                else bof3::InputScript_PausedLatch();   // a recipe keeps its frame count through a pause
                if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    if (msg.message == WM_QUIT) {
                        bof3::Log("WinMain: WM_QUIT");
                        quit = true;
                        break;
                    }
                    TranslateMessage(&msg);
                    DispatchMessageA(&msg);
                }
                if (!App_Active) continue;
                if (Game_RestartFlag) {
                    Game_RestartFlag = 0;
                    Task_SetStackBase();
                    break;   // Task_Create again
                }
                // While a stream plays the loop runs at the ordinary period (run_speed.h).
                // A hold of two minutes is no jingle - looping music has the
                // buffer the stream had - and is dropped.
                bool stream_hold = g_speed_scripted && bof3::RunSpeed_StreamPlaying();
                stream_hold_frames = stream_hold ? stream_hold_frames + 1 : 0;
                if (stream_hold_frames > 3600) {
                    bof3::Log("DIV-0048    BOF3X_SPEED: a stream still playing after 3600 frames at Frame_Counter %lu; held no longer",
                              static_cast<unsigned long>(Frame_Counter));
                    bof3::RunSpeed_StreamForget();
                    stream_hold = false;
                    stream_hold_frames = 0;
                }
                const int want_speed = stream_hold ? 1 : g_speed;
                if (want_speed != speed) {
                    // DIV-0048: the count so far at the old period becomes the
                    // base, so the change starts from the current deadline.
                    deadline_base += static_cast<double>(deadline_frames) * (g_frame_ms / speed);
                    deadline_frames = 0;
                    speed = want_speed;
                }
                double now = Now();
                double deadline = deadline_base + static_cast<double>(deadline_frames) * (g_frame_ms / speed);
                // DIV-0034: the debt clamp.
                if (now - deadline > kMaxDebtMs) {
                    bof3::Log("DIV-0034    frame deadline %.0f ms behind at Frame_Counter %lu: restarted",
                              now - deadline, static_cast<unsigned long>(Frame_Counter));
                    deadline_base = now + kFirstFrameMs;
                    deadline_frames = 0;
                    deadline = deadline_base;
                }
                // A late frame is not drawn - except one a scripted shot is
                // about to save, so a capture is the same frame at any speed.
                if (now < deadline || bof3::InputScript_ShotPending()) {
                    if (g_fps_log) QueryPerformanceCounter(&q0);
                    unsigned char* env = Gfx_CurrentEnv;
                    Gpu_PutDispEnv(env);
                    Gpu_PutDrawEnv(env + 0x14);
                    Gfx_FlushDirtyStrip();
                    Gfx_FlushUploadQueue();
                    Gfx_DrawOTag(reinterpret_cast<unsigned long*>(env + 0x8C));
                    if (Overlay_Frames != 0) {
                        Overlay(Overlay_Text);
                        Overlay_Frames -= 1;
                    } else if (Fps_Shown) {
                        Overlay(fps);
                    }
                    ++frames_drawn;
                    if (g_fps_log) {
                        QueryPerformanceCounter(&q1);
                        draw_ticks += q1.QuadPart - q0.QuadPart;
                    }
                    // DIV-0095: the exe host's icon, once the VRAM shadow holds
                    // its sheet; on this thread, which owns the window.
                    bof3::ExeIcon_Frame(hwnd);
                }
                // The wait: Sound_Tick on every spin, at least once a frame.
                for (;;) {
                    Sound_Tick();
                    now = Now();
                    if (!(now < deadline)) break;
                }
                if (last_tick - last_fps_tick > 1000) {
                    last_fps_tick = last_tick;
                    Crt_sprintf(fps, StrWinMain(::bof3::Img(kStrFrameRate.v)), frames_drawn);
                    if (g_fps_log) {
                        const double ms = 1000.0 / static_cast<double>(qpf.QuadPart);
                        const unsigned long logic = static_cast<unsigned long>(Frame_Counter - logic_at_fps_tick);
                        bof3::Log("fps         %d drawn, %lu logic, speed x%d; draw %.2f ms each, logic %.2f ms each", frames_drawn,
                                  logic, speed, frames_drawn ? static_cast<double>(draw_ticks) * ms / frames_drawn : 0.0,
                                  logic ? static_cast<double>(logic_ticks) * ms / logic : 0.0);
                        draw_ticks = logic_ticks = 0;
                    }
                    logic_at_fps_tick = Frame_Counter;
                    frames_drawn = 0;
                }
                // DIV-0047: one more frame of the period. The original added
                // 33.334 to the float at 0x6BC628 and held it under 2^32
                // (0x4FCF0F..0x4FCF3A); that float is not written here.
                ++deadline_frames;
                Gfx_BufferIndex ^= 1;
                unsigned char* env = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(::bof3::Img(kEnvBase.v) + Gfx_BufferIndex * kEnvStride));
                Gfx_CurrentEnv = env;
                Gpu_ClearOTagR(reinterpret_cast<unsigned long*>(env + 0x70), 8);
                Gfx_BeginFrame();
                SpriteCell_Reset();
                if (g_fps_log) QueryPerformanceCounter(&q2);
                if (!Game_Paused) {
                    // Through the original's entry, not straight to ours: the
                    // call tracer counts a frame as an arrival at 0x5A98A0,
                    // whose first five bytes jmp to our Task_RunAll (or, under
                    // BOF3X_ORIGINAL, are Capcom's). docs/task_sched.md section 5.
                    bof3::orig::Task_RunAll();
                    if (win_main::g_pad_reset) PadReset_Frame();
                } else if (bof3::QuickMenu_Frame()) {
                    // F5's quick menu (draft D) in place of the pause lines.
                } else {
                    // two 4-byte slots each (Slot32, common/slot32.h): the stride is the image's on every width
                    const ::bof3::Slot32<const unsigned char>* lines = (Title_LogoState == 0 && Title_Fade == 0) ? Pause_LinesGame : Pause_LinesTitle;
                    // DIV-0038: the English lines are centred on their width
                    Text_DrawAt(PauseText_X(lines[0], 100), 100, 0, 100, lines[0]);
                    Text_DrawAt(PauseText_X(lines[1], 0x70), 0x80, 0, 100, lines[1]);
                }
                bof3::orig::Gfx_LinkOTags();
                if (g_fps_log) {
                    QueryPerformanceCounter(&q1);
                    logic_ticks += q1.QuadPart - q2.QuadPart;
                }
                Frame_Counter += 1;
                // The quicksave point (docs/quicksave.md 2): every task is
                // suspended on its slice and this frame's logic and lists are
                // done; the next iteration draws what the span then holds.
                bof3::Snapshot_Pending();
                bof3::QuickMenu_AfterPoint();   // F5's menu: a load it asked ran, or stays open refused
            }
        }
    }
    bof3::Log("WinMain: loop left, tearing down");
    Display_Teardown(hwnd);
    bof3::Log("WinMain: display down");
    Sound_Shutdown();
    bof3::Log("WinMain: sound down");
    DInput_Shutdown();
    bof3::Log("WinMain: input down, returning");
    return 1;
}

// original 0x4FD030; sole caller WinMain (0x4FCBFE; ours since) before the window.
// The original's frame - 0x3C bytes, edi and esi saved under it - laid out
// here so that what Cfg_SetKeyTable copies past the frame is what it was:
// the return address and the caller's bytes above it. A pair by width
// (docs/x64-asm-policy.md 8 unit 7), not C++: the shell fuzz compares the
// frame and everything above it, which a compiler-placed local cannot
// reproduce. The x86_64 half lays a 0x3C-byte frame above the 32 bytes of
// home space the callee may write (rsp+0x20..+0x5C of a 0x68-byte frame,
// which keeps rsp at 0 mod 16 for the call); the bytes the copy runs past
// are then this frame's pad, the return address and the caller's, as on
// i686 - and with DIV-0078's own table on, as it is after Shell_Inject, the
// copy never leaves the table.
extern "C" __attribute__((naked)) void __cdecl Cfg_Load(void) {
#if defined(__i386__)
    asm("subl $0x3C, %esp\n\t"
        "pushl %edi\n\t"
        "pushl %esi\n\t"
        "leal 8(%esp), %eax\n\t"
        "pushl %eax\n\t"
        "call " BOF3_SYM(Shell_CfgLoadFrame) "\n\t"
        "addl $4, %esp\n\t"
        "popl %esi\n\t"
        "popl %edi\n\t"
        "addl $0x3C, %esp\n\t"
        "ret");
#elif defined(__x86_64__)
    asm("subq $0x68, %rsp\n\t"
        "leaq 0x20(%rsp), %rcx\n\t"
        "call " BOF3_SYM(Shell_CfgLoadFrame) "\n\t"
        "addq $0x68, %rsp\n\t"
        "ret");
#else
#error "Cfg_Load: no frame for this architecture"
#endif
}

// original 0x4FD110; sole caller WinMain after Display_Setup. The GTE's
// geometry, the pad devices, the sound set-up, the two double-buffer blocks
// (block 0 draws at y 0 and shows y 240, block 1 the other way round), buffer
// 0 current, the frame begun, the quit flag down - and then a jmp, not a call,
// to Task_SetStackBase, so that the task stacks hang from the stack pointer
// this was entered with: the body runs as a call, and the jump is made from
// here - a C++ tail call since the 64-bit build round (docs/x64-asm-policy.md
// 8 unit 7), which on i686 is the same `call; jmp *%eax` as the asm it
// replaces (codegen_same.py: 0 differ) and on x86-64 the same shape for
// rsp; musttail refuses to compile where the jump would not be one.
extern "C" void __cdecl Game_Init(void) {
    using Tail = void (__cdecl*)();
    const Tail tail = reinterpret_cast<Tail>(Shell_GameInitBody());
    [[clang::musttail]] return tail();
}

// original 0x4FD200; Game_Init's, once for each block. The block's ordering
// table cleared (ClearOTagR, eight slots at +0x70), then its DRAWENV's isbg
// byte (+0x2C) 1 and clear colour (+0x2D..+0x2F) black. The eax the original
// leaves (Gpu_ClearOTagR's, al 0) is not read by its caller.
extern "C" void __cdecl Gfx_InitBufferBlock(unsigned char* block) {
    shell::g.clear_otag_r(reinterpret_cast<unsigned long*>(block + kBlockOt), kOtSlots);
    block[0x2C] = 1;
    block[0x2D] = 0;
    block[0x2E] = 0;
    block[0x2F] = 0;
}

// original 0x4FD230. Called by WinMain once per logic frame, just before the
// tasks run: aims the packet pool, the ordering-table pointers and the frame
// nodes at this frame's half of the double buffer, and rewinds the unpack
// scratch.
extern "C" void __cdecl Gfx_BeginFrame(void) {
    // DIV-0004. The upload queue is drained only in WinMain's rendered-frame
    // branch, and nothing bounds it. On a rendered pass it is empty by now. On
    // an unrendered one - the catch-up after the window was unfocused or its
    // title bar held - the original leaves it to grow, and the flush that
    // eventually comes unpacks past the end of the scratch buffer into the
    // draw structures. So an unrendered frame drains it here instead: same
    // uploads, same order, before the next frame's logic as always, and
    // before the rewind below so the unpackers see the pointer they expect.
    if (Gfx_UploadQueueCount != 0) {
        BOF3_LOG_FIRST_CALL("DIV-0004: %u uploads left by an unrendered frame, draining",
                            Gfx_UploadQueueCount);
        Gfx_FlushUploadQueue();
    }

    const std::uint32_t b = Gfx_BufferIndex;

    Gfx_PacketNext = Gfx_PacketPools + (b << 16);

    unsigned long* head = Gfx_OtHeads + ((b << 5) >> 2);
    for (unsigned k = 0; k < Gfx_OtPointers_count; ++k) Gfx_OtPointers[k] = head + k;

    // The walk starts 8 bytes into the block per buffer, not one record: the
    // two buffers' records interleave. As the original has it.
    auto* node = reinterpret_cast<std::uint32_t*>(
        reinterpret_cast<std::uint8_t*>(&Gfx_FrameNodes) + b * 8);
    for (int n = 0x38; n != 0; --n) {
        node[0] = 0;
        node[1] = Low32(node);
        node = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(node) + 0x30);
    }

    Gfx_UnpackNext = Gfx_UnpackScratch;
}

// original 0x4FD290; WinMain once a frame, after the frame's logic. For slot
// i of 8: Gpu_AddPrim(the current block's slot i, the list head Gfx_OtHeads[
// Gfx_BufferIndex * 8 + i], Gfx_OtPointers[i]) - the head spliced onto the
// table, the table's old link stored where the frame's packets of that slot
// end. Gfx_OtPointers[i], the buffer index (a byte) and Gfx_CurrentEnv are all
// read afresh each pass, after the call before.
extern "C" void __cdecl Gfx_LinkOTags(void) {
    for (U i = 0; i < kOtSlots; ++i) {
        const U link = Long(::bof3::Img(kGfxOtPointers.v) + 4 * i);
        const U buffer = Gfx_BufferIndex;
        const U block = Addr(Gfx_CurrentEnv);
        shell::g.add_prim(Longs(block + kBlockOt + 4 * i), Longs(::bof3::Img(Gfx_OtHeads_addr) + (i + buffer * 8) * 4), Longs(link));
    }
}

// original 0x4FD2E0 (hidden in Gfx_LinkOTags' recorded extent): Cond_ByteFE 1:
// the step word +0x2E 0 and the gradient (0x40, 0x5A, 0xFF); 2: the step up
// one below 0xBF (signed), the gradient (c + 0x40, 0x5A, max(0xFF - 2c, 0))
// and a tail jump to the glow; else nothing.
extern "C" void __cdecl EffectKind18_01_Sunset(void) {
    const unsigned fe = Cond_ByteFE;
    if (fe == 1) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0);
        SH_CALL(Gfx_DrawSkyGradient)(0x40, 0x5A, 0xFF);
        return;
    }
    if (fe != 2) return;
    unsigned char* s = S();
    const std::int32_t step = static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e));
    if (step < 0xBF) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, static_cast<U>(step + 1));
        s = S();
    }
    const std::int32_t c = static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e));
    std::int32_t b = 0xFF - 2 * c;
    if (b <= 0) b = 0;
    SH_CALL(Gfx_DrawSkyGradient)(static_cast<unsigned>(c + 0x40), 0x5A, static_cast<unsigned>(b));
    SH_CALL(Gfx_DrawSunsetGlow)();
}

// original 0x4FD350 (read 2026-09-30): the screen-wide gradient of a colour
// over black - Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, 0x95, 0) committed (7,
// 0xC), then a POLY_G4 at the new cursor: (0, 0)..(319, 239) as floats, the
// three argument bytes at the top corners, black at the bottom ones, the
// semi-transparency left as Gpu_SetPolyG4 set it; committed (7, 0x44). Its
// caller (EffectKind18_01_Sunset 0x4FD2E0, effect_5a.cpp) steps the colour from (0x40, 0x5A, 0xFF) toward
// (0x40 + 0x17E, ...) as a word at 0x937F88 + 0x2E climbs to 0xBF: the sunset
// of area 23's cutscene. DIV-0041: under a wide picture the corners are
// (-53, 0)..(372, 239) - the same rule as AreaMap_DrawBackdrop above, and
// the fuzz (before Widescreen_Inject) compares the original's.
extern "C" void __cdecl Gfx_DrawSkyGradient(unsigned r, unsigned g_, unsigned b) {
    area_backdrop::g.draw_mode(Gfx_PacketNext, 0, 1, 0x95, 0);
    area_backdrop::g.commit(7, 0xC);
    unsigned char* const quad = Gfx_PacketNext;
    area_backdrop::g.set_poly_g4(quad);
    const float wide = static_cast<float>(Widescreen_Live());
    const float left = 0.0f - wide;   // not -wide: -0.0f when wide is 0
    const float right = 319.0f + wide;
    SetFloat(quad + 0x2C, 239.0f);
    SetFloat(quad + 0x3C, 239.0f);
    quad[0x14] = quad[4] = static_cast<unsigned char>(r);
    quad[0x15] = quad[5] = static_cast<unsigned char>(g_);
    SetFloat(quad + 0x8, left);
    SetDword(quad + 0xC, 0);
    SetFloat(quad + 0x18, right);
    SetDword(quad + 0x1C, 0);
    SetFloat(quad + 0x28, left);
    SetFloat(quad + 0x38, right);
    quad[0x16] = quad[6] = static_cast<unsigned char>(b);
    quad[0x34] = quad[0x24] = 0;
    quad[0x35] = quad[0x25] = 0;
    quad[0x36] = quad[0x26] = 0;
    area_backdrop::g.commit(7, 0x44);
}

// original 0x4FD3E0 (read 2026-09-30): the sunset's glow over the sky above -
// Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, 0xB5, 0) committed (3, 0xC), then a
// POLY_F4 at the new cursor, semi-transparent (Gpu_SetSemiTrans 1), (0, 0)..
// (320, 240) as floats, red = Sprite_Current's word +0x2E / 8 (a signed
// division: the step the caller of Gfx_DrawSkyGradient climbs to 0xBF),
// green and blue 0; committed (3, 0x38). Caller EffectKind18_01_Sunset 0x4FD2E0.
// DIV-0041: (-53, 0)..(373, 240) under a wide picture, the fuzz (before
// Widescreen_Inject) comparing the original's.
extern "C" void __cdecl Gfx_DrawSunsetGlow(void) {
    area_backdrop::g.draw_mode(Gfx_PacketNext, 0, 0, 0xB5, 0);
    area_backdrop::g.commit(3, 0xC);
    unsigned char* const quad = Gfx_PacketNext;
    area_backdrop::g.set_poly_f4(quad);
    area_backdrop::g.set_semi(quad, 1);
    const float wide = static_cast<float>(Widescreen_Live());
    const float left = 0.0f - wide;   // not -wide: -0.0f when wide is 0
    const float right = 320.0f + wide;
    SetFloat(quad + 0x8, left);
    SetDword(quad + 0xC, 0);
    SetFloat(quad + 0x14, right);
    SetDword(quad + 0x18, 0);
    SetFloat(quad + 0x20, left);
    SetFloat(quad + 0x24, 240.0f);
    SetFloat(quad + 0x2C, right);
    SetFloat(quad + 0x30, 240.0f);
    std::int16_t step;
    std::memcpy(&step, Sprite_Current + 0x2E, sizeof step);
    quad[5] = 0;
    quad[6] = 0;
    quad[4] = static_cast<unsigned char>(static_cast<int>(step) / 8);   // cdq / and 7 / sar 3: toward zero
    area_backdrop::g.commit(3, 0x38);
}

void Magic080_Inject() {
    BOF3_INJECT(Magic080_Task);
    BOF3_INJECT(Magic080_Start);
    BOF3_INJECT(Magic080_Run);
    BOF3_INJECT(Magic080_DrawText);
    BOF3_INJECT(Magic080_DrawGlyphs);
    BOF3_INJECT(Input_Latch);
    BOF3_INJECT(Game_WndProc);
    BOF3_INJECT(Cursor_Sync);
    BOF3_INJECT(Game_WinMain);
    BOF3_INJECT(Cfg_Load);
    BOF3_INJECT(Game_Init);
    BOF3_INJECT(Gfx_InitBufferBlock);
    BOF3_INJECT(Gfx_BeginFrame);
    BOF3_INJECT(Gfx_LinkOTags);
    BOF3_INJECT(EffectKind18_01_Sunset);
    BOF3_INJECT(Gfx_DrawSkyGradient);
    BOF3_INJECT(Gfx_DrawSunsetGlow);
}
