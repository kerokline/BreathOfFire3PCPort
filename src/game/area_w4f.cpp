// World 4's areas 192..199: the PSX's BIN/WORLD04/AREA192..199.EMI compiled
// into the exe at 0x42BD60..0x42D710 (Area_Descriptors entries 192..199; 194
// and 195 have no code here). Round ten, group AR4F: the band's 49 functions
// (the tool listed 48; area 198's effect kind 0xA6 has two states where it
// saw one), none ours before, each read to its last instruction with capstone
// (2026-09-28) and taken through the area harness (area_harness.h).
// docs/area_w4f.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through the areas' two two-state tables abort past the table
// (what follows is data, which the original would jump into), and area 193's
// choice 4 aborts on an answer past its three-row stack table (the original
// reads its own frame there); every other unchecked read stays inside .data
// or the field frame and is kept (docs/area_w4f.md section 6). Every call
// goes through the harness (AH_CALL / AH_AT), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies; the callees of the
// group's own are called the same way, so each function is fuzzed alone.
//
// 0x42D710, where the band ends, is not area code: game mode 9's frame step
// (0x517330, GameMode steps table 0x656AC8 entry 1) calls it, and it jumps by
// the byte 0x929F00 through the table 0x64ADAC (just past area 199's
// descriptor) into BATE.EMI's code. Not taken here.
#include "game/area_w4f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4f_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4f::at;
using area_harness::Handler;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
// Sprite_Objects record `k` (a byte index, unchecked as the originals').
unsigned char* ObjectAt(unsigned k) { return Mem(at::kSpriteObjects + k * at::kObjectStride); }

void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
// The answer as the originals' movsx reads it.
int AnswerS8() { return static_cast<signed char>(B(at::kChoiceAnswer)); }
unsigned char* StoryFlags() { return Mem(at::kStoryFlags); }
// The movement script's position (MoveScript_Object's word +0xA) moved by
// `v`, MoveScript_Object read at the store.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned index) {
    if (index >= at::kStateCount)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, at::kStateCount, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The signed quotient (Field_ActiveMember - Sprite_Objects) / 0xA4 the
// effect spawns keep (the originals' multiply by 0x63E7063F, sar 6, plus the
// sign bit: truncating toward 0).
unsigned char MemberIndex() {
    const auto d = static_cast<std::int32_t>(Key(Field_ActiveMember) - at::kSpriteObjects);
    return static_cast<unsigned char>(d / static_cast<std::int32_t>(at::kObjectStride));
}

using ObjectFn = void (__cdecl*)(unsigned char*);
using SlotFn = unsigned char (__cdecl*)(unsigned char*, const unsigned char*);
// 0x454A80(object): the object's Field_Slots scripts released.
void ReleaseSlots(unsigned char* object) { AH_AT(ObjectFn, at::kSlotsReleaseFor)(object); }
// 0x455290(object, script): a Field_Slots script started (its answer unread
// here).
void StartSlot(unsigned char* object, U script) { AH_AT(SlotFn, at::kSlotStart)(object, Mem(script)); }
// 0x441090(value, sign): the high word of a 16.16 value, rounded up when sign
// is not negative (ax).
unsigned short RoundHigh(std::int32_t value, std::int32_t sign) {
    return AH_AT(unsigned short (__cdecl*)(std::int32_t, std::int32_t), at::kRoundHigh)(value, sign);
}

// Area 192's choice 2 and area 193's choice 3 (0x42BD60, 0x42C430: one body,
// two tables): the focus object's dwords +0x18 and +0x1C the pair of bytes
// the s8 answer picks (table + answer * 2, unchecked), the message word 0xFFFF
// stored between the first read and the first store; the focus pointer and
// the answer read again for the second.
void FocusPair(U table) {
    int a = AnswerS8();
    unsigned char* focus = Ptr(at::kFocusObject);
    const unsigned first = B(table + static_cast<U>(a * 2));
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, static_cast<std::int32_t>(first));
    focus = Ptr(at::kFocusObject);
    a = AnswerS8();
    SetLong(focus + 0x1C, static_cast<std::int32_t>(B(table + 1 + static_cast<U>(a * 2))));
}

// A choice's message: the word the s8 answer picks (table + answer * 2).
unsigned short MessageByAnswer(U table) { return Word(Mem(table + static_cast<U>(AnswerS8() * 2))); }

// The member search of areas 196 and 197's handlers (nine bodies, one a
// handler, each over its own two tables; AR3F's Area143_MessageByMember is
// the same code): for each of the four keys in turn, each of
// Field_MemberCount's party records (the count read once, compared
// unsigned): the first record whose +0x89 is the key opens the key's message
// (Msg_OpenScript) and sets Field_Request 2. None: nothing. The records are
// not bounded by three (the count is).
void MessageByMember(U keys) {
    const U messages = keys + at::kKeysToMessages;
    const unsigned char count = Field_MemberCount;
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char key = B(keys + i);
        for (unsigned char j = 0; j < count; ++j) {
            if (B(at::kLeader89 + j * at::kPartyStride) != key) continue;
            AH_CALL(Msg_OpenScript)(Word(Mem(messages + i * 2)));
            Field_Request = 2;
            return;
        }
    }
}

// The tail disarmed: kind 0, state 0.
void Disarm() {
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
}
// The tail armed after ScriptFlags_Set40: kind `kind`, state `state`.
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = kind;
    B(at::kTailState) = state;
}
// The run armed after ScriptFlags_Set40: MoveScript_Var7 `var`, its step
// byte `step`.
void ArmRun(signed char var, unsigned char step) {
    AH_CALL(ScriptFlags_Set40)();
    MoveScript_Var7 = var;
    B(at::kVar7Step) = step;
}
// The four bytes after Party_Zenny's first word set together by area 192's
// tail and its EffectKind18 state: the word 0x1E0 and the two bytes 0.
void ResetWord1E0() {
    SetWord(Mem(at::kWord90405C), 0x1E0);
    B(at::kByte929EC1) = 0;
    B(at::kByte9036D0) = 0;
}

// ---- area 198's shakes and fades ----

// The s8 step a shake table gives for the running object's +0xA (its low
// nibble), Sprite_Current read at the call.
int ShakeStep(U table) { return static_cast<signed char>(B(table + (Sprite_Current[0xA] & 0xFu))); }
// The +0x34 / +0x38 moves of both shakes: x by step << 11, z by -step << 11
// (32-bit), Sprite_Current and the step read again for each.
void ShakeXZ(U table) {
    unsigned char* c = Sprite_Current;
    std::int32_t x = Long(c + 0x34);
    int s = ShakeStep(table);
    SetLong(c + 0x34, static_cast<std::int32_t>(static_cast<U>(x) + (static_cast<U>(s) << 11)));
    c = Sprite_Current;
    s = ShakeStep(table);
    SetLong(c + 0x38, static_cast<std::int32_t>(static_cast<U>(Long(c + 0x38)) + (static_cast<U>(-s) << 11)));
}

// Area 198's effect kind 0xA6's record `k` (8 bytes: s16 x scale, s16 z scale,
// u16 animation bank, u8 animation), by the running record's +6 (unchecked).
unsigned char* A6Record(unsigned k) { return Mem(at::kArea198EffectA6Records + k * 8u); }

}  // namespace

// ============================================================================
// Area 192 (descriptor 0x647F18; PSX 0x801F37C8)
// ============================================================================

// original 0x42BD60 (area 192 +0x34[2]): FocusPair(Area192_FocusPairs).
extern "C" void __cdecl Area192_ChoiceFocusPair(void) { FocusPair(at::kArea192FocusPairs); }

// original 0x42BDA0 (areas 191 and 192 +0x34[4]): message 0xFFFF; the tail's
// state 2 for the answer 0, 0xA for 1, 0x14 for any other (the answer read
// once, as a byte, before the message store).
extern "C" void __cdecl Area192_ChoiceTailState(void) {
    const unsigned char a = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    B(at::kTailState) = a == 0 ? 2 : a == 1 ? 0xA : 0x14;
}

// original 0x42BDD0 (area 192 +0x34[5]): message 0xFFFF; the answer 1:
// ScriptFlags_Set40, tail kind 0x36 at state 0x1E; 2: ScriptFlags_Set40,
// MoveScript_Var7 1, its step 0x14; other answers nothing more.
extern "C" void __cdecl Area192_ChoiceArmTail54(void) {
    const unsigned char a = B(at::kChoiceAnswer);
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
//   3: once MoveScript_WaitWordDA is 0: Draw_PassFlags 0, Sound_StopMusic,
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
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0:
        AH_CALL(Msg_OpenScript)(0x6D);
        Field_Request = 2;
        B(at::kTailState) = 1;
        return;
    case 2:
        if (Field_Request == 2) return;
        AH_CALL(Transition_Start)(0);
        B(at::kTailState) = 3;
        return;
    case 3:
        if (MoveScript_WaitWordDA != 0) return;
        Draw_PassFlags = 0;
        AH_CALL(Sound_StopMusic)();
        AH_CALL(Area192_RestoreCharacters)();
        AH_CALL(Sound_LoadStream)(0);
        B(at::kTailState) = 4;
        return;
    case 4: {
        if (AH_CALL(Sound_StreamDone)() == 0) return;
        AH_CALL(Flags_Set)(StoryFlags(), 0x82);
        const unsigned char bits = Field_StatusBits;
        B(at::kByte929EC1) = 0;
        B(at::kByte9036D0) = 0;
        SetWord(Mem(at::kWord90405C), 0x1E0);
        Field_StatusBits = static_cast<unsigned char>(bits & 0xFE);
        B(at::kTailState) = 0xA;
        return;
    }
    case 0xA: {
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        const std::int32_t z = Long(Mem(at::kReturnPoint + 4));
        const std::int32_t x = Long(Mem(at::kReturnPoint));
        const unsigned area = Word(Mem(at::kReturnArea));
        AH_CALL(Field_ChangeArea)(area, x, z, 4);
        B(at::kScriptFlags2) |= 0x40;
        B(at::kReturnByteA) = 0;
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
        B(at::kScriptFlags2) |= 0x40;
        B(at::kReturnByteA) = 0;
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
// byte +0x1E (the record MoveScript_EffectState maps who to). Cond_Flags row
// 14's flag 0xA set: Area192_TalkMessageB(i * 9, who, rank, level). Else the
// index i * 9 plus 8 (level 8 or more), 7 (level 5..7 and rank below 7) or
// Area192_TalkSteps[rank]; the chapter's flag 2 picks Area192_TalkMessagesF
// over Area192_TalkMessages at that index (a byte, unchecked: i = 5 reads on
// into the next table).
extern "C" unsigned short __cdecl Area192_TalkMessage(unsigned char who) {
    unsigned char i = 0;
    while (B(at::kArea192TalkWho + i) != who) {
        if (++i >= at::kArea192TalkWhoCount) break;
    }
    auto index = static_cast<unsigned char>(i * 9);
    unsigned char rank = B(at::kByte90405E);
    if (rank > 8) rank = 8;
    const unsigned char record = B(at::kMemberToRecord + who);
    const unsigned char level = B(at::kChar0Byte1E + record * at::kCharStride);
    if (AH_CALL(Flags_Test)(Mem(at::kCondRow14), 0xA) != 0)
        return AH_CALL(Area192_TalkMessageB)(index, who, rank, level);
    if (level >= 8)
        index = static_cast<unsigned char>(index + 8);
    else if (level > 4 && rank < 7)
        index = static_cast<unsigned char>(index + 7);
    else
        index = static_cast<unsigned char>(index + B(at::kArea192TalkSteps + rank));
    if (AH_CALL(Flags_Test)(Ptr(at::kFlagRow), 2) != 0) return B(at::kArea192TalkMessagesF + index);
    return B(at::kArea192TalkMessages + index);
}

// original 0x42C1C0 (called by Area192_TalkMessage only): the index (a byte)
// plus Area192_TalkStepsB[rank] for a level below 5, else plus 8; the message
// Area192_TalkMessagesB at it. `who` is passed and not read.
extern "C" unsigned short __cdecl Area192_TalkMessageB(unsigned base, unsigned who, unsigned rank, unsigned level) {
    (void)who;
    auto index = static_cast<unsigned char>(base);
    if (static_cast<unsigned char>(level) < 5)
        index = static_cast<unsigned char>(index + B(at::kArea192TalkStepsB + (rank & 0xFF)));
    else
        index = static_cast<unsigned char>(index + 8);
    return B(at::kArea192TalkMessagesB + index);
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
    const unsigned char level = B(at::kChar0Byte1E);
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x77) != 0) return;
    unsigned char set;
    if (AH_CALL(Flags_Test)(Mem(at::kCondRow14), 0xA) == 0) {
        if (level >= 9)
            set = 2;
        else if (level >= 5)
            set = 1;
        else
            set = B(at::kByte90405E) >= 5 ? 1 : 0;
    } else {
        if (level >= 5)
            set = 2;
        else
            set = B(at::kByte90405E) >= 5 ? 1 : 0;
    }
    for (unsigned k = 0; k < at::kPlaceOpCount; ++k) {
        const unsigned char slot = AH_CALL(Sprite_FindFree)();
        SetWord(Mem(at::kObjectIndex), slot);
        if (slot == 0xFF) continue;
        const U ops = static_cast<U>(Long(Mem(at::kArea192PlaceOps + set * 4u)));
        AH_CALL(EventOp_0x)(Mem(ops + k * at::kPlaceOpStride));
    }
}

// original 0x42C2D0 (called by Area192_Tail54 state 3 and by area 191's code,
// 0x42B864): every character record with +0xB bit 0: HP (+0x18) = max HP
// (+0x20), AP (+0x1A) = max AP (+0x22), the status word +0x10 0; then for
// each of Field_MemberCount's members (a signed compare), the 0xA4 bytes of
// the character record MoveScript_EffectState maps its id to copied over its
// party record's +0x80 block (dword by dword, forward).
extern "C" void __cdecl Area192_RestoreCharacters(void) {
    for (unsigned k = 0; k < at::kCharCount; ++k) {
        unsigned char* const r = Mem(at::kCharRecords + k * at::kCharStride);
        if ((r[0xB] & 1) == 0) continue;
        SetWord(r + 0x18, Word(r + 0x20));
        SetWord(r + 0x1A, Word(r + 0x22));
        SetWord(r + 0x10, 0);
    }
    const int count = Field_MemberCount;
    for (int i = 0; i < count; ++i) {
        const unsigned char id = B(at::kPartyList + static_cast<U>(i));
        const unsigned char record = B(at::kMemberToRecord + id);
        unsigned char* const to = Mem(at::kParty80 + static_cast<U>(i) * at::kPartyStride);
        const unsigned char* const from = Mem(at::kCharRecords + record * at::kCharStride);
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

// ============================================================================
// Area 193 (descriptor 0x648908; PSX 0x801F39B8)
// ============================================================================

// original 0x42C390 (area 193 +0x34[0]): the message word
// Area193_Messages0[s8 answer] (unchecked).
extern "C" void __cdecl Area193_ChoiceMessage(void) { SetMessage(MessageByAnswer(at::kArea193Messages0)); }

// original 0x42C3B0 (area 193 +0x34[1]): the message Area193_Messages1[s8
// answer]; the answer 1: ScriptFlags_Set40, MoveScript_Var7 1, its step 0.
extern "C" void __cdecl Area193_ChoiceRunOnYes(void) {
    const unsigned char a = B(at::kChoiceAnswer);
    SetMessage(MessageByAnswer(at::kArea193Messages1));
    if (a == 1) ArmRun(1, 0);
}

// original 0x42C3E0 (area 193 +0x34[2]): the message Area193_Messages2[s8
// answer]; the answer 1: n = Inventory_Count(0, 0x5B, 0), Inventory_Add(0,
// 0x5B, 0x10 - n as a byte - the original pushes a fourth word, 0, never
// read), Sound_PlayEffect(0x106).
extern "C" void __cdecl Area193_ChoiceFill5B(void) {
    const unsigned char a = B(at::kChoiceAnswer);
    SetMessage(MessageByAnswer(at::kArea193Messages2));
    if (a != 1) return;
    const unsigned char n = static_cast<unsigned char>(AH_CALL(Inventory_Count)(0, 0x5B, 0));
    AH_CALL(Inventory_Add)(0, 0x5B, static_cast<unsigned char>(0x10 - n));
    AH_CALL(Sound_PlayEffect)(0x106);
}

// original 0x42C430 (area 193 +0x34[3]): FocusPair(Area193_FocusPairs).
extern "C" void __cdecl Area193_ChoiceFocusPair(void) { FocusPair(at::kArea193FocusPairs); }

// original 0x42C470 (area 193 +0x34[4]): three rows of a stack table (a
// message word and a tail state byte each) by the s8 answer: 0 message 0x1D
// and state 0xC, 1 0x1E and 0xE, 2 0xFFFF and 0x10. The original reads past
// its twelve bytes for any other answer (its own frame, then the caller's);
// ours aborts there.
extern "C" void __cdecl Area193_ChoiceMessageState(void) {
    static const unsigned short kMessages[3] = {0x1D, 0x1E, 0xFFFF};
    static const unsigned char kStates[3] = {0xC, 0xE, 0x10};
    const int a = AnswerS8();
    if (a < 0 || a > 2) bof3::Fatal("Area193_ChoiceMessageState: answer %d is past its three-row stack table", a);
    SetMessage(kMessages[a]);
    B(at::kTailState) = kStates[a];
}

// original 0x42C4D0 (area 193 +0x34[5] = +0x3C[0]; PSX 0x801F2E38):
// Field_ActiveMember's +0x80 bit 0 cleared; the leader's +0x89 7:
// ScriptFlags_Set40 and Field_ActiveMember's (read again) word +0x8A + 1.
extern "C" void __cdecl Area193_MemberBit0Leader7(void) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(at::kLeader89) != 7) return;
    AH_CALL(ScriptFlags_Set40)();
    AddWord(Field_ActiveMember + 0x8A, 1);
}

// original 0x42C500 (Field_ModeTailKinds[57]): by the s8 state - 0xA through a
// jump table of 13 in its extent (the `ja` makes every other state nothing):
//   0xA: message 0x1C, Field_Request 2, state 0xB (0xB waits);
//   0xC: unless Field_Request is 2: story flag 0x77 cleared,
//      ScriptFlags_Clear40, Field_ChangeArea(0xBD, 0x12000000, 0x17FF0000, 2);
//      Field_ScriptFlags2 bit 6; the return point's byte +0xB 2, the word
//      0x90405C 0, 0x90405F 0xF0, 0x90405E, 0x929EC1, 0x9036D0 0;
//      Field_StatusBits (read after the call) bit 0 set; disarmed;
//   0xE: unless Field_Request is 2: flag 0x77 cleared, ScriptFlags_Clear40,
//      Field_ChangeArea(0x97, 0x160000, 0x150000, 3), Field_ScriptFlags2 bit
//      6, disarmed;
//   0x10: unless Field_Request is 2: ScriptFlags_Clear40, disarmed;
//   0x14: once Field_Request is 0: message 0x25 if Cond_Flags row 14's flag
//      3, else 0x20; Field_Request 2, state 0x15;
//   0x15: unless Field_Request is 2: Party_HealJoined, Sound_LoadStream(0),
//      state 0x16;
//   0x16: once Sound_StreamDone: counter 0 = 1, Music_Play(0x95, 0x10),
//      Transition_Start(1), Draw_PassFlags 0x1F, row 14's flag 3 and story
//      flag 0x8A cleared, disarmed.
extern "C" void __cdecl Area193_Tail57(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0xA:
        AH_CALL(Msg_OpenScript)(0x1C);
        Field_Request = 2;
        B(at::kTailState) = 0xB;
        return;
    case 0xC: {
        if (Field_Request == 2) return;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Field_ChangeArea)(0xBD, 0x12000000, 0x17FF0000, 2);
        const unsigned char bits = Field_StatusBits;
        B(at::kScriptFlags2) |= 0x40;
        B(at::kReturnByteB) = 2;
        SetWord(Mem(at::kWord90405C), 0);
        B(at::kByte90405F) = 0xF0;
        B(at::kByte90405E) = 0;
        B(at::kByte929EC1) = 0;
        B(at::kByte9036D0) = 0;
        Field_StatusBits = static_cast<unsigned char>(bits | 1);
        Disarm();
        return;
    }
    case 0xE:
        if (Field_Request == 2) return;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Field_ChangeArea)(0x97, 0x160000, 0x150000, 3);
        B(at::kScriptFlags2) |= 0x40;
        Disarm();
        return;
    case 0x10:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        Disarm();
        return;
    case 0x14:
        if (Field_Request != 0) return;
        AH_CALL(Msg_OpenScript)(AH_CALL(Flags_Test)(Mem(at::kCondRow14), 3) != 0 ? 0x25 : 0x20);
        Field_Request = 2;
        B(at::kTailState) = 0x15;
        return;
    case 0x15:
        if (Field_Request == 2) return;
        AH_CALL(Party_HealJoined)();
        AH_CALL(Sound_LoadStream)(0);
        B(at::kTailState) = 0x16;
        return;
    case 0x16:
        if (AH_CALL(Sound_StreamDone)() == 0) return;
        B(at::kCounter0) = 1;
        AH_CALL(Music_Play)(0x95, 0x10);
        AH_CALL(Transition_Start)(1);
        Draw_PassFlags = 0x1F;
        AH_CALL(Flags_Clear)(Mem(at::kCondRow14), 3);
        AH_CALL(Flags_Clear)(StoryFlags(), 0x8A);
        Disarm();
        return;
    default:
        return;
    }
}

// original 0x42C700 (Area_StepHook's case for area 0xC1): x or z (whole
// dwords) exactly 0x18000, then either exactly 0x338000 (in that order):
// ScriptFlags_Set40, tail kind 0x39 at state 0xA, al 1; else al 0.
extern "C" unsigned char __cdecl Area193_StepHook(long x, long z) {
    const U ux = static_cast<U>(x), uz = static_cast<U>(z);
    if (ux != 0x18000 && uz != 0x18000 && ux != 0x338000 && uz != 0x338000) return 0;
    ArmTail(0x39, 0xA);
    return 1;
}

// original 0x42C740 (area 193's init, its descriptor +0x40; PSX
// 0x801F31C0): story flag 0x8A set: Draw_PassFlags 0, tail kind 0x39 at
// state 0x14; clear: the pending-area byte 0x937F98 1.
extern "C" void __cdecl Area193_Init(void) {
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x8A) != 0) {
        Draw_PassFlags = 0;
        B(at::kTailKind) = 0x39;
        B(at::kTailState) = 0x14;
        return;
    }
    B(at::kPendingKind) = 1;
}

// ============================================================================
// Area 196 (descriptor 0x6490A8; PSX 0x801F42B0)
// ============================================================================

// original 0x42C780 (area 196 +0x3C[0]; PSX 0x801F3F90): MessageByMember over
// Area196_Keys0.
extern "C" void __cdecl Area196_MessageByMember0(void) { MessageByMember(at::kArea196Keys0); }
// original 0x42C810 (area 196 +0x3C[1]; PSX 0x801F4054): over Area196_Keys1.
extern "C" void __cdecl Area196_MessageByMember1(void) { MessageByMember(at::kArea196Keys1); }

// original 0x42C8A0 (area 196 +0x3C[2], PSX 0x801F4118; also area 148
// +0x34[2] = +0x3C[0], area 167 +0x34[14] = +0x3C[9], area 173 +0x3C[0]):
// Cond_ByteFE 1.
extern "C" void __cdecl Area196_SetCondFE(void) { Cond_ByteFE = 1; }

// ============================================================================
// Area 197 (descriptor 0x649780; PSX 0x801F49E4)
// ============================================================================

// originals 0x42C8B0, 0x42C940, 0x42C9D0, 0x42CA60, 0x42CAF0, 0x42CB80,
// 0x42CC10 (area 197 +0x3C[0..6]; PSX 0x801F39E0 .. 0x801F3E78):
// MessageByMember over Area197_Keys0..6 (0xC bytes apart).
extern "C" void __cdecl Area197_MessageByMember0(void) { MessageByMember(at::kArea197Keys0); }
extern "C" void __cdecl Area197_MessageByMember1(void) { MessageByMember(at::kArea197Keys0 + 1 * at::kKeysStride); }
extern "C" void __cdecl Area197_MessageByMember2(void) { MessageByMember(at::kArea197Keys0 + 2 * at::kKeysStride); }
extern "C" void __cdecl Area197_MessageByMember3(void) { MessageByMember(at::kArea197Keys0 + 3 * at::kKeysStride); }
extern "C" void __cdecl Area197_MessageByMember4(void) { MessageByMember(at::kArea197Keys0 + 4 * at::kKeysStride); }
extern "C" void __cdecl Area197_MessageByMember5(void) { MessageByMember(at::kArea197Keys0 + 5 * at::kKeysStride); }
extern "C" void __cdecl Area197_MessageByMember6(void) { MessageByMember(at::kArea197Keys0 + 6 * at::kKeysStride); }

// original 0x42CCA0 (area 197 +0x3C[7]; PSX 0x801F3F3C): Area197_ShakeStates
// by the running object's +4 (unchecked there; ours aborts past its two).
extern "C" void __cdecl Area197_RunShake(void) {
    StateEntry("Area197_RunShake", at::kArea197ShakeStates, Sprite_Current[4])();
}

// original 0x42CCC0 (Area197_ShakeStates[0]): the running object's +0xA 4 and
// +4 1 (Sprite_Current read for each); Field_ActiveMember's word +0x8A - 2.
extern "C" void __cdecl Area197_ShakeStart(void) {
    Sprite_Current[0xA] = 4;
    Sprite_Current[4] = 1;
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x42CCF0 (Area197_ShakeStates[1]): the running object's +0xA 0:
// its +4 0. Else x += step << 11 and z -= step << 11 (Area197_ShakeSteps by
// +0xA's low nibble, s8), +0xA - 1, Field_ActiveMember's word +0x8A - 2.
extern "C" void __cdecl Area197_ShakeStep(void) {
    unsigned char* const c = Sprite_Current;
    if (c[0xA] == 0) {
        c[4] = 0;
        return;
    }
    ShakeXZ(at::kArea197ShakeSteps);
    Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] - 1);
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x42CD60 (area 197 +0x3C[8]; PSX 0x801F4050): an effect of kind
// 0x92 (Effect_FindFree; none: nothing): +0 1, +5 0x92, words +0x2E 0 and
// +0x30 -0x1E, +0x29 6, +0xB the active member's index in Sprite_Objects
// (read after the search; a signed quotient, as a byte).
extern "C" void __cdecl Area197_SpawnEffect92(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    const unsigned char who = MemberIndex();
    unsigned char* const r = EffectAt(slot);
    r[0] = 1;
    r[5] = 0x92;
    SetWord(r + 0x2E, 0);
    SetWord(r + 0x30, 0xFFE2);
    r[0x29] = 6;
    r[0xB] = who;
}

// original 0x42CDD0 (Field_ModeTailKinds[51]): by the s8 state - 0:
// Party_DropIn(0), state 1; 1: once counter 3 is 0x24: story flag 0x55 set,
// Field_ChangeArea(0xAA, 0x60000, 0x3F0000, 0x82), state 0x1E. Any other state
// nothing.
extern "C" void __cdecl Area197_Tail51(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    if (state == 0) {
        AH_CALL(Party_DropIn)(0);
        B(at::kTailState) = 1;
        return;
    }
    if (state != 1 || B(at::kCounter3) != 0x24) return;
    AH_CALL(Flags_Set)(StoryFlags(), 0x55);
    AH_CALL(Field_ChangeArea)(0xAA, 0x60000, 0x3F0000, 0x82);
    B(at::kTailState) = 0x1E;
}

// original 0x42CE30 (Area_StepHook's case for area 0xC5): only with
// Cond_ByteFD 4. A: z exactly 0x708000 or 0x738000 with x's high word 3..5;
// or x exactly 0x28000 or 0x58000 with z's high word 0x71..0x73 (the 16-bit
// compares of the high words). B: x exactly 0xD8000 with z's high word 0x71
// or 0x72 and key item 0xD not held. A with key item 0xD held:
// ScriptFlags_Set40, tail kind 0x33 at state 0, al 1; A without it, and B:
// ScriptFlags_Set40, MoveScript_Var7 5, its step 0x14, al 1. Else al 0.
extern "C" unsigned char __cdecl Area197_StepHook(long x, long z) {
    if (Cond_ByteFD != 4) return 0;
    const U ux = static_cast<U>(x), uz = static_cast<U>(z);
    const auto xh = static_cast<std::uint16_t>(ux >> 16), zh = static_cast<std::uint16_t>(uz >> 16);
    bool a = false;
    if ((uz == 0x708000 || uz == 0x738000) && static_cast<std::uint16_t>(xh - 3) < 3) a = true;
    if (!a) {
        if (ux == 0x28000 || ux == 0x58000) {
            if (static_cast<std::uint16_t>(zh - 0x71) >= 3) return 0;
            a = true;
        } else if (ux == 0xD8000) {
            if (static_cast<std::uint16_t>(zh - 0x71) >= 2) return 0;
            if (AH_CALL(KeyItem_Has)(0xD) != 0) return 0;
        } else {
            return 0;
        }
    }
    if (a && AH_CALL(KeyItem_Has)(0xD) != 0) {
        ArmTail(0x33, 0);
        return 1;
    }
    ArmRun(5, 0x14);
    return 1;
}

// ============================================================================
// Area 198 (descriptor 0x649CC0; PSX 0x801F4294)
// ============================================================================

// original 0x42CF00 (area 198 +0x3C[0]; PSX 0x801F2E00): the running
// object's Field_Slots scripts released (0x454A80), Area198_SlotScript0
// started for it (0x455290), its +0x2A 1, Sprite_SetAnimation(0) - Sprite_Current
// read for each. Area 85's 0x40F960 without the +0x2A store.
extern "C" void __cdecl Area198_StartSlotScript0(void) {
    ReleaseSlots(Sprite_Current);
    StartSlot(Sprite_Current, at::kArea198SlotScript0);
    Sprite_Current[0x2A] = 1;
    AH_CALL(Sprite_SetAnimation)(0);
}

// original 0x42CF40 (area 198 +0x3C[4]; PSX 0x801F2FF8): the running object's
// word +0x3E - 0x10. The script object's byte +2 below 0x80 with bits 0..1
// clear: Sprite_SetAnimationAt(Area198_SinkAnims[bits 2..3], the object's word
// +0x58 - 2) and the script object read again. The script object's +2 + 1.
// The word +0x3E below -0x280 (signed): each of +0x5D, +0x5E, +0x5F - 1 unless
// it is 0x80. The word exactly 0xF600: counter 0 + 1. The word above -0xC80
// (signed): the script position - 2; else the script object's +2 0.
extern "C" void __cdecl Area198_SinkFade(void) {
    AddWord(Sprite_Current + 0x3E, -0x10);
    unsigned char* object = MoveScript_Object;
    const unsigned char v = object[2];
    if (v < 0x80 && (v & 3) == 0) {
        unsigned char* const c = Sprite_Current;
        const unsigned anim = B(at::kArea198SinkAnims + ((v >> 2) & 3u));
        AH_CALL(Sprite_SetAnimationAt)(static_cast<unsigned char>(anim), static_cast<unsigned short>(Word(c + 0x58) - 2));
        object = MoveScript_Object;
    }
    object[2] = static_cast<unsigned char>(object[2] + 1);
    unsigned char* c = Sprite_Current;
    if (static_cast<std::int16_t>(Word(c + 0x3E)) < -0x280) {
        for (unsigned off = 0x5D; off <= 0x5F; ++off) {
            if (c[off] == 0x80) continue;
            c[off] = static_cast<unsigned char>(c[off] - 1);
            c = Sprite_Current;
        }
    }
    if (Word(c + 0x3E) == 0xF600) B(at::kCounter0) = static_cast<unsigned char>(B(at::kCounter0) + 1);
    if (static_cast<std::int16_t>(Word(c + 0x3E)) > static_cast<std::int16_t>(0xF380)) {
        ScriptStep(-2);
        return;
    }
    MoveScript_Object[2] = 0;
}

// original 0x42D000 (area 198 +0x3C[5]; PSX 0x801F3198): three effects of
// kind 0xA6 in turn (Effect_FindFree; none: that one skipped): +0 1, +5 0xA6,
// +6 1, 0 and 2, +7 the active member's index in Sprite_Objects (read after
// each search; a signed quotient, as a byte).
extern "C" void __cdecl Area198_SpawnEffectsA6(void) {
    static const unsigned char kRecords[3] = {1, 0, 2};
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot == 0xFF) continue;
        const unsigned char who = MemberIndex();
        unsigned char* const r = EffectAt(slot);
        r[0] = 1;
        r[5] = 0xA6;
        r[6] = kRecords[k];
        r[7] = who;
    }
}

// original 0x42D100 (area 198 +0x3C[6]; PSX 0x801F3384): x += step << 11,
// z -= step << 11, the word +0x3E += step << 4 (Area198_ShakeSteps by the
// running object's +0xA low nibble, s8, read again for each), +0xA - 1, the
// script position - 2. Counter 1 not 0: an effect of kind 0xAC (+0 1, +5
// 0xAC; none: no effect), counter 1 0, the object's +0x5F, +0x5E, +0x5D 0xC0.
// Counter 1 0: each of +0x5D, +0x5E, +0x5F + 8 unless it is 0.
extern "C" void __cdecl Area198_Shake(void) {
    ShakeXZ(at::kArea198ShakeSteps);
    {
        unsigned char* const c = Sprite_Current;
        const int s = ShakeStep(at::kArea198ShakeSteps);
        SetWord(c + 0x3E, static_cast<unsigned>(Word(c + 0x3E) + (static_cast<unsigned>(s) << 4)));
    }
    Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] - 1);
    ScriptStep(-2);
    if (B(at::kCounter1) != 0) {
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot != 0xFF) {
            EffectAt(slot)[0] = 1;
            EffectAt(slot)[5] = 0xAC;
        }
        unsigned char* const c = Sprite_Current;
        B(at::kCounter1) = 0;
        c[0x5F] = 0xC0;
        Sprite_Current[0x5E] = 0xC0;
        Sprite_Current[0x5D] = 0xC0;
        return;
    }
    unsigned char* c = Sprite_Current;
    for (unsigned off = 0x5D; off <= 0x5F; ++off) {
        if (c[off] == 0) continue;
        c[off] = static_cast<unsigned char>(c[off] + 8);
        c = Sprite_Current;
    }
}

// original 0x42D200 (area 198 +0x3C[7]; PSX 0x801F3524): d = (0x190000 - the
// running object's x) >> 15 (32-bit, arithmetic); not 0: the script object's
// +7 = abs(d) as a byte, the running object's +8 (its direction) 3,
// MoveCmd_Move(the script object, +8) - each pointer read again.
extern "C" void __cdecl Area198_WalkToX190(void) {
    const auto d = static_cast<std::int32_t>(0x190000u - static_cast<U>(Long(Sprite_Current + 0x34))) >> 15;
    if (d == 0) return;
    MoveScript_Object[7] = static_cast<unsigned char>(d < 0 ? 0u - static_cast<U>(d) : static_cast<U>(d));
    Sprite_Current[8] = 3;
    unsigned char* const c = Sprite_Current;
    unsigned char* const object = MoveScript_Object;
    AH_CALL(MoveCmd_Move)(object, c[8]);
}

// original 0x42D250 (area 198 +0x3C[8], PSX 0x801F35A0; also area 174
// +0x3C[1]): Field_Request 5: the running object's Field_Slots scripts
// released (0x454A80). Else the script position - 2 (it waits).
extern "C" void __cdecl Area198_ReleaseOnRequest5(void) {
    if (Field_Request == 5) {
        ReleaseSlots(Sprite_Current);
        return;
    }
    ScriptStep(-2);
}

// original 0x42D280 (area 198 +0x3C[9]; PSX 0x801F35FC): every eighth frame
// (Frame_Counter's low 3 bits 0) with counter 0 above 2: the running object's
// +0xB with bits 0..2 clear - Sound_PlayEffect(0) (the original computes the
// id as `((b & 8) + 0x203) == 0` through neg / sbb / inc, which is 0 for every
// b); Area198_SpawnDrop, and again with counter 0 (read again) above 3. The
// script position - 2 every time.
extern "C" void __cdecl Area198_SpawnDrops(void) {
    if ((Frame_Counter & 7) == 0 && B(at::kCounter0) > 2) {
        if ((Sprite_Current[0xB] & 7) == 0) AH_CALL(Sound_PlayEffect)(0);
        AH_CALL(Area198_SpawnDrop)();
        if (B(at::kCounter0) > 3) AH_CALL(Area198_SpawnDrop)();
    }
    ScriptStep(-2);
}

// original 0x42D2E0 (called by Area198_SpawnDrops): Sprite_FindFree into the
// scratch word 0x903850; a slot: EventOp_0x(Area198_DropOp), then the new
// Sprite_Current's dwords +0x10, +0xC 0 and +0x14 8, +9 0x10, the word +0x3E
// (Rand() % 30 - 6) << 5, +0x2B 1, and a cell (a, b) drawn as a = Rand() % 14
// + 0x10, b = Rand() % 25 + 0xB (bytes; the CRT's remainders, signed) again
// while a is below 0x1A and b below 0x15: x = a << 16, z = b << 16
// (Sprite_Current read for each store). Then Sprite_Current put back as it was
// on entry and its +0xB + 1.
extern "C" void __cdecl Area198_SpawnDrop(void) {
    unsigned char* const self = Sprite_Current;
    const unsigned char slot = AH_CALL(Sprite_FindFree)();
    SetWord(Mem(at::kObjectIndex), slot);
    if (slot != 0xFF) {
        AH_CALL(EventOp_0x)(Mem(at::kArea198DropOp));
        SetLong(Sprite_Current + 0x10, 0);
        SetLong(Sprite_Current + 0xC, 0);
        SetLong(Sprite_Current + 0x14, 8);
        Sprite_Current[9] = 0x10;
        const int r = AH_CALL(Rand)();
        SetWord(Sprite_Current + 0x3E, static_cast<unsigned>((r % 30 - 6) << 5));
        Sprite_Current[0x2B] = 1;
        unsigned char a, b;
        do {
            a = static_cast<unsigned char>(AH_CALL(Rand)() % 14 + 0x10);
            b = static_cast<unsigned char>(AH_CALL(Rand)() % 25 + 0xB);
        } while (static_cast<int>(a) - 0x15 < 5 && static_cast<int>(b) - 0x10 < 5);
        SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(static_cast<U>(a) << 16));
        SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(static_cast<U>(b) << 16));
    }
    Sprite_Current = self;
    self[0xB] = static_cast<unsigned char>(self[0xB] + 1);
}

// original 0x42D3F0 (area 198 +0x3C[10]; PSX 0x801F3850): an effect of kind
// 0xA7 (Effect_FindFree's answer kept at the running object's +0xB and read
// back from there for each store): none - the script position - 2 (it waits);
// else +0 1, +5 0xA7, and the record's x, z, y the object's (y + 0x1000000).
extern "C" void __cdecl Area198_SpawnEffectA7(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = slot;
    unsigned char* const c = Sprite_Current;
    if (c[0xB] == 0xFF) {
        ScriptStep(-2);
        return;
    }
    EffectAt(c[0xB])[0] = 1;
    EffectAt(c[0xB])[5] = 0xA7;
    unsigned char* const r34 = EffectAt(c[0xB]);
    SetLong(r34 + 0x34, Long(c + 0x34));
    const std::int32_t z = Long(c + 0x38);
    SetLong(EffectAt(c[0xB]) + 0x38, z);
    const std::int32_t y = Long(c + 0x3C);
    SetLong(EffectAt(c[0xB]) + 0x3C, static_cast<std::int32_t>(static_cast<U>(y) + 0x1000000u));
}

// original 0x42D470 (area 198 +0x3C[11]; PSX 0x801F39B0): the running
// object's Field_Slots scripts released, Area198_SlotScript11 started for it,
// its +0x2A 1, its +0 bit 5, Sprite_SetAnimation(0) - Sprite_Current read for
// each.
extern "C" void __cdecl Area198_StartSlotScript11(void) {
    ReleaseSlots(Sprite_Current);
    StartSlot(Sprite_Current, at::kArea198SlotScript11);
    Sprite_Current[0x2A] = 1;
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x20);
    AH_CALL(Sprite_SetAnimation)(0);
}

// original 0x42D4B0 (Effect_KindHandlers[0xA6], .data 0x6555E8; the kind area
// 198's handler 5 spawns): Area198_EffectA6States by the running record's +1
// (unchecked there; ours aborts past its two).
extern "C" void __cdecl Area198_EffectA6Run(void) {
    StateEntry("Area198_EffectA6Run", at::kArea198EffectA6States, Sprite_Current[1])();
}

// original 0x42D4D0 (Area198_EffectA6States[0]): the record's x, z, y field
// object +7's (Sprite_Objects, the byte unchecked; Sprite_Current and +7 read
// again for each); Sprite_SetAnimationBank(the bank of record +6); +0x48 1,
// +0x24 0, +0x2A 0; Sprite_SetAnimation(the animation of record +6); +1 1;
// then a tail jmp into state 1 (0x42D580, the next byte).
extern "C" void __cdecl Area198_EffectA6Start(void) {
    SetLong(Sprite_Current + 0x34, Long(ObjectAt(Sprite_Current[7]) + 0x34));
    SetLong(Sprite_Current + 0x38, Long(ObjectAt(Sprite_Current[7]) + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(ObjectAt(Sprite_Current[7]) + 0x3C));
    AH_CALL(Sprite_SetAnimationBank)(Word(A6Record(Sprite_Current[6]) + 4));
    Sprite_Current[0x48] = 1;
    Sprite_Current[0x24] = 0;
    Sprite_Current[0x2A] = 0;
    AH_CALL(Sprite_SetAnimation)(A6Record(Sprite_Current[6])[6]);
    Sprite_Current[1] = 1;
    AH_CALL(Area198_EffectA6Follow)();
}

// original 0x42D580 (Area198_EffectA6States[1]; also reached by state 0's
// tail jmp): o = field object +7 (read once); the record's +0 o's +0;
// Sprite_ScriptTick, Sprite_QueueOverlay; then o's +0x5C..+0x5F, +0x27, +0x48,
// dwords +0x40, +0x44, words +0x2E, +0x30, +0x32 and dword +0x60 copied to the
// record (Sprite_Current read for each), +0x29 6. The record's +0x48 (read
// back) not 0: its words +0x2E / +0x30 += 0x441090(record +6's s16 x / z
// scale * the dword +0x40 / +0x44, that dword); 0: += 0x441090(scale << 16,
// 0x10000). Each word's address taken before its call, added to after.
extern "C" void __cdecl Area198_EffectA6Follow(void) {
    unsigned char* const o = ObjectAt(Sprite_Current[7]);
    Sprite_Current[0] = o[0];
    AH_CALL(Sprite_ScriptTick)();
    AH_CALL(Sprite_QueueOverlay)();
    Sprite_Current[0x5C] = o[0x5C];
    Sprite_Current[0x5D] = o[0x5D];
    Sprite_Current[0x5E] = o[0x5E];
    Sprite_Current[0x5F] = o[0x5F];
    Sprite_Current[0x27] = o[0x27];
    Sprite_Current[0x48] = o[0x48];
    SetLong(Sprite_Current + 0x40, Long(o + 0x40));
    SetLong(Sprite_Current + 0x44, Long(o + 0x44));
    SetWord(Sprite_Current + 0x2E, Word(o + 0x2E));
    SetWord(Sprite_Current + 0x30, Word(o + 0x30));
    SetWord(Sprite_Current + 0x32, Word(o + 0x32));
    SetLong(Sprite_Current + 0x60, Long(o + 0x60));
    Sprite_Current[0x29] = 6;
    unsigned char* c = Sprite_Current;
    if (c[0x48] != 0) {
        std::int32_t scale = Long(c + 0x40);
        unsigned char* w = c + 0x2E;
        std::int32_t v = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int16_t>(Word(A6Record(c[6])))) * static_cast<U>(scale));
        unsigned short r = RoundHigh(v, scale);
        SetWord(w, static_cast<unsigned>(Word(w) + r));
        c = Sprite_Current;
        scale = Long(c + 0x44);
        w = c + 0x30;
        v = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int16_t>(Word(A6Record(c[6]) + 2))) * static_cast<U>(scale));
        r = RoundHigh(v, scale);
        SetWord(w, static_cast<unsigned>(Word(w) + r));
        return;
    }
    unsigned char* w = c + 0x2E;
    unsigned short r = RoundHigh(static_cast<std::int32_t>(static_cast<U>(static_cast<std::int16_t>(Word(A6Record(c[6])))) << 16), 0x10000);
    SetWord(w, static_cast<unsigned>(Word(w) + r));
    c = Sprite_Current;
    w = c + 0x30;
    r = RoundHigh(static_cast<std::int32_t>(static_cast<U>(static_cast<std::int16_t>(Word(A6Record(c[6]) + 2))) << 16), 0x10000);
    SetWord(w, static_cast<unsigned>(Word(w) + r));
}

// ============================================================================
// Area 199 (descriptor 0x64AD68)
// ============================================================================

// original 0x42D6F0 (area 199 +0x34[0]): message 0xFFFF; MoveScript_Var7's
// step byte 1 for any answer but 0, else 0.
extern "C" void __cdecl Area199_ChoiceStepIfAnswer(void) {
    const unsigned char a = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    B(at::kVar7Step) = a != 0 ? 1 : 0;
}

void AreaW4f_Inject() {
    if (bof3::WantsShadow("area_w4f")) area_w4f::SelfTest();
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
    BOF3_INJECT(Area193_ChoiceMessage);
    BOF3_INJECT(Area193_ChoiceRunOnYes);
    BOF3_INJECT(Area193_ChoiceFill5B);
    BOF3_INJECT(Area193_ChoiceFocusPair);
    BOF3_INJECT(Area193_ChoiceMessageState);
    BOF3_INJECT(Area193_MemberBit0Leader7);
    BOF3_INJECT(Area193_Tail57);
    BOF3_INJECT(Area193_StepHook);
    BOF3_INJECT(Area193_Init);
    BOF3_INJECT(Area196_MessageByMember0);
    BOF3_INJECT(Area196_MessageByMember1);
    BOF3_INJECT(Area196_SetCondFE);
    BOF3_INJECT(Area197_MessageByMember0);
    BOF3_INJECT(Area197_MessageByMember1);
    BOF3_INJECT(Area197_MessageByMember2);
    BOF3_INJECT(Area197_MessageByMember3);
    BOF3_INJECT(Area197_MessageByMember4);
    BOF3_INJECT(Area197_MessageByMember5);
    BOF3_INJECT(Area197_MessageByMember6);
    BOF3_INJECT(Area197_RunShake);
    BOF3_INJECT(Area197_ShakeStart);
    BOF3_INJECT(Area197_ShakeStep);
    BOF3_INJECT(Area197_SpawnEffect92);
    BOF3_INJECT(Area197_Tail51);
    BOF3_INJECT(Area197_StepHook);
    BOF3_INJECT(Area198_StartSlotScript0);
    BOF3_INJECT(Area198_SinkFade);
    BOF3_INJECT(Area198_SpawnEffectsA6);
    BOF3_INJECT(Area198_Shake);
    BOF3_INJECT(Area198_WalkToX190);
    BOF3_INJECT(Area198_ReleaseOnRequest5);
    BOF3_INJECT(Area198_SpawnDrops);
    BOF3_INJECT(Area198_SpawnDrop);
    BOF3_INJECT(Area198_SpawnEffectA7);
    BOF3_INJECT(Area198_StartSlotScript11);
    BOF3_INJECT(Area198_EffectA6Run);
    BOF3_INJECT(Area198_EffectA6Start);
    BOF3_INJECT(Area198_EffectA6Follow);
    BOF3_INJECT(Area199_ChoiceStepIfAnswer);
}
