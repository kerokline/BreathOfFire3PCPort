// launcher/ui/ui.h drawn with Dear ImGui on SDL3's renderer
// (docs/launcher-portable.md, stage P1; the owner's call 1, 2026-11-19). The
// only file of the launcher that includes Dear ImGui: the pages name ui.h's
// widgets. SDL3 is the window, the renderer, the pad, the PNG reader, the
// message box and the file pickers; nothing here names an operating system.
#include "launcher/ui/ui.h"

#include "launcher/ui/assets.h"

#include "input/bindings.h"
#include "input/pad_sdl.h"

#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace bof3x::ui {

struct Picture {
    SDL_Texture* texture = nullptr;
};

namespace {

// A font's line and ascent as parts of its em, from its `hhea` and `head`
// tables. Dear ImGui sizes a font by its line (ascent to descent); the Win32
// launcher's sizes are ems (CreateFontW's negative height), and the pages keep
// those numbers.
struct FaceMetrics {
    float line_per_em = 1.0f, ascent_per_em = 0.8f;
};

struct FaceFont {
    ImFont* font = nullptr;
    FaceMetrics metrics;
};

// The widget that comes next: where, how large, grey or not (Place, Size, Grey).
struct Next {
    bool at = false;
    float x = 0, y = 0, w = 0, h = 0;
    bool grey = false;
    std::string reason;
    bool red = false;
};

struct PickAsk {
    std::string kind, pattern;
    SDL_DialogFileFilter filter{};
};

struct State {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool imgui = false, hidden = false, closed = false;
    float scale = 1.0f;
    FaceFont faces[2];
    std::vector<unsigned char> banner_file;   // --banner-font's bytes: Dear ImGui reads them for as long as it lives
    std::vector<Picture*> pictures;
    Next next;
    int wake = 0;             // frames still to draw without waiting
    // A check's widget (TestSet): the name, the value, and whether a grey widget of that name was drawn.
    bool test_set = false, test_grey = false;
    std::string test_widget, test_value;
    bool test_pad = false;    // a check pressed a pad's key (TestKey)
    bool busy_before = false; // an open list or an edit field held the keys in the frame before
    std::string save_path, save_why;
    int save_state = 0;       // 0 none asked, 1 asked, 2 written, 3 failed
    std::mutex pick_lock;
    std::map<int, PickAsk> pick_asks;
    std::map<int, std::string> pick_done;
    // The bind capture (CaptureBegin): the key taken from the events, the pad
    // inputs held when it began, and a check's key and pad input (TestCaptureKey).
    bool capturing = false;
    int cap_key = 0;   // an SDL scancode; 0 none
    bool cap_held[static_cast<int>(input::PadInput::kCount)] = {};
    int test_cap_key = 0, test_cap_pad = -1;
    // The widgets drawn in a check's frame, with their text red or not (TestDrawn).
    bool cur_red = false;
    std::map<std::string, bool> drawn_now, drawn_last;
    // A check's question and pickers (TestAnswer, TestPick): never shown.
    int test_answer = -1;      // the answer given and not yet taken: 1 yes, 0 no
    std::string test_asked;    // the question waiting for it
    int test_pick_ask = -1;    // the picker asked last and not answered
    // A log's length in the frame before, by its name, and the frames still to scroll it to its end (LogText).
    std::map<std::string, std::pair<std::size_t, int>> logs;
};
State g;
std::atomic<bool> g_events{false};   // SDL's events are up: WakeFrame may push one

std::uint32_t Be32(const unsigned char* p) {
    return static_cast<std::uint32_t>(p[0]) << 24 | static_cast<std::uint32_t>(p[1]) << 16 |
           static_cast<std::uint32_t>(p[2]) << 8 | p[3];
}
int Be16(const unsigned char* p) { return static_cast<std::int16_t>(p[0] << 8 | p[1]); }

bool ReadMetrics(const unsigned char* d, std::size_t size, FaceMetrics* out) {
    if (size < 12) return false;
    const int tables = Be16(d + 4);
    const unsigned char *hhea = nullptr, *head = nullptr;
    for (int i = 0; i < tables; ++i) {
        const std::size_t at = 12 + 16 * static_cast<std::size_t>(i);
        if (at + 16 > size) return false;
        const std::uint32_t offset = Be32(d + at + 8), length = Be32(d + at + 12);
        if (offset > size || length > size - offset) return false;
        if (!std::memcmp(d + at, "hhea", 4) && length >= 8) hhea = d + offset;
        if (!std::memcmp(d + at, "head", 4) && length >= 20) head = d + offset;
    }
    if (!hhea || !head) return false;
    const float ascent = static_cast<float>(Be16(hhea + 4)), descent = static_cast<float>(Be16(hhea + 6));
    const float em = static_cast<float>(static_cast<std::uint16_t>(Be16(head + 18)));
    if (em <= 0 || ascent - descent <= 0) return false;
    out->line_per_em = (ascent - descent) / em;
    out->ascent_per_em = ascent / em;
    return true;
}

ImFont* AddFont(const unsigned char* bytes, std::size_t size, FaceMetrics* metrics) {
    if (!ReadMetrics(bytes, size, metrics)) return nullptr;
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;   // ours: the built-in array, or State::banner_file
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(bytes), static_cast<int>(size), 0.0f,
                                                      &cfg);
}

// A font file for the banner in place of the built-in one, to try a font
// (OpenOptions::banner_font; main.cpp --banner-font).
bool ReadBannerFile(const char* path) {
    std::size_t size = 0;
    void* data = SDL_LoadFile(path, &size);
    if (!data) return false;
    g.banner_file.assign(static_cast<unsigned char*>(data), static_cast<unsigned char*>(data) + size);
    SDL_free(data);
    return true;
}

ImU32 Col(Color c) { return IM_COL32(c.r, c.g, c.b, c.a); }
ImVec4 Rgb(Color c, float a = 1.0f) { return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, a); }

// The look: dark, after the picture's black bands, with the dune's sand as
// the one accent. The first proposal; the owner tunes it off a capture
// (docs/launcher-portable.md, stage P1 "the look"). Each colour is its red,
// green and blue: no number here is an address (CLAUDE.md rule 3).
constexpr Color kPaper{20, 22, 26}, kInk{232, 228, 220}, kInkGrey{138, 135, 128}, kPanel{27, 30, 36},
    kField{36, 40, 48}, kFieldHot{48, 53, 63}, kFieldDown{59, 66, 80}, kButton{42, 46, 54}, kButtonHot{58, 63, 74},
    kButtonDown{84, 69, 44}, kEdge{76, 70, 58}, kSand{200, 160, 112}, kSandHot{221, 184, 136},
    kSandDeep{106, 86, 50}, kFocus{242, 214, 154}, kNone{0, 0, 0};
// A Controls cell that does two or more things (Red). The Win32 page's red is
// 192, 0, 0 on a light button; on this look's dark buttons a lighter red reads.
constexpr Color kAlarm{240, 96, 84};

void Look() {
    ImGuiStyle& st = ImGui::GetStyle();
    st = ImGuiStyle();
    st.WindowPadding = ImVec2(0, 0);
    st.WindowBorderSize = 0;
    st.WindowRounding = 0;
    st.ChildBorderSize = 0;
    st.FramePadding = ImVec2(8, 4);
    st.FrameRounding = 3;
    st.FrameBorderSize = 1;
    st.PopupRounding = 3;
    st.PopupBorderSize = 1;
    st.GrabRounding = 3;
    st.GrabMinSize = 12;
    st.ItemSpacing = ImVec2(8, 5);
    st.ItemInnerSpacing = ImVec2(6, 4);
    st.ScrollbarSize = 12;
    st.ScrollbarRounding = 3;
    st.DisabledAlpha = 0.45f;
    st.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    st.AntiAliasedLinesUseTex = false;
    ImVec4* c = st.Colors;
    c[ImGuiCol_Text] = Rgb(kInk);
    c[ImGuiCol_TextDisabled] = Rgb(kInkGrey);
    c[ImGuiCol_WindowBg] = Rgb(kNone, 0);
    c[ImGuiCol_ChildBg] = Rgb(kNone, 0);
    c[ImGuiCol_PopupBg] = Rgb(kPanel);
    c[ImGuiCol_Border] = Rgb(kEdge);
    c[ImGuiCol_BorderShadow] = Rgb(kNone, 0);
    c[ImGuiCol_FrameBg] = Rgb(kField);
    c[ImGuiCol_FrameBgHovered] = Rgb(kFieldHot);
    c[ImGuiCol_FrameBgActive] = Rgb(kFieldDown);
    c[ImGuiCol_Button] = Rgb(kButton);
    c[ImGuiCol_ButtonHovered] = Rgb(kButtonHot);
    c[ImGuiCol_ButtonActive] = Rgb(kButtonDown);
    c[ImGuiCol_CheckMark] = Rgb(kSand);
    c[ImGuiCol_SliderGrab] = Rgb(kSand);
    c[ImGuiCol_SliderGrabActive] = Rgb(kSandHot);
    c[ImGuiCol_Header] = Rgb(kSandDeep);
    c[ImGuiCol_HeaderHovered] = Rgb(kFieldDown);
    c[ImGuiCol_HeaderActive] = Rgb(kSandDeep);
    c[ImGuiCol_ScrollbarBg] = Rgb(kPanel);
    c[ImGuiCol_ScrollbarGrab] = Rgb(kEdge);
    c[ImGuiCol_ScrollbarGrabHovered] = Rgb(kSandDeep);
    c[ImGuiCol_ScrollbarGrabActive] = Rgb(kSand);
    c[ImGuiCol_PlotHistogram] = Rgb(kSand);
    c[ImGuiCol_NavCursor] = Rgb(kFocus);
    c[ImGuiCol_TextSelectedBg] = Rgb(kSandDeep);
    c[ImGuiCol_InputTextCursor] = Rgb(kInk);
    st.ScaleAllSizes(g.scale);
}

float S(float v) { return v * g.scale; }

// Place, Size and Grey for the widget that starts here; its end takes them back.
struct Placed {
    float w, h;
    bool grey;
    std::string reason;
    bool red;
    Placed() : w(g.next.w), h(g.next.h), grey(g.next.grey), reason(std::move(g.next.reason)), red(g.next.red) {
        if (g.next.at) ImGui::SetCursorPos(ImVec2(S(g.next.x), S(g.next.y)));
        g.next = Next{};
        g.cur_red = red;
        if (red) ImGui::PushStyleColor(ImGuiCol_Text, Rgb(kAlarm));
        if (grey) ImGui::BeginDisabled();
    }
    ~Placed() {
        if (red) ImGui::PopStyleColor();
        g.cur_red = false;
        if (!grey) return;
        ImGui::EndDisabled();
        if (!reason.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", reason.c_str());
    }
    void Width() const {
        if (w > 0) ImGui::SetNextItemWidth(S(w));
    }
    ImVec2 Box() const { return ImVec2(S(w), S(h)); }
};

// Whether `widget`, as a check writes it, names the widget `label`: its whole
// label, its part before ## or its part after.
bool Names(const std::string& widget, const char* label) {
    const char* cut = std::strstr(label, "##");
    const std::string whole = label, shown = cut ? std::string(label, cut) : whole, name = cut ? cut + 2 : "";
    return widget == whole || widget == shown || (!name.empty() && widget == name);
}

// TestSet's value for the widget `label`, once, when the widget can be used.
// In a check's frame it also notes the widget as drawn (TestDrawn).
bool TestTake(const char* label, bool grey, std::string* value) {
    if (g.hidden) g.drawn_now[label] = g.cur_red;   // a check's frame: what it drew (TestDrawn)
    if (!g.test_set) return false;
    if (!Names(g.test_widget, label)) return false;
    if (grey) {
        g.test_grey = true;
        return false;
    }
    *value = g.test_value;
    g.test_set = false;
    return true;
}

int TestNumber(const std::string& value) { return std::atoi(value.c_str()); }

// A check box and a radio button are lower than a button: the Win32 pages
// set their rows 11 dialog units apart (20 of the picture's pixels).
struct SmallBox {
    SmallBox() { ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(8), S(2))); }
    ~SmallBox() { ImGui::PopStyleVar(); }
};

void Handle(const SDL_Event& e) {
    // The bind capture takes a key going down before the widgets see it, and
    // the text it would type; a key's going up still reaches them, so a key
    // they saw go down is not left down for them.
    if (g.capturing && (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_TEXT_INPUT)) {
        if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && g.cap_key == 0 && e.key.scancode != SDL_SCANCODE_UNKNOWN)
            g.cap_key = static_cast<int>(e.key.scancode);
        return;
    }
    ImGui_ImplSDL3_ProcessEvent(&e);
    if (e.type == SDL_EVENT_QUIT) g.closed = true;
    if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && e.window.windowID == SDL_GetWindowID(g.window)) g.closed = true;
}

// A frame is owed without waiting while something moves by itself: a held
// key or pad direction that repeats, an edit field's caret, a drag.
bool Moving() {
    if (ImGui::IsAnyItemActive() || ImGui::GetIO().WantTextInput) return true;
    for (ImGuiKey key : {ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadDpadLeft,
                         ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickUp, ImGuiKey_GamepadLStickDown,
                         ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight})
        if (ImGui::IsKeyDown(key)) return true;
    return false;
}

void Wake() {
    if (!g_events) return;   // before Open or after Close: no queue to push to
    SDL_Event e{};
    e.type = SDL_EVENT_USER;
    SDL_PushEvent(&e);   // SDL lets any thread push an event
}

void SDLCALL PickDone(void* user, const char* const* list, int) {
    const int ask = static_cast<int>(reinterpret_cast<std::intptr_t>(user));
    {
        std::lock_guard<std::mutex> lock(g.pick_lock);
        g.pick_done[ask] = list && list[0] ? list[0] : "";
    }
    Wake();
}

}  // namespace

// ---- The window ---------------------------------------------------------------

bool Open(const OpenOptions& options, std::string* why) {
    auto fail = [why](const char* what) {
        if (why) *why = std::string(what) + ": " + SDL_GetError();
        Close();
        return false;
    };
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return fail("SDL_Init");
    g_events = true;
    g.hidden = options.hidden;
    double scale = options.scale;
    if (scale <= 0) scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (scale <= 0) scale = 1;
    g.scale = static_cast<float>(scale);
    const int w = static_cast<int>(std::lround(options.width * scale)),
              h = static_cast<int>(std::lround(options.height * scale));
    g.window = SDL_CreateWindow(options.title, w, h, options.hidden ? SDL_WINDOW_HIDDEN : 0);
    if (!g.window) return fail("SDL_CreateWindow");
    g.renderer = SDL_CreateRenderer(g.window, nullptr);
    if (!g.renderer) return fail("SDL_CreateRenderer");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    g.imgui = true;
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // the launcher's one file is bof3x.ini
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    if (!ImGui_ImplSDL3_InitForSDLRenderer(g.window, g.renderer) || !ImGui_ImplSDLRenderer3_Init(g.renderer)) {
        if (why) *why = "the drawing backends did not start";
        Close();
        return false;
    }
    // Every pad moves the focus, not the first one alone.
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll);

    FaceFont& text = g.faces[static_cast<int>(Face::kText)];
    text.font = AddFont(kUiTextFont, kUiTextFontSize, &text.metrics);
    if (!text.font) {
        if (why) *why = "the built-in text font is not a font";
        Close();
        return false;
    }
    // The banner's lettering: TeX Gyre Pagella, built in (assets.h), the same
    // on both systems. The Win32 launcher draws it in Palatino Linotype, which
    // Pagella follows letter for letter (docs/launcher-portable.md section 9).
    FaceFont& banner = g.faces[static_cast<int>(Face::kBanner)];
    if (options.banner_font && ReadBannerFile(options.banner_font))
        banner.font = AddFont(g.banner_file.data(), g.banner_file.size(), &banner.metrics);
    if (!banner.font) banner.font = AddFont(kUiBannerFont, kUiBannerFontSize, &banner.metrics);
    if (!banner.font) banner = text;
    io.FontDefault = text.font;

    Look();
    // The pages' text: 12 of the picture's pixels to the em, the size of the
    // Win32 launcher's tabs and title buttons (config_dialog.cpp Layout).
    ImGui::GetStyle().FontSizeBase = 12.0f * text.metrics.line_per_em;
    ImGui::GetStyle().FontScaleDpi = g.scale;
    return true;
}

void Close() {
    for (Picture* p : g.pictures) {
        if (p->texture) SDL_DestroyTexture(p->texture);
        delete p;
    }
    g.pictures.clear();
    if (g.imgui) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        g.imgui = false;
    }
    if (g.renderer) SDL_DestroyRenderer(g.renderer);
    if (g.window) SDL_DestroyWindow(g.window);
    g.renderer = nullptr;
    g.window = nullptr;
    g_events = false;
    SDL_Quit();
}

double Scale() { return g.scale; }

Color Paper() { return kPaper; }
Color Ink() { return kInk; }

const char* RendererName() {
    const char* name = g.renderer ? SDL_GetRendererName(g.renderer) : nullptr;
    return name ? name : "none";
}

bool FrameBegin() {
    SDL_Event e;
    bool any = false;
    if (!g.hidden && g.wake <= 0 && !Moving()) {
        // Nothing moves: wait for the player. The timeout is for what comes
        // with no event, a pad plugged in between two polls.
        // The capture reads the pad each 16 ms, as the Win32 capture's timer.
        if (SDL_WaitEventTimeout(&e, g.capturing ? 16 : 500)) {
            Handle(e);
            any = true;
        }
    }
    while (SDL_PollEvent(&e)) {
        Handle(e);
        any = true;
    }
    // Dear ImGui settles a change over a few frames (a list that opens, a
    // focus that moves): draw them before the next wait.
    if (any) g.wake = 4;
    else if (g.wake > 0) --g.wake;
    if (g.closed) return false;
    g.drawn_last.swap(g.drawn_now);
    g.drawn_now.clear();

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    // A check's pad (TestKey): Dear ImGui drops a pad's keys in a frame whose
    // backend says no pad is there, and the backend says so each frame.
    if (g.test_pad) ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_HasGamepad;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("##window", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollWithMouse);
    g.next = Next{};
    return true;
}

void FrameEnd() {
    g.busy_before = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) || ImGui::GetIO().WantTextInput;
    ImGui::End();
    ImGui::Render();
    SDL_SetRenderDrawColor(g.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g.renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), g.renderer);
    if (g.save_state == 1) {
        SDL_Surface* frame = SDL_RenderReadPixels(g.renderer, nullptr);
        const bool ok = frame && SDL_SavePNG(frame, g.save_path.c_str());
        if (!ok) g.save_why = SDL_GetError();
        if (frame) SDL_DestroySurface(frame);
        g.save_state = ok ? 2 : 3;
    }
    SDL_RenderPresent(g.renderer);
}

void SaveFrame(const std::string& png_path) {
    g.save_path = png_path;
    g.save_state = 1;
}

bool FrameSaved(std::string* why) {
    if (g.save_state == 3 && why) *why = g.save_why;
    return g.save_state == 2;
}

bool Pressed(Press press) {
    switch (press) {
        case Press::kStart:
            return ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false);
        case Press::kBack:
            // An open list or an edit field takes Escape and the circle for itself, in the frame before.
            return !g.busy_before && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                                      ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false));
    }
    return false;
}

// ---- For checks ---------------------------------------------------------------

bool TestKey(const char* name, bool down) {
    struct Named {
        const char* name;
        ImGuiKey key;
    };
    static const Named kNamed[] = {
        {"up", ImGuiKey_UpArrow},           {"down", ImGuiKey_DownArrow},
        {"left", ImGuiKey_LeftArrow},       {"right", ImGuiKey_RightArrow},
        {"enter", ImGuiKey_Enter},          {"space", ImGuiKey_Space},
        {"escape", ImGuiKey_Escape},        {"tab", ImGuiKey_Tab},
        {"pad-up", ImGuiKey_GamepadDpadUp}, {"pad-down", ImGuiKey_GamepadDpadDown},
        {"pad-left", ImGuiKey_GamepadDpadLeft}, {"pad-right", ImGuiKey_GamepadDpadRight},
        {"pad-cross", ImGuiKey_GamepadFaceDown}, {"pad-circle", ImGuiKey_GamepadFaceRight},
        {"pad-start", ImGuiKey_GamepadStart},
    };
    for (const Named& n : kNamed) {
        if (std::strcmp(n.name, name)) continue;
        ImGuiIO& io = ImGui::GetIO();
        if (n.key >= ImGuiKey_GamepadStart) g.test_pad = true;
        g.wake = 8;
        io.AddKeyEvent(n.key, down);
        return true;
    }
    return false;
}

void TestSet(const char* widget, const char* value) {
    g.test_set = true;
    g.test_grey = false;
    g.test_widget = widget;
    g.test_value = value;
    g.wake = 8;   // the frames that draw the widget come without a wait
}

bool TestPending() { return g.test_set; }

void TestCaptureKey(int scancode) {
    g.test_cap_key = scancode;
    g.wake = 8;
}

void TestCapturePad(int input) {
    g.test_cap_pad = input;
    g.wake = 8;
}

bool TestDrawn(const char* widget, bool* red) {
    for (const auto& [label, was_red] : g.drawn_last) {
        if (!Names(widget, label.c_str())) continue;
        if (red) *red = was_red;
        return true;
    }
    if (red) *red = false;
    return false;
}

bool TestGrey() { return g.test_grey; }

void TestAnswer(bool yes) {
    g.test_answer = yes ? 1 : 0;
    g.wake = 8;
}

std::string TestAsked() { return g.test_asked; }

void TestPick(const std::string& path) {
    if (g.test_pick_ask < 0) return;
    {
        std::lock_guard<std::mutex> lock(g.pick_lock);
        g.pick_done[g.test_pick_ask] = path;
    }
    g.test_pick_ask = -1;
    g.wake = 8;
}

bool TestPicking() { return g.test_pick_ask >= 0; }

// ---- The tools ------------------------------------------------------------------

void WakeFrame() { Wake(); }

void Rest(int ms) {
    if (!g_events) return;
    if (SDL_WaitEventTimeout(nullptr, ms)) g.wake = 4;   // an event waits: it is read by the next FrameBegin
}

// ---- Drawing under the widgets ------------------------------------------------

Picture* PictureFromPng(const unsigned char* bytes, unsigned size) {
    SDL_IOStream* io = SDL_IOFromConstMem(bytes, size);
    SDL_Surface* surface = io ? SDL_LoadPNG_IO(io, true) : nullptr;
    if (!surface) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(g.renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture) return nullptr;
    const bool whole = g.scale == std::floor(g.scale);
    SDL_SetTextureScaleMode(texture, whole ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
    g.pictures.push_back(new Picture{texture});
    return g.pictures.back();
}

void PictureDraw(Picture* picture, float x, float y, float w, float h) {
    if (!picture) return;
    ImGui::GetBackgroundDrawList()->AddImage(
        ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<std::intptr_t>(picture->texture))), ImVec2(S(x), S(y)),
        ImVec2(S(x + w), S(y + h)));
}

void Fill(float x, float y, float w, float h, Color color) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(S(x), S(y)), ImVec2(S(x + w), S(y + h)), Col(color));
}

float TextWidth(Face face, float em, const char* text) {
    const FaceFont& f = g.faces[static_cast<int>(face)];
    return f.font->CalcTextSizeA(S(em * f.metrics.line_per_em), FLT_MAX, 0.0f, text).x / g.scale;
}

void Text(Face face, float em, float x, float baseline, Color color, const char* text) {
    const FaceFont& f = g.faces[static_cast<int>(face)];
    ImGui::GetBackgroundDrawList()->AddText(f.font, S(em * f.metrics.line_per_em),
                                            ImVec2(S(x), S(baseline - em * f.metrics.ascent_per_em)), Col(color), text);
}

// ---- Widgets ------------------------------------------------------------------

void Place(float x, float y) {
    g.next.at = true;
    g.next.x = x;
    g.next.y = y;
}

void Size(float w, float h) {
    g.next.w = w;
    g.next.h = h;
}

void Grey(const char* reason) {
    g.next.grey = true;
    g.next.reason = reason ? reason : "";
}

void Red() { g.next.red = true; }

void DefaultFocus() {
    ImGui::SetKeyboardFocusHere(-1);
    // Shown, as a Win32 dialog marks its default button: Dear ImGui lets Enter
    // and the pad's cross use the focused widget only while the focus is shown.
    ImGui::SetNavCursorVisible(true);
}

bool Button(const char* label) {
    Placed p;
    std::string test;
    const bool taken = TestTake(label, p.grey, &test);
    return ImGui::Button(label, p.Box()) || taken;
}

bool Tab(const char* label, bool on) {
    Placed p;
    if (on) {
        ImGui::PushStyleColor(ImGuiCol_Button, Rgb(kSand));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Rgb(kSandHot));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Rgb(kSandHot));
        ImGui::PushStyleColor(ImGuiCol_Text, Rgb(kPaper));
    }
    std::string test;
    const bool pressed = ImGui::Button(label, p.Box()) | TestTake(label, p.grey, &test);
    if (on) ImGui::PopStyleColor(4);
    return pressed && !on;
}

bool Check(const char* label, bool* on) {
    Placed p;
    std::string test;
    if (TestTake(label, p.grey, &test)) {
        const bool want = test != "0", changed = want != *on;
        *on = want;
        SmallBox small;
        ImGui::Checkbox(label, on);
        return changed;
    }
    SmallBox small;
    return ImGui::Checkbox(label, on);
}

bool Radio(const char* label, int* value, int mine) {
    Placed p;
    std::string test;
    if (TestTake(label, p.grey, &test)) {
        const bool changed = *value != mine;
        *value = mine;
        SmallBox small;
        ImGui::RadioButton(label, value, mine);
        return changed;
    }
    SmallBox small;
    return ImGui::RadioButton(label, value, mine);
}

bool ComboGreyed(const char* label, int* index, const std::vector<std::string>& items,
                 const std::vector<bool>& usable) {
    Placed p;
    p.Width();
    const int count = static_cast<int>(items.size());
    const auto can = [&](int i) { return static_cast<std::size_t>(i) >= usable.size() || usable[static_cast<std::size_t>(i)]; };
    bool changed = false;
    std::string test;
    if (TestTake(label, p.grey, &test)) {
        const int want = TestNumber(test);
        if (want >= 0 && want < count && can(want) && want != *index) {
            *index = want;
            changed = true;
        }
    }
    const char* shown = *index >= 0 && *index < count ? items[static_cast<std::size_t>(*index)].c_str() : "";
    ImGui::PushID(label);
    if (ImGui::BeginCombo("##combo", shown)) {
        for (int i = 0; i < count; ++i) {
            ImGui::PushID(i);
            const bool chosen = i == *index;
            if (ImGui::Selectable(items[static_cast<std::size_t>(i)].c_str(), chosen,
                                  can(i) ? 0 : ImGuiSelectableFlags_Disabled) &&
                !chosen) {
                *index = i;
                changed = true;
            }
            if (chosen) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    return changed;
}

bool Combo(const char* label, int* index, const std::vector<std::string>& items) {
    return ComboGreyed(label, index, items, std::vector<bool>());
}

bool Slider(const char* label, int* value, int low, int high) {
    Placed p;
    p.Width();
    bool changed = false;
    std::string test;
    if (TestTake(label, p.grey, &test)) {
        const int want = std::min(std::max(TestNumber(test), low), high);
        changed = want != *value;
        *value = want;
    }
    ImGui::PushID(label);
    changed |= ImGui::SliderInt("##slider", value, low, high, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopID();
    return changed;
}

bool SliderShowing(const char* label, int* value, int low, int high, const char* shown) {
    Placed p;
    p.Width();
    bool changed = false;
    std::string test;
    if (TestTake(label, p.grey, &test)) {
        const int want = std::min(std::max(TestNumber(test), low), high);
        changed = want != *value;
        *value = want;
    }
    // The text as Dear ImGui's format: a % in it is doubled, so it is shown as written.
    std::string format;
    for (const char* c = shown; c && *c; ++c) format += *c == '%' ? "%%" : std::string(1, *c);
    ImGui::PushID(label);
    changed |= ImGui::SliderInt("##slider", value, low, high, format.c_str(),
                                ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput);
    ImGui::PopID();
    return changed;
}

bool Edit(const char* label, std::string* text) {
    Placed p;
    p.Width();
    bool changed = false;
    std::string test;
    if (TestTake(label, p.grey, &test)) {
        changed = test != *text;
        *text = test;
    }
    ImGui::PushID(label);
    changed |= ImGui::InputText("##edit", text);
    ImGui::PopID();
    return changed;
}

bool List(const char* label, int* index, const std::vector<std::string>& items) {
    Placed p;
    bool changed = false;
    std::string test;
    if (TestTake(label, p.grey, &test)) {
        const int want = TestNumber(test);
        if (want >= 0 && want < static_cast<int>(items.size()) && want != *index) {
            *index = want;
            changed = true;
        }
    }
    ImGui::PushID(label);
    if (ImGui::BeginListBox("##list", p.Box())) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            ImGui::PushID(i);
            const bool chosen = i == *index;
            if (ImGui::Selectable(items[static_cast<std::size_t>(i)].c_str(), chosen) && !chosen) {
                *index = i;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    ImGui::PopID();
    return changed;
}

void Label(const char* text) {
    Placed p;
    if (p.w > 0) ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + S(p.w));
    ImGui::TextUnformatted(text);
    if (p.w > 0) ImGui::PopTextWrapPos();
}

void ReadText(const char* label, const std::string& text) {
    Placed p;
    ImGui::PushID(label);
    // Read only: Dear ImGui writes nothing through the pointer.
    ImGui::InputTextMultiline("##read", const_cast<char*>(text.c_str()), text.size() + 1, p.Box(),
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::PopID();
}

void LabelCut(const char* text, bool middle) {
    Placed p;
    const std::string whole = text ? text : "";
    const float room = p.w > 0 ? S(p.w) : FLT_MAX;
    std::string shown = whole;
    if (ImGui::CalcTextSize(whole.c_str()).x > room) {
        // The longest cut that fits, with "..." where the text was taken out. A
        // UTF-8 letter is never split: a cut moves past its continuation bytes.
        auto letter_start = [&](std::size_t i) { return i >= whole.size() || (static_cast<unsigned char>(whole[i]) & 0xC0) != 0x80; };
        if (!middle) {
            std::size_t keep = whole.size();
            while (keep > 0) {
                do --keep;
                while (keep > 0 && !letter_start(keep));
                shown = whole.substr(0, keep) + "...";
                if (ImGui::CalcTextSize(shown.c_str()).x <= room) break;
            }
        } else {
            // A path: its last part kept whole where it can be (SS_PATHELLIPSIS), the start shortened.
            const std::size_t sep = whole.find_last_of("\\/");
            const std::size_t tail_at = sep == std::string::npos ? whole.size() / 2 : sep;
            std::size_t head = tail_at;
            std::string tail = whole.substr(tail_at);
            for (;;) {
                shown = whole.substr(0, head) + "..." + tail;
                if (ImGui::CalcTextSize(shown.c_str()).x <= room) break;
                if (head == 0) {
                    // the last part alone is too long: cut it at its end
                    if (tail.empty()) break;
                    std::size_t keep = tail.size();
                    do --keep;
                    while (keep > 0 && (static_cast<unsigned char>(tail[keep]) & 0xC0) == 0x80);
                    tail = tail.substr(0, keep);
                    continue;
                }
                do --head;
                while (head > 0 && !letter_start(head));
            }
        }
    }
    ImGui::TextUnformatted(shown.c_str());
    if (shown != whole && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", whole.c_str());
}

void LogText(const char* label, const std::string& text) {
    Placed p;
    auto& seen = g.logs[label];
    if (seen.first != text.size()) {
        seen.first = text.size();
        seen.second = 3;   // its new height is known a frame later: scrolled for a few
    }
    // A box of its own, not InputTextMultiline: that widget keeps its own scroll
    // and was seen not to follow the text's end (a capture of the Mods page).
    ImGui::PushID(label);
    if (ImGui::BeginChild("##log", p.Box(), ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_None)) {
        ImGui::PushTextWrapPos(0.0f);   // wrapped at the box's width
        ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
        ImGui::PopTextWrapPos();
        if (seen.second > 0) {
            --seen.second;
            ImGui::SetScrollHereY(1.0f);   // the last line at the box's bottom
        }
    }
    ImGui::EndChild();
    ImGui::PopID();
}

void Progress(float fraction) {
    Placed p;
    ImGui::ProgressBar(fraction, ImVec2(p.w > 0 ? S(p.w) : -FLT_MIN, S(p.h)), "");
}

bool CheckList(const char* label, int* index, const std::vector<ListRow>& rows, const std::vector<float>& stops,
               int* toggled) {
    Placed p;
    *toggled = -1;
    bool changed = false;
    const int count = static_cast<int>(rows.size());
    std::string test;
    if (TestTake(label, p.grey, &test)) {
        const bool box = test.compare(0, 4, "box ") == 0;
        const int want = TestNumber(box ? test.substr(4) : test);
        if (want >= 0 && want < count) {
            if (want != *index) changed = true;
            *index = want;
            if (box) *toggled = want;
        }
    }
    ImGui::PushID(label);
    if (ImGui::BeginListBox("##checklist", p.Box())) {
        const float line = ImGui::GetTextLineHeight();
        const float side = line + S(2);   // the box column: as wide as a row is high
        for (int i = 0; i < count; ++i) {
            const ListRow& r = rows[static_cast<std::size_t>(i)];
            ImGui::PushID(i);
            const bool chosen = i == *index;
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            if (ImGui::Selectable("##row", chosen, 0, ImVec2(width, line))) {
                if (!chosen) {
                    *index = i;
                    changed = true;
                }
                if (r.box && ImGui::IsItemClicked() && ImGui::GetIO().MousePos.x - at.x < side) *toggled = i;
            }
            // Space on the row the focus is on: it is chosen (Dear ImGui's own use of the row) and its box used
            if (r.box && ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Space, false)) *toggled = i;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImU32 ink = ImGui::GetColorU32(r.grey ? ImGuiCol_TextDisabled : ImGuiCol_Text);
            if (r.box) {
                const ImVec2 a(at.x + S(2), at.y + S(1)), b(at.x + side - S(2), at.y + line - S(1));
                dl->AddRect(a, b, ink, S(2));
                if (r.ticked) {
                    const float w = b.x - a.x, h = b.y - a.y;
                    const ImVec2 tick[3] = {ImVec2(a.x + w * 0.2f, a.y + h * 0.5f), ImVec2(a.x + w * 0.42f, a.y + h * 0.75f),
                                            ImVec2(a.x + w * 0.8f, a.y + h * 0.25f)};
                    dl->AddPolyline(tick, 3, r.grey ? ink : ImGui::GetColorU32(ImGuiCol_CheckMark), S(1.6f));
                }
            }
            // the columns, each clipped at the next one's start
            const float x0 = at.x + side + S(4), w = width - side - S(4);
            std::size_t from = 0;
            for (std::size_t col = 0; from <= r.text.size(); ++col) {
                std::size_t end = r.text.find('\t', from);
                if (end == std::string::npos) end = r.text.size();
                const float left = x0 + (col == 0 ? 0.0f : col - 1 < stops.size() ? w * stops[col - 1] : w);
                const float right = col < stops.size() ? x0 + w * stops[col] - S(4) : at.x + width;
                if (right > left) {
                    dl->PushClipRect(ImVec2(left, at.y), ImVec2(right, at.y + line), true);
                    dl->AddText(ImVec2(left, at.y), ink, r.text.c_str() + from, r.text.c_str() + end);
                    dl->PopClipRect();
                }
                from = end + 1;
            }
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    ImGui::PopID();
    return changed;
}

void GroupBegin(const char* title, float x, float y, float w, float h) {
    g.next = Next{};
    // The box's line runs through the middle of its title, as a Win32 group box's does.
    const ImGuiStyle& st = ImGui::GetStyle();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 a(origin.x + S(x) - ImGui::GetScrollX(), origin.y + S(y) - ImGui::GetScrollY());
    const float line = ImGui::GetFontSize(), top = a.y + line * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRect(ImVec2(a.x, top), ImVec2(a.x + S(w), a.y + S(h)), ImGui::GetColorU32(ImGuiCol_Border), st.FrameRounding);
    const char* end = std::strstr(title, "##");   // the part from ## is the name alone
    if (!end) end = title + std::strlen(title);
    if (end != title) {
        const ImVec2 size = ImGui::CalcTextSize(title, end);
        const float tx = a.x + S(9);
        dl->AddRectFilled(ImVec2(tx - S(4), a.y), ImVec2(tx + size.x + S(4), a.y + line), Col(Paper()));
        dl->AddText(ImVec2(tx, a.y), ImGui::GetColorU32(ImGuiCol_Text), title, end);
    }
    ImGui::SetCursorPos(ImVec2(S(x), S(y)));
    ImGui::BeginChild(title, ImVec2(S(w), S(h)), ImGuiChildFlags_NavFlattened,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
}

void GroupEnd() { ImGui::EndChild(); }

void AreaBegin(const char* name, float x, float y, float w, float h) {
    g.next = Next{};
    ImGui::SetCursorPos(ImVec2(S(x), S(y)));
    ImGui::BeginChild(name, ImVec2(S(w), S(h)), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
}

void AreaEnd() { ImGui::EndChild(); }

// ---- The bind capture ---------------------------------------------------------

void CaptureBegin(int layout) {
    g.capturing = true;
    g.cap_key = 0;
    g.test_cap_key = 0;
    g.test_cap_pad = -1;
    // The pad's cross and circle are the capture's, not the navigation's;
    // Dear ImGui still reads the pad (imgui_impl_sdl3 fills its inputs either way).
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    // The pad as the Win32 launcher reads it (input/pad_sdl.cpp). SDL counts
    // the opens of one pad, so this and Dear ImGui's backend hold the same pad
    // at once, and either's close leaves the other's open. PadSdl_Stop is
    // never called here: it ends SDL with the window (SDL_Quit).
    input::PadSdl_Start(static_cast<input::Layout>(layout), nullptr);
    input::PadSdl_Find();
    for (int i = 0; i < static_cast<int>(input::PadInput::kCount); ++i)
        g.cap_held[i] = input::PadSdl_InputDown(static_cast<input::PadInput>(i));
    g.wake = 4;
}

void CaptureEnd() {
    g.capturing = false;
    g.cap_key = 0;
    g.test_cap_key = 0;
    g.test_cap_pad = -1;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    g.wake = 4;
}

bool Capturing() { return g.capturing; }

bool CaptureKey(int* scancode) {
    if (!g.capturing) return false;
    if (g.test_cap_key != 0) {
        *scancode = g.test_cap_key;
        g.test_cap_key = 0;
        return true;
    }
    if (g.cap_key == 0) return false;
    *scancode = g.cap_key;
    g.cap_key = 0;
    return true;
}

bool CapturePad(int* input) {
    if (!g.capturing) return false;
    if (g.test_cap_pad >= 0) {
        *input = g.test_cap_pad;
        g.test_cap_pad = -1;
        return true;
    }
    if (!input::PadSdl_Started()) return false;
    // The window's loop pumped SDL's events, which brings the pad's state up
    // to date; PadSdl_Poll would take the pad's events out of the queue before
    // Dear ImGui sees them, so the pad is looked for by PadSdl_Find instead.
    input::PadSdl_Find();
    for (int i = 0; i < static_cast<int>(input::PadInput::kCount); ++i) {
        const bool down = input::PadSdl_InputDown(static_cast<input::PadInput>(i));
        if (!down) {
            g.cap_held[i] = false;
        } else if (!g.cap_held[i]) {
            *input = i;
            return true;
        }
    }
    return false;
}

// ---- The system's own windows -------------------------------------------------

void Message(const char* title, const char* text, bool error) {
    if (g.hidden) {
        // A check: not shown (stage P5, the save editor's messages). Said on stdout, one line.
        std::string line = text ? text : "";
        for (char& c : line)
            if (c == '\n') c = ' ';
        std::printf("bof3x-launcher-ui: check: message: %s: %s\n", title, line.c_str());
        std::fflush(stdout);
        return;
    }
    // SDL shows its message box before SDL_Init too: a refusal before the window opens is still read.
    if (!SDL_ShowSimpleMessageBox(error ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION, title, text, g.window))
        std::fprintf(stderr, "%s: %s\n", title, text);
}

int Ask(const char* title, const char* text) {
    if (g.hidden) {
        // A check: not shown. The question waits for TestAnswer.
        if (g.test_answer >= 0) {
            const int answer = g.test_answer;
            g.test_answer = -1;
            g.test_asked.clear();
            return answer;
        }
        g.test_asked = std::string(title) + ": " + text;
        return -1;
    }
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes"},
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No"},
    };
    SDL_MessageBoxData box{};
    box.flags = SDL_MESSAGEBOX_INFORMATION;
    box.window = g.window;
    box.title = title;
    box.message = text;
    box.numbuttons = 2;
    box.buttons = buttons;
    int answer = 0;
    if (!SDL_ShowMessageBox(&box, &answer)) {
        std::fprintf(stderr, "%s: %s\n(no answer: %s)\n", title, text, SDL_GetError());
        return 0;   // a question that cannot be shown is answered No: nothing is started
    }
    g.wake = 4;
    return answer == 1 ? 1 : 0;
}

void OpenLink(const char* url) {
    if (g.hidden) {
        std::printf("bof3x-launcher-ui: check: the page asked to open %s\n", url);
        std::fflush(stdout);
        return;
    }
    if (!SDL_OpenURL(url)) std::fprintf(stderr, "bof3x-launcher-ui: cannot open %s: %s\n", url, SDL_GetError());
}

void PickFile(int ask, const char* kind, const char* pattern, const char* title) {
    if (g.hidden) {
        // A check: no picker on the screen. TestPick answers it.
        std::lock_guard<std::mutex> lock(g.pick_lock);
        g.pick_done.erase(ask);
        g.test_pick_ask = ask;
        return;
    }
    PickAsk* held = nullptr;
    {
        std::lock_guard<std::mutex> lock(g.pick_lock);
        g.pick_done.erase(ask);
        PickAsk& a = g.pick_asks[ask];   // a map's elements stay where they are: SDL reads the filter until it answers
        a.kind = kind ? kind : "";
        a.pattern = pattern ? pattern : "*";
        a.filter.name = a.kind.c_str();
        a.filter.pattern = a.pattern.c_str();
        held = &a;
    }
    const SDL_PropertiesID props = SDL_CreateProperties();
    if (pattern) {
        SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, &held->filter);
        SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, 1);
    }
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, g.window);
    if (title) SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, title);
    SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFILE, PickDone,
                                     reinterpret_cast<void*>(static_cast<std::intptr_t>(ask)), props);
    SDL_DestroyProperties(props);   // SDL copies what it keeps before it returns
}

void PickFolder(int ask, const char* title) {
    {
        std::lock_guard<std::mutex> lock(g.pick_lock);
        g.pick_done.erase(ask);
        if (g.hidden) {
            g.test_pick_ask = ask;   // a check: TestPick answers it
            return;
        }
    }
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, g.window);
    if (title) SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, title);
    SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFOLDER, PickDone,
                                     reinterpret_cast<void*>(static_cast<std::intptr_t>(ask)), props);
    SDL_DestroyProperties(props);
}

bool Picked(int ask, std::string* path) {
    std::lock_guard<std::mutex> lock(g.pick_lock);
    const auto it = g.pick_done.find(ask);
    if (it == g.pick_done.end()) return false;
    if (path) *path = it->second;
    g.pick_done.erase(it);
    return true;
}

// ---- The save editor (stage P5) -----------------------------------------------

namespace {

// PickFileIn's filters, kept until SDL answers (it reads them until then), by the page's question.
struct PickMany {
    std::vector<std::string> kinds, patterns;
    std::vector<SDL_DialogFileFilter> filters;
};
std::map<int, PickMany> g_pick_many;

// The popup of a box opened under it, as wide as `width` at least and about 14 lines high at most.
void ListUnder(ImVec2 at, float width) {
    ImGui::SetNextWindowPos(at);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0),
                                        ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * 14 + S(8)));
}

}  // namespace

bool ComboWide(const char* label, int* index, const std::vector<std::string>& items, float list_width) {
    Placed p;
    p.Width();
    const int count = static_cast<int>(items.size());
    bool changed = false;
    std::string test;
    if (TestTake(label, p.grey, &test)) {
        int want = TestNumber(test);
        if (test.compare(0, 4, "has ") == 0) {   // the first entry that holds the text
            want = -1;
            for (int i = 0; i < count && want < 0; ++i)
                if (items[static_cast<std::size_t>(i)].find(test.substr(4)) != std::string::npos) want = i;
        }
        if (want >= 0 && want < count && want != *index) {
            *index = want;
            changed = true;
        }
    }
    const char* shown = *index >= 0 && *index < count ? items[static_cast<std::size_t>(*index)].c_str() : "";
    ImGui::PushID(label);
    ImGui::SetNextWindowSizeConstraints(ImVec2(S(list_width), 0), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::BeginCombo("##combo", shown, ImGuiComboFlags_HeightLarge)) {
        for (int i = 0; i < count; ++i) {
            ImGui::PushID(i);
            const bool chosen = i == *index;
            if (ImGui::Selectable(items[static_cast<std::size_t>(i)].c_str(), chosen) && !chosen) {
                *index = i;
                changed = true;
            }
            if (chosen) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    return changed;
}

bool EditCombo(const char* label, std::string* text, const std::vector<std::string>& items, int* picked, bool focus) {
    Placed p;
    *picked = -1;
    const int count = static_cast<int>(items.size());
    std::string test;
    if (TestTake(label, p.grey, &test)) {
        if (test.compare(0, 6, "entry ") == 0) {
            const int want = TestNumber(test.substr(6));
            if (want >= 0 && want < count) {
                *picked = want;
                *text = items[static_cast<std::size_t>(want)];
            }
        } else {
            *text = test;
        }
    }
    ImGui::PushID(label);
    const float arrow = ImGui::GetFrameHeight();
    const float width = p.w > 0 ? S(p.w) : ImGui::CalcItemWidth();
    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(std::max(width - arrow, 1.0f));
    if (focus) ImGui::SetKeyboardFocusHere();
    const bool enter = ImGui::InputText("##text", text, ImGuiInputTextFlags_EnterReturnsTrue);
    const ImVec2 under(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y);
    ImGui::SameLine(0, 0);
    if (ImGui::ArrowButton("##open", ImGuiDir_Down)) ImGui::OpenPopup("##list");
    ImGui::EndGroup();
    ListUnder(under, width);
    if (ImGui::BeginPopup("##list")) {
        for (int i = 0; i < count; ++i) {
            ImGui::PushID(i);
            const std::string& item = items[static_cast<std::size_t>(i)];
            const bool chosen = item == *text;
            if (ImGui::Selectable(item.c_str(), chosen)) {
                *picked = i;
                *text = item;
                ImGui::CloseCurrentPopup();
            }
            if (chosen) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return enter;
}

bool ColumnList(const char* label, int* index, const std::vector<std::string>& titles, const std::vector<float>& weights,
                const std::vector<std::vector<std::string>>& rows, int* used) {
    Placed p;
    *used = -1;
    bool changed = false;
    const int count = static_cast<int>(rows.size());
    std::string test;
    if (TestTake(label, p.grey, &test)) {
        const bool use = test.compare(0, 4, "use ") == 0;
        const int want = TestNumber(use ? test.substr(4) : test);
        if (want >= 0 && want < count) {
            if (want != *index) changed = true;
            *index = want;
            if (use) *used = want;
        }
    }
    const int columns = static_cast<int>(titles.size());
    ImGui::PushID(label);
    if (columns > 0 &&
        ImGui::BeginTable("##columns", columns,
                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingStretchProp,
                          p.Box())) {
        ImGui::TableSetupScrollFreeze(0, 1);
        for (int c = 0; c < columns; ++c)
            ImGui::TableSetupColumn(titles[static_cast<std::size_t>(c)].c_str(), ImGuiTableColumnFlags_WidthStretch,
                                    static_cast<std::size_t>(c) < weights.size() ? weights[static_cast<std::size_t>(c)] : 1.0f);
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;
        clipper.Begin(count);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const std::vector<std::string>& row = rows[static_cast<std::size_t>(i)];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(i);
                const bool chosen = i == *index;
                const bool pressed = ImGui::Selectable(row.empty() ? "" : row[0].c_str(), chosen,
                                                       ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick);
                const bool double_click = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                if (pressed && !chosen) {
                    *index = i;
                    changed = true;
                } else if (pressed && chosen && !ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !double_click) {
                    *used = i;   // the chosen row used again by the keyboard or the pad
                }
                if (double_click) {
                    *index = i;
                    *used = i;
                }
                for (int c = 1; c < columns && static_cast<std::size_t>(c) < row.size(); ++c) {
                    ImGui::TableSetColumnIndex(c);
                    ImGui::TextUnformatted(row[static_cast<std::size_t>(c)].c_str());
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
    return changed;
}

int AskNo(const char* title, const char* text) {
    if (g.hidden) return Ask(title, text);   // a check: the same wait for TestAnswer
    const SDL_MessageBoxButtonData buttons[] = {
        {0, 1, "Yes"},
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No"},
    };
    SDL_MessageBoxData box{};
    box.flags = SDL_MESSAGEBOX_WARNING;
    box.window = g.window;
    box.title = title;
    box.message = text;
    box.numbuttons = 2;
    box.buttons = buttons;
    int answer = 0;
    if (!SDL_ShowMessageBox(&box, &answer)) {
        std::fprintf(stderr, "%s: %s\n(no answer: %s)\n", title, text, SDL_GetError());
        return 0;   // a question that cannot be shown is answered No: nothing is discarded or replaced
    }
    g.wake = 4;
    return answer == 1 ? 1 : 0;
}

void PickFileIn(int ask, const std::vector<PickFilter>& filters, const char* folder, bool may_be_new, const char* title) {
    {
        std::lock_guard<std::mutex> lock(g.pick_lock);
        g.pick_done.erase(ask);
        if (g.hidden) {
            g.test_pick_ask = ask;   // a check: TestPick answers it
            return;
        }
    }
    PickMany& held = g_pick_many[ask];   // a map's elements stay where they are: SDL reads them until it answers
    held = PickMany{};
    for (const PickFilter& f : filters) {
        held.kinds.push_back(f.kind);
        held.patterns.push_back(f.pattern);
    }
    for (std::size_t i = 0; i < filters.size(); ++i)
        held.filters.push_back(SDL_DialogFileFilter{held.kinds[i].c_str(), held.patterns[i].c_str()});
    const SDL_PropertiesID props = SDL_CreateProperties();
    if (!held.filters.empty()) {
        SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, held.filters.data());
        SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, static_cast<Sint64>(held.filters.size()));
    }
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, g.window);
    if (folder && *folder) SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, folder);
    if (title) SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, title);
    SDL_ShowFileDialogWithProperties(may_be_new ? SDL_FILEDIALOG_SAVEFILE : SDL_FILEDIALOG_OPENFILE, PickDone,
                                     reinterpret_cast<void*>(static_cast<std::intptr_t>(ask)), props);
    SDL_DestroyProperties(props);   // SDL copies what it keeps before it returns
}

}  // namespace bof3x::ui
