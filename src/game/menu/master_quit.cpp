// Moved here by tools/move_funcs.py from rest_2d.cpp (docs/source-layout-round.md).
#include "game/menu/master_quit.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field/field_idle.h"   // DIV-0223: CampTalk
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2d.h"
#include "game/rest_2d_callees.h"
#include "game/scenario_harness.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_2d::at;

}  // namespace

namespace {

using U = std::uint32_t;

// move_script::At is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::Long;

using move_script::SetLong;

using move_script::Word;

unsigned char& BRest2d(U a) { return move_script::At(a)[0]; }

// A state table's handler, jumped to as the original's `jmp [table + 4 *
// index]` does: the index must lie inside the table's own count (its reader's
// reach, docs/rest_2d.md section 3); past it the original jumps through what
// follows the table, which no handler of the screen writes - ours aborts. The
// entry is called as the cell holds it: Capcom's address in the game (Inject's
// jmp to ours where it is ours), a recorder while the fuzz runs.
using Handler = void (__cdecl*)();

void Jump(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: step %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(move_script::At(table + 4 * index)))))();
}

// A record the original writes by an index it never bounds. Past the array it
// writes into the next block; no path in play gives such an index (section 5),
// so ours aborts there.
U RecordChecked(U base, U stride, unsigned count, unsigned index, const char* array, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: %s index %u is past its %u records; the original writes at 0x%X", who, array, index, count,
                    static_cast<unsigned>(::bof3::VaOf(base + index * stride)));
    return base + index * stride;
}

// --- callees by address (R2C's and R2E's, ours; named in rest_2d_callees.h) ----------------
void MemberPanel(U x, U y, U member, U row) {
    SH_AT(void (__cdecl*)(U, U, U, U), rest_2d::at::kMemberPanel.v)(x, y, member, row);
}

void MemberLabel(U x, U y, U member) { SH_AT(void (__cdecl*)(U, U, U), rest_2d::at::kMemberLabel.v)(x, y, member); }

void PromptBox(U x, U y, U w, U h, U style) { SH_AT(void (__cdecl*)(U, U, U, U, U), rest_2d::at::kPromptBox.v)(x, y, w, h, style); }

// The master's message: word [0x6644E8 + 2 * master] + k (the master byte
// unbounded: the table is read in place).
unsigned short MasterMessageRest2d(unsigned k) {
    return static_cast<unsigned short>(Word(move_script::At(::bof3::Img(rest_2d::at::kMasterMessages.v) + 2u * AreaW0c_TailSub)) + k);
}

// Field_Members' byte of the party slot `slot` (ObjTrio + 0x148, 0x14C apart).
unsigned char MemberAt(unsigned slot) { return (rv::Bytes(ObjTrio) + 0x148)[slot * rest_2d::at::kObjStride]; }

// The member under the cursor: a CharacterRecords index.
unsigned char CursorMember() { return MemberAt(Rest2d_Cursor); }

U RecordOf(unsigned member) { return ::bof3::Img(CharacterRecords_addr) + member * rest_2d::at::kRecordStride; }

// The name (the record's first nine bytes) into Text_Records' first slot.
void NameToText(U record) {
    unsigned char* const text = move_script::At(::bof3::Img(bof3::addr::Text_Records));
    std::memcpy(text, move_script::At(record), 8);
    text[8] = move_script::At(record)[8];
}

// The pick's and the yes / no prompt's frame: each member's panel and label,
// the cursor's frame over the cursor's member (blinking when `blink`), the
// prompt's box, and the line - message `message` of MessagePools.
void DrawMasterScreen(U message, unsigned blink, const unsigned char* (*line)(U)) {
    for (unsigned i = 0; i < Field_MemberCount; ++i) {
        const U y = 0x3E + 54 * i;
        MemberPanel(0x11, y, MemberAt(i), i);
        MemberLabel(0x89, y, MemberAt(i));
    }
    SH_CALL(MasterScreen_DrawCursorFrame)(0x11, 0x3E + 54u * Rest2d_Cursor, 0x110, 0x34, blink, 6);
    PromptBox(0x14, 0x10, 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour));
    line(message);
}

const unsigned char* MessageText(U message) {
    const unsigned word = Word(move_script::At(::bof3::Img(rest_2d::at::kMessagePools.v) + 2 * (message & 0xFFFF)));
    return move_script::At(::bof3::Img(rest_2d::at::kMessagePools.v) + word);
}

// DIV-0027's two sites in Capcom's body of MasterScreen_AskYesNo: the line
// (Text_DrawAt, 0x586E78) and the hand (Menu_DrawHand, 0x586E97), re-aimed by
// YesNoLayout_Inject under a Latin overlay. Ours calls where each reaches now,
// as yes_no_layout.cpp holds it (docs/operand-store.md).
const unsigned char* AskLine(U message) {
    const U target = YesNoLayout_MasterLineDraw();
    using Fn = const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*);
    if (target == bof3::addr::Text_DrawAt) return SH_CALL(Text_DrawAt)(0x1B, 0x13, 0, 0xFF, MessageText(message));
    return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(::bof3::CodePlace(target)))(0x1B, 0x13, 0, 0xFF, MessageText(message));
}

void AskHand(U x) {
    const U target = YesNoLayout_MasterHandDraw();
    using Fn = void (__cdecl*)(int, int, int);
    if (target == bof3::addr::Menu_DrawHand) {
        SH_CALL(Menu_DrawHand)(static_cast<int>(x), 0x15, 0);
        return;
    }
    reinterpret_cast<Fn>(static_cast<std::uintptr_t>(::bof3::CodePlace(target)))(static_cast<int>(x), 0x15, 0);
}

// The state handlers' wait-then-message shape: once no message is open
// (Field_Request not 2), message `k` of the master's set, the step on, and
// Field_Request 2.
void TellAndStep(unsigned k) {
    if (Field_Request == 2) return;
    SH_CALL(Msg_OpenScript)(MasterMessageRest2d(k));
    const unsigned char step = AreaW3f_AnswerMarkD1;
    Field_Request = 2;
    AreaW3f_AnswerMarkD1 = static_cast<unsigned char>(step + 1);
}

// ... and the closing one: message 4, and state 6 (the screen leaves).
void TellAndLeave() {
    if (Field_Request == 2) return;
    SH_CALL(Msg_OpenScript)(MasterMessageRest2d(4));
    Field_Request = 2;
    AreaW0c_AnswerMark = 6;
}

}  // namespace

// original 0x586D20 (0x181 bytes): the yes / no prompt. Input_AutoRepeat(
// Input_Pressed & 0xA000); confirm - on yes (the answer 0) Sound_PlayEffect(0x102)
// and the step on, on no the step back, then 0x104; cancel - the answer 1, the
// step back, 0x106; else either direction flips the answer (0x100). The screen
// drawn with the cursor's frame still, the line `message` and the hand at
// 0xCF + 36 * the answer, y 0x15 - both through DIV-0027's sites.
extern "C" void __cdecl MasterScreen_AskYesNo(unsigned message) {
    const U held = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xA000);
    if (Field_ConfirmButtons & Input_Pressed) {
        unsigned char step;
        if (Rest2d_Answer != 0) {
            step = static_cast<unsigned char>(AreaW3f_AnswerMarkD1 - 1);
        } else {
            SH_CALL(Sound_PlayEffect)(0x102);
            step = static_cast<unsigned char>(AreaW3f_AnswerMarkD1 + 1);
        }
        AreaW3f_AnswerMarkD1 = step;
        SH_CALL(Sound_PlayEffect)(0x104);
    } else if (Field_CancelButtons & Input_Pressed) {
        const auto step = static_cast<unsigned char>(AreaW3f_AnswerMarkD1 - 1);
        Rest2d_Answer = 1;
        AreaW3f_AnswerMarkD1 = step;
        SH_CALL(Sound_PlayEffect)(0x106);
    } else if (held & 0xFFFF) {
        SH_CALL(Sound_PlayEffect)(0x100);
        Rest2d_Answer = static_cast<unsigned char>(Rest2d_Answer ^ 1);
    }
    DrawMasterScreen(message, 0, &AskLine);
    AskHand(0xCF + 36u * Rest2d_Answer);
}

// original 0x586EB0 (0x159 bytes), step 5: on no, message 4, step 0, state 6; on
// yes the cursor's member's name into Text_Records, its record's master byte
// +0x1F = the master, +0x88 = its level +0xA, +0x89..+0x8E = the master's six
// bytes of 0x664370; message 0xA, the step on.
extern "C" void __cdecl MasterJoin_Step5Apply(void) {
    if (Rest2d_Answer != 0) {
        SH_CALL(Msg_OpenScript)(MasterMessageRest2d(4));
        Field_Request = 2;
        AreaW0c_AnswerMark = 6;
        AreaW3f_AnswerMarkD1 = 0;
        return;
    }
    const U record = RecordChecked(::bof3::Img(CharacterRecords_addr), rest_2d::at::kRecordStride, 8, CursorMember(), "CharacterRecords",
                                   "MasterJoin_Step5Apply");
    unsigned char* const r = move_script::At(record);
    unsigned char* const text = move_script::At(::bof3::Img(bof3::addr::Text_Records));
    std::memcpy(text, r, 8);
    const unsigned char name8 = r[8];
    r[0x1F] = AreaW0c_TailSub;
    text[8] = name8;
    r[0x88] = r[0xA];
    for (unsigned k = 0; k < 6; ++k) r[0x89 + k] = BRest2d(::bof3::Img(rest_2d::at::kMasterStats.v) + 6u * AreaW0c_TailSub + k);
    SH_CALL(Msg_OpenScript)(MasterMessageRest2d(0xA));
    const unsigned char step = AreaW3f_AnswerMarkD1;
    Field_Request = 2;
    AreaW3f_AnswerMarkD1 = static_cast<unsigned char>(step + 1);
}

// original 0x587010 (0x39 bytes), step 6: once no message is open, message 0xB,
// the step on.
extern "C" void __cdecl MasterJoin_Step6Told(void) { TellAndStep(0xB); }

// original 0x587050 (0x81 bytes), step 7: once no message is open - every member
// the master's apprentice (from the first, stopping at one who is not):
// message 7, the step on; else step 0.
extern "C" void __cdecl MasterJoin_Step7AllCheck(void) {
    if (Field_Request == 2) return;
    const unsigned char master = AreaW0c_TailSub;
    const int n = Field_MemberCount;
    int i = 0;
    while (i < n && move_script::At(RecordOf(MemberAt(static_cast<unsigned>(i))))[0x1F] == master) ++i;
    if (i < n) {
        AreaW3f_AnswerMarkD1 = 0;
        return;
    }
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(move_script::At(::bof3::Img(rest_2d::at::kMasterMessages.v) + 2u * master)) + 7));
    const unsigned char step = AreaW3f_AnswerMarkD1;
    Field_Request = 2;
    AreaW3f_AnswerMarkD1 = static_cast<unsigned char>(step + 1);
}

// original 0x5870E0 (0x34 bytes), step 8: once no message is open, message 4 and
// state 6.
extern "C" void __cdecl MasterJoin_Step8Close(void) { TellAndLeave(); }

// original 0x587120 (0xE bytes), 0x66450C[4]: jmp [MasterQuit_Steps + 4 * the
// step], unchecked.
extern "C" void __cdecl MasterQuit_ByStep(void) { Jump(::bof3::Img(MasterQuit_Steps_addr), rest_2d::at::kQuitStepCount, AreaW3f_AnswerMarkD1, "MasterQuit_ByStep"); }

// original 0x587130 (0x1F bytes), step 2: the pick of an apprentice, message 0x10.
extern "C" void __cdecl MasterQuit_Step2Pick(void) { SH_CALL(MasterScreen_PickMember)(MasterMessageRest2d(0x10), 1); }

// original 0x587150 (0x1B bytes), step 3: the prompt, message 0x11.
extern "C" void __cdecl MasterQuit_Step3Ask(void) { SH_CALL(MasterScreen_AskYesNo)(MasterMessageRest2d(0x11)); }

// original 0x587170 (0xE6 bytes), step 5: on no, message 4, step 0, state 6; on
// yes the cursor's member's name into Text_Records, its record's master byte
// 0xFF and +0x88..+0x8E 0; message 0xC, the step on.
extern "C" void __cdecl MasterQuit_Step5Apply(void) {
    if (Rest2d_Answer != 0) {
        SH_CALL(Msg_OpenScript)(MasterMessageRest2d(4));
        AreaW3f_AnswerMarkD1 = 0;
        Field_Request = 2;
        AreaW0c_AnswerMark = 6;
        return;
    }
    const U record = RecordChecked(::bof3::Img(CharacterRecords_addr), rest_2d::at::kRecordStride, 8, CursorMember(), "CharacterRecords",
                                   "MasterQuit_Step5Apply");
    unsigned char* const r = move_script::At(record);
    unsigned char* const text = move_script::At(::bof3::Img(bof3::addr::Text_Records));
    std::memcpy(text, r, 8);
    const unsigned char name8 = r[8];
    r[0x1F] = 0xFF;
    for (unsigned k = 0; k < 7; ++k) r[0x88 + k] = 0;
    text[8] = name8;
    SH_CALL(Msg_OpenScript)(MasterMessageRest2d(0xC));
    const unsigned char step = AreaW3f_AnswerMarkD1;
    Field_Request = 2;
    AreaW3f_AnswerMarkD1 = static_cast<unsigned char>(step + 1);
}

// original 0x587260 (0x39 bytes), step 6: once no message is open, message 0xD,
// the step on.
extern "C" void __cdecl MasterQuit_Step6Told(void) { TellAndStep(0xD); }

// original 0x5872A0 (0x81 bytes), step 7: once no message is open - some member
// still the master's apprentice: step 0; none: message 9, the step on.
extern "C" void __cdecl MasterQuit_Step7NoneLeftCheck(void) {
    if (Field_Request == 2) return;
    const unsigned char master = AreaW0c_TailSub;
    const int n = Field_MemberCount;
    int i = 0;
    while (i < n && move_script::At(RecordOf(MemberAt(static_cast<unsigned>(i))))[0x1F] != master) ++i;
    if (i < n) {
        AreaW3f_AnswerMarkD1 = 0;
        return;
    }
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(move_script::At(::bof3::Img(rest_2d::at::kMasterMessages.v) + 2u * master)) + 9));
    const unsigned char step = AreaW3f_AnswerMarkD1;
    Field_Request = 2;
    AreaW3f_AnswerMarkD1 = static_cast<unsigned char>(step + 1);
}

// original 0x587330 (0x34 bytes), step 8: once no message is open, message 4 and
// state 6.
extern "C" void __cdecl MasterQuit_Step8Close(void) { TellAndLeave(); }

// original 0x587370 (0xE bytes), 0x66450C[5]: jmp [MasterGrant_Steps + 4 * the
// step], unchecked.
extern "C" void __cdecl MasterGrant_ByStep(void) { Jump(::bof3::Img(MasterGrant_Steps_addr), rest_2d::at::kGrantStepCount, AreaW3f_AnswerMarkD1, "MasterGrant_ByStep"); }

// original 0x587380 (0x37 bytes), step 0: message 5, the step on, the cursor 0.
extern "C" void __cdecl MasterGrant_Step0Open(void) {
    SH_CALL(Msg_OpenScript)(MasterMessageRest2d(5));
    const unsigned char step = AreaW3f_AnswerMarkD1;
    Field_Request = 2;
    Rest2d_Cursor = 0;
    AreaW3f_AnswerMarkD1 = static_cast<unsigned char>(step + 1);
}

// original 0x5873C0 (0x2B4 bytes with its four-case table at +0x2A4), step 1:
// once no message is open, for the cursor's member when it is this master's
// apprentice, the levels gained g = level +0xA - the level at joining +0x88 (a
// byte). Masters 0xB, 0xD, 0xE: at g >= 3, once each (bits 3, 4, 5 of
// 0x904061): the bit set, the name into Text_Records, message 0xE. Master 0xC:
// at g >= 3 and story flag 0x6C clear, Inventory_Add(3, 0x16, 1) - when it
// takes, the flag set, the name, message 0xE. Any other: the first of the
// master's six (level, ability) pairs of 0x6642A4 with level <= g whose bit of
// 0x904088 is clear and that AbilityList_Add(ability, member, 0, 0) takes -
// the name, the ability's record's first 16 bytes into Text_Records + 0x20,
// the bit set, message 0xE. The step on in every case.
extern "C" void __cdecl MasterGrant_Step1Member(void) {
    if (Field_Request == 2) return;
    const unsigned char member = CursorMember();
    const U record = RecordOf(member);
    const unsigned char master = AreaW0c_TailSub;
    bool told = false;
    if (move_script::At(record)[0x1F] == master) {
        const auto gained = static_cast<unsigned char>(move_script::At(record)[0xA] - move_script::At(record)[0x88]);
        const unsigned which = static_cast<unsigned>(master) - 0xB;
        if (which <= 3) {
            if (gained >= 3) {
                if (which == 1) {
                    if (SH_CALL(Flags_Test)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x6C) == 0 && SH_CALL(Inventory_Add)(3, 0x16, 1) != 0) {
                        SH_CALL(Flags_Set)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x6C);
                        SH_CALL(MasterScreen_NameToText)(CursorMember());
                        told = true;
                    }
                } else {
                    const unsigned char bit = which == 0 ? 8 : which == 2 ? 0x10 : 0x20;
                    if ((Cond_Flags[0xd1] & bit) == 0) {
                        Cond_Flags[0xd1] = static_cast<unsigned char>(Cond_Flags[0xd1] | bit);
                        SH_CALL(MasterScreen_NameToText)(member);
                        told = true;
                    }
                }
            }
        } else {
            for (unsigned i = 0; i < 6; ++i) {
                const U pair = ::bof3::Img(rest_2d::at::kMasterSkills.v) + 2 * (6u * AreaW0c_TailSub + i);
                if (BRest2d(pair) > gained) continue;
                const unsigned char ability = BRest2d(pair + 1);
                const U bit = 1u << (ability & 0x1F);
                if (static_cast<U>(Long(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xf8 + 4u * (ability >> 5)))) & bit) continue;
                if (SH_CALL(AbilityList_Add)(ability, CursorMember(), 0, 0) == 0) continue;
                SH_CALL(MasterScreen_NameToText)(CursorMember());
                const unsigned char taught = BRest2d(::bof3::Img(rest_2d::at::kMasterSkills.v) + 2 * (6u * AreaW0c_TailSub + i) + 1);
                const U from = ::bof3::Img(rest_2d::at::kAbilityRecords.v) + 0x18u * taught;
                unsigned char* const text = move_script::At(::bof3::Img(bof3::addr::Text_Records) + 0x20);
                std::memcpy(text, move_script::At(from), 16);
                unsigned char* const word = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xf8 + 4u * (taught >> 5));
                SetLong(word, static_cast<std::int32_t>(static_cast<U>(Long(word)) | (1u << (taught & 0x1F))));
                told = true;
                break;
            }
        }
    }
    if (told) {
        SH_CALL(Msg_OpenScript)(MasterMessageRest2d(0xE));
        Field_Request = 2;
    }
    AreaW3f_AnswerMarkD1 = static_cast<unsigned char>(AreaW3f_AnswerMarkD1 + 1);
}

// original 0x587680 (0x35 bytes): CharacterRecords[member & 0xFF]'s first nine
// bytes (the name) into Text_Records.
extern "C" void __cdecl MasterScreen_NameToText(unsigned member) { NameToText(RecordOf(member & 0xFF)); }

// original 0x5876C0 (0x2F bytes), step 2: once no message is open, the cursor
// on; past the last member state 2; step 1.
extern "C" void __cdecl MasterGrant_Step2Next(void) {
    if (Field_Request == 2) return;
    const auto cursor = static_cast<unsigned char>(Rest2d_Cursor + 1);
    Rest2d_Cursor = cursor;
    if (cursor >= Field_MemberCount) AreaW0c_AnswerMark = 2;
    AreaW3f_AnswerMarkD1 = 1;
}

// original 0x5876F0 (0x43 bytes), 0x66450C[6]: once no message is open, the
// script's sprite (Sprite_Objects by the byte 0x90384B) given the state byte
// 0x66459C[master] unless that is 0xFF; the screen's state 0 and the mode
// tail's step on.
//
// DIV-0223 CampMasters: a talk from the master list skips the pose. The byte
// names the master's own sprite only in the master's area (its script sets
// it); from the list the talk runs elsewhere, where the byte is whatever the
// standing area left in that script variable - another object, or past the
// 30 records. The Definitive Edition romhack leaves this state as it is (its
// SISYOU.EMI is the release's), so on its disc the pose lands on that other
// object; ours does not (docs/camp-masters.md 3).
extern "C" void __cdecl MasterScreen_State6Leave(void) {
    if (Field_Request == 2) return;
    const unsigned char pose = BRest2d(::bof3::Img(rest_2d::at::kMasterLeavePose.v) + AreaW0c_TailSub);
    if (pose != 0xFF && !field_idle::CampTalk()) {
        const U sprite = RecordChecked(::bof3::Img(Sprite_Objects_addr), 0xA4, 30, BRest2d(::bof3::Img(rest_2d::at::kScriptObject.v)), "Sprite_Objects",
                                       "MasterScreen_State6Leave");
        BRest2d(sprite + 1) = pose;
    }
    const unsigned char step = static_cast<unsigned char>(AreaW0c_TailState);
    AreaW0c_AnswerMark = 0;
    AreaW0c_TailState = static_cast<unsigned char>(step + 1);
}

void MasterQuit_Inject() {
    BOF3_INJECT(MasterScreen_AskYesNo);
    BOF3_INJECT(MasterJoin_Step5Apply);
    BOF3_INJECT(MasterJoin_Step6Told);
    BOF3_INJECT(MasterJoin_Step7AllCheck);
    BOF3_INJECT(MasterJoin_Step8Close);
    BOF3_INJECT(MasterQuit_ByStep);
    BOF3_INJECT(MasterQuit_Step2Pick);
    BOF3_INJECT(MasterQuit_Step3Ask);
    BOF3_INJECT(MasterQuit_Step5Apply);
    BOF3_INJECT(MasterQuit_Step6Told);
    BOF3_INJECT(MasterQuit_Step7NoneLeftCheck);
    BOF3_INJECT(MasterQuit_Step8Close);
    BOF3_INJECT(MasterGrant_ByStep);
    BOF3_INJECT(MasterGrant_Step0Open);
    BOF3_INJECT(MasterGrant_Step1Member);
    BOF3_INJECT(MasterScreen_NameToText);
    BOF3_INJECT(MasterGrant_Step2Next);
    BOF3_INJECT(MasterScreen_State6Leave);
}
