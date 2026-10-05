// Round fourteen group R4F (docs/rest_4f.md): 52 functions in 0x460CB0..0x464B9E -
// the cut's 50 rows for R4F (analysis/round14_cut.tsv), the Config screen's
// controller step 0x4613B0 no list had, and kind 2's 0x464970 (a table cell and
// two states' shared tail) - each read with capstone to its last instruction
// (2026-10-05). Two things live in the band, neither the community village:
//
//   the Config screen   field menu state 7's own machine (ConfigMenu_Body
//               0x590340 jumps to ConfigScreen_Run): its opening, the top bar's
//               two buttons, the six rows, the controller panel, its closing,
//               and the panel draws (docs/config-screen.md has the text);
//               WorldMap_ExitRecords, the exit list Field_ExitFromCell walks
//   effect kinds 2, 7, 8, 9 and 0xB   the states E1A's dispatchers jump to and
//               the draws they call: kind 7 a full sprite fading (its states 2..4),
//               kind 8 grey lines along the ground to an object, kind 9 a fan
//               that opens blade by blade and spins, kind 0xB a quad that rises
//               from the first extra sprite and follows it, kind 2 a panel that
//               slides in and a copy of the leader's sprite recoloured
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies; Sprite_Current
// and the menu block are read again wherever the original reads them again
// after a call. The call sites and operands DIV-0011, DIV-0017, DIV-0026 and
// DIV-0051 rewrite inside these functions are read in place at every call, so
// each divergence holds for ours as it does for Capcom's code
// (docs/rest_4f.md section 6). The x87 arithmetic is done in long double, as the
// originals' fild / fsub / fadd / fstp chains are (rest_3f.cpp's form). Where the
// original indexes past a table of handlers or of records, ours aborts with a
// message (docs/rest_4f.md section 7).
#include "game/rest_4f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_4f_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_4f::at;
using U = std::uint32_t;
using LD = long double;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = void (__cdecl*)();
using Frame = void (__cdecl*)(int, int, int, int);
using DrawText = const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*);

unsigned char* S() { return Sprite_Current; }
unsigned char& B(U a) { return At(a)[0]; }
U L(U a) { return static_cast<U>(Long(At(a))); }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
std::int32_t SL(const unsigned char* p) { return Long(p); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U W(U a) { return Word(At(a)); }
void SetW(U a, U v) { SetWord(At(a), v); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// Where a call site sends its call now: Capcom's callee, or what a divergence
// re-aimed it at (bof3::RetargetCall).
U SiteTarget(U site) { return site + 5 + L(site + 1); }

// --- x87 as the original has it (rest_3f.cpp's form): `fld dword` through inline
// assembly, every operation in long double, `fstp dword` out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}
LD I(U v) { return static_cast<LD>(static_cast<std::int32_t>(v)); }
void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// jmp / call [table + byte * 4]: the table's `entries` handlers read in place (the
// fuzz swaps the cells for recorders); a Fatal past them, where the original
// goes through the dword after - the next table's cell or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u handlers of 0x%X - the original jumps through the dword after "
                    "(docs/rest_4f.md section 7)",
                    who, byte, index, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * index)))();
}

// ===========================================================================
// The Config screen
// ===========================================================================

void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
void Backdrop() { SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop)); }
// The banner box: (0x14, y, 0x118, 0x13) in the window style.
void TitleBox(int y) { SH_CALL(Menu_DrawTitleBox)(0x14, y, 0x118, 0x13, B(at::kStyle)); }
// The banner's text, system message 0xBC + the cursor (s8), at (0x1C, 0x13).
void TitleText(U id) {
    const unsigned char* const text = SH_CALL(Msg_SystemPtr)(id);
    SH_CALL(Text_DrawAt)(0x1C, 0x13, 0, 0xFF, text);
}
U CursorId() { return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kCursor)))) + 0xBC; }
int CursorS() { return static_cast<signed char>(B(at::kCursor)); }
// The top bar's hand: x = cursor * 48 + 0x76.
void TopHand() { SH_CALL(Menu_DrawHand)(CursorS() * 48 + 0x76, 0x2A, 0); }
U Repeat() { return SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xF000u); }
// The cursor's row state for the panel's row draws: the counter byte on the
// row under the cursor (cursor - 2), 0 on the others; 0x929F04 read as a dword.
U RowState(unsigned row) {
    const U d = L(at::kCounter);
    const int cursor = static_cast<signed char>(static_cast<unsigned char>(d >> 8)) - 2;
    return cursor == static_cast<int>(row) ? (d & 0xFF) : 0;
}
void SetDefaults() {
    B(at::kStyle) = 0;
    B(at::kBackdrop) = 0;
    B(at::kSetting3) = 0;
    B(at::kSetting5C) = 0;
    B(at::kSetting5D) = 0;
    B(at::kSetting4) = 0;
    B(at::kSetting0) = 1;
    SetW(at::kButtons, 0x23);
    SetW(at::kButtons + 2, 0x80);
    SetW(at::kButtons + 0xC, 0x40);
    SetW(at::kButtons + 4, 0x10);
    SetW(at::kButtons + 6, 8);
    SetW(at::kButtons + 8, 4);
}

}  // namespace

// original 0x460CB0 (ConfigMenu_Body's tail jump; hidden in 0x460C40): jmp
// [ConfigScreen_States + 0x929F02 * 4] - 0 the opening, 1 the top bar, 2 the
// rows, 3 the controller panel, 4 the closing. Unbounded: ours aborts past 5.
extern "C" void __cdecl ConfigScreen_Run(void) {
    Dispatch("ConfigScreen_Run", at::kConfigStates, at::kConfigStateCount, B(at::kState), "the state byte 0x929F02");
}

// original 0x460CC0 (ConfigScreen_States[0]): jmp [ConfigScreen_OpenStates +
// 0x929F03 * 4] - 0 a fade started, 1 its wait, 2 the slide in
// (ConfigMenu_Open sets 2). Unbounded: ours aborts past 3.
extern "C" void __cdecl ConfigScreen_OpenRun(void) {
    Dispatch("ConfigScreen_OpenRun", at::kConfigOpenStates, at::kConfigOpenCount, B(at::kOpenState),
             "the opening byte 0x929F03");
}

// original 0x460CD0 (ConfigScreen_OpenStates[0]): Transition_Start(1); the
// cursor 0; 0x929F03 up one (read after the call).
extern "C" void __cdecl ConfigScreen_OpenFade(void) {
    SH_CALL(Transition_Start)(1);
    const unsigned char n = B(at::kOpenState);
    B(at::kCursor) = 0;
    B(at::kOpenState) = static_cast<unsigned char>(n + 1);
}

// original 0x460CF0 (ConfigScreen_OpenStates[1]): the screen drawn at rest
// (the backdrop, the banner box, the panel at (0x1C, 0x40), the top bar with no
// button lit); once MoveScript_WaitWordDA is 0, the counter and 0x929F03 0 and
// the state on.
extern "C" void __cdecl ConfigScreen_OpenWait(void) {
    Backdrop();
    TitleBox(0x10);
    SH_CALL(Config_DrawPanel)(0x1C, 0x40);
    SH_CALL(ConfigScreen_DrawButtons)(0xFF, 0x70, 0x26);
    if (MoveScript_WaitWordDA != 0) return;
    const unsigned char st = B(at::kState);
    B(at::kCounter) = 0;
    B(at::kOpenState) = 0;
    B(at::kState) = static_cast<unsigned char>(st + 1);
}

// original 0x460D50 (ConfigScreen_OpenStates[2]): the cursor 0; sound 0x102 when
// the counter is 5; the screen drawn slid by the counter c (read again after
// each draw): the banner box at y 0x10 - 16c, the panel at x 0x1C + 64c, the top
// bar at y 0x26 - 16c; the counter down one, at 0 the state on (0x929F03 0).
extern "C" void __cdecl ConfigScreen_SlideIn(void) {
    const unsigned char c0 = B(at::kCounter);
    B(at::kCursor) = 0;
    if (c0 == 5) Sound(0x102);
    Backdrop();
    TitleBox(0x10 - static_cast<int>(B(at::kCounter)) * 16);
    SH_CALL(Config_DrawPanel)(static_cast<int>(B(at::kCounter)) * 64 + 0x1C, 0x40);
    SH_CALL(ConfigScreen_DrawButtons)(0xFF, 0x70, 0x26 - static_cast<int>(B(at::kCounter)) * 16);
    const unsigned char n = static_cast<unsigned char>(B(at::kCounter) - 1);
    B(at::kCounter) = n;
    if (n != 0) return;
    const unsigned char st = B(at::kState);
    B(at::kCounter) = 0;
    B(at::kOpenState) = 0;
    B(at::kState) = static_cast<unsigned char>(st + 1);
}

// original 0x460E10 (ConfigScreen_States[1], and the cell 0x63F298): the top
// bar. The counter 0; the screen drawn, the banner's text system message 0xBC +
// the cursor, the hand at x = cursor * 48 + 0x76. Input_AutoRepeat of
// Input_Pressed & 0xF000: 0xA000 flips the cursor between 0 and 1 (sound
// 0x101); 0x1000 enters the rows at cursor 7, 0x4000 at cursor 2 (sound 0x100).
// Then Input_Pressed (a dword read once): a confirm button on cursor 0 leaves
// (sounds 0x106 and 0x102 unless Game_Step is 4; the state + 3, the counter and
// 0x929F03 0); on cursor 1 the defaults (sound 0x104, the top bar and the hand
// drawn again): the settings 0 (row 0's 1), six button words. Else, at Game_Step
// 4, bit 0x800 leaves without a sound; elsewhere a cancel button leaves with the
// two sounds.
extern "C" void __cdecl ConfigScreen_TopBar(void) {
    const unsigned char bd = B(at::kBackdrop);
    B(at::kCounter) = 0;
    SH_CALL(Menu_DrawBackdrop)(bd);
    TitleBox(0x10);
    TitleText(CursorId());
    SH_CALL(Config_DrawPanel)(0x1C, 0x40);
    SH_CALL(ConfigScreen_DrawButtons)(0xFF, 0x70, 0x26);
    TopHand();
    const U rep = Repeat();
    if (rep & 0xA000) {
        Sound(0x101);
        B(at::kCursor) = B(at::kCursor) == 0 ? 1 : 0;
    }
    if (rep & 0x1000) {
        Sound(0x100);
        const unsigned char st = B(at::kState);
        B(at::kCursor) = 7;
        B(at::kState) = static_cast<unsigned char>(st + 1);
        return;
    }
    if (rep & 0x4000) {
        Sound(0x100);
        const unsigned char st = B(at::kState);
        B(at::kCursor) = 2;
        B(at::kState) = static_cast<unsigned char>(st + 1);
        return;
    }
    const U pressed = L(at::kPressedDword);
    if (Field_ConfirmButtons & pressed & 0xFFFFu) {
        if (B(at::kCursor) == 0) {
            if (Game_Step != 4) {
                Sound(0x106);
                Sound(0x102);
            }
            const unsigned char st = B(at::kState);
            B(at::kOpenState) = 0;
            B(at::kCounter) = 0;
            B(at::kState) = static_cast<unsigned char>(st + 3);
            return;
        }
        Sound(0x104);
        SH_CALL(ConfigScreen_DrawButtons)(B(at::kCursor), 0x70, 0x26);
        TopHand();
        SetDefaults();
        return;
    }
    if (Game_Step == 4) {
        if (!(pressed & 0x800)) return;
    } else {
        if (!(Field_CancelButtons & pressed & 0xFFFFu)) return;
        Sound(0x106);
        Sound(0x102);
    }
    const unsigned char st = B(at::kState);
    B(at::kCounter) = 0;
    B(at::kOpenState) = 0;
    B(at::kState) = static_cast<unsigned char>(st + 3);
}

namespace {

// A setting stepped by 0x2000 (up) or 0x8000 (down), wrapping within 0..top
// (signed compares, as the original's jle / jns); `down_first` tests 0x8000
// first (row 2's order).
void StepSetting(U cell, int top, bool down_first) {
    const U p = L(at::kPressedDword);
    const auto up = [&] {
        Sound(0x101);
        const unsigned char v = static_cast<unsigned char>(B(cell) + 1);
        B(cell) = v;
        if (static_cast<signed char>(v) > top) B(cell) = 0;
    };
    const auto down = [&] {
        Sound(0x101);
        const unsigned char v = static_cast<unsigned char>(B(cell) - 1);
        B(cell) = v;
        if (static_cast<signed char>(v) < 0) B(cell) = static_cast<unsigned char>(top);
    };
    if (down_first) {
        if (p & 0x8000) down();
        else if (p & 0x2000) up();
    } else {
        if (p & 0x2000) up();
        else if (p & 0x8000) down();
    }
}

}  // namespace

// original 0x461070 (ConfigScreen_States[2]): the six rows. The screen drawn,
// the banner's text by the cursor, the top bar with the cursor lit. By the
// cursor - 2 (a jump table of six in the function, bounded): rows 0..2 step
// their setting (0x903A58 in 0..2, 0x903A5A and 0x903A5B in 0..3; row 2 tests
// down first), rows 3 and 4 flip theirs on 0xA000 (Input_Pressed as read before
// the switch), row 5 on a confirm button or 0xA000 opens the controller panel
// (the state + 1, cursor 8). Then Input_AutoRepeat: 0x1000 the cursor down one,
// below 2 back to the top bar (the state - 1, cursor 0); 0x4000 up one, past 7
// the same; either sets the counter to 1. Else a cancel button or 0x800 back to
// the top bar (sound 0x106), and the counter up one while below 3.
extern "C" void __cdecl ConfigScreen_Rows(void) {
    Backdrop();
    TitleBox(0x10);
    TitleText(CursorId());
    SH_CALL(Config_DrawPanel)(0x1C, 0x40);
    SH_CALL(ConfigScreen_DrawButtons)(B(at::kCursor), 0x70, 0x26);
    const U row = static_cast<U>(CursorS() - 2);
    if (row <= 5) {
        const U held = L(at::kPressedDword);
        switch (row) {
        case 0: StepSetting(at::kSetting0, 2, false); break;
        case 1: StepSetting(at::kStyle, 3, false); break;
        case 2: StepSetting(at::kBackdrop, 3, true); break;
        case 3:
            if (held & 0xA000) {
                Sound(0x101);
                B(at::kSetting3) = static_cast<unsigned char>(B(at::kSetting3) ^ 1);
            }
            break;
        case 4:
            if (held & 0xA000) {
                Sound(0x101);
                B(at::kSetting4) = static_cast<unsigned char>(B(at::kSetting4) ^ 1);
            }
            break;
        default: {
            const U buttons = Field_ConfirmButtons | 0xA000u;
            if (Input_Pressed & buttons) {
                Sound(0x101);
                const unsigned char st = B(at::kState);
                B(at::kCursor) = 8;
                B(at::kState) = static_cast<unsigned char>(st + 1);
                return;
            }
            break;
        }
        }
    }
    const U rep = Repeat();
    if (rep & 0x1000) {
        Sound(0x100);
        const unsigned char c = static_cast<unsigned char>(B(at::kCursor) - 1);
        B(at::kCursor) = c;
        if (static_cast<signed char>(c) < 2) {
            const unsigned char st = B(at::kState);
            B(at::kCursor) = 0;
            B(at::kState) = static_cast<unsigned char>(st - 1);
        }
        B(at::kCounter) = 1;
        return;
    }
    if (rep & 0x4000) {
        Sound(0x100);
        const unsigned char c = static_cast<unsigned char>(B(at::kCursor) + 1);
        B(at::kCursor) = c;
        if (static_cast<signed char>(c) > 7) {
            const unsigned char st = B(at::kState);
            B(at::kCursor) = 0;
            B(at::kState) = static_cast<unsigned char>(st - 1);
        }
        B(at::kCounter) = 1;
        return;
    }
    const U cancel = Field_CancelButtons | 0x800u;
    if (Input_Pressed & cancel) {
        Sound(0x106);
        const unsigned char st = B(at::kState);
        B(at::kCursor) = 0;
        B(at::kState) = static_cast<unsigned char>(st - 1);
    }
    const unsigned char n = B(at::kCounter);
    if (n < 3) B(at::kCounter) = static_cast<unsigned char>(n + 1);
}

// original 0x4613B0 (ConfigScreen_States[3]; in no list of the cut, inside the
// span the catalog gave 0x461070): the controller panel. The screen drawn, the
// banner's text system message 0xBC + the cursor, or on cursor 9 0xC5 / 0xCA by
// row 4's setting; Config_DrawControllerPanel(0xA8, 0x62); the hand at (0xB0,
// 18 * cursor - 0x26). A face button (Input_Pressed's low byte & 0xFC, its lowest
// bit kept; sound 0x103): bits 0 and 1 cleared from the words 0x903580 and
// Field_ConfirmButtons; the row whose word holds the bit takes the cursor row's
// word, the cursor row's word becomes the bit; bits 0 and 1 set again. Then
// Input_AutoRepeat: 0x1000 / 0x4000 the cursor down / up through 8..13,
// wrapping with a second sound; else 0xA000 back to the rows at cursor 7 (sound
// 0x101), 0x800 to the top bar (the state - 2, cursor 0, sound 0x106).
extern "C" void __cdecl ConfigScreen_Controller(void) {
    Backdrop();
    TitleBox(0x10);
    SH_CALL(Config_DrawPanel)(0x1C, 0x40);
    SH_CALL(ConfigScreen_DrawButtons)(B(at::kCursor), 0x70, 0x26);
    const unsigned char cur = B(at::kCursor);
    const U id = cur == 9 ? (B(at::kSetting4) != 0 ? 0xCAu : 0xC5u)
                          : static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(cur))) + 0xBC;
    TitleText(id);
    SH_CALL(Config_DrawControllerPanel)(0xA8, 0x62);
    SH_CALL(Menu_DrawHand)(0xB0, CursorS() * 18 - 0x26, 0);
    U bits = Input_Pressed & 0xFCu;
    if (bits != 0) {
        Sound(0x103);
        SetW(at::kButtons, W(at::kButtons) & 0xFFFC);
        Field_ConfirmButtons = static_cast<unsigned short>(Field_ConfirmButtons & 0xFFFC);
        unsigned i = 0;
        for (; i < 16; ++i)
            if (bits & (1u << i)) break;
        if (i < 16)
            for (++i; i < 16; ++i) bits &= ~(1u << i);
        unsigned r = 0;
        for (; r < at::kPadRowCount; ++r)
            if (W(at::kButtons + 2u * B(at::kPadRows + r)) & bits) break;
        const unsigned char c = B(at::kCursor);
        const U by_cursor = at::kPadRowsByCursor + static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(c)));
        if (r != at::kPadRowCount) {
            const U from = at::kButtons + 2u * B(by_cursor);
            const U to = at::kButtons + 2u * B(at::kPadRows + r);
            SetW(to, W(from));
        }
        SetW(at::kButtons + 2u * B(by_cursor), bits);
        SetW(at::kButtons, W(at::kButtons) | 3);
        Field_ConfirmButtons = static_cast<unsigned short>(Field_ConfirmButtons | 3);
    }
    const U rep = Repeat();
    if (rep & 0x1000) {
        Sound(0x100);
        const unsigned char c = static_cast<unsigned char>(B(at::kCursor) - 1);
        B(at::kCursor) = c;
        if (static_cast<signed char>(c) >= 8) return;
        Sound(0x100);
        B(at::kCursor) = 0xD;
        return;
    }
    if (rep & 0x4000) {
        Sound(0x100);
        const unsigned char c = static_cast<unsigned char>(B(at::kCursor) + 1);
        B(at::kCursor) = c;
        if (static_cast<signed char>(c) <= 0xD) return;
        Sound(0x100);
        B(at::kCursor) = 8;
        return;
    }
    const U p = L(at::kPressedDword);
    if (p & 0xA000) {
        Sound(0x101);
        const unsigned char st = B(at::kState);
        B(at::kCursor) = 7;
        B(at::kState) = static_cast<unsigned char>(st - 1);
        return;
    }
    if (p & 0x800) {
        Sound(0x106);
        const unsigned char st = B(at::kState);
        B(at::kCursor) = 0;
        B(at::kState) = static_cast<unsigned char>(st + 0xFE);
    }
}

// original 0x461620 (ConfigScreen_States[4]): the closing. The backdrop; at
// Game_Step 4 (the title's Config) the screen drawn at rest, Transition_Start(0)
// and Game_Step 5; else the screen slid by the counter c (read again after each
// draw, as the slide in) with no button lit, the counter up one, at 5 the
// screen's step 0x929F01 up one (ConfigMenu_Close).
extern "C" void __cdecl ConfigScreen_SlideOut(void) {
    Backdrop();
    if (Game_Step == 4) {
        TitleBox(0x10);
        SH_CALL(Config_DrawPanel)(0x1C, 0x40);
        SH_CALL(ConfigScreen_DrawButtons)(0, 0x70, 0x26);
        SH_CALL(Transition_Start)(0);
        Game_Step = 5;
        return;
    }
    TitleBox(0x10 - static_cast<int>(B(at::kCounter)) * 16);
    SH_CALL(Config_DrawPanel)(static_cast<int>(B(at::kCounter)) * 64 + 0x1C, 0x40);
    SH_CALL(ConfigScreen_DrawButtons)(0, 0x70, 0x26 - static_cast<int>(B(at::kCounter)) * 16);
    const unsigned char n = static_cast<unsigned char>(B(at::kCounter) + 1);
    B(at::kCounter) = n;
    if (n == 5) B(at::kScreenStep) = static_cast<unsigned char>(B(at::kScreenStep) + 1);
}

// original 0x4616F0 (cdecl (sel, x, y)): Menu_DrawButtonRow(x, y, set 6, sel,
// the style byte - the callee's unused fifth).
extern "C" void __cdecl ConfigScreen_DrawButtons(int sel, int x, int y) {
    SH_CALL(Menu_DrawButtonRow)(x, y, 6, sel, B(at::kStyle));
}

// original 0x461710 (cdecl (x, y)): the panel. The five settings read first (rows
// 0..4: 0x903A58, 0x903A5A, 0x903A5B, 0x903A59, 0x903A5E; row 5 none); the frame
// (x, y, 0x21, 0xD) through the call at 0x461778 (DIV-0011's Menu_DrawFrame, read
// in place); then for each row at (x + 5, y + 9 + 15 * row): Config_DrawRowLabel
// and Config_DrawRowOptions, each with the row's state (RowState, read afresh).
extern "C" void __cdecl Config_DrawPanel(int x, int y) {
    const unsigned char settings[6] = {B(at::kSetting0), B(at::kStyle), B(at::kBackdrop), B(at::kSetting3), B(at::kSetting4), 0};
    SH_AT(Frame, SiteTarget(at::kPanelFrameSite))(x, y, 0x21, 0xD);
    const int x5 = x + 5;
    int row_y = y + 9;
    for (unsigned row = 0; row < 6; ++row) {
        SH_CALL(Config_DrawRowLabel)(x5, row_y, row, RowState(row));
        const U state = RowState(row);
        SH_CALL(Config_DrawRowOptions)(x5, row_y, row, settings[row], state);
        row_y += 0xF;
    }
}

// original 0x461970 (cdecl (x, y, row, setting, state)): a row's options. The
// row's byte names its first record and its count (0x653808 / 0x653810, read in
// place; the count read again each pass); each 16-byte record (a count, an s8
// x, the string): colour 2 for the option the setting names, else 0 (in the row
// argument's low byte); x = the record's x (as a 16-bit word) - half the width +
// x + 0x74. The chosen option with the state 3 is drawn large - the width count
// * 12, or * 8 under DIV-0017 (the `lea` at 0x4619E1 read in place), through the
// call at 0x4619F9 (Text_DrawAt, or ConfigText_DrawSelected) at y - 2; the
// rest small (count * 8, Text_DrawSmall) at y + 1.
extern "C" void __cdecl Config_DrawRowOptions(int x, int y, unsigned row, unsigned setting, unsigned state) {
    const U r = row & 0xFF;
    const U first = B(at::kOptionFirst + r);
    if (B(at::kOptionCount + r) == 0) return;
    U rec = at::kOptionRecords + first * 16;
    const unsigned chosen = setting & 0xFF;
    for (unsigned i = 0;;) {
        const U colour = (row & 0xFFFFFF00u) | (chosen == i ? 2u : 0u);
        const unsigned char n = B(rec);
        const U rx = static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<signed char>(B(rec + 1))));
        const unsigned char* const text = At(rec + 2);
        if (chosen == i && state == 3) {
            static const unsigned char kTwelve[] = {0x8D, 0x04, 0x40};   // lea eax,[eax+eax*2]
            static const unsigned char kEight[] = {0xD1, 0xE0, 0x90};    // DIV-0017: shl eax,1 / nop
            U width;
            if (std::memcmp(At(at::kOptionWidth), kTwelve, sizeof kTwelve) == 0) width = n * 12u;
            else if (std::memcmp(At(at::kOptionWidth), kEight, sizeof kEight) == 0) width = n * 8u;
            else bof3::Fatal("Config_DrawRowOptions: the width code at 0x%X is neither the original's nor DIV-0017's",
                             (unsigned)at::kOptionWidth);
            const U xc = rx - width / 2 + static_cast<U>(x) + 0x74;
            SH_AT(DrawText, SiteTarget(at::kOptionBigSite))(static_cast<int>(xc), y - 2, static_cast<int>(colour), n, text);
        } else {
            const U xc = rx - (n * 8u) / 2 + static_cast<U>(x) + 0x74;
            SH_CALL(Text_DrawSmall)(static_cast<int>(xc), y + 1, colour, n, text);
        }
        rec += 16;
        ++i;
        if (static_cast<std::int16_t>(i) >= static_cast<std::int16_t>(B(at::kOptionCount + r))) break;
    }
}

// original 0x461A50 (cdecl (x, y)): the controller panel. The frame (x, y, w, h)
// through the call at 0x461A84 (DIV-0011), w and h the immediates at 0x461A62
// (0xC; DIV-0026 / DIV-0051 make it 0xD) and 0x461A60 (0xF), read in place; then
// for rows 0..5 at y' = y + 18 * row + 6: Config_DrawControllerRow(x + 5, y', the
// row in the y argument's low byte) and Config_DrawControllerCell(x + 0x58, y',
// the button word 0x903580 + 2 * {0, 1, 6, 2, 3, 4}[row] under y's high half).
extern "C" void __cdecl Config_DrawControllerPanel(int x, int y) {
    const int w = static_cast<signed char>(B(at::kPadFrameW));
    const int h = static_cast<signed char>(B(at::kPadFrameH));
    SH_AT(Frame, SiteTarget(at::kPadFrameSite))(x, y, w, h);
    static const unsigned char kWords[6] = {0, 1, 6, 2, 3, 4};
    const U hi = static_cast<U>(y);
    for (unsigned row = 0; row < 6; ++row) {
        const int ry = y + static_cast<int>(row) * 18 + 6;
        SH_CALL(Config_DrawControllerRow)(x + 5, ry, static_cast<int>((hi & 0xFFFFFF00u) | row));
        const U word = W(at::kButtons + 2u * kWords[row]);
        SH_CALL(Config_DrawControllerCell)(x + 0x58, ry, (hi & 0xFFFF0000u) | word);
    }
}

// original 0x461AF0 (cdecl (x, y, row)): one controller row. The row's dark box
// Menu_DrawBox(x, y, w, 0x10, 0, the style byte), w the immediate at 0x461B07
// (0x68; DIV-0051 0x58); the name 0x66A368[row] (ours aborts past six) right
// aligned at x + edge - len * k (edge the imm8 at 0x461B41: 0x20, DIV-0026 0x36;
// k 6, or 4 under DIV-0026's `lea` at 0x461B36) and y + 1 through the call at
// 0x461B43 (Text_DrawAt, or ConfigText_DrawSelected); a grey LINE_F2 at x16 +
// 0x3F from y16 + 1 to y16 + 0xE (x16, y16 the arguments' low words), committed
// (1, 0x20); unless DIV-0051's jump stands at 0x461BAC, a second at x16 + 0x53
// with the first's two y floats copied as dwords.
extern "C" void __cdecl Config_DrawControllerRow(int x, int y, int row) {
    const unsigned char style = B(at::kStyle);
    SH_CALL(Menu_DrawBox)(x, y, B(at::kPadBoxWidth), 0x10, 0, style);
    const U r = static_cast<U>(row) & 0xFF;
    if (r >= at::kPadNameCount)
        bof3::Fatal("Config_DrawControllerRow: row %u, past the six names of 0x%X - the original reads the dword after "
                    "as a string (docs/rest_4f.md section 7)",
                    (unsigned)r, (unsigned)at::kPadNames);
    const auto* const name = At(L(at::kPadNames + 4 * r));
    const U len = static_cast<U>(std::strlen(reinterpret_cast<const char*>(name)));
    U k;
    const unsigned char sib = At(at::kPadNameWidth)[2];
    if (At(at::kPadNameWidth)[0] == 0x8D && At(at::kPadNameWidth)[1] == 0x04 && sib == 0x49) k = 6;
    else if (At(at::kPadNameWidth)[0] == 0x8D && At(at::kPadNameWidth)[1] == 0x04 && sib == 0x09) k = 4;
    else
        bof3::Fatal("Config_DrawControllerRow: the width code at 0x%X is neither the original's nor DIV-0026's",
                    (unsigned)at::kPadNameWidth);
    const int edge = static_cast<signed char>(B(at::kPadNameEdge));
    SH_AT(DrawText, SiteTarget(at::kPadNameSite))(x - static_cast<int>(len * k) + edge, y + 1, 0, 0xFF, name);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    const int xs = static_cast<int>(static_cast<U>(x) & 0xFFFF);
    const int ys = static_cast<int>(static_cast<U>(y) & 0xFFFF);
    const float x0 = static_cast<float>(xs + 0x3F);
    const float y0 = static_cast<float>(ys + 1);
    const float y1 = static_cast<float>(ys + 0xE);
    prim[4] = prim[5] = 0x80;
    std::memcpy(prim + 0x14, &x0, 4);
    std::memcpy(prim + 8, &x0, 4);
    prim[6] = 0x80;
    std::memcpy(prim + 0xC, &y0, 4);
    std::memcpy(prim + 0x18, &y1, 4);
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
    static const unsigned char kSecond[] = {0x8B, 0x35, 0x70, 0x06, 0x7E, 0x00};   // mov esi, [Gfx_PacketNext]
    static const unsigned char kSkip[] = {0xEB, 0x3B, 0x90, 0x90, 0x90, 0x90};     // DIV-0051: jmp over the second cell
    if (std::memcmp(At(at::kPadSecondCell), kSkip, sizeof kSkip) == 0) return;
    if (std::memcmp(At(at::kPadSecondCell), kSecond, sizeof kSecond) != 0)
        bof3::Fatal("Config_DrawControllerRow: the code at 0x%X is neither the original's nor DIV-0051's",
                    (unsigned)at::kPadSecondCell);
    unsigned char* const prim2 = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim2);
    const float x1 = static_cast<float>(xs + 0x53);
    std::memcpy(prim2 + 0xC, &y0, 4);
    std::memcpy(prim2 + 0x18, &y1, 4);
    prim2[4] = prim2[5] = 0x80;
    std::memcpy(prim2 + 0x14, &x1, 4);
    std::memcpy(prim2 + 8, &x1, 4);
    prim2[6] = 0x80;
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// original 0x462AC0 (called by Field_ExitFromCell): WorldMap_Records' +0x14 of
// the record WorldMap_RecordIndex's low byte names - the area's exit records
// (x, z, area, kind). Index 11 ("none") reads the dword after the eleventh
// record, EffectKind07_States[3] (a code pointer, as effect_1a.cpp's dispatchers
// read their +4 / +8 there); ours aborts past 11. Area 104's record holds 0.
extern "C" const unsigned char* __cdecl WorldMap_ExitRecords(void) {
    const U index = SH_CALL(WorldMap_RecordIndex)() & 0xFF;
    if (index >= at::kRecordCount)
        bof3::Fatal("WorldMap_ExitRecords: WorldMap_RecordIndex answered %u, past the twelve rows (docs/rest_4f.md "
                    "section 7)",
                    (unsigned)index);
    return At(L(at::kExitRecords + index * at::kRecordSize));
}

// ===========================================================================
// Kind 7's states 2..4 (EffectKind07_States; E1A's EffectKind07_Start /
// _FadeIn are 0 and 1)
// ===========================================================================

// original 0x462F10 (cdecl (semi)): a draw mode (0, 0, page 0xDD, 0) committed
// (+0x29, 0xC); a SPRT at (32.0, 32.0), u 0, v 0x20, CLUT word 0x7A80, w 0x100,
// h 0x6E8, the colour +0x5D..+0x5F, Gpu_SetShadeTex 0, semi-transparency the
// argument's byte; committed (+0x29, 0x1C). PSX twin 0x801F7134 (a SCENA
// overlay, analysis/pairs_propagated.json; the sibling names none).
extern "C" void __cdecl EffectKind07_DrawSprite(int semi) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xDD, 0);
    SH_CALL(Gfx_CommitPrim)(S()[0x29], 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt)(prim);
    prim[0x14] = 0;
    SetUL(prim + 8, 0x42000000u);
    SetUL(prim + 0xC, 0x42000000u);
    prim[0x15] = 0x20;
    SetWord(prim + 0x18, 0x100);
    SetWord(prim + 0x1A, 0x6E8);
    SetWord(prim + 0x16, 0x7A80);
    const unsigned char* const s = S();
    prim[4] = s[0x5D];
    prim[5] = s[0x5E];
    prim[6] = s[0x5F];
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    SH_CALL(Gpu_SetSemiTrans)(prim, static_cast<unsigned>(semi) & 0xFF);
    SH_CALL(Gfx_CommitPrim)(S()[0x29], 0x1C);
}

// original 0x462FC0 (EffectKind07_States[2] and [3]): the sprite drawn
// semi-transparent; +9 down one while not 0, else +1 up and +9 0xFF.
extern "C" void __cdecl EffectKind07_Hold(void) {
    SH_CALL(EffectKind07_DrawSprite)(1);
    unsigned char* const s = S();
    if (s[9] != 0) {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        return;
    }
    s[1] = static_cast<unsigned char>(s[1] + 1);
    s[9] = 0xFF;
}

// original 0x462FF0 (EffectKind07_States[4]): the sprite drawn; +0x5D..+0x5F down
// two; at +0x5D 0, +9 0 and Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKind07_FadeOut(void) {
    SH_CALL(EffectKind07_DrawSprite)(1);
    unsigned char* const s = S();
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0xFE);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0xFE);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0xFE);
    if (s[0x5D] != 0) return;
    s[9] = 0;
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 8 (EffectKind08_States: 0..2 here, 3 Effect_StateRelease)
// ===========================================================================

// original 0x463060 (EffectKind08_States[0]): the point (+0x34 0xB0000, +0x38
// 0xF8000), the word +0x3E the ground's AreaMap_Elevation there, the slot +0x29
// 5, +1 = 1.
extern "C" void __cdecl EffectKind08_Start(void) {
    unsigned char* s = S();
    SetUL(s + 0x34, 0xB0000);
    SetUL(s + 0x38, 0xF8000);
    const long e = SH_CALL(AreaMap_Elevation)(SL(s + 0x34), SL(s + 0x38));
    s = S();
    SetWord(s + 0x3E, static_cast<U>(e));
    s[0x29] = 5;
    s[1] = 1;
}

// original 0x463350 (cdecl, seven words by value: a point A (x, z, a height
// dword whose high word is read), a point B the same, a colour's three bytes): a
// LINE_F2 at Gfx_PacketNext - B then A turned into the GTE's vector (x and z as
// (v >> 9) - 0x4000, the height -(s16 high word) / 2) and projected
// (Gte_RotTransPers, Gte_StoreDepthF) to +8 / +0x10 and +0x14 / +0x1C; the colour
// to +4..+6; committed (+0x29, 0x20). The vector's fourth word is the original's
// stale stack, ours 0 (DIV-0023's ruling, as effect_1a.cpp's AttachVector).
extern "C" void __cdecl EffectKind08_DrawLine(int ax, int az, int ah, int bx, int bz, int bh, int colour) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    const auto vector = [](short* v, int x, int z, int h) {
        v[0] = static_cast<short>(Sar(static_cast<U>(x), 9) - 0x4000u);
        v[1] = static_cast<short>(Sar(static_cast<U>(z), 9) - 0x4000u);
        v[2] = static_cast<short>(-(static_cast<std::int32_t>(static_cast<std::int16_t>(static_cast<U>(h) >> 16)) / 2));
        v[3] = 0;
    };
    short v[4];
    long p;
    vector(v, bx, bz, bh);
    SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(prim + 8), &p);
    SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(prim + 0x10));
    vector(v, ax, az, ah);
    SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(prim + 0x14), &p);
    SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(prim + 0x1C));
    const U c = static_cast<U>(colour);
    prim[4] = static_cast<unsigned char>(c);
    prim[5] = static_cast<unsigned char>(c >> 8);
    prim[6] = static_cast<unsigned char>(c >> 16);
    SH_CALL(Gfx_CommitPrim)(S()[0x29], 0x20);
}

namespace {

// The Sprite_Objects record kind 8 traces to: +6's byte, unchecked in the
// original (ours aborts past the thirty).
const unsigned char* TraceObject(const char* who) {
    const unsigned idx = S()[6];
    if (idx >= at::kObjectCount)
        bof3::Fatal("%s: the object byte +6 is %u, past the thirty Sprite_Objects records - the original reads what "
                    "follows (docs/rest_4f.md section 7)",
                    who, idx);
    return Sprite_Objects + idx * 0xA4u;
}

// One run of kind 8's line along z, in steps of 0x8000 (`dir` +1 or -1) from z to
// the object's z: each step's height the ground's (its high word; the low word
// is what the frame's height cell held - never read, EffectKind08_DrawLine reads
// the high word), the last the object's own height; the line from the new point
// back to the last (`back`: from the last to the new).
struct Trace {
    U x, z, h, hd;
};
void Run(Trace& t, std::int32_t target, U oh, int dir, bool back, U colour) {
    for (bool more = true; more;) {
        std::int32_t nz = static_cast<std::int32_t>(t.z + static_cast<U>(dir * 0x8000));
        U nh;
        if (dir > 0 ? nz > target : nz < target) {
            nz = target;
            t.hd = oh;
            nh = oh;
            more = false;
        } else {
            const long e = SH_CALL(AreaMap_Elevation)(static_cast<long>(t.x), nz);
            t.hd = (t.hd & 0xFFFFu) | (static_cast<U>(e) << 16);
            nh = t.hd;
        }
        if (back)
            SH_CALL(EffectKind08_DrawLine)(static_cast<int>(t.x), static_cast<int>(t.z), static_cast<int>(t.h),
                                           static_cast<int>(t.x), nz, static_cast<int>(nh), static_cast<int>(colour));
        else
            SH_CALL(EffectKind08_DrawLine)(static_cast<int>(t.x), nz, static_cast<int>(nh), static_cast<int>(t.x),
                                           static_cast<int>(t.z), static_cast<int>(t.h), static_cast<int>(colour));
        t.z = static_cast<U>(nz);
        t.h = nh;
    }
}

}  // namespace

// original 0x4630B0 (EffectKind08_States[1]): a grey (0xC0) line along the ground
// from the record's point (+0x34, +0x38, +0x3C) to the z of the Sprite_Objects
// record +6 names, in steps of 0x8000 (the last step the object's z and height);
// +1 = 2 when the chapter's byte 0x90384A is 0x32.
extern "C" void __cdecl EffectKind08_TraceLine(void) {
    const unsigned char* const s = S();
    const unsigned char* const o = TraceObject("EffectKind08_TraceLine");
    Trace t{UL(s + 0x34), UL(s + 0x38), UL(s + 0x3C), UL(s + 0x3C)};
    Run(t, SL(o + 0x38), UL(o + 0x3C), +1, false, 0xC0C0C0);
    if (B(at::kChapterByte4A) == 0x32) S()[1] = 2;
}

// original 0x4631B0 (EffectKind08_States[2]): the grey line as state 1, then a
// dark (0x30) one from (0xB0000, 0x138000) - its height the ground's there -
// down z to the object's; +1 = 3 when 0x90384A is 0x34. The frame's height cell
// is not set before the first step (stale stack in the original; its low word is
// never read - ours 0).
extern "C" void __cdecl EffectKind08_TraceTwo(void) {
    const unsigned char* const s = S();
    const unsigned char* const o = TraceObject("EffectKind08_TraceTwo");
    const std::int32_t target = SL(o + 0x38);
    const U oh = UL(o + 0x3C);
    Trace t{UL(s + 0x34), UL(s + 0x38), UL(s + 0x3C), 0};
    Run(t, target, oh, +1, false, 0xC0C0C0);
    t.x = 0xB0000;
    t.z = 0x138000;
    const long e = SH_CALL(AreaMap_Elevation)(0xB0000, 0x138000);
    t.h = (t.h & 0xFFFFu) | (static_cast<U>(e) << 16);
    Run(t, target, oh, -1, true, 0x303030);
    if (B(at::kChapterByte4A) == 0x34) S()[1] = 3;
}

// ===========================================================================
// Kind 9 (EffectKind09_States: ten)
// ===========================================================================

// original 0x463460 (EffectKind09_States[0]): the point (0x638000, 0xD0000, the
// height word 0x300); +0x64..+0x6C 0; +9 0x78; the colour 0xFF; +0xC 1; +0x10,
// +0x20, +0x1C, +0x18 0; +0xA, +0xB, +8, +7, +6 0; sound 0x20E; +1 = 1.
extern "C" void __cdecl EffectKind09_Start(void) {
    unsigned char* s = S();
    SetUL(s + 0x34, 0x638000);
    SetUL(s + 0x38, 0xD0000);
    SetWord(s + 0x3E, 0x300);
    SetUL(s + 0x64, 0);
    SetUL(s + 0x68, 0);
    SetUL(s + 0x6C, 0);
    s[9] = 0x78;
    s[0x5F] = s[0x5E] = s[0x5D] = 0xFF;
    SetUL(s + 0xC, 1);
    SetUL(s + 0x10, 0);
    SetUL(s + 0x20, 0);
    SetUL(s + 0x1C, 0);
    SetUL(s + 0x18, 0);
    s[0xA] = s[0xB] = s[8] = s[7] = s[6] = 0;
    Sound(0x20E);
    S()[1] = 1;
}

namespace {

// Kind 9's fans, the n-th drawn with its length and its three angles (each
// state draws the first n; Sprite_Current read before each).
void Fan(unsigned n) {
    const unsigned char* const s = S();
    switch (n) {
    case 1: SH_CALL(EffectKind09_DrawFan)(SL(s + 0x18), 0x200, 0, 0); break;
    case 2: SH_CALL(EffectKind09_DrawFan)(SL(s + 0x1C), 0x71, 0, 0x8E3); break;
    case 3: SH_CALL(EffectKind09_DrawFan)(s[0xB], 0x155, 0, 0x200); break;
    case 4: SH_CALL(EffectKind09_DrawFan)(s[8], 0xE3, 0, 0xE38); break;
    case 5: SH_CALL(EffectKind09_DrawFan)(s[0xA], 0x31C, 0, 0x555); break;
    case 6: SH_CALL(EffectKind09_DrawFan)(s[6], 0x238, 0, 0xAAA); break;
    case 7: SH_CALL(EffectKind09_DrawFan)(s[7], 0, 0, 0x71C); break;
    default: SH_CALL(EffectKind09_DrawFan)(SL(s + 0x20), 0, 0, 0xC00); break;
    }
}
void Fans(unsigned n) {
    for (unsigned i = 1; i <= n; ++i) Fan(i);
}
// A byte length up by `step`; sound 0x20E at `mid` (0 for none); at 0x60 the
// sound and +1 = next.
void GrowByte(unsigned off, unsigned step, unsigned mid, unsigned char next) {
    unsigned char* const s = S();
    s[off] = static_cast<unsigned char>(s[off] + step);
    if (mid != 0 && S()[off] == mid) Sound(0x20E);
    if (S()[off] != 0x60) return;
    Sound(0x20E);
    S()[1] = next;
}

}  // namespace

// original 0x463540 (EffectKind09_States[1]): fan 1; its length +0x18 up 8; at
// 0x60 sound 0x20E and +1 = 2.
extern "C" void __cdecl EffectKind09_Open1(void) {
    Fans(1);
    unsigned char* const s = S();
    SetUL(s + 0x18, UL(s + 0x18) + 8);
    if (UL(S() + 0x18) != 0x60) return;
    Sound(0x20E);
    S()[1] = 2;
}

// original 0x463590 (EffectKind09_States[2]): fans 1, 2; +0x1C up 0x10; sound
// 0x20E at 0x20; at 0xA0 the sound and +1 = 3.
extern "C" void __cdecl EffectKind09_Open2(void) {
    Fans(2);
    unsigned char* const s = S();
    SetUL(s + 0x1C, UL(s + 0x1C) + 0x10);
    if (UL(S() + 0x1C) == 0x20) Sound(0x20E);
    if (UL(S() + 0x1C) != 0xA0) return;
    Sound(0x20E);
    S()[1] = 3;
}

// original 0x463610 (EffectKind09_States[3]): fans 1..3; +0xB up 0x10; at 0x60
// sound 0x20E and +1 = 4.
extern "C" void __cdecl EffectKind09_Open3(void) {
    Fans(3);
    GrowByte(0xB, 0x10, 0, 4);
}

// original 0x463690 (EffectKind09_States[4]): fans 1..4; +8 up 8; at 0x60 the
// sound and +1 = 5.
extern "C" void __cdecl EffectKind09_Open4(void) {
    Fans(4);
    GrowByte(8, 8, 0, 5);
}

// original 0x463730 (EffectKind09_States[5]): fans 1..5; +0xA up 8; sound at
// 0x10; at 0x60 the sound and +1 = 6.
extern "C" void __cdecl EffectKind09_Open5(void) {
    Fans(5);
    GrowByte(0xA, 8, 0x10, 6);
}

// original 0x463810 (EffectKind09_States[6]): fans 1..6; +6 up 8; sound at
// 0x10; at 0x60 the sound and +1 = 7.
extern "C" void __cdecl EffectKind09_Open6(void) {
    Fans(6);
    GrowByte(6, 8, 0x10, 7);
}

// original 0x463910 (EffectKind09_States[7]): fans 1..7; +7 up 8; at 0x60 the
// sound and +1 = 8.
extern "C" void __cdecl EffectKind09_Open7(void) {
    Fans(7);
    GrowByte(7, 8, 0, 8);
}

// original 0x463A10 (EffectKind09_States[8]): fans 1..8; +0x20 up 8; sound 0x20E
// at 0x10; at 0x60 sound 0x203, the chapter's count 0x903848 up one, +1 = 9.
extern "C" void __cdecl EffectKind09_Open8(void) {
    Fans(8);
    unsigned char* const s = S();
    SetUL(s + 0x20, UL(s + 0x20) + 8);
    if (UL(S() + 0x20) == 0x10) Sound(0x20E);
    if (UL(S() + 0x20) != 0x60) return;
    Sound(0x203);
    B(at::kChapterCount) = static_cast<unsigned char>(B(at::kChapterCount) + 1);
    S()[1] = 9;
}

// original 0x463B40 (EffectKind09_States[9]): at +9 0, Effect_Release (a tail
// jmp); else the blades drawn, +0xC up 4 while below 0x80, +0x10 up 2 to 0xB6
// (s32 compares), +9 down one.
extern "C" void __cdecl EffectKind09_Spin(void) {
    if (S()[9] == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    SH_CALL(EffectKind09_DrawBlades)();
    unsigned char* const s = S();
    const std::int32_t c = SL(s + 0xC);
    if (c < 0x80) SetUL(s + 0xC, static_cast<U>(c) + 4);
    const std::int32_t d = SL(s + 0x10);
    if (d < 0xB6) SetUL(s + 0x10, static_cast<U>(d) + 2);
    else SetUL(s + 0x10, 0xB6);
    s[9] = static_cast<unsigned char>(s[9] - 1);
}

namespace {

// Kind 9's triangle: a POLY_G3 at Gfx_PacketNext of the three vectors, projected
// under the record's object matrix (Gte_PushMatrix, Sprite_ObjectMatrix,
// Camera_LoadMatrix ... Gte_PopMatrix). `colours` sets the vertex colours before
// the semi-transparency; committed (+0x29, 0x34).
template <typename C> void Triangle(const short* v0, const short* v1, const short* v2, C colours) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG3)(prim);
    SH_CALL(Gte_PushMatrix)();
    short m[16];
    SH_CALL(Sprite_ObjectMatrix)(m);
    SH_CALL(Camera_LoadMatrix)(m);
    long p;
    SH_CALL(Gte_RotTransPers3)(v0, v1, v2, reinterpret_cast<float*>(prim + 8), reinterpret_cast<float*>(prim + 0x18),
                               reinterpret_cast<float*>(prim + 0x28), &p);
    SH_CALL(Gte_PrimDepths3_10B)(prim);
    colours(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(Gfx_CommitPrim)(S()[0x29], 0x34);
    SH_CALL(Gte_PopMatrix)();
}
// (Math_Cos or Math_Sin of `angle`) * `len` >> 8, as the original's imul / sar.
short Arm(int (__cdecl* f)(int), int angle, U len) {
    const U v = static_cast<U>(f(angle));
    return static_cast<short>(Sar(v * len, 8));
}

}  // namespace

// original 0x463BA0 (void, Sprite_Current): twenty-four blades. +0x6C set to
// (+9 << 12) / 360 (the original's multiply by 0xB60B60B7); two triangles of
// the length L = +0xC: (0, 0, 0) - (L << 4, 0, 0) - (L cos(+0x10) >> 8, L
// sin(+0x10) >> 8, 0) on odd blades, the same with the angle 0xAC on even ones;
// each blade's colour +0x5D..+0x5F at its root and -(2 * +9) at the other two
// corners; +0x6C up 0xAA after each.
extern "C" void __cdecl EffectKind09_DrawBlades(void) {
    unsigned char* const s0 = S();
    const U n = static_cast<U>(s0[9]) << 12;
    const U len = UL(s0 + 0xC);
    const U angle = UL(s0 + 0x10);
    const U hi = static_cast<U>((static_cast<std::int64_t>(static_cast<std::int32_t>(n)) *
                                 static_cast<std::int64_t>(static_cast<std::int32_t>(0xB60B60B7u))) >> 32);
    U q = Sar(hi + n, 8);
    q += q >> 31;
    SetUL(s0 + 0x6C, q);
    const short c1 = Arm(SH_CALL(Math_Cos), static_cast<int>(angle), len);
    const short s1 = Arm(SH_CALL(Math_Sin), static_cast<int>(angle), len);
    const short reach = static_cast<short>(len << 4);
    const short odd0[4] = {0, 0, 0, 0}, odd1[4] = {reach, 0, 0, 0}, odd2[4] = {c1, s1, 0, 0};
    const short c2 = Arm(SH_CALL(Math_Cos), 0xAC, len);
    const short s2 = Arm(SH_CALL(Math_Sin), 0xAC, len);
    const short even0[4] = {0, 0, 0, 0}, even1[4] = {reach, 0, 0, 0}, even2[4] = {c2, s2, 0, 0};
    for (unsigned blade = 0; blade < 0x18; ++blade) {
        const auto colours = [](unsigned char* prim) {
            const unsigned char* const s = S();
            prim[4] = s[0x5D];
            prim[5] = s[0x5E];
            prim[6] = s[0x5F];
            const auto tip = static_cast<unsigned char>(-(s[9] << 1));
            prim[0x14] = prim[0x15] = prim[0x16] = tip;
            prim[0x24] = prim[0x25] = prim[0x26] = tip;
        };
        if (blade & 1) Triangle(odd0, odd1, odd2, colours);
        else Triangle(even0, even1, even2, colours);
        unsigned char* const s = S();
        SetUL(s + 0x6C, UL(s + 0x6C) + 0xAA);
    }
}

// original 0x463D80 (cdecl (len, a1, a2, a3)): one fan. The record's +0x64..+0x70
// kept; +0x64, +0x68, +0x6C set to a1, a2, a3 (the object matrix's angles); a
// triangle (0, 0, 0) - (len << 4, 0, 0) - (len cos(0x4F) >> 8, len sin(0x4F) >> 8,
// 0), its colours 0x28 / 0x6E / 0x64; the kept four dwords put back into the
// record current afterwards.
extern "C" void __cdecl EffectKind09_DrawFan(int len, int a1, int a2, int a3) {
    unsigned char* const s0 = S();
    const U k64 = UL(s0 + 0x64), k68 = UL(s0 + 0x68), k6C = UL(s0 + 0x6C), k70 = UL(s0 + 0x70);
    SetUL(s0 + 0x64, static_cast<U>(a1));
    SetUL(s0 + 0x68, static_cast<U>(a2));
    SetUL(s0 + 0x6C, static_cast<U>(a3));
    const short c = Arm(SH_CALL(Math_Cos), 0x4F, static_cast<U>(len));
    const short sn = Arm(SH_CALL(Math_Sin), 0x4F, static_cast<U>(len));
    const short v0[4] = {0, 0, 0, 0};
    const short v1[4] = {static_cast<short>(static_cast<U>(len) << 4), 0, 0, 0};
    const short v2[4] = {c, sn, 0, 0};
    Triangle(v0, v1, v2, [](unsigned char* prim) {
        prim[4] = prim[5] = prim[6] = 0x28;
        prim[0x14] = prim[0x15] = prim[0x16] = 0x6E;
        prim[0x24] = prim[0x25] = prim[0x26] = 0x64;
    });
    unsigned char* const s = S();
    SetUL(s + 0x64, k64);
    SetUL(s + 0x68, k68);
    SetUL(s + 0x6C, k6C);
    SetUL(s + 0x70, k70);
}

// ===========================================================================
// Kind 0xB (EffectKind0B_States: five)
// ===========================================================================

// original 0x463F00 (EffectKind0B_States[0]): the screen point +0x74 / +0x78
// (floats) the first extra sprite's words +0x2E / +0x30 plus the two s8 of
// 0x653AAC; +0x7C 0.01f; +9 0; +6 0x20; the colour 0x80; the quad drawn; +1 = +7.
extern "C" void __cdecl EffectKind0B_Start(void) {
    unsigned char* s = S();
    StF(s + 0x74, I(static_cast<U>(static_cast<signed char>(B(at::kKind0BOffset)) +
                                   static_cast<std::int16_t>(W(at::kExtra0 + 0x2E)))));
    StF(s + 0x78, I(static_cast<U>(static_cast<signed char>(B(at::kKind0BOffset + 1)) +
                                   static_cast<std::int16_t>(W(at::kExtra0 + 0x30)))));
    SetUL(s + 0x7C, 0x3C23D70Au);
    s[9] = 0;
    s[6] = 0x20;
    s[0x5F] = s[0x5E] = s[0x5D] = 0x80;
    SH_CALL(EffectKind0B_DrawQuad)();
    s = S();
    s[1] = s[7];
}

namespace {

// Kinds 0xB's fades: +0x5D down `step`, +0x5F and +0x5E down `step` (a byte);
// +6 up one when +9 & mask is 0. False (nothing changed) at +0x5D 0.
bool Fade0B(unsigned step, unsigned mask) {
    unsigned char* const s = S();
    const unsigned char v = s[0x5D];
    if (v == 0) return false;
    s[0x5D] = static_cast<unsigned char>(v - step);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] - step);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] - step);
    if ((s[9] & mask) == 0) s[6] = static_cast<unsigned char>(s[6] + 1);
    return true;
}

}  // namespace

// original 0x463FA0 (EffectKind0B_States[1]): x -= 4.0; y += the s8 of
// 0x653AB0 by +9 >> 2 (read in place); at +0x5D 0 Effect_Release; else the
// colour down 2, +9 up one, +6 up one each fourth, the quad drawn.
extern "C" void __cdecl EffectKind0B_Rise(void) {
    unsigned char* const s = S();
    StF(s + 0x74, F(s + 0x74) - F(At(at::kKind0BStepX)));
    const auto rise = static_cast<U>(static_cast<signed char>(B(at::kKind0BRise + (s[9] >> 2))));
    StF(s + 0x78, I(rise) + F(s + 0x78));
    if (s[0x5D] == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    s[0x5D] = static_cast<unsigned char>(s[0x5D] - 2);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0xFE);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0xFE);
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if ((s[9] & 3) == 0) s[6] = static_cast<unsigned char>(s[6] + 1);
    SH_CALL(EffectKind0B_DrawQuad)();
}

// original 0x464030 (EffectKind0B_States[2]): y -= 2.0; at +0x5D 0 Effect_Release
// (a tail jmp); else the colour down 4, +6 up one when +9 & 7 is 0, the quad (a
// tail jmp).
extern "C" void __cdecl EffectKind0B_Drop(void) {
    unsigned char* const s = S();
    StF(s + 0x78, F(s + 0x78) - F(At(at::kKind0BStepY)));
    if (!Fade0B(4, 7)) {
        SH_CALL(Effect_Release)();
        return;
    }
    SH_CALL(EffectKind0B_DrawQuad)();
}

// original 0x464090 (EffectKind0B_States[3]): the point +0x34..+0x3C and the
// angles +0x64..+0x6C the first extra sprite's; plus MoveCmd_AttachOffset of the
// byte +0x1C (x, z, and the height word +0x3E); +1 up.
extern "C" void __cdecl EffectKind0B_Attach(void) {
    unsigned char* s = S();
    SetUL(s + 0x34, L(at::kExtra0 + 0x34));
    SetUL(s + 0x38, L(at::kExtra0 + 0x38));
    SetUL(s + 0x3C, L(at::kExtra0 + 0x3C));
    SetUL(s + 0x64, L(at::kExtra0 + 0x64));
    SetUL(s + 0x68, L(at::kExtra0 + 0x68));
    SetUL(s + 0x6C, L(at::kExtra0 + 0x6C));
    long out[3];
    SH_CALL(MoveCmd_AttachOffset)(out, s[0x1C]);
    s = S();
    SetUL(s + 0x34, UL(s + 0x34) + static_cast<U>(out[0]));
    SetUL(s + 0x38, UL(s + 0x38) + static_cast<U>(out[1]));
    SetWord(s + 0x3E, Word(s + 0x3E) + static_cast<U>(out[2]));
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x464140 (EffectKind0B_States[4]): the height word +0x3E up 0x20; the
// point (+0x34, +0x38, +0x3C) projected into +0x74 (EffectGte_ProjectPoint); at
// +0x5D 0 Effect_Release; else the colour down 4, +6 up one when +9 & 7 is 0, the
// quad drawn. PSX twin 0x801F8AB8 (a SCENA overlay).
extern "C" void __cdecl EffectKind0B_Follow(void) {
    unsigned char* s = S();
    SetWord(s + 0x3E, Word(s + 0x3E) + 0x20);
    const long point[3] = {SL(s + 0x34), SL(s + 0x38), SL(s + 0x3C)};
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(s + 0x74));
    if (!Fade0B(4, 7)) {
        SH_CALL(Effect_Release)();
        return;
    }
    SH_CALL(EffectKind0B_DrawQuad)();
}

// original 0x4641D0 (void, Sprite_Current): a POLY_FT4 square of side +6 centred
// on +0x74 / +0x78 less h = +6 >> 1 (x87: (x - h), (x - h) + side), the colour
// +0x5D..+0x5F, the depth +0x7C on each corner (copied through the FPU), u / v
// (0xE0, 0x30)-(0xFF, 0x50), Gpu_GetClut(0xA0, 0x1E3), Gpu_GetTPage(0, 1, 0x2C0,
// 0x100), no shading, semi-transparent; committed (3, 0x48) only while
// Draw_PassFlags is set. PSX twin 0x801F8BCC (a SCENA overlay).
extern "C" void __cdecl EffectKind0B_DrawQuad(void) {
    unsigned char* const prim = Gfx_PacketNext;
    const unsigned half = static_cast<unsigned char>(S()[6] >> 1);
    SH_CALL(Gpu_SetPolyFT4)(prim);
    const unsigned char* const s = S();
    const LD h = I(half);
    const LD side = I(s[6]);
    const LD x = F(s + 0x74) - h, y = F(s + 0x78) - h;
    StF(prim + 8, x);
    StF(prim + 0xC, y);
    StF(prim + 0x18, F(s + 0x74) - h + side);
    StF(prim + 0x1C, F(s + 0x78) - h);
    StF(prim + 0x28, F(s + 0x74) - h);
    StF(prim + 0x2C, F(s + 0x78) - h + side);
    StF(prim + 0x38, F(s + 0x74) - h + side);
    prim[0x15] = prim[0x25] = 0x30;
    prim[0x14] = 0xE0;
    prim[0x24] = 0xFF;
    prim[0x34] = 0xE0;
    prim[0x35] = 0x50;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x50;
    StF(prim + 0x3C, F(s + 0x78) - h + side);
    prim[4] = s[0x5D];
    prim[5] = s[0x5E];
    prim[6] = s[0x5F];
    const LD depth = F(s + 0x7C);
    StF(prim + 0x40, depth);
    StF(prim + 0x30, depth);
    StF(prim + 0x20, depth);
    StF(prim + 0x10, depth);
    const unsigned clut = SH_CALL(Gpu_GetClut)(0xA0, 0x1E3);
    SetWord(prim + 0x16, clut);
    const unsigned page = SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100);
    SetWord(prim + 0x26, page);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    if (Draw_PassFlags != 0) SH_CALL(Gfx_CommitPrim)(3, 0x48);
}

// ===========================================================================
// Kind 2 (EffectKind02_States: 0, 2, 3, 4 here; 1 is BareRet)
// ===========================================================================

namespace {

// EffectHud_Draw(0xC, y): y the word +0x10 under `upper` (the original pushes eax
// after `mov ax, [+0x10]` - Sprite_Current's high half, or the sub-state
// handler's leftover; the panel reads the word).
void Panel(U upper) { SH_CALL(EffectHud_Draw)(0xC, static_cast<int>(upper | Word(S() + 0x10))); }
U Hi(const void* p) { return Key(p) & 0xFFFF0000u; }

// Palette 0x7B: entries 1..30 the leader's palette (ObjTrio 0's +0x27) with bit
// 15 set, entries 0 and 31 zero; Gfx_ClutStripDirty is set by the callers.
void TintPalette() {
    const U from = at::kClutShadow + 2 + static_cast<U>(B(at::kLeader + 0x27)) * at::kClutBytes;
    for (U i = 0; i < 30; ++i) SetW(at::kClut7B + 2 + 2 * i, W(from + 2 * i) | 0x8000);
    SetW(at::kClut7B, 0);
    SetW(at::kClut7B + 0x3E, 0);
}
// The record shows palette 0x7B: +0xB the palette it had, +0 bit 5 set, +0x27 0x7B.
void UseTint(unsigned char* s) {
    s[0xB] = s[0x27];
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    s[0x27] = 0x7B;
}
// Back to its own palette: the colour 0, +0 bit 5 clear, +0x27 from +0xB.
void DropTint(unsigned char* s) {
    s[0x5D] = s[0x5E] = s[0x5F] = 0;
    s[0] = static_cast<unsigned char>(s[0] & 0xDF);
    s[0x27] = s[0xB];
}
bool Above38() { return static_cast<std::int16_t>(W(at::kKind02Word)) > 0x38; }

}  // namespace

// original 0x464680 (EffectKind02_States[0]): the leader's +0x25..+0x28 copied;
// the slot +0x29 2; +0x24 0x80; the words +0x2E 0x39, +0x30 0xC8; +0x48, +0x5C,
// the colour 0; the panel's y +0x10 0xF0; +1 up.
extern "C" void __cdecl EffectKind02_Start(void) {
    unsigned char* const s = S();
    s[0x25] = B(at::kLeader + 0x25);
    s[0x26] = B(at::kLeader + 0x26);
    s[0x27] = B(at::kLeader + 0x27);
    s[0x28] = B(at::kLeader + 0x28);
    s[0x29] = 2;
    s[0x24] = 0x80;
    SetWord(s + 0x2E, 0x39);
    SetWord(s + 0x30, 0xC8);
    s[0x48] = s[0x5C] = s[0x5D] = s[0x5E] = s[0x5F] = 0;
    SetUL(s + 0x10, 0xF0);
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x464730 (EffectKind02_States[2]): the panel's y down 8, at below 0xAE
// held there and +1 up; EffectHud_Draw(0xC, y).
extern "C" void __cdecl EffectKind02_SlideIn(void) {
    unsigned char* const s = S();
    SetUL(s + 0x10, UL(s + 0x10) + 0xFFFFFFF8u);
    if (SL(s + 0x10) < 0xAE) {
        SetUL(s + 0x10, 0xAE);
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
    Panel(Hi(s));
}

// original 0x464780 (EffectKind02_States[3]): call [EffectKind02_Modes + +2 * 4]
// (unbounded: ours aborts past 4), then the panel at y +0x10.
extern "C" void __cdecl EffectKind02_Body(void) {
    Dispatch("EffectKind02_Body", at::kKind02Modes, at::kKind02ModeCount, S()[2], "the sub-state +2");
    Panel(0);
}

// original 0x4647B0 (EffectKind02_States[4]): the panel's y up 8; past 0xF0 held
// there, +1 = 1, +2 = +3 = 0; the panel drawn.
extern "C" void __cdecl EffectKind02_SlideOut(void) {
    unsigned char* const s = S();
    SetUL(s + 0x10, UL(s + 0x10) + 8);
    if (SL(s + 0x10) > 0xF0) {
        SetUL(s + 0x10, 0xF0);
        s[1] = 1;
        s[2] = s[3] = 0;
    }
    Panel(Hi(s));
}

// original 0x464810 (EffectKind02_Modes[1]): jmp [EffectKind02_Mode1Steps + +3 *
// 4] (unbounded: ours aborts past 2).
extern "C" void __cdecl EffectKind02_RunMode1(void) {
    Dispatch("EffectKind02_RunMode1", at::kKind02Mode1, at::kKind02Mode1Count, S()[3], "the step +3");
}

// original 0x464A20 (EffectKind02_Modes[3]): jmp [EffectKind02_Mode3Steps + +3 *
// 4] (unbounded: ours aborts past 3).
extern "C" void __cdecl EffectKind02_RunMode3(void) {
    Dispatch("EffectKind02_RunMode3", at::kKind02Mode3, at::kKind02Mode3Count, S()[3], "the step +3");
}

// original 0x464830 (EffectKind02_Mode1Steps[0]): palette 0x7B tinted from the
// leader's; Gfx_ClutStripDirty 1; +0x5C 1. With the word 0x905E62 above 0x38 (s16)
// +2 = 3 and +3 = 2; else the record on palette 0x7B, the colour 0x80, +3 up.
extern "C" void __cdecl EffectKind02_Mode1Tint(void) {
    TintPalette();
    unsigned char* const s = S();
    Gfx_ClutStripDirty = 1;
    s[0x5C] = 1;
    if (Above38()) {
        s[2] = 3;
        s[3] = 2;
        return;
    }
    UseTint(s);
    s[0x5D] = s[0x5E] = s[0x5F] = 0x80;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x4648F0 (EffectKind02_Mode1Steps[1]): the colour up 0x10; once +0x5D
// is at least 0xC0 as an s8 (0xC0..0x7F), the record back on its palette, +3 0
// and +2 up; then (a tail jmp) EffectKind02_CopyLeader.
extern "C" void __cdecl EffectKind02_Mode1Brighten(void) {
    unsigned char* const s = S();
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0x10);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0x10);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0x10);
    if (static_cast<signed char>(s[0x5D]) >= static_cast<signed char>(0xC0)) {
        DropTint(s);
        s[3] = 0;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(EffectKind02_CopyLeader)();
}

// original 0x464970 (EffectKind02_Modes[2], and the tail of _Mode1Brighten and
// _Mode3Darken; inside the catalog's extent of 0x4648F0, in no list): with the
// leader's +2 at 1, +2 = 0 and nothing more; +2 = 3 when the word 0x905E62 is
// above 0x38; the leader's +0x2A, +0x49..+0x4B, +0x50, +0x54, the words +0x58,
// +0x5A copied; Sprite_QueueOverlay (a tail jmp).
extern "C" void __cdecl EffectKind02_CopyLeader(void) {
    unsigned char* const s = S();
    if (B(at::kLeader + 2) == 1) {
        s[2] = 0;
        return;
    }
    if (Above38()) s[2] = 3;
    s[0x2A] = B(at::kLeader + 0x2A);
    s[0x49] = B(at::kLeader + 0x49);
    s[0x4A] = B(at::kLeader + 0x4A);
    s[0x4B] = B(at::kLeader + 0x4B);
    SetUL(s + 0x50, L(at::kLeader + 0x50));
    SetUL(s + 0x54, L(at::kLeader + 0x54));
    SetWord(s + 0x58, W(at::kLeader + 0x58));
    SetWord(s + 0x5A, W(at::kLeader + 0x5A));
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x464A40 (EffectKind02_Mode3Steps[0]): palette 0x7B tinted; the record
// on it, +0x5C 1, the colour 0xC0; Gfx_ClutStripDirty 1; +3 up;
// Sprite_QueueOverlay (a tail jmp).
extern "C" void __cdecl EffectKind02_Mode3Tint(void) {
    TintPalette();
    unsigned char* const s = S();
    UseTint(s);
    s[0x5C] = 1;
    s[0x5D] = s[0x5E] = s[0x5F] = 0xC0;
    Gfx_ClutStripDirty = 1;
    s[3] = static_cast<unsigned char>(s[3] + 1);
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x464AF0 (EffectKind02_Mode3Steps[1]): the colour down 0x10; at +0x5D
// 0x80 the record back on its palette and +3 up; else (a tail jmp)
// EffectKind02_CopyLeader.
extern "C" void __cdecl EffectKind02_Mode3Darken(void) {
    unsigned char* const s = S();
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0xF0);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0xF0);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0xF0);
    if (s[0x5D] != 0x80) {
        SH_CALL(EffectKind02_CopyLeader)();
        return;
    }
    DropTint(s);
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x464B60 (EffectKind02_Mode3Steps[2]): with the leader's +2 at 1, +2 =
// +3 = 0; else while the word 0x905E62 is below 0x38 (s16), +2 = 1 and +3 = 0.
extern "C" void __cdecl EffectKind02_Mode3Wait(void) {
    unsigned char* const s = S();
    if (B(at::kLeader + 2) == 1) {
        s[2] = 0;
        s[3] = 0;
        return;
    }
    if (static_cast<std::int16_t>(W(at::kKind02Word)) >= 0x38) return;
    s[2] = 1;
    s[3] = 0;
}

void Rest4F_Inject() {
    if (bof3::WantsShadow("rest_4f")) rest_4f::SelfTest();
    BOF3_INJECT(ConfigScreen_Run);
    BOF3_INJECT(ConfigScreen_OpenRun);
    BOF3_INJECT(ConfigScreen_OpenFade);
    BOF3_INJECT(ConfigScreen_OpenWait);
    BOF3_INJECT(ConfigScreen_SlideIn);
    BOF3_INJECT(ConfigScreen_TopBar);
    BOF3_INJECT(ConfigScreen_Rows);
    BOF3_INJECT(ConfigScreen_Controller);
    BOF3_INJECT(ConfigScreen_SlideOut);
    BOF3_INJECT(ConfigScreen_DrawButtons);
    BOF3_INJECT(Config_DrawPanel);
    BOF3_INJECT(Config_DrawRowOptions);
    BOF3_INJECT(Config_DrawControllerPanel);
    BOF3_INJECT(Config_DrawControllerRow);
    BOF3_INJECT(WorldMap_ExitRecords);
    BOF3_INJECT(EffectKind07_DrawSprite);
    BOF3_INJECT(EffectKind07_Hold);
    BOF3_INJECT(EffectKind07_FadeOut);
    BOF3_INJECT(EffectKind08_Start);
    BOF3_INJECT(EffectKind08_TraceLine);
    BOF3_INJECT(EffectKind08_TraceTwo);
    BOF3_INJECT(EffectKind08_DrawLine);
    BOF3_INJECT(EffectKind09_Start);
    BOF3_INJECT(EffectKind09_Open1);
    BOF3_INJECT(EffectKind09_Open2);
    BOF3_INJECT(EffectKind09_Open3);
    BOF3_INJECT(EffectKind09_Open4);
    BOF3_INJECT(EffectKind09_Open5);
    BOF3_INJECT(EffectKind09_Open6);
    BOF3_INJECT(EffectKind09_Open7);
    BOF3_INJECT(EffectKind09_Open8);
    BOF3_INJECT(EffectKind09_Spin);
    BOF3_INJECT(EffectKind09_DrawBlades);
    BOF3_INJECT(EffectKind09_DrawFan);
    BOF3_INJECT(EffectKind0B_Start);
    BOF3_INJECT(EffectKind0B_Rise);
    BOF3_INJECT(EffectKind0B_Drop);
    BOF3_INJECT(EffectKind0B_Attach);
    BOF3_INJECT(EffectKind0B_Follow);
    BOF3_INJECT(EffectKind0B_DrawQuad);
    BOF3_INJECT(EffectKind02_Start);
    BOF3_INJECT(EffectKind02_SlideIn);
    BOF3_INJECT(EffectKind02_Body);
    BOF3_INJECT(EffectKind02_SlideOut);
    BOF3_INJECT(EffectKind02_RunMode1);
    BOF3_INJECT(EffectKind02_RunMode3);
    BOF3_INJECT(EffectKind02_Mode1Tint);
    BOF3_INJECT(EffectKind02_Mode1Brighten);
    BOF3_INJECT(EffectKind02_CopyLeader);
    BOF3_INJECT(EffectKind02_Mode3Tint);
    BOF3_INJECT(EffectKind02_Mode3Darken);
    BOF3_INJECT(EffectKind02_Mode3Wait);
}
