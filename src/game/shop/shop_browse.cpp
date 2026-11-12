// Moved here by tools/move_funcs.py from field_s.cpp, rest_2c.cpp (docs/source-layout-round.md).
#include "game/shop/shop_browse.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_s.h"
#include "game/field_s_callees.h"
#include "game/field/field_idle.h"   // DIV-0223: g_master_pending
#include "game/lang_layout.h"
#include "game/menu_frame.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2c.h"
#include "game/rest_2c_callees.h"
#include "game/scenario_harness.h"
#include "game/shop/shop_browse_callees.h"
#include "game/yes_no_layout.h"
#include "hook/behaviour.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = shop_browse::at;

}  // namespace

namespace {

using U = std::uint32_t;

unsigned char* At(U address) { return move_script::At(address); }

unsigned char& B(U address) { return *At(address); }

void SetW(U address, U v) { move_script::SetWord(At(address), v); }

}  // namespace

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

constexpr auto& AtRest2c = move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char& BRest2c(U address) { return *AtRest2c(address); }

U LRest2c(U address) { return static_cast<U>(Long(AtRest2c(address))); }

void PutW(U address, unsigned v) { SetWord(AtRest2c(address), v); }

// A state table's handler, jumped to as the original's `jmp [table + 4 *
// index]` does: the index must lie inside the table's own count (its reader's
// reach, docs/rest_2c.md section 3). Past it the original jumps through the
// next table's cell; no state writes such an index (section 5), so ours
// aborts. The entry is called as the cell holds it: Capcom's address in the
// game (Inject's jmp to ours where it is ours), a recorder while the fuzz runs.
using Handler = void (__cdecl*)();

void Jump(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(LRest2c(table + 4 * index)))();
}

// The window record n's byte / word at +off.
U Win(unsigned n, unsigned off) { return ::bof3::Img(WindowRecords_addr) + n * rest_2c::at::kWindowStride + off; }

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x583350, ShopMode_States[9]: `jmp [ShopBrowse_States + 4 * 0x929F01]`.
extern "C" void __cdecl ShopBrowse_Dispatch(void) { Jump(::bof3::Img(ShopBrowse_States_addr), 3, FieldS_State, "ShopBrowse_Dispatch"); }

// original 0x583360, state 0: `jmp [ShopBrowse_OpenSteps + 4 * 0x929F02]`.
extern "C" void __cdecl ShopBrowse_OpenStep(void) { Jump(::bof3::Img(ShopBrowse_OpenSteps_addr), 2, FieldS_Step, "ShopBrowse_OpenStep"); }

// original 0x583370, open step 0: ShopBrowse_InitWindows, the step up (read
// after it), the timer 6, Sound_PlayEffect(0x102).
extern "C" void __cdecl ShopBrowse_Open(void) {
    SH_CALL(ShopBrowse_InitWindows)();
    const auto step = static_cast<unsigned char>(FieldS_Step + 1);
    BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = 6;
    FieldS_Step = step;
    SH_CALL(Sound_PlayEffect)(0x102);
}

// original 0x5833A0, open step 1: the timer down; at 0 the step 0 and the
// state up.
extern "C" void __cdecl ShopBrowse_OpenWait(void) {
    const auto timer = static_cast<unsigned char>(BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) - 1);
    BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = timer;
    if (timer != 0) return;
    const auto state = static_cast<unsigned char>(FieldS_State + 1);
    FieldS_Step = 0;
    FieldS_State = state;
}

// original 0x5833D0, state 1: `jmp [ShopBrowse_ChooseSteps + 4 * 0x929F02]`.
extern "C" void __cdecl ShopBrowse_ChooseStep(void) { Jump(::bof3::Img(ShopBrowse_ChooseSteps_addr), 3, FieldS_Step, "ShopBrowse_ChooseStep"); }

// original 0x5833E0, choose step 0: the list in window 1 (+0xA the first
// shown, +0xB the cursor, +8 the scroll, +0xD the entry under it). The
// hand (window 21) at window 1's x + 4 and y + 13 * (cursor - first + 2) -
// both 16-bit stores - and on; window 8's +0xA 0, window 0's +0x10 0x32.
// Input_AutoRepeat(Input_Pressed & 0xF00C); the cursor read after it (and kept
// to compare). Up (0x1000): the cursor back one above 0, and the scroll 0xF0
// when it went above the first shown. Else down (0x4000): on one below 0x10,
// the scroll 0x10 when it reached the first + 9 (as ints). Else 0x0004 (a
// page back): from the first at 0 the cursor 0; first below 9 - the cursor
// less it and the first 0; else both less 9. Else 0x0008 (a page on): the
// first at 8 - the cursor 0x10; else the cursor plus 8 - first, the first 8.
// A moved cursor Sound_PlayEffect(0x100). While scrolling (+8 not 0)
// nothing more. Then Input_Pressed read again: a confirm button - with an
// entry (+0xD not 0xFF) 0x103, 0x102, ShopBrowse_OpenDetail, the hand off,
// window 1 +3 = 3, the timer 8 and the step up (read before the stores); no
// entry 0x107. Else a cancel button: 0x106, 0x102, the hand off, windows 0
// and 1 +3 = 1, the timer 0xA, the state up (read before) and the step 0.
extern "C" void __cdecl ShopBrowse_Choose(void) {
    PutW(Win(21, 4), LRest2c(Win(1, 4)) + 4);
    BRest2c(Win(8, 0xA)) = 0;
    const unsigned rows = static_cast<unsigned>(BRest2c(Win(1, 0xB))) - BRest2c(Win(1, 0xA)) + 2;
    PutW(Win(0, 0x10), 0x32);
    BRest2c(Win(21, 0)) = 1;
    PutW(Win(21, 6), (rows * 13 + Word(AtRest2c(Win(1, 6)))) & 0xFFFF);
    const unsigned keys = SH_CALL(Input_AutoRepeat)(Input_Pressed & Sigma(0xF00C));
    unsigned char cursor = BRest2c(Win(1, 0xB));
    const unsigned char kept = cursor;
    if (keys & 0x1000) {
        if (cursor > 0) {
            --cursor;
            BRest2c(Win(1, 0xB)) = cursor;
        }
        if (cursor < BRest2c(Win(1, 0xA))) BRest2c(Win(1, 8)) = 0xF0;
    } else if (keys & 0x4000) {
        if (cursor < 0x10) {
            ++cursor;
            BRest2c(Win(1, 0xB)) = cursor;
        }
        if (static_cast<int>(cursor) >= static_cast<int>(BRest2c(Win(1, 0xA))) + 9) BRest2c(Win(1, 8)) = 0x10;
    } else if (keys & Sigma(0x0004)) {
        const unsigned char first = BRest2c(Win(1, 0xA));
        if (first == 0) {
            cursor = 0;
            BRest2c(Win(1, 0xB)) = cursor;
        } else if (first < 9) {
            cursor = static_cast<unsigned char>(cursor - first);
            BRest2c(Win(1, 0xA)) = 0;
            BRest2c(Win(1, 0xB)) = cursor;
        } else {
            cursor = static_cast<unsigned char>(cursor - 9);
            BRest2c(Win(1, 0xB)) = cursor;
            BRest2c(Win(1, 0xA)) = static_cast<unsigned char>(first - 9);
        }
    } else if (keys & Sigma(0x0008)) {
        const unsigned char first = BRest2c(Win(1, 0xA));
        if (first == 8) {
            cursor = 0x10;
            BRest2c(Win(1, 0xB)) = cursor;
        } else {
            BRest2c(Win(1, 0xA)) = 8;
            cursor = static_cast<unsigned char>(cursor + static_cast<unsigned char>(8 - first));
            BRest2c(Win(1, 0xB)) = cursor;
        }
    }
    if (kept != cursor) SH_CALL(Sound_PlayEffect)(0x100);
    if (BRest2c(Win(1, 8)) != 0) return;
    const unsigned pressed = Input_Pressed;
    // The cancel's sounds and stores: the list closes (ShopBrowse_End then
    // ShopMode 1, the field).
    const auto close = [] {
        SH_CALL(Sound_PlayEffect)(0x106);
        SH_CALL(Sound_PlayEffect)(0x102);
        const auto state = static_cast<unsigned char>(FieldS_State + 1);
        BRest2c(Win(21, 0)) = 0;
        BRest2c(Win(0, 3)) = 1;
        BRest2c(Win(1, 3)) = 1;
        BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = 0xA;
        FieldS_State = state;
        FieldS_Step = 0;
    };
    if (Field_ConfirmButtons & pressed) {
        const unsigned char entry = BRest2c(Win(1, 0xD));
        if (entry == 0xFF) {
            SH_CALL(Sound_PlayEffect)(0x107);
            return;
        }
        // DIV-0223 CampMasters. The Definitive Edition romhack (evidence: the
        // patched US disc, SHOP.EMI 0x801E2394, this function's twin
        // 0x801E2150): the confirm's two sounds replaced by a store of the
        // entry + 1 to its byte 0x801AE318 and a jump into the cancel path
        // 0x801E2410 - the master is talked to on the field (FieldIdle_Tick),
        // and no detail page opens.
        if (bof3::BehaviourOn(bof3::behaviours::Id::CampMasters)) {
            field_idle::g_master_pending = static_cast<std::uint8_t>(entry + 1);
            close();
            return;
        }
        SH_CALL(Sound_PlayEffect)(0x103);
        SH_CALL(Sound_PlayEffect)(0x102);
        SH_CALL(ShopBrowse_OpenDetail)();
        const auto step = static_cast<unsigned char>(FieldS_Step + 1);
        BRest2c(Win(21, 0)) = 0;
        BRest2c(Win(1, 3)) = 3;
        BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = 8;
        FieldS_Step = step;
        return;
    }
    if ((Field_CancelButtons & pressed) == 0) return;
    close();
}

// original 0x5835F0, choose step 1 (the entry's detail, ShopBrowse_OpenDetail's
// windows 2 and 3): window 0's +0x10 = the entry (+0xD) + 0x122 as a word,
// window 1's +0xC = its cursor; a timer above 0 runs down and nothing more.
// At 0, a cancel button: 0x106, 0x102, windows 2 and 3 +3 = 1, window 1 +3 =
// 2 and +0xC = 0x7F, the timer 8, the step up (read before the stores).
extern "C" void __cdecl ShopBrowse_Detail(void) {
    PutW(Win(0, 0x10), (BRest2c(Win(1, 0xD)) + 0x122u) & 0xFFFF);
    const unsigned char cursor = BRest2c(Win(1, 0xB));
    const unsigned char timer = BRest2c(::bof3::Img(rest_2c::at::kTimer.v));
    BRest2c(Win(1, 0xC)) = cursor;
    if (timer != 0) {
        BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = static_cast<unsigned char>(timer - 1);
        return;
    }
    if ((Field_CancelButtons & Input_Pressed) == 0) return;
    SH_CALL(Sound_PlayEffect)(0x106);
    SH_CALL(Sound_PlayEffect)(0x102);
    BRest2c(Win(2, 3)) = 1;
    BRest2c(Win(3, 3)) = 1;
    const auto step = static_cast<unsigned char>(FieldS_Step + 1);
    BRest2c(Win(1, 3)) = 2;
    BRest2c(Win(1, 0xC)) = 0x7F;
    BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = 8;
    FieldS_Step = step;
}

// original 0x583680, choose step 2: the timer down; at 0 the step two back
// (to the list).
extern "C" void __cdecl ShopBrowse_DetailClose(void) {
    const auto timer = static_cast<unsigned char>(BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) - 1);
    BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = timer;
    if (timer == 0) FieldS_Step = static_cast<unsigned char>(FieldS_Step + 0xFE);
}

// original 0x5836A0, state 2: `jmp [ShopBrowse_CloseSteps + 4 * 0x929F02]`.
extern "C" void __cdecl ShopBrowse_CloseStep(void) { Jump(::bof3::Img(ShopBrowse_CloseSteps_addr), 3, FieldS_Step, "ShopBrowse_CloseStep"); }

// original 0x5836B0, close step 1 (step 0 is FS's Menu_StepAfterTimer): the
// step up.
extern "C" void __cdecl ShopBrowse_CloseNext(void) { ++FieldS_Step; }

// original 0x5836C0, close step 2: mode 1, the state and the step 0.
extern "C" void __cdecl ShopBrowse_End(void) {
    BattleE1_Mode = 1;
    FieldS_State = 0;
    FieldS_Step = 0;
}

#pragma clang attribute pop

// original 0x5836E0: three windows of WindowRecords set up - record 0
// (0x803160: on, kind 7, +2 0, +3 2, at (0x14, -0x14), help word 0), record 21
// (0x803454: kind 7, +2 3, off) and record 1 (0x803184: kind 7, +2 0xF, +3 2,
// +0xA / +0xB / +0xD 0, +0xC 0x7F, +8 0, at (-0x64, 0x3E), on). Called by
// 0x583370.
extern "C" void __cdecl ShopBrowse_InitWindows(void) {
    records::WindowRecord* const win0 = &WindowRecords[0];
    records::WindowRecord* const win1 = &WindowRecords[1];
    records::WindowRecord* const win21 = &WindowRecords[0x15];
    win0->handler = 7;
    win0->kind = 0;
    win0->state = 2;
    win0->in_use = 1;
    win0->x = 0x14;
    win0->y = 0xFFEC;
    SetW(::bof3::Img(0x803170), 0);
    win21->handler = 7;
    win21->kind = 3;
    win21->in_use = 0;
    win1->handler = 7;
    win1->kind = 0xF;
    win1->state = 2;
    B(::bof3::Img(0x80318F)) = 0;
    B(::bof3::Img(0x80318E)) = 0;
    B(::bof3::Img(0x803190)) = 0x7F;
    B(::bof3::Img(0x803191)) = 0;
    B(::bof3::Img(0x80318C)) = 0;
    win1->x = 0xFF9C;
    win1->y = 0x3E;
    win1->in_use = 1;
}

// original 0x583770: two more windows - record 2 (0x8031A8: kind 7, +2 0x10,
// +3 2, at (0x140, 0x3F), on, +0xA the chosen entry 0x803191) and record 3
// (0x8031CC: kind 7, +2 0x11, +3 2, at (0xB4, 0xF0), on). Called by 0x5833E0
// on a confirm.
extern "C" void __cdecl ShopBrowse_OpenDetail(void) {
    records::WindowRecord* const win2 = &WindowRecords[2];
    records::WindowRecord* const win3 = &WindowRecords[3];
    B(::bof3::Img(0x8031B2)) = B(::bof3::Img(0x803191));
    win2->handler = 7;
    win2->kind = 0x10;
    win2->state = 2;
    win2->x = 0x140;
    win2->y = 0x3F;
    win2->in_use = 1;
    win3->handler = 7;
    win3->kind = 0x11;
    win3->state = 2;
    win3->x = 0xB4;
    win3->y = 0xF0;
    win3->in_use = 1;
}

void ShopBrowse_Inject() {
    BOF3_INJECT(ShopBrowse_Dispatch);
    BOF3_INJECT(ShopBrowse_OpenStep);
    BOF3_INJECT(ShopBrowse_Open);
    BOF3_INJECT(ShopBrowse_OpenWait);
    BOF3_INJECT(ShopBrowse_ChooseStep);
    BOF3_INJECT(ShopBrowse_Choose);
    BOF3_INJECT(ShopBrowse_Detail);
    BOF3_INJECT(ShopBrowse_DetailClose);
    BOF3_INJECT(ShopBrowse_CloseStep);
    BOF3_INJECT(ShopBrowse_CloseNext);
    BOF3_INJECT(ShopBrowse_End);
    BOF3_INJECT(ShopBrowse_InitWindows);
    BOF3_INJECT(ShopBrowse_OpenDetail);
}
