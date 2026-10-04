// The masters' screen and three field-menu screens - round fourteen, wave two,
// group R2D: the 51 functions 0x5869A0..0x58B1CD of the cut
// (analysis/round14_cut.tsv), each read with capstone to its last instruction
// (docs/rest_2d.md section 1).
//
// Every call out goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a table's handler is called by the address the table holds. No divergence:
// each is a faithful replacement. The one patch inside the band is DIV-0027's
// (yes_no_layout.cpp): two call sites of the masters' yes / no prompt
// (0x586E78, 0x586E97) re-aimed under a Latin overlay; MasterScreen_AskYesNo
// reads where each site reaches now and calls that, so the patch, its
// BOF3X_ORIGINAL switch and the language are honoured as they were in Capcom's
// body. Where the original jumps through a table past its own count, or
// writes past a record array by a byte no path in play makes that large, ours
// aborts with a message (section 5 of the doc); reads past them are made in
// place, as the original makes them.
#include "game/rest_2d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2d_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_2d::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U a) { return At(a)[0]; }

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
                    static_cast<unsigned>(table), static_cast<unsigned>(table + 4 * index));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))))();
}

// A record the original writes by an index it never bounds. Past the array it
// writes into the next block; no path in play gives such an index (section 5),
// so ours aborts there.
U RecordChecked(U base, U stride, unsigned count, unsigned index, const char* array, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: %s index %u is past its %u records; the original writes at 0x%X", who, array, index, count,
                    static_cast<unsigned>(base + index * stride));
    return base + index * stride;
}
U Window(unsigned index, const char* who) {
    return RecordChecked(at::kWindows, at::kWindowStride, at::kWindowCount, index, "WindowRecords", who);
}

// --- callees by address (R2C's and R2E's, raw until the round's rebinding) -----------------
void MemberPanel(U x, U y, U member, U row) {
    SH_AT(void (__cdecl*)(U, U, U, U), at::kMemberPanel)(x, y, member, row);
}
void MemberLabel(U x, U y, U member) { SH_AT(void (__cdecl*)(U, U, U), at::kMemberLabel)(x, y, member); }
void PromptBox(U x, U y, U w, U h, U style) { SH_AT(void (__cdecl*)(U, U, U, U, U), at::kPromptBox)(x, y, w, h, style); }

// The master's message: word [0x6644E8 + 2 * master] + k (the master byte
// unbounded: the table is read in place).
unsigned short MasterMessage(unsigned k) {
    return static_cast<unsigned short>(Word(At(at::kMasterMessages + 2u * B(at::kMaster))) + k);
}
// Field_Members' byte of the party slot `slot` (ObjTrio + 0x148, 0x14C apart).
unsigned char MemberAt(unsigned slot) { return Field_Members[slot * at::kObjStride]; }
// The member under the cursor: a CharacterRecords index.
unsigned char CursorMember() { return MemberAt(B(at::kCursor)); }
U RecordOf(unsigned member) { return at::kRecords + member * at::kRecordStride; }

// The name (the record's first nine bytes) into Text_Records' first slot.
void NameToText(U record) {
    unsigned char* const text = At(bof3::addr::Text_Records);
    std::memcpy(text, At(record), 8);
    text[8] = At(record)[8];
}

// --- the masters' screen's shared draw -------------------------------------------------------

// The pick's and the yes / no prompt's frame: each member's panel and label,
// the cursor's frame over the cursor's member (blinking when `blink`), the
// prompt's box, and the line - message `message` of MessagePools.
void DrawMasterScreen(U message, unsigned blink, const unsigned char* (*line)(U)) {
    for (unsigned i = 0; i < Field_MemberCount; ++i) {
        const U y = 0x3E + 54 * i;
        MemberPanel(0x11, y, MemberAt(i), i);
        MemberLabel(0x89, y, MemberAt(i));
    }
    SH_CALL(MasterScreen_DrawCursorFrame)(0x11, 0x3E + 54u * B(at::kCursor), 0x110, 0x34, blink, 6);
    PromptBox(0x14, 0x10, 0x118, 0x13, B(at::kStyle));
    line(message);
}

const unsigned char* MessageText(U message) {
    const unsigned word = Word(At(at::kMessagePools + 2 * (message & 0xFFFF)));
    return At(at::kMessagePools + word);
}

const unsigned char* PickLine(U message) { return SH_CALL(Text_DrawAt)(0x1B, 0x13, 0, 0xFF, MessageText(message)); }

// DIV-0027's two sites in Capcom's body of MasterScreen_AskYesNo: the line
// (Text_DrawAt) and the hand (Menu_DrawHand), re-aimed by YesNoLayout_Inject
// under a Latin overlay. Ours calls where each reaches now.
constexpr U kAskLineSite = 0x586E78;
constexpr U kAskHandSite = 0x586E97;
U SiteTarget(U site) {
    const unsigned char* const p = At(site);
    if (p[0] != 0xE8) bof3::Fatal("MasterScreen_AskYesNo: 0x%X is not a call (DIV-0027's site)", static_cast<unsigned>(site));
    return site + 5 + static_cast<U>(Long(p + 1));
}
const unsigned char* AskLine(U message) {
    const U target = SiteTarget(kAskLineSite);
    using Fn = const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*);
    if (target == bof3::addr::Text_DrawAt) return SH_CALL(Text_DrawAt)(0x1B, 0x13, 0, 0xFF, MessageText(message));
    return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(target))(0x1B, 0x13, 0, 0xFF, MessageText(message));
}
void AskHand(U x) {
    const U target = SiteTarget(kAskHandSite);
    using Fn = void (__cdecl*)(int, int, int);
    if (target == bof3::addr::Menu_DrawHand) {
        SH_CALL(Menu_DrawHand)(static_cast<int>(x), 0x15, 0);
        return;
    }
    reinterpret_cast<Fn>(static_cast<std::uintptr_t>(target))(static_cast<int>(x), 0x15, 0);
}

// The state handlers' wait-then-message shape: once no message is open
// (Field_Request not 2), message `k` of the master's set, the step on, and
// Field_Request 2.
void TellAndStep(unsigned k) {
    if (Field_Request == 2) return;
    SH_CALL(Msg_OpenScript)(MasterMessage(k));
    const unsigned char step = B(at::kStep);
    Field_Request = 2;
    B(at::kStep) = static_cast<unsigned char>(step + 1);
}
// ... and the closing one: message 4, and state 6 (the screen leaves).
void TellAndLeave() {
    if (Field_Request == 2) return;
    SH_CALL(Msg_OpenScript)(MasterMessage(4));
    Field_Request = 2;
    B(at::kState) = 6;
}

}  // namespace

// ============================================================================================
// The masters' screen
// ============================================================================================

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
        const unsigned char master = At(RecordOf(CursorMember()))[0x1F];
        const bool same = master == B(at::kMaster);
        const bool want = (apprentices & 0xFF) != 0;
        if (same == want) {
            SH_CALL(Sound_PlayEffect)(0x104);
            const unsigned char step = B(at::kStep);
            B(at::kAnswer) = 0;
            B(at::kStep) = static_cast<unsigned char>(step + 1);
        } else {
            SH_CALL(Sound_PlayEffect)(0x107);
        }
    } else if (Field_CancelButtons & Input_Pressed) {
        B(at::kAnswer) = 1;
        SH_CALL(Sound_PlayEffect)(0x102);
        SH_CALL(Sound_PlayEffect)(0x106);
        B(at::kStep) = 4;
    } else if (held & 0x1000) {
        if (B(at::kCursor) == 0) B(at::kCursor) = static_cast<unsigned char>(Field_MemberCount - 1);
        else B(at::kCursor) = static_cast<unsigned char>(B(at::kCursor) - 1);
        SH_CALL(Sound_PlayEffect)(0x100);
    } else if (held & 0x4000) {
        if (static_cast<int>(B(at::kCursor)) == static_cast<int>(Field_MemberCount) - 1) B(at::kCursor) = 0;
        else B(at::kCursor) = static_cast<unsigned char>(B(at::kCursor) + 1);
        SH_CALL(Sound_PlayEffect)(0x100);
    }
    DrawMasterScreen(message, 1, &PickLine);
}

// original 0x586B90 (0x163 bytes): a cursor's frame - a LINE_F3 (x + 4, y),
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
        if (B(at::kAnswer) != 0) {
            step = static_cast<unsigned char>(B(at::kStep) - 1);
        } else {
            SH_CALL(Sound_PlayEffect)(0x102);
            step = static_cast<unsigned char>(B(at::kStep) + 1);
        }
        B(at::kStep) = step;
        SH_CALL(Sound_PlayEffect)(0x104);
    } else if (Field_CancelButtons & Input_Pressed) {
        const auto step = static_cast<unsigned char>(B(at::kStep) - 1);
        B(at::kAnswer) = 1;
        B(at::kStep) = step;
        SH_CALL(Sound_PlayEffect)(0x106);
    } else if (held & 0xFFFF) {
        SH_CALL(Sound_PlayEffect)(0x100);
        B(at::kAnswer) = static_cast<unsigned char>(B(at::kAnswer) ^ 1);
    }
    DrawMasterScreen(message, 0, &AskLine);
    AskHand(0xCF + 36u * B(at::kAnswer));
}

// original 0x587680 (0x35 bytes): CharacterRecords[member & 0xFF]'s first nine
// bytes (the name) into Text_Records.
extern "C" void __cdecl MasterScreen_NameToText(unsigned member) { NameToText(RecordOf(member & 0xFF)); }

// original 0x5876F0 (0x43 bytes), 0x66450C[6]: once no message is open, the
// script's sprite (Sprite_Objects by the byte 0x90384B) given the state byte
// 0x66459C[master] unless that is 0xFF; the screen's state 0 and the mode
// tail's step on.
extern "C" void __cdecl MasterScreen_State6Leave(void) {
    if (Field_Request == 2) return;
    const unsigned char pose = B(at::kMasterLeavePose + B(at::kMaster));
    if (pose != 0xFF) {
        const U sprite = RecordChecked(at::kSprites, 0xA4, 30, B(at::kScriptObject), "Sprite_Objects",
                                       "MasterScreen_State6Leave");
        B(sprite + 1) = pose;
    }
    const unsigned char step = B(at::kTailStep);
    B(at::kState) = 0;
    B(at::kTailStep) = static_cast<unsigned char>(step + 1);
}

// --- state 3: a member made the master's apprentice (R2C's step table 0x664548) ---------------

// original 0x586D00 (0x1B bytes), step 3: the prompt with message 0x11.
extern "C" void __cdecl MasterJoin_Step3Ask(void) { SH_CALL(MasterScreen_AskYesNo)(MasterMessage(0x11)); }

// original 0x586EB0 (0x159 bytes), step 5: on no, message 4, step 0, state 6; on
// yes the cursor's member's name into Text_Records, its record's master byte
// +0x1F = the master, +0x88 = its level +0xA, +0x89..+0x8E = the master's six
// bytes of 0x664370; message 0xA, the step on.
extern "C" void __cdecl MasterJoin_Step5Apply(void) {
    if (B(at::kAnswer) != 0) {
        SH_CALL(Msg_OpenScript)(MasterMessage(4));
        Field_Request = 2;
        B(at::kState) = 6;
        B(at::kStep) = 0;
        return;
    }
    const U record = RecordChecked(at::kRecords, at::kRecordStride, 8, CursorMember(), "CharacterRecords",
                                   "MasterJoin_Step5Apply");
    unsigned char* const r = At(record);
    unsigned char* const text = At(bof3::addr::Text_Records);
    std::memcpy(text, r, 8);
    const unsigned char name8 = r[8];
    r[0x1F] = B(at::kMaster);
    text[8] = name8;
    r[0x88] = r[0xA];
    for (unsigned k = 0; k < 6; ++k) r[0x89 + k] = B(at::kMasterStats + 6u * B(at::kMaster) + k);
    SH_CALL(Msg_OpenScript)(MasterMessage(0xA));
    const unsigned char step = B(at::kStep);
    Field_Request = 2;
    B(at::kStep) = static_cast<unsigned char>(step + 1);
}

// original 0x587010 (0x39 bytes), step 6: once no message is open, message 0xB,
// the step on.
extern "C" void __cdecl MasterJoin_Step6Told(void) { TellAndStep(0xB); }

// original 0x587050 (0x81 bytes), step 7: once no message is open - every member
// the master's apprentice (from the first, stopping at one who is not):
// message 7, the step on; else step 0.
extern "C" void __cdecl MasterJoin_Step7AllCheck(void) {
    if (Field_Request == 2) return;
    const unsigned char master = B(at::kMaster);
    const int n = Field_MemberCount;
    int i = 0;
    while (i < n && At(RecordOf(MemberAt(static_cast<unsigned>(i))))[0x1F] == master) ++i;
    if (i < n) {
        B(at::kStep) = 0;
        return;
    }
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(At(at::kMasterMessages + 2u * master)) + 7));
    const unsigned char step = B(at::kStep);
    Field_Request = 2;
    B(at::kStep) = static_cast<unsigned char>(step + 1);
}

// original 0x5870E0 (0x34 bytes), step 8: once no message is open, message 4 and
// state 6.
extern "C" void __cdecl MasterJoin_Step8Close(void) { TellAndLeave(); }

// --- state 4: an apprentice released (MasterQuit_Steps) -----------------------------------------

// original 0x587120 (0xE bytes), 0x66450C[4]: jmp [MasterQuit_Steps + 4 * the
// step], unchecked.
extern "C" void __cdecl MasterQuit_ByStep(void) { Jump(at::kQuitSteps, at::kQuitStepCount, B(at::kStep), "MasterQuit_ByStep"); }

// original 0x587130 (0x1F bytes), step 2: the pick of an apprentice, message 0x10.
extern "C" void __cdecl MasterQuit_Step2Pick(void) { SH_CALL(MasterScreen_PickMember)(MasterMessage(0x10), 1); }

// original 0x587150 (0x1B bytes), step 3: the prompt, message 0x11.
extern "C" void __cdecl MasterQuit_Step3Ask(void) { SH_CALL(MasterScreen_AskYesNo)(MasterMessage(0x11)); }

// original 0x587170 (0xE6 bytes), step 5: on no, message 4, step 0, state 6; on
// yes the cursor's member's name into Text_Records, its record's master byte
// 0xFF and +0x88..+0x8E 0; message 0xC, the step on.
extern "C" void __cdecl MasterQuit_Step5Apply(void) {
    if (B(at::kAnswer) != 0) {
        SH_CALL(Msg_OpenScript)(MasterMessage(4));
        B(at::kStep) = 0;
        Field_Request = 2;
        B(at::kState) = 6;
        return;
    }
    const U record = RecordChecked(at::kRecords, at::kRecordStride, 8, CursorMember(), "CharacterRecords",
                                   "MasterQuit_Step5Apply");
    unsigned char* const r = At(record);
    unsigned char* const text = At(bof3::addr::Text_Records);
    std::memcpy(text, r, 8);
    const unsigned char name8 = r[8];
    r[0x1F] = 0xFF;
    for (unsigned k = 0; k < 7; ++k) r[0x88 + k] = 0;
    text[8] = name8;
    SH_CALL(Msg_OpenScript)(MasterMessage(0xC));
    const unsigned char step = B(at::kStep);
    Field_Request = 2;
    B(at::kStep) = static_cast<unsigned char>(step + 1);
}

// original 0x587260 (0x39 bytes), step 6: once no message is open, message 0xD,
// the step on.
extern "C" void __cdecl MasterQuit_Step6Told(void) { TellAndStep(0xD); }

// original 0x5872A0 (0x81 bytes), step 7: once no message is open - some member
// still the master's apprentice: step 0; none: message 9, the step on.
extern "C" void __cdecl MasterQuit_Step7NoneLeftCheck(void) {
    if (Field_Request == 2) return;
    const unsigned char master = B(at::kMaster);
    const int n = Field_MemberCount;
    int i = 0;
    while (i < n && At(RecordOf(MemberAt(static_cast<unsigned>(i))))[0x1F] != master) ++i;
    if (i < n) {
        B(at::kStep) = 0;
        return;
    }
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(At(at::kMasterMessages + 2u * master)) + 9));
    const unsigned char step = B(at::kStep);
    Field_Request = 2;
    B(at::kStep) = static_cast<unsigned char>(step + 1);
}

// original 0x587330 (0x34 bytes), step 8: once no message is open, message 4 and
// state 6.
extern "C" void __cdecl MasterQuit_Step8Close(void) { TellAndLeave(); }

// --- state 5: what the levels gained under the master grant (MasterGrant_Steps) ----------------

// original 0x587370 (0xE bytes), 0x66450C[5]: jmp [MasterGrant_Steps + 4 * the
// step], unchecked.
extern "C" void __cdecl MasterGrant_ByStep(void) { Jump(at::kGrantSteps, at::kGrantStepCount, B(at::kStep), "MasterGrant_ByStep"); }

// original 0x587380 (0x37 bytes), step 0: message 5, the step on, the cursor 0.
extern "C" void __cdecl MasterGrant_Step0Open(void) {
    SH_CALL(Msg_OpenScript)(MasterMessage(5));
    const unsigned char step = B(at::kStep);
    Field_Request = 2;
    B(at::kCursor) = 0;
    B(at::kStep) = static_cast<unsigned char>(step + 1);
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
    const unsigned char master = B(at::kMaster);
    bool told = false;
    if (At(record)[0x1F] == master) {
        const auto gained = static_cast<unsigned char>(At(record)[0xA] - At(record)[0x88]);
        const unsigned which = static_cast<unsigned>(master) - 0xB;
        if (which <= 3) {
            if (gained >= 3) {
                if (which == 1) {
                    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x6C) == 0 && SH_CALL(Inventory_Add)(3, 0x16, 1) != 0) {
                        SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x6C);
                        SH_CALL(MasterScreen_NameToText)(CursorMember());
                        told = true;
                    }
                } else {
                    const unsigned char bit = which == 0 ? 8 : which == 2 ? 0x10 : 0x20;
                    if ((B(at::kMasterBits) & bit) == 0) {
                        B(at::kMasterBits) = static_cast<unsigned char>(B(at::kMasterBits) | bit);
                        SH_CALL(MasterScreen_NameToText)(member);
                        told = true;
                    }
                }
            }
        } else {
            for (unsigned i = 0; i < 6; ++i) {
                const U pair = at::kMasterSkills + 2 * (6u * B(at::kMaster) + i);
                if (B(pair) > gained) continue;
                const unsigned char ability = B(pair + 1);
                const U bit = 1u << (ability & 0x1F);
                if (static_cast<U>(Long(At(at::kAbilityBits + 4u * (ability >> 5)))) & bit) continue;
                if (SH_CALL(AbilityList_Add)(ability, CursorMember(), 0, 0) == 0) continue;
                SH_CALL(MasterScreen_NameToText)(CursorMember());
                const unsigned char taught = B(at::kMasterSkills + 2 * (6u * B(at::kMaster) + i) + 1);
                const U from = at::kAbilityRecords + 0x18u * taught;
                unsigned char* const text = At(bof3::addr::Text_Records + 0x20);
                std::memcpy(text, At(from), 16);
                unsigned char* const word = At(at::kAbilityBits + 4u * (taught >> 5));
                SetLong(word, static_cast<std::int32_t>(static_cast<U>(Long(word)) | (1u << (taught & 0x1F))));
                told = true;
                break;
            }
        }
    }
    if (told) {
        SH_CALL(Msg_OpenScript)(MasterMessage(0xE));
        Field_Request = 2;
    }
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x5876C0 (0x2F bytes), step 2: once no message is open, the cursor
// on; past the last member state 2; step 1.
extern "C" void __cdecl MasterGrant_Step2Next(void) {
    if (Field_Request == 2) return;
    const auto cursor = static_cast<unsigned char>(B(at::kCursor) + 1);
    B(at::kCursor) = cursor;
    if (cursor >= Field_MemberCount) B(at::kState) = 2;
    B(at::kStep) = 1;
}

// ============================================================================================
// The field menu
// ============================================================================================

// original 0x589E00 (0x44 bytes), FieldMenu_TopBarSteps[1]: Menu_DrawBackdrop;
// the timer down, at 0 the screen of the top bar's entry: the mode = the
// entry + 2, the state, step and 0x929F03 0, the timer 5.
extern "C" void __cdecl FieldMenu_TopBarCountdown(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    const auto t = static_cast<unsigned char>(B(at::kMenuTimer) - 1);
    B(at::kMenuTimer) = t;
    if (t != 0) return;
    const auto mode = static_cast<unsigned char>(B(at::kTopCursor) + 2);
    B(at::kMenuTimer) = 5;
    B(at::kMenuMode) = mode;
    B(at::kMenuState) = 0;
    B(at::kMenuStep) = 0;
    B(at::kMenuSub) = 0;
}

// original 0x589FB0 (0x2F bytes): al 0 when the leader's cell (AreaMap_ByteAt of
// ObjTrio's x / z words) masked 0xF0 is 0xA0 - the original also compares
// 0xA1, 0xAF and 0x91, which the mask makes unreachable (docs/rest_2d.md
// section 5) - else 1.
extern "C" unsigned char __cdecl FieldMenu_CampAllowedCell(void) {
    const unsigned char cell =
        static_cast<unsigned char>(SH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(ObjTrio + 0x36)), static_cast<short>(Word(ObjTrio + 0x3A))) & 0xF0);
    if (cell == 0xA0 || cell == 0xA1 || cell == 0xAF || cell == 0x91) return 0;
    return 1;
}

// --- the field abilities (FieldAbility_Effects, called by FieldAbility_Use) ---------------------

namespace {
// The handlers' answer: 1 when the callee did something (al not 0), else 4.
unsigned Done(unsigned char al) { return al != 0 ? 1u : 4u; }
// (the caster's word +0x2A + 100) * k / 100, as the original's signed division.
unsigned Amount(unsigned caster, int k) {
    const int stat = Word(At(RecordOf(caster & 0xFF) + 0x2A));
    return static_cast<unsigned>((stat + 100) * k / 100);
}
unsigned HealAll(unsigned caster, unsigned battle, int k) {
    unsigned char any = 0;
    for (unsigned char i = 0; i < static_cast<unsigned char>(SH_CALL(Party_Count)(0));) {
        const unsigned amount = Amount(caster, k);
        any = static_cast<unsigned char>(any | SH_CALL(Char_HealHp)(MemberAt(i), amount, battle));
        ++i;
    }
    return Done(any);
}
}  // namespace

// original 0x58A0E0 (3 bytes), FieldAbility_Effects[0]: al 3.
extern "C" unsigned char __cdecl FieldAbility_NotHere(unsigned, unsigned, unsigned) { return 3; }
// original 0x58A0F0 (0x50 bytes), [1]: Char_HealHp(target, (stat + 100) * 20 / 100, battle).
extern "C" unsigned __cdecl FieldAbility_HealOne20(unsigned caster, unsigned target, unsigned battle) {
    return Done(SH_CALL(Char_HealHp)(target, Amount(caster, 20), battle));
}
// original 0x58A140 (0x50 bytes), [2]: the same at 40.
extern "C" unsigned __cdecl FieldAbility_HealOne40(unsigned caster, unsigned target, unsigned battle) {
    return Done(SH_CALL(Char_HealHp)(target, Amount(caster, 40), battle));
}
// original 0x58A190 (0x1E bytes), [3]: Char_HealHp(target, 0, battle) - 0 heals to the full.
extern "C" unsigned __cdecl FieldAbility_HealOneFull(unsigned, unsigned target, unsigned battle) {
    return Done(SH_CALL(Char_HealHp)(target, 0, battle));
}
// original 0x58A1B0 (0xA9 bytes), [4]: each party member (Party_Count(0) asked
// each pass), Char_HealHp(member, (stat + 100) * 40 / 100, battle); 1 when any took.
extern "C" unsigned __cdecl FieldAbility_HealAll40(unsigned caster, unsigned, unsigned battle) {
    return HealAll(caster, battle, 40);
}
// original 0x58A260 (0xAC bytes), [5]: the same at 120.
extern "C" unsigned __cdecl FieldAbility_HealAll120(unsigned caster, unsigned, unsigned battle) {
    return HealAll(caster, battle, 120);
}
// original 0x58A310 (0x21 bytes), [6]: Char_ClearStatus(target, 0x80, battle).
extern "C" unsigned __cdecl FieldAbility_Clear80(unsigned, unsigned target, unsigned battle) {
    return Done(SH_CALL(Char_ClearStatus)(target, 0x80, battle));
}
// original 0x58A340 (3 bytes), [7]: al 4.
extern "C" unsigned char __cdecl FieldAbility_NoEffect(unsigned, unsigned, unsigned) { return 4; }
// original 0x58A350 (0x21 bytes), [8]: Char_ClearStatus(target, 0xA0, battle).
extern "C" unsigned __cdecl FieldAbility_ClearA0(unsigned, unsigned target, unsigned battle) {
    return Done(SH_CALL(Char_ClearStatus)(target, 0xA0, battle));
}
// original 0x58A380 (0x34 bytes), [9]: Char_HealHp(target, 0, battle), then
// Char_ClearStatus(target, 0xA0, battle); 1 when either took.
extern "C" unsigned __cdecl FieldAbility_HealFullClearA0(unsigned, unsigned target, unsigned battle) {
    const unsigned char healed = SH_CALL(Char_HealHp)(target, 0, battle);
    const unsigned char cleared = SH_CALL(Char_ClearStatus)(target, 0xA0, battle);
    return Done(static_cast<unsigned char>(healed | cleared));
}

// original 0x58A3C0 (0xF7 bytes): an ability used from the field menu. The
// caster's record - the working record 0x802DC0 + 0x14C * caster when
// `battle`'s low byte is set, else CharacterRecords - and its AP word +0x1A
// below Skill_ApCost(caster, ability, 0): al 0. Else the handler of
// FieldAbility_Effects by the ability byte (0x46..0x4B and 0xAE..0xB3 the
// handlers 1..6, 0x50 7, 0x73 and 0xD7 8, 0xA8 9, any other 0) called (caster,
// target, battle); its answer 1 or 5 takes the cost (asked again) off the AP
// word. Answers the handler's al.
extern "C" unsigned char __cdecl FieldAbility_Use(unsigned caster, unsigned target, unsigned ability, unsigned battle) {
    const unsigned c = caster & 0xFF;
    const bool working = (battle & 0xFF) != 0;
    const U record = working ? at::kWorking + c * at::kObjStride : RecordOf(c);
    const unsigned char cost = SH_CALL(Skill_ApCost)(caster, ability, 0);
    if (Word(At(record + 0x1A)) < cost) return 0;
    const auto b = static_cast<unsigned char>(ability);
    unsigned index;
    if (b >= 0x46 && b <= 0x4B) index = b - 0x45u;
    else if (b >= 0xAE && b <= 0xB3) index = static_cast<unsigned char>(b + 0x53);
    else if (b == 0x73 || b == 0xD7) index = 8;
    else if (b == 0x50) index = 7;
    else index = b == 0xA8 ? 9 : 0;
    using Effect = unsigned (__cdecl*)(unsigned, unsigned, unsigned);
    const auto handler = reinterpret_cast<Effect>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(at::kAbilityEffects + 4 * index)))));
    const auto answer = static_cast<unsigned char>(handler(caster, target, battle));
    if (answer == 1 || answer == 5) {
        const unsigned char again = SH_CALL(Skill_ApCost)(caster, ability, 0);
        const U checked = working ? RecordChecked(at::kWorking, at::kObjStride, 3, c, "the working records", "FieldAbility_Use")
                                  : RecordChecked(at::kRecords, at::kRecordStride, 8, c, "CharacterRecords", "FieldAbility_Use");
        SetWord(At(checked + 0x1A), Word(At(checked + 0x1A)) - again);
    }
    return answer;
}

// --- the Status screen: FieldMenu_States[6] ----------------------------------------------------

namespace {
const unsigned char* StatusRecord() {
    const auto c = static_cast<signed char>(B(at::kStatusCursor));
    return At(RecordOf(MoveScript_EffectState[B(at::kPartyList + c)]));
}
}  // namespace

// original 0x58A4C0 (0xE bytes), FieldMenu_States[6]: jmp [FieldMenuStatus_States
// + 4 * the menu state], unchecked.
extern "C" void __cdecl FieldMenuStatus_ByState(void) {
    Jump(at::kStatusStates, at::kStatusStateCount, B(at::kMenuState), "FieldMenuStatus_ByState");
}

// original 0x58A4D0 (0x3A bytes), state 0: Menu_DrawBackdrop, the windows placed;
// the cursor 0, the timer 8, the state on; Sound_PlayEffect(0x102).
extern "C" void __cdecl FieldMenuStatus_Open(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    SH_CALL(FieldMenuStatus_PlaceWindows)();
    const auto state = static_cast<unsigned char>(B(at::kMenuState) + 1);
    B(at::kStatusCursor) = 0;
    B(at::kMenuTimer) = 8;
    B(at::kMenuState) = state;
    SH_CALL(Sound_PlayEffect)(0x102);
}

// original 0x58A510 (0x5F bytes), state 1: the cursor's member's name record
// (TextRecord_Set(0, 8, its record)), window 11's word +0x10 = 0x12,
// Menu_DrawBackdrop; the timer down, the state on when it was 0.
extern "C" void __cdecl FieldMenuStatus_SlideIn(void) {
    SH_CALL(TextRecord_Set)(0, 8, StatusRecord());
    const unsigned char kind = B(at::kBackdrop);
    SetWord(At(0x8032FC), 0x12);
    SH_CALL(Menu_DrawBackdrop)(kind);
    const unsigned char t = B(at::kMenuTimer);
    B(at::kMenuTimer) = static_cast<unsigned char>(t - 1);
    if (t == 0) B(at::kMenuState) = static_cast<unsigned char>(B(at::kMenuState) + 1);
}

// original 0x58A570 (0x1B8 bytes), state 2: Menu_DrawBackdrop; window 19 on at the
// cursor's member's window (4 + cursor)'s place; the name record, window 11's
// +0x10 = 0x12. Input_AutoRepeat(Input_Pressed & 0x5000): 0x1000 the cursor
// back (below 0 to the last), else 0x4000 on (past the last to 0) - signed
// against Party_Count(0); a cursor that differs from the one read before
// (zero-extended against sign-extended) Sound_PlayEffect(0x100). Confirm -
// 0x103, 0x102, the detail's windows, window 19 off, the timer 8, the state
// on; cancel (Input_Pressed read again) - 0x106, 0x102, windows 12 + i state 1
// for each member, window 11 state 1, window 19 off, the timer 8, state + 2.
extern "C" void __cdecl FieldMenuStatus_Choose(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    const auto c = static_cast<signed char>(B(at::kStatusCursor));
    B(0x80340C) = 1;
    const U member_window = at::kWindows + at::kWindowStride * static_cast<U>(4 + c);
    SetWord(At(0x803410), Word(At(member_window + 4)));
    SetWord(At(0x803412), Word(At(member_window + 6)));
    SH_CALL(TextRecord_Set)(0, 8, At(RecordOf(MoveScript_EffectState[B(at::kPartyList + c)])));
    const unsigned pressed = Input_Pressed;
    SetWord(At(0x8032FC), 0x12);
    const U held = SH_CALL(Input_AutoRepeat)(pressed & 0x5000);
    const auto count = static_cast<unsigned char>(SH_CALL(Party_Count)(0));
    const unsigned char old = B(at::kStatusCursor);
    unsigned char cursor = old;
    if (held & 0x1000) {
        cursor = static_cast<unsigned char>(cursor - 1);
        B(at::kStatusCursor) = cursor;
        if (static_cast<signed char>(cursor) < 0) {
            cursor = static_cast<unsigned char>(count - 1);
            B(at::kStatusCursor) = cursor;
        }
    } else if (held & 0x4000) {
        cursor = static_cast<unsigned char>(cursor + 1);
        B(at::kStatusCursor) = cursor;
        if (static_cast<int>(static_cast<signed char>(cursor)) >= static_cast<int>(count)) {
            cursor = 0;
            B(at::kStatusCursor) = 0;
        }
    }
    if (static_cast<int>(old) != static_cast<int>(static_cast<signed char>(cursor))) SH_CALL(Sound_PlayEffect)(0x100);
    if (Field_ConfirmButtons & Input_Pressed) {
        SH_CALL(Sound_PlayEffect)(0x103);
        SH_CALL(Sound_PlayEffect)(0x102);
        SH_CALL(FieldMenuStatus_DetailWindows)();
        const unsigned char state = B(at::kMenuState);
        B(0x80340C) = 0;
        B(at::kMenuTimer) = 8;
        B(at::kMenuState) = static_cast<unsigned char>(state + 1);
    }
    if (Field_CancelButtons & Input_Pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        SH_CALL(Sound_PlayEffect)(0x102);
        int i = 0;
        if (static_cast<unsigned char>(SH_CALL(Party_Count)(0)) != 0) {
            do {
                B(Window(12 + static_cast<unsigned>(i), "FieldMenuStatus_Choose") + 3) = 1;
                ++i;
            } while (i < static_cast<int>(static_cast<unsigned char>(SH_CALL(Party_Count)(0))));
        }
        const unsigned char state = B(at::kMenuState);
        B(0x8032EF) = 1;
        B(0x80340C) = 0;
        B(at::kMenuTimer) = 8;
        B(at::kMenuState) = static_cast<unsigned char>(state + 2);
    }
}

// original 0x58A730 (0x95 bytes), state 3: Menu_DrawBackdrop, the name record,
// window 11's +0x10 = 0x12; while the timer runs it counts down; then cancel -
// 0x106, 0x102, the list's windows, the timer 8, the state back two.
extern "C" void __cdecl FieldMenuStatus_Detail(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    SH_CALL(TextRecord_Set)(0, 8, StatusRecord());
    const unsigned char t = B(at::kMenuTimer);
    SetWord(At(0x8032FC), 0x12);
    if (t != 0) {
        B(at::kMenuTimer) = static_cast<unsigned char>(B(at::kMenuTimer) - 1);
        return;
    }
    if ((Input_Pressed & Field_CancelButtons) == 0) return;
    SH_CALL(Sound_PlayEffect)(0x106);
    SH_CALL(Sound_PlayEffect)(0x102);
    SH_CALL(FieldMenuStatus_ListWindows)();
    const unsigned char state = B(at::kMenuState);
    B(at::kMenuTimer) = 8;
    B(at::kMenuState) = static_cast<unsigned char>(state - 2);
}

// original 0x58A7D0 (0x7F bytes), state 4: Menu_DrawBackdrop; the timer down, at
// 0 the screen's windows cleared, windows 4 + i state 4 for each member, the
// timer 0, windows 7..10 state 2, back to the top bar (mode 1, state 0).
extern "C" void __cdecl FieldMenuStatus_Close(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    const auto t = static_cast<unsigned char>(B(at::kMenuTimer) - 1);
    B(at::kMenuTimer) = t;
    if (t != 0) return;
    SH_CALL(FieldMenuStatus_ClearWindows)();
    int i = 0;
    if (static_cast<unsigned char>(SH_CALL(Party_Count)(0)) != 0) {
        do {
            B(Window(4 + static_cast<unsigned>(i), "FieldMenuStatus_Close") + 3) = 4;
            ++i;
        } while (i < static_cast<int>(static_cast<unsigned char>(SH_CALL(Party_Count)(0))));
    }
    B(at::kMenuTimer) = 0;
    B(0x80325F) = 2;
    B(0x803283) = 2;
    B(0x8032A7) = 2;
    B(0x8032CB) = 2;
    B(at::kMenuMode) = 1;
    B(at::kMenuState) = 0;
}

// original 0x58A850 (0xCF bytes): for each member i (Party_Count(0) asked each
// pass), window 12 + i: +1 6, +2 5, +3 2, +0 1, +4 0x140, +0xA i, +6 window
// (4 + i)'s +6; window 11: +1 6, +2 9, +3 2, +0 1, +4 0x14, +6 -20, +0x10 0;
// window 19: +1 6, +2 0xA, +0 0, +0xA 0, +0xB 1.
extern "C" void __cdecl FieldMenuStatus_PlaceWindows(void) {
    for (unsigned char i = 0; i < static_cast<unsigned char>(SH_CALL(Party_Count)(0));) {
        const U w = Window(12 + i, "FieldMenuStatus_PlaceWindows");
        B(w + 1) = 6;
        B(w + 2) = 5;
        B(w + 3) = 2;
        B(w + 0) = 1;
        SetWord(At(w + 4), 0x140);
        const unsigned short y = Word(At(at::kWindows + at::kWindowStride * (4u + i) + 6));
        B(w + 0xA) = i;
        ++i;
        SetWord(At(w + 6), y);
    }
    B(0x8032ED) = 6;
    B(0x8032EE) = 9;
    B(0x8032EF) = 2;
    B(0x8032EC) = 1;
    SetWord(At(0x8032F0), 0x14);
    SetWord(At(0x8032F2), 0xFFEC);
    SetWord(At(0x8032FC), 0);
    B(0x80340D) = 6;
    B(0x80340E) = 0xA;
    B(0x80340C) = 0;
    B(0x803416) = 0;
    B(0x803417) = 1;
}

namespace {
// The detail's and the list's windows: for each member i, window 4 + i's state
// `mine` and window 12 + i's `mine_list` when i is the cursor (signed), else
// `other` and `other_list`.
void MemberWindowStates(unsigned char mine, unsigned char mine_list, unsigned char other, unsigned char other_list,
                        const char* who) {
    for (unsigned char i = 0; i < static_cast<unsigned char>(SH_CALL(Party_Count)(0));) {
        const bool at_cursor = static_cast<int>(i) == static_cast<int>(static_cast<signed char>(B(at::kStatusCursor)));
        B(Window(4 + i, who) + 3) = at_cursor ? mine : other;
        B(Window(12 + i, who) + 3) = at_cursor ? mine_list : other_list;
        ++i;
    }
}
}  // namespace

// original 0x58A920 (0x109 bytes): the detail's windows - the members' windows 5 /
// 3 at the cursor, 7 / 5 elsewhere; windows 15, 16, 17 state 2, +1 6, +2 6 / 7
// / 8, +0 1, (+4, +6) (0x140, 0x74) / (-100, 0x78) / (-100, 0xA8), +0xA the
// cursor.
extern "C" void __cdecl FieldMenuStatus_DetailWindows(void) {
    MemberWindowStates(5, 3, 7, 5, "FieldMenuStatus_DetailWindows");
    const unsigned char cursor = B(at::kStatusCursor);
    B(0x80337F) = 2;
    B(0x8033A3) = 2;
    B(0x8033C7) = 2;
    B(0x80337D) = 6;
    B(0x80337E) = 6;
    B(0x80337C) = 1;
    SetWord(At(0x803380), 0x140);
    SetWord(At(0x803382), 0x74);
    B(0x803386) = cursor;
    B(0x8033A1) = 6;
    B(0x8033A2) = 7;
    B(0x8033A0) = 1;
    SetWord(At(0x8033A4), 0xFF9C);
    SetWord(At(0x8033A6), 0x78);
    B(0x8033AA) = cursor;
    B(0x8033C5) = 6;
    B(0x8033C6) = 8;
    B(0x8033C4) = 1;
    SetWord(At(0x8033C8), 0xFF9C);
    SetWord(At(0x8033CA), 0xA8);
    B(0x8033CE) = cursor;
}

// original 0x58AA30 (0x80 bytes): back to the list - the members' windows 6 / 4
// at the cursor, 8 / 6 elsewhere; windows 15, 16, 17 state 1.
extern "C" void __cdecl FieldMenuStatus_ListWindows(void) {
    MemberWindowStates(6, 4, 8, 6, "FieldMenuStatus_ListWindows");
    B(0x80337F) = 1;
    B(0x8033A3) = 1;
    B(0x8033C7) = 1;
}

// original 0x58AAB0 (0x21 bytes): windows 12..17's +0 cleared.
extern "C" void __cdecl FieldMenuStatus_ClearWindows(void) {
    B(0x803310) = 0;
    B(0x803334) = 0;
    B(0x803358) = 0;
    B(0x80337C) = 0;
    B(0x8033A0) = 0;
    B(0x8033C4) = 0;
}

// --- the Items screen: FieldMenu_States[2] -------------------------------------------------------

// original 0x58AAE0 (0xE bytes), FieldMenu_States[2]: jmp [FieldMenuItems_States
// + 4 * the menu state], unchecked.
extern "C" void __cdecl FieldMenuItems_ByState(void) {
    Jump(at::kItemsStates, at::kItemsStateCount, B(at::kMenuState), "FieldMenuItems_ByState");
}

// original 0x58AAF0 (0x38 bytes), state 0: Menu_DrawBackdrop, the windows placed
// (R2E's 0x58BC30), Sound_PlayEffect(0x102); the state on, the category 0,
// the timer 5.
extern "C" void __cdecl FieldMenuItems_Open(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    SH_AT(void (__cdecl*)(), at::kItemsWindows)();
    SH_CALL(Sound_PlayEffect)(0x102);
    const unsigned char state = B(at::kMenuState);
    B(at::kItemsCategory) = 0;
    B(at::kMenuTimer) = 5;
    B(at::kMenuState) = static_cast<unsigned char>(state + 1);
}

// original 0x58AB30 (0x30 bytes), state 1: Menu_DrawBackdrop; the timer down, at
// 0 the timer 0 and the state on.
extern "C" void __cdecl FieldMenuItems_SlideIn(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    const auto t = static_cast<unsigned char>(B(at::kMenuTimer) - 1);
    B(at::kMenuTimer) = t;
    if (t != 0) return;
    const unsigned char state = B(at::kMenuState);
    B(at::kMenuTimer) = 0;
    B(at::kMenuState) = static_cast<unsigned char>(state + 1);
}

// original 0x58AB60 (0x1C8 bytes), state 2: the category row. Menu_DrawBackdrop;
// window 13's +8 0, window 11's +0x10 the category + 0x13, window 13's +0xC
// 0xFF, the hand (window 21) on at window 12's (x + 48 * category, y + 4),
// window 12's +0xB 0xFF. Input_AutoRepeat(Input_Pressed & 0xA000): 0x8000 the
// category back (below 0 to 3), 0x2000 on (past 3 to 0), signed; a change
// 0x101. Confirm - window 12's +0xB the category, 0x104; category 1: the step
// 0, state + 3; category 3: window 13's pick and top from 0x93989C / 0x9398BC,
// its +0xA 4, state + 7; else window 13's +0xD 0xFF, its top and pick from
// 0x9398B8 / 0x939898 by its own +0xA, state + 1. Cancel - 0x106, 0x102,
// windows 11, 12, 13 state 1, the hand off, the timer + 5, state + 2.
extern "C" void __cdecl FieldMenuItems_Category(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    const auto category = static_cast<signed char>(B(at::kItemsCategory));
    B(0x80333C) = 0;
    const U window_x = static_cast<U>(Long(At(0x803314)));
    SetWord(At(0x8032FC), static_cast<unsigned>(category + 0x13));
    const unsigned pressed = Input_Pressed;
    B(0x803340) = 0xFF;
    const unsigned short y = static_cast<unsigned short>(Word(At(0x803316)) + 4);
    B(0x803454) = 1;
    SetWord(At(0x803458), static_cast<unsigned>(48 * category) + window_x);
    SetWord(At(0x80345A), y);
    B(0x80331B) = 0xFF;
    const U held = SH_CALL(Input_AutoRepeat)(pressed & 0xA000);
    unsigned char c = B(at::kItemsCategory);
    const unsigned char was = c;
    if (held & 0x8000) {
        c = static_cast<unsigned char>(c - 1);
        B(at::kItemsCategory) = c;
        if (static_cast<signed char>(c) < 0) {
            c = 3;
            B(at::kItemsCategory) = 3;
        }
    }
    if (held & 0x2000) {
        c = static_cast<unsigned char>(c + 1);
        B(at::kItemsCategory) = c;
        if (static_cast<signed char>(c) > 3) {
            c = 0;
            B(at::kItemsCategory) = 0;
        }
    }
    if (was != c) {
        SH_CALL(Sound_PlayEffect)(0x101);
        c = B(at::kItemsCategory);
    }
    if (Field_ConfirmButtons & Input_Pressed) {
        B(0x80331B) = c;
        SH_CALL(Sound_PlayEffect)(0x104);
        const unsigned char chosen = B(at::kItemsCategory);
        if (chosen == 1) {
            const unsigned char state = B(at::kMenuState);
            B(at::kMenuStep) = 0;
            B(at::kMenuState) = static_cast<unsigned char>(state + 3);
            return;
        }
        if (chosen == 3) {
            const unsigned char pick = B(0x93989C);
            const unsigned char top = B(0x9398BC);
            B(0x803340) = pick;
            const unsigned char state = B(at::kMenuState);
            B(0x80333E) = 4;
            B(0x80333F) = top;
            B(at::kMenuState) = static_cast<unsigned char>(state + 7);
            return;
        }
        const unsigned char list = B(0x80333E);
        B(0x803341) = 0xFF;
        const unsigned char top = B(at::kItemsTops + list);
        const unsigned char state = B(at::kMenuState);
        B(0x80333F) = top;
        const unsigned char pick = B(at::kItemsPicks + list);
        B(at::kMenuState) = static_cast<unsigned char>(state + 1);
        B(0x803340) = pick;
        return;
    }
    if (Field_CancelButtons & Input_Pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        SH_CALL(Sound_PlayEffect)(0x102);
        const unsigned char t = B(at::kMenuTimer);
        const unsigned char state = B(at::kMenuState);
        B(0x8032EF) = 1;
        B(0x803313) = 1;
        B(0x803337) = 1;
        B(0x803454) = 0;
        B(at::kMenuTimer) = static_cast<unsigned char>(t + 5);
        B(at::kMenuState) = static_cast<unsigned char>(state + 2);
    }
}

namespace {
// Inventory_IdLists' / Inventory_CountLists' list of a category: the original
// reads the five-pointer tables by the category byte unbounded; past them the
// cell is not a list - ours aborts.
U ListOf(U table, unsigned category, const char* who) {
    if (category >= 5)
        bof3::Fatal("%s: category %u is past Inventory_IdLists' five lists; the original reads 0x%X as one", who, category,
                    static_cast<unsigned>(table + 4 * category));
    return static_cast<U>(Long(At(table + 4 * category)));
}
}  // namespace

// original 0x58AD30 (0x3F5 bytes), state 3: the item list of window 13's
// category +0xA. Menu_DrawBackdrop; window 13's +8 = 3 with the category row's
// cursor not 0, else 1; window 11's +0x10 = Item_HelpMessage(category, the
// picked item); the hand (window 21) on at (window 13's x + 7, its y + 13 *
// (pick - top + 2)). Input_AutoRepeat(Input_Pressed & 0xF00C): 0x8000 / 0x2000
// another category (window 13's +0x10 0x32 / 0x31; the old one's pick and top
// kept in 0x939898 / 0x9398B8, the new one's taken; 0..3 wrapping); else 0x1000
// / 0x4000 the pick up / down (0..0x7F; leaving the nine rows shown sets window
// 13's +0x12 to 0xF0 / 0x10, the scroll), 4 / 8 a page up / down (the top 0..0x77
// by nine); the pick and top kept; a moved pick 0x100. While the scroll word is
// 0: confirm on an empty slot 0x107; else 0x103 - with the category row's cursor
// not 0, Item_CanUse(window 13's +8, 0, category, item) refused 0x107, taken:
// +0xD the pick, the answer 0, state + 3; with it 0, the same test, then an item
// whose flags have bit 4 used at once through ItemUse_Dispatch(0, item, 0) -
// answers 0 or 4: 0x104 and one fewer (the slot emptied at 0), any other 0x107
// - and one without: the cursor 0, +0xD the pick, the hand off, state + 4.
// Cancel - 0x106, the state back.
extern "C" void __cdecl FieldMenuItems_List(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    const unsigned char cursor_row = B(at::kItemsCategory);
    const unsigned char category = B(0x80333E);
    const U ids = ListOf(at::kItemIdLists, category, "FieldMenuItems_List");
    B(0x80333C) = cursor_row != 0 ? 3 : 1;
    const unsigned char item = B(ids + B(0x803340));
    const U help = SH_CALL(Item_HelpMessage)(category, item);
    const unsigned char top = B(0x80333F);
    const U window_x = static_cast<U>(Long(At(0x803338)));
    SetWord(At(0x8032FC), help);
    const int row = static_cast<int>(B(0x803340)) - static_cast<int>(top) + 2;
    const unsigned pressed = Input_Pressed;
    SetWord(At(0x803458), window_x + 7);
    B(0x803454) = 1;
    SetWord(At(0x80345A), static_cast<unsigned>(row * 13) + Word(At(0x80333A)));
    const U held = SH_CALL(Input_AutoRepeat)(pressed & 0xF00C);
    if (held & 0x8000) {
        SH_CALL(Sound_PlayEffect)(0x101);
        const unsigned char old = B(0x80333E);
        const unsigned char pick = B(0x803340);
        auto next = static_cast<unsigned char>(old - 1);
        SetWord(At(0x803344), 0x32);
        B(0x80333E) = next;
        B(at::kItemsPicks + old) = pick;
        B(at::kItemsTops + old) = B(0x80333F);
        if (static_cast<signed char>(next) < 0) {
            next = 3;
            B(0x80333E) = 3;
        }
        B(0x803340) = B(at::kItemsPicks + next);
        B(0x80333F) = B(at::kItemsTops + next);
        return;
    }
    if (held & 0x2000) {
        SetWord(At(0x803344), 0x31);
        SH_CALL(Sound_PlayEffect)(0x101);
        const unsigned char old = B(0x80333E);
        const unsigned char pick = B(0x803340);
        auto next = static_cast<unsigned char>(old + 1);
        B(0x80333E) = next;
        B(at::kItemsPicks + old) = pick;
        B(at::kItemsTops + old) = B(0x80333F);
        if (next > 3) {
            next = 0;
            B(0x80333E) = 0;
        }
        B(0x803340) = B(at::kItemsPicks + next);
        B(0x80333F) = B(at::kItemsTops + next);
        return;
    }
    unsigned char pick = B(0x803340);
    const unsigned char was = pick;
    unsigned char first = B(0x80333F);
    if (held & 0x1000) {
        if (pick != 0) {
            pick = static_cast<unsigned char>(pick - 1);
            B(0x803340) = pick;
        }
        first = B(0x80333F);
        if (pick < first) SetWord(At(0x803346), 0xF0);
    } else if (held & 0x4000) {
        if (pick < 0x7F) {
            pick = static_cast<unsigned char>(pick + 1);
            B(0x803340) = pick;
        }
        first = B(0x80333F);
        if (static_cast<int>(B(0x803340)) >= static_cast<int>(first) + 9) SetWord(At(0x803346), 0x10);
    } else if (held & 4) {
        first = B(0x80333F);
        if (first == 0) {
            pick = 0;
            B(0x803340) = 0;
        } else {
            if (first < 9) {
                pick = static_cast<unsigned char>(pick - first);
                first = 0;
            } else {
                pick = static_cast<unsigned char>(pick - 9);
                first = static_cast<unsigned char>(first - 9);
            }
            B(0x803340) = pick;
            B(0x80333F) = first;
        }
    } else {
        first = B(0x80333F);
        if (held & 8) {
            if (first == 0x77) {
                pick = 0x7F;
                B(0x803340) = 0x7F;
            } else {
                if (first > 0x6E) {
                    pick = static_cast<unsigned char>(pick + (0x77 - first));
                    first = 0x77;
                } else {
                    pick = static_cast<unsigned char>(pick + 9);
                    first = static_cast<unsigned char>(first + 9);
                }
                B(0x803340) = pick;
                B(0x80333F) = first;
            }
        }
    }
    const unsigned char list = B(0x80333E);
    B(at::kItemsTops + list) = first;
    B(at::kItemsPicks + list) = pick;
    if (was != pick) SH_CALL(Sound_PlayEffect)(0x100);
    const unsigned slot = B(0x803340);
    const unsigned char now = B(0x80333E);
    const U id_at = ListOf(at::kItemIdLists, now, "FieldMenuItems_List") + slot;
    const U count_list = ListOf(at::kItemCountLists, now, "FieldMenuItems_List");
    if (Word(At(0x803346)) != 0) return;
    if (Field_ConfirmButtons & Input_Pressed) {
        if (B(id_at) == 0) {
            SH_CALL(Sound_PlayEffect)(0x107);
            return;
        }
        SH_CALL(Sound_PlayEffect)(0x103);
        if (B(at::kItemsCategory) != 0) {
            const unsigned char cat = B(0x80333E);
            const unsigned char it = B(ListOf(at::kItemIdLists, cat, "FieldMenuItems_List") + B(0x803340));
            if (SH_CALL(Item_CanUse)(B(0x80333C), 0, cat, it) == 0) {
                SH_CALL(Sound_PlayEffect)(0x107);
                return;
            }
            const unsigned char p = B(0x803340);
            B(0x803341) = p;
            const unsigned char state = B(at::kMenuState);
            B(at::kMenuAnswer) = 0;
            B(at::kMenuState) = static_cast<unsigned char>(state + 3);
            return;
        }
        if (SH_CALL(Item_CanUse)(B(0x80333C), 0, B(0x80333E), B(id_at)) == 0) {
            SH_CALL(Sound_PlayEffect)(0x107);
            return;
        }
        const unsigned char it = B(id_at);
        if ((B(at::kItemFlags + 22u * it) & 0x10) == 0) {
            const unsigned char state = B(at::kMenuState);
            const unsigned char p = B(0x803340);
            B(at::kStatusCursor) = 0;
            B(0x803341) = p;
            B(0x803454) = 0;
            B(at::kMenuState) = static_cast<unsigned char>(state + 4);
            return;
        }
        const unsigned char used = SH_CALL(ItemUse_Dispatch)(0, it, 0);
        if (used != 0 && used != 4) {
            SH_CALL(Sound_PlayEffect)(0x107);
            return;
        }
        SH_CALL(Sound_PlayEffect)(0x104);
        if (count_list == 0)
            bof3::Fatal("FieldMenuItems_List: category %u has no count list (Inventory_CountLists' fifth is 0); the original "
                        "writes at 0x%X",
                        static_cast<unsigned>(now), slot);
        const auto left = static_cast<unsigned char>(B(count_list + slot) - 1);
        B(count_list + slot) = left;
        if (left == 0) B(id_at) = 0;
        return;
    }
    if (Field_CancelButtons & Input_Pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        B(at::kMenuState) = static_cast<unsigned char>(B(at::kMenuState) - 1);
    }
}

// original 0x58B130 (0x8D bytes), state 4: Menu_DrawBackdrop; the timer down, at
// 0 the windows taken down (R2E's 0x58C2A0), windows 4 + i state 3 for each
// member, the timer 0, windows 7..10 state 2, back to the top bar.
extern "C" void __cdecl FieldMenuItems_Close(void) {
    SH_CALL(Menu_DrawBackdrop)(B(at::kBackdrop));
    const auto t = static_cast<unsigned char>(B(at::kMenuTimer) - 1);
    B(at::kMenuTimer) = t;
    if (t != 0) return;
    SH_AT(void (__cdecl*)(), at::kItemsReset)();
    for (unsigned char i = 0; i < static_cast<unsigned char>(SH_CALL(Party_Count)(0));) {
        const unsigned char k = i;
        ++i;
        B(Window(4 + k, "FieldMenuItems_Close") + 3) = 3;
    }
    B(at::kMenuTimer) = 0;
    B(0x80325F) = 2;
    B(0x803283) = 2;
    B(0x8032A7) = 2;
    B(0x8032CB) = 2;
    B(at::kMenuMode) = 1;
    B(at::kMenuState) = 0;
}

// original 0x58B1C0 (0xE bytes), FieldMenuItems_States[5]: jmp
// [FieldMenuItems_State5Steps + 4 * the menu step], unchecked.
extern "C" void __cdecl FieldMenuItems_State5ByStep(void) {
    Jump(at::kItemsState5Steps, at::kItemsState5StepCount, B(at::kMenuStep), "FieldMenuItems_State5ByStep");
}

// ============================================================================================

void Rest2D_Inject() {
    if (bof3::WantsShadow("rest_2d")) rest_2d::SelfTest();
    BOF3_INJECT(MasterScreen_PickMember);
    BOF3_INJECT(MasterScreen_DrawCursorFrame);
    BOF3_INJECT(MasterJoin_Step3Ask);
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
    BOF3_INJECT(FieldMenu_TopBarCountdown);
    BOF3_INJECT(FieldMenu_CampAllowedCell);
    BOF3_INJECT(FieldAbility_NotHere);
    BOF3_INJECT(FieldAbility_HealOne20);
    BOF3_INJECT(FieldAbility_HealOne40);
    BOF3_INJECT(FieldAbility_HealOneFull);
    BOF3_INJECT(FieldAbility_HealAll40);
    BOF3_INJECT(FieldAbility_HealAll120);
    BOF3_INJECT(FieldAbility_Clear80);
    BOF3_INJECT(FieldAbility_NoEffect);
    BOF3_INJECT(FieldAbility_ClearA0);
    BOF3_INJECT(FieldAbility_HealFullClearA0);
    BOF3_INJECT(FieldAbility_Use);
    BOF3_INJECT(FieldMenuStatus_ByState);
    BOF3_INJECT(FieldMenuStatus_Open);
    BOF3_INJECT(FieldMenuStatus_SlideIn);
    BOF3_INJECT(FieldMenuStatus_Choose);
    BOF3_INJECT(FieldMenuStatus_Detail);
    BOF3_INJECT(FieldMenuStatus_Close);
    BOF3_INJECT(FieldMenuStatus_PlaceWindows);
    BOF3_INJECT(FieldMenuStatus_DetailWindows);
    BOF3_INJECT(FieldMenuStatus_ListWindows);
    BOF3_INJECT(FieldMenuStatus_ClearWindows);
    BOF3_INJECT(FieldMenuItems_ByState);
    BOF3_INJECT(FieldMenuItems_Open);
    BOF3_INJECT(FieldMenuItems_SlideIn);
    BOF3_INJECT(FieldMenuItems_Category);
    BOF3_INJECT(FieldMenuItems_List);
    BOF3_INJECT(FieldMenuItems_Close);
    BOF3_INJECT(FieldMenuItems_State5ByStep);
}
