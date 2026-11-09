// Moved here by tools/move_funcs.py from field_s.cpp, rest_2c.cpp (docs/source-layout-round.md).
#include "game/field/party_form.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field/party_form_callees.h"
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
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = party_form::at;

}  // namespace

namespace {

using U = std::uint32_t;

unsigned char* At(U address) { return move_script::At(address); }

unsigned char& B(U address) { return *At(address); }

U W(U address) { return move_script::Word(At(address)); }

U L(U address) { return static_cast<U>(move_script::Long(At(address))); }

void SetL(U address, U v) { move_script::SetLong(At(address), static_cast<std::int32_t>(v)); }

int S8(U v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

int S16(U v) { return static_cast<short>(static_cast<unsigned short>(v)); }

// CharacterRecords' record n (stride 0xA4), unchecked.
unsigned char* Rec(U n) { return At(::bof3::Img(CharacterRecords_addr) + n * field_s::at::kRecordStride); }

U RecAddr(U n) { return ::bof3::Img(CharacterRecords_addr) + n * field_s::at::kRecordStride; }

// A member id's record index: Character_IndexById[id], unchecked.
U RecordOf(U id) { return Character_IndexById[id & 0xFF]; }

U Pressed() { return Input_Pressed; }

bool Confirmed(U pressed) { return (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed & 0xFFFF) != 0; }

bool Cancelled(U pressed) { return (static_cast<std::uint32_t>(Field_CancelButtons) & pressed & 0xFFFF) != 0; }

void Sound(U id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }

U PartyCount() { return static_cast<U>(SH_CALL(Party_Count)(0)) & 0xFF; }

const unsigned char* Message(U id) { return SH_CALL(Msg_SystemPtr)(id); }

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

constexpr auto& AtRest2c = move_script::At;

using move_script::Long;

U LRest2c(U address) { return static_cast<U>(Long(AtRest2c(address))); }

void Jump(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(LRest2c(table + 4 * index)))();
}

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x580A40, ShopMode_States[6]: `jmp [PartyForm_States + 4 * 0x929F01]`.
extern "C" void __cdecl PartyForm_Dispatch(void) { Jump(::bof3::Img(PartyForm_States_addr), 4, FieldS_State, "PartyForm_Dispatch"); }

#pragma clang attribute pop

// original 0x580A50 (PSX 0x801DE17C): PartyForm_States[0], a tail jump through
// PartyForm_OpenSteps 0x6640AC by the step byte, unchecked.
extern "C" void __cdecl PartyForm_OpenStep(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(CodeAt(::bof3::Img(PartyForm_OpenSteps_addr), FieldS_Step, "PartyForm_OpenStep"))();
}

// original 0x580A60 (PSX 0x801DE1B8): PartyForm_OpenSteps[0]. The reserve 0x6BC894: every
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
    FieldS_ReserveCount = 0;
    for (U r = 0; r < 8; ++r) {
        const unsigned char* const rec = Rec(r);
        if ((rec[0xB] & 1) == 0) continue;
        U k = 0;
        const unsigned char id = rec[9];
        while (k < n && id != B(::bof3::Img(field_s::at::kPartyList) + k)) ++k;
        if (k == n) B(::bof3::Img(field_s::at::kReserve) + count++) = rec[9];
    }
    FieldS_ReserveCount = count;
    U edx = 0;
    if (n > 0) {
        for (U k = 0; k < n; ++k) B(::bof3::Img(field_s::at::kReserveFlags) + 2 * k) = 1;
        edx = n;
    }
    if (n == 0 || edx < 3) {
        U p = ::bof3::Img(field_s::at::kReserveFlags) + 2 * edx;
        do {
            B(p) = 0;
            p += 2;
        } while (p < ::bof3::Img(field_s::at::kReserveFlags) + 6);
    }
    for (U k = 0; k < count; ++k) B(::bof3::Img(field_s::at::kReserveFlags) + 1 + 2 * k) = 1;
    const auto step = static_cast<unsigned char>(FieldS_Step + 1);
    FieldS_Column = 0;
    BattleE1_ModeByte9 = 0;
    ShopStates_Clear0A = 0x7F;
    FieldS_PickRow = 0x7F;
    FieldS_PartyFormResult = 0xFF;
    B(::bof3::Img(field_s::at::kTimer)) = 3;
    FieldS_Step = step;
    SH_CALL(Transition_Start)(2);
}

// original 0x580B80 (PSX 0x801DE380): PartyForm_OpenSteps[1]. The transition over:
// Transition_Start(3) and the step up one.
extern "C" void __cdecl PartyForm_FadeWait(void) {
    if (Transition_Busy != 0) return;
    SH_CALL(Transition_Start)(3);
    FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// original 0x580BB0 (PSX 0x801DE3C4): PartyForm_OpenSteps[2]. The screen drawn; the transition
// over: step 0 and the state up two (to the choosing).
extern "C" void __cdecl PartyForm_OpenWait(void) {
    SH_CALL(PartyForm_Draw)();
    if (Transition_Busy != 0) return;
    const auto state = FieldS_State;
    FieldS_Step = 0;
    FieldS_State = static_cast<unsigned char>(state + 2);
}

// original 0x580BE0 (PSX 0x801DE414): PartyForm_States[1]. The backdrop (kind 0x903A5B, in eax
// over the entry's upper bytes; Menu_DrawBackdrop reads its byte), the screen
// at the slide's offset, the counter down one; at 0 the counter 3 and the
// state up one.
extern "C" void __cdecl PartyForm_SlideIn(void) {
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    SH_CALL(PartyForm_DrawSliding)();
    const auto t = static_cast<unsigned char>(B(::bof3::Img(field_s::at::kTimer)) - 1);
    B(::bof3::Img(field_s::at::kTimer)) = t;
    if (t != 0) return;
    const auto state = FieldS_State;
    B(::bof3::Img(field_s::at::kTimer)) = 3;
    FieldS_State = static_cast<unsigned char>(state + 1);
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
    if (FieldS_PartyFormResult == 0xFF) {
        SH_CALL(Text_DrawAt)(0x1A, 0x14, 0, 0xFF, Message(0x60));
    } else {
        SH_CALL(TextRecord_Set)(0, 8, Rec(RecordOf(FieldS_PartyFormResult)));
        SH_CALL(Text_DrawAt)(0x1A, 0x14, 0, 0xFF, Message(0x61));
    }
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0xF000);
    const U count = PartyCount();
    unsigned char column = static_cast<unsigned char>(FieldS_Column);
    const unsigned char reserve = FieldS_ReserveCount;
    const unsigned char old_column = column;
    if ((repeat & 0xA000) != 0 && reserve != 0) {
        column = static_cast<unsigned char>(column ^ 1);
        FieldS_Column = column;
    }
    unsigned char row = static_cast<unsigned char>(BattleE1_ModeByte9);
    const int row0 = S8(row);
    if (column == 0) {
        if (row0 > static_cast<int>(count) - 1) {
            row = static_cast<unsigned char>(count - 1);
            BattleE1_ModeByte9 = row;
        }
    } else if (row0 > static_cast<int>(reserve) - 1) {
        row = static_cast<unsigned char>(reserve - 1);
        BattleE1_ModeByte9 = row;
    }
    if (repeat & 0x1000) {
        row = static_cast<unsigned char>(row - 1);
        BattleE1_ModeByte9 = row;
        if (S8(row) < 0) {
            row = static_cast<unsigned char>(static_cast<unsigned char>(FieldS_Column) == 0 ? count - 1 : reserve - 1);
            BattleE1_ModeByte9 = row;
        }
    } else if (repeat & 0x4000) {
        const unsigned char c = static_cast<unsigned char>(FieldS_Column);
        row = static_cast<unsigned char>(row + 1);
        BattleE1_ModeByte9 = row;
        const int last = c == 0 ? static_cast<int>(count) - 1 : static_cast<int>(reserve) - 1;
        if (S8(row) > last) {
            row = 0;
            BattleE1_ModeByte9 = 0;
        }
    }
    if (static_cast<int>(old_column) != S8(B(::bof3::Img(field_s::at::kColumn))) || row0 != S8(row)) Sound(0x100);
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        if (static_cast<unsigned char>(ShopStates_Clear0A) == 0x7F) {
            Sound(0x103);
            const unsigned char c = static_cast<unsigned char>(FieldS_Column), r = static_cast<unsigned char>(BattleE1_ModeByte9);
            ShopStates_Clear0A = c;
            FieldS_PickRow = r;
            return;
        }
        const unsigned char result = SH_CALL(PartyForm_Swap)();
        FieldS_PartyFormResult = result;
        Sound(result == 0xFF ? 0x103 : 0x107);
        ShopStates_Clear0A = 0x7F;
        FieldS_PickRow = 0x7F;
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    if (static_cast<unsigned char>(ShopStates_Clear0A) != 0x7F) {
        ShopStates_Clear0A = 0x7F;
        FieldS_PickRow = 0x7F;
        return;
    }
    const auto state = static_cast<unsigned char>(FieldS_State + 1);
    FieldS_Step = 0;
    FieldS_State = state;
    SH_CALL(Transition_Start)(2);
}

// original 0x580E80 (PSX 0x801DE808): PartyForm_States[3], a tail jump through
// PartyForm_LeaveSteps 0x6640B8 by the step byte, unchecked.
extern "C" void __cdecl PartyForm_LeaveStep(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(CodeAt(::bof3::Img(PartyForm_LeaveSteps_addr), FieldS_Step, "PartyForm_LeaveStep"))();
}

// original 0x580E90 (PSX 0x801DE844): PartyForm_LeaveSteps[0]. While the transition runs, a
// tail jump to PartyForm_Draw; then the black screen, the counter 2 and the
// step up one.
extern "C" void __cdecl PartyForm_LeaveFade(void) {
    if (Transition_Busy != 0) {
        [[clang::musttail]] return SH_CALL(PartyForm_Draw)();
    }
    SH_CALL(Menu_DrawBlackScreen)();
    const auto step = FieldS_Step;
    B(::bof3::Img(field_s::at::kTimer)) = 2;
    FieldS_Step = static_cast<unsigned char>(step + 1);
}

// original 0x580EC0 (PSX 0x801DE8A0): PartyForm_LeaveSteps[1]. The black screen, the counter
// down one; at 0 the step up one.
extern "C" void __cdecl PartyForm_LeaveBlack(void) {
    SH_CALL(Menu_DrawBlackScreen)();
    const auto t = static_cast<unsigned char>(B(::bof3::Img(field_s::at::kTimer)) - 1);
    B(::bof3::Img(field_s::at::kTimer)) = t;
    if (t == 0) FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
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
    const unsigned char kept = ObjTrio[0].char_id;
    SH_CALL(PartySet_Load)(Cond_Flags[0xd2], Cond_Flags[0xd3], Cond_Flags[0xd4], 0);
    bool lone = false;
    if (AreaW3c_CampFlag != 0) {
        const unsigned char flag = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x77);
        const U area = W(::bof3::Img(field_s::at::kArea));
        if (((flag & 0xFF) == 0 || area == 0xBB) && area != 0x85) lone = true;
    }
    if (lone) {
        const unsigned char member = CharacterRecords[0].char_id;
        std::memcpy(At(::bof3::Img(field_s::at::kLeaderName)), At(::bof3::Img(CharacterRecords_addr)), 0xA4);
        ObjTrio[0].actor = 0;
        Sprite_Current = rv::Bytes(ObjTrio);
        Field_MemberCount = 1;
        ObjTrio[0].scale_mode = 0;
        SH_CALL(Field_MemberSprite)(member, 0);
        SH_CALL(Sprite_SetAnimation)(rv::Current<records::PartyMember>()->direction);
        rv::Current<records::PartyMember>()->state = 0;
        rv::Current<records::PartyMember>()->step = 0;
        rv::Current<records::PartyMember>()->state3 = 0;
        rv::Current<records::PartyMember>()->state4 = 0;
    } else {
        Field_MemberCount = static_cast<unsigned char>(SH_CALL(Party_Count)(0));
        SH_CALL(Field_PartyLoad)(0);
        if (Field_MemberCount != 0) {
            U i = 0;
            do {
                records::PartyMember* const obj = &ObjTrio[i];
                const unsigned char member = obj->char_id;
                Sprite_Current = rv::Bytes(obj);
                SH_CALL(Field_MemberSprite)(member, i);
                SH_CALL(Sprite_SetAnimation)(rv::Current<records::PartyMember>()->direction);
                if (AreaW3c_CampFlag != 0) {
                    rv::Current<records::PartyMember>()->state = 0;
                    rv::Current<records::PartyMember>()->step = 0;
                    rv::Current<records::PartyMember>()->state3 = 0;
                    rv::Current<records::PartyMember>()->state4 = 0;
                }
                ++i;
            } while (i < Field_MemberCount);
        }
        if (kept != 2 && ObjTrio[0].char_id == 2) {
            const U facing = ObjTrio[0].direction * 4u;
            ObjTrio[0].x = static_cast<U>(ObjTrio[0].x) + (static_cast<U>(S16(W(::bof3::Img(field_s::at::kFacingSteps) + facing))) << 15);
            ObjTrio[0].z = static_cast<U>(ObjTrio[0].z) + (static_cast<U>(S16(W(::bof3::Img(field_s::at::kFacingSteps) + 2 + facing))) << 15);
        }
        if ((Field_StatusBits & 0x40) == 0) {
            Field_Kind2X = static_cast<long>(static_cast<U>(ObjTrio[0].x));
            Field_Kind2Z = static_cast<long>(static_cast<U>(ObjTrio[0].z));
            SH_CALL(Field_ViewReset)();
        }
    }
    if (Game_AreaNumber == 0x85) {
        const U n = Field_MemberCount;
        for (U i = 0; i < n; ++i) {
            const U obj = ::bof3::Img(0x802D40) + i * field_s::at::kObjStride;
            SetL(obj + 0x34, L(::bof3::Img(field_s::at::kArea85Place) + 16 * i));
            SetL(obj + 0x38, L(::bof3::Img(field_s::at::kArea85Place) + 16 * i + 4));
            SetL(obj + 0x3C, L(::bof3::Img(field_s::at::kArea85Place) + 16 * i + 8));
        }
        if ((Field_StatusBits & 0x40) != 0) {
            SH_CALL(Transition_Start)(3);
            FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
            return;
        }
        Field_Kind2X = static_cast<long>(static_cast<U>(ObjTrio[0].x));
        Field_Kind2Z = static_cast<long>(static_cast<U>(ObjTrio[0].z));
        SH_CALL(Field_ViewReset)();
    }
    if (Game_AreaNumber == 0x5C) {
        if (Field_MemberCount != 0) {
            U i = 0;
            do {
                const U obj = ::bof3::Img(0x802D40) + i * field_s::at::kObjStride;
                SetL(obj + 0x34, 0x190000);
                SetL(obj + 0x38, 0x3C0000);
                const long h = SH_CALL(AreaMap_Elevation)(static_cast<long>(static_cast<U>(ObjTrio[0].x)), static_cast<long>(static_cast<U>(ObjTrio[0].z)));
                SetL(obj + 0x3C, static_cast<U>(S16(static_cast<U>(h))) << 16);
                ++i;
            } while (i < Field_MemberCount);
        }
        Field_Kind2X = static_cast<long>(static_cast<U>(ObjTrio[0].x));
        Field_Kind2Z = static_cast<long>(static_cast<U>(ObjTrio[0].z));
        SH_CALL(Field_ViewReset)();
    }
    SH_CALL(Transition_Start)(3);
    FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// original 0x5811B0 (PSX 0x801DED14): PartyForm_LeaveSteps[3]. The transition over: the
// counter 0, the mode 1 (the field), state and step 0, 0x929F0F 1.
extern "C" void __cdecl PartyForm_End(void) {
    if (Transition_Busy != 0) return;
    B(::bof3::Img(field_s::at::kTimer)) = 0;
    BattleE1_Mode = 1;
    FieldS_State = 0;
    FieldS_Step = 0;
    AreaW3c_Load0F = 1;
}

// original 0x5811E0 (PSX 0x801DED58): the formation screen at the slide's offset (the counter
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
        const U record = RecordOf(B(::bof3::Img(field_s::at::kPartyList) + k));
        const int y = static_cast<int>(54 * k) + 0x3E;
        SH_CALL(Menu_DrawMemberStatus)(0x11 - 40 * static_cast<int>(B(::bof3::Img(field_s::at::kTimer))), y, record, Rec(record)[0xB] & 2);
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(::bof3::Img(field_s::at::kFmtMember)), k + 1);
        DrawFont8(0x1A - 40 * static_cast<int>(B(::bof3::Img(field_s::at::kTimer))), y + 5, 0);
    }
    SH_CALL(PartyForm_DrawReserve)(45 * static_cast<int>(B(::bof3::Img(field_s::at::kTimer))) + 0x96, 0x3B);
    SH_CALL(Menu_DrawTitleBox)(0x14, 0x12 - 8 * static_cast<int>(B(::bof3::Img(field_s::at::kTimer))), 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour));
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
    SH_WORD(void (__cdecl*)(int, int, int, int), field_s::FrameCallTarget())(x, y, 0x12, 0x15);
    const int X = x + 7;
    int Y = y + 6;
    if (FieldS_ReserveCount == 0) return;
    const int pen = X + 0x18;
    int base = y + 0x1B;
    unsigned char row = 0;
    do {
        const U index = RecordOf(B(::bof3::Img(field_s::at::kReserve) + row));
        const unsigned char* const rec = Rec(index);
        unsigned char colour = 0;
        SH_CALL(Menu_DrawBox)(X, Y, 0x7D, 0x30, 0x80, static_cast<unsigned char>(Config_WindowColour));
        SH_CALL(Menu_DrawItemIcon)(X + 0x56, Y, rec[9], (rec[0x10] >> 6) & 2);
        SH_CALL(Text_DrawAt)(X + 0x14, base - 0x14, 0, 5, rec);
        SH_AT(void (__cdecl*)(int, int, int, int), field_s::at::kDrawTag.v)(X + 2, base, 3, 0);
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(::bof3::Img(field_s::at::kFmtLevel)), rec[0xA]);
        DrawFont8(pen, base, 0);
        const U flags = W(RecAddr(index) + 0x10);
        U status = 0;
        if (flags & 0x80) {
            if (flags & 0x20) status = (Frame_Counter & 0x20) ? ::bof3::Img(field_s::at::kStatusA) : ::bof3::Img(field_s::at::kStatusB);
            else status = ::bof3::Img(field_s::at::kStatusA);
        } else if (flags & 0x20) {
            status = ::bof3::Img(field_s::at::kStatusB);
        }
        if (status != 0) SH_CALL(Text_DrawSmall)(X + 0x30, base, 1, 0x10, Text(status));
        const int hp_y = base + 8;
        SH_AT(void (__cdecl*)(int, int, int, int), field_s::at::kDrawTag.v)(X + 2, hp_y, 0, 0);
        if (rec[0x11] & 0x20) colour = 4;
        const U hp = W(RecAddr(index) + 0x18);
        if (hp <= 1) colour = 2;
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(::bof3::Img(field_s::at::kFmtFigureA)), hp);
        DrawFont8(pen, hp_y, colour);
        colour = rec[0x1E] != 0 ? 4 : 0;
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(::bof3::Img(field_s::at::kFmtFigureB)), W(RecAddr(index) + 0x20));
        DrawFont8(pen, hp_y, colour);
        const int ap_y = base + 0x10;
        SH_AT(void (__cdecl*)(int, int, int, int), field_s::at::kDrawTag.v)(X + 2, ap_y, 1, 0);
        const U quarter = (static_cast<U>(rv::As<records::CharRecord>(Rec(index))->stats.max_ap) >> 2) & 0xFFFF;
        const U ap = W(RecAddr(index) + 0x1A);
        colour = 0;
        if (ap <= quarter) colour = 4;
        if (ap == 0) colour = 2;
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(::bof3::Img(field_s::at::kFmtFigureA)), ap);
        DrawFont8(pen, ap_y, colour);
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(::bof3::Img(field_s::at::kFmtFigureB)), W(RecAddr(index) + 0x22));
        DrawFont8(pen, ap_y, 0);
        Y += 0x34;
        base += 0x34;
        ++row;
    } while (row < FieldS_ReserveCount);
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
    const unsigned char column = static_cast<unsigned char>(FieldS_Column);
    if (column == static_cast<unsigned char>(ShopStates_Clear0A)) {
        const U list = column == 0 ? (::bof3::Img(Cond_Flags_addr) + 0xd2) : ::bof3::Img(field_s::at::kReserve);
        SwapBytes(list + static_cast<std::uint32_t>(BattleE1_ModeByte9), list + static_cast<std::uint32_t>(FieldS_PickRow));
        return 0xFF;
    }
    unsigned char party, other;
    if (column == 0) {
        party = B(::bof3::Img(Cond_Flags_addr) + 0xd2 + static_cast<std::uint32_t>(BattleE1_ModeByte9));
        other = B(::bof3::Img(field_s::at::kReserve) + static_cast<std::uint32_t>(FieldS_PickRow));
    } else {
        party = B(::bof3::Img(Cond_Flags_addr) + 0xd2 + static_cast<std::uint32_t>(FieldS_PickRow));
        other = B(::bof3::Img(field_s::at::kReserve) + static_cast<std::uint32_t>(BattleE1_ModeByte9));
    }
    if (Rec(RecordOf(party))[0xB] & 2) return party;
    if (Rec(RecordOf(other))[0xB] & 2) return other;
    Sound(0x103);
    if (PartyCount() != 0) {
        unsigned char k = 0;
        do {
            if (B(::bof3::Img(field_s::at::kPartyList) + k) == party) B(::bof3::Img(field_s::at::kPartyList) + k) = other;
            if (B(::bof3::Img(field_s::at::kPartyList2) + k) == party) B(::bof3::Img(field_s::at::kPartyList2) + k) = other;
            ++k;
        } while (k < PartyCount());
    }
    const U n = FieldS_ReserveCount;
    for (U i = 0; i < n; ++i)
        if (B(::bof3::Img(field_s::at::kReserve) + i) == other) B(::bof3::Img(field_s::at::kReserve) + i) = party;
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
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    const U n = PartyCount();
    SH_CALL(Menu_DrawTitleBox)(0x14, 0x12, 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour));
    for (U k = 0; k < n; ++k) {
        const U record = RecordOf(B(::bof3::Img(field_s::at::kPartyList) + k));
        const int y = static_cast<int>(54 * k) + 0x3E;
        SH_CALL(Menu_DrawMemberStatus)(0x11, y, record, (Rec(record)[0xB] >> 1) & 1);
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(::bof3::Img(field_s::at::kFmtMember)), k + 1);
        DrawFont8(0x1A, y + 5, (Rec(record)[0xB] & 2) ? 7 : 0);
    }
    SH_CALL(PartyForm_DrawReserve)(0x96, 0x3B);
    const unsigned char pick = static_cast<unsigned char>(ShopStates_Clear0A);
    if (pick != 0x7F) {
        const U cell = ::bof3::Img(field_s::at::kPickCells) + static_cast<U>(6 * (S8(pick) + 2 * FieldS_PickRow));
        const bool wide = B(cell + 4) != 0;
        SH_CALL(Menu_DrawCursorBox)(static_cast<int>(W(cell)), static_cast<int>(W(cell + 2)), wide ? 0x7E : 0x80, wide ? 0x30 : 0x34, 0, 6);
    }
    const U cell = ::bof3::Img(field_s::at::kPickCells) + static_cast<U>(6 * (FieldS_Column + 2 * BattleE1_ModeByte9));
    const bool wide = B(cell + 4) != 0;
    SH_CALL(Menu_DrawCursorBox)(static_cast<int>(W(cell)), static_cast<int>(W(cell + 2)), wide ? 0x7E : 0x80, wide ? 0x30 : 0x34, 1, 6);
}

void PartyForm_Inject() {
    BOF3_INJECT(PartyForm_Dispatch);
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
}
