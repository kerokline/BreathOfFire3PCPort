// Round twelve group FS (docs/takeover-queue-field-battle.md section 3,
// docs/field_s.md): the PSX SHOP.EMI's remaining states compiled into the exe
// and the equip screen's two choosers, taken with the scenario harness in
// field mode (scenario_harness.h, docs/scenario_harness.md section 7). Each
// read to its last instruction with capstone against bof3/BOF3.exe,
// 2026-09-29; entry and size (bytes of code, the padding after it left out):
//
//   FieldSave_Confirm      0x57FF80 0x85    ShopResist_Grant        0x583D30 0xAB
//   Rest_Begin             0x580310 0x6B    ShopResist_Farewell     0x583DE0 0x47
//   Rest_PlaceParty        0x580380 0x1D8   ShopResist_Notice       0x583E30 0x64
//   PartyForm_OpenStep     0x580A50 0xE     ShopResist_DrawBits     0x583EA0 0x166
//   PartyForm_Setup        0x580A60 0x116   ShopResist_DrawMembers  0x584010 0x103
//   PartyForm_FadeWait     0x580B80 0x21    ShopResist_Message      0x584120 0x5D
//   PartyForm_OpenWait     0x580BB0 0x23    SharedList_Begin        0x584190 0x19
//   PartyForm_SlideIn      0x580BE0 0x35    SharedList_Menu         0x5841B0 0x2A4
//   PartyForm_Choose       0x580C20 0x257   SharedList_MoveStep     0x584460 0xE
//   PartyForm_LeaveStep    0x580E80 0xE     SharedList_UseItem      0x584470 0x16D
//   PartyForm_LeaveFade    0x580E90 0x28    Menu_StepAfterTimer     0x5845E0 0x15
//   PartyForm_LeaveBlack   0x580EC0 0x1A    SharedList_PickMember   0x584600 0x1E3
//   PartyForm_Reload       0x580EE0 0x2CC   SharedList_PickSlot     0x5847F0 0x1B3
//   PartyForm_End          0x5811B0 0x29    SharedList_PickShared   0x5849B0 0x1FD
//   PartyForm_DrawSliding  0x5811E0 0x113   SharedList_SortStep     0x584BB0 0xE
//   PartyForm_DrawReserve  0x581300 0x285   SharedList_SortOpen     0x584BC0 0x50
//   PartyForm_Swap         0x581590 0x18F   SharedList_SortMenu     0x584C10 0x117
//   PartyForm_Draw         0x581720 0x18F   SharedList_Arrange      0x584D30 0x23A
//   ShopBrowse_InitWindows 0x5836E0 0x90    SharedList_Setup        0x584F90 0xFC
//   ShopBrowse_OpenDetail  0x583770 0x65    SharedList_DrawList     0x585090 0x469
//   ShopResist_Open        0x5837F0 0x82    SharedList_DrawMember   0x585500 0x2D3
//   ShopResist_PickMember  0x583880 0x196   SharedList_Sort         0x5857E0 0x10
//   ShopResist_PickBit     0x583A20 0x1BA   SharedList_Compact      0x5857F0 0x4E
//   ShopResist_Confirm     0x583BE0 0xF0    SharedList_SortCostDown 0x585840 0x76
//   ShopResist_Close       0x583CD0 0x5D    SharedList_SortCostUp   0x5858C0 0x76
//   Equip_ChooseSlot       0x58C7A0 0x338   SharedList_DrawItemCount 0x585940 0xB9
//   Equip_ChooseItem       0x58CAE0 0x251
//
// Faithful: no divergence. Every call out goes through the harness (SH_CALL /
// SH_AT), so the start-up fuzz can stand recorders in for the callees. Names
// are from what the code does; the screens' meaning in the game is the
// owner's to say (docs/field_s.md section 1).
//
// Registers the originals push whole - a byte loaded into al over whatever
// eax held, a coordinate computed from a register whose upper half is a
// callee's leftover - are passed here from the value the callee reads: each
// callee's reading is cited where it matters (docs/field_s.md section 3), and
// the fuzz lists those callees with masks of exactly what they read.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_s.h"
#include "game/field_s_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_s::at;
using U = std::uint32_t;

unsigned char* At(U address) { return move_script::At(address); }
unsigned char& B(U address) { return *At(address); }
U W(U address) { return move_script::Word(At(address)); }
void SetW(U address, U v) { move_script::SetWord(At(address), v); }
U L(U address) { return static_cast<U>(move_script::Long(At(address))); }
void SetL(U address, U v) { move_script::SetLong(At(address), static_cast<std::int32_t>(v)); }
int S8(U v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }
int S16(U v) { return static_cast<short>(static_cast<unsigned short>(v)); }

// CharacterRecords' record n (stride 0xA4), unchecked.
unsigned char* Rec(U n) { return At(at::kRecords + n * at::kRecordStride); }
U RecAddr(U n) { return at::kRecords + n * at::kRecordStride; }
// A member id's record index: MoveScript_EffectState[id], unchecked.
U RecordOf(U id) { return MoveScript_EffectState[id & 0xFF]; }
// Field_Members of ObjTrio record k (s8, unchecked): the byte +0x148.
U MemberAt(int k) { return B(static_cast<U>(at::kFieldMembers + static_cast<std::int32_t>(k) * static_cast<std::int32_t>(at::kObjStride))); }

U Pressed() { return W(at::kPressed); }
bool Confirmed(U pressed) { return (W(at::kConfirm) & pressed & 0xFFFF) != 0; }
bool Cancelled(U pressed) { return (W(at::kCancel) & pressed & 0xFFFF) != 0; }

void Sound(U id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
U PartyCount() { return static_cast<U>(SH_CALL(Party_Count)(0)) & 0xFF; }
const unsigned char* Message(U id) { return SH_CALL(Msg_SystemPtr)(id); }
char* PrintBuf() { return reinterpret_cast<char*>(At(at::kPrintBuf)); }
const char* Format(U address) { return reinterpret_cast<const char*>(At(address)); }
const unsigned char* Text(U address) { return At(address); }
void DrawFont8(int x, int y, int colour) {
    SH_CALL(Text_DrawFont8)(x, y, colour, reinterpret_cast<const unsigned char*>(PrintBuf()));
}

// A .data table's word read in place (the fuzz swaps its entries for
// recorders). Outside the fuzz a word that is not code is past the table:
// the original jumps there; ours aborts (docs/field_s.md section 6).
constexpr U kTextLo = 0x401000, kTextHi = 0x5C3000;
U CodeAt(U table, U index, const char* who) {
    const U cell = table + 4 * index;
    const U entry = L(cell);
    if (!scenario_harness::g_active && (entry < kTextLo || entry >= kTextHi))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X, not code - past its table (the original jumps there)", who,
                    (unsigned)index, (unsigned)entry, (unsigned)cell);
    return entry;
}
using Handler = void (__cdecl*)();

// The swap helper 0x58BD50 (nobody's): the two bytes exchanged.
void SwapBytes(U a, U b) { SH_AT(void (__cdecl*)(unsigned char*, unsigned char*), at::kSwapBytes)(At(a), At(b)); }

// The five-byte stretch of window stores two SharedList states and
// SharedList_UseItem make (windows 2 and 3 of WindowRecords, 0x8031A8 and
// 0x8031CC): the member list and the member's slots, the joined record `rec`
// shown in both.
void OpenMemberWindows(unsigned char rec) {
    B(0x8031A9) = 7;
    B(0x8031AA) = 0xD;
    B(0x8031B2) = rec;
    B(0x8031D6) = rec;
    B(0x8031AB) = 2;
    B(0x8031A8) = 1;
    SetW(0x8031AC, 0xFF6A);
    SetW(0x8031AE, 0x74);
    B(0x8031CD) = 7;
    B(0x8031CE) = 0xC;
    B(0x8031CF) = 2;
    B(0x8031CC) = 1;
    B(0x8031D7) = 0;
    B(0x8031D8) = 0xFF;
    B(0x8031D9) = 0;
    SetW(0x8031D0, 0x140);
    SetW(0x8031D2, 0x3E);
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
    } else if (repeat & 4) {
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
    } else if (repeat & 8) {
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

std::uint32_t field_s::FrameCallTarget() { return at::kFrameSite + 5 + L(at::kFrameSite + 1); }

// ===========================================================================
// The field save's confirm (FieldSave_States[3]) and the rest sequence
// (Rest_States 0x663FB8, ShopMode_States[10]'s)
// ===========================================================================

// original 0x57FF80: FieldSave_States[3] (docs/shop_states.md section 1). The
// title box (0x14, 0x12, 0x118, 0x13, style), message 0x9F at (0x1C, 0x16), the
// slots at (0x20, 0x30) unlit, then Menu_YesNo: on an answer, with 0x929F0B
// (yes) sound 0x104 and the step up one (state 4, the write), else sound 0x106
// and the step down one (back to the slots). The style goes out in eax over
// the entry's upper bytes; Menu_DrawTitleBox reads its byte.
extern "C" void __cdecl FieldSave_Confirm(void) {
    SH_CALL(Menu_DrawTitleBox)(0x14, 0x12, 0x118, 0x13, B(at::kStyle));
    SH_CALL(Text_DrawAt)(0x1C, 0x16, 0, 0xFF, Message(0x9F));
    SH_CALL(SaveMenu_DrawSlots)(0x20, 0x30, 0);
    if ((SH_CALL(Menu_YesNo)() & 0xFF) == 0) return;
    if (B(at::kAnswer) != 0) {
        Sound(0x104);
        B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
        return;
    }
    Sound(0x106);
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) - 1);
}

// original 0x580310: Rest_States[0]. With story flag 0x77 set: state 6 and
// script message 0xEF (the state stored before the call). Else the flag set -
// unless the chapter byte is 0xE and the chapter row's flag 5 is set - then
// Transition_Start(0) and the state up one.
extern "C" void __cdecl Rest_Begin(void) {
    if ((SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x77) & 0xFF) != 0) {
        B(at::kState) = 6;
        SH_CALL(Msg_OpenScript)(0xEF);
        return;
    }
    if (B(at::kChapter) != 0xE ||
        (SH_CALL(Flags_Test)(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(L(at::kFlagRow))), 5) & 0xFF) == 0)
        SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x77);
    SH_CALL(Transition_Start)(0);
    B(at::kState) = static_cast<unsigned char>(B(at::kState) + 1);
}

// original 0x580380: Rest_States[1]. Nothing while the transition runs (the
// wait word). Then the black screen; areas 0xBB and 0xC1 - and 0xBF in
// chapter 0xE with the row's flag 5 - only step the state on. Else
// Music_Track 0xFF; a place kind 0, 1 for area 0x85 (story flag 0x77 cleared,
// the area read again) or 2 for 0xBF; Field_ScriptFlags2's low three bits
// cleared; the members loaded (Field_MemberCount = Party_Count(0),
// Field_PartyLoad(0)); for each member i below Field_MemberCount (read again
// after each), Sprite_Current its ObjTrio record, Field_MemberSprite(its +0x89,
// i), Sprite_SetAnimation of its +8, +0x34 / +0x38 / +0x3C from the kind's
// row i of 0x663FD8 and its state bytes +1..+4 0 (each through Sprite_Current
// read again); the kind-2 sprite at the leader's +0x34 / +0x38,
// Field_ViewReset, Area_RunPlacement of the area descriptor's first dword,
// and the state up one.
//
// As the original has it: the place row is 16 x (3 kind + i) past 0x663FD8,
// unbounded by the member count; the area indexes Area_Descriptors (200) as a
// word, unchecked.
extern "C" void __cdecl Rest_PlaceParty(void) {
    if (W(at::kWait) != 0) return;
    SH_CALL(Menu_DrawBlackScreen)();
    U area = W(at::kArea);
    if (area == 0xBB || area == 0xC1) {
        B(at::kState) = static_cast<unsigned char>(B(at::kState) + 1);
        return;
    }
    if (area == 0xBF && B(at::kChapter) == 0xE) {
        if ((SH_CALL(Flags_Test)(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(L(at::kFlagRow))), 5) & 0xFF) != 0) {
            B(at::kState) = static_cast<unsigned char>(B(at::kState) + 1);
            return;
        }
        area = W(at::kArea);
    }
    Music_Track = 0xFF;
    U kind = 0;
    if (area == 0x85) {
        SH_CALL(Flags_Clear)(At(at::kStoryFlags), 0x77);
        area = W(at::kArea);
        kind = 1;
    }
    if (area == 0xBF) kind = 2;
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF8);
    Field_MemberCount = static_cast<unsigned char>(SH_CALL(Party_Count)(0));
    SH_CALL(Field_PartyLoad)(0);
    if (Field_MemberCount != 0) {
        unsigned char i = 0;
        do {
            unsigned char* const obj = ObjTrio + i * at::kObjStride;
            Sprite_Current = obj;
            SH_CALL(Field_MemberSprite)(obj[0x89], i);
            SH_CALL(Sprite_SetAnimation)(Sprite_Current[8]);
            const U row = at::kPlaceTable + 16 * (3 * kind + i);
            ++i;
            move_script::SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(L(row)));
            move_script::SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(L(row + 4)));
            move_script::SetLong(Sprite_Current + 0x3C, static_cast<std::int32_t>(L(row + 8)));
            Sprite_Current[1] = 0;
            Sprite_Current[2] = 0;
            Sprite_Current[3] = 0;
            Sprite_Current[4] = 0;
        } while (i < Field_MemberCount);
    }
    Field_Kind2X = static_cast<long>(L(0x802D74));
    Field_Kind2Z = static_cast<long>(L(0x802D78));
    SH_CALL(Field_ViewReset)();
    const U descriptor = L(at::kAreaDescriptors + 4 * W(at::kArea));
    SH_CALL(Area_RunPlacement)(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(L(descriptor))));
    B(at::kState) = static_cast<unsigned char>(B(at::kState) + 1);
}

// ===========================================================================
// The party formation (PartyForm_States 0x66409C, ShopMode_States[6]'s)
// ===========================================================================

// original 0x580A50: PartyForm_States[0], a tail jump through
// PartyForm_OpenSteps 0x6640AC by the step byte, unchecked.
extern "C" void __cdecl PartyForm_OpenStep(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(CodeAt(at::kPartyFormOpenSteps, B(at::kStep), "PartyForm_OpenStep"))();
}

// original 0x580A60: PartyForm_OpenSteps[0]. The reserve 0x6BC894: every
// record of the eight whose +0xB bit 0 is set and whose +9 is not among the
// party list's first Party_Count(0) ids, in order, its count to 0x6BC897
// (cleared first). Then the flag pairs from 0x6BC88C: the party's first bytes
// 1 for each member, the rest of the three pairs' first bytes 0 (nothing when
// three are in); the reserve's second bytes 1 for each. The column, row 0,
// the picks 0x7F, 0x6BC898 0xFF, the counter 3, the step up one,
// Transition_Start(2).
//
// As the original has it: the reserve's flags are written two apart from
// 0x6BC88D for as many as the reserve holds - past the three pairs, over the
// reserve's own bytes 0x6BC895 / 0x6BC897 from its fifth entry on (docs/
// field_s.md section 7, L1); a clearing loop after it never runs.
extern "C" void __cdecl PartyForm_Setup(void) {
    const U n = PartyCount();
    unsigned char count = 0;
    B(at::kReserveCount) = 0;
    for (U r = 0; r < 8; ++r) {
        const unsigned char* const rec = Rec(r);
        if ((rec[0xB] & 1) == 0) continue;
        U k = 0;
        const unsigned char id = rec[9];
        while (k < n && id != B(at::kPartyList + k)) ++k;
        if (k == n) B(at::kReserve + count++) = rec[9];
    }
    B(at::kReserveCount) = count;
    U edx = 0;
    if (n > 0) {
        for (U k = 0; k < n; ++k) B(at::kReserveFlags + 2 * k) = 1;
        edx = n;
    }
    if (n == 0 || edx < 3) {
        U p = at::kReserveFlags + 2 * edx;
        do {
            B(p) = 0;
            p += 2;
        } while (p < at::kReserveFlags + 6);
    }
    for (U k = 0; k < count; ++k) B(at::kReserveFlags + 1 + 2 * k) = 1;
    const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
    B(at::kColumn) = 0;
    B(at::kRow) = 0;
    B(at::kPickColumn) = 0x7F;
    B(at::kPickRow) = 0x7F;
    B(at::kPartyFormResult) = 0xFF;
    B(at::kTimer) = 3;
    B(at::kStep) = step;
    SH_CALL(Transition_Start)(2);
}

// original 0x580B80: PartyForm_OpenSteps[1]. The transition over:
// Transition_Start(3) and the step up one.
extern "C" void __cdecl PartyForm_FadeWait(void) {
    if (W(at::kWait) != 0) return;
    SH_CALL(Transition_Start)(3);
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x580BB0: PartyForm_OpenSteps[2]. The screen drawn; the transition
// over: step 0 and the state up two (to the choosing).
extern "C" void __cdecl PartyForm_OpenWait(void) {
    SH_CALL(PartyForm_Draw)();
    if (W(at::kWait) != 0) return;
    const auto state = B(at::kState);
    B(at::kStep) = 0;
    B(at::kState) = static_cast<unsigned char>(state + 2);
}

// original 0x580BE0: PartyForm_States[1]. The backdrop (kind 0x903A5B, in eax
// over the entry's upper bytes; Menu_DrawBackdrop reads its byte), the screen
// at the slide's offset, the counter down one; at 0 the counter 3 and the
// state up one.
extern "C" void __cdecl PartyForm_SlideIn(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    SH_CALL(PartyForm_DrawSliding)();
    const auto t = static_cast<unsigned char>(B(at::kTimer) - 1);
    B(at::kTimer) = t;
    if (t != 0) return;
    const auto state = B(at::kState);
    B(at::kTimer) = 3;
    B(at::kState) = static_cast<unsigned char>(state + 1);
}

// original 0x580C20: PartyForm_States[2], the choosing. The screen; the help
// line - message 0x60, or with 0x6BC898 not 0xFF the refused record's name
// (TextRecord_Set(0, 8, its record)) and message 0x61 - at (0x1A, 0x14). The
// repeat of the pad's 0xF000: 0xA000 flips the column when the reserve is
// not empty; the row cut to the column's last (the party's Party_Count(0) - 1
// or the reserve's count - 1, s8 against it); 0x1000 up (below 0 to the
// column's last, the column read again), 0x4000 down (past the last to 0);
// sound 0x100 when the column (u8 before, s8 after) or the row (s8) moved.
// Confirm: with nothing picked (0x7F) sound 0x103 and the column and row
// picked; else PartyForm_Swap's answer to 0x6BC898, sound 0x103 (0xFF) or
// 0x107, the picks 0x7F. Cancel: sound 0x106; a pick is dropped, or with none
// the state up one, step 0, Transition_Start(2).
extern "C" void __cdecl PartyForm_Choose(void) {
    SH_CALL(PartyForm_Draw)();
    if (B(at::kPartyFormResult) == 0xFF) {
        SH_CALL(Text_DrawAt)(0x1A, 0x14, 0, 0xFF, Message(0x60));
    } else {
        SH_CALL(TextRecord_Set)(0, 8, Rec(RecordOf(B(at::kPartyFormResult))));
        SH_CALL(Text_DrawAt)(0x1A, 0x14, 0, 0xFF, Message(0x61));
    }
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0xF000);
    const U count = PartyCount();
    unsigned char column = B(at::kColumn);
    const unsigned char reserve = B(at::kReserveCount);
    const unsigned char old_column = column;
    if ((repeat & 0xA000) != 0 && reserve != 0) {
        column = static_cast<unsigned char>(column ^ 1);
        B(at::kColumn) = column;
    }
    unsigned char row = B(at::kRow);
    const int row0 = S8(row);
    if (column == 0) {
        if (row0 > static_cast<int>(count) - 1) {
            row = static_cast<unsigned char>(count - 1);
            B(at::kRow) = row;
        }
    } else if (row0 > static_cast<int>(reserve) - 1) {
        row = static_cast<unsigned char>(reserve - 1);
        B(at::kRow) = row;
    }
    if (repeat & 0x1000) {
        row = static_cast<unsigned char>(row - 1);
        B(at::kRow) = row;
        if (S8(row) < 0) {
            row = static_cast<unsigned char>(B(at::kColumn) == 0 ? count - 1 : reserve - 1);
            B(at::kRow) = row;
        }
    } else if (repeat & 0x4000) {
        const unsigned char c = B(at::kColumn);
        row = static_cast<unsigned char>(row + 1);
        B(at::kRow) = row;
        const int last = c == 0 ? static_cast<int>(count) - 1 : static_cast<int>(reserve) - 1;
        if (S8(row) > last) {
            row = 0;
            B(at::kRow) = 0;
        }
    }
    if (static_cast<int>(old_column) != S8(B(at::kColumn)) || row0 != S8(row)) Sound(0x100);
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        if (B(at::kPickColumn) == 0x7F) {
            Sound(0x103);
            const unsigned char c = B(at::kColumn), r = B(at::kRow);
            B(at::kPickColumn) = c;
            B(at::kPickRow) = r;
            return;
        }
        const unsigned char result = SH_CALL(PartyForm_Swap)();
        B(at::kPartyFormResult) = result;
        Sound(result == 0xFF ? 0x103 : 0x107);
        B(at::kPickColumn) = 0x7F;
        B(at::kPickRow) = 0x7F;
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    if (B(at::kPickColumn) != 0x7F) {
        B(at::kPickColumn) = 0x7F;
        B(at::kPickRow) = 0x7F;
        return;
    }
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    B(at::kStep) = 0;
    B(at::kState) = state;
    SH_CALL(Transition_Start)(2);
}

// original 0x580E80: PartyForm_States[3], a tail jump through
// PartyForm_LeaveSteps 0x6640B8 by the step byte, unchecked.
extern "C" void __cdecl PartyForm_LeaveStep(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(CodeAt(at::kPartyFormLeaveSteps, B(at::kStep), "PartyForm_LeaveStep"))();
}

// original 0x580E90: PartyForm_LeaveSteps[0]. While the transition runs, a
// tail jump to PartyForm_Draw; then the black screen, the counter 2 and the
// step up one.
extern "C" void __cdecl PartyForm_LeaveFade(void) {
    if (W(at::kWait) != 0) {
        [[clang::musttail]] return SH_CALL(PartyForm_Draw)();
    }
    SH_CALL(Menu_DrawBlackScreen)();
    const auto step = B(at::kStep);
    B(at::kTimer) = 2;
    B(at::kStep) = static_cast<unsigned char>(step + 1);
}

// original 0x580EC0: PartyForm_LeaveSteps[1]. The black screen, the counter
// down one; at 0 the step up one.
extern "C" void __cdecl PartyForm_LeaveBlack(void) {
    SH_CALL(Menu_DrawBlackScreen)();
    const auto t = static_cast<unsigned char>(B(at::kTimer) - 1);
    B(at::kTimer) = t;
    if (t == 0) B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x580EE0: PartyForm_LeaveSteps[2], the party back on the field.
// The black screen; the leader's +0x89 kept (bl); PartySet_Load of the party
// list's three ids, mode 0 (the ids in registers over callees' leftovers;
// PartySet_Find reads bytes). With 0x904152 set and (story flag 0x77 clear or
// the area 0xBB) and the area not 0x85: one member - record 0's 0xA4 bytes
// over the leader's +0x80 (Text_CurrentName), +5 0, Sprite_Current the leader,
// Field_MemberCount 1, +0x48 0, Field_MemberSprite(record 0's +9, 0),
// Sprite_SetAnimation(+8), state bytes +1..+4 0. Else the members as
// Rest_PlaceParty loads them (state bytes cleared only with 0x904152), then
// - when the kept +0x89 was not 2 and the leader's now is - the leader stepped
// half a cell (s16 pair of 0x6640F8 by its facing +8, << 15) and, unless
// Field_StatusBits bit 6, the kind-2 sprite and view on it. Then area 0x85
// places the members from 0x6640C8 (and the view likewise), area 0x5C at
// (0x190000, 0x3C0000) with the elevation of the leader's cell (s16 << 16) and
// the view; Transition_Start(3) and the step up one.
extern "C" void __cdecl PartyForm_Reload(void) {
    SH_CALL(Menu_DrawBlackScreen)();
    const unsigned char kept = B(0x802DC9);
    SH_CALL(PartySet_Load)(B(at::kPartyList), B(at::kPartyList + 1), B(at::kPartyList + 2), 0);
    bool lone = false;
    if (B(at::kLoneParty) != 0) {
        const unsigned char flag = SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x77);
        const U area = W(at::kArea);
        if (((flag & 0xFF) == 0 || area == 0xBB) && area != 0x85) lone = true;
    }
    if (lone) {
        const unsigned char member = B(0x903A79);
        std::memcpy(At(at::kLeaderName), At(at::kRecords), 0xA4);
        B(0x802D45) = 0;
        Sprite_Current = ObjTrio;
        Field_MemberCount = 1;
        B(0x802D88) = 0;
        SH_CALL(Field_MemberSprite)(member, 0);
        SH_CALL(Sprite_SetAnimation)(Sprite_Current[8]);
        Sprite_Current[1] = 0;
        Sprite_Current[2] = 0;
        Sprite_Current[3] = 0;
        Sprite_Current[4] = 0;
    } else {
        Field_MemberCount = static_cast<unsigned char>(SH_CALL(Party_Count)(0));
        SH_CALL(Field_PartyLoad)(0);
        if (Field_MemberCount != 0) {
            U i = 0;
            do {
                unsigned char* const obj = ObjTrio + i * at::kObjStride;
                const unsigned char member = obj[0x89];
                Sprite_Current = obj;
                SH_CALL(Field_MemberSprite)(member, i);
                SH_CALL(Sprite_SetAnimation)(Sprite_Current[8]);
                if (B(at::kLoneParty) != 0) {
                    Sprite_Current[1] = 0;
                    Sprite_Current[2] = 0;
                    Sprite_Current[3] = 0;
                    Sprite_Current[4] = 0;
                }
                ++i;
            } while (i < Field_MemberCount);
        }
        if (kept != 2 && B(0x802DC9) == 2) {
            const U facing = B(0x802D48) * 4u;
            SetL(0x802D74, L(0x802D74) + (static_cast<U>(S16(W(at::kFacingSteps + facing))) << 15));
            SetL(0x802D78, L(0x802D78) + (static_cast<U>(S16(W(at::kFacingSteps + 2 + facing))) << 15));
        }
        if ((Field_StatusBits & 0x40) == 0) {
            Field_Kind2X = static_cast<long>(L(0x802D74));
            Field_Kind2Z = static_cast<long>(L(0x802D78));
            SH_CALL(Field_ViewReset)();
        }
    }
    if (W(at::kArea) == 0x85) {
        const U n = Field_MemberCount;
        for (U i = 0; i < n; ++i) {
            const U obj = 0x802D40 + i * at::kObjStride;
            SetL(obj + 0x34, L(at::kArea85Place + 16 * i));
            SetL(obj + 0x38, L(at::kArea85Place + 16 * i + 4));
            SetL(obj + 0x3C, L(at::kArea85Place + 16 * i + 8));
        }
        if ((Field_StatusBits & 0x40) != 0) {
            SH_CALL(Transition_Start)(3);
            B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
            return;
        }
        Field_Kind2X = static_cast<long>(L(0x802D74));
        Field_Kind2Z = static_cast<long>(L(0x802D78));
        SH_CALL(Field_ViewReset)();
    }
    if (W(at::kArea) == 0x5C) {
        if (Field_MemberCount != 0) {
            U i = 0;
            do {
                const U obj = 0x802D40 + i * at::kObjStride;
                SetL(obj + 0x34, 0x190000);
                SetL(obj + 0x38, 0x3C0000);
                const long h = SH_CALL(AreaMap_Elevation)(static_cast<long>(L(0x802D74)), static_cast<long>(L(0x802D78)));
                SetL(obj + 0x3C, static_cast<U>(S16(static_cast<U>(h))) << 16);
                ++i;
            } while (i < Field_MemberCount);
        }
        Field_Kind2X = static_cast<long>(L(0x802D74));
        Field_Kind2Z = static_cast<long>(L(0x802D78));
        SH_CALL(Field_ViewReset)();
    }
    SH_CALL(Transition_Start)(3);
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x5811B0: PartyForm_LeaveSteps[3]. The transition over: the
// counter 0, the mode 1 (the field), state and step 0, 0x929F0F 1.
extern "C" void __cdecl PartyForm_End(void) {
    if (W(at::kWait) != 0) return;
    B(at::kTimer) = 0;
    B(at::kMode) = 1;
    B(at::kState) = 0;
    B(at::kStep) = 0;
    B(at::kMessageEnd) = 1;
}

// original 0x5811E0: the formation screen at the slide's offset (the counter
// t): for each member k below Party_Count(0) (asked once), its status panel
// Menu_DrawMemberStatus(0x11 - 40 t, 54 k + 0x3E, record of id 0x904062[k],
// its +0xB bit 1) and its number (k + 1, "%d"-like 0x5E10C0) in the 8 px font
// at (0x1A - 40 t, 54 k + 0x43); the reserve at (45 t + 0x96, 0x3B); the title
// box (0x14, 0x12 - 8 t, 0x118, 0x13, style).
//
// As the original has it: the number's x, the reserve's x and the title's y
// are computed from t over a callee's leftover upper half; every consumer
// reads the low word (docs/field_s.md section 3).
extern "C" void __cdecl PartyForm_DrawSliding(void) {
    const U n = PartyCount();
    for (U k = 0; k < n; ++k) {
        const U record = RecordOf(B(at::kPartyList + k));
        const int y = static_cast<int>(54 * k) + 0x3E;
        SH_CALL(Menu_DrawMemberStatus)(0x11 - 40 * static_cast<int>(B(at::kTimer)), y, record, Rec(record)[0xB] & 2);
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtMember), k + 1);
        DrawFont8(0x1A - 40 * static_cast<int>(B(at::kTimer)), y + 5, 0);
    }
    SH_CALL(PartyForm_DrawReserve)(45 * static_cast<int>(B(at::kTimer)) + 0x96, 0x3B);
    SH_CALL(Menu_DrawTitleBox)(0x14, 0x12 - 8 * static_cast<int>(B(at::kTimer)), 0x118, 0x13, B(at::kStyle));
}

// original 0x581300: the reserve's panels from (x, y): the port's dropped call
// (x, y, 0x12, 0x15); then for each record of 0x6BC894 below the count
// 0x6BC897 (read again each row), at X = x + 7, Y = y + 6 + 0x34 row: the box
// (X, Y, 0x7D, 0x30, 0x80, style), the portrait (X + 0x56, Y, +9, +0x10 bit 7
// as 2), the name (X + 0x14, Y + 1, 0, 5), the tag 0x574400(X + 2, Y + 0x15,
// 3, 0), the level (+0xA) at (X + 0x18, Y + 0x15); the status text in the
// small font at (X + 0x30, Y + 0x15, 1, 0x10) - 0x66A0E8 for +0x10 bit 7 (with
// bit 5 as well: on Frame_Counter bit 5, else 0x66A0F0), 0x66A0F0 for bit 5
// alone; the tag (X + 2, Y + 0x1D, 0, 0); HP +0x18 and its maximum +0x20 at
// (X + 0x18, Y + 0x1D) - the HP's colour 4 with +0x11 bit 5, 2 at 1 or less,
// the maximum's 4 with +0x1E set; the tag (X + 2, Y + 0x25, 1, 0); AP +0x1A
// (colour 4 at a quarter of its maximum or less, 2 at 0) and its maximum
// +0x22 at (X + 0x18, Y + 0x25).
//
// As the original has it: the colours go out in a dword whose upper bytes
// are the stack's (Text_DrawFont8 reads six bits); the portrait's shade over
// the record offset's upper bytes (Menu_DrawItemIcon reads the byte).
//
// DIV-0011 lives at the first call: menu_frame.cpp's RetargetCall re-aims the
// original's site 0x581313 (the empty 0x4DF820) at Menu_DrawFrame, the frame
// the PlayStation draws. Ours calls whatever that site reaches - read from its
// rel32 - so the divergence and its BOF3X_ORIGINAL=Menu_DrawFrame switch work
// for ours as for Capcom's body (docs/field_s.md section 5).
extern "C" void __cdecl PartyForm_DrawReserve(int x, int y) {
    SH_AT(void (__cdecl*)(int, int, int, int), field_s::FrameCallTarget())(x, y, 0x12, 0x15);
    const int X = x + 7;
    int Y = y + 6;
    if (B(at::kReserveCount) == 0) return;
    const int pen = X + 0x18;
    int base = y + 0x1B;
    unsigned char row = 0;
    do {
        const U index = RecordOf(B(at::kReserve + row));
        const unsigned char* const rec = Rec(index);
        unsigned char colour = 0;
        SH_CALL(Menu_DrawBox)(X, Y, 0x7D, 0x30, 0x80, B(at::kStyle));
        SH_CALL(Menu_DrawItemIcon)(X + 0x56, Y, rec[9], (rec[0x10] >> 6) & 2);
        SH_CALL(Text_DrawAt)(X + 0x14, base - 0x14, 0, 5, rec);
        SH_AT(void (__cdecl*)(int, int, int, int), at::kDrawTag)(X + 2, base, 3, 0);
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtLevel), rec[0xA]);
        DrawFont8(pen, base, 0);
        const U flags = W(RecAddr(index) + 0x10);
        U status = 0;
        if (flags & 0x80) {
            if (flags & 0x20) status = (Frame_Counter & 0x20) ? at::kStatusA : at::kStatusB;
            else status = at::kStatusA;
        } else if (flags & 0x20) {
            status = at::kStatusB;
        }
        if (status != 0) SH_CALL(Text_DrawSmall)(X + 0x30, base, 1, 0x10, Text(status));
        const int hp_y = base + 8;
        SH_AT(void (__cdecl*)(int, int, int, int), at::kDrawTag)(X + 2, hp_y, 0, 0);
        if (rec[0x11] & 0x20) colour = 4;
        const U hp = W(RecAddr(index) + 0x18);
        if (hp <= 1) colour = 2;
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtFigureA), hp);
        DrawFont8(pen, hp_y, colour);
        colour = rec[0x1E] != 0 ? 4 : 0;
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtFigureB), W(RecAddr(index) + 0x20));
        DrawFont8(pen, hp_y, colour);
        const int ap_y = base + 0x10;
        SH_AT(void (__cdecl*)(int, int, int, int), at::kDrawTag)(X + 2, ap_y, 1, 0);
        const U quarter = (W(RecAddr(index) + 0x22) >> 2) & 0xFFFF;
        const U ap = W(RecAddr(index) + 0x1A);
        colour = 0;
        if (ap <= quarter) colour = 4;
        if (ap == 0) colour = 2;
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtFigureA), ap);
        DrawFont8(pen, ap_y, colour);
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtFigureB), W(RecAddr(index) + 0x22));
        DrawFont8(pen, ap_y, 0);
        Y += 0x34;
        base += 0x34;
        ++row;
    } while (row < B(at::kReserveCount));
}

// original 0x581590: the two picks exchanged; answers 0xFF when they were,
// else the id refused. In one column: the two bytes of the party list (or of
// the reserve) at the rows (s8) swapped by 0x58BD50. Across: the party's id
// (at its row) and the reserve's; an id whose record +0xB has bit 1 is
// refused (the party's asked first); else sound 0x103, every byte of both
// party lists below Party_Count(0) (asked after each) that holds the party's
// id gets the reserve's, and every reserve byte below the count 0x6BC897
// holding the reserve's gets the party's.
extern "C" unsigned char __cdecl PartyForm_Swap(void) {
    const unsigned char column = B(at::kColumn);
    if (column == B(at::kPickColumn)) {
        const U list = column == 0 ? at::kPartyList : at::kReserve;
        SwapBytes(list + static_cast<U>(S8(B(at::kRow))), list + static_cast<U>(S8(B(at::kPickRow))));
        return 0xFF;
    }
    unsigned char party, other;
    if (column == 0) {
        party = B(at::kPartyList + static_cast<U>(S8(B(at::kRow))));
        other = B(at::kReserve + static_cast<U>(S8(B(at::kPickRow))));
    } else {
        party = B(at::kPartyList + static_cast<U>(S8(B(at::kPickRow))));
        other = B(at::kReserve + static_cast<U>(S8(B(at::kRow))));
    }
    if (Rec(RecordOf(party))[0xB] & 2) return party;
    if (Rec(RecordOf(other))[0xB] & 2) return other;
    Sound(0x103);
    if (PartyCount() != 0) {
        unsigned char k = 0;
        do {
            if (B(at::kPartyList + k) == party) B(at::kPartyList + k) = other;
            if (B(at::kPartyList2 + k) == party) B(at::kPartyList2 + k) = other;
            ++k;
        } while (k < PartyCount());
    }
    const U n = B(at::kReserveCount);
    for (U i = 0; i < n; ++i)
        if (B(at::kReserve + i) == other) B(at::kReserve + i) = party;
    return 0xFF;
}

// original 0x581720: the formation screen: the backdrop, the title box (0x14,
// 0x12, 0x118, 0x13, style); for each member k below Party_Count(0) (asked
// once) its panel at (0x11, 54 k + 0x3E) lit by its record's +0xB bit 1 and its
// number at (0x1A, 54 k + 0x43) in colour 7 with +0xB bit 1, else 0; the
// reserve at (0x96, 0x3B); a pick's cursor box - the cell of 0x664078 at
// 6 (column + 2 row), s8 each: (x, y) words and a flag (0x7E x 0x30 when set,
// else 0x80 x 0x34), not blinking - and the cursor's, blinking.
extern "C" void __cdecl PartyForm_Draw(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    const U n = PartyCount();
    SH_CALL(Menu_DrawTitleBox)(0x14, 0x12, 0x118, 0x13, B(at::kStyle));
    for (U k = 0; k < n; ++k) {
        const U record = RecordOf(B(at::kPartyList + k));
        const int y = static_cast<int>(54 * k) + 0x3E;
        SH_CALL(Menu_DrawMemberStatus)(0x11, y, record, (Rec(record)[0xB] >> 1) & 1);
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtMember), k + 1);
        DrawFont8(0x1A, y + 5, (Rec(record)[0xB] & 2) ? 7 : 0);
    }
    SH_CALL(PartyForm_DrawReserve)(0x96, 0x3B);
    const unsigned char pick = B(at::kPickColumn);
    if (pick != 0x7F) {
        const U cell = at::kPickCells + static_cast<U>(6 * (S8(pick) + 2 * S8(B(at::kPickRow))));
        const bool wide = B(cell + 4) != 0;
        SH_CALL(Menu_DrawCursorBox)(static_cast<int>(W(cell)), static_cast<int>(W(cell + 2)), wide ? 0x7E : 0x80, wide ? 0x30 : 0x34, 0, 6);
    }
    const U cell = at::kPickCells + static_cast<U>(6 * (S8(B(at::kColumn)) + 2 * S8(B(at::kRow))));
    const bool wide = B(cell + 4) != 0;
    SH_CALL(Menu_DrawCursorBox)(static_cast<int>(W(cell)), static_cast<int>(W(cell + 2)), wide ? 0x7E : 0x80, wide ? 0x30 : 0x34, 1, 6);
}

// ===========================================================================
// ShopMode_States[9]'s two window set-ups (group FS's; the states are nobody's)
// ===========================================================================

// original 0x5836E0: three windows of WindowRecords set up - record 0
// (0x803160: on, kind 7, +2 0, +3 2, at (0x14, -0x14), help word 0), record 21
// (0x803454: kind 7, +2 3, off) and record 1 (0x803184: kind 7, +2 0xF, +3 2,
// +0xA / +0xB / +0xD 0, +0xC 0x7F, +8 0, at (-0x64, 0x3E), on). Called by
// 0x583370.
extern "C" void __cdecl ShopBrowse_InitWindows(void) {
    B(0x803161) = 7;
    B(0x803162) = 0;
    B(0x803163) = 2;
    B(0x803160) = 1;
    SetW(0x803164, 0x14);
    SetW(0x803166, 0xFFEC);
    SetW(0x803170, 0);
    B(0x803455) = 7;
    B(0x803456) = 3;
    B(0x803454) = 0;
    B(0x803185) = 7;
    B(0x803186) = 0xF;
    B(0x803187) = 2;
    B(0x80318F) = 0;
    B(0x80318E) = 0;
    B(0x803190) = 0x7F;
    B(0x803191) = 0;
    B(0x80318C) = 0;
    SetW(0x803188, 0xFF9C);
    SetW(0x80318A, 0x3E);
    B(0x803184) = 1;
}

// original 0x583770: two more windows - record 2 (0x8031A8: kind 7, +2 0x10,
// +3 2, at (0x140, 0x3F), on, +0xA the chosen entry 0x803191) and record 3
// (0x8031CC: kind 7, +2 0x11, +3 2, at (0xB4, 0xF0), on). Called by 0x5833E0
// on a confirm.
extern "C" void __cdecl ShopBrowse_OpenDetail(void) {
    B(0x8031B2) = B(0x803191);
    B(0x8031A9) = 7;
    B(0x8031AA) = 0x10;
    B(0x8031AB) = 2;
    SetW(0x8031AC, 0x140);
    SetW(0x8031AE, 0x3F);
    B(0x8031A8) = 1;
    B(0x8031CD) = 7;
    B(0x8031CE) = 0x11;
    B(0x8031CF) = 2;
    SetW(0x8031D0, 0xB4);
    SetW(0x8031D2, 0xF0);
    B(0x8031CC) = 1;
}

// ===========================================================================
// The resistance shop (ShopResist_States 0x6641BC, ShopMode_States[5]'s,
// jumped through by 0x5837E0 on 0x929F01): a member (0x929F06) is given one
// of the eight bits of its record's +0x1D (0x929F0D) for ten zenny a level
// (+0xA) - the bits Char_RecalcStats turns into resistances
// (docs/char-stats.md section 2) - once: a record with any bit set is refused.
// ===========================================================================

namespace {
// The cursor box on the member row (s8 0x929F06): (0x11, 54 row + 0x3E, 0x80,
// 0x34, blink, 6). The row's upper half is a callee's leftover in the
// original; Menu_DrawCursorBox reads the low word.
void MemberCursor(int blink) {
    SH_CALL(Menu_DrawCursorBox)(0x11, 54 * S8(B(at::kCursor)) + 0x3E, 0x80, 0x34, blink, 6);
}
}  // namespace

// original 0x5837F0: ShopResist_States[0], sliding in. Sound 0x102 on its
// first frame (the counter 6); the members sliding, the bits box at
// (32 t + 0x98, 0x3F) with no row lit; the counter down one; at 0 the cursor,
// the counter, 0x929F05 and 0x929F0F 0, the state up one, step 0.
extern "C" void __cdecl ShopResist_Open(void) {
    if (B(at::kTimer) == 6) Sound(0x102);
    SH_CALL(ShopResist_DrawMembers)(1);
    SH_CALL(ShopResist_DrawBits)(B(at::kCursor), 32 * static_cast<int>(B(at::kTimer)) + 0x98, 0x3F, 0, 0xFF);
    const auto t = static_cast<unsigned char>(B(at::kTimer) - 1);
    B(at::kTimer) = t;
    if (t != 0) return;
    const auto state = B(at::kState);
    B(at::kCursor) = 0;
    B(at::kTimer) = 0;
    B(at::kMenu05) = 0;
    B(at::kMessageEnd) = 0;
    B(at::kState) = static_cast<unsigned char>(state + 1);
    B(at::kStep) = 0;
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
    SH_CALL(ShopResist_DrawBits)(B(at::kCursor), 0x98, 0x3F, 0, 0xFF);
    MemberCursor(1);
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0x5000);
    if (repeat & 0x4000) {
        Sound(0x100);
        const auto c = static_cast<unsigned char>(B(at::kCursor) + 1);
        B(at::kCursor) = c;
        if (S8(c) >= S8(count)) B(at::kCursor) = 0;
    }
    if (repeat & 0x1000) {
        Sound(0x100);
        const auto c = static_cast<unsigned char>(B(at::kCursor) - 1);
        B(at::kCursor) = c;
        if (S8(c) < 0) {
            --count;
            B(at::kCursor) = count;
        }
    }
    if (Confirmed(Pressed())) {
        if (Rec(MemberAt(S8(B(at::kCursor))))[0x1D] != 0) {
            Sound(0x107);
            const auto state = static_cast<unsigned char>(B(at::kState) + 6);
            B(at::kAnswer) = 1;
            B(at::kState) = state;
            B(at::kMessageEnd) = 2;
            B(at::kTimer) = 0x3C;
        } else {
            Sound(0x104);
            const auto state = static_cast<unsigned char>(B(at::kState) + 1);
            B(at::kPickRow) = 0;
            B(at::kState) = state;
        }
    }
    if (Cancelled(Pressed())) {
        Sound(0x106);
        B(at::kAnswer) = 0xF;
        B(at::kState) = 6;
        B(at::kTimer) = 0x3C;
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
    SH_CALL(Text_DrawAt)(0x1C, 0x15, 0, 0xFF, Message(static_cast<U>(S8(B(at::kPickRow)) + 0x65)));
    SH_CALL(ShopResist_DrawBits)(B(at::kCursor), 0x98, 0x3F, 0, 0xFF);
    MemberCursor(0);
    SH_CALL(Menu_DrawHand)(0x9B, 13 * S8(B(at::kPickRow)) + 0x44, 0);
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0x5000);
    if (repeat & 0x4000) {
        Sound(0x100);
        const auto b = static_cast<unsigned char>(B(at::kPickRow) + 1);
        B(at::kPickRow) = b;
        if (S8(b) >= 8) B(at::kPickRow) = 0;
    }
    if (repeat & 0x1000) {
        Sound(0x100);
        const auto b = static_cast<unsigned char>(B(at::kPickRow) - 1);
        B(at::kPickRow) = b;
        if (S8(b) < 0) B(at::kPickRow) = 7;
    }
    if (Confirmed(Pressed())) {
        const U record = MemberAt(S8(B(at::kCursor)));
        const U zenny = L(at::kZenny);
        if (10u * Rec(record)[0xA] > zenny) {
            Sound(0x107);
            const auto state = static_cast<unsigned char>(B(at::kState) + 5);
            B(at::kAnswer) = 0xB;
            B(at::kState) = state;
            B(at::kMessageEnd) = 0xC;
            B(at::kTimer) = 0x3C;
        } else {
            Sound(0x104);
            const auto state = static_cast<unsigned char>(B(at::kState) + 1);
            B(at::kAnswer) = 0;
            B(at::kState) = state;
        }
    }
    if (Cancelled(Pressed())) {
        Sound(0x106);
        B(at::kState) = static_cast<unsigned char>(B(at::kState) - 1);
    }
}

// original 0x583BE0: ShopResist_States[3], "buy?". The members, message 0x6D,
// the bits with the chosen one lit, the cursor box, Menu_YesNo: on yes
// (0x929F0B) message 0xC, counter 0x3C, the zenny less ten times the level
// (unchecked: PickBit asked), the state up two (the grant); on no sound 0x106
// and the state down two (the member).
extern "C" void __cdecl ShopResist_Confirm(void) {
    SH_CALL(ShopResist_DrawMembers)(0);
    SH_CALL(Text_DrawAt)(0x1C, 0x15, 0, 0xFF, Message(0x6D));
    SH_CALL(ShopResist_DrawBits)(B(at::kCursor), 0x98, 0x3F, 0, B(at::kPickRow));
    MemberCursor(0);
    if ((SH_CALL(Menu_YesNo)() & 0xFF) == 0) return;
    if (B(at::kAnswer) != 0) {
        const int cursor = S8(B(at::kCursor));
        B(at::kAnswer) = 0xC;
        B(at::kTimer) = 0x3C;
        const U level = Rec(MemberAt(cursor))[0xA];
        const U zenny = L(at::kZenny) - 10u * level;
        const auto state = static_cast<unsigned char>(B(at::kState) + 2);
        SetL(at::kZenny, zenny);
        B(at::kState) = state;
        return;
    }
    Sound(0x106);
    B(at::kState) = static_cast<unsigned char>(B(at::kState) - 2);
}

// original 0x583CD0: ShopResist_States[4], sliding out. The members sliding,
// the bits at (32 t + 0x98, 0x3F); the counter up one; at 7 the counter 0, the
// mode 1 (the field), state and step 0.
extern "C" void __cdecl ShopResist_Close(void) {
    SH_CALL(ShopResist_DrawMembers)(1);
    SH_CALL(ShopResist_DrawBits)(B(at::kCursor), 32 * static_cast<int>(B(at::kTimer)) + 0x98, 0x3F, 0, 0xFF);
    const auto t = static_cast<unsigned char>(B(at::kTimer) + 1);
    B(at::kTimer) = t;
    if (t != 7) return;
    B(at::kTimer) = 0;
    B(at::kMode) = 1;
    B(at::kState) = 0;
    B(at::kStep) = 0;
}

// original 0x583D30: ShopResist_States[5], the grant. The bits with the chosen
// one lit, the members, the cursor box; the messages from 0x929F0B to 0xF
// (ShopResist_Message); done, the member's +0x1D = 1 << bit (the byte shift of
// the s8 bit, its count masked to five bits: 8 and up give 0),
// Char_RecalcStats of the record and the state down four (the member).
extern "C" void __cdecl ShopResist_Grant(void) {
    SH_CALL(ShopResist_DrawBits)(B(at::kCursor), 0x98, 0x3F, 0, B(at::kPickRow));
    SH_CALL(ShopResist_DrawMembers)(0);
    MemberCursor(0);
    if ((SH_CALL(ShopResist_Message)(At(at::kAnswer), 0xF) & 0xFF) == 0) return;
    const U record = MemberAt(S8(B(at::kCursor)));
    const U count = static_cast<U>(B(at::kPickRow)) & 0x1F;
    Rec(record)[0x1D] = static_cast<unsigned char>(count >= 8 ? 0 : (1u << count));
    SH_CALL(Char_RecalcStats)(Rec(record));
    B(at::kState) = static_cast<unsigned char>(B(at::kState) - 4);
}

// original 0x583DE0: ShopResist_States[6], the farewell. The bits, the
// members; the messages to 0x10; done, the counter 0 and the state down two
// (the slide out).
extern "C" void __cdecl ShopResist_Farewell(void) {
    SH_CALL(ShopResist_DrawBits)(B(at::kCursor), 0x98, 0x3F, 0, 0xFF);
    SH_CALL(ShopResist_DrawMembers)(0);
    if ((SH_CALL(ShopResist_Message)(At(at::kAnswer), 0x10) & 0xFF) == 0) return;
    const auto state = B(at::kState);
    B(at::kTimer) = 0;
    B(at::kState) = static_cast<unsigned char>(state - 2);
}

// original 0x583E30: ShopResist_States[7], a notice (the record refused, the
// zenny short). The bits, the members, the cursor box blinking; the messages
// to 0x929F0F (a byte); done, the state down six (the member).
extern "C" void __cdecl ShopResist_Notice(void) {
    SH_CALL(ShopResist_DrawBits)(B(at::kCursor), 0x98, 0x3F, 0, 0xFF);
    SH_CALL(ShopResist_DrawMembers)(0);
    MemberCursor(1);
    if ((SH_CALL(ShopResist_Message)(At(at::kAnswer), B(at::kMessageEnd)) & 0xFF) == 0) return;
    B(at::kState) = static_cast<unsigned char>(B(at::kState) - 6);
}

// original 0x583EA0: the bits box of member row `member` (its record through
// Field_Members): the box (x + 3, y + 3, 0x9E, 0x70, 0, colour) and its border
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
    const U price = (10u * B(RecAddr(record) + 0xA)) & 0xFFFF;
    U text = colour & 0xFFFFFF00u;
    if (L(at::kZenny) >= price && B(RecAddr(record) + 0x1D) == 0)
        text |= (colour & 0xFF) != 0 ? 7u : 0u;
    else
        text |= 7u;
    for (U k = 0; k < 8; ++k) {
        const U bit = 1u << k;
        if ((B(RecAddr(record) + 0x1D) & bit & 0xFF) == 0 && k != (highlight & 0xFF))
            SH_CALL(Text_DrawAt)(x + 8, row_y, static_cast<int>(text), 0xFF, Message(k + 0xDE));
        else
            SH_CALL(Text_DrawAt)(x + 8, row_y, 2, 0xFF, Message(k + 0xDE));
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtCost), price);
        SH_CALL(Text_DrawFont12)(x + 0x68, row_y, static_cast<int>(text), reinterpret_cast<const unsigned char*>(PrintBuf()));
        row_y += 0xD;
    }
}

// original 0x584010: the resistance shop's members: the title box (0x14,
// 0x10 - 10 s, 0x118, 0x13, style) where s is the counter when `sliding`, else
// 0; for each member k below Party_Count(0) (asked after each) its panel
// (0x11 - 32 s, 54 k + 0x3E, its record, lit when the record's +0x1D is not 0);
// the money box (32 s + 0xB4, 0x26, 0, zenny).
//
// As the original has it: s is a word over the caller's esi, so every
// coordinate carries the caller's upper half; Menu_DrawTitleBox,
// Menu_DrawMemberStatus's draws and Menu_DrawMoneyBox's read the low word.
extern "C" void __cdecl ShopResist_DrawMembers(unsigned sliding) {
    const int s = (sliding & 0xFF) != 0 ? static_cast<int>(B(at::kTimer)) : 0;
    SH_CALL(Menu_DrawTitleBox)(0x14, 0x10 - 10 * s, 0x118, 0x13, B(at::kStyle));
    if (PartyCount() != 0) {
        unsigned char k = 0;
        do {
            const U record = MemberAt(k);
            SH_CALL(Menu_DrawMemberStatus)(0x11 - 32 * s, 54 * k + 0x3E, record, Rec(record)[0x1D] != 0 ? 1u : 0u);
            ++k;
        } while (k < PartyCount());
    }
    SH_CALL(Menu_DrawMoneyBox)(32 * s + 0xB4, 0x26, 0, L(at::kZenny));
}

// original 0x584120: one message of a run - system message 0x63 + *index at
// (0x1C, 0x14) - held until the counter runs down or any pad bit is pressed;
// then the counter 0x3C and *index up one; answers 1 when that reached `end`'s
// byte, else 0.
extern "C" unsigned char __cdecl ShopResist_Message(unsigned char* index, unsigned end) {
    SH_CALL(Text_DrawAt)(0x1C, 0x14, 0, 0xFF, Message(static_cast<U>(*index) + 0x63));
    const auto t = static_cast<unsigned char>(B(at::kTimer) - 1);
    B(at::kTimer) = t;
    if (t != 0 && Pressed() == 0) return 0;
    B(at::kTimer) = 0x3C;
    const auto next = static_cast<unsigned char>(*index + 1);
    *index = next;
    return next == static_cast<unsigned char>(end) ? 1 : 0;
}

// ===========================================================================
// The shared ability list (SharedList_States 0x664254, ShopMode_States[7]'s,
// jumped through by 0x584180 on 0x929F01): the 128 ability ids at 0x904574
// that AbilityList_Add fills when asked for the shared list, and the members'
// ten-byte lists at record +0x7E. State 1 chooses between moving abilities (1:
// SharedList_MoveSteps) and arranging the list (0: SharedList_SortSteps); the
// first wants key item 0xF or, without it, one of item 0x58 of category 0,
// used up (SharedList_UseItem).
// ===========================================================================

// original 0x584190: SharedList_States[0]. The set-up, the choice 0, the
// state up one.
extern "C" void __cdecl SharedList_Begin(void) {
    SH_CALL(SharedList_Setup)();
    const auto state = B(at::kState);
    B(at::kSharedChoice) = 0;
    B(at::kState) = static_cast<unsigned char>(state + 1);
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
    const U choice = static_cast<U>(S8(B(at::kSharedChoice)));
    B(0x80318F) = 0xFF;
    SetW(0x803170, choice + 0x73);
    if (B(0x803187) == 0) {
        const U x = 48 * choice + L(0x803188);
        const U y = W(0x80318A) + 4;
        B(0x803454) = 1;
        SetW(0x803458, x);
        SetW(0x80345A, y);
    } else {
        B(0x803454) = 0;
    }
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0xA000);
    if (repeat & 0xA000) {
        Sound(0x101);
        B(at::kSharedChoice) = static_cast<unsigned char>(B(at::kSharedChoice) ^ 1);
    }
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        if (B(at::kSharedChoice) != 0) {
            if ((SH_CALL(KeyItem_Has)(at::kKeyItemInk) & 0xFF) == 0 &&
                (SH_CALL(Inventory_Count)(0, at::kInkItem, 0) & 0xFFFF) == 0) {
                Sound(0x107);
                return;
            }
            if ((SH_CALL(KeyItem_Has)(at::kKeyItemInk) & 0xFF) == 0) {
                const auto state = B(at::kState);
                B(at::kAnswer) = 1;
                B(at::kState) = static_cast<unsigned char>(state + 1);
            } else {
                Sound(0x102);
                const unsigned char first = B(at::kJoined);
                const auto state = static_cast<unsigned char>(B(at::kState) + 1);
                B(0x8031A9) = 7;
                B(0x8031CD) = 7;
                B(at::kPickIndex) = 0;
                B(0x8031AA) = 0xD;
                B(0x8031AB) = 2;
                B(0x8031A8) = 1;
                B(0x8031B2) = first;
                SetW(0x8031AC, 0xFF6A);
                SetW(0x8031AE, 0x74);
                B(0x8031CE) = 0xC;
                B(0x8031CF) = 2;
                B(0x8031CC) = 1;
                B(0x8031D6) = first;
                B(0x8031D7) = 0;
                B(0x8031D8) = 0xFF;
                B(0x8031D9) = 0;
                SetW(0x8031D0, 0x140);
                SetW(0x8031D2, 0x3E);
                B(at::kState) = state;
                B(at::kStep) = 1;
                Sound(0x103);
                B(0x80318F) = B(at::kSharedChoice);
                B(at::kTimer) = 5;
                return;
            }
        } else {
            Sound(0x104);
            Sound(0x102);
            const auto state = static_cast<unsigned char>(B(at::kState) + 2);
            B(0x8031F1) = 7;
            B(at::kState) = state;
            B(0x8031F2) = 0xB;
            B(0x8031F3) = 3;
            B(0x8031F0) = 1;
            B(0x8031FA) = 0;
            B(0x8031FB) = 0;
            B(0x8031FC) = 0xFF;
            B(0x8031FD) = 0;
            SetW(0x8031F4, 0xFF56);
            SetW(0x8031F6, 0x3E);
        }
        B(0x80318F) = B(at::kSharedChoice);
        Sound(0x103);
        B(at::kTimer) = 5;
        B(at::kStep) = 0;
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    B(0x803163) = 1;
    B(0x803187) = 1;
    B(0x803454) = 0;
    if ((SH_CALL(KeyItem_Has)(at::kKeyItemInk) & 0xFF) == 0) B(0x80323B) = 1;
    B(at::kTimer) = 5;
    B(at::kState) = 4;
}

// original 0x584460: SharedList_States[2], a tail jump through
// SharedList_MoveSteps 0x664268 by the step byte, unchecked.
extern "C" void __cdecl SharedList_MoveStep(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(CodeAt(at::kSharedListMoveSteps, B(at::kStep), "SharedList_MoveStep"))();
}

// original 0x584470: SharedList_MoveSteps[0], "use the item?". The help word
// 0x36; the hand on (at window 0's x - 36 answer (s8) + 0xE8, its y + 5, as
// words). The pressed dword's 0xA000 flips 0x929F0B with sound 0x101 (the
// dword read again). Confirm on yes: sounds 0x104 and 0x102, the pick 0,
// Inventory_Remove(0, 0x58, 1), windows 2 and 3 open on the joined record at
// the pick (read again), the step up one. Confirm on no, or cancel: sound
// 0x106 and the state down one.
extern "C" void __cdecl SharedList_UseItem(void) {
    const U hand_x = W(0x803164) - 36u * static_cast<U>(S8(B(at::kAnswer))) + 0xE8;
    const U hand_y = W(0x803166) + 5;
    U pressed = L(at::kPressed);
    SetW(0x803170, 0x36);
    B(0x803454) = 1;
    SetW(0x803458, hand_x);
    SetW(0x80345A, hand_y);
    if (pressed & 0xA000) {
        Sound(0x101);
        B(at::kAnswer) = static_cast<unsigned char>(B(at::kAnswer) ^ 1);
        pressed = L(at::kPressed);
    }
    if (Confirmed(pressed)) {
        if (B(at::kAnswer) != 0) {
            Sound(0x104);
            Sound(0x102);
            B(at::kPickIndex) = 0;
            SH_CALL(Inventory_Remove)(0, at::kInkItem, 1);
            const unsigned char rec = B(at::kJoined + B(at::kPickIndex));
            const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
            OpenMemberWindows(rec);
            B(at::kStep) = step;
            return;
        }
    } else if (!Cancelled(pressed)) {
        return;
    }
    Sound(0x106);
    B(at::kState) = static_cast<unsigned char>(B(at::kState) - 1);
}

// original 0x5845E0: SharedList_MoveSteps[1], and ShopMode_States[9]'s step
// table 0x6641B0 [0]: the counter down one; at 0 the step up one.
extern "C" void __cdecl Menu_StepAfterTimer(void) {
    const auto t = static_cast<unsigned char>(B(at::kTimer) - 1);
    B(at::kTimer) = t;
    if (t == 0) B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
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
    SH_CALL(TextRecord_Set)(0, 8, Rec(B(at::kJoined + B(at::kPickIndex))));
    const unsigned char open = B(0x8031CF);
    SetW(0x803170, 0x75);
    if (open == 0) {
        const U x = L(0x8031D0) + 0x2A;
        const U y = W(0x8031D2) + 7;
        B(0x803454) = 1;
        SetW(0x803458, x);
        SetW(0x80345A, y);
    } else {
        B(0x803454) = 0;
    }
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0xF000);
    if (repeat & 0x8000) {
        Sound(0x101);
        const auto p = static_cast<unsigned char>(B(at::kPickIndex) - 1);
        B(0x8031D9) = 0x32;
        B(at::kPickIndex) = p;
        if (S8(p) < 0) B(at::kPickIndex) = static_cast<unsigned char>(B(at::kJoinedCount) - 1);
    } else if (repeat & 0x2000) {
        Sound(0x101);
        const auto p = static_cast<unsigned char>(B(at::kPickIndex) + 1);
        const int last = static_cast<int>(B(at::kJoinedCount)) - 1;
        B(at::kPickIndex) = p;
        B(0x8031D9) = 0x31;
        if (static_cast<int>(B(at::kPickIndex)) > last) B(at::kPickIndex) = 0;
    }
    const unsigned char rec = B(at::kJoined + B(at::kPickIndex));
    B(0x8031B2) = rec;
    B(0x8031D6) = rec;
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        Sound(0x103);
        Sound(0x102);
        const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
        B(0x8031FA) = 0;
        B(0x8031FB) = 0;
        B(0x8031FD) = 0;
        B(0x8031AB) = 1;
        B(0x8031D8) = 0xFF;
        B(0x8031CF) = 3;
        B(0x8031F1) = 7;
        B(0x8031F2) = 0xB;
        B(0x8031F3) = 2;
        B(0x8031F0) = 1;
        B(0x8031FC) = 0xFF;
        SetW(0x8031F4, 0x140);
        SetW(0x8031F6, 0x3E);
        B(at::kStep) = step;
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    const auto state = static_cast<unsigned char>(B(at::kState) - 1);
    B(0x8031AB) = 1;
    B(0x8031CF) = 1;
    B(at::kState) = state;
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
    const U member_rec = B(0x8031D6);
    unsigned char slot = B(0x8031D7);
    const U ability = B(RecAddr(member_rec) + 0x7E + slot);
    SetW(0x803170, W(at::kAbilityRecords + 0x16 + 24 * ability));
    if (B(0x8031CF) == 0) {
        const U x = L(0x8031D0) + 7;
        SetW(0x803458, x);
        B(0x803454) = 1;
        SetW(0x80345A, 13 * (slot + 2u) + W(0x8031D2));
    } else {
        B(0x803454) = 0;
    }
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0xF000);
    unsigned char cl = B(0x8031D7);
    const unsigned char before = cl;
    if (repeat & 0x1000) {
        --cl;
        B(0x8031D7) = cl;
        if (S8(cl) < 0) {
            cl = 0;
            B(0x8031D7) = 0;
        }
    } else if (repeat & 0x4000) {
        ++cl;
        B(0x8031D7) = cl;
        if (cl > 9) {
            cl = 9;
            B(0x8031D7) = 9;
        }
    }
    if (before != cl) {
        Sound(0x100);
        cl = B(0x8031D7);
    }
    B(0x8031D8) = 0xFF;
    const U cell = RecAddr(B(0x8031D6)) + 0x7E + cl;
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        Sound(0x103);
        B(0x8031D8) = B(0x8031D7);
        B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
        return;
    }
    if (pressed & 0x10) {
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
    const auto step = static_cast<unsigned char>(B(at::kStep) - 1);
    B(0x8031AB) = 2;
    B(0x8031CF) = 4;
    B(0x8031D8) = 0xFF;
    B(0x8031F3) = 1;
    B(at::kStep) = step;
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
    const unsigned char cur = B(0x8031FB);
    B(0x803454) = 1;
    SetW(0x803170, W(at::kAbilityRecords + 0x16 + 24u * B(at::kSharedList + cur)));
    SetW(0x803458, L(0x8031F4) + 7);
    SetW(0x80345A, 13 * (static_cast<U>(cur) - B(0x8031FA) + 2) + W(0x8031F6));
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0x500C);
    const unsigned char before = B(0x8031FB);
    unsigned char cl = ScrollList(repeat, 0x8031FB, 0x8031FA, 0x8031FD);
    if (before != cl) {
        Sound(0x100);
        cl = B(0x8031FB);
    }
    if (B(0x8031FD) != 0) return;
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        const U slot = RecAddr(B(0x8031D6)) + 0x7E + B(0x8031D7);
        const unsigned char entry = B(at::kSharedList + cl);
        if (entry == 0 && B(slot) == 0) {
            Sound(0x107);
            return;
        }
        const unsigned char had = B(slot);
        B(slot) = entry;
        B(at::kSharedList + B(0x8031FB)) = had;
        Sound(0x103);
        B(at::kStep) = static_cast<unsigned char>(B(at::kStep) - 1);
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) - 1);
}

// original 0x584BB0: SharedList_States[3], a tail jump through
// SharedList_SortSteps 0x66427C by the step byte, unchecked.
extern "C" void __cdecl SharedList_SortStep(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(CodeAt(at::kSharedListSortSteps, B(at::kStep), "SharedList_SortStep"))();
}

// original 0x584BC0: SharedList_SortSteps[0]: the counter down one; at 0
// window 6 (0x803214) open - kind 7, +2 0xE, +0xA 0, +0xB 0xFF, at (0xC8,
// 0x3F) - and the step up one.
extern "C" void __cdecl SharedList_SortOpen(void) {
    const auto t = static_cast<unsigned char>(B(at::kTimer) - 1);
    B(at::kTimer) = t;
    if (t != 0) return;
    const auto step = B(at::kStep);
    B(0x803215) = 7;
    B(0x803216) = 0xE;
    B(0x803214) = 1;
    B(0x80321E) = 0;
    B(0x80321F) = 0xFF;
    SetW(0x803218, 0xC8);
    SetW(0x80321A, 0x3F);
    B(at::kStep) = static_cast<unsigned char>(step + 1);
}

// original 0x584C10: SharedList_SortSteps[1], window 6's three rows (+0xA).
// The help word 0x18, the hand at (its x + 5, its y + 16 row + 0x1A). The
// repeat of 0x5000: 0x1000 up (sound 0x101; below 0, s8, to 2), 0x4000 down
// (past 2 to 0). Confirm: sound 0x103; row 0 - +0xB 0 and the step up one
// (the arranging); else SharedList_Sort(row). Cancel: sounds 0x102 and 0x106,
// window 6 off, window 4's +3 4, the state down two.
extern "C" void __cdecl SharedList_SortMenu(void) {
    SetW(0x803170, 0x18);
    SetW(0x803458, W(0x803218) + 5);
    SetW(0x80345A, 16u * B(0x80321E) + W(0x80321A) + 0x1A);
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0x5000);
    if (repeat & 0x1000) {
        Sound(0x101);
        const auto r = static_cast<unsigned char>(B(0x80321E) - 1);
        B(0x80321E) = r;
        if (S8(r) < 0) B(0x80321E) = 2;
    } else if (repeat & 0x4000) {
        Sound(0x101);
        const auto r = static_cast<unsigned char>(B(0x80321E) + 1);
        B(0x80321E) = r;
        if (r > 2) B(0x80321E) = 0;
    }
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        Sound(0x103);
        const unsigned char row = B(0x80321E);
        if (row == 0) {
            B(0x80321F) = 0;
            B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
            return;
        }
        SH_CALL(SharedList_Sort)(row);
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x102);
    Sound(0x106);
    const auto state = static_cast<unsigned char>(B(at::kState) - 2);
    B(0x803214) = 0;
    B(0x8031F3) = 4;
    B(at::kState) = state;
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
    const unsigned char cur = B(0x8031FB);
    B(0x803454) = 1;
    SetW(0x803170, W(at::kAbilityRecords + 0x16 + 24u * B(at::kSharedList + cur)));
    SetW(0x803458, W(0x8031F4));
    SetW(0x80345A, 13 * (static_cast<U>(cur) - B(0x8031FA) + 2) + W(0x8031F6));
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0x500C);
    const unsigned char before = B(0x8031FB);
    unsigned char cl = ScrollList(repeat, 0x8031FB, 0x8031FA, 0x8031FD);
    if (before != cl) {
        Sound(0x100);
        cl = B(0x8031FB);
    }
    if (B(0x8031FD) != 0) return;
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        if (B(0x8031FC) == 0xFF) {
            if (B(at::kSharedList + cl) != 0) {
                Sound(0x103);
                B(0x8031FC) = B(0x8031FB);
                return;
            }
            Sound(0x107);
            return;
        }
        Sound(0x103);
        const U first = B(0x8031FC);
        B(0x8031FC) = 0xFF;
        const unsigned char a = B(at::kSharedList + first);
        const U second = B(0x8031FB);
        B(at::kSharedList + first) = B(at::kSharedList + second);
        B(at::kSharedList + second) = a;
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    if (B(0x8031FC) != 0xFF) {
        B(0x8031FC) = 0xFF;
        return;
    }
    B(0x80321F) = 0xFF;
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) - 1);
}

// original 0x584F90: the shared list's set-up. The joined records (+0xB bit
// 0) of the eight in order to 0x6BC8BC, their count to 0x6BC8C5 (cleared
// first); window 0 (kind 7, +2 0, +3 2, on, +0xA 0, at (0x14, -0x14)), window 1
// (kind 7, +2 2, +3 2, on, +0xE 4, +0xF 0xFF, at (0x70, -0x14)) and window 21
// (kind 7, +2 3, +0xA 4, off); without key item 0xF, window 5 (0x803238: kind
// 7, +2 0x12, +3 2, on, at (0x140, 0x26)) - the item count's.
extern "C" void __cdecl SharedList_Setup(void) {
    unsigned char n = 0;
    B(at::kJoinedCount) = 0;
    for (unsigned char r = 0; r < 8; ++r)
        if (Rec(r)[0xB] & 1) B(at::kJoined + n++) = r;
    B(at::kJoinedCount) = n;
    B(0x803161) = 7;
    B(0x803162) = 0;
    B(0x803163) = 2;
    B(0x803160) = 1;
    B(0x80316A) = 0;
    SetW(0x803164, 0x14);
    SetW(0x803166, 0xFFEC);
    B(0x803185) = 7;
    B(0x803186) = 2;
    B(0x803187) = 2;
    B(0x803184) = 1;
    B(0x80318E) = 4;
    B(0x80318F) = 0xFF;
    SetW(0x803188, 0x70);
    SetW(0x80318A, 0xFFEC);
    B(0x803455) = 7;
    B(0x803456) = 3;
    B(0x80345E) = 4;
    B(0x803454) = 0;
    if ((SH_CALL(KeyItem_Has)(at::kKeyItemInk) & 0xFF) != 0) return;
    B(0x803239) = 7;
    B(0x80323A) = 0x12;
    B(0x80323B) = 2;
    B(0x803238) = 1;
    SetW(0x80323C, 0x140);
    SetW(0x80323E, 0x26);
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
// is pushed before 0x591AC0's three and popped with the next calls'.
extern "C" void __cdecl SharedList_DrawList(unsigned char* window) {
    const int wx = static_cast<int>(move_script::Word(window + 4));
    const int wy = static_cast<int>(move_script::Word(window + 6));
    SH_CALL(Menu_DrawBox)(wx + 3, wy + 3, 0x99, 0x9A, 0, B(at::kStyle));
    unsigned char offset = 0;   // both written by Menu_ListScroll on every path
    unsigned char moving = 0;
    const unsigned char top = SH_CALL(Menu_ListScroll)(window + 0xA, &offset, &moving, window + 0xD);
    const int y0 = S8(offset) + move_script::Word(window + 6);
    const unsigned char* p = At(at::kSharedList + top);
    int y = y0 + 0x1A;
    for (U row = 0; static_cast<int>(row) < static_cast<int>(moving) + 9; ++row, ++p, y += 0xD) {
        const unsigned char id = *p;
        if (id == 0) continue;
        const unsigned char adjust = (window[0xD] & 0xF0) != 0xF0 ? moving : 0;
        const U record = at::kAbilityRecords + 24u * id;
        const unsigned char kind = SH_CALL(Skill_FlagIndex)(id);
        const U cursor = static_cast<U>(window[0xC]) + adjust;
        const U at_row = row + window[0xA];
        const int colour = at_row == cursor ? 2 : 0;
        const U picked = static_cast<U>(window[0xB]) + adjust;
        const int x = move_script::Word(window + 4) + 7;
        if (at_row == picked || at_row == cursor) {
            SH_CALL(Menu_DrawSkillRow)(x, y, 7, kind, At(record), B(record + 0x12), 1);
            SH_CALL(Menu_DrawSkillRow)(move_script::Word(window + 4) + 7, y - 2, colour, kind, At(record), B(record + 0x12), 0);
        } else {
            SH_CALL(Menu_DrawSkillRow)(x, y, 0, kind, At(record), B(record + 0x12), 0);
        }
    }
    SH_CALL(Menu_DrawBox)(move_script::Word(window + 4) + 3, move_script::Word(window + 6) + 3, 0x99, 0x14, window[9], B(at::kStyle));
    SH_CALL(Menu_DrawBox)(move_script::Word(window + 4) + 3, move_script::Word(window + 6) + 0x92, 0x99, 8, window[9], B(at::kStyle));
    const int title_y = move_script::Word(window + 6) + 7;
    const unsigned char length = SH_CALL(Text_CharCount)(Text(at::kListTitle));
    SH_CALL(Text_DrawAt)(6 * (0xD - static_cast<int>(length)) + move_script::Word(window + 4), title_y, 0, 0x10, Text(at::kListTitle));
    const U count = SH_AT(U (__cdecl*)(unsigned, unsigned, unsigned), at::kAbilityListCount)(0, 0xFF, 0) & 0xFF;
    SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtCount), count);
    DrawFont8(move_script::Word(window + 4) + 0x55, move_script::Word(window + 6) + 0x92, 0);
    SH_CALL(Menu_DrawPieces)(move_script::Word(window + 4), move_script::Word(window + 6), Text(at::kPiecesA), 1);
    SH_CALL(Menu_DrawPieces)(move_script::Word(window + 4), move_script::Word(window + 6), Text(at::kPiecesB), 1);
    for (int i = 0; i < 10; ++i) SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 8 * i + 0x28, move_script::Word(window + 6), 1, 1);
    for (int i = 0; i < 15; ++i) SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4), move_script::Word(window + 6) + 8 * i + 0x18, 4, 1);
    SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 0x90, move_script::Word(window + 6) + 0x18, 0x16, 1);
    for (int i = 0; i < 12; ++i) SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 0x90, move_script::Word(window + 6) + 8 * i + 0x28, 0x17, 1);
    SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 0x90, move_script::Word(window + 6) + 0x88, 0x18, 1);
    SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4), move_script::Word(window + 6) + 0x90, 0x19, 1);
    for (int i = 0; i < 9; ++i) SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 8 * i + 8, move_script::Word(window + 6) + 0x90, 0x1A, 1);
    SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 0x50, move_script::Word(window + 6) + 0x90, 0x1B, 1);
    for (int i = 0; i < 6; ++i) SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 8 * i + 0x58, move_script::Word(window + 6) + 0x90, 0x1C, 1);
    SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 0x88, move_script::Word(window + 6) + 0x90, 0x1D, 1);
    SH_CALL(Menu_DrawScrollBar)(At(at::kSharedList), window[0xA], move_script::Word(window + 4) + 0x90, move_script::Word(window + 6) + 0x18, 9, 0x80, 0x74);
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
    SH_CALL(Menu_DrawBox)(move_script::Word(window + 4) + 3, move_script::Word(window + 6) + 3, 0x92, 0xA0, 0, B(at::kStyle));
    int y = move_script::Word(window + 6) + 0x1A;
    U slot = RecAddr(window[0xA]) + 0x7E;
    for (unsigned char k = 0; k < 0xA; ++k, y += 0xD, ++slot) {
        if (B(slot) == 0) continue;
        const unsigned char kind = SH_CALL(Skill_FlagIndex)(B(slot));
        const U record = at::kAbilityRecords + 24u * B(slot);
        const int colour = k != window[0xC] ? 0 : 2;
        const int x = move_script::Word(window + 4) + 7;
        if (k == window[0xB] || k == window[0xC]) {
            SH_CALL(Menu_DrawSkillRow)(x, y, 7, kind, At(record), B(record + 0x12), 1);
            SH_CALL(Menu_DrawSkillRow)(move_script::Word(window + 4) + 7, y - 2, colour, kind, At(record), B(record + 0x12), 0);
        } else {
            SH_CALL(Menu_DrawSkillRow)(x, y, 0, kind, At(record), B(record + 0x12), 0);
        }
    }
    const unsigned char* const name = Rec(window[0xA]);
    unsigned char n = SH_CALL(Text_CharCount)(name);
    if (n > 5) n = 5;
    SH_CALL(Text_DrawAt)(6 * (0xC - static_cast<int>(n)) + move_script::Word(window + 4), move_script::Word(window + 6) + 7, 0, 5, name);
    SH_CALL(Menu_DrawPieces)(move_script::Word(window + 4), move_script::Word(window + 6), Text((window[0xD] & 2) ? at::kPiecesC : at::kPiecesA), 1);
    SH_CALL(Menu_DrawPieces)(move_script::Word(window + 4) - 8, move_script::Word(window + 6), Text((window[0xD] & 1) ? at::kPiecesE : at::kPiecesD), 1);
    const unsigned char state = window[0xD];
    window[0xD] = (state & 0xF0) == 0 ? 0 : static_cast<unsigned char>(state - 0x10);
    for (int i = 0; i < 9; ++i) SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 8 * i + 0x28, move_script::Word(window + 6), 1, 1);
    for (int i = 0; i < 17; ++i) SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4), move_script::Word(window + 6) + 8 * i + 0x18, 4, 1);
    SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 0x88, move_script::Word(window + 6) + 0x18, 0x20, 1);
    for (int i = 0; i < 15; ++i) SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 0x88, move_script::Word(window + 6) + 8 * i + 0x20, 0x21, 1);
    SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4), move_script::Word(window + 6) + 0xA0, 5, 1);
    SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 0x88, move_script::Word(window + 6) + 0x98, 0x22, 1);
    for (int i = 0; i < 16; ++i) SH_CALL(Menu_DrawPiece)(move_script::Word(window + 4) + 8 * i + 8, move_script::Word(window + 6) + 0xA0, 6, 1);
}

// original 0x5857E0: a tail jump through SharedList_Sorts 0x664298 by the
// argument's byte, unchecked (the argument stays on the stack for the entry,
// which reads none): 0 compacts, 1 sorts up, 2 down.
extern "C" void __cdecl SharedList_Sort(unsigned how) {
    [[clang::musttail]] return reinterpret_cast<void (__cdecl*)(unsigned)>(
        CodeAt(at::kSharedListSorts, how & 0xFF, "SharedList_Sort"))(how);
}

// original 0x5857F0: SharedList_Sorts[0]: the empty entries of the shared
// list moved to its end - 127 passes, pass p over entries i below 128 - p
// (the p-th from 0x7F down), swapping (0x58BD50) an empty entry with a
// non-empty next one.
extern "C" void __cdecl SharedList_Compact(void) {
    for (U limit = 0x7F; limit != 0; --limit)
        for (U i = 0; i < limit; ++i) {
            const U e = at::kSharedList + i;
            if (B(e) == 0 && B(e + 1) != 0) SwapBytes(e, e + 1);
        }
}

// The two sorts' pass: adjacent non-empty entries swapped when their abilities'
// byte +0x12 compare as `swap` says.
template <typename Swap> void SortShared(Swap swap) {
    for (U limit = 0x7F; limit != 0; --limit)
        for (U i = 0; i < limit; ++i) {
            const U e = at::kSharedList + i;
            const unsigned char a = B(e);
            if (a == 0) continue;
            const unsigned char b = B(e + 1);
            if (b == 0) continue;
            if (swap(B(at::kAbilityRecords + 24u * a + 0x12), B(at::kAbilityRecords + 24u * b + 0x12))) SwapBytes(e, e + 1);
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

// original 0x585940: window kind handler (called by 0x59C260 with x, y): the
// count of item 0x58 of category 0 - the box (x + 3, y + 3, 0x55, 0x10, 0,
// style), the label 0x66A118 at (x + 4, y + 4), the count (Inventory_Count(0,
// 0x58, 0), a word) in the 12 px font at (x + 0x34, y + 4); the piece lists
// 0x66422C, 0x664238 nine times 8 apart, 0x664244.
extern "C" void __cdecl SharedList_DrawItemCount(int x, int y) {
    SH_CALL(Menu_DrawBox)(x + 3, y + 3, 0x55, 0x10, 0, B(at::kStyle));
    SH_CALL(Text_DrawAt)(x + 4, y + 4, 0, 0xFF, Text(at::kItemLabel));
    const U count = SH_CALL(Inventory_Count)(0, at::kInkItem, 0) & 0xFFFF;
    SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtItems), count);
    SH_CALL(Text_DrawFont12)(x + 0x34, y + 4, 0, reinterpret_cast<const unsigned char*>(PrintBuf()));
    SH_CALL(Menu_DrawPieces)(x, y, Text(at::kPiecesF), 0);
    for (int i = 0; i < 9; ++i) SH_CALL(Menu_DrawPieces)(x + 8 * i, y, Text(at::kPiecesG), 0);
    SH_CALL(Menu_DrawPieces)(x, y, Text(at::kPiecesH), 0);
}

// ===========================================================================
// The equip screen's two choosers (the field menu's Equipment states 5 and 6,
// entries 12 and 13 of 0x667380 - its state table 0x66739C by 0x929F01,
// docs/menu-screens.md section 1)
// ===========================================================================

// original 0x58C7A0: the slot. The backdrop; the hand on at (window 0x803334's
// x + 6, its y + 13 slot + 0x57); the repeat of 0xF000: 0x4000 down (sound
// 0x101; past 5 to 0), 0x1000 up (below 0, s8, to 5); window +0xB 0xFF, 0x803341
// 1; 0x8000 the member back (sound 0x101; below 0 to Party_Count(0) less one),
// 0x2000 on (past it to 0). The member (0x803340) and slot (0x80333E) to the
// item window 0x803358 (+0xC, +9), its top from 0x939880 [slot + 6 member];
// the slot's byte of the member's record - slot 0 (or past 5) the weapon +0x12
// (category 1), 1..3 the armour +0x13..+0x15 (2), 4 and 5 the accessories
// +0x16 / +0x17 (3) - and its help word (Item_HelpMessage) to 0x8032FC.
// Confirm: sound 0x103, +0xB the slot, the item window's cursor from
// 0x9398A0 [slot + 6 member], the state up one. Pressed bit 4: an item in a
// slot but the weapon's is taken off (sound 0x103, Inventory_Add(category,
// item, 1), the byte 0, Char_RecalcStats), else a buzz (0x107). Cancel:
// sounds 0x106 and 0x102, the member window's +0x1B 3, 0x803337 4, the slot and
// +0xB 0xFF, each member's window (0x8031F0 + 0x24 k) +3 8, the hand off, the
// counter 0, the state down two.
extern "C" void __cdecl Equip_ChooseSlot(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    const U slot0 = B(0x80333E);
    B(0x803454) = 1;
    SetW(0x803458, L(0x803338) + 6);
    SetW(0x80345A, 13 * slot0 + W(0x80333A) + 0x57);
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0xF000);
    B(0x80333F) = 0xFF;
    B(0x803341) = 1;
    if (repeat & 0x4000) {
        Sound(0x101);
        const auto s = static_cast<unsigned char>(B(0x80333E) + 1);
        B(0x80333E) = s;
        if (s > 5) B(0x80333E) = 0;
    } else if (repeat & 0x1000) {
        Sound(0x101);
        const auto s = static_cast<unsigned char>(B(0x80333E) - 1);
        B(0x80333E) = s;
        if (S8(s) < 0) B(0x80333E) = 5;
    }
    auto count = static_cast<unsigned char>(SH_CALL(Party_Count)(0));
    if (repeat & 0x8000) {
        Sound(0x101);
        const auto m = static_cast<unsigned char>(B(0x803340) - 1);
        B(0x803340) = m;
        if (S8(m) < 0) {
            --count;
            B(0x803340) = count;
        }
    } else if (repeat & 0x2000) {
        Sound(0x101);
        const auto m = static_cast<unsigned char>(B(0x803340) + 1);
        B(0x803340) = m;
        if (static_cast<int>(B(0x803340)) > S8(count) - 1) B(0x803340) = 0;
    }
    const unsigned char member = B(0x803340);
    const unsigned char slot = B(0x80333E);
    B(0x803364) = member;
    B(0x803361) = slot;
    B(0x803362) = B(at::kEquipBytes + slot + 6u * member);
    const U record = RecAddr(RecordOf(B(at::kPartyList + member)));
    U category, cell;
    switch (slot) {
    case 1: category = 2; cell = record + 0x13; break;
    case 2: category = 2; cell = record + 0x14; break;
    case 3: category = 2; cell = record + 0x15; break;
    case 4: category = 3; cell = record + 0x16; break;
    case 5: category = 3; cell = record + 0x17; break;
    default: category = 1; cell = record + 0x12; break;
    }
    SetW(0x8032FC, SH_CALL(Item_HelpMessage)(category, B(cell)));
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        Sound(0x103);
        const U m = B(0x803340);
        const unsigned char s = B(0x80333E);
        B(0x80333F) = s;
        const auto state = static_cast<unsigned char>(B(at::kState) + 1);
        B(0x803363) = B(at::kEquipCursor + s + 6u * m);
        B(at::kState) = state;
        return;
    }
    if (pressed & 0x10) {
        if (B(0x80333E) != 0 && B(cell) != 0) {
            Sound(0x103);
            SH_CALL(Inventory_Add)(category, B(cell), 1);
            B(cell) = 0;
            SH_CALL(Char_RecalcStats)(At(record));
            return;
        }
        Sound(0x107);
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    Sound(0x102);
    B(0x80335B) = 3;
    B(0x803337) = 4;
    B(0x80333F) = 0xFF;
    B(0x80333E) = 0xFF;
    if (PartyCount() != 0) {
        unsigned char k = 0;
        do {
            B(0x8031F3 + 0x24u * k) = 8;
            ++k;
        } while (k < PartyCount());
    }
    const auto state = B(at::kState);
    B(0x803454) = 0;
    B(at::kTimer) = 0;
    B(at::kState) = static_cast<unsigned char>(state - 2);
}

// original 0x58CAE0: the item (the item window 0x803358: +0xA the top, +0xB
// the cursor, +8 the category, +0xD the chosen item, +0x10 the scroll word).
// The backdrop; the hand on at (its x + 7, its y + 13 (cursor - top + 2)); the
// chosen item's help word to 0x8032FC; the repeat of 0xF00C; the preview
// 0x58D640; the scroll of a 128-entry list shown nine rows at a time (as
// SharedList_PickShared's, but the page down's test is on the cursor), the
// scroll word 0xF0 / 0x10 set when the cursor leaves the page; the top and
// cursor kept in 0x939880 / 0x9398A0 [slot + 6 member] - slots 4 and 5 both
// (an accessory's two); sound 0x100 when the cursor moved. While the list
// scrolls nothing more. Confirm: with an item, sound 0x104, the change
// 0x58D570, the cursor 0xFF and the state down one; without, a buzz. Cancel:
// sound 0x106, the cursor 0xFF, window 0x803400's +0xC 0, the state down one.
extern "C" void __cdecl Equip_ChooseItem(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    const U hand_y = 13 * (static_cast<U>(B(0x803363)) - B(0x803362) + 2) + W(0x80335E);
    SetW(0x803458, L(0x80335C) + 7);
    B(0x803454) = 1;
    SetW(0x80345A, hand_y);
    SetW(0x8032FC, SH_CALL(Item_HelpMessage)(B(0x803360), B(0x803365)));
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0xF00C);
    SH_AT(void (__cdecl*)(), at::kEquipPreview)();
    unsigned char al = B(0x803363);
    const unsigned char before = al;
    unsigned char cl;
    if (repeat & 0x1000) {
        if (al != 0) {
            --al;
            B(0x803363) = al;
        }
        cl = B(0x803362);
        if (al < cl) SetW(0x803368, 0xF0);
    } else if (repeat & 0x4000) {
        if (al < 0x7F) {
            ++al;
            B(0x803363) = al;
        }
        cl = B(0x803362);
        if (static_cast<int>(al) >= static_cast<int>(cl) + 9) SetW(0x803368, 0x10);
    } else if (repeat & 4) {
        cl = B(0x803362);
        if (cl == 0) {
            al = 0;
            B(0x803363) = 0;
        } else if (cl < 9) {
            al = static_cast<unsigned char>(al - cl);
            cl = 0;
            B(0x803363) = al;
            B(0x803362) = cl;
        } else {
            al = static_cast<unsigned char>(al - 9);
            cl = static_cast<unsigned char>(cl - 9);
            B(0x803363) = al;
            B(0x803362) = cl;
        }
    } else {
        cl = B(0x803362);
        if (repeat & 8) {
            if (cl == 0x77) {
                al = 0x7F;
                B(0x803363) = 0x7F;
            } else if (al > 0x6E) {
                const auto d = static_cast<unsigned char>(0x77 - cl);
                cl = 0x77;
                al = static_cast<unsigned char>(al + d);
                B(0x803363) = al;
                B(0x803362) = cl;
            } else {
                al = static_cast<unsigned char>(al + 9);
                cl = static_cast<unsigned char>(cl + 9);
                B(0x803363) = al;
                B(0x803362) = cl;
            }
        }
    }
    const unsigned char slot = B(0x80333E);
    const U row = 6u * B(0x803340);
    if (slot < 4) {
        B(at::kEquipBytes + slot + row) = cl;
        B(at::kEquipCursor + slot + row) = al;
    } else {
        B(at::kEquipBytes + 5 + row) = cl;
        B(at::kEquipCursor + 5 + row) = al;
        B(at::kEquipBytes + 4 + row) = cl;
        B(at::kEquipCursor + 4 + row) = al;
    }
    if (before != al) Sound(0x100);
    if (W(0x803368) != 0) return;
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        if (B(0x803365) != 0) {
            Sound(0x104);
            SH_AT(void (__cdecl*)(), at::kEquipApply)();
            const auto state = B(at::kState);
            B(0x803363) = 0xFF;
            B(at::kState) = static_cast<unsigned char>(state - 1);
            return;
        }
        Sound(0x107);
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    const auto state = B(at::kState);
    B(0x803363) = 0xFF;
    B(0x80340C) = 0;
    B(at::kState) = static_cast<unsigned char>(state - 1);
}

void FieldS_Inject() {
    if (bof3::WantsShadow("field_s")) field_s::SelfTest();
    BOF3_INJECT(FieldSave_Confirm);
    BOF3_INJECT(Rest_Begin);
    BOF3_INJECT(Rest_PlaceParty);
    BOF3_INJECT(PartyForm_OpenStep);
    BOF3_INJECT(PartyForm_Setup);
    BOF3_INJECT(PartyForm_FadeWait);
    BOF3_INJECT(PartyForm_OpenWait);
    BOF3_INJECT(PartyForm_SlideIn);
    BOF3_INJECT(PartyForm_Choose);
    BOF3_INJECT(PartyForm_LeaveStep);
    BOF3_INJECT(PartyForm_LeaveFade);
    BOF3_INJECT(PartyForm_LeaveBlack);
    BOF3_INJECT(PartyForm_Reload);
    BOF3_INJECT(PartyForm_End);
    BOF3_INJECT(PartyForm_DrawSliding);
    BOF3_INJECT(PartyForm_DrawReserve);
    BOF3_INJECT(PartyForm_Swap);
    BOF3_INJECT(PartyForm_Draw);
    BOF3_INJECT(ShopBrowse_InitWindows);
    BOF3_INJECT(ShopBrowse_OpenDetail);
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
    BOF3_INJECT(SharedList_Setup);
    BOF3_INJECT(SharedList_DrawList);
    BOF3_INJECT(SharedList_DrawMember);
    BOF3_INJECT(SharedList_Sort);
    BOF3_INJECT(SharedList_Compact);
    BOF3_INJECT(SharedList_SortCostDown);
    BOF3_INJECT(SharedList_SortCostUp);
    BOF3_INJECT(SharedList_DrawItemCount);
    BOF3_INJECT(Equip_ChooseSlot);
    BOF3_INJECT(Equip_ChooseItem);
}
