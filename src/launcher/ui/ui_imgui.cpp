// launcher/ui/ui.h drawn with Dear ImGui on SDL3's renderer
// (docs/launcher-portable.md, stage P1; the owner's call 1, 2026-11-19). The
// only file of the launcher that includes Dear ImGui: the pages name ui.h's
// widgets. SDL3 is the window, the renderer, the pad, the PNG reader, the
// message box and the file pickers; nothing here names an operating system.
#include "launcher/ui/ui.h"

#include "launcher/ui/assets.h"

#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
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
};
State g;

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
    Placed() : w(g.next.w), h(g.next.h), grey(g.next.grey), reason(std::move(g.next.reason)) {
        if (g.next.at) ImGui::SetCursorPos(ImVec2(S(g.next.x), S(g.next.y)));
        g.next = Next{};
        if (grey) ImGui::BeginDisabled();
    }
    ~Placed() {
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

// TestSet's value for the widget `label`, once, when the widget can be used.
bool TestTake(const char* label, bool grey, std::string* value) {
    if (!g.test_set) return false;
    const char* cut = std::strstr(label, "##");
    const std::string whole = label, shown = cut ? std::string(label, cut) : whole, name = cut ? cut + 2 : "";
    if (g.test_widget != whole && g.test_widget != shown && (name.empty() || g.test_widget != name)) return false;
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
        if (SDL_WaitEventTimeout(&e, 500)) {
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
bool TestGrey() { return g.test_grey; }

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

void Progress(float fraction) {
    Placed p;
    ImGui::ProgressBar(fraction, ImVec2(p.w > 0 ? S(p.w) : -FLT_MIN, S(p.h)), "");
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

// ---- The system's own windows -------------------------------------------------

void Message(const char* title, const char* text, bool error) {
    // SDL shows its message box before SDL_Init too: a refusal before the window opens is still read.
    if (!SDL_ShowSimpleMessageBox(error ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION, title, text, g.window))
        std::fprintf(stderr, "%s: %s\n", title, text);
}

void PickFile(int ask, const char* kind, const char* pattern) {
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
    SDL_ShowOpenFileDialog(PickDone, reinterpret_cast<void*>(static_cast<std::intptr_t>(ask)), g.window,
                           pattern ? &held->filter : nullptr, pattern ? 1 : 0, nullptr, false);
}

void PickFolder(int ask) {
    {
        std::lock_guard<std::mutex> lock(g.pick_lock);
        g.pick_done.erase(ask);
    }
    SDL_ShowOpenFolderDialog(PickDone, reinterpret_cast<void*>(static_cast<std::intptr_t>(ask)), g.window, nullptr,
                             false);
}

bool Picked(int ask, std::string* path) {
    std::lock_guard<std::mutex> lock(g.pick_lock);
    const auto it = g.pick_done.find(ask);
    if (it == g.pick_done.end()) return false;
    if (path) *path = it->second;
    g.pick_done.erase(it);
    return true;
}

}  // namespace bof3x::ui
