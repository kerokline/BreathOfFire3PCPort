// Moved here by tools/move_funcs.py from field_s.cpp, rest_2c.cpp (docs/source-layout-round.md).
#include "game/shop/shop_resist.h"

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
#include "game/shop/shop_resist_callees.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = shop_resist::at;

}  // namespace

namespace {

using U = std::uint32_t;

unsigned char* At(U address) { return move_script::At(address); }

unsigned char& B(U address) { return *At(address); }

int S8(U v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

// CharacterRecords' record n (stride 0xA4), unchecked.
unsigned char* Rec(U n) { return At(::bof3::Img(CharacterRecords_addr) + n * field_s::at::kRecordStride); }

// ObjTrio record k's char_index (s8, unchecked): the byte +0x148.
U MemberAt(int k) { return B(static_cast<U>(::bof3::Img(ObjTrio_addr) + 0x148 + static_cast<std::int32_t>(k) * static_cast<std::int32_t>(field_s::at::kObjStride))); }

U Pressed() { return Input_Pressed; }

bool Confirmed(U pressed) { return (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed & 0xFFFF) != 0; }

bool Cancelled(U pressed) { return (static_cast<std::uint32_t>(Field_CancelButtons) & pressed & 0xFFFF) != 0; }

void Sound(U id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }

U PartyCount() { return static_cast<U>(SH_CALL(Party_Count)(0)) & 0xFF; }

const unsigned char* Message(U id) { return SH_CALL(Msg_SystemPtr)(id); }

char* PrintBuf() { return reinterpret_cast<char*>(At(::bof3::Img(field_s::at::kPrintBuf))); }

const char* Format(U address) { return reinterpret_cast<const char*>(At(address)); }

// The cursor box on the member row (s8 0x929F06): (0x11, 54 row + 0x3E, 0x80,
// 0x34, blink, 6). The row's upper half is a callee's leftover in the
// original; Menu_DrawCursorBox reads the low word.
void MemberCursor(int blink) {
    SH_CALL(Menu_DrawCursorBox)(0x11, 54 * BattleActorCopies_ItemActor + 0x3E, 0x80, 0x34, blink, 6);
}

constexpr auto& AtRest2c = move_script::At;

using move_script::Long;

U LRest2c(U address) { return static_cast<U>(Long(AtRest2c(address))); }

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

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x5837E0, ShopMode_States[5]: `jmp [ShopResist_States + 4 * 0x929F01]`.
extern "C" void __cdecl ShopResist_Dispatch(void) { Jump(::bof3::Img(ShopResist_States_addr), 8, FieldS_State, "ShopResist_Dispatch"); }

#pragma clang attribute pop

// original 0x5837F0: ShopResist_States[0], sliding in. Sound 0x102 on its
// first frame (the counter 6); the members sliding, the bits box at
// (32 t + 0x98, 0x3F) with no row lit; the counter down one; at 0 the cursor,
// the counter, 0x929F05 and 0x929F0F 0, the state up one, step 0.
extern "C" void __cdecl ShopResist_Open(void) {
    if (B(::bof3::Img(field_s::at::kTimer)) == 6) Sound(0x102);
    SH_CALL(ShopResist_DrawMembers)(1);
    SH_CALL(ShopResist_DrawBits)(static_cast<unsigned char>(BattleActorCopies_ItemActor), 32 * static_cast<int>(B(::bof3::Img(field_s::at::kTimer))) + 0x98, 0x3F, 0, 0xFF);
    const auto t = static_cast<unsigned char>(B(::bof3::Img(field_s::at::kTimer)) - 1);
    B(::bof3::Img(field_s::at::kTimer)) = t;
    if (t != 0) return;
    const auto state = FieldS_State;
    BattleActorCopies_ItemActor = 0;
    B(::bof3::Img(field_s::at::kTimer)) = 0;
    MenuLists_Cursor = 0;
    AreaW3c_Load0F = 0;
    FieldS_State = static_cast<unsigned char>(state + 1);
    FieldS_Step = 0;
}

// original 0x583880: ShopResist_States[1], the member. The members, message
// 0x63 at (0x1C, 0x15), the member's bits, the cursor box blinking. The repeat
// of the pad's 0x5000: 0x4000 down (sound 0x100; at the count, s8, to 0), 0x1000
// up (sound 0x100; below 0 to the count less one). Confirm: a record whose
// +0x1D is not 0 - sound 0x107, message 1 (0x929F0B) to 2 (0x929F0F), counter
// 0x3C, the state up six (the notice); else sound 0x104, the bit 0 and the
// state up one. Cancel: sound 0x106, message 0xF, state 6 (the farewell),
// counter 0x3C.
extern "C" void __cdecl ShopResist_PickMember(void) {
    auto count = static_cast<unsigned char>(SH_CALL(Party_Count)(0));
    SH_CALL(ShopResist_DrawMembers)(0);
    SH_CALL(Text_DrawAt)(0x1C, 0x15, 0, 0xFF, Message(0x63));
    SH_CALL(ShopResist_DrawBits)(static_cast<unsigned char>(BattleActorCopies_ItemActor), 0x98, 0x3F, 0, 0xFF);
    MemberCursor(1);
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0x5000);
    if (repeat & 0x4000) {
        Sound(0x100);
        const auto c = static_cast<unsigned char>(static_cast<unsigned char>(BattleActorCopies_ItemActor) + 1);
        BattleActorCopies_ItemActor = c;
        if (S8(c) >= S8(count)) BattleActorCopies_ItemActor = 0;
    }
    if (repeat & 0x1000) {
        Sound(0x100);
        const auto c = static_cast<unsigned char>(static_cast<unsigned char>(BattleActorCopies_ItemActor) - 1);
        BattleActorCopies_ItemActor = c;
        if (S8(c) < 0) {
            --count;
            BattleActorCopies_ItemActor = count;
        }
    }
    if (Confirmed(Pressed())) {
        if (Rec(MemberAt(BattleActorCopies_ItemActor))[0x1D] != 0) {
            Sound(0x107);
            const auto state = static_cast<unsigned char>(FieldS_State + 6);
            FieldS_Answer = 1;
            FieldS_State = state;
            AreaW3c_Load0F = 2;
            B(::bof3::Img(field_s::at::kTimer)) = 0x3C;
        } else {
            Sound(0x104);
            const auto state = static_cast<unsigned char>(FieldS_State + 1);
            FieldS_PickRow = 0;
            FieldS_State = state;
        }
    }
    if (Cancelled(Pressed())) {
        Sound(0x106);
        FieldS_Answer = 0xF;
        FieldS_State = 6;
        B(::bof3::Img(field_s::at::kTimer)) = 0x3C;
    }
}

// original 0x583A20: ShopResist_States[2], the bit. The members, message
// 0x65 + bit (s8) at (0x1C, 0x15), the bits, the cursor box still, the hand at
// (0x9B, 13 bit + 0x44). The repeat of 0x5000: 0x4000 down (sound 0x100; 8 and
// past, s8, to 0), 0x1000 up (below 0 to 7). Confirm: ten times the member's
// level above the zenny - sound 0x107, message 0xB to 0xC, counter 0x3C, the
// state up five (the notice); else sound 0x104, 0x929F0B 0 and the state up
// one. Cancel: sound 0x106 and the state down one.
extern "C" void __cdecl ShopResist_PickBit(void) {
    SH_CALL(ShopResist_DrawMembers)(0);
    SH_CALL(Text_DrawAt)(0x1C, 0x15, 0, 0xFF, Message(static_cast<U>(FieldS_PickRow + 0x65)));
    SH_CALL(ShopResist_DrawBits)(static_cast<unsigned char>(BattleActorCopies_ItemActor), 0x98, 0x3F, 0, 0xFF);
    MemberCursor(0);
    SH_CALL(Menu_DrawHand)(0x9B, 13 * FieldS_PickRow + 0x44, 0);
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0x5000);
    if (repeat & 0x4000) {
        Sound(0x100);
        const auto b = static_cast<unsigned char>(static_cast<unsigned char>(FieldS_PickRow) + 1);
        FieldS_PickRow = b;
        if (S8(b) >= 8) FieldS_PickRow = 0;
    }
    if (repeat & 0x1000) {
        Sound(0x100);
        const auto b = static_cast<unsigned char>(static_cast<unsigned char>(FieldS_PickRow) - 1);
        FieldS_PickRow = b;
        if (S8(b) < 0) FieldS_PickRow = 7;
    }
    if (Confirmed(Pressed())) {
        const U record = MemberAt(BattleActorCopies_ItemActor);
        const U zenny = Party_Zenny;
        if (10u * Rec(record)[0xA] > zenny) {
            Sound(0x107);
            const auto state = static_cast<unsigned char>(FieldS_State + 5);
            FieldS_Answer = 0xB;
            FieldS_State = state;
            AreaW3c_Load0F = 0xC;
            B(::bof3::Img(field_s::at::kTimer)) = 0x3C;
        } else {
            Sound(0x104);
            const auto state = static_cast<unsigned char>(FieldS_State + 1);
            FieldS_Answer = 0;
            FieldS_State = state;
        }
    }
    if (Cancelled(Pressed())) {
        Sound(0x106);
        FieldS_State = static_cast<unsigned char>(FieldS_State - 1);
    }
}

// original 0x583BE0 (PSX 0x801D7970): ShopResist_States[3], "buy?". The members, message 0x6D,
// the bits with the chosen one lit, the cursor box, Menu_YesNo: on yes
// (0x929F0B) message 0xC, counter 0x3C, the zenny less ten times the level
// (unchecked: PickBit asked), the state up two (the grant); on no sound 0x106
// and the state down two (the member).
extern "C" void __cdecl ShopResist_Confirm(void) {
    SH_CALL(ShopResist_DrawMembers)(0);
    SH_CALL(Text_DrawAt)(0x1C, 0x15, 0, 0xFF, Message(0x6D));
    SH_CALL(ShopResist_DrawBits)(static_cast<unsigned char>(BattleActorCopies_ItemActor), 0x98, 0x3F, 0, static_cast<unsigned char>(FieldS_PickRow));
    MemberCursor(0);
    if ((SH_CALL(Menu_YesNo)() & 0xFF) == 0) return;
    if (static_cast<unsigned char>(FieldS_Answer) != 0) {
        const int cursor = BattleActorCopies_ItemActor;
        FieldS_Answer = 0xC;
        B(::bof3::Img(field_s::at::kTimer)) = 0x3C;
        const U level = Rec(MemberAt(cursor))[0xA];
        const U zenny = Party_Zenny - 10u * level;
        const auto state = static_cast<unsigned char>(FieldS_State + 2);
        Party_Zenny = zenny;
        FieldS_State = state;
        return;
    }
    Sound(0x106);
    FieldS_State = static_cast<unsigned char>(FieldS_State - 2);
}

// original 0x583CD0 (PSX 0x801D7AF4): ShopResist_States[4], sliding out. The members sliding,
// the bits at (32 t + 0x98, 0x3F); the counter up one; at 7 the counter 0, the
// mode 1 (the field), state and step 0.
extern "C" void __cdecl ShopResist_Close(void) {
    SH_CALL(ShopResist_DrawMembers)(1);
    SH_CALL(ShopResist_DrawBits)(static_cast<unsigned char>(BattleActorCopies_ItemActor), 32 * static_cast<int>(B(::bof3::Img(field_s::at::kTimer))) + 0x98, 0x3F, 0, 0xFF);
    const auto t = static_cast<unsigned char>(B(::bof3::Img(field_s::at::kTimer)) + 1);
    B(::bof3::Img(field_s::at::kTimer)) = t;
    if (t != 7) return;
    B(::bof3::Img(field_s::at::kTimer)) = 0;
    BattleE1_Mode = 1;
    FieldS_State = 0;
    FieldS_Step = 0;
}

// original 0x583D30 (PSX 0x801D7B84): ShopResist_States[5], the grant. The bits with the chosen
// one lit, the members, the cursor box; the messages from 0x929F0B to 0xF
// (ShopResist_Message); done, the member's +0x1D = 1 << bit (the byte shift of
// the s8 bit, its count masked to five bits: 8 and up give 0),
// Char_RecalcStats of the record and the state down four (the member).
extern "C" void __cdecl ShopResist_Grant(void) {
    SH_CALL(ShopResist_DrawBits)(static_cast<unsigned char>(BattleActorCopies_ItemActor), 0x98, 0x3F, 0, static_cast<unsigned char>(FieldS_PickRow));
    SH_CALL(ShopResist_DrawMembers)(0);
    MemberCursor(0);
    if ((SH_CALL(ShopResist_Message)(At(::bof3::Img(FieldS_Answer_addr)), 0xF) & 0xFF) == 0) return;
    const U record = MemberAt(BattleActorCopies_ItemActor);
    const U count = static_cast<std::uint32_t>(static_cast<unsigned char>(FieldS_PickRow)) & 0x1F;
    Rec(record)[0x1D] = static_cast<unsigned char>(count >= 8 ? 0 : (1u << count));
    SH_CALL(Char_RecalcStats)(Rec(record));
    FieldS_State = static_cast<unsigned char>(FieldS_State - 4);
}

// original 0x583DE0 (PSX 0x801D7CD8): ShopResist_States[6], the farewell. The bits, the
// members; the messages to 0x10; done, the counter 0 and the state down two
// (the slide out).
extern "C" void __cdecl ShopResist_Farewell(void) {
    SH_CALL(ShopResist_DrawBits)(static_cast<unsigned char>(BattleActorCopies_ItemActor), 0x98, 0x3F, 0, 0xFF);
    SH_CALL(ShopResist_DrawMembers)(0);
    if ((SH_CALL(ShopResist_Message)(At(::bof3::Img(FieldS_Answer_addr)), 0x10) & 0xFF) == 0) return;
    const auto state = FieldS_State;
    B(::bof3::Img(field_s::at::kTimer)) = 0;
    FieldS_State = static_cast<unsigned char>(state - 2);
}

// original 0x583E30 (PSX 0x801D7D58): ShopResist_States[7], a notice (the record refused, the
// zenny short). The bits, the members, the cursor box blinking; the messages
// to 0x929F0F (a byte); done, the state down six (the member).
extern "C" void __cdecl ShopResist_Notice(void) {
    SH_CALL(ShopResist_DrawBits)(static_cast<unsigned char>(BattleActorCopies_ItemActor), 0x98, 0x3F, 0, 0xFF);
    SH_CALL(ShopResist_DrawMembers)(0);
    MemberCursor(1);
    if ((SH_CALL(ShopResist_Message)(At(::bof3::Img(FieldS_Answer_addr)), AreaW3c_Load0F) & 0xFF) == 0) return;
    FieldS_State = static_cast<unsigned char>(FieldS_State - 6);
}

// original 0x583EA0: the bits box of member row `member` (its record through
// its char_index, ObjTrio +0x148): the box (x + 3, y + 3, 0x9E, 0x70, 0, colour) and its border
// (x, y, 0x12, 0xD); the price, ten times the level as a word; then the eight
// bits' names (system messages 0xDE + k) at (x + 8, y + 6 + 13 k) and the
// price at (x + 0x68, the same) in the 12 px font - a bit the record has, and
// the row `highlight`, in colour 2; the rest in 7 when the price is above the
// zenny or the record has a bit, else 7 when `colour`'s byte is not 0, else 0.
//
// As the original has it: the text colour is a dword in the `colour`
// argument's slot, its low byte replaced, its upper bytes the argument's
// (Text_DrawString and Text_DrawFont12 read six bits); the bit is 1 << k by
// the dword counter's shift.
extern "C" void __cdecl ShopResist_DrawBits(unsigned member, int x, int y, unsigned colour, unsigned highlight) {
    SH_CALL(Menu_DrawBox)(x + 3, y + 3, 0x9E, 0x70, 0, static_cast<int>(colour));
    SH_CALL(Menu_DrawBorder)(x, y, 0x12, 0xD);
    int row_y = y + 6;
    const U record = MemberAt(static_cast<int>(member & 0xFF));
    const U price = (10u * rv::As<records::CharRecord>(Rec(record))->level) & 0xFFFF;
    U text = colour & 0xFFFFFF00u;
    if (Party_Zenny >= price && rv::As<records::CharRecord>(Rec(record))->halving == 0)
        text |= (colour & 0xFF) != 0 ? 7u : 0u;
    else
        text |= 7u;
    for (U k = 0; k < 8; ++k) {
        const U bit = 1u << k;
        if ((rv::As<records::CharRecord>(Rec(record))->halving & bit & 0xFF) == 0 && k != (highlight & 0xFF))
            SH_CALL(Text_DrawAt)(x + 8, row_y, static_cast<int>(text), 0xFF, Message(k + 0xDE));
        else
            SH_CALL(Text_DrawAt)(x + 8, row_y, 2, 0xFF, Message(k + 0xDE));
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(::bof3::Img(field_s::at::kFmtCost)), price);
        SH_CALL(Text_DrawFont12)(x + 0x68, row_y, static_cast<int>(text), reinterpret_cast<const unsigned char*>(PrintBuf()));
        row_y += 0xD;
    }
}

// original 0x584010 (PSX 0x801D8054): the resistance shop's members: the title box (0x14,
// 0x10 - 10 s, 0x118, 0x13, style) where s is the counter when `sliding`, else
// 0; for each member k below Party_Count(0) (asked after each) its panel
// (0x11 - 32 s, 54 k + 0x3E, its record, lit when the record's +0x1D is not 0);
// the money box (32 s + 0xB4, 0x26, 0, zenny).
//
// As the original has it: s is a word over the caller's esi, so every
// coordinate carries the caller's upper half; Menu_DrawTitleBox,
// Menu_DrawMemberStatus's draws and Menu_DrawMoneyBox's read the low word.
extern "C" void __cdecl ShopResist_DrawMembers(unsigned sliding) {
    const int s = (sliding & 0xFF) != 0 ? static_cast<int>(B(::bof3::Img(field_s::at::kTimer))) : 0;
    SH_CALL(Menu_DrawTitleBox)(0x14, 0x10 - 10 * s, 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour));
    if (PartyCount() != 0) {
        unsigned char k = 0;
        do {
            const U record = MemberAt(k);
            SH_CALL(Menu_DrawMemberStatus)(0x11 - 32 * s, 54 * k + 0x3E, record, Rec(record)[0x1D] != 0 ? 1u : 0u);
            ++k;
        } while (k < PartyCount());
    }
    SH_CALL(Menu_DrawMoneyBox)(32 * s + 0xB4, 0x26, 0, Party_Zenny);
}

// original 0x584120 (PSX 0x801D81B8): one message of a run - system message 0x63 + *index at
// (0x1C, 0x14) - held until the counter runs down or any pad bit is pressed;
// then the counter 0x3C and *index up one; answers 1 when that reached `end`'s
// byte, else 0.
extern "C" unsigned char __cdecl ShopResist_Message(unsigned char* index, unsigned end) {
    SH_CALL(Text_DrawAt)(0x1C, 0x14, 0, 0xFF, Message(static_cast<U>(*index) + 0x63));
    const auto t = static_cast<unsigned char>(B(::bof3::Img(field_s::at::kTimer)) - 1);
    B(::bof3::Img(field_s::at::kTimer)) = t;
    if (t != 0 && Pressed() == 0) return 0;
    B(::bof3::Img(field_s::at::kTimer)) = 0x3C;
    const auto next = static_cast<unsigned char>(*index + 1);
    *index = next;
    return next == static_cast<unsigned char>(end) ? 1 : 0;
}

void ShopResist_Inject() {
    BOF3_INJECT(ShopResist_Dispatch);
    BOF3_INJECT(ShopResist_Open);
    BOF3_INJECT(ShopResist_PickMember);
    BOF3_INJECT(ShopResist_PickBit);
    BOF3_INJECT(ShopResist_Confirm);
    BOF3_INJECT(ShopResist_Close);
    BOF3_INJECT(ShopResist_Grant);
    BOF3_INJECT(ShopResist_Farewell);
    BOF3_INJECT(ShopResist_Notice);
    BOF3_INJECT(ShopResist_DrawBits);
    BOF3_INJECT(ShopResist_DrawMembers);
    BOF3_INJECT(ShopResist_Message);
}
