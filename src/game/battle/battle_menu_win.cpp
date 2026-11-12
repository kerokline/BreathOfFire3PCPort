// Moved here by tools/move_funcs.py from battle_draw.cpp, battle_e7.cpp, fmv_play.cpp, menu_draw_helpers.cpp, mode_flow.cpp, msgbox.cpp, rest_2h.cpp, window_task.cpp (docs/source-layout-round.md).
#include "game/battle/battle_menu_win.h"

#include <SDL3/SDL.h>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dinput.h>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/area_data_reload.h"
#include "game/battle/battle_menu_win_callees.h"
#include "game/battle_draw.h"
#include "game/battle_draw_callees.h"
#include "game/battle_e7.h"
#include "game/battle_e7_callees.h"
#include "game/boss_harness.h"
#include "game/button_text.h"
#include "game/d3d_lines.h"
#include "game/dat_load.h"
#include "game/display_setup.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_draw_helpers.h"
#include "game/menu_draw_helpers_callees.h"
#include "game/menu_frame.h"
#include "game/mode_flow.h"
#include "game/mode_flow_callees.h"
#include "game/move_script_bytes.h"
#include "game/msgbox.h"
#include "game/msgbox_callees.h"
#include "game/pad_read.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2h.h"
#include "game/rest_2h_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/text_draw.h"
#include "game/text_pairs.h"
#include "game/widescreen.h"
#include "game/win_main.h"
#include "game/window_task.h"
#include "game/window_task_callees.h"
#include "game/yes_no_layout.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"
#include "video/movie.h"

namespace {

namespace at = battle_menu_win::at;

}  // namespace

namespace {

using namespace battle_draw;

using namespace menu_draw_helpers;

}  // namespace

using namespace mode_flow;

using namespace msgbox;

using namespace window_task;

namespace {

unsigned char* AtBattleDraw(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

battle_draw::U Addr(const void* p) { return Low32(p); }

battle_draw::U WordBattleDraw(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

battle_draw::U LongBattleDraw(const unsigned char* p) {
    battle_draw::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutWord(unsigned char* p, battle_draw::U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

unsigned char Byte(battle_draw::U address) { return AtBattleDraw(address)[0]; }

std::int32_t S8(battle_draw::U v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

std::int32_t S16(battle_draw::U v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

// The x of the title within its window: the original's 6 * (13 - n), or
// under DIV-0059 the same centre, x + 78, less half the real width.
int ListTitleX(const unsigned char* label, int n) {
    if (!g_list_title_centre) return 6 * (13 - n);
    return 78 - static_cast<int>(TextAdvance_Width(label) / 2);
}

using U = std::uint32_t;

using move_script::SetWord;

unsigned char& BBattleE7(U address) { return move_script::At(address)[0]; }

unsigned W(const unsigned char* p) { return move_script::Word(p); }

const char* Str(U address) { return reinterpret_cast<const char*>(move_script::At(address)); }

char* Buf(U address) { return reinterpret_cast<char*>(move_script::At(address)); }

const unsigned char* Text(U address) { return move_script::At(address); }

// DIV-0035: where the 640 x 480 video lands in a client of cw x ch. DIV-0042
// (the owner, 2026-09-23): with snap, the largest whole multiple of 640 x 480
// that fits, centred - a 3x game window shows the video at 2x with a border,
// and the window never changes size for a video; without snap, or when not
// even 1x fits, the largest 4:3 fit. `whole` says whether the fit is a whole
// multiple (the stretch then copies pixels; else it averages).
RECT Destination(LONG cw, LONG ch, bool* whole) {
    const LONG kx = cw / 640, ky = ch / 480;
    LONG k = DisplaySetup_Snap() ? (kx < ky ? kx : ky) : 0;
    LONG w, h;
    if (k >= 1) {
        w = 640 * k;
        h = 480 * k;
    } else if (cw * 3 >= ch * 4) {
        h = ch;
        w = ch * 4 / 3;
    } else {
        w = cw;
        h = cw * 3 / 4;
    }
    *whole = k >= 1;
    const LONG x = (cw - w) / 2, y = (ch - h) / 2;
    return {x, y, w, h};   // right = width, bottom = height, as MCI's "put" took them
}

// A movie that cannot be opened - a disc-only install has no AVIs (docs/
// disc-only-install.md), and LOGOS.AVI has no disc counterpart at all - is
// skipped and the game goes on, exactly as the original's two failed opens
// return; the original said nothing, this says it once per file name a
// process.
void LogNotOpened(const char* filename, const char* why) {
    static char seen[4][64];
    static unsigned count = 0;
    for (unsigned i = 0; i < count; ++i)
        if (std::strncmp(seen[i], filename, sizeof seen[i] - 1) == 0) return;
    if (count < 4) std::snprintf(seen[count++], sizeof seen[0], "%s", filename);
    bof3::Log("fmv: %s not opened (%s; the CD root %s tried too): skipped, the game goes on as the original does when "
              "the open fails",
              filename, why, File_CdRoot());
}

// The file's bytes, read whole (both movies are under 5 MB). Null when it
// cannot be opened or read, with the reason in `why`.
std::uint8_t* ReadWhole(const char* path, std::size_t* size, char* why, std::size_t why_n) {
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        std::snprintf(why, why_n, "error %lu", GetLastError());
        return nullptr;
    }
    LARGE_INTEGER n;
    if (!GetFileSizeEx(f, &n) || n.QuadPart <= 0 || n.QuadPart > 256 * 1024 * 1024) {
        std::snprintf(why, why_n, "its size (%lld bytes) is not one this reads", static_cast<long long>(n.QuadPart));
        CloseHandle(f);
        return nullptr;
    }
    auto* data = static_cast<std::uint8_t*>(HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(n.QuadPart)));
    if (!data) {
        std::snprintf(why, why_n, "no memory for %lld bytes", static_cast<long long>(n.QuadPart));
        CloseHandle(f);
        return nullptr;
    }
    std::size_t got = 0;
    while (got < static_cast<std::size_t>(n.QuadPart)) {
        DWORD read = 0;
        const DWORD want = static_cast<DWORD>(static_cast<std::size_t>(n.QuadPart) - got > 0x1000000 ? 0x1000000 : static_cast<std::size_t>(n.QuadPart) - got);
        if (!ReadFile(f, data + got, want, &read, nullptr) || read == 0) {
            std::snprintf(why, why_n, "read error %lu at %zu", GetLastError(), got);
            HeapFree(GetProcessHeap(), 0, data);
            CloseHandle(f);
            return nullptr;
        }
        got += read;
    }
    CloseHandle(f);
    *size = got;
    return data;
}

struct Playing {
    HWND hwnd;
    const video::Movie* movie;
    RECT dest;         // DIV-0035's rectangle: left, top, width, height
    bool whole;        // a whole multiple: pixels copied, not averaged
    bool paused;       // WM_ACTIVATEAPP's pause (DIV-0049: not under DIV-0033)
    LONGLONG pause_began;    // QPC ticks when the pause began
    LONGLONG paused_ticks;   // QPC ticks spent paused so far: the clock stands still through them
    SDL_AudioStream* audio;
};

Playing* g_playing;

// The current frame into the rectangle. DIV-0042's whole multiple copies
// pixels (COLORONCOLOR); a fit averages them (HALFTONE), as MCI's own draw
// did when told to stretch.
void Draw(HDC dc) {
    const Playing& p = *g_playing;
    const U w = p.movie->Width(), h = p.movie->Height();
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof bmi.bmiHeader;
    bmi.bmiHeader.biWidth = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(h);   // top-down rows, as the decoders give them
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    if (p.whole) {
        SetStretchBltMode(dc, COLORONCOLOR);
    } else {
        SetStretchBltMode(dc, HALFTONE);
        SetBrushOrgEx(dc, 0, 0, nullptr);
    }
    StretchDIBits(dc, p.dest.left, p.dest.top, p.dest.right, p.dest.bottom, 0, 0, static_cast<int>(w), static_cast<int>(h),
                  p.movie->Frame(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}

void DrawToWindow() {
    HDC dc = GetDC(g_playing->hwnd);
    if (!dc) return;
    Draw(dc);
    ReleaseDC(g_playing->hwnd, dc);
}

LONGLONG Now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

// WM_ACTIVATEAPP's pair, in place of MCI's "pause vfw" / "resume vfw": the
// clock stops with the picture, and the audio stream with them.
void Pause(bool on) {
    Playing* p = g_playing;
    if (!p || p->paused == on) return;
    p->paused = on;
    if (on) {
        p->pause_began = Now();
        if (p->audio) SDL_PauseAudioStreamDevice(p->audio);
    } else {
        p->paused_ticks += Now() - p->pause_began;
        if (p->audio) SDL_ResumeAudioStreamDevice(p->audio);
    }
}

// The PCM stream, when the file has one, as an SDL3 audio stream of its own
// (DIV-0201's library; the game's sound output does not exist yet, the
// movies play before Display_Setup and Game_Init). The stream takes the
// movie's own rate - 22 kHz for the AVI, 37,800 Hz for CAPCOM30.STR
// (DIV-0214) - and SDL3's audio stream resamples it to the device. The whole
// stream is put in before the play starts - 7 s of stereo - and the device
// plays it at its own pace. Null, with a log line, when the device cannot be
// opened: the picture plays without it.
SDL_AudioStream* OpenAudio(video::Movie& movie, const char* filename) {
    if (!movie.HasPcm()) return nullptr;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        bof3::Log("fmv: %s: SDL_InitSubSystem(AUDIO) failed: %s - the picture plays without its sound", filename,
                  SDL_GetError());
        return nullptr;
    }
    const SDL_AudioSpec want = {movie.AudioBits() == 8 ? SDL_AUDIO_U8 : SDL_AUDIO_S16LE,
                                static_cast<int>(movie.AudioChannels()), static_cast<int>(movie.AudioRate())};
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want, nullptr, nullptr);
    if (!stream) {
        bof3::Log("fmv: %s: SDL_OpenAudioDeviceStream failed: %s - the picture plays without its sound", filename,
                  SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return nullptr;
    }
    U n;
    while (const std::uint8_t* chunk = movie.NextAudio(&n))
        if (n) SDL_PutAudioStreamData(stream, chunk, static_cast<int>(n));
    SDL_FlushAudioStream(stream);
    return stream;
}

void CloseAudio(SDL_AudioStream* stream) {
    if (!stream) return;
    SDL_DestroyAudioStream(stream);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

constexpr auto& AtMenuDrawHelpers = move_script::At;

constexpr auto& LongMenuDrawHelpers = move_script::Long;

constexpr auto& WordMenuDrawHelpers = move_script::Word;

using Handler = void (__cdecl*)();

unsigned char* RecMenuDrawHelpers() {
    return AtMenuDrawHelpers(*reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr))));
}

// rv::As<records::WindowRecord>(p) views a window record
// (docs/typed-records.md; WindowRecords
// 0x803160, the records BattleWinStates_Current points at). Only +0..+7 are
// typed: +8..+0x1F mean something else in each handler (typed-records 3).
short S(const unsigned char* p) { return static_cast<short>(WordMenuDrawHelpers(p)); }

// `call / jmp [table + index * 4]`: the .data dword read at the call, the
// index unbounded as in the original (a byte, zero-extended).
void Dispatch(menu_draw_helpers::U table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<menu_draw_helpers::U>(LongMenuDrawHelpers(AtMenuDrawHelpers(table + index * 4)))))();
}

// The step: the current record's byte +3 through a step table.
void Step(menu_draw_helpers::U table) { Dispatch(table, RecMenuDrawHelpers()[3]); }

// The slides: x (+4) or y (+6) stepped by `by`; past `bound` (signed 16-bit;
// `below` says which side) it is set to `to` and the step +3 goes to 0.
void SlideX(int by, short bound, bool below, short to) {
    unsigned char* const w = RecMenuDrawHelpers();
    rv::As<records::WindowRecord>(w)->x = static_cast<unsigned>(rv::As<records::WindowRecord>(w)->x + by);
    const short x = S(w + 4);
    if (below ? x < bound : x > bound) {
        rv::As<records::WindowRecord>(w)->x = static_cast<std::uint16_t>(to);
        w[3] = 0;
    }
}

}  // namespace

namespace mode_flow {

using move_script::At;

}  // namespace mode_flow

namespace {

constexpr auto& AtRest2h = move_script::At;

constexpr auto& LongRest2h = move_script::Long;

constexpr auto& WordRest2h = move_script::Word;

unsigned char& BRest2h(U address) { return *AtRest2h(address); }

U LRest2h(U address) { return static_cast<U>(LongRest2h(AtRest2h(address))); }

int S8Rest2h(unsigned v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

U Key(const void* p) { return Low32(p); }

// The current window record, re-read at every use (the originals reload
// 0x905B84 after each call).
unsigned char* RecRest2h() { return AtRest2h(*reinterpret_cast<volatile U*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr)))); }

int XRest2h(const unsigned char* w) { return rv::As<records::WindowRecord>(w)->x; }

int YRest2h(const unsigned char* w) { return rv::As<records::WindowRecord>(w)->y; }

unsigned char Style() { return static_cast<unsigned char>(Config_WindowColour); }

// The step: the current record's +3 through a step table, called through the
// word the table holds (the fuzz swaps the words for recorders); past the
// table's own run of handlers the original jumps through whatever follows,
// ours aborts.
void StepRest2h(const char* who, U table, unsigned entries) {
    const unsigned index = RecRest2h()[3];
    if (index >= entries)
        bof3::Fatal("%s: the window's step +3 is %u, past the %u entries of 0x%X - the original jumps through the "
                    "dword after (docs/rest_2h.md section 7)",
                    who, index, entries, (unsigned)::bof3::VaOf(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(LRest2h(table + 4 * index)))();
}

// A slide: x (+4) or y (+6) moved by `by`; past `bound` (signed 16-bit; `below`
// says which side) it is set to `to` and the step +3 goes to 0. The originals
// re-read 0x905B84 between the stores with nothing called in between.
void Slide(unsigned at_word, int by, short bound, bool below, short to) {
    unsigned char* const w = RecRest2h();
    move_script::SetWord(w + at_word, static_cast<unsigned>(WordRest2h(w + at_word) + by));
    const auto v = static_cast<short>(WordRest2h(w + at_word));
    if (below ? v < bound : v > bound) {
        move_script::SetWord(w + at_word, static_cast<std::uint16_t>(to));
        w[3] = 0;
    }
}

U RecordOf(U id) { return BRest2h(::bof3::Img(rest_2h::at::kMemberMap.v) + (id & 0xFF)); }

char* PrintBuf() { return reinterpret_cast<char*>(AtRest2h(::bof3::Img(rest_2h::at::kPrintBuf.v))); }

const char* FormatRest2h(U address) { return reinterpret_cast<const char*>(AtRest2h(address)); }

const unsigned char* TextRest2h(U address) { return AtRest2h(address); }

void PutFloat(unsigned char* at, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

void Piece(int x, int y, unsigned id) { SH_CALL(Menu_DrawPiece)(x, y, id, 1); }

void Pieces(int x, int y, U list) { SH_CALL(Menu_DrawPieces)(x, y, TextRest2h(list), 1); }

}  // namespace

namespace window_task {

using move_script::At;

namespace {

// The record the layer is running, re-read from 0x905B84 at every point the
// original re-reads it - after every call, and in most of these functions
// after every store too (the port's compiler never kept it in a register
// across one). Volatile so that ours cannot cache it either.
unsigned char* Rec() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(
        *reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr)))));
}

}  // namespace

}  // namespace window_task

// original 0x59CB40, handler 8 kind 1: the step (BattleMenuWin_ItemListSteps:
// a bare ret, 0x59A580 - DH's, BattleMenuWin_ItemListSlideRight, 0x59CB90 -
// in no group, 0x59A5E0 - DH's), then BattleMenu_DrawItemList(the record,
// re-read after the step).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMenuWin_ItemListRun(void) {
    Step(::bof3::Img(BattleMenuWin_ItemListSteps_addr));
    menu_draw_helpers::g.battle_items(RecMenuDrawHelpers());
}

// original 0x59CB60, handler 8 kind 1 step 2: x += 0x20; above 0x53, x = 0x52
// and step 0. As the original has it: the test is against 0x53 and the store
// 0x52, so an x that lands on 0x53 stays there a frame and goes on to 0x73
// and then 0x52.
extern "C" void __cdecl BattleMenuWin_ItemListSlideRight(void) { SlideX(0x20, 0x53, false, 0x52); }

// original 0x59CB90, BattleMenuWin_ItemListSteps[3]: x -= 0x20; below 0x53,
// x = 0x52 and step 0. As the original has it: the test against 0x53 and the
// store 0x52, the pair of its twin BattleMenuWin_ItemListSlideRight (D88).
extern "C" void __cdecl BattleMenuWin_ItemListSlideLeft(void) { Slide(4, -0x20, 0x53, true, 0x52); }

// original 0x59CBC0, handler 8 kind 2: the step (BattleMenuWin_SkillListSteps:
// a bare ret, 0x59A5E0 - DH's, BattleMenuWin_SkillListSlideIn), then
// BattleMenu_DrawSkillList(the record, re-read after the step).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMenuWin_SkillListRun(void) {
    Step(::bof3::Img(BattleMenuWin_SkillListSteps_addr));
    menu_draw_helpers::g.battle_skills(RecMenuDrawHelpers());
}

// original 0x59CBE0, handler 8 kind 2 step 2: x -= 0x20; below 0x53, x = 0x53
// and step 0.
extern "C" void __cdecl BattleMenuWin_SkillListSlideIn(void) { SlideX(-0x20, 0x53, true, 0x53); }

// original 0x59CC10, handler 8 kind 3: the step (BattleMenuWin_EquipSteps: a
// bare ret, 0x596920 - R2F's, MenuSlide_RightTo17, BattleMenuWin_EquipSlideTo62),
// then BattleEquipWin_Draw(the member 0x66972C[0x904065[+0xC]], +4, +6, +0x20,
// +0xD, the record) of the record re-read.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMenuWin_EquipRun(void) {
    StepRest2h("BattleMenuWin_EquipRun", ::bof3::Img(BattleMenuWin_EquipSteps_addr), 4);
    unsigned char* const w = RecRest2h();
    const U member = RecordOf(BRest2h(::bof3::Img(rest_2h::at::kBattleParty.v) + w[0xC]));
    SH_CALL(BattleEquipWin_Draw)(member, static_cast<unsigned>(XRest2h(w)), static_cast<unsigned>(YRest2h(w)),
                                 static_cast<U>(LongRest2h(w + 0x20)), w[0xD], Key(w));
}

// original 0x59CC60, kind 3 step 3: x += 0x20; above 0x62, x = 0x62 and step 0.
extern "C" void __cdecl BattleMenuWin_EquipSlideTo62(void) { Slide(4, 0x20, 0x62, false, 0x62); }

// original 0x59CC90, handler 8 kind 4: the step (BattleMenuWin_EquipItemsSteps:
// a bare ret, BattleMenuWin_EquipItemsSlideTo98, MenuSlide_RightOff, 0x59A610 -
// R2G's), then BattleEquipWin_DrawItems(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMenuWin_EquipItemsRun(void) {
    StepRest2h("BattleMenuWin_EquipItemsRun", ::bof3::Img(BattleMenuWin_EquipItemsSteps_addr), 4);
    SH_CALL(BattleEquipWin_DrawItems)(RecRest2h());
}

// original 0x59CCB0, kind 4 step 1: x += 0x20; above 0x98, x = 0x98 and step 0.
extern "C" void __cdecl BattleMenuWin_EquipItemsSlideTo98(void) { Slide(4, 0x20, 0x98, false, 0x98); }

// original 0x59CCE0, handler 8 kind 5: Menu_DrawVerbPair(+4, +6, +0xB) of the
// record. No step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMenuWin_VerbPairRun(void) {
    const unsigned char* const w = RecRest2h();
    SH_CALL(Menu_DrawVerbPair)(XRest2h(w), YRest2h(w), w[0xB]);
}

extern "C" void __cdecl BattleMenu_DrawItemList(unsigned char* w) {
    auto* const window_rec = rv::As<records::WindowRecord>(w);
    battle_draw::g.box(static_cast<battle_draw::U>(window_rec->x) + 3, static_cast<battle_draw::U>(window_rec->y) + 3, 0x99, 0x82, w[9], static_cast<unsigned char>(Config_WindowColour));
    unsigned char offset = 0, moving = 0;
    const unsigned char top = battle_draw::g.list_scroll(w + 0xB, &offset, &moving, w + 0x12);
    const battle_draw::U category = w[0xA];
    battle_draw::U items = LongBattleDraw(AtBattleDraw(::bof3::Img(kInventory.v) + category * 4)) + top;
    battle_draw::U counts = LongBattleDraw(AtBattleDraw(::bof3::Img(kCounts.v) + category * 4)) + top;   // 0 + top for key items: not read then
    battle_draw::U row = static_cast<std::uint16_t>(S8(offset) + static_cast<battle_draw::U>(window_rec->y)) + 0x1Au;
    const unsigned rows = moving + 7u;
    for (unsigned char i = 0; i < rows; ++i) {
        const unsigned char item = AtBattleDraw(items)[0];
        if (item != 0) {
            unsigned char dim = 1;
            if (w[9] == 0) {
                const unsigned member = Byte(::bof3::Img(kMemberRecord.v) + Byte(::bof3::Img(kBattleParty.v) + BattleActorCopies_ItemActor));
                dim = battle_draw::g.can_use(w[8], member, w[0xA], item) ? 0 : 1;
            }
            unsigned char colour = dim ? 7 : 0;
            const battle_draw::U here = static_cast<battle_draw::U>(i) + w[0xB];
            const battle_draw::U cursor = w[0xD];
            if (here == cursor) colour = 2;
            const unsigned char row_category = w[0xA];
            const unsigned char count = row_category == 4 ? 1 : AtBattleDraw(counts)[0];
            const int x = static_cast<int>(static_cast<battle_draw::U>(window_rec->x)) + 7;
            if (here == cursor || here == w[0xC]) {
                if (colour != 7) battle_draw::g.item_row(x, static_cast<int>(row), 7, row_category, AtBattleDraw(items)[0], count, 1);
                battle_draw::g.item_row(static_cast<int>(static_cast<battle_draw::U>(window_rec->x)) + 7, static_cast<int>(row) - 2, colour, w[0xA],
                           AtBattleDraw(items)[0], count, dim);
            } else {
                battle_draw::g.item_row(x, static_cast<int>(row), colour, row_category, AtBattleDraw(items)[0], count, dim);
            }
        }
        ++items;
        ++counts;
        row += 0xD;
    }

    battle_draw::g.box(static_cast<battle_draw::U>(window_rec->x) + 3, static_cast<battle_draw::U>(window_rec->y) + 3, 0x99, 0x14, w[9], static_cast<unsigned char>(Config_WindowColour));
    battle_draw::g.box(static_cast<battle_draw::U>(window_rec->x) + 3, static_cast<battle_draw::U>(window_rec->y) + 0x7A, 0x99, 8, w[9], static_cast<unsigned char>(Config_WindowColour));
    {
        const unsigned char grey = w[9] ? 7 : 0;
        const unsigned char* const label = AtBattleDraw(LongBattleDraw(AtBattleDraw(::bof3::Img(kItemLabels.v) + w[0xA] * 4u)));
        const int y = static_cast<int>(static_cast<battle_draw::U>(window_rec->y)) + 7;
        const unsigned n = battle_draw::g.char_count(label);
        battle_draw::g.text_draw_at(static_cast<int>(ListTitleX(label, static_cast<int>(n)) + static_cast<int>(static_cast<battle_draw::U>(window_rec->x))), y, grey,
                       0x10, label);
    }
    {
        const unsigned char cat = w[0xA];
        const unsigned room = cat != 4 ? 0x80 : 0x20;
        const unsigned have = battle_draw::g.count_used(cat) & 0xFFu;
        battle_draw::g.sprintf_(reinterpret_cast<char*>(AtBattleDraw(::bof3::Img(kPrintBuf.v))), reinterpret_cast<const char*>(AtBattleDraw(::bof3::Img(kFmt3d3d.v))), have, room);
        const unsigned char grey = w[9] ? 7 : 0;
        battle_draw::g.font8(static_cast<battle_draw::U>(window_rec->x) + 0x55, static_cast<battle_draw::U>(window_rec->y) + 0x7A, grey, AtBattleDraw(::bof3::Img(kPrintBuf.v)));
    }
    battle_draw::g.pieces(static_cast<battle_draw::U>(window_rec->x), static_cast<battle_draw::U>(window_rec->y), AtBattleDraw((w[0x10] & 2) ? ::bof3::Img(kTitleBit1On.v) : ::bof3::Img(kTitleBit1Off.v)), 1);
    battle_draw::g.pieces(static_cast<battle_draw::U>(window_rec->x), static_cast<battle_draw::U>(window_rec->y), AtBattleDraw((w[0x10] & 1) ? ::bof3::Img(kTitleBit0On.v) : ::bof3::Img(kTitleBit0Off.v)), 1);
    {
        const battle_draw::U t = WordBattleDraw(w + 0x10);
        PutWord(w + 0x10, (t & 0xF0) ? t - 0x10 : 0);
    }
    for (int b = 0; b < 10; ++b) battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + b * 8 + 0x28, static_cast<battle_draw::U>(window_rec->y), 1, 1);
    for (int b = 0; b < 12; ++b) battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x), static_cast<battle_draw::U>(window_rec->y) + b * 8 + 0x18, 4, 1);
    battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + 0x90, static_cast<battle_draw::U>(window_rec->y) + 0x18, 0x16, 1);
    for (int b = 0; b < 9; ++b) battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + 0x90, static_cast<battle_draw::U>(window_rec->y) + b * 8 + 0x28, 0x17, 1);
    battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + 0x90, static_cast<battle_draw::U>(window_rec->y) + 0x70, 0x18, 1);
    battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x), static_cast<battle_draw::U>(window_rec->y) + 0x78, 0x19, 1);
    for (int b = 0; b < 9; ++b) battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + b * 8 + 8, static_cast<battle_draw::U>(window_rec->y) + 0x78, 0x1A, 1);
    battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + 0x50, static_cast<battle_draw::U>(window_rec->y) + 0x78, 0x1B, 1);
    for (int b = 0; b < 6; ++b) battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + b * 8 + 0x58, static_cast<battle_draw::U>(window_rec->y) + 0x78, 0x1C, 1);
    battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + 0x88, static_cast<battle_draw::U>(window_rec->y) + 0x78, 0x1D, 1);
    battle_draw::g.scroll_bar(AtBattleDraw(LongBattleDraw(AtBattleDraw(::bof3::Img(kInventory.v) + w[0xA] * 4u))), w[0xB], static_cast<battle_draw::U>(window_rec->x) + 0x90, static_cast<battle_draw::U>(window_rec->y) + 0x18, 7, 0x80,
                 0x5C);
}

// original 0x59D200 (no PSX pair): the battle's skill list. The window record:
// +4 / +6 x and y, +8 the usable test's mode, +9 two arrow flags and a
// countdown in its high nibble, +0xA the member and +0xB the list's kind
// (group BD's 0x591E50 with 1: a 10-byte list of skill ids in the member's
// battle record), +0xC a marked index, +0xD the cursor, +0x10 the top index
// (a byte to the scroll step; read back as an s16 per row), +0x12 the scroll
// state, word +0x14 non-zero for the list titled 0x66A220 instead of by kind
// (docs/dialogue-localisation.md names that string the skill list's header).
//
// The box (x + 3, y + 3, 0x99, 0x82) with flags 0; the scroll step; each row
// 13 apart whose id is not 0: 0x57DA70(+8, 0x929F06, id) - dim when it answers
// 0; colour 7 when dim, else 0, and 2 when the ROW INDEX is +0xD; the cost
// 0x591DB0(0x929F06, id, 1) and the icon kind 0x5918A0(id); the row through
// 0x57DC90 with the skill's record 0x65C4C8 + 0x18 * id; the cursor's and the
// marked row - top (s16 +0x10) + row index equal to +0xD or +0xC - drawn 2
// higher over a colour-7 dim shadow (none when the colour is 7). The colour
// test and the raise test disagree once the list is scrolled (docs/battle_draw.md
// section 5). Then the title box, the title centred by its byte length (strlen,
// where the item list counts characters), the arrows by +9's bits 1 and 0, the
// countdown in +9 stepped down, the frame's pieces (two more with word +0x14)
// and the scroll bar over the list's 10 entries.
extern "C" void __cdecl BattleMenu_DrawSkillList(unsigned char* w) {
    auto* const window_rec = rv::As<records::WindowRecord>(w);
    battle_draw::g.box(static_cast<battle_draw::U>(window_rec->x) + 3, static_cast<battle_draw::U>(window_rec->y) + 3, 0x99, 0x82, 0, static_cast<unsigned char>(Config_WindowColour));
    unsigned char offset = 0, moving = 0;
    const unsigned char top = battle_draw::g.list_scroll(w + 0x10, &offset, &moving, w + 0x12);
    battle_draw::U ids = Addr(battle_draw::g.skill_list(w[0xA], w[0xB], 1)) + top;
    battle_draw::U row = static_cast<std::uint16_t>(S8(offset) + static_cast<battle_draw::U>(window_rec->y)) + 0x1Au;
    const unsigned rows = moving + 7u;
    for (unsigned char i = 0; i < rows; ++i) {
        const unsigned char id = AtBattleDraw(ids)[0];
        if (id != 0) {
            const unsigned char usable = static_cast<unsigned char>(battle_draw::g.skill_usable(w[8], static_cast<unsigned char>(BattleActorCopies_ItemActor), id));
            const unsigned char dim = usable == 0 ? 1 : 0;
            unsigned char colour = dim ? 7 : 0;
            if (i == w[0xD]) colour = 2;
            const unsigned cost = battle_draw::g.skill_cost(static_cast<unsigned char>(BattleActorCopies_ItemActor), AtBattleDraw(ids)[0], 1) & 0xFFu;
            const unsigned icon = battle_draw::g.skill_icon(AtBattleDraw(ids)[0]) & 0xFFu;
            const std::int32_t here = static_cast<std::int32_t>(i) + S16(WordBattleDraw(w + 0x10));
            const unsigned char* const record = AtBattleDraw(::bof3::Img(kSkillRecords.v) + AtBattleDraw(ids)[0] * 0x18u);
            if (here == static_cast<std::int32_t>(w[0xD]) || here == static_cast<std::int32_t>(w[0xC])) {
                if (colour != 7) battle_draw::g.skill_row(static_cast<int>(static_cast<battle_draw::U>(window_rec->x)) + 7, static_cast<int>(row), 7, icon, record, cost, 1);
                battle_draw::g.skill_row(static_cast<int>(static_cast<battle_draw::U>(window_rec->x)) + 7, static_cast<int>(row) - 2, colour, icon, record, cost,
                            dim);
            } else {
                battle_draw::g.skill_row(static_cast<int>(static_cast<battle_draw::U>(window_rec->x)) + 7, static_cast<int>(row), colour, icon, record, cost, dim);
            }
        }
        ++ids;
        row += 0xD;
    }

    battle_draw::g.box(static_cast<battle_draw::U>(window_rec->x) + 3, static_cast<battle_draw::U>(window_rec->y) + 3, 0x99, 0x14, 0, static_cast<unsigned char>(Config_WindowColour));
    {
        const unsigned char* const label =
            AtBattleDraw(WordBattleDraw(w + 0x14) != 0 ? BattleDraw_SkillLabelAlt : LongBattleDraw(AtBattleDraw(::bof3::Img(kSkillLabels.v) + w[0xB] * 4u)));
        const int y = static_cast<int>(static_cast<battle_draw::U>(window_rec->y)) + 7;
        const battle_draw::U length = static_cast<battle_draw::U>(std::strlen(reinterpret_cast<const char*>(label)));
        battle_draw::g.text_draw_at(static_cast<int>(ListTitleX(label, static_cast<int>(length)) + static_cast<battle_draw::U>(window_rec->x)), y, 0, 0x10, label);
    }
    battle_draw::g.pieces(static_cast<battle_draw::U>(window_rec->x), static_cast<battle_draw::U>(window_rec->y), AtBattleDraw((w[9] & 2) ? ::bof3::Img(kTitleBit1On.v) : ::bof3::Img(kTitleBit1Off.v)), 1);
    battle_draw::g.pieces(static_cast<battle_draw::U>(window_rec->x), static_cast<battle_draw::U>(window_rec->y), AtBattleDraw((w[9] & 1) ? ::bof3::Img(kTitleBit0On.v) : ::bof3::Img(kTitleBit0Off.v)), 1);
    {
        const unsigned char t = w[9];
        w[9] = (t & 0xF0) ? static_cast<unsigned char>(t - 0x10) : 0;
    }
    for (int b = 0; b < 10; ++b) battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + b * 8 + 0x28, static_cast<battle_draw::U>(window_rec->y), 1, 1);
    for (int b = 0; b < 12; ++b) battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x), static_cast<battle_draw::U>(window_rec->y) + b * 8 + 0x18, 4, 1);
    battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + 0x90, static_cast<battle_draw::U>(window_rec->y) + 0x18, 0x16, 1);
    for (int b = 0; b < 9; ++b) battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + 0x90, static_cast<battle_draw::U>(window_rec->y) + b * 8 + 0x28, 0x17, 1);
    battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + 0x90, static_cast<battle_draw::U>(window_rec->y) + 0x70, 0x18, 1);
    battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x), static_cast<battle_draw::U>(window_rec->y) + 0x78, 0x19, 1);
    for (int b = 0; b < 17; ++b) battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + b * 8 + 8, static_cast<battle_draw::U>(window_rec->y) + 0x78, 0x1A, 1);
    if (WordBattleDraw(w + 0x14) != 0) {
        battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + 8, static_cast<battle_draw::U>(window_rec->y), 0x32, 1);
        battle_draw::g.piece(static_cast<battle_draw::U>(window_rec->x) + 0x90, static_cast<battle_draw::U>(window_rec->y), 0x33, 1);
    }
    {
        // x, y and the top read before the list lookup, as the original's
        const int x = static_cast<int>(static_cast<battle_draw::U>(window_rec->x)) + 0x90, y = static_cast<int>(static_cast<battle_draw::U>(window_rec->y)) + 0x18;
        const unsigned char first = w[0x10];
        const unsigned char* const list = battle_draw::g.skill_list(w[0xA], w[0xB], 1);
        battle_draw::g.scroll_bar(list, first, x, y, 7, 0xA, 0x5C);
    }
}

// original 0x59D640: (member, x, y, set, flags, record) - the window at (x +
// 4, y + 4) 0x78 by 0xA3; the member's name (CharacterRecords + 0xA4 member,
// 5 characters at most) centred on x + 0x3E; the four stat labels 0x66A0F8..
// at (x + 5, y + 0x27 + 13 k); Char_RecalcStats, then the four stats +0x24,
// +0x26, +0x2A, +0x28 (by 0x64E324, 8-pixel font, at (x + 0x31, y + 0x1C + 13
// k)). Unless flags bit 0: Equip_PreviewSet(member, set, marks, values) into
// the stack, and per stat a bar 0x59DB70(x + 0x51, y + 0x1C + 13 k, 0x11 for
// mark 4, 0x12 for 1, else 0x13, 0x1C, CLUT (0, 0x1E0), 0x80) and the value
// in the mark's colour at (x + 0x59, same y). Then the six equipped items
// (+0x12..+0x17, categories 0x66B5BC): the record's +0x10 = 0 first; row r
// in colour 2 when it is the record's +0xB; with the record's +0xD bit 1, an
// item Item_CanUse(2, the member 0x66972C[0x904065[s8 0x929F06]], category,
// item) refuses is drawn dim in colour 7, and for one it allows on row +0xA
// the record's +0x10 = category << 8 | item, and it is first drawn dim in
// colour 7; the row +0xA / +0xB raised by 2 pixels; then its icon
// (0x66B5B4) and name. Last the pieces 0x66B508 and the frame's edges.
//
// DIVERGENCE DIV-0065: the labels' row. The original draws label k at y +
// 0x27 + 13 k and the values at y + 0x1C + 13 k, so each label sits two
// pixels above the next stat's value - the first value has no label and the
// fourth label is drawn under the frame, off the box. The field's member
// panel (0x5738A0: the label at y + 8, the value at y + 0xA) has the label
// two pixels above its own value; 0x27 is that relation one 13-pixel row
// down, 0x1A + 0xD. Ours draws the labels at y + 0x1A + 13 k once the flag
// is on; BattleE7_Inject sets it after the self-test, which compares the
// original's rows.
unsigned char g_equip_labels_row = 0;

extern "C" void __cdecl BattleEquipWin_Draw(U member, U x, U y, U set, U flags, U record) {
    const int xi = static_cast<int>(x), yi = static_cast<int>(y);
    const int label_y = g_equip_labels_row ? 0x1A : 0x27;
    BH_CALL(Menu_DrawBox)(xi + 4, yi + 4, 0x78, 0xA3, 0, static_cast<unsigned char>(Config_WindowColour));
    unsigned char* const ch = move_script::At(::bof3::Img(CharacterRecords_addr) + (member & 0xFF) * battle_e7::at::kCharStride);
    unsigned n = BH_CALL(Text_CharCount)(ch) & 0xFF;
    if (n > 5) n = 5;
    BH_CALL(Text_DrawAt)(xi - 6 * static_cast<int>(n) + 0x3E, yi + 7, 0, 5, ch);
    for (unsigned k = 0; k < 4; ++k)
        BH_CALL(Text_DrawAt)(xi + 5, yi + label_y + 13 * static_cast<int>(k), 0, 4, Text(::bof3::Img(battle_e7::at::kStatLabels[k])));
    BH_CALL(Char_RecalcStats)(ch);
    static constexpr unsigned kStats[4] = {0x24, 0x26, 0x2A, 0x28};
    for (unsigned k = 0; k < 4; ++k) {
        BH_CALL(Crt_sprintf)(Buf(::bof3::Img(battle_e7::at::kPrintBuf.v)), Str(::bof3::Img(battle_e7::at::kFmtStat.v)), W(ch + kStats[k]));
        BH_CALL(Text_DrawFont8)(xi + 0x31, yi + 0x1C + 13 * static_cast<int>(k), 0, Text(::bof3::Img(battle_e7::at::kPrintBuf.v)));
    }
    if ((flags & 1) == 0) {
        unsigned char marks[4];
        unsigned short values[4];
        BH_CALL(Equip_PreviewSet)(member, reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(set)), marks,
                                  values);
        for (unsigned k = 0; k < 4; ++k) {
            const unsigned mark = marks[k];
            const unsigned colour = mark == 4 ? 0x11u : (mark != 1 ? 0x13u : 0x12u);
            BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xF, 0);
            BH_CALL(Gfx_CommitPrim)(1, 0xC);
            const int by = yi + 13 * static_cast<int>(k) + 0x1C;
            const unsigned clut = BH_CALL(Gpu_GetClut)(0, 0x1E0);
            using BarFn = void (__cdecl*)(U, U, U, U, U, U);
            BH_AT(BarFn, battle_e7::at::kDrawBar.v)(x + 0x51, static_cast<U>(by), colour, 0x1C, clut, 0x80);
            BH_CALL(Crt_sprintf)(Buf(::bof3::Img(battle_e7::at::kPrintBuf.v)), Str(::bof3::Img(battle_e7::at::kFmtStat.v)), static_cast<unsigned>(values[k]));
            BH_CALL(Text_DrawFont8)(xi + 0x59, by, static_cast<int>(marks[k]), Text(::bof3::Img(battle_e7::at::kPrintBuf.v)));
        }
    }
    unsigned char items[6];
    std::memcpy(items, ch + 0x12, sizeof items);
    unsigned char* const rec = move_script::At(record);
    move_script::SetWord(rec + 0x10, 0);
    int text_y = yi + 0x55, icon_y = yi + 0x57;
    for (unsigned r = 0; r < 6; ++r) {
        const unsigned category = BBattleE7(::bof3::Img(battle_e7::at::kEquipCategories.v) + r);
        const unsigned char* const name = BH_CALL(Item_NamePtr)(category, items[r]);
        unsigned colour = 0;
        if (r == rec[0xB]) colour = 2;
        if (rec[0xD] & 2) {
            const unsigned slot = BBattleE7(::bof3::Img(battle_e7::at::kPartyOrder.v) + static_cast<std::uint32_t>(BattleActorCopies_ItemActor));
            const unsigned who = Character_IndexById[slot];
            if (BH_CALL(Item_CanUse)(2, who, category, items[r]) == 0) {
                colour = 7;
            } else if (r == rec[0xA]) {
                move_script::SetWord(rec + 0x10, (category << 8) + items[r]);
            }
        }
        if (colour != 7) {
            BH_CALL(Menu_DrawIcon8)(xi + 6, icon_y, BBattleE7(::bof3::Img(battle_e7::at::kEquipIcons.v) + r), 1);
            const unsigned m = BH_CALL(Text_CharCount)(name);
            BH_CALL(Text_DrawAt)(xi + 0x11, text_y, 7, static_cast<int>(m), name);
        }
        const bool raised = r == rec[0xB] || r == rec[0xA];
        if (raised) {
            text_y -= 2;
            icon_y -= 2;
        }
        BH_CALL(Menu_DrawIcon8)(xi + 6, icon_y, BBattleE7(::bof3::Img(battle_e7::at::kEquipIcons.v) + r), colour == 7 ? 1 : 0);
        const unsigned m = BH_CALL(Text_CharCount)(name);
        BH_CALL(Text_DrawAt)(xi + 0x11, text_y, static_cast<int>(colour), static_cast<int>(m), name);
        if (r == rec[0xB] || r == rec[0xA]) {
            icon_y += 2;
            text_y += 2;
        }
        icon_y += 0xD;
        text_y += 0xD;
    }
    BH_CALL(Menu_DrawPieces)(xi, yi, Text(::bof3::Img(battle_e7::at::kEquipPieces.v)), 1);
    for (int j = 0; j < 7; ++j) {
        BH_CALL(Menu_DrawPiece)(xi, yi + 8 * j + 0x18, 4, 1);
        BH_CALL(Menu_DrawPiece)(xi + 0x78, yi + 8 * j + 0x18, 8, 1);
    }
    for (int j = 0; j < 9; ++j) {
        BH_CALL(Menu_DrawPiece)(xi, yi + 8 * j + 0x58, 4, 1);
        BH_CALL(Menu_DrawPiece)(xi + 0x78, yi + 8 * j + 0x58, 8, 1);
    }
    for (int j = 0; j < 14; ++j) {
        BH_CALL(Menu_DrawPiece)(xi + 8 * j + 8, yi + 0x50, 0xE, 1);
        BH_CALL(Menu_DrawPiece)(xi + 8 * j + 8, yi + 0xA0, 0x11, 1);
    }
    BH_CALL(Menu_DrawPiece)(xi, yi + 0x50, 0xD, 1);
    BH_CALL(Menu_DrawPiece)(xi + 0x78, yi + 0x50, 0xF, 1);
    BH_CALL(Menu_DrawPiece)(xi, yi + 0xA0, 0x10, 1);
    BH_CALL(Menu_DrawPiece)(xi + 0x78, yi + 0xA0, 0x12, 1);
}

// original 0x59DB70: one 8 x 8 sprite of a stat bar (x, y, u, v, clut, shade):
// at the packet cursor (read once) Gpu_SetSprt8; x and y (their words,
// unsigned) as floats; u << 3, v << 3 (bytes); the CLUT word; the shade byte
// as its grey; Gpu_SetSemiTrans(.., 0); Gfx_CommitPrim(1, 0x18).
// BattleEquipWin_Draw (battle_e7.cpp) and 0x585DC0 (R2C's) call it.
extern "C" void __cdecl BattleEquipWin_DrawBar(int x, int y, unsigned u, unsigned v, unsigned clut, unsigned shade) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt8)(p);
    PutFloat(p + 8, static_cast<std::int32_t>(static_cast<U>(x) & 0xFFFF));
    PutFloat(p + 0xC, static_cast<std::int32_t>(static_cast<U>(y) & 0xFFFF));
    p[0x15] = static_cast<unsigned char>(v << 3);
    p[0x14] = static_cast<unsigned char>(u << 3);
    move_script::SetWord(p + 0x16, clut);
    p[6] = p[5] = p[4] = static_cast<unsigned char>(shade);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0x18);
}

// original 0x59DBF0: the battle equipment window's item list of the window
// `w`. +8 = the category the slot +9 names (1..3: 2, the armour; 4, 5: 3;
// else 1). For each of the category's 128 items (Inventory_IdLists /
// _CountLists by +8) that Item_CanUse(4, the member 0x66972C[0x904065[+0xC]],
// +8, item) allows - and, for the armour, whose Item_IconKind(2, item) is +9 +
// 1 - its id and count into 0x6BE0C4 / 0x6BE144, then 0s to 128 bytes. +0xD =
// the id at +0xB. The box (x + 3, y + 3, 0x99, 0x82, 0, style);
// Menu_ListScroll(+0xA, &offset, &moving, +0x10); from the answered top, rows r
// below moving + 7 at y + offset + 0x1A + 13 r, each non-empty one
// Menu_DrawItemRow(x + 7, that, colour, +8, id, count, dim) - the row whose
// +0xA (re-read) + r is +0xB lit (7, dim 1) and then two up in 0, the others in
// 0. The title and count boxes (x + 3, y + 3, 0x99, 0x14), (x + 3, y + 0x7A,
// 0x99, 8); the category's title (0x66B5C4[+8]) centred (x + 6 (13 - its
// length), y + 7, 0, 0x10) - through the call DIV-0059 re-aims; the count "n /
// 128" (0x6639C8) at (x + 0x55, y + 0x7A); the frame pieces; the scroll bar
// (0x6BE0C4, +0xA, x + 0x90, y + 0x18, 7, 0x80, 0x5C).
//
// As the original has it: the rows read the lists by top + r past their 128
// bytes, in place; the title's pointer is read in place (labels.cpp retargets
// the table's words under a language overlay).
extern "C" void __cdecl BattleEquipWin_DrawItems(unsigned char* w) {
    const unsigned slot = static_cast<unsigned>(w[9]) - 1u;
    if (slot <= 2) w[8] = 2;
    else if (slot <= 4) w[8] = 3;
    else w[8] = 1;
    const unsigned cat = w[8];
    const unsigned char* ids = AtRest2h(LRest2h(::bof3::Img(rest_2h::at::kIdLists.v) + 4 * cat));
    const unsigned char* counts = AtRest2h(LRest2h(::bof3::Img(rest_2h::at::kCountLists.v) + 4 * cat));
    unsigned char n = 0;
    for (unsigned k = 0; k < 0x80; ++k, ++ids, ++counts) {
        const unsigned id = *ids;
        const U member = RecordOf(BRest2h(::bof3::Img(rest_2h::at::kBattleParty.v) + w[0xC]));
        const unsigned char allowed = SH_CALL(Item_CanUse)(4, member, w[8], id);
        if (allowed == 0) continue;
        if (w[8] == 2) {
            const unsigned kind = SH_CALL(Item_IconKind)(2, *ids) & 0xFF;
            if (static_cast<int>(w[9]) - 1 != static_cast<int>(kind) - 2) continue;
        }
        BRest2h(::bof3::Img(rest_2h::at::kUseIds.v) + n) = *ids;
        BRest2h(::bof3::Img(rest_2h::at::kUseCounts.v) + n) = *counts;
        ++n;
    }
    if (n < 0x80) {
        std::memset(AtRest2h(::bof3::Img(rest_2h::at::kUseCounts.v) + n), 0, 0x80u - n);
        std::memset(AtRest2h(::bof3::Img(rest_2h::at::kUseIds.v) + n), 0, 0x80u - n);
    }
    w[0xD] = BRest2h(::bof3::Img(rest_2h::at::kUseIds.v) + w[0xB]);
    const unsigned char style = Style();
    SH_CALL(Menu_DrawBox)(XRest2h(w) + 3, YRest2h(w) + 3, 0x99, 0x82, 0, style);
    unsigned char offset = 0, moving = 0;
    const unsigned char top = SH_CALL(Menu_ListScroll)(w + 0xA, &offset, &moving, w + 0x10);
    int text_y = S8Rest2h(offset) + YRest2h(w) + 0x1A;
    const U row_ids = ::bof3::Img(rest_2h::at::kUseIds.v) + top;
    const U row_counts = ::bof3::Img(rest_2h::at::kUseCounts.v) + top;
    for (unsigned char r = 0; static_cast<int>(r) < static_cast<int>(moving) + 7; ++r, text_y += 0xD) {
        const unsigned char id = BRest2h(row_ids + r);
        if (id == 0) continue;
        if (static_cast<unsigned>(w[0xA]) + r == w[0xB]) {
            SH_CALL(Menu_DrawItemRow)(XRest2h(w) + 7, text_y, 7, w[8], id, BRest2h(row_counts + r), 1);
            SH_CALL(Menu_DrawItemRow)(XRest2h(w) + 7, text_y - 2, 0, w[8], BRest2h(row_ids + r), BRest2h(row_counts + r), 0);
        } else {
            SH_CALL(Menu_DrawItemRow)(XRest2h(w) + 7, text_y, 0, w[8], id, BRest2h(row_counts + r), 0);
        }
    }
    SH_CALL(Menu_DrawBox)(XRest2h(w) + 3, YRest2h(w) + 3, 0x99, 0x14, 0, Style());
    SH_CALL(Menu_DrawBox)(XRest2h(w) + 3, YRest2h(w) + 0x7A, 0x99, 8, 0, Style());
    const unsigned title_cat = w[8];
    const int title_y = YRest2h(w) + 7;
    const unsigned char* const title = AtRest2h(LRest2h(::bof3::Img(rest_2h::at::kUseTitles.v) + 4 * title_cat));
    const unsigned char length = SH_CALL(Text_CharCount)(title);
    using DrawAt = const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*);
    SH_WORD(DrawAt, ListTitle_SiteDraw())(6 * (0xD - static_cast<int>(length)) + XRest2h(w), title_y, 0, 0x10, title);
    SH_CALL(Crt_sprintf)(PrintBuf(), FormatRest2h(::bof3::Img(rest_2h::at::kFmtCount.v)), static_cast<unsigned>(n), 0x80u);
    SH_CALL(Text_DrawFont8)(XRest2h(w) + 0x55, YRest2h(w) + 0x7A, 0, TextRest2h(::bof3::Img(rest_2h::at::kPrintBuf.v)));
    Pieces(XRest2h(w), YRest2h(w), ::bof3::Img(rest_2h::at::kUsePiecesA.v));
    Pieces(XRest2h(w), YRest2h(w), ::bof3::Img(rest_2h::at::kUsePiecesB.v));
    for (int i = 0; i < 10; ++i) Piece(8 * i + 0x28 + XRest2h(w), YRest2h(w), 1);
    for (int i = 0; i < 12; ++i) Piece(XRest2h(w), 8 * i + 0x18 + YRest2h(w), 4);
    Piece(XRest2h(w) + 0x90, YRest2h(w) + 0x18, 0x16);
    for (int i = 0; i < 9; ++i) Piece(XRest2h(w) + 0x90, 8 * i + 0x28 + YRest2h(w), 0x17);
    Piece(XRest2h(w) + 0x90, YRest2h(w) + 0x70, 0x18);
    Piece(XRest2h(w), YRest2h(w) + 0x78, 0x19);
    for (int i = 0; i < 9; ++i) Piece(8 * i + 8 + XRest2h(w), YRest2h(w) + 0x78, 0x1A);
    Piece(XRest2h(w) + 0x50, YRest2h(w) + 0x78, 0x1B);
    for (int i = 0; i < 6; ++i) Piece(8 * i + 0x58 + XRest2h(w), YRest2h(w) + 0x78, 0x1C);
    Piece(XRest2h(w) + 0x88, YRest2h(w) + 0x78, 0x1D);
    Piece(XRest2h(w) + 8, YRest2h(w), 0x32);
    Piece(XRest2h(w) + 0x90, YRest2h(w), 0x33);
    SH_CALL(Menu_DrawScrollBar)(AtRest2h(::bof3::Img(rest_2h::at::kUseIds.v)), w[0xA], XRest2h(w) + 0x90, YRest2h(w) + 0x18, 7, 0x80, 0x5C);
}

// original 0x59E160: two verb buttons (x, y, selected): the box (x, y, 0x2D,
// 0x14, 0, style), verb 0x66A228 at (x + 0xA, y + 3, colour, 0x10) - colour 7
// unless selected is 0xFF or 0 - and the pieces 0x66B500 when selected is 0,
// else 0x66B4F8; then the same 0x30 to the right for verb 0x66A240, its colour 7
// unless selected is 0xFF or 1, its pieces 0x66B500 when selected is 1. The
// catalogue filed it under the renderer by its address; it is game code, in no
// group, and only BattleMenuWin_VerbPairRun calls it.
extern "C" void __cdecl Menu_DrawVerbPair(int x, int y, unsigned selected) {
    const unsigned char style = Style();
    SH_CALL(Menu_DrawBox)(x, y, 0x2D, 0x14, 0, style);
    const auto s = static_cast<unsigned char>(selected);
    SH_CALL(Text_DrawAt)(x + 0xA, y + 3, s == 0xFF || s == 0 ? 0 : 7, 0x10, TextRest2h(::bof3::Img(Menu_Verbs_addr)));
    SH_CALL(Menu_DrawPieces)(x, y, TextRest2h(s == 0 ? ::bof3::Img(rest_2h::at::kVerbPiecesOn.v) : ::bof3::Img(rest_2h::at::kVerbPiecesOff.v)), 1);
    const int x2 = x + 0x30;
    const unsigned char style2 = Style();
    SH_CALL(Menu_DrawBox)(x2, y, 0x2D, 0x14, 0, style2);
    SH_CALL(Text_DrawAt)(x2 + 0xA, y + 3, s == 0xFF || s == 1 ? 0 : 7, 0x10, TextRest2h(::bof3::Img(Menu_Verbs_addr) + 0x18));
    SH_CALL(Menu_DrawPieces)(x2, y, TextRest2h(s == 1 ? ::bof3::Img(rest_2h::at::kVerbPiecesOn.v) : ::bof3::Img(rest_2h::at::kVerbPiecesOff.v)), 1);
}

// original 0x59E230 (PSX 0x8015973C): three passes over the 22 window records
// at 0x803160, starting from the pass byte at 0x802D20. A record with byte 0
// non-zero whose byte +0xF is the running pass becomes the current record
// (0x905B84) and runs handler byte +1 of a nine-entry table the function
// builds on its own stack.
//
// As the original has it: the starting pass is read once into a register, so
// a handler that changes 0x802D20 (Window_Kind0Close does, to 0) only affects
// the NEXT frame; a starting pass above 2 runs nothing; the record walk is a
// register too, so a handler that repoints 0x905B84 does not move it. There
// is no bound on byte +1: index 9 of the original's stack table is its own
// return address. That cannot be reproduced, so ours aborts loudly
// (CLAUDE.md rule 4) - Window_Alloc is the only thing that sets byte +1.
//
// DIVERGENCE DIV-0041 (amended 2026-10-13, the faster slide,
// docs/ultrawide-slides.md): around each handler, Widescreen_SlideBefore and
// _SlideAfter - a parked slide-off box drawn at the wide bound, and Capcom's x
// back in +4 once the handler has drawn. Both do nothing until
// Widescreen_ArmFills, and when the view is narrow.
extern "C" void __cdecl Field_RunTaskRecords(void) {
    unsigned pass = BossSa_WindowPass;
    if (pass > 2) return;
    do {
        for (std::uint32_t a = ::bof3::Img(WindowRecords_addr); a < ::bof3::Img(window_task::at::kRecordsEnd.v); a += 0x24) {
            unsigned char* const record = window_task::At(a);
            if (record[0] == 0) {
                Widescreen_SlideIdle(record);
                continue;
            }
            if (record[0xF] != pass) continue;
            BattleWinStates_Current = static_cast<std::int32_t>(Low32(record));
            const unsigned handler = record[1];
            if (handler >= 9)
                bof3::Fatal("window record %u names handler %u; the original's stack table holds nine",
                            static_cast<unsigned>((a - ::bof3::Img(WindowRecords_addr)) / 0x24), handler);
            Widescreen_SlideBefore(record);
            window_task::g.records[handler]();
            Widescreen_SlideAfter(record);
        }
        ++pass;
    } while (pass <= 2);
}

// original 0x59E2D0 (PSX 0x80159874). Claims window record `slot` (stride
// 0x24 off 0x803160) for `kind` if its first byte is free.
//
// As the original has it: the slot is masked to a byte for the index but the
// ANSWER is the argument dword - unchanged on success, with its low byte
// forced to 0xFF on failure (`or al, 0xFF`), so a caller that passes a slot
// with high bytes set gets them back. No bound on the slot.
extern "C" unsigned __cdecl Window_Alloc(unsigned slot, unsigned kind) {
    unsigned char* const rec = msgbox::At(::bof3::Img(WindowRecords_addr) + (slot & 0xFF) * 0x24u);
    if (rec[0] != 0) return slot | 0xFFu;
    rec[0] = 1;
    rec[1] = static_cast<unsigned char>(kind);
    rec[2] = 0;
    rec[3] = 0;
    return slot;
}

// original 0x59E310 (PSX 0x801598DC): frees the current record - byte 0, the
// kind byte +2 and the state byte +3 to zero. Byte +1, the record handler
// Field_RunTaskRecords dispatches on, is left as it was. The pointer is
// re-read for each of the three stores.
extern "C" void __cdecl Window_FreeCurrent(void) {
    Rec()[0] = 0;
    Rec()[2] = 0;
    Rec()[3] = 0;
}

// original 0x59E330 (PSX 0x8015990C, the JP executable): every window
// record's in-use, handler, kind, state and pass bytes (+0..+3, +0xF)
// zeroed, and the pass the walk starts from. No calls.
//
// As the original has it, and the JP disc: the other fields keep what the
// last window left, and one is read before any write - +0xD bit 0 (no
// frame), which Window_Grow / Window_Shrink read for kinds 1 and 2 and never
// set, so a list or battle window that set it leaves the next window
// frameless. The US executable's reset (PSX 0x80158E50) zeroes +0..+0x1F of
// every record - the bytes, the words +4 / +6 and the eight words +0x10..
// +0x1E - and leaves +0x20..+0x23, as here.
//
// DIVERGENCE DIV-0175 (known-defects D352): 1 through
// PatchBytes("WindowResetFull") in ModeFlow_Inject, after the self-test,
// which compared Capcom's five-field reset.
namespace mode_flow { std::uint8_t g_reset_full = 0; }

extern "C" void __cdecl Window_ResetAll(void) {
    for (unsigned i = 0; i < 22; ++i) {
        unsigned char* const r = mode_flow::At(::bof3::Img(WindowRecords_addr) + i * 0x24);
        if (mode_flow::g_reset_full) {
            std::memset(r, 0, 0x20);
            continue;
        }
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
        r[3] = 0;
        r[0xF] = 0;
    }
    BossSa_WindowPass = 0;
}

extern "C" void __cdecl Fmv_Play(const char* filename, void* hwnd_, int fullscreen) {
    HWND hwnd = static_cast<HWND>(hwnd_);
    Fmv_Playing = 0;

    // The open: the name as given, then under the CD root - the original's
    // two MCI opens, "%s" then "%s%s" with File_CdRoot (0x66B674, 0x66B654).
    //
    // DIVERGENCE DIV-0214: for CAPCOM.AVI, when neither open succeeds, the
    // same two opens of CAPCOM30.STR, the PlayStation disc's own logo movie
    // (a disc-only install has no AVI); a CAPCOM.AVI that is there still wins.
    const char* const requested = filename;
    const char* const names[2] = {filename, _stricmp(filename, "CAPCOM.AVI") == 0 ? "CAPCOM30.STR" : nullptr};
    char path[0x104];
    char why[256];
    char why_str[256] = "";
    std::size_t size = 0;
    std::uint8_t* data = nullptr;
    for (int n = 0; n < 2 && names[n] && !data; ++n) {
        char* const w = n == 0 ? why : why_str;
        const std::size_t wn = n == 0 ? sizeof why : sizeof why_str;
        std::snprintf(path, sizeof path, "%s", names[n]);
        data = ReadWhole(path, &size, w, wn);
        if (!data) {
            char root_why[128];
            std::snprintf(path, sizeof path, "%s%s", File_CdRoot(), names[n]);
            data = ReadWhole(path, &size, root_why, sizeof root_why);
            if (!data) std::snprintf(w + std::strlen(w), wn - std::strlen(w), "; under the root %s", root_why);
        }
        if (data) filename = names[n];
    }
    if (!data) {
        if (names[1]) std::snprintf(why + std::strlen(why), sizeof why - std::strlen(why), "; %s: %s", names[1], why_str);
        LogNotOpened(requested, why);
        return;
    }
    if (filename != requested)
        bof3::Log("DIV-0214    %s is not there; %s plays in its place, the PlayStation disc's own movie", requested, filename);
    video::Movie movie;
    if (!movie.Open(data, size, why, sizeof why)) {
        LogNotOpened(filename, why);
        HeapFree(GetProcessHeap(), 0, data);
        return;
    }

    (void)fullscreen;   // DIV-0035: no Fmv_EnterFullscreen, whatever it says

    // SetWindowLongPtrA / GWLP_WNDPROC: SetWindowLongA / GWL_WNDPROC on i686
    // (the SDK's own aliases), the pointer-wide form on x86-64, where the
    // 32-bit one does not exist.
    const LONG_PTR saved_proc =
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, static_cast<LONG_PTR>(::bof3::Code(bof3::addr::Fmv_WndProc)));

    RECT client = {};
    GetClientRect(hwnd, &client);
    Playing playing = {};
    playing.hwnd = hwnd;
    playing.movie = &movie;
    playing.dest = Destination(client.right - client.left, client.bottom - client.top, &playing.whole);
    bof3::Log("DIV-0035    %s into the window at %ld,%ld %ld x %ld of %ld x %ld", filename, playing.dest.left,
              playing.dest.top, playing.dest.right, playing.dest.bottom, client.right - client.left,
              client.bottom - client.top);
    const double frame_s = movie.FrameSeconds();
    const U frames = movie.FrameCount();
    const double audio_s = movie.AudioSeconds();
    const double video_s = frames * frame_s;
    const double end_s = audio_s > video_s ? audio_s : video_s;
    // The log lines carry the ledger entry of the movie that ran: DIV-0205 for an AVI, DIV-0214 for an STR.
    const bool str = std::strcmp(movie.Container(), "STR") == 0;
    bof3::Log("%s    %s: %s %u x %u, %u frames at %.3f a second (%.2f s)%s", str ? "DIV-0214" : "DIV-0205", filename,
              movie.Codec(), movie.Width(), movie.Height(), frames, 1.0 / frame_s, video_s,
              movie.HasPcm() ? "" : ", no audio stream");
    if (movie.HasPcm())
        bof3::Log("%s    %s: PCM %u Hz, %u channel(s), %u bits, %.2f s", str ? "DIV-0214" : "DIV-0205", filename,
                  movie.AudioRate(), movie.AudioChannels(), movie.AudioBits(), audio_s);
    playing.audio = OpenAudio(movie, filename);
    g_playing = &playing;

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    const LONGLONG t0 = Now();
    if (playing.audio) SDL_ResumeAudioStreamDevice(playing.audio);
    Fmv_Playing = 1;
    // DIVERGENCE DIV-0066: the pad skips too. The original's pump is a
    // blocking GetMessage, ended by Fmv_WndProc (a key, a click, the video's
    // end, WM_DESTROY); the pad reaches the game only through Pad_Read, which
    // the loop never calls. Ours drains the queue the same way, then polls
    // the pad and waits up to a frame for the next message. A pad input
    // going down during the video ends it as a key does; one held from
    // before the video is ignored until released. A WM_QUIT taken off the
    // queue is put back for WinMain's loop, which GetMessage returning 0
    // used to leave there.
    bool pad_was_down = PadRead_AnyInputDown();
    MSG msg;
    U shown = 0;        // frames decoded so far; the frame on screen is shown - 1
    bool faulted = false;
    while (Fmv_Playing) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                Fmv_Playing = 0;
                PostQuitMessage(static_cast<int>(msg.wParam));
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (!Fmv_Playing) break;
        }
        if (!Fmv_Playing) break;
        const bool pad_down = PadRead_AnyInputDown();
        if (pad_down && !pad_was_down) {
            bof3::Log("DIV-0066    %s skipped by the pad", filename);
            Fmv_Playing = 0;
            break;
        }
        pad_was_down = pad_down;
        DWORD wait = 16;
        if (!playing.paused) {
            const double elapsed =
                static_cast<double>(Now() - t0 - playing.paused_ticks) / static_cast<double>(freq.QuadPart);
            if (elapsed >= end_s) {   // the video's end - MCI's notify
                Fmv_Playing = 0;
                break;
            }
            // The frame due now; every frame up to it is decoded (each is
            // coded over the last), only the due one drawn.
            U due = static_cast<U>(elapsed / frame_s) + 1;
            if (due > frames) due = frames;
            while (shown < due && !faulted) {
                if (!movie.NextFrame(why, sizeof why)) {
                    faulted = true;   // the picture stays on the last good frame to the end
                    bof3::Log("fmv: %s: frame %u: %s - the picture holds from here", filename, movie.FramesDecoded(), why);
                }
                ++shown;
                if (shown == due || faulted) DrawToWindow();
            }
            const double next = shown < frames ? shown * frame_s : end_s;
            const double until = next - elapsed;
            wait = until <= 0 ? 0 : until >= 0.016 ? 16 : static_cast<DWORD>(until * 1000);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, wait, QS_ALLINPUT);
    }
    // The stop and close: the audio stream and the subclass.
    bof3::Log("%s    %s: %s after %.2f s, %u of %u frames shown%s", str ? "DIV-0214" : "DIV-0205", filename,
              shown >= frames ? "ended" : "skipped",
              static_cast<double>(Now() - t0 - playing.paused_ticks) / static_cast<double>(freq.QuadPart), shown, frames,
              faulted ? ", a decode fault held the picture" : "");
    CloseAudio(playing.audio);
    g_playing = nullptr;
    SetWindowLongPtrA(hwnd, GWLP_WNDPROC, saved_proc);
    HeapFree(GetProcessHeap(), 0, data);
}

// original 0x59E570 (read whole, 0xD4 bytes; DIV-0049): the window procedure
// Fmv_Play installs for the length of a video.
//   WM_KEYDOWN, WM_LBUTTONDOWN, WM_RBUTTONDOWN: Fmv_Playing cleared if set
//     (the skip); 0 returned.
//   WM_DESTROY: Fmv_Playing = 0, Game_QuitFlag = 1, PostQuitMessage(0); 0.
//   WM_ACTIVATEAPP: wParam set - SetFocus(hwnd), "resume vfw"; clear -
//     "pause vfw"; 0. (The original; symbols.toml's "falls through to the
//     saved procedure" was wrong - the default case is DefWindowProcA.)
//   MM_MCINOTIFY (0x3B9): Fmv_Playing = 0 when wParam is
//     MCI_NOTIFY_SUCCESSFUL (1), the video's end; 0 either way.
//   else: DefWindowProcA.
// DIV-0049: under DIV-0033 (the game keeps running when the window is not
// in front) the pause and resume are not sent, so the video plays on;
// BOF3X_BACKGROUND=0 keeps the original's pair.
// DIV-0205: the pause and resume stop and start our clock and audio stream
// (Pause) in place of the MCI strings; the video's end is the loop's own,
// so MM_MCINOTIFY never arrives (the case is kept, harmless); WM_PAINT
// draws the current frame again, which MCI's window did for itself.
extern "C" long __stdcall Fmv_WndProc(void* hwnd_, unsigned msg, unsigned wparam, long lparam) {
    HWND hwnd = static_cast<HWND>(hwnd_);
    switch (msg) {
    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (Fmv_Playing) Fmv_Playing = 0;
        return 0;
    case WM_DESTROY:
        Fmv_Playing = 0;
        Game_QuitFlag = 1;
        PostQuitMessage(0);
        return 0;
    case WM_ACTIVATEAPP:
        if (wparam) {
            SetFocus(hwnd);
            if (!WinMain_Background()) Pause(false);
        } else if (!WinMain_Background()) {
            Pause(true);
        }
        return 0;
    case WM_PAINT:
        if (g_playing && g_playing->movie->FramesDecoded()) {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (dc) Draw(dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        return static_cast<long>(DefWindowProcA(hwnd, msg, wparam, static_cast<LPARAM>(lparam)));
    case MM_MCINOTIFY:
        if (wparam == MCI_NOTIFY_SUCCESSFUL) Fmv_Playing = 0;
        return 0;
    default:
        return static_cast<long>(DefWindowProcA(hwnd, msg, wparam, static_cast<LPARAM>(lparam)));
    }
}

void BattleMenuWinFromBattleE7_Inject() {
    // DIVERGENCE DIV-0065: after the self-test, which compares the original's rows.
    {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("BattleEquipLabelsRow", Low32(&g_equip_labels_row),
                         &was, &is, 1);
        bof3::Log("DIV-0065    the battle equip window's stat labels beside their values");
    }
}

void BattleMenuWin_Inject() {
    BOF3_INJECT(BattleMenuWin_ItemListRun);
    BOF3_INJECT(BattleMenuWin_ItemListSlideRight);
    BOF3_INJECT(BattleMenuWin_ItemListSlideLeft);
    BOF3_INJECT(BattleMenuWin_SkillListRun);
    BOF3_INJECT(BattleMenuWin_SkillListSlideIn);
    BOF3_INJECT(BattleMenuWin_EquipRun);
    BOF3_INJECT(BattleMenuWin_EquipSlideTo62);
    BOF3_INJECT(BattleMenuWin_EquipItemsRun);
    BOF3_INJECT(BattleMenuWin_EquipItemsSlideTo98);
    BOF3_INJECT(BattleMenuWin_VerbPairRun);
    BOF3_INJECT(BattleMenu_DrawItemList);
    BOF3_INJECT(BattleMenu_DrawSkillList);
    BOF3_INJECT(BattleEquipWin_Draw);
    BOF3_INJECT(BattleEquipWin_DrawBar);
    BOF3_INJECT(BattleEquipWin_DrawItems);
    BOF3_INJECT(Menu_DrawVerbPair);
    BOF3_INJECT(Field_RunTaskRecords);
    BOF3_INJECT(Window_Alloc);
    BOF3_INJECT(Window_FreeCurrent);
    BOF3_INJECT(Window_ResetAll);
    BOF3_INJECT(Fmv_Play);
    BOF3_INJECT(Fmv_WndProc);
}

void BattleMenuWinFromModeFlow_Inject() {
    {
        // DIV-0175: after the self-test, which compared Capcom's five-field reset.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("WindowResetFull", Low32(&mode_flow::g_reset_full), &was, &is, 1);
        bof3::Log("DIV-0175    the window reset clears +0..+0x1F of every record, as the US disc (on unless the line above says OFF)");
    }
}
