// Moved here by tools/move_funcs.py from field_s.cpp, rest_2c.cpp, shop_states2.cpp (docs/source-layout-round.md).
#include "game/shop/shared_list.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_s.h"
#include "game/field_s_callees.h"
#include "game/lang_layout.h"
#include "game/menu_frame.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2c.h"
#include "game/rest_2c_callees.h"
#include "game/scenario_harness.h"
#include "game/shop/shared_list_callees.h"
#include "game/shop_states2.h"
#include "game/shop_states2_callees.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = shared_list::at;

}  // namespace

using namespace shop_states2;

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using U = std::uint32_t;

unsigned char* At(U address) { return move_script::At(address); }

unsigned char& B(U address) { return *At(address); }

U W(U address) { return move_script::Word(At(address)); }

void SetW(U address, U v) { move_script::SetWord(At(address), v); }

U L(U address) { return static_cast<U>(move_script::Long(At(address))); }

int S8(U v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

// CharacterRecords' record n (stride 0xA4), unchecked.
unsigned char* Rec(U n) { return At(::bof3::Img(CharacterRecords_addr) + n * field_s::at::kRecordStride); }

U RecAddr(U n) { return ::bof3::Img(CharacterRecords_addr) + n * field_s::at::kRecordStride; }

U Pressed() { return Input_Pressed; }

bool Confirmed(U pressed) { return (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed & 0xFFFF) != 0; }

bool Cancelled(U pressed) { return (static_cast<std::uint32_t>(Field_CancelButtons) & pressed & 0xFFFF) != 0; }

void Sound(U id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }

char* PrintBuf() { return reinterpret_cast<char*>(At(::bof3::Img(field_s::at::kPrintBuf))); }

const char* Format(U address) { return reinterpret_cast<const char*>(At(address)); }

const unsigned char* Text(U address) { return At(address); }

void DrawFont8(int x, int y, int colour) {
    SH_CALL(Text_DrawFont8)(x, y, colour, reinterpret_cast<const unsigned char*>(PrintBuf()));
}

// A .data table's word read in place (the fuzz swaps its entries for
// recorders). Outside the fuzz a word that is not code is past the table:
// the original jumps there; ours aborts (docs/field_s.md section 6).
U CodeAt(U table, U index, const char* who) {
    const U cell = table + 4 * index;
    const U entry = L(cell);
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    (unsigned)index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return entry;
}

using Handler = void (__cdecl*)();

// The swap helper 0x58BD50 (nobody's): the two bytes exchanged.
void SwapBytes(U a, U b) { SH_AT(void (__cdecl*)(unsigned char*, unsigned char*), field_s::at::kSwapBytes.v)(At(a), At(b)); }

// The five-byte stretch of window stores two SharedList states and
// SharedList_UseItem make (windows 2 and 3 of WindowRecords, 0x8031A8 and
// 0x8031CC): the member list and the member's slots, the joined record `rec`
// shown in both.
void OpenMemberWindows(unsigned char rec) {
    records::WindowRecord* const win2 = &WindowRecords[2];
    records::WindowRecord* const win3 = &WindowRecords[3];
    win2->handler = 7;
    win2->kind = 0xD;
    B(::bof3::Img(0x8031B2)) = rec;
    B(::bof3::Img(0x8031D6)) = rec;
    win2->state = 2;
    win2->in_use = 1;
    win2->x = 0xFF6A;
    win2->y = 0x74;
    win3->handler = 7;
    win3->kind = 0xC;
    win3->state = 2;
    win3->in_use = 1;
    B(::bof3::Img(0x8031D7)) = 0;
    B(::bof3::Img(0x8031D8)) = 0xFF;
    B(::bof3::Img(0x8031D9)) = 0;
    win3->x = 0x140;
    win3->y = 0x3E;
}

// The scroll of a 128-entry list shown nine rows at a time (SharedList_PickShared
// and SharedList_Arrange, whose code is the same to the byte here): the cursor
// `cur` and the top `top` bytes, the scroll state `moving` set to 0xF0 / 0x10
// when the cursor leaves the page up / down. Bits of the repeat: 0x1000 up,
// 0x4000 down, 4 a page up, 8 a page down. Answers the cursor as the
// original's cl holds it (equal to the byte).
unsigned char ScrollList(U repeat, U cur, U top, U moving) {
    unsigned char cl = B(cur);
    if (repeat & 0x1000) {
        if (cl != 0) {
            --cl;
            B(cur) = cl;
        }
        if (cl < B(top)) B(moving) = 0xF0;
    } else if (repeat & 0x4000) {
        if (cl < 0x7F) {
            ++cl;
            B(cur) = cl;
        }
        if (static_cast<int>(cl) >= static_cast<int>(B(top)) + 9) B(moving) = 0x10;
    } else if (repeat & Sigma(4)) {
        unsigned char al = B(top);
        if (al == 0) {
            cl = 0;
            B(cur) = 0;
        } else if (al < 9) {
            cl = static_cast<unsigned char>(cl - al);
            B(top) = 0;
            B(cur) = cl;
        } else {
            cl = static_cast<unsigned char>(cl - 9);
            al = static_cast<unsigned char>(al - 9);
            B(cur) = cl;
            B(top) = al;
        }
    } else if (repeat & Sigma(8)) {
        unsigned char al = B(top);
        if (al == 0x77) {
            cl = 0x7F;
            B(cur) = 0x7F;
        } else if (al > 0x6E) {
            const auto bl = static_cast<unsigned char>(0x77 - al);
            B(top) = 0x77;
            cl = static_cast<unsigned char>(cl + bl);
            B(cur) = cl;
        } else {
            cl = static_cast<unsigned char>(cl + 9);
            al = static_cast<unsigned char>(al + 9);
            B(cur) = cl;
            B(top) = al;
        }
    }
    return cl;
}

}  // namespace

// The two sorts' pass: adjacent non-empty entries swapped when their abilities'
// byte +0x12 compare as `swap` says.
template <typename Swap> void SortShared(Swap swap) {
    for (U limit = 0x7F; limit != 0; --limit)
        for (U i = 0; i < limit; ++i) {
            const U e = ::bof3::Img(field_s::at::kSharedList) + i;
            const unsigned char a = B(e);
            if (a == 0) continue;
            const unsigned char b = B(e + 1);
            if (b == 0) continue;
            if (swap(B(::bof3::Img(field_s::at::kAbilityRecords) + 24u * a + 0x12), B(::bof3::Img(field_s::at::kAbilityRecords) + 24u * b + 0x12))) SwapBytes(e, e + 1);
        }
}

namespace {

constexpr auto& AtRest2c = move_script::At;

using move_script::Long;

U LRest2c(U address) { return static_cast<U>(Long(AtRest2c(address))); }

void Jump(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(LRest2c(table + 4 * index)))();
}

unsigned char* AtShopStates2(shop_states2::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

shop_states2::U ByteShopStates2(shop_states2::U address) { return AtShopStates2(address)[0]; }

void PutByteShopStates2(shop_states2::U address, shop_states2::U v) { AtShopStates2(address)[0] = static_cast<unsigned char>(v); }

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x584180, ShopMode_States[7]: `jmp [SharedList_States + 4 * 0x929F01]`.
extern "C" void __cdecl SharedList_Dispatch(void) { Jump(::bof3::Img(SharedList_States_addr), 5, FieldS_State, "SharedList_Dispatch"); }

#pragma clang attribute pop

// original 0x584190 (PSX 0x801DF8A8): SharedList_States[0]. The set-up, the choice 0, the
// state up one.
extern "C" void __cdecl SharedList_Begin(void) {
    SH_CALL(SharedList_Setup)();
    const auto state = FieldS_State;
    FieldS_SharedChoice = 0;
    FieldS_State = static_cast<unsigned char>(state + 1);
}

// original 0x5841B0: SharedList_States[1], the choice (0x6BC8C4). Window 1's
// cursor 0xFF, the help word 0x73 + choice (s8); with window 1 open (+3 0) the
// hand at (window 1's x + 48 choice, its y + 4) on, else off. The repeat of
// 0xA000 flips the choice with sound 0x101. Confirm on 1: without key item 0xF
// and none of item 0x58 - sound 0x107; without the key item - 0x929F0B 1 and
// the state up one (the use); with it - sound 0x102, windows 2 and 3 open on
// the first joined record, the pick 0, the state up one and step 1, sound
// 0x103, window 1's cursor the choice, counter 5. Confirm on 0: sounds 0x104
// and 0x102, window 4 (0x8031F0) open on the list's top, the state up two;
// then window 1's cursor the choice, sound 0x103, counter 5, step 0. Cancel:
// sound 0x106, windows 0 and 1 closing (+3 1), the hand off, window 5's +3 1
// without the key item, counter 5, state 4 (the close).
extern "C" void __cdecl SharedList_Menu(void) {
    records::WindowRecord* const win0 = &WindowRecords[0];
    records::WindowRecord* const win1 = &WindowRecords[1];
    records::WindowRecord* const win2 = &WindowRecords[2];
    records::WindowRecord* const win3 = &WindowRecords[3];
    records::WindowRecord* const win4 = &WindowRecords[4];
    records::WindowRecord* const win6 = &WindowRecords[6];
    records::WindowRecord* const win21 = &WindowRecords[0x15];
    const U choice = FieldS_SharedChoice;
    B(::bof3::Img(0x80318F)) = 0xFF;
    SetW(::bof3::Img(0x803170), choice + 0x73);
    if (win1->state == 0) {
        const U x = 48 * choice + L(::bof3::Img(0x803188));
        const U y = static_cast<U>(win1->y) + 4;
        win21->in_use = 1;
        win21->x = x;
        win21->y = y;
    } else {
        win21->in_use = 0;
    }
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0xA000);
    if (repeat & 0xA000) {
        Sound(0x101);
        FieldS_SharedChoice = static_cast<unsigned char>(static_cast<unsigned char>(FieldS_SharedChoice) ^ 1);
    }
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        if (static_cast<unsigned char>(FieldS_SharedChoice) != 0) {
            if ((SH_CALL(KeyItem_Has)(field_s::at::kKeyItemInk) & 0xFF) == 0 &&
                (SH_CALL(Inventory_Count)(0, field_s::at::kInkItem, 0) & 0xFFFF) == 0) {
                Sound(0x107);
                return;
            }
            if ((SH_CALL(KeyItem_Has)(field_s::at::kKeyItemInk) & 0xFF) == 0) {
                const auto state = FieldS_State;
                FieldS_Answer = 1;
                FieldS_State = static_cast<unsigned char>(state + 1);
            } else {
                Sound(0x102);
                const unsigned char first = B(::bof3::Img(field_s::at::kJoined));
                const auto state = static_cast<unsigned char>(FieldS_State + 1);
                win2->handler = 7;
                win3->handler = 7;
                FieldS_PickIndex = 0;
                win2->kind = 0xD;
                win2->state = 2;
                win2->in_use = 1;
                B(::bof3::Img(0x8031B2)) = first;
                win2->x = 0xFF6A;
                win2->y = 0x74;
                win3->kind = 0xC;
                win3->state = 2;
                win3->in_use = 1;
                B(::bof3::Img(0x8031D6)) = first;
                B(::bof3::Img(0x8031D7)) = 0;
                B(::bof3::Img(0x8031D8)) = 0xFF;
                B(::bof3::Img(0x8031D9)) = 0;
                win3->x = 0x140;
                win3->y = 0x3E;
                FieldS_State = state;
                FieldS_Step = 1;
                Sound(0x103);
                B(::bof3::Img(0x80318F)) = static_cast<unsigned char>(FieldS_SharedChoice);
                B(::bof3::Img(field_s::at::kTimer)) = 5;
                return;
            }
        } else {
            Sound(0x104);
            Sound(0x102);
            const auto state = static_cast<unsigned char>(FieldS_State + 2);
            win4->handler = 7;
            FieldS_State = state;
            win4->kind = 0xB;
            win4->state = 3;
            win4->in_use = 1;
            B(::bof3::Img(0x8031FA)) = 0;
            B(::bof3::Img(0x8031FB)) = 0;
            B(::bof3::Img(0x8031FC)) = 0xFF;
            B(::bof3::Img(0x8031FD)) = 0;
            win4->x = 0xFF56;
            win4->y = 0x3E;
        }
        B(::bof3::Img(0x80318F)) = static_cast<unsigned char>(FieldS_SharedChoice);
        Sound(0x103);
        B(::bof3::Img(field_s::at::kTimer)) = 5;
        FieldS_Step = 0;
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    win0->state = 1;
    win1->state = 1;
    win21->in_use = 0;
    if ((SH_CALL(KeyItem_Has)(field_s::at::kKeyItemInk) & 0xFF) == 0) win6->state = 1;
    B(::bof3::Img(field_s::at::kTimer)) = 5;
    FieldS_State = 4;
}

// original 0x584460 (PSX 0x801DFCAC): SharedList_States[2], a tail jump through
// SharedList_MoveSteps 0x664268 by the step byte, unchecked.
extern "C" void __cdecl SharedList_MoveStep(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(CodeAt(::bof3::Img(SharedList_MoveSteps_addr), FieldS_Step, "SharedList_MoveStep"))();
}

extern "C" void __cdecl SharedList_UseItem(void) {
    records::WindowRecord* const win0 = &WindowRecords[0];
    records::WindowRecord* const win21 = &WindowRecords[0x15];
    const U hand_x = g_use_item_yes_no_hand
                         ? static_cast<U>(YesNoLayout_ShopHandX(static_cast<short>(static_cast<U>(win0->x)) + 7, FieldS_Answer))
                         : static_cast<U>(win0->x) - 36u * static_cast<std::uint32_t>(FieldS_Answer) + 0xE8;
    const U hand_y = static_cast<U>(win0->y) + 5;
    U pressed = L(::bof3::Img(field_s::at::kPressed));
    SetW(::bof3::Img(0x803170), 0x36);
    win21->in_use = 1;
    win21->x = hand_x;
    win21->y = hand_y;
    if (pressed & 0xA000) {
        Sound(0x101);
        FieldS_Answer = static_cast<unsigned char>(static_cast<unsigned char>(FieldS_Answer) ^ 1);
        pressed = L(::bof3::Img(field_s::at::kPressed));
    }
    if (Confirmed(pressed)) {
        if (static_cast<unsigned char>(FieldS_Answer) != 0) {
            Sound(0x104);
            Sound(0x102);
            FieldS_PickIndex = 0;
            SH_CALL(Inventory_Remove)(0, field_s::at::kInkItem, 1);
            const unsigned char rec = B(::bof3::Img(field_s::at::kJoined) + FieldS_PickIndex);
            const auto step = static_cast<unsigned char>(FieldS_Step + 1);
            OpenMemberWindows(rec);
            FieldS_Step = step;
            return;
        }
    } else if (!Cancelled(pressed)) {
        return;
    }
    Sound(0x106);
    FieldS_State = static_cast<unsigned char>(FieldS_State - 1);
}

// original 0x5845E0 (PSX 0x801DFF24): SharedList_MoveSteps[1], and ShopMode_States[9]'s step
// table 0x6641B0 [0]: the counter down one; at 0 the step up one.
extern "C" void __cdecl Menu_StepAfterTimer(void) {
    const auto t = static_cast<unsigned char>(B(::bof3::Img(field_s::at::kTimer)) - 1);
    B(::bof3::Img(field_s::at::kTimer)) = t;
    if (t == 0) FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// original 0x584600: SharedList_MoveSteps[2], the member. Its record's name
// to text record 0 (TextRecord_Set(0, 8, record)); the help word 0x75; with
// window 3 open (+3 0) the hand at its (x + 0x2A, y + 7), else off. The repeat
// of 0xF000: 0x8000 back (sound 0x101, window 3's +9 0x32; below 0, s8, to the
// joined count less one), 0x2000 on (+9 0x31; past the count less one to 0).
// Windows 2 and 3 on the joined record at the pick. Confirm: sounds 0x103 and
// 0x102, window 4 open on the list (its top and cursor 0), window 2 +3 1,
// window 3 +3 3, the step up one. Cancel: sound 0x106, windows 2 and 3 +3 1,
// the state down one.
extern "C" void __cdecl SharedList_PickMember(void) {
    records::WindowRecord* const win2 = &WindowRecords[2];
    records::WindowRecord* const win3 = &WindowRecords[3];
    records::WindowRecord* const win4 = &WindowRecords[4];
    records::WindowRecord* const win21 = &WindowRecords[0x15];
    SH_CALL(TextRecord_Set)(0, 8, Rec(B(::bof3::Img(field_s::at::kJoined) + FieldS_PickIndex)));
    const unsigned char open = win3->state;
    SetW(::bof3::Img(0x803170), 0x75);
    if (open == 0) {
        const U x = L(::bof3::Img(0x8031D0)) + 0x2A;
        const U y = static_cast<U>(win3->y) + 7;
        win21->in_use = 1;
        win21->x = x;
        win21->y = y;
    } else {
        win21->in_use = 0;
    }
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0xF000);
    if (repeat & 0x8000) {
        Sound(0x101);
        const auto p = static_cast<unsigned char>(FieldS_PickIndex - 1);
        B(::bof3::Img(0x8031D9)) = 0x32;
        FieldS_PickIndex = p;
        if (S8(p) < 0) FieldS_PickIndex = static_cast<unsigned char>(FieldS_JoinedCount - 1);
    } else if (repeat & 0x2000) {
        Sound(0x101);
        const auto p = static_cast<unsigned char>(FieldS_PickIndex + 1);
        const int last = FieldS_JoinedCount - 1;
        FieldS_PickIndex = p;
        B(::bof3::Img(0x8031D9)) = 0x31;
        if (FieldS_PickIndex > last) FieldS_PickIndex = 0;
    }
    const unsigned char rec = B(::bof3::Img(field_s::at::kJoined) + FieldS_PickIndex);
    B(::bof3::Img(0x8031B2)) = rec;
    B(::bof3::Img(0x8031D6)) = rec;
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        Sound(0x103);
        Sound(0x102);
        const auto step = static_cast<unsigned char>(FieldS_Step + 1);
        B(::bof3::Img(0x8031FA)) = 0;
        B(::bof3::Img(0x8031FB)) = 0;
        B(::bof3::Img(0x8031FD)) = 0;
        win2->state = 1;
        B(::bof3::Img(0x8031D8)) = 0xFF;
        win3->state = 3;
        win4->handler = 7;
        win4->kind = 0xB;
        win4->state = 2;
        win4->in_use = 1;
        B(::bof3::Img(0x8031FC)) = 0xFF;
        win4->x = 0x140;
        win4->y = 0x3E;
        FieldS_Step = step;
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    const auto state = static_cast<unsigned char>(FieldS_State - 1);
    win2->state = 1;
    win3->state = 1;
    FieldS_State = state;
}

// original 0x5847F0: SharedList_MoveSteps[3], the member's slot (window 3's
// +0xB of ten). The help word of the ability in the slot (Ability_Records
// +0x16); with window 3 open the hand at its (x + 7, y + 13 (slot + 2)). The
// repeat of 0xF000: 0x1000 up (below 0, s8, to 0), 0x4000 down (above 9 to
// 9); sound 0x100 when it moved. Window 3's +0xC 0xFF. Confirm: sound 0x103,
// +0xC the slot, the step up one. Pressed bit 4: an ability in the slot goes
// to the shared list (sound 0x103, AbilityList_Add(id, 0, 1, 0), the slot 0),
// an empty slot buzzes (0x107). Cancel: sounds 0x102 and 0x106, windows 2 +3 2,
// 3 +3 4 and +0xC 0xFF, 4 +3 1, the step down one.
extern "C" void __cdecl SharedList_PickSlot(void) {
    records::WindowRecord* const win2 = &WindowRecords[2];
    records::WindowRecord* const win3 = &WindowRecords[3];
    records::WindowRecord* const win4 = &WindowRecords[4];
    records::WindowRecord* const win21 = &WindowRecords[0x15];
    const U member_rec = B(::bof3::Img(0x8031D6));
    unsigned char slot = B(::bof3::Img(0x8031D7));
    const U ability = B(RecAddr(member_rec) + 0x7E + slot);
    SetW(::bof3::Img(0x803170), W(::bof3::Img(field_s::at::kAbilityRecords) + 0x16 + 24 * ability));
    if (win3->state == 0) {
        const U x = L(::bof3::Img(0x8031D0)) + 7;
        win21->x = x;
        win21->in_use = 1;
        win21->y = 13 * (slot + 2u) + static_cast<U>(win3->y);
    } else {
        win21->in_use = 0;
    }
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0xF000);
    unsigned char cl = B(::bof3::Img(0x8031D7));
    const unsigned char before = cl;
    if (repeat & 0x1000) {
        --cl;
        B(::bof3::Img(0x8031D7)) = cl;
        if (S8(cl) < 0) {
            cl = 0;
            B(::bof3::Img(0x8031D7)) = 0;
        }
    } else if (repeat & 0x4000) {
        ++cl;
        B(::bof3::Img(0x8031D7)) = cl;
        if (cl > 9) {
            cl = 9;
            B(::bof3::Img(0x8031D7)) = 9;
        }
    }
    if (before != cl) {
        Sound(0x100);
        cl = B(::bof3::Img(0x8031D7));
    }
    B(::bof3::Img(0x8031D8)) = 0xFF;
    const U cell = RecAddr(B(::bof3::Img(0x8031D6))) + 0x7E + cl;
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        Sound(0x103);
        B(::bof3::Img(0x8031D8)) = B(::bof3::Img(0x8031D7));
        FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
        return;
    }
    if (pressed & Sigma(0x10)) {   // DIVERGENCE DIV-0139: Japan's triangle through σ (0x58491F)
        if (B(cell) != 0) {
            Sound(0x103);
            SH_CALL(AbilityList_Add)(B(cell), 0, 1, 0);
            B(cell) = 0;
            return;
        }
        Sound(0x107);
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x102);
    Sound(0x106);
    const auto step = static_cast<unsigned char>(FieldS_Step - 1);
    win2->state = 2;
    win3->state = 4;
    B(::bof3::Img(0x8031D8)) = 0xFF;
    win4->state = 1;
    FieldS_Step = step;
}

// original 0x5849B0: SharedList_MoveSteps[4], the shared list's entry
// (window 4: +0xA the top, +0xB the cursor, +0xD the scroll). The hand on at
// (its x + 7, its y + 13 (cursor - top + 2)); the help word of the entry's
// ability. The repeat of 0x500C scrolls (ScrollList); sound 0x100 when the
// cursor moved. While the list scrolls nothing more. Confirm: an empty entry
// over an empty slot buzzes (0x107); else the slot's byte and the entry's
// swapped, sound 0x103 and the step down one. Cancel: sound 0x106 and the
// step down one.
extern "C" void __cdecl SharedList_PickShared(void) {
    records::WindowRecord* const win4 = &WindowRecords[4];
    records::WindowRecord* const win21 = &WindowRecords[0x15];
    const unsigned char cur = B(::bof3::Img(0x8031FB));
    win21->in_use = 1;
    SetW(::bof3::Img(0x803170), W(::bof3::Img(field_s::at::kAbilityRecords) + 0x16 + 24u * B(::bof3::Img(field_s::at::kSharedList) + cur)));
    win21->x = L(::bof3::Img(0x8031F4)) + 7;
    win21->y = 13 * (static_cast<U>(cur) - B(::bof3::Img(0x8031FA)) + 2) + static_cast<U>(win4->y);
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & Sigma(0x500C));
    const unsigned char before = B(::bof3::Img(0x8031FB));
    unsigned char cl = ScrollList(repeat, ::bof3::Img(0x8031FB), ::bof3::Img(0x8031FA), ::bof3::Img(0x8031FD));
    if (before != cl) {
        Sound(0x100);
        cl = B(::bof3::Img(0x8031FB));
    }
    if (B(::bof3::Img(0x8031FD)) != 0) return;
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        const U slot = RecAddr(B(::bof3::Img(0x8031D6))) + 0x7E + B(::bof3::Img(0x8031D7));
        const unsigned char entry = B(::bof3::Img(field_s::at::kSharedList) + cl);
        if (entry == 0 && B(slot) == 0) {
            Sound(0x107);
            return;
        }
        const unsigned char had = B(slot);
        B(slot) = entry;
        B(::bof3::Img(field_s::at::kSharedList) + B(::bof3::Img(0x8031FB))) = had;
        Sound(0x103);
        FieldS_Step = static_cast<unsigned char>(FieldS_Step - 1);
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    FieldS_Step = static_cast<unsigned char>(FieldS_Step - 1);
}

// original 0x584BB0 (PSX 0x801E0814): SharedList_States[3], a tail jump through
// SharedList_SortSteps 0x66427C by the step byte, unchecked.
extern "C" void __cdecl SharedList_SortStep(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(CodeAt(::bof3::Img(SharedList_SortSteps_addr), FieldS_Step, "SharedList_SortStep"))();
}

// original 0x584BC0 (PSX 0x801E0850): SharedList_SortSteps[0]: the counter down one; at 0
// window 6 (0x803214) open - kind 7, +2 0xE, +0xA 0, +0xB 0xFF, at (0xC8,
// 0x3F) - and the step up one.
extern "C" void __cdecl SharedList_SortOpen(void) {
    records::WindowRecord* const win5 = &WindowRecords[5];
    const auto t = static_cast<unsigned char>(B(::bof3::Img(field_s::at::kTimer)) - 1);
    B(::bof3::Img(field_s::at::kTimer)) = t;
    if (t != 0) return;
    const auto step = FieldS_Step;
    win5->handler = 7;
    win5->kind = 0xE;
    win5->in_use = 1;
    B(::bof3::Img(0x80321E)) = 0;
    B(::bof3::Img(0x80321F)) = 0xFF;
    win5->x = 0xC8;
    win5->y = 0x3F;
    FieldS_Step = static_cast<unsigned char>(step + 1);
}

// original 0x584C10 (PSX 0x801E08DC): SharedList_SortSteps[1], window 6's three rows (+0xA).
// The help word 0x18, the hand at (its x + 5, its y + 16 row + 0x1A). The
// repeat of 0x5000: 0x1000 up (sound 0x101; below 0, s8, to 2), 0x4000 down
// (past 2 to 0). Confirm: sound 0x103; row 0 - +0xB 0 and the step up one
// (the arranging); else SharedList_Sort(row). Cancel: sounds 0x102 and 0x106,
// window 6 off, window 4's +3 4, the state down two.
extern "C" void __cdecl SharedList_SortMenu(void) {
    records::WindowRecord* const win4 = &WindowRecords[4];
    records::WindowRecord* const win5 = &WindowRecords[5];
    records::WindowRecord* const win21 = &WindowRecords[0x15];
    SetW(::bof3::Img(0x803170), 0x18);
    win21->x = static_cast<U>(win5->x) + 5;
    win21->y = 16u * B(::bof3::Img(0x80321E)) + static_cast<U>(win5->y) + 0x1A;
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0x5000);
    if (repeat & 0x1000) {
        Sound(0x101);
        const auto r = static_cast<unsigned char>(B(::bof3::Img(0x80321E)) - 1);
        B(::bof3::Img(0x80321E)) = r;
        if (S8(r) < 0) B(::bof3::Img(0x80321E)) = 2;
    } else if (repeat & 0x4000) {
        Sound(0x101);
        const auto r = static_cast<unsigned char>(B(::bof3::Img(0x80321E)) + 1);
        B(::bof3::Img(0x80321E)) = r;
        if (r > 2) B(::bof3::Img(0x80321E)) = 0;
    }
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        Sound(0x103);
        const unsigned char row = B(::bof3::Img(0x80321E));
        if (row == 0) {
            B(::bof3::Img(0x80321F)) = 0;
            FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
            return;
        }
        SH_CALL(SharedList_Sort)(row);
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x102);
    Sound(0x106);
    const auto state = static_cast<unsigned char>(FieldS_State - 2);
    win5->in_use = 0;
    win4->state = 4;
    FieldS_State = state;
}

// original 0x584D30: SharedList_SortSteps[2], two entries of the shared list
// exchanged by hand (window 4, its +0xC the first pick). The hand on at (its
// x, its y + 13 (cursor - top + 2)), the help word of the entry; the scroll as
// SharedList_PickShared's. While the list scrolls nothing more. Confirm with
// no pick: an entry not 0 is picked (sound 0x103), an empty one buzzes (0x107);
// with a pick: sound 0x103, the pick dropped and the two entries swapped.
// Cancel: sound 0x106; a pick is dropped, or window 6's +0xB 0xFF and the
// step down one.
extern "C" void __cdecl SharedList_Arrange(void) {
    records::WindowRecord* const win4 = &WindowRecords[4];
    records::WindowRecord* const win21 = &WindowRecords[0x15];
    const unsigned char cur = B(::bof3::Img(0x8031FB));
    win21->in_use = 1;
    SetW(::bof3::Img(0x803170), W(::bof3::Img(field_s::at::kAbilityRecords) + 0x16 + 24u * B(::bof3::Img(field_s::at::kSharedList) + cur)));
    win21->x = static_cast<U>(win4->x);
    win21->y = 13 * (static_cast<U>(cur) - B(::bof3::Img(0x8031FA)) + 2) + static_cast<U>(win4->y);
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & Sigma(0x500C));
    const unsigned char before = B(::bof3::Img(0x8031FB));
    unsigned char cl = ScrollList(repeat, ::bof3::Img(0x8031FB), ::bof3::Img(0x8031FA), ::bof3::Img(0x8031FD));
    if (before != cl) {
        Sound(0x100);
        cl = B(::bof3::Img(0x8031FB));
    }
    if (B(::bof3::Img(0x8031FD)) != 0) return;
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        if (B(::bof3::Img(0x8031FC)) == 0xFF) {
            if (B(::bof3::Img(field_s::at::kSharedList) + cl) != 0) {
                Sound(0x103);
                B(::bof3::Img(0x8031FC)) = B(::bof3::Img(0x8031FB));
                return;
            }
            Sound(0x107);
            return;
        }
        Sound(0x103);
        const U first = B(::bof3::Img(0x8031FC));
        B(::bof3::Img(0x8031FC)) = 0xFF;
        const unsigned char a = B(::bof3::Img(field_s::at::kSharedList) + first);
        const U second = B(::bof3::Img(0x8031FB));
        B(::bof3::Img(field_s::at::kSharedList) + first) = B(::bof3::Img(field_s::at::kSharedList) + second);
        B(::bof3::Img(field_s::at::kSharedList) + second) = a;
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    if (B(::bof3::Img(0x8031FC)) != 0xFF) {
        B(::bof3::Img(0x8031FC)) = 0xFF;
        return;
    }
    B(::bof3::Img(0x80321F)) = 0xFF;
    FieldS_Step = static_cast<unsigned char>(FieldS_Step - 1);
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x584F70 (PSX 0x801E0E08): the counter down, and at 0
// Window_ResetAll and the menu ended (0x929F00 = 1). Also entry 4 of
// 0x664264 (group DF's, or no group's: the menu kind 0x584120's states).
extern "C" void __cdecl ShopTrade_Close(void) {
    const U c = (ByteShopStates2(::bof3::Img(shop_states2::at::kCounter.v)) - 1) & 0xFF;
    PutByteShopStates2(::bof3::Img(shop_states2::at::kCounter.v), c);
    if (c != 0) return;
    g.window_reset();
    BattleE1_Mode = 1;
}

#pragma clang attribute pop

// original 0x584F90: the shared list's set-up. The joined records (+0xB bit
// 0) of the eight in order to 0x6BC8BC, their count to 0x6BC8C5 (cleared
// first); window 0 (kind 7, +2 0, +3 2, on, +0xA 0, at (0x14, -0x14)), window 1
// (kind 7, +2 2, +3 2, on, +0xE 4, +0xF 0xFF, at (0x70, -0x14)) and window 21
// (kind 7, +2 3, +0xA 4, off); without key item 0xF, window 5 (0x803238: kind
// 7, +2 0x12, +3 2, on, at (0x140, 0x26)) - the item count's.
extern "C" void __cdecl SharedList_Setup(void) {
    records::WindowRecord* const win0 = &WindowRecords[0];
    records::WindowRecord* const win1 = &WindowRecords[1];
    records::WindowRecord* const win6 = &WindowRecords[6];
    records::WindowRecord* const win21 = &WindowRecords[0x15];
    unsigned char n = 0;
    FieldS_JoinedCount = 0;
    for (unsigned char r = 0; r < 8; ++r)
        if (Rec(r)[0xB] & 1) B(::bof3::Img(field_s::at::kJoined) + n++) = r;
    FieldS_JoinedCount = n;
    win0->handler = 7;
    win0->kind = 0;
    win0->state = 2;
    win0->in_use = 1;
    B(::bof3::Img(0x80316A)) = 0;
    win0->x = 0x14;
    win0->y = 0xFFEC;
    win1->handler = 7;
    win1->kind = 2;
    win1->state = 2;
    win1->in_use = 1;
    B(::bof3::Img(0x80318E)) = 4;
    B(::bof3::Img(0x80318F)) = 0xFF;
    win1->x = 0x70;
    win1->y = 0xFFEC;
    win21->handler = 7;
    win21->kind = 3;
    B(::bof3::Img(0x80345E)) = 4;
    win21->in_use = 0;
    if ((SH_CALL(KeyItem_Has)(field_s::at::kKeyItemInk) & 0xFF) != 0) return;
    win6->handler = 7;
    win6->kind = 0x12;
    win6->state = 2;
    win6->in_use = 1;
    win6->x = 0x140;
    win6->y = 0x26;
}

// original 0x585090: window kind handler (called by 0x59BE50 with the window
// record): the shared list. The box (x + 3, y + 3, 0x99, 0x9A, 0, style); the
// scroll step Menu_ListScroll(+0xA, &offset, &moving, +0xD); from the entry at
// the answered top, rows while row < moving + 9 at y + offset + 0x1A + 13 row:
// a non-empty entry's row (its Ability_Records record as name, cost +0x12,
// kind Skill_FlagIndex) - the rows +0xB and +0xC (each + the moving, 0 while
// the list scrolls up) drawn lit twice (colour 7 dim 1, then two up in
// colour 2 for +0xC's, else 0), the others plain; the title and count boxes
// (x + 3, y + 3, 0x99, 0x14) and (x + 3, y + 0x92, 0x99, 8) in the record's +9;
// the title 0x664288 centred (x + 6 (13 - its length), y + 7, 0, 0x10); the
// count (0x591AC0(0, 0xFF, 0)'s byte) at (x + 0x55, y + 0x92); the frame
// pieces; the scroll bar (0x904574, top +0xA, x + 0x90, y + 0x18, 9, 0x80,
// 0x74).
//
// As the original has it: Menu_ListScroll's offset lands in the low byte of
// the window argument's stack slot; the kinds go out as the flag index over
// a stack dword's upper bytes, the second lit row's colour likewise
// (Menu_DrawSkillRow reads the byte, six bits of the colour); an extra 0x80
// is pushed before 0x591AC0's three and popped with the next calls'. That
// 0x80 is Crt_sprintf's argument after the count: the format 0x6639C8 is
// "%3d/%3d", and its second number reads it - "n/128". Ours passes it
// (known-defects.md D355: until 2026-10-24 ours passed the count alone and
// the second number was whatever our stack held, "0/144" or a negative).
extern "C" void __cdecl SharedList_DrawList(unsigned char* window) {
    auto* const w = rv::As<records::WindowRecord>(window);
    const int wx = static_cast<int>(w->x);
    const int wy = static_cast<int>(w->y);
    SH_CALL(Menu_DrawBox)(wx + 3, wy + 3, 0x99, 0x9A, 0, static_cast<unsigned char>(Config_WindowColour));
    unsigned char offset = 0;   // both written by Menu_ListScroll on every path
    unsigned char moving = 0;
    const unsigned char top = SH_CALL(Menu_ListScroll)(window + 0xA, &offset, &moving, window + 0xD);
    const int y0 = S8(offset) + w->y;
    const unsigned char* p = At(::bof3::Img(field_s::at::kSharedList) + top);
    int y = y0 + 0x1A;
    for (U row = 0; static_cast<int>(row) < static_cast<int>(moving) + 9; ++row, ++p, y += 0xD) {
        const unsigned char id = *p;
        if (id == 0) continue;
        const unsigned char adjust = (window[0xD] & 0xF0) != 0xF0 ? moving : 0;
        const U record = ::bof3::Img(field_s::at::kAbilityRecords) + 24u * id;
        const unsigned char kind = SH_CALL(Skill_FlagIndex)(id);
        const U cursor = static_cast<U>(window[0xC]) + adjust;
        const U at_row = row + window[0xA];
        const int colour = at_row == cursor ? 2 : 0;
        const U picked = static_cast<U>(window[0xB]) + adjust;
        const int x = w->x + 7;
        if (at_row == picked || at_row == cursor) {
            SH_CALL(Menu_DrawSkillRow)(x, y, 7, kind, At(record), B(record + 0x12), 1);
            SH_CALL(Menu_DrawSkillRow)(w->x + 7, y - 2, colour, kind, At(record), B(record + 0x12), 0);
        } else {
            SH_CALL(Menu_DrawSkillRow)(x, y, 0, kind, At(record), B(record + 0x12), 0);
        }
    }
    SH_CALL(Menu_DrawBox)(w->x + 3, w->y + 3, 0x99, 0x14, window[9], static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(Menu_DrawBox)(w->x + 3, w->y + 0x92, 0x99, 8, window[9], static_cast<unsigned char>(Config_WindowColour));
    const int title_y = w->y + 7;
    const unsigned char length = SH_CALL(Text_CharCount)(Text(::bof3::Img(field_s::at::kListTitle)));
    SH_CALL(Text_DrawAt)(6 * (0xD - static_cast<int>(length)) + w->x, title_y, 0, 0x10, Text(::bof3::Img(field_s::at::kListTitle)));
    const U count = SH_AT(U (__cdecl*)(unsigned, unsigned, unsigned), field_s::at::kAbilityListCount.v)(0, 0xFF, 0) & 0xFF;
    SH_CALL(Crt_sprintf)(PrintBuf(), Format(::bof3::Img(field_s::at::kFmtCount)), count, 0x80u);  // D355: the stray push, the total
    DrawFont8(w->x + 0x55, w->y + 0x92, 0);
    SH_CALL(Menu_DrawPieces)(w->x, w->y, Text(::bof3::Img(field_s::at::kPiecesA)), 1);
    SH_CALL(Menu_DrawPieces)(w->x, w->y, Text(::bof3::Img(field_s::at::kPiecesB)), 1);
    for (int i = 0; i < 10; ++i) SH_CALL(Menu_DrawPiece)(w->x + 8 * i + 0x28, w->y, 1, 1);
    for (int i = 0; i < 15; ++i) SH_CALL(Menu_DrawPiece)(w->x, w->y + 8 * i + 0x18, 4, 1);
    SH_CALL(Menu_DrawPiece)(w->x + 0x90, w->y + 0x18, 0x16, 1);
    for (int i = 0; i < 12; ++i) SH_CALL(Menu_DrawPiece)(w->x + 0x90, w->y + 8 * i + 0x28, 0x17, 1);
    SH_CALL(Menu_DrawPiece)(w->x + 0x90, w->y + 0x88, 0x18, 1);
    SH_CALL(Menu_DrawPiece)(w->x, w->y + 0x90, 0x19, 1);
    for (int i = 0; i < 9; ++i) SH_CALL(Menu_DrawPiece)(w->x + 8 * i + 8, w->y + 0x90, 0x1A, 1);
    SH_CALL(Menu_DrawPiece)(w->x + 0x50, w->y + 0x90, 0x1B, 1);
    for (int i = 0; i < 6; ++i) SH_CALL(Menu_DrawPiece)(w->x + 8 * i + 0x58, w->y + 0x90, 0x1C, 1);
    SH_CALL(Menu_DrawPiece)(w->x + 0x88, w->y + 0x90, 0x1D, 1);
    SH_CALL(Menu_DrawScrollBar)(At(::bof3::Img(field_s::at::kSharedList)), window[0xA], w->x + 0x90, w->y + 0x18, 9, 0x80, 0x74);
}

// original 0x585500: window kind handler (called by 0x59BEA0 with the window
// record): a member's ten slots. The box (x + 3, y + 3, 0x92, 0xA0, 0, style);
// for each slot k of record +0xA's +0x7E (read afresh) holding an ability, at
// y + 0x1A + 13 k, its row as SharedList_DrawList draws one - the rows +0xB and
// +0xC lit twice (the second in colour 2 for +0xC's, else 0), the rest plain;
// the record's name (at most five characters) centred at (x + 6 (12 - n),
// y + 7); the piece lists 0x6641FC / 0x6641E4 by +0xD bit 1 and 0x664220 /
// 0x664214 (8 left) by bit 0; +0xD's high nibble counted down by 0x10 (the
// byte cleared when it is 0); the frame pieces.
//
// As the original has it: the kind goes out in the window argument's stack
// slot with its low byte the flag index (the record pointer's upper bytes
// above it); the lit row's colour over the stack's; Menu_DrawSkillRow reads
// the kind's byte and six bits of the colour.
extern "C" void __cdecl SharedList_DrawMember(unsigned char* window) {
    auto* const w = rv::As<records::WindowRecord>(window);
    SH_CALL(Menu_DrawBox)(w->x + 3, w->y + 3, 0x92, 0xA0, 0, static_cast<unsigned char>(Config_WindowColour));
    int y = w->y + 0x1A;
    U slot = RecAddr(window[0xA]) + 0x7E;
    for (unsigned char k = 0; k < 0xA; ++k, y += 0xD, ++slot) {
        if (B(slot) == 0) continue;
        const unsigned char kind = SH_CALL(Skill_FlagIndex)(B(slot));
        const U record = ::bof3::Img(field_s::at::kAbilityRecords) + 24u * B(slot);
        const int colour = k != window[0xC] ? 0 : 2;
        const int x = w->x + 7;
        if (k == window[0xB] || k == window[0xC]) {
            SH_CALL(Menu_DrawSkillRow)(x, y, 7, kind, At(record), B(record + 0x12), 1);
            SH_CALL(Menu_DrawSkillRow)(w->x + 7, y - 2, colour, kind, At(record), B(record + 0x12), 0);
        } else {
            SH_CALL(Menu_DrawSkillRow)(x, y, 0, kind, At(record), B(record + 0x12), 0);
        }
    }
    const unsigned char* const name = Rec(window[0xA]);
    unsigned char n = SH_CALL(Text_CharCount)(name);
    if (n > 5) n = 5;
    SH_CALL(Text_DrawAt)(6 * (0xC - static_cast<int>(n)) + w->x, w->y + 7, 0, 5, name);
    SH_CALL(Menu_DrawPieces)(w->x, w->y, Text((window[0xD] & 2) ? ::bof3::Img(field_s::at::kPiecesC) : ::bof3::Img(field_s::at::kPiecesA)), 1);
    SH_CALL(Menu_DrawPieces)(w->x - 8, w->y, Text((window[0xD] & 1) ? ::bof3::Img(field_s::at::kPiecesE) : ::bof3::Img(field_s::at::kPiecesD)), 1);
    const unsigned char state = window[0xD];
    window[0xD] = (state & 0xF0) == 0 ? 0 : static_cast<unsigned char>(state - 0x10);
    for (int i = 0; i < 9; ++i) SH_CALL(Menu_DrawPiece)(w->x + 8 * i + 0x28, w->y, 1, 1);
    for (int i = 0; i < 17; ++i) SH_CALL(Menu_DrawPiece)(w->x, w->y + 8 * i + 0x18, 4, 1);
    SH_CALL(Menu_DrawPiece)(w->x + 0x88, w->y + 0x18, 0x20, 1);
    for (int i = 0; i < 15; ++i) SH_CALL(Menu_DrawPiece)(w->x + 0x88, w->y + 8 * i + 0x20, 0x21, 1);
    SH_CALL(Menu_DrawPiece)(w->x, w->y + 0xA0, 5, 1);
    SH_CALL(Menu_DrawPiece)(w->x + 0x88, w->y + 0x98, 0x22, 1);
    for (int i = 0; i < 16; ++i) SH_CALL(Menu_DrawPiece)(w->x + 8 * i + 8, w->y + 0xA0, 6, 1);
}

// original 0x5857E0 (PSX 0x801E1A58): a tail jump through SharedList_Sorts 0x664298 by the
// argument's byte, unchecked (the argument stays on the stack for the entry,
// which reads none): 0 compacts, 1 sorts up, 2 down.
extern "C" void __cdecl SharedList_Sort(unsigned how) {
    [[clang::musttail]] return reinterpret_cast<void (__cdecl*)(unsigned)>(
        CodeAt(::bof3::Img(SharedList_Sorts_addr), how & 0xFF, "SharedList_Sort"))(how);
}

// original 0x5857F0: SharedList_Sorts[0]: the empty entries of the shared
// list moved to its end - 127 passes, pass p over entries i below 128 - p
// (the p-th from 0x7F down), swapping (0x58BD50) an empty entry with a
// non-empty next one.
extern "C" void __cdecl SharedList_Compact(void) {
    for (U limit = 0x7F; limit != 0; --limit)
        for (U i = 0; i < limit; ++i) {
            const U e = ::bof3::Img(field_s::at::kSharedList) + i;
            if (B(e) == 0 && B(e + 1) != 0) SwapBytes(e, e + 1);
        }
}

// original 0x585840: SharedList_Sorts[1]: compacted, then bubble-sorted by
// the abilities' byte +0x12 (the cost Menu_DrawSkillRow shows), the largest
// first: a swap when the first's is below the next's (the original's jae).
extern "C" void __cdecl SharedList_SortCostDown(void) {
    SH_CALL(SharedList_Compact)();
    SortShared([](unsigned char a, unsigned char b) { return a < b; });
}

// original 0x5858C0: SharedList_Sorts[2]: compacted, then sorted the other
// way, the smallest first (a swap when the first's is above the next's).
extern "C" void __cdecl SharedList_SortCostUp(void) {
    SH_CALL(SharedList_Compact)();
    SortShared([](unsigned char a, unsigned char b) { return a > b; });
}

// original 0x585940 (PSX 0x801E1D38): window kind handler (called by 0x59C260 with x, y): the
// count of item 0x58 of category 0 - the box (x + 3, y + 3, 0x55, 0x10, 0,
// style), the label 0x66A118 at (x + 4, y + 4), the count (Inventory_Count(0,
// 0x58, 0), a word) in the 12 px font at (x + 0x34, y + 4); the piece lists
// 0x66422C, 0x664238 nine times 8 apart, 0x664244.
extern "C" void __cdecl SharedList_DrawItemCount(int x, int y) {
    SH_CALL(Menu_DrawBox)(x + 3, y + 3, 0x55, 0x10, 0, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(Text_DrawAt)(x + 4, y + 4, 0, 0xFF, Text(::bof3::Img(field_s::at::kItemLabel)));
    const U count = SH_CALL(Inventory_Count)(0, field_s::at::kInkItem, 0) & 0xFFFF;
    SH_CALL(Crt_sprintf)(PrintBuf(), Format(::bof3::Img(Boss26Fx_CountFormat_addr)), count);
    SH_CALL(Text_DrawFont12)(x + 0x34, y + 4, 0, reinterpret_cast<const unsigned char*>(PrintBuf()));
    SH_CALL(Menu_DrawPieces)(x, y, Text(::bof3::Img(field_s::at::kPiecesF)), 0);
    for (int i = 0; i < 9; ++i) SH_CALL(Menu_DrawPieces)(x + 8 * i, y, Text(::bof3::Img(field_s::at::kPiecesG)), 0);
    SH_CALL(Menu_DrawPieces)(x, y, Text(::bof3::Img(field_s::at::kPiecesH)), 0);
}

void SharedList_Inject() {
    BOF3_INJECT(SharedList_Dispatch);
    BOF3_INJECT(SharedList_Begin);
    BOF3_INJECT(SharedList_Menu);
    BOF3_INJECT(SharedList_MoveStep);
    BOF3_INJECT(SharedList_UseItem);
    BOF3_INJECT(Menu_StepAfterTimer);
    BOF3_INJECT(SharedList_PickMember);
    BOF3_INJECT(SharedList_PickSlot);
    BOF3_INJECT(SharedList_PickShared);
    BOF3_INJECT(SharedList_SortStep);
    BOF3_INJECT(SharedList_SortOpen);
    BOF3_INJECT(SharedList_SortMenu);
    BOF3_INJECT(SharedList_Arrange);
    BOF3_INJECT(ShopTrade_Close);
    BOF3_INJECT(SharedList_Setup);
    BOF3_INJECT(SharedList_DrawList);
    BOF3_INJECT(SharedList_DrawMember);
    BOF3_INJECT(SharedList_Sort);
    BOF3_INJECT(SharedList_Compact);
    BOF3_INJECT(SharedList_SortCostDown);
    BOF3_INJECT(SharedList_SortCostUp);
    BOF3_INJECT(SharedList_DrawItemCount);
}
