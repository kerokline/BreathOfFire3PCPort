// Moved here by tools/move_funcs.py from rest_2c.cpp, rest_2d.cpp (docs/source-layout-round.md).
#include "game/menu/master_talk_2.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/menu/master_talk_2_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2c.h"
#include "game/rest_2c_callees.h"
#include "game/rest_2d.h"
#include "game/rest_2d_callees.h"
#include "game/scenario_harness.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = master_talk_2::at;

}  // namespace

namespace {

using U = std::uint32_t;

constexpr auto& AtRest2c = move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char& BRest2c(U address) { return *AtRest2c(address); }

U LRest2c(U address) { return static_cast<U>(Long(AtRest2c(address))); }

void PutW(U address, unsigned v) { SetWord(AtRest2c(address), v); }

void PutF(unsigned char* at, float v) { std::memcpy(at, &v, sizeof v); }

U Key(const void* p) { return Low32(p); }

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

// A party id's form: id 4 is drawn and saved as 0xB unless the story byte's
// low seven bits are 5 or 0xC (Save_BuildBlock's and MasterPanel_DrawFace's
// test, the same instructions).
unsigned char FormOf(unsigned char id) {
    if (id != 4) return id;
    const unsigned story = AreaW0b_PartySet & 0x7F;
    return story == 5 || story == 0xC ? id : static_cast<unsigned char>(0xB);
}

// A master's script message: the u16 at 0x6644E8 + 2 * master (read in place
// for any master byte, as the original reads it) plus k, as a word.
unsigned MasterMessage(unsigned char master, unsigned k) {
    return static_cast<std::uint16_t>(Word(AtRest2c(::bof3::Img(rest_2c::at::kMasterMessages.v) + 2u * master)) + k);
}

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

namespace {

// The ask steps' message: the master's base + k, Field_Request 2, the step up
// (read after the call).
void Say(unsigned k) {
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(MasterMessage(AreaW0c_TailSub, k)));
    const auto step = static_cast<unsigned char>(AreaW3f_AnswerMarkD1 + 1);
    Field_Request = 2;
    AreaW3f_AnswerMarkD1 = step;
}

// The members (Field_MemberCount, ObjTrio's +0x148 record index) scanned for
// the first whose record's +0x1F (its master) is (`equal`) or is not the
// master byte: its slot, or the count when none is (signed compares).
unsigned FirstMember(unsigned char master, bool equal) {
    const int count = Field_MemberCount;
    int i = 0;
    for (; i < count; ++i) {
        const unsigned record = BRest2c(::bof3::Img(rest_2c::at::kMembers.v) + static_cast<U>(i) * rest_2c::at::kObjStride);
        const bool same = BRest2c(::bof3::Img(rest_2c::at::kRecords.v) + 0x1F + record * rest_2c::at::kRecordStride) == master;
        if (same == equal) break;
    }
    return static_cast<unsigned>(i);
}

}  // namespace

#pragma clang attribute pop

namespace {

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

const unsigned char* PickLine(U message) { return SH_CALL(Text_DrawAt)(0x1B, 0x13, 0, 0xFF, MessageText(message)); }

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x586570: a party member's face, 40 x 48, at (x, y) (low 16 bits
// as floats): id 4 as FormOf; a draw mode (0, 1, 0x1E, 0) committed; a sprite
// (Gpu_SetSprt) at the packet cursor (read again), the colour by shade (0
// grey 0x80, 1 dark 0x30, else (0x40, 0x40, 0x80)), u / v the face table's
// first two bytes (0x6644B8 + 4 id, read in place for any id), the CLUT word
// (row + 0x1E0) << 6 | x byte >> 4; committed as (1, 0x1C).
extern "C" void __cdecl MasterPanel_DrawFace(int x, int y, unsigned id, unsigned shade) {
    const unsigned char form = FormOf(static_cast<unsigned char>(id));
    const U entry = ::bof3::Img(rest_2c::at::kFaceTable.v) + 4u * form;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext.Native(), 0, 1, 0x1E, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const p = Gfx_PacketNext.Native();
    SH_CALL(Gpu_SetSprt)(p);
    const auto s = static_cast<unsigned char>(shade);
    if (s == 0) {
        p[6] = 0x80;
        p[5] = 0x80;
        p[4] = 0x80;
    } else if (s == 1) {
        p[6] = 0x30;
        p[5] = 0x30;
        p[4] = 0x30;
    } else {
        p[6] = 0x80;
        p[5] = 0x40;
        p[4] = 0x40;
    }
    PutF(p + 8, static_cast<float>(static_cast<int>(static_cast<U>(x) & 0xFFFF)));
    PutF(p + 0xC, static_cast<float>(static_cast<int>(static_cast<U>(y) & 0xFFFF)));
    p[0x14] = BRest2c(entry);
    p[0x15] = BRest2c(entry + 1);
    const unsigned clut = (((BRest2c(entry + 3) + 0x1E0u) << 6) | (BRest2c(entry + 2) >> 4)) & 0xFFFF;
    PutW(Key(p + 0x18), 0x28);
    PutW(Key(p + 0x1A), 0x30);
    PutW(Key(p + 0x16), clut);
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x586670 (FieldTail_LoadBank's step 2, a tail jump):
// `jmp [MasterTalk_States + 4 * 0x9398CF]`.
extern "C" void __cdecl MasterTalk_Dispatch(void) { Jump(::bof3::Img(MasterTalk_States_addr), 7, AreaW0c_AnswerMark, "MasterTalk_Dispatch"); }

// original 0x586680, mode 0: the master's introduction flag (Flags_Test of
// 0x904657 by the master 0x9039F5) set - mode 5; else master 9 - script
// message 0x2E, Field_Request 2, mode 6; else mode 1.
extern "C" void __cdecl MasterTalk_Begin(void) {
    if (SH_CALL(Flags_Test)(AtRest2c(::bof3::Img(rest_2c::at::kIntroFlags.v)), AreaW0c_TailSub) != 0) {
        AreaW0c_AnswerMark = 5;
        return;
    }
    if (AreaW0c_TailSub == 9) {
        SH_CALL(Msg_OpenScript)(0x2E);
        Field_Request = 2;
        AreaW0c_AnswerMark = 6;
        return;
    }
    AreaW0c_AnswerMark = 1;
}

// original 0x5866D0, mode 1: `jmp [MasterTalk_IntroSteps + 4 * 0x9398D1]`.
extern "C" void __cdecl MasterTalk_IntroStep(void) { Jump(::bof3::Img(MasterTalk_IntroSteps_addr), 2, AreaW3f_AnswerMarkD1, "MasterTalk_IntroStep"); }

// original 0x5866E0, intro step 0: Flags_Set(0x904654, master), the master's
// message (its base, the master read again), Field_Request 2, the step up.
extern "C" void __cdecl MasterTalk_IntroSay(void) {
    SH_CALL(Flags_Set)(AtRest2c(::bof3::Img(rest_2c::at::kMetFlags.v)), AreaW0c_TailSub);
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(MasterMessage(AreaW0c_TailSub, 0)));
    const auto step = static_cast<unsigned char>(AreaW3f_AnswerMarkD1 + 1);
    Field_Request = 2;
    AreaW3f_AnswerMarkD1 = step;
}

// original 0x586720, intro step 1: once Field_Request is not 2 (the message
// closed): Flags_Set(0x904657, master), mode 3, the step 0.
extern "C" void __cdecl MasterTalk_IntroWait(void) {
    if (Field_Request == 2) return;
    SH_CALL(Flags_Set)(AtRest2c(::bof3::Img(rest_2c::at::kIntroFlags.v)), AreaW0c_TailSub);
    AreaW0c_AnswerMark = 3;
    AreaW3f_AnswerMarkD1 = 0;
}

// original 0x586750, mode 2: `jmp [MasterTalk_AskSteps + 4 * 0x9398D1]`.
extern "C" void __cdecl MasterTalk_AskStep(void) { Jump(::bof3::Img(MasterTalk_AskSteps_addr), 6, AreaW3f_AnswerMarkD1, "MasterTalk_AskStep"); }

// original 0x586760, ask step 0: the master's message + 5.
extern "C" void __cdecl MasterTalk_Say5(void) { Say(5); }

// original 0x586790, ask step 1: once Field_Request is not 2, message + 6.
extern "C" void __cdecl MasterTalk_Say6(void) {
    if (Field_Request != 2) Say(6);
}

// original 0x5867D0, ask step 2: once Field_Request is not 2 - a member whose
// master is not this one: mode 3, the step 0; every member's (or none): the
// master's message + 7 (the master byte read once, before the scan).
extern "C" void __cdecl MasterTalk_CheckAllPupils(void) {
    if (Field_Request == 2) return;
    const unsigned char master = AreaW0c_TailSub;
    if (static_cast<int>(FirstMember(master, false)) < Field_MemberCount) {
        AreaW0c_AnswerMark = 3;
        AreaW3f_AnswerMarkD1 = 0;
        return;
    }
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(MasterMessage(master, 7)));
    const auto step = static_cast<unsigned char>(AreaW3f_AnswerMarkD1 + 1);
    Field_Request = 2;
    AreaW3f_AnswerMarkD1 = step;
}

// original 0x586860, ask step 3: once Field_Request is not 2, message + 8.
extern "C" void __cdecl MasterTalk_Say8(void) {
    if (Field_Request != 2) Say(8);
}

// original 0x5868A0, ask step 4: once Field_Request is not 2 - a member whose
// master is this one: mode 4, the step 0; none: message + 9.
extern "C" void __cdecl MasterTalk_CheckAnyPupil(void) {
    if (Field_Request == 2) return;
    const unsigned char master = AreaW0c_TailSub;
    if (static_cast<int>(FirstMember(master, true)) < Field_MemberCount) {
        AreaW0c_AnswerMark = 4;
        AreaW3f_AnswerMarkD1 = 0;
        return;
    }
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(MasterMessage(master, 9)));
    const auto step = static_cast<unsigned char>(AreaW3f_AnswerMarkD1 + 1);
    Field_Request = 2;
    AreaW3f_AnswerMarkD1 = step;
}

// original 0x586930, ask step 5: once Field_Request is not 2, message + 4,
// Field_Request 2, mode 6 and the step 0.
extern "C" void __cdecl MasterTalk_SayFarewell(void) {
    if (Field_Request == 2) return;
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(MasterMessage(AreaW0c_TailSub, 4)));
    Field_Request = 2;
    AreaW0c_AnswerMark = 6;
    AreaW3f_AnswerMarkD1 = 0;
}

// original 0x586970, mode 3: `jmp [MasterTalk_PickSteps + 4 * 0x9398D1]`.
extern "C" void __cdecl MasterTalk_PickStep(void) { Jump(::bof3::Img(MasterTalk_PickSteps_addr), 9, AreaW3f_AnswerMarkD1, "MasterTalk_PickStep"); }

// original 0x586980, pick step 2: R2D's 0x5869A0 (the pick) with the master's
// message + 0xF and 0.
extern "C" void __cdecl MasterTalk_PickAsk(void) {
    SH_AT(void (__cdecl*)(unsigned, unsigned), rest_2c::at::kPickAsk.v)(MasterMessage(AreaW0c_TailSub, 0xF), 0);
}

#pragma clang attribute pop

// original 0x5869A0 (0x1E2 bytes): the member pick. Input_AutoRepeat(Input_Pressed
// & 0x5000); confirm - the cursor's member taken (Sound_PlayEffect(0x104), the
// answer 0, the step on) when its record's master byte +0x1F is this master's
// exactly when `apprentices`' low byte is not 0, else refused (0x107); cancel -
// the answer 1, sounds 0x102 and 0x106, step 4; else 0x1000 / 0x4000 move the
// cursor over the members, wrapping (0x100). Then the screen drawn with the
// cursor's frame blinking and the line `message`.
extern "C" void __cdecl MasterScreen_PickMember(unsigned message, unsigned apprentices) {
    const U held = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0x5000);
    if (Field_ConfirmButtons & Input_Pressed) {
        const unsigned char master = move_script::At(RecordOf(CursorMember()))[0x1F];
        const bool same = master == AreaW0c_TailSub;
        const bool want = (apprentices & 0xFF) != 0;
        if (same == want) {
            SH_CALL(Sound_PlayEffect)(0x104);
            const unsigned char step = AreaW3f_AnswerMarkD1;
            Rest2d_Answer = 0;
            AreaW3f_AnswerMarkD1 = static_cast<unsigned char>(step + 1);
        } else {
            SH_CALL(Sound_PlayEffect)(0x107);
        }
    } else if (Field_CancelButtons & Input_Pressed) {
        Rest2d_Answer = 1;
        SH_CALL(Sound_PlayEffect)(0x102);
        SH_CALL(Sound_PlayEffect)(0x106);
        AreaW3f_AnswerMarkD1 = 4;
    } else if (held & 0x1000) {
        if (Rest2d_Cursor == 0) Rest2d_Cursor = static_cast<unsigned char>(Field_MemberCount - 1);
        else Rest2d_Cursor = static_cast<unsigned char>(Rest2d_Cursor - 1);
        SH_CALL(Sound_PlayEffect)(0x100);
    } else if (held & 0x4000) {
        if (Rest2d_Cursor == static_cast<int>(Field_MemberCount) - 1) Rest2d_Cursor = 0;
        else Rest2d_Cursor = static_cast<unsigned char>(Rest2d_Cursor + 1);
        SH_CALL(Sound_PlayEffect)(0x100);
    }
    DrawMasterScreen(message, 1, &PickLine);
}

// original 0x586B90 (0x163 bytes; PSX 0x801D1CC8): a cursor's frame - a LINE_F3 (x + 4, y),
// (x, y + 4), (x, y + h - 1) and a LINE_F4 (x + 4, y), (x + w - 1, y), (x + w -
// 1, y + h - 1), (x, y + h - 1), the four words' low halves as floats. Its
// colour: a byte c - 0xFF, or with `blink`'s low byte set ((Frame_Counter & 8 ?
// Frame_Counter : ~Frame_Counter) & 6) << 5 + 0x3F - in each of the three
// channels whose bit of `colours` is set, 0 in the others (packet bytes +6, +5,
// +4 for bits 0, 1, 2). Gfx_CommitPrim(1, 0x2C) and (1, 0x38).
extern "C" void __cdecl MasterScreen_DrawCursorFrame(unsigned x, unsigned y, unsigned w, unsigned h, unsigned blink,
                                                     unsigned colours) {
    unsigned char c = 0xFF;
    if ((blink & 0xFF) != 0) {
        const auto frame = static_cast<unsigned char>(Frame_Counter);
        const unsigned char bits = (frame & 8) ? frame : static_cast<unsigned char>(~frame);
        c = static_cast<unsigned char>(((bits & 6) << 5) + 0x3F);
    }
    unsigned char rgb[3];
    for (unsigned i = 0; i < 3; ++i) rgb[i] = (colours >> i) & 1 ? c : 0;
    const int x0 = static_cast<int>(x & 0xFFFF), y0 = static_cast<int>(y & 0xFFFF);
    const int w0 = static_cast<int>(w & 0xFFFF), h0 = static_cast<int>(h & 0xFFFF);
    auto put = [](unsigned char* p, unsigned off, int v) {
        const float f = static_cast<float>(v);
        std::memcpy(p + off, &f, 4);
    };
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF3)(p);
    p[4] = rgb[2];
    put(p, 8, x0 + 4);
    put(p, 0xC, y0);
    p[5] = rgb[1];
    p[6] = rgb[0];
    put(p, 0x14, x0);
    put(p, 0x18, y0 + 4);
    put(p, 0x20, x0);
    put(p, 0x24, y0 + h0 - 1);
    SH_CALL(Gfx_CommitPrim)(1, 0x2C);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF4)(p);
    put(p, 8, x0 + 4);
    put(p, 0x2C, x0);
    put(p, 0x30, y0 + h0 - 1);
    put(p, 0xC, y0);
    put(p, 0x18, y0);
    put(p, 0x24, y0 + h0 - 1);
    p[4] = rgb[2];
    p[5] = rgb[1];
    p[6] = rgb[0];
    put(p, 0x20, x0 + w0 - 1);
    put(p, 0x14, x0 + w0 - 1);
    SH_CALL(Gfx_CommitPrim)(1, 0x38);
}

// original 0x586D00 (0x1B bytes), step 3: the prompt with message 0x11.
extern "C" void __cdecl MasterJoin_Step3Ask(void) { SH_CALL(MasterScreen_AskYesNo)(MasterMessageRest2d(0x11)); }

void MasterTalk2_Inject() {
    BOF3_INJECT(MasterPanel_DrawFace);
    BOF3_INJECT(MasterTalk_Dispatch);
    BOF3_INJECT(MasterTalk_Begin);
    BOF3_INJECT(MasterTalk_IntroStep);
    BOF3_INJECT(MasterTalk_IntroSay);
    BOF3_INJECT(MasterTalk_IntroWait);
    BOF3_INJECT(MasterTalk_AskStep);
    BOF3_INJECT(MasterTalk_Say5);
    BOF3_INJECT(MasterTalk_Say6);
    BOF3_INJECT(MasterTalk_CheckAllPupils);
    BOF3_INJECT(MasterTalk_Say8);
    BOF3_INJECT(MasterTalk_CheckAnyPupil);
    BOF3_INJECT(MasterTalk_SayFarewell);
    BOF3_INJECT(MasterTalk_PickStep);
    BOF3_INJECT(MasterTalk_PickAsk);
    BOF3_INJECT(MasterScreen_PickMember);
    BOF3_INJECT(MasterScreen_DrawCursorFrame);
    BOF3_INJECT(MasterJoin_Step3Ask);
}
