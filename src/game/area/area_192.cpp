// Moved here by tools/move_funcs.py from area_w4f.cpp (docs/source-layout-round.md).
#include "game/area/area_192.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4f::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The answer as the originals' movsx reads it.
int AnswerS8() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// Area 192's choice 2 and area 193's choice 3 (0x42BD60, 0x42C430: one body,
// two tables): the focus object's dwords +0x18 and +0x1C the pair of bytes
// the s8 answer picks (table + answer * 2, unchecked), the message word 0xFFFF
// stored between the first read and the first store; the focus pointer and
// the answer read again for the second.
void FocusPair(U table) {
    int a = AnswerS8();
    unsigned char* focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    const unsigned first = B(table + static_cast<U>(a * 2));
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, static_cast<std::int32_t>(first));
    focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    a = AnswerS8();
    SetLong(focus + 0x1C, static_cast<std::int32_t>(B(table + 1 + static_cast<U>(a * 2))));
}

// The tail disarmed: kind 0, state 0.
void Disarm() {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// The tail armed after ScriptFlags_Set40: kind `kind`, state `state`.
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}

// The run armed after ScriptFlags_Set40: MoveScript_Var7 `var`, its step
// byte `step`.
void ArmRun(signed char var, unsigned char step) {
    AH_CALL(ScriptFlags_Set40)();
    MoveScript_Var7 = var;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = step;
}

// The four bytes after Party_Zenny's first word set together by area 192's
// tail and its EffectKind18 state: the word 0x1E0 and the two bytes 0.
void ResetWord1E0() {
    SetWord(Mem(::bof3::Img(Cond_Flags_addr) + 0xcc), 0x1E0);
    B(::bof3::Img(Area189_StepsSinceEvent_addr)) = 0;
    B(::bof3::Img(Area189_EventsInRow_addr)) = 0;
}

}  // namespace

// original 0x42BD60 (area 192 +0x34[2]): FocusPair(Area192_FocusPairs).
extern "C" void __cdecl Area192_ChoiceFocusPair(void) { FocusPair(::bof3::Img(Area192_FocusPairs_addr)); }

// original 0x42BDA0 (areas 191 and 192 +0x34[4]): message 0xFFFF; the tail's
// state 2 for the answer 0, 0xA for 1, 0x14 for any other (the answer read
// once, as a byte, before the message store).
extern "C" void __cdecl Area192_ChoiceTailState(void) {
    const unsigned char a = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    B(::bof3::Img(AreaW0c_TailState_addr)) = a == 0 ? 2 : a == 1 ? 0xA : 0x14;
}

// original 0x42BDD0 (area 192 +0x34[5]): message 0xFFFF; the answer 1:
// ScriptFlags_Set40, tail kind 0x36 at state 0x1E; 2: ScriptFlags_Set40,
// MoveScript_Var7 1, its step 0x14; other answers nothing more.
extern "C" void __cdecl Area192_ChoiceArmTail54(void) {
    const unsigned char a = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (a == 1)
        ArmTail(0x36, 0x1E);
    else if (a == 2)
        ArmRun(1, 0x14);
}

// original 0x42BE10 (Field_ModeTailKinds[54]): by the s8 state (a byte table
// of 31 into a jump table of 8, both in its extent; the `ja` makes every
// other state, and every negative one, nothing):
//   0: message 0x6D, Field_Request 2, state 1 (1 waits);
//   2: unless Field_Request is 2, Transition_Start(0), state 3;
//   3: once Transition_Busy is 0: Draw_PassFlags 0, Sound_StopMusic,
//      Area192_RestoreCharacters, Sound_LoadStream(0), state 4;
//   4: once Sound_StreamDone: story flag 0x82 set; Field_StatusBits (read
//      after the call) bit 0 cleared, with the word 0x90405C 0x1E0 and the
//      bytes 0x929EC1, 0x9036D0 0 stored between; state 0xA;
//   0xA: unless Field_Request is 2: ScriptFlags_Clear40, Field_ChangeArea to
//      the return point (its area word, x, z; flags 4), Field_ScriptFlags2 bit
//      6, the return point's byte +0xA 0, story flag 0x77 cleared, disarmed;
//   0x14: unless Field_Request is 2: ScriptFlags_Clear40, disarmed;
//   0x1E: unless Field_Request is 2: ScriptFlags_Clear40,
//      Field_ChangeArea(0x96, 0x180000, 0x300000, 1), Field_ScriptFlags2 bit 6,
//      byte +0xA 0, story flag 0x77 cleared, disarmed, Field_StatusBits (read
//      after the call) bit 0 cleared.
extern "C" void __cdecl Area192_Tail54(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0:
        AH_CALL(Msg_OpenScript)(0x6D);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    case 2:
        if (Field_Request == 2) return;
        AH_CALL(Transition_Start)(0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 3;
        return;
    case 3:
        if (Transition_Busy != 0) return;
        Draw_PassFlags = 0;
        AH_CALL(Sound_StopMusic)();
        AH_CALL(Area192_RestoreCharacters)();
        AH_CALL(Sound_LoadStream)(0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 4;
        return;
    case 4: {
        if (AH_CALL(Sound_StreamDone)() == 0) return;
        AH_CALL(Flags_Set)(StoryFlags(), 0x82);
        const unsigned char bits = Field_StatusBits;
        B(::bof3::Img(Area189_StepsSinceEvent_addr)) = 0;
        B(::bof3::Img(Area189_EventsInRow_addr)) = 0;
        SetWord(Mem(::bof3::Img(Cond_Flags_addr) + 0xcc), 0x1E0);
        Field_StatusBits = static_cast<unsigned char>(bits & 0xFE);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xA;
        return;
    }
    case 0xA: {
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        const std::int32_t z = Long(Mem(::bof3::Img(AreaW2d_ReturnPoint_addr) + 4));
        const std::int32_t x = Long(Mem(::bof3::Img(AreaW2d_ReturnPoint_addr)));
        const unsigned area = Word(Mem(::bof3::Img(AreaW4d_ReturnArea_addr)));
        AH_CALL(Field_ChangeArea)(area, x, z, 4);
        B(::bof3::Img(Field_ScriptFlags2_addr)) |= 0x40;
        B(::bof3::Img(AreaW3c_CampFlag_addr)) = 0;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        Disarm();
        return;
    }
    case 0x14:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        Disarm();
        return;
    case 0x1E: {
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Field_ChangeArea)(0x96, 0x180000, 0x300000, 1);
        B(::bof3::Img(Field_ScriptFlags2_addr)) |= 0x40;
        B(::bof3::Img(AreaW3c_CampFlag_addr)) = 0;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        const unsigned char bits = Field_StatusBits;
        Disarm();
        Field_StatusBits = static_cast<unsigned char>(bits & 0xFE);
        return;
    }
    default:
        return;
    }
}

// original 0x42C000 (Area_StepHook's case for area 0xC0): (x, z) 16.16; the
// cell at their high words and, when x's low word is not 0, the cell one on in
// x, when z's is not 0 one on in z, when both the diagonal (AreaMap_ByteAt,
// each asked whatever the others answered): any of them 0xA6 - ScriptFlags_Set40,
// tail kind 0x36 at state 0, al 1; else al 0.
extern "C" unsigned char __cdecl Area192_StepHook(long x, long z) {
    const U ux = static_cast<U>(x), uz = static_cast<U>(z);
    const auto cx = static_cast<short>(ux >> 16), cz = static_cast<short>(uz >> 16);
    const auto cx1 = static_cast<short>(cx + 1), cz1 = static_cast<short>(cz + 1);
    unsigned char hits = 0;
    if (AH_CALL(AreaMap_ByteAt)(cx, cz) == 0xA6) hits = 1;
    if ((ux & 0xFFFF) != 0 && AH_CALL(AreaMap_ByteAt)(cx1, cz) == 0xA6) ++hits;
    if ((uz & 0xFFFF) != 0 && AH_CALL(AreaMap_ByteAt)(cx, cz1) == 0xA6) ++hits;
    if ((ux & 0xFFFF) != 0 && (uz & 0xFFFF) != 0 && AH_CALL(AreaMap_ByteAt)(cx1, cz1) == 0xA6) ++hits;
    if (hits == 0) return 0;
    ArmTail(0x36, 0);
    return 1;
}

// original 0x42C0A0 (called by chapter 14's objects 0..4, 0x5677DD ..
// 0x5678DD, and chapter 15's talk, 0x56AEC6, in area 0xC0): the message the
// member `who` says. i = who's place among Area192_TalkWho's five (5 none);
// the rank the byte 0x90405E capped at 8; the level the character record's
// byte +0x1E (the record Character_IndexById maps who to). Cond_Flags row
// 14's flag 0xA set: Area192_TalkMessageB(i * 9, who, rank, level). Else the
// index i * 9 plus 8 (level 8 or more), 7 (level 5..7 and rank below 7) or
// Area192_TalkSteps[rank]; the chapter's flag 2 picks Area192_TalkMessagesF
// over Area192_TalkMessages at that index (a byte, unchecked: i = 5 reads on
// into the next table).
extern "C" unsigned short __cdecl Area192_TalkMessage(unsigned char who) {
    unsigned char i = 0;
    while (B(::bof3::Img(Area192_TalkWho_addr) + i) != who) {
        if (++i >= at::kArea192TalkWhoCount) break;
    }
    auto index = static_cast<unsigned char>(i * 9);
    unsigned char rank = B(::bof3::Img(Cond_Flags_addr) + 0xce);
    if (rank > 8) rank = 8;
    const unsigned char record = B(::bof3::Img(Character_IndexById_addr) + who);
    const unsigned char level = B(::bof3::Img(Field_ActorStates_addr) + 0xe + record * at::kCharStride);
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x70), 0xA) != 0)
        return AH_CALL(Area192_TalkMessageB)(index, who, rank, level);
    if (level >= 8)
        index = static_cast<unsigned char>(index + 8);
    else if (level > 4 && rank < 7)
        index = static_cast<unsigned char>(index + 7);
    else
        index = static_cast<unsigned char>(index + B(::bof3::Img(Area192_TalkSteps_addr) + rank));
    if (AH_CALL(Flags_Test)(Ptr(::bof3::Img(Chapter_FlagRow_addr)), 2) != 0) return B(::bof3::Img(Area192_TalkMessagesF_addr) + index);
    return B(::bof3::Img(Area192_TalkMessages_addr) + index);
}

// original 0x42C1C0 (called by Area192_TalkMessage only): the index (a byte)
// plus Area192_TalkStepsB[rank] for a level below 5, else plus 8; the message
// Area192_TalkMessagesB at it. `who` is passed and not read.
extern "C" unsigned short __cdecl Area192_TalkMessageB(unsigned base, unsigned who, unsigned rank, unsigned level) {
    (void)who;
    auto index = static_cast<unsigned char>(base);
    if (static_cast<unsigned char>(level) < 5)
        index = static_cast<unsigned char>(index + B(::bof3::Img(Area192_TalkStepsB_addr) + (rank & 0xFF)));
    else
        index = static_cast<unsigned char>(index + 8);
    return B(::bof3::Img(Area192_TalkMessagesB_addr) + index);
}

// original 0x42C200 (area 192's init, its descriptor +0x40; Capcom's own jmp
// over eleven nops at the entry, the body at 0x42C210): the level byte of
// character record 0 read first; story flag 0x77 set: nothing. Else a set
// 0..2 - Cond_Flags row 14's flag 0xA clear: 2 for a level of 9 or more, 1
// for 5..8, else whether the byte 0x90405E is 5 or more; set: 2 for a level
// of 5 or more, else the same test - and six times: Sprite_FindFree into the
// scratch word 0x903850; a slot: EventOp_0x on op k of the set's block
// (Area192_PlaceOps[set] + k * 0x11, the pointer read each time).
extern "C" void __cdecl Area192_Init(void) {
    const unsigned char level = B(::bof3::Img(Field_ActorStates_addr) + 0xe);
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x77) != 0) return;
    unsigned char set;
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x70), 0xA) == 0) {
        if (level >= 9)
            set = 2;
        else if (level >= 5)
            set = 1;
        else
            set = B(::bof3::Img(Cond_Flags_addr) + 0xce) >= 5 ? 1 : 0;
    } else {
        if (level >= 5)
            set = 2;
        else
            set = B(::bof3::Img(Cond_Flags_addr) + 0xce) >= 5 ? 1 : 0;
    }
    for (unsigned k = 0; k < at::kPlaceOpCount; ++k) {
        const unsigned char slot = AH_CALL(Sprite_FindFree)();
        SetWord(Mem(::bof3::Img(at::kObjectIndex.v)), slot);
        if (slot == 0xFF) continue;
        const U ops = static_cast<U>(Long(Mem(::bof3::Img(Area192_PlaceOps_addr) + set * 4u)));
        AH_CALL(EventOp_0x)(Mem(ops + k * at::kPlaceOpStride));
    }
}

// original 0x42C2D0 (called by Area192_Tail54 state 3 and by area 191's code,
// 0x42B864): every character record with +0xB bit 0: HP (+0x18) = max HP
// (+0x20), AP (+0x1A) = max AP (+0x22), the status word +0x10 0; then for
// each of Field_MemberCount's members (a signed compare), the 0xA4 bytes of
// the character record Character_IndexById maps its id to copied over its
// party record's +0x80 block (dword by dword, forward).
extern "C" void __cdecl Area192_RestoreCharacters(void) {
    for (unsigned k = 0; k < at::kCharCount; ++k) {
        unsigned char* const r = Mem(::bof3::Img(CharacterRecords_addr) + k * at::kCharStride);
        if ((r[0xB] & 1) == 0) continue;
        rv::Store<unsigned>(rv::As<records::CharRecord>(r)->hp, rv::As<records::CharRecord>(r)->stats.max_hp);
        rv::Store<unsigned>(rv::As<records::CharRecord>(r)->ap, rv::As<records::CharRecord>(r)->stats.max_ap);
        rv::Store<unsigned>(rv::As<records::CharRecord>(r)->status, 0);
    }
    const int count = Field_MemberCount;
    for (int i = 0; i < count; ++i) {
        const unsigned char id = B(::bof3::Img(Cond_Flags_addr) + 0xd2 + static_cast<U>(i));
        const unsigned char record = B(::bof3::Img(Character_IndexById_addr) + id);
        unsigned char* const to = Mem(::bof3::Img(at::kParty80.v) + static_cast<U>(i) * at::kPartyStride);
        const unsigned char* const from = Mem(::bof3::Img(CharacterRecords_addr) + record * at::kCharStride);
        for (unsigned d = 0; d < at::kCharStride; d += 4) std::memcpy(to + d, from + d, 4);
    }
}

// original 0x42C350 (EffectKind18_States[78], .data 0x6541A4; in area 192's
// block): story flag 0x77 clear: nothing. Set: the word 0x90405C 0x1E0, the
// bytes 0x929EC1 and 0x9036D0 0, Field_StatusBits bit 0 cleared, then a tail
// jmp to Effect_Release.
extern "C" void __cdecl Area192_Effect18Release77(void) {
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x77) == 0) return;
    ResetWord1E0();
    Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & 0xFE);
    AH_CALL(Effect_Release)();
}

void Area192_Inject() {
    BOF3_INJECT(Area192_ChoiceFocusPair);
    BOF3_INJECT(Area192_ChoiceTailState);
    BOF3_INJECT(Area192_ChoiceArmTail54);
    BOF3_INJECT(Area192_Tail54);
    BOF3_INJECT(Area192_StepHook);
    BOF3_INJECT(Area192_TalkMessage);
    BOF3_INJECT(Area192_TalkMessageB);
    BOF3_INJECT(Area192_Init);
    BOF3_INJECT(Area192_RestoreCharacters);
    BOF3_INJECT(Area192_Effect18Release77);
}
