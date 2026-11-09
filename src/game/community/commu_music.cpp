// Moved here by tools/move_funcs.py from rest_4e.cpp (docs/source-layout-round.md).
#include "game/community/commu_music.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/faerie_name.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/rdata_consts.h"
#include "game/rest_4e.h"
#include "game/rest_4e_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::Word;

using HandlerRest4e = void (__cdecl*)();

// The 60-entry table (8 bytes an entry: +0 in use, +1 a kind, +2 / +3 an item
// and its tab, +4 a dword) and its 60 names of 5 bytes; the entry 0x9039F5.
constexpr ::bof3::image::Va kEntries{0x9046D0};

// kEntry: now AreaW0c_TailSub (symbols.toml). u8: the entry the item and track screens are for
// kFacility: now AreaW0c_TailState (symbols.toml). u8: the facility table 0x652AE4's index (R4B's reader)
constexpr ::bof3::image::Va kTrackCounts{0x9048AB};   // u8 at + 8 * kind: the track count of a kind (the records 0x9048B0..)

constexpr U kMemberStride = 0xA4;

constexpr unsigned kMemberCount = 7;

constexpr ::bof3::image::Va kTextRecords{0x904CE0};   // Text_Records (0x20 each)

constexpr ::bof3::image::Va kScriptPool{0x803580};    // Msg_OpenScript's pool: base + u16[base + 2 id]

constexpr ::bof3::image::Va kTracks{0x653098};        // u8 x 40: the list's tracks

// kLabelA: now CommuMusic_LabelA (symbols.toml).
// kLabelB: now CommuMusic_LabelB (symbols.toml).
// kLabelC: now CommuMusic_LabelC (symbols.toml). pointers to three labels
constexpr ::bof3::image::Va kGlyphsA{0x653144};

constexpr ::bof3::image::Va kGlyphsB{0x653148};

constexpr ::bof3::image::Va kGlyphsC{0x65314C};

constexpr rdata::Const kFour{0x5C41C8}, kEight{0x5C41CC};   // the floats 4.0, 8.0 (rdata_consts.h)

unsigned char& BRest4e(U address) { return move_script::At(address)[0]; }

U W(U address) { return move_script::Word(move_script::At(address)); }

U L(U address) { return static_cast<U>(Long(move_script::At(address))); }

U Key(const void* p) { return Low32(p); }

template <class T> T Get(const unsigned char* p, unsigned offset) {
    T v;
    std::memcpy(&v, p + offset, sizeof v);
    return v;
}

template <class T> void Put(unsigned char* p, unsigned offset, T v) { std::memcpy(p + offset, &v, sizeof v); }

float F(int v) { return static_cast<float>(v); }

int S16(U v) { return static_cast<short>(v); }

// jmp / call [table + 4 * byte]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original jumps through the dword after.
void Run(const char* who, U table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_4e.md section 7)",
                    who, byte, index, entries, (unsigned)table);
    reinterpret_cast<HandlerRest4e>(static_cast<std::uintptr_t>(L(table + 4 * index)))();
}

// --- the callees ------------------------------------------------------------------------
void Commit(unsigned size) { SH_CALL(Gfx_CommitPrim)(1, size); }

void Grey(unsigned char* p) { p[4] = p[5] = p[6] = 0x80; }

void OpenMessage(unsigned short id) { SH_CALL(Msg_OpenScript)(id); }

void Box(int x, int y, int w, int h, int flags) { SH_CALL(Menu_DrawBox)(x, y, w, h, flags, static_cast<unsigned char>(Config_WindowColour)); }

}  // namespace

// original 0x45F750: 0x652A70[11] - jmp through CommuMusic_Modes by the mode
// 0x939A3E, unchecked (ours aborts past its 3).
extern "C" void __cdecl CommuMusic_Dispatch(void) {
    Run("CommuMusic_Dispatch (0x45F750)", Key(CommuMusic_Modes), CommuMusic_Modes_count, Rest4b_BoardState,
        "the mode 0x939A3E");
}

// original 0x45F760: CommuMusic_Modes[0] - jmp through CommuMusic_OpenSteps by
// the step 0x939A40, unchecked (ours aborts past its 2).
extern "C" void __cdecl CommuMusic_OpenDispatch(void) {
    Run("CommuMusic_OpenDispatch (0x45F760)", Key(CommuMusic_OpenSteps), CommuMusic_OpenSteps_count,
        Rest4b_BoardStep, "the step 0x939A40");
}

// original 0x45F770: CommuMusic_OpenSteps[0] - the seven members' nine name
// bytes into Text_Records 0..6 (+0, +4, +8), Music_Track kept at 0x675FD0, the
// row 0, the slide 4, the step + 1.
extern "C" void __cdecl CommuMusic_Open(void) {
    for (unsigned i = 0; i < kMemberCount; ++i) {
        const unsigned char* const record = At(::bof3::Img(CharacterRecords_addr) + kMemberStride * i);
        unsigned char* const text = At(::bof3::Img(kTextRecords.v) + 0x20 * i);
        Put<U>(text, 0, Get<U>(record, 0));
        Put<U>(text, 4, Get<U>(record, 4));
        text[8] = record[8];
    }
    CommuMusic_SavedTrack = Music_Track;
    CommuMusic_Track = 0;
    CommuMusic_Slide = 4;
    ++Rest4b_BoardStep;
}

// original 0x45F7D0: CommuMusic_OpenSteps[1] - the slide down one, the list
// drawn at (0x32, 0x28 - 50 slide); at 0 the step 0 and the mode + 1. The
// original's y carries leftovers above its low word, which the list reads
// alone.
extern "C" void __cdecl CommuMusic_SlideIn(void) {
    const auto slide = static_cast<unsigned char>(CommuMusic_Slide - 1);
    CommuMusic_Slide = slide;
    SH_CALL(CommuMusic_DrawList)(0x32, 0x28 - 50 * slide);
    if (CommuMusic_Slide == 0) {
        const unsigned char mode = Rest4b_BoardState;
        Rest4b_BoardStep = 0;
        Rest4b_BoardState = static_cast<unsigned char>(mode + 1);
    }
}

// original 0x45F820: CommuMusic_Modes[1] - jmp through CommuMusic_BrowseSteps
// by the step 0x939A40, unchecked (ours aborts past its 2).
extern "C" void __cdecl CommuMusic_BrowseDispatch(void) {
    Run("CommuMusic_BrowseDispatch (0x45F820)", Key(CommuMusic_BrowseSteps), CommuMusic_BrowseSteps_count,
        Rest4b_BoardStep, "the step 0x939A40");
}

// original 0x45F830: CommuMusic_BrowseSteps[0] - the pad: Input_AutoRepeat of
// the pressed word & 0xA000 moves the row (down 0x2000 past the count wraps to
// 0, up 0x8000 below 0 to the count; the count the byte 0x9048AB + 8 kind, the
// kind the entry's +1); nothing moved, Input_Pressed is read again: 0x20 stops
// a playing track (Music_FadeOutStop(10)) and, the row's track the one playing,
// leaves Music_Track 0xFF, else the step + 1 (play it); 0x40 the step 0 and the
// mode + 1 (close). The list drawn at (0x32, 0x28) on every path. 0x20 and
// 0x40 are Japan's circle and cross, asked through sigma (group HT-2,
// docs/button-tests.md 3).
extern "C" void __cdecl CommuMusic_Browse(void) {
    const unsigned pressed = Input_Pressed & 0xA000u;
    const unsigned char count = BRest4e(::bof3::Img(kTrackCounts.v) + 8u * BRest4e(::bof3::Img(kEntries.v) + 1 + 8u * AreaW0c_TailSub));
    const unsigned moved = SH_CALL(Input_AutoRepeat)(pressed);
    if ((moved & 0xFFFF) != 0) {
        if (moved & 0x2000) {
            const auto row = static_cast<std::uint16_t>(static_cast<std::uint32_t>(static_cast<std::uint16_t>(CommuMusic_Track)) + 1);
            CommuMusic_Track = row;
            if (static_cast<short>(row) > static_cast<short>(count)) {
                CommuMusic_Track = 0;
                SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
                return;
            }
        } else if (moved & 0x8000) {
            const auto row = static_cast<std::uint16_t>(static_cast<std::uint32_t>(static_cast<std::uint16_t>(CommuMusic_Track)) - 1);
            CommuMusic_Track = row;
            if (static_cast<short>(row) < 0) {
                CommuMusic_Track = count;
                SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
                return;
            }
        }
    } else {
        const unsigned now = Input_Pressed & 0xFF;
        if (now & Sigma(0x20)) {
            if (Music_Track != 0xFF) SH_CALL(Music_FadeOutStop)(10);
            const unsigned track = BRest4e(::bof3::Img(kTracks.v) + TrackIndex());
            if (Music_Track == track) {
                Music_Track = 0xFF;
            } else {
                ++Rest4b_BoardStep;
            }
            SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
            return;
        }
        if (now & Sigma(0x40)) {
            const unsigned char mode = Rest4b_BoardState;
            Rest4b_BoardStep = 0;
            Rest4b_BoardState = static_cast<unsigned char>(mode + 1);
        }
    }
    SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
}

// original 0x45F950: CommuMusic_BrowseSteps[1] - once File_LoadDone: the row's
// track played (Music_Play(track, 8)) and the step 0; the list drawn.
extern "C" void __cdecl CommuMusic_Play(void) {
    if (SH_CALL(File_LoadDone)() != 0) {
        const unsigned track = BRest4e(::bof3::Img(kTracks.v) + TrackIndex());
        SH_CALL(Music_Play)(track, 8);
        Rest4b_BoardStep = 0;
    }
    SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
}

// original 0x45F990: CommuMusic_Modes[2] - jmp through CommuMusic_CloseSteps by
// the step 0x939A40, unchecked (ours aborts past its 4).
extern "C" void __cdecl CommuMusic_CloseDispatch(void) {
    Run("CommuMusic_CloseDispatch (0x45F990)", Key(CommuMusic_CloseSteps), CommuMusic_CloseSteps_count,
        Rest4b_BoardStep, "the step 0x939A40");
}

// original 0x45F9A0: CommuMusic_CloseSteps[0] - a track playing that is not the
// one kept: Music_FadeOutStop(10); the list drawn; the step + 1.
extern "C" void __cdecl CommuMusic_CloseFade(void) {
    const unsigned char track = Music_Track;
    if (track != 0xFF && track != CommuMusic_SavedTrack) SH_CALL(Music_FadeOutStop)(10);
    SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
    ++Rest4b_BoardStep;
}

// original 0x45F9E0: CommuMusic_CloseSteps[1] - once File_LoadDone: the kept
// track played again (Music_Play(kept, 8)), the step + 1, the slide 0; the list
// drawn.
extern "C" void __cdecl CommuMusic_CloseRestore(void) {
    if (SH_CALL(File_LoadDone)() != 0) {
        SH_CALL(Music_Play)(CommuMusic_SavedTrack, 8);
        const unsigned char step = Rest4b_BoardStep;
        CommuMusic_Slide = 0;
        Rest4b_BoardStep = static_cast<unsigned char>(step + 1);
    }
    SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
}

// original 0x45FA20: CommuMusic_CloseSteps[2] - the slide up one, the list
// drawn at (0x32, 0x28 - 50 slide); at 4 message 0x5A, the step + 1,
// Field_Request 2 (CommuMusic_CloseSteps[3] is R4D's 0x45E6A0).
extern "C" void __cdecl CommuMusic_SlideOut(void) {
    const auto slide = static_cast<unsigned char>(CommuMusic_Slide + 1);
    CommuMusic_Slide = slide;
    SH_CALL(CommuMusic_DrawList)(0x32, 0x28 - 50 * slide);
    if (CommuMusic_Slide == 4) {
        OpenMessage(0x5A);
        const unsigned char step = Rest4b_BoardStep;
        Field_Request = 2;
        Rest4b_BoardStep = static_cast<unsigned char>(step + 1);
    }
}

// original 0x45FA80: the track list at (x, y): a box 0xDC x 0x6E flags 0xF0,
// three notched outlines, the row's title (script message 0x271 + row) at (x +
// 0x14, y + 0x17) in colour 2 when its track is the one playing, else 0; a box
// and outline 0x7C x 0xE at (x + 0x48, y + 0x3C); two labels of the font quads
// (CommuMusic_DrawLabel; the first by 0x903A59) and four Text_DrawFont8 strings
// at y + 0x54; then two POLY_FT4s at (x + 0x49, y + 0x3F + 4 k), whose width
// the original adds from two zeroed ints of its stack - 0: both quads are a
// line (docs/rest_4e.md section 7, L3).
extern "C" void __cdecl CommuMusic_DrawList(int x, int y) {
    Box(x, y, 0xDC, 0x6E, 0xF0);
    SH_CALL(Menu_DrawOutlineNotched)(x + 4, y + 4, 0xD2, 0x66, 0);
    SH_CALL(Menu_DrawOutlineNotched)(x + 8, y + 8, 0xCA, 0x5E, 0);
    SH_CALL(Menu_DrawOutlineNotched)(x + 0x10, y + 0x12, 0xBC, 0x22, 1);
    const U row = TrackIndex();
    const int colour = Music_Track == BRest4e(::bof3::Img(kTracks.v) + row) ? 2 : 0;
    SH_CALL(Text_DrawAt)(x + 0x14, y + 0x17, colour, 0xFF, At(::bof3::Img(kScriptPool.v) + W(::bof3::Img(kScriptPool.v) + 2 * (0x271 + row))));
    Box(x + 0x48, y + 0x3C, 0x7C, 0xE, 1);
    SH_CALL(Menu_DrawOutlineNotched)(x + 0x48, y + 0x3C, 0x7C, 0xE, 1);
    const U label = Rest4f_Setting3 == 0 ? CommuMusic_LabelA : CommuMusic_LabelB;
    SH_CALL(CommuMusic_DrawLabel)(x + 0x10, y + 0x40, 0, At(label));
    SH_CALL(CommuMusic_DrawLabel)(x + 0x20, y + 0x54, 0, At(CommuMusic_LabelC));
    SH_CALL(Text_DrawFont8)(x + 0x44, y + 0x54, 0, At(::bof3::Img(kGlyphsC.v)));
    SH_CALL(Text_DrawFont8)(x + 0x5C, y + 0x54, 3, At(::bof3::Img(kGlyphsB.v)));
    SH_CALL(Text_DrawFont8)(x + 0x94, y + 0x54, 0, At(::bof3::Img(kGlyphsC.v)));
    SH_CALL(Text_DrawFont8)(x + 0xAC, y + 0x54, 3, At(::bof3::Img(kGlyphsA.v)));
    const float left = F(S16(static_cast<U>(x)) + 0x49);
    const int widths[2] = {0, 0};
    int top = S16(static_cast<U>(y)) + 0x3F;
    for (unsigned k = 0; k < 2; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        const float right = F(widths[k]) + left;
        Put<float>(p, 8, left);
        Put<float>(p, 0x28, left);
        Grey(p);
        p[0x14] = 0x98;
        Put<float>(p, 0x18, right);
        Put<float>(p, 0x38, right);
        const float fy = F(top);
        p[0x24] = 0x9C;
        p[0x34] = 0x98;
        p[0x44] = 0x9C;
        p[0x15] = 0xD8;
        Put<float>(p, 0xC, fy);
        Put<float>(p, 0x1C, fy);
        const float fy4 = fy + Get<float>(At(kFour), 0);
        p[0x25] = 0xD8;
        p[0x35] = 0xE0;
        p[0x45] = 0xE0;
        Put<std::uint16_t>(p, 0x16, 0x780B);
        Put<std::uint16_t>(p, 0x26, 0xF);
        Put<float>(p, 0x2C, fy4);
        Put<float>(p, 0x3C, fy4);
        Commit(0x48);
        top += 4;
    }
}

// original 0x45FCE0: a label in the 8 x 8 font quads at (x, y): each byte of
// `text` to its NUL a POLY_FT4 8 x 8 slanted 4 to the left at the bottom
// ((x + 4, y), (x + 0xC, y), (x, y + 8), (x + 8, y + 8)), u = ((c - 0x20) %
// 32) 8, v = ((c - 0x20) / 32) 8 (C's signed division), clut 0x7800 | colour &
// 0x3F, texture page 0xF; a space draws nothing; each next byte 8 to the right
// (x read again as its low word, signed, each time).
extern "C" void __cdecl CommuMusic_DrawLabel(int x, int y, unsigned colour, const unsigned char* text) {
    const auto clut = static_cast<std::uint16_t>(0x7800 | (colour & 0x3F));
    const float four = Get<float>(At(kFour), 0), eight = Get<float>(At(kEight), 0);
    for (int pen = x; text[0] != 0; ++text, pen += 8) {
        if (text[0] == 0x20) continue;
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        const unsigned char c = text[0];
        const float fx = F(S16(static_cast<U>(pen)) + 4);
        Put<float>(p, 8, fx);
        const float fx8 = fx + eight;
        Put<float>(p, 0x18, fx8);
        Put<float>(p, 0x28, fx - four);
        Put<float>(p, 0x38, fx8 - four);
        const float fy = F(S16(static_cast<U>(y)));
        Put<float>(p, 0xC, fy);
        Put<float>(p, 0x1C, fy);
        Put<float>(p, 0x2C, fy + eight);
        Put<float>(p, 0x3C, fy + eight);
        const int code = static_cast<int>(c) - 0x20;
        const auto u = static_cast<unsigned char>((code % 32) << 3);
        const auto v = static_cast<unsigned char>((code / 32) << 3);
        p[0x14] = u;
        p[0x24] = static_cast<unsigned char>(u + 7);
        p[0x34] = u;
        p[0x44] = static_cast<unsigned char>(u + 7);
        p[0x15] = v;
        p[0x25] = v;
        p[0x35] = static_cast<unsigned char>(v + 8);
        p[0x45] = static_cast<unsigned char>(v + 8);
        Grey(p);
        Put<std::uint16_t>(p, 0x16, clut);
        Put<std::uint16_t>(p, 0x26, 0xF);
        Commit(0x48);
    }
}

// original 0x45FE00: 0x652A70[18] - call through CommuItem_Modes by the mode
// 0x939A3E, unchecked (ours aborts past its 2); then, Field_Request 0 (read
// after the call), Field_RunTaskRecords (a tail jmp).
extern "C" void __cdecl CommuItem_Frame(void) {
    Run("CommuItem_Frame (0x45FE00)", Key(CommuItem_Modes), CommuItem_Modes_count, Rest4b_BoardState,
        "the mode 0x939A3E");
    if (Field_Request == 0) SH_CALL(Field_RunTaskRecords)();
}

// original 0x45FE20: CommuItem_Modes[0] - jmp through CommuItem_Steps by the
// step 0x939A40, unchecked (ours aborts past its 7).
extern "C" void __cdecl CommuItem_StepDispatch(void) {
    Run("CommuItem_StepDispatch (0x45FE20)", Key(CommuItem_Steps), CommuItem_Steps_count, Rest4b_BoardStep,
        "the step 0x939A40");
}

// original 0x45FE30: CommuItem_Steps[0] - no message open: message 0x60, the
// step + 1, Field_Request 2.
extern "C" void __cdecl CommuItem_Prompt(void) {
    if (Field_Request == 2) return;
    OpenMessage(0x60);
    const unsigned char step = Rest4b_BoardStep;
    Field_Request = 2;
    Rest4b_BoardStep = static_cast<unsigned char>(step + 1);
}

void CommuMusic_Inject() {
    BOF3_INJECT(CommuMusic_Dispatch);
    BOF3_INJECT(CommuMusic_OpenDispatch);
    BOF3_INJECT(CommuMusic_Open);
    BOF3_INJECT(CommuMusic_SlideIn);
    BOF3_INJECT(CommuMusic_BrowseDispatch);
    BOF3_INJECT(CommuMusic_Browse);
    BOF3_INJECT(CommuMusic_Play);
    BOF3_INJECT(CommuMusic_CloseDispatch);
    BOF3_INJECT(CommuMusic_CloseFade);
    BOF3_INJECT(CommuMusic_CloseRestore);
    BOF3_INJECT(CommuMusic_SlideOut);
    BOF3_INJECT(CommuMusic_DrawList);
    BOF3_INJECT(CommuMusic_DrawLabel);
    BOF3_INJECT(CommuItem_Frame);
    BOF3_INJECT(CommuItem_StepDispatch);
    BOF3_INJECT(CommuItem_Prompt);
}
