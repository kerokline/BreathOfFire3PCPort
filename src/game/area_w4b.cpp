// World 4's areas 168..172: the PSX's BIN/WORLD04/AREA168..172.EMI compiled
// into the exe at 0x426560..0x428450 (Area_Descriptors entries 168..172).
// Round ten, group AR4B: the band's 56 functions, none ours before, each read
// to its last instruction with capstone (2026-09-28) and taken through the
// area harness (area_harness.h). docs/area_w4b.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept where they stay
// in .data; the three dispatchers through area 172's state tables abort past
// the entries that are code (what follows is data, which the original would
// jump into; docs/area_w4b.md section 6). Every call goes through the harness
// (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for ours as
// for the originals' copies; the callees of the group's own are called the
// same way, so each function is fuzzed alone.
#include "game/area_w4b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4b_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4b::at;
using area_harness::Handler;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
void SetFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }
// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
// ObjTrio record `m` (0x14C bytes; not checked, as the originals' index).
unsigned char* PartyRecord(unsigned m) { return Mem(at::kLeader + m * at::kPartyStride); }
// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }
// A word in [lo, lo + 3), compared as the originals' `sub; cmp r16, 3; jb`.
bool In3(std::uint16_t v, unsigned lo) { return static_cast<std::uint16_t>(v - lo) < 3; }

void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
unsigned char* Story() { return Mem(at::kStoryFlags); }
unsigned char Answer() { return B(at::kChoiceAnswer); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the entries that are code (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past the %u code entries of its table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The tail armed: ScriptFlags_Set40, then kind and state.
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = kind;
    B(at::kTailState) = state;
}
// The tail ended: kind and state 0.
void EndTail() {
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
}
// The tail disarmed: ScriptFlags_Clear40, kind and state 0.
void Disarm() {
    AH_CALL(ScriptFlags_Clear40)();
    EndTail();
}
// The map byte setter, as the originals pass it: x and z, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// A choice that arms a tail on answer 0 (areas 168, 170): the answer read,
// the message 0xFFFF, then for 0 ScriptFlags_Set40 and the tail.
void ChoiceArm(unsigned char kind, unsigned char state) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0) ArmTail(kind, state);
}
// Engine tail kind 10 armed at state 5 with sub-kind 0xFF (area 168's choice
// 1 on answer 3, area 171's choice 0 on answer 0).
void ArmTail10() {
    ArmTail(0xA, 5);
    B(at::kTailSub) = 0xFF;
}
// Area 168's choices 0 / 3 and 4: answer 0 asks for key item `item` - held,
// message 6 and `bit` set in Cond_ByteFE (read before the message store);
// not held, message 7. Any other answer: message 0xFFFF.
void ChoiceKeyItem(unsigned item, unsigned char bit) {
    if (Answer() != 0) {
        SetMessage(0xFFFF);
        return;
    }
    if (AH_CALL(KeyItem_Has)(item) != 0) {
        const unsigned char fe = Cond_ByteFE;
        SetMessage(6);
        Cond_ByteFE = static_cast<unsigned char>(fe | bit);
        return;
    }
    SetMessage(7);
}

// Areas 169 and 171's rectangle search (0x4269F0, 0x4278B0; one body over a
// Rects each): party record member & 0xFF's x and z (+0x34, +0x38) against
// each rectangle's (x0, z0, x1, z1) bytes << 16, signed, inclusive; the first
// that holds both: its index. None: 0xFF.
unsigned char MemberRect(const at::Rects& t, unsigned member) {
    const unsigned char* const record = PartyRecord(member & 0xFF);
    const std::int32_t x = Long(record + 0x34);
    const std::int32_t z = Long(record + 0x38);
    for (unsigned i = 0; i < t.count; ++i) {
        const unsigned char* const e = Mem(t.rects + i * at::kRectStride);
        if (x < static_cast<std::int32_t>(static_cast<U>(e[0]) << 16)) continue;
        if (x > static_cast<std::int32_t>(static_cast<U>(e[2]) << 16)) continue;
        if (z < static_cast<std::int32_t>(static_cast<U>(e[1]) << 16)) continue;
        if (z <= static_cast<std::int32_t>(static_cast<U>(e[3]) << 16)) return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// Areas 169 and 171's member frame (0x426810, 0x4276D0; called by the
// engine's 0x46D780 every field frame in the area, one body over a Rects
// each). Area 117's frame (Area117_MembersFrame, AR3A) without its story-flag
// turn of the facing and over five-byte rectangles. Field_ScriptFlags bit 13
// cleared; for each member m below Field_MemberCount (read again after each
// member; m a byte), with the bit b = 1 << (m & 31) and its low byte b8 (0
// for m & 31 of 8 and more, as the original's 8-bit shift): Field_State and
// Sprite_Current made record m, the rectangle search (through the copy's own
// address). Outside every rectangle: a member marked in the caller's +0xB
// (b8) is unmarked there and in Field_ScriptFlags2 (~b, 16 bits). Inside, not
// yet marked, and b in neither Field_ScriptFlags2 nor Field_ScriptFlags:
// marked in +0xB and Field_ScriptFlags2, Field_State +0x128 = 2, +9 = 0, +8
// = the rectangle's facing, Sprite_EnsureAnimation(+8). Inside and marked:
// +9 0 - Field_JumpSetUp, then Field_JumpCamera for member 0 only; else when
// +8 is not the facing, +8 = it, the dwords +0xC, +0x10, +0x14 negated and
// +9 = 8 - +9; then Field_LeaderStepTick, +9 - 1, Sprite_ScriptTick and
// Field_ScriptFlags bit 13 set. After the members: Field_Request 5 clears
// bit 13 again; Sprite_Current put back (Field_State is not).
using RectFn = unsigned char (__cdecl*)(unsigned);
void MembersFrame(const at::Rects& t) {
    const unsigned count = Field_MemberCount;
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xDFFF);
    unsigned char* const caller = Sprite_Current;
    if (count != 0) {
        unsigned m = 0;
        do {
            unsigned char* const record = PartyRecord(m);
            Field_State = record;
            Sprite_Current = record;
            const unsigned char found = AH_AT(RectFn, t.fn_rect)(m);
            const U bit = 1u << (m & 31);
            const auto bit8 = static_cast<unsigned char>((m & 31) < 8 ? bit : 0);
            if (found == 0xFF) {
                const unsigned char marks = caller[0xB];
                if ((marks & bit8) != 0) {
                    caller[0xB] = static_cast<unsigned char>(marks & ~bit8);
                    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~bit);
                }
            } else if ((caller[0xB] & bit8) == 0) {
                const unsigned char marks = caller[0xB];
                if (((static_cast<U>(Field_ScriptFlags2) | static_cast<U>(Field_ScriptFlags)) & bit) == 0) {
                    caller[0xB] = static_cast<unsigned char>(marks | bit8);
                    unsigned char* const field = Field_State;
                    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | bit);
                    field[at::kFieldStateMember] = 2;
                    Sprite_Current[9] = 0;
                    Sprite_Current[8] = Mem(t.rects + 4 + found * at::kRectStride)[0];
                    AH_CALL(Sprite_EnsureAnimation)(Sprite_Current[8]);
                }
            } else {
                unsigned char* o = Sprite_Current;
                if (o[9] == 0) {
                    AH_CALL(Field_JumpSetUp)();
                    if (m == 0) AH_CALL(Field_JumpCamera)();
                } else {
                    const unsigned char facing = Mem(t.rects + 4 + found * at::kRectStride)[0];
                    if (o[8] != facing) {
                        o[8] = facing;
                        o = Sprite_Current;
                        SetLong(o + 0xC, static_cast<std::int32_t>(0u - static_cast<U>(Long(o + 0xC))));
                        o = Sprite_Current;
                        SetLong(o + 0x10, static_cast<std::int32_t>(0u - static_cast<U>(Long(o + 0x10))));
                        o = Sprite_Current;
                        SetLong(o + 0x14, static_cast<std::int32_t>(0u - static_cast<U>(Long(o + 0x14))));
                        o = Sprite_Current;
                        o[9] = static_cast<unsigned char>(8 - o[9]);
                    }
                }
                AH_CALL(Field_LeaderStepTick)();
                Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
                AH_CALL(Sprite_ScriptTick)();
                Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x2000);
            }
            m = (m + 1) & 0xFF;
        } while (m < Field_MemberCount);
    }
    if (Field_Request == 5) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xDFFF);
    Sprite_Current = caller;
}

// Areas 169 and 171's tails' states 1..3 (the same code in both): once the
// DA wait word is 0, sound 0x202, Party_HealJoined, Transition_Start(9),
// state 2; once it is 0 again, message 1 and Field_Request 2, state 3; once
// Field_Request is not 2, ScriptFlags_Clear40, story flag 0x74, the tail
// ended.
void HealStates(signed char state) {
    switch (state) {
    case 1:
        if (MoveScript_WaitWordDA != 0) return;
        AH_CALL(Sound_PlayEffect)(0x202);
        AH_CALL(Party_HealJoined)();
        AH_CALL(Transition_Start)(9);
        B(at::kTailState) = 2;
        return;
    case 2:
        if (MoveScript_WaitWordDA != 0) return;
        AH_CALL(Msg_OpenScript)(1);
        Field_Request = 2;
        B(at::kTailState) = 3;
        return;
    case 3:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Flags_Set)(Story(), 0x74);
        EndTail();
        return;
    default: return;
    }
}

// Areas 171 and 172's spawns of an effect of kind 0x92 (0x427600, 0x427FA0):
// a free slot (none: nothing); the record live, kind 0x92, word +0x2E 0, word
// +0x30 `y`, +0x29 `slot29`, +0xB the active member's index among
// Sprite_Objects (its distance over 0xA4, signed, truncated; read after the
// search).
void SpawnEffect92(unsigned short y, unsigned char slot29) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    const auto distance = static_cast<std::int32_t>(static_cast<U>(reinterpret_cast<std::uintptr_t>(Field_ActiveMember)) - at::kSpriteObjects);
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x92;
    SetWord(e + 0x2E, 0);
    SetWord(e + 0x30, y);
    e[0x29] = slot29;
    e[0xB] = static_cast<unsigned char>(distance / static_cast<std::int32_t>(at::kObjectStride));
}

// Area 170's tail: on counter 3 at `count`, story flag `flag` and
// Field_ChangeArea(0xAA, x, z, flags), state 30.
void ChangeIfCounter3(unsigned char count, unsigned flag, int x, int z, unsigned flags) {
    if (B(at::kCounter3) != count) return;
    AH_CALL(Flags_Set)(Story(), flag);
    AH_CALL(Field_ChangeArea)(0xAA, x, z, flags);
    B(at::kTailState) = 0x1E;
}
// Area 170's tail: Party_DropIn(entry), then `next`.
void DropIn(unsigned entry, unsigned char next) {
    AH_CALL(Party_DropIn)(entry);
    B(at::kTailState) = next;
}
// Area 170's tail states 41 and 44: an effect of kind 0x13 at a free slot
// (kept in the scratch byte, read back): live, its +0x64, +0x68 (the camera
// yaw, sign-extended, read after the search), +0x6C, and +9.
void SpawnEffect13(std::int32_t a, std::int32_t c, unsigned char nine) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    B(at::kScratch) = slot;
    if (slot == 0xFF) return;
    const std::int32_t yaw = static_cast<std::int16_t>(Word(Mem(at::kCameraYaw)));
    unsigned char* const e = EffectAt(B(at::kScratch));
    e[0] = 1;
    e[5] = 0x13;
    SetLong(e + 0x64, a);
    SetLong(e + 0x68, yaw);
    SetLong(e + 0x6C, c);
    e[9] = nine;
}
// MoveScript_F3Divisor = Field_MoveSpeeds byte << 3, 16 bits.
void SetDivisor(U speed) { SetWord(Mem(at::kF3Divisor), static_cast<unsigned>(B(speed)) << 3); }

// Area 172's two draws' prim byte stores and float vertices.
void SetFloatInt(unsigned char* p, int v) { SetFloat(p, static_cast<float>(v)); }

}  // namespace

// ===========================================================================
// Area 168 (descriptor 0x63C900: +0x34 only; no init, no handlers): seven
// choices (choice 2 is 0x425C30, area 167's block) and tail kind 59.
// ===========================================================================

// original 0x426560 (area 168 +0x34[1]): the answer read, message 0xFFFF;
// answer 3 arms engine tail kind 10 (state 5, sub-kind 0xFF). Then the focus
// object's dwords +0x18 and +0x1C = the two bytes of Area168_FocusPairs by
// the answer (read again, signed; not checked - a byte answer stays in
// .data), each with the focus pointer read again.
extern "C" void __cdecl Area168_ChoiceFocusPair(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 3) ArmTail10();
    U index = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(Answer())));
    unsigned char* focus = Ptr(at::kFocusObject);
    SetLong(focus + 0x18, B(at::kArea168FocusPairs + index * 2));
    focus = Ptr(at::kFocusObject);
    index = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(Answer())));
    SetLong(focus + 0x1C, B(at::kArea168FocusPairs + 1 + index * 2));
}

// original 0x4265C0 (area 168 +0x34[0] and +0x34[3]): key item 0xC, bit 1.
extern "C" void __cdecl Area168_ChoiceKeyItemC(void) { ChoiceKeyItem(0xC, 1); }

// original 0x426610 (area 168 +0x34[4]): key item 0xD, bit 2.
extern "C" void __cdecl Area168_ChoiceKeyItemD(void) { ChoiceKeyItem(0xD, 2); }

// original 0x426660 (area 168 +0x34[5]): message 0xFFFF; answer 0: story flag
// 0x8F, then tail kind 59 armed at state 0.
extern "C" void __cdecl Area168_ChoiceFlag8FArm59(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(Flags_Set)(Story(), 0x8F);
    ArmTail(0x3B, 0);
}

// original 0x4266A0 (area 168 +0x34[6]): message 0xFFFF; answer 0: tail kind
// 59 armed at state 10.
extern "C" void __cdecl Area168_ChoiceArm59At10(void) { ChoiceArm(0x3B, 0xA); }

// original 0x4266D0 (area 168 +0x34[7]): message 0x12 for answer 0, 0x13 for
// 1, 0x14 for any other.
extern "C" void __cdecl Area168_ChoiceMessage12(void) {
    const unsigned char answer = Answer();
    if (answer == 0) {
        SetMessage(0x12);
        return;
    }
    SetMessage(0x13u + (answer != 1 ? 1u : 0u));
}

// original 0x426700 (tail kind 59): by the signed state - 0: once
// Field_Request is not 2, state 1; 1: Field_ChangeArea(0xA7, 0xB0000,
// 0x698000, 0x89), the tail ended; 10: Kind2_Place(0), state 11; 11: once
// counter 0 is 1, sound 0x204, story flag 0x91, state 12; 12: once counter 0
// is 2, ScriptFlags_Clear40, counter 0 and the tail cleared. Any other state
// (2..9, above 12, negative): nothing.
extern "C" void __cdecl Area168_Tail59(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0:
        if (Field_Request != 2) B(at::kTailState) = 1;
        return;
    case 1:
        AH_CALL(Field_ChangeArea)(0xA7, 0xB0000, 0x698000, 0x89);
        EndTail();
        return;
    case 10:
        AH_CALL(Kind2_Place)(0);
        B(at::kTailState) = 0xB;
        return;
    case 11:
        if (B(at::kCounter0) != 1) return;
        AH_CALL(Sound_PlayEffect)(0x204);
        AH_CALL(Flags_Set)(Story(), 0x91);
        B(at::kTailState) = 0xC;
        return;
    case 12:
        if (B(at::kCounter0) != 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(at::kCounter0) = 0;
        EndTail();
        return;
    default: return;
    }
}

// ===========================================================================
// Area 169 (descriptor 0x63C9F8; PSX 0x801F3B58): the init, the member frame
// and its rectangle search, tail kind 47 and the arrive hook.
// ===========================================================================

// original 0x4267F0 (area 169 +0x40; PSX 0x801F3524): Cond_ByteFD not 1:
// story flag 0x74 cleared.
extern "C" void __cdecl Area169_InitClearFlag74(void) {
    if (Cond_ByteFD != 1) AH_CALL(Flags_Clear)(Story(), 0x74);
}

// original 0x426810 (called by 0x46D7C3, 0x46D780's case for area 169; a gap
// of the tool): the member frame over Area169_Rects.
extern "C" void __cdecl Area169_MembersFrame(void) { MembersFrame(at::kRects169); }

// original 0x4269F0 (called by Area169_MembersFrame; a gap of the tool): one
// rectangle of Area169_Rects 0x63CA3C holding the member, or 0xFF.
extern "C" unsigned char __cdecl Area169_MemberRect(unsigned member) { return MemberRect(at::kRects169, member); }

// original 0x426A60 (tail kind 47): state 0: Transition_Start(8), state 1;
// 1..3: HealStates. Any other state: nothing.
extern "C" void __cdecl Area169_TailHealParty(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    if (state == 0) {
        AH_CALL(Transition_Start)(8);
        B(at::kTailState) = 1;
        return;
    }
    HealStates(state);
}

// original 0x426B20 (Area_ArriveHook's case for area 169, 0x56E57D):
// Cond_ByteFD 1 and story flag 0x74 clear: tail kind 47 armed at state 0, al
// 1; else al 0.
extern "C" unsigned char __cdecl Area169_ArriveHook(long, long) {
    if (Cond_ByteFD != 1) return 0;
    if (AH_CALL(Flags_Test)(Story(), 0x74) != 0) return 0;
    ArmTail(0x2F, 0);
    return 1;
}

// ===========================================================================
// Area 170 (descriptor 0x63D5F8; PSX 0x801F6474): six choices (two also its
// handlers), the init, tail kind 37, the step hook and object triggers 19,
// 20 and 56.
// ===========================================================================

// original 0x426B60 (area 170 +0x34[0]): answer 0 arms tail kind 37 at 5.
extern "C" void __cdecl Area170_ChoiceArm37At5(void) { ChoiceArm(0x25, 5); }
// original 0x426B90 (area 170 +0x34[1]): answer 0 arms tail kind 37 at 15.
extern "C" void __cdecl Area170_ChoiceArm37At15(void) { ChoiceArm(0x25, 0xF); }
// original 0x426BC0 (area 170 +0x34[2]): answer 0 arms tail kind 37 at 25.
extern "C" void __cdecl Area170_ChoiceArm37At25(void) { ChoiceArm(0x25, 0x19); }

// original 0x426BF0 (area 170 +0x34[3]): message 0xFFFF; the tail's state 52
// for answer 0, 55 for any other (the kind is not written).
extern "C" void __cdecl Area170_ChoiceState52Or55(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    B(at::kTailState) = answer != 0 ? 0x37 : 0x34;
}

// original 0x426C10 (area 170 +0x3C[0] = +0x34[4]; PSX 0x801F4A7C):
// Field_ScriptFlags |= 0x1010.
extern "C" void __cdecl Area170_ScriptFlagsSet1010(void) { Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x1010); }

// original 0x426C20 (area 170 +0x3C[1] = +0x34[5]; PSX 0x801F4A9C):
// Field_ScriptFlags &= 0xEFEF.
extern "C" void __cdecl Area170_ScriptFlagsClear1010(void) { Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFEF); }

// original 0x426C30 (area 170 +0x40; PSX 0x801F4ABC): tail kind 51 becomes
// 37; the engine's 0x486D60; then the entry zone 2 with Cond_ByteFD 1 plays
// sound 0x20C (zone 2 with another value returns), and the zone (read again)
// 3 with Cond_ByteFD 2 plays it.
extern "C" void __cdecl Area170_Init(void) {
    if (B(at::kTailKind) == 0x33) B(at::kTailKind) = 0x25;
    AH_AT(Handler, at::kArea170MapSetUp)();
    if (B(at::kEntryZone) == 2) {
        if (Cond_ByteFD != 1) return;
        AH_CALL(Sound_PlayEffect)(0x20C);
    }
    if (B(at::kEntryZone) == 3 && Cond_ByteFD == 2) AH_CALL(Sound_PlayEffect)(0x20C);
}

// original 0x426C90 (tail kind 37): 61 states through a byte table into 25
// cases (docs/area_w4b.md section 1). A state outside 0..60 or on no case:
// nothing.
extern "C" void __cdecl Area170_Tail37(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0: DropIn(0, 1); return;
    case 1: ChangeIfCounter3(0x24, 0x55, 0x60000, 0x3F0000, 0x82); return;
    case 5: DropIn(1, 6); return;
    case 6: ChangeIfCounter3(0x24, 0x55, 0x40000, 0x750000, 0x83); return;
    case 10: DropIn(4, 0xB); return;
    case 11: ChangeIfCounter3(0x44, 0x56, 0x100000, 0x5D0000, 0x86); return;
    case 15: DropIn(5, 0x10); return;
    case 16: ChangeIfCounter3(0x44, 0x56, 0xA0000, 0x930000, 0x87); return;
    case 20: DropIn(8, 0x15); return;
    case 21: ChangeIfCounter3(0x64, 0x57, 0x170000, 0x5D0000, 0x8A); return;
    case 25: DropIn(9, 0x1A); return;
    case 26: ChangeIfCounter3(0x64, 0x57, 0x110000, 0x930000, 0x8B); return;
    case 29:
        if (B(at::kCounter3) != 0) return;
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x1010);
        AH_CALL(Flags_Clear)(Story(), 0x57);
        B(at::kTailState) = 0x35;
        return;
    case 30:
        if (B(at::kCounter3) != 0) return;
        AH_CALL(Flags_Clear)(Story(), 0x55);
        AH_CALL(Flags_Clear)(Story(), 0x56);
        AH_CALL(Flags_Clear)(Story(), 0x57);
        EndTail();
        return;
    case 40: {
        // an effect of kind 0x7D: +0x2E and +0x30 words 0x64, +6 the message
        // word's low byte - 0xB (read after the search); the timer 0
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot != 0xFF) {
            const auto six = static_cast<unsigned char>(B(at::kMessage) - 0xB);
            unsigned char* const e = EffectAt(slot);
            e[0] = 1;
            e[5] = 0x7D;
            SetWord(e + 0x2E, 0x64);
            SetWord(e + 0x30, 0x64);
            e[6] = six;
        }
        SetWord(Mem(at::kTailTimer), 0);
        B(at::kTailState) = 0x29;
        return;
    }
    case 41:
        // the timer 1: the glide divisor from speed 2, Field_Kind2X / Z a fixed
        // point, an effect of kind 0x13, state 42; then (either way) the timer
        // 0xFF: state 45
        if (Word(Mem(at::kTailTimer)) == 1) {
            SetDivisor(at::kMoveSpeed2);
            SetLong(Mem(at::kKind2X), 0x1A0000);
            SetLong(Mem(at::kKind2Z), 0x910000);
            SpawnEffect13(0, 0, 0x70);
            B(at::kTailState) = 0x2A;
        }
        if (Word(Mem(at::kTailTimer)) == 0xFF) B(at::kTailState) = 0x2D;
        return;
    case 42:
        if (Field_Kind2Hold != 0) return;
        SetWord(Mem(at::kTailTimer), 0x1E);
        B(at::kTailState) = 0x2B;
        return;
    case 43:
        AddWord(Mem(at::kTailTimer), -1);
        if (Word(Mem(at::kTailTimer)) != 0) return;
        SetWord(Mem(at::kTailTimer), 0x1E);
        B(at::kTailState) = 0x2C;
        return;
    case 44: {
        AddWord(Mem(at::kTailTimer), -1);
        if (Word(Mem(at::kTailTimer)) != 0) return;
        // the glide divisor from speed 3, Field_Kind2X / Z the leader's
        // position, an effect of kind 0x13, state 45
        const unsigned speed = B(at::kMoveSpeed3);
        const std::int32_t x = Long(Mem(at::kLeaderX));
        const std::int32_t z = Long(Mem(at::kLeaderZ));
        SetWord(Mem(at::kF3Divisor), speed << 3);
        SetLong(Mem(at::kKind2X), x);
        SetLong(Mem(at::kKind2Z), z);
        SpawnEffect13(static_cast<std::int32_t>(0xFFFFFD56u), 0x200, 0x38);
        B(at::kTailState) = 0x2D;
        return;
    }
    case 45:
        if (Field_Kind2Hold != 0) return;
        Disarm();
        return;
    case 50:
        AH_CALL(Msg_OpenScript)(0xA);
        Field_Request = 2;
        B(at::kTailState) = 0x33;
        return;
    case 52:
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Party_DropIn)(0xC);
        B(at::kTailState) = 0x35;
        return;
    case 53: {
        // while Field_Request is 0: the held word kept in the scratch cells,
        // its top nibble swapped through Area170_InputSwap
        if (Field_Request != 0) return;
        const std::uint16_t held = Word(Mem(at::kInputHeld));
        SetWord(Mem(at::kScratch), held);
        const unsigned swapped = static_cast<unsigned>(B(at::kArea170InputSwap + (held >> 12))) << 12;
        SetWord(Mem(at::kInputHeld), (swapped | (held & 0xFFFu)) & 0xFFFFu);
        return;
    }
    case 55:
        if (Field_Request == 2) return;
        Disarm();
        return;
    case 60:
        AH_CALL(ScriptFlags_Clear40)();
        AddWord(Mem(at::kParty1Word12E), 1);
        AddWord(Mem(at::kParty2Word12E), 1);
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFEF);
        EndTail();
        return;
    default: return;
    }
}

// original 0x427270 (Area_StepHook's case for area 170, 0x56E1C2): by
// Cond_ByteFD - 4: z exactly 0x708000 or 0x738000 with x's high word 3..5, or
// x exactly 0x28000 or 0x58000 with z's high word 0x71..0x73: tail kind 37
// armed at 0, al 1. 5: z 0x8E8000 / 0x918000 with x's high word 9..11, or x
// 0x88000 / 0xB8000 with z's 0x8F..0x91: armed at 10, al 1; z 0x8E8000 /
// 0x918000 with x's 0x10..0x12, or x 0xF8000 / 0x128000 with z's 0x8F..0x91:
// armed at 20, story flag 0x7E set, Field_ScriptFlags &= 0xEFEF, al 1; else,
// story flag 0x7E clear, z exactly 0x900000 and x exactly 0x208000 with the
// leader's pose 7: armed at 50, al 1; x exactly 0x218000 with pose 3: kind 37
// and state 60 written without ScriptFlags_Set40, al 0. Anything else al 0.
extern "C" unsigned char __cdecl Area170_StepHook(long x, long z) {
    const auto ux = static_cast<U>(x);
    const auto uz = static_cast<U>(z);
    const unsigned char fd = Cond_ByteFD;
    if (fd == 4) {
        if ((uz == 0x708000 || uz == 0x738000) && In3(High(x), 3)) {
            ArmTail(0x25, 0);
            return 1;
        }
        if ((ux == 0x28000 || ux == 0x58000) && In3(High(z), 0x71)) {
            ArmTail(0x25, 0);
            return 1;
        }
        return 0;
    }
    if (fd != 5) return 0;
    if (((uz == 0x8E8000 || uz == 0x918000) && In3(High(x), 9)) || ((ux == 0x88000 || ux == 0xB8000) && In3(High(z), 0x8F))) {
        ArmTail(0x25, 0xA);
        return 1;
    }
    if (((uz == 0x8E8000 || uz == 0x918000) && In3(High(x), 0x10)) || ((ux == 0xF8000 || ux == 0x128000) && In3(High(z), 0x8F))) {
        ArmTail(0x25, 0x14);
        AH_CALL(Flags_Set)(Story(), 0x7E);
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFEF);
        return 1;
    }
    if (AH_CALL(Flags_Test)(Story(), 0x7E) != 0) return 0;
    if (ux == 0x208000) {
        if (uz != 0x900000 || B(at::kLeaderPose) != 7) return 0;
        ArmTail(0x25, 0x32);
        return 1;
    }
    if (ux == 0x218000 && uz == 0x900000 && B(at::kLeaderPose) == 3) {
        B(at::kTailKind) = 0x25;
        B(at::kTailState) = 0x3C;
    }
    return 0;
}

// original 0x427470 (Field_ObjectTriggers id 19; a gap of the tool): tail
// kind 37 armed at 40, story flag 0x7E cleared, al 1.
extern "C" unsigned char __cdecl Area170_Trigger19(unsigned char*, unsigned char*) {
    ArmTail(0x25, 0x28);
    AH_CALL(Flags_Clear)(Story(), 0x7E);
    return 1;
}

// original 0x4274A0 (Field_ObjectTriggers id 20; a gap): story flag 0x67, al 0.
extern "C" unsigned char __cdecl Area170_Trigger20(unsigned char*, unsigned char*) {
    AH_CALL(Flags_Set)(Story(), 0x67);
    return 0;
}

// original 0x4274C0 (Field_ObjectTriggers id 56; a gap): Sprite_Current kept;
// story flag 0x79 clear: Inventory_Add(1, 0x4B, 1) (the original pushes a
// fourth word, 0, the callee does not read) - added: flag 0x79 set,
// Sprite_SetAnimation(1), message 0x22, sound 0x106; not added: system
// message 3. Flag 0x79 set: system message 1. Then Party_DropIn(0xD),
// Sprite_Current put back, Field_Request 2, al 0.
extern "C" unsigned char __cdecl Area170_Trigger56(unsigned char*, unsigned char*) {
    unsigned char* const saved = Sprite_Current;
    if (AH_CALL(Flags_Test)(Story(), 0x79) == 0) {
        if (AH_CALL(Inventory_Add)(1, 0x4B, 1) != 0) {
            AH_CALL(Flags_Set)(Story(), 0x79);
            AH_CALL(Sprite_SetAnimation)(1);
            AH_CALL(Msg_OpenScript)(0x22);
            AH_CALL(Sound_PlayEffect)(0x106);
        } else {
            AH_CALL(Msg_OpenSystem)(3);
        }
    } else {
        AH_CALL(Msg_OpenSystem)(1);
    }
    AH_CALL(Party_DropIn)(0xD);
    Sprite_Current = saved;
    Field_Request = 2;
    return 0;
}

// ===========================================================================
// Area 171 (descriptor 0x63D808; PSX 0x801F4144): three choices (two also its
// handlers), the init, the member frame and its rectangle search (area 169's
// code over its own rectangles), tail kind 48, the step and arrive hooks and
// object trigger 55.
// ===========================================================================

// original 0x427540 (area 171 +0x34[0]): message 0xFFFF; answer 0 arms engine
// tail kind 10 (state 5, sub-kind 0xFF).
extern "C" void __cdecl Area171_ChoiceArmTail10(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0) ArmTail10();
}

// original 0x427570 (area 171 +0x3C[0] = +0x34[1]; PSX 0x801F3558): the
// active member's +0x80 bit 0 cleared. The leader's +0x89 5: unless the byte
// 0x903DB6 is 0x4B, nothing more; else story flag 0x73, map cells (2, 0x21),
// (2, 0x22) 0xC0 and (3, 0x21), (3, 0x22) 0, the active member's word +0x8A +
// 1. Then the leader's +0x89 (read again) 7: tail kind 48 armed at 10.
extern "C" void __cdecl Area171_Leader89Gate(void) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(at::kLeader89) == 5) {
        if (B(at::kGateKey) != 0x4B) return;
        AH_CALL(Flags_Set)(Story(), 0x73);
        SetByte(2, 0x21, 0xC0);
        SetByte(2, 0x22, 0xC0);
        SetByte(3, 0x21, 0);
        SetByte(3, 0x22, 0);
        AddWord(Field_ActiveMember + 0x8A, 1);
    }
    if (B(at::kLeader89) == 7) ArmTail(0x30, 0xA);
}

// original 0x427600 (area 171 +0x3C[1] = +0x34[2]; PSX 0x801F3650): an effect
// of kind 0x92 (+0x30 0xFFE2, +0x29 6); then the four cells of handler 0 set
// to 0x89.
extern "C" void __cdecl Area171_SpawnEffect92(void) {
    SpawnEffect92(0xFFE2, 6);
    SetByte(2, 0x21, 0x89);
    SetByte(2, 0x22, 0x89);
    SetByte(3, 0x21, 0x89);
    SetByte(3, 0x22, 0x89);
}

// original 0x4276B0 (area 171 +0x40; PSX 0x801F3764): Cond_ByteFD not 3:
// story flag 0x74 cleared.
extern "C" void __cdecl Area171_InitClearFlag74(void) {
    if (Cond_ByteFD != 3) AH_CALL(Flags_Clear)(Story(), 0x74);
}

// original 0x4276D0 (called by 0x46D7D0, 0x46D780's case for area 171; a gap
// of the tool): Area169_MembersFrame's code over Area171_Rects.
extern "C" void __cdecl Area171_MembersFrame(void) { MembersFrame(at::kRects171); }

// original 0x4278B0 (called by Area171_MembersFrame; a gap): Area171_Rects
// 0x63D84C.
extern "C" unsigned char __cdecl Area171_MemberRect(unsigned member) { return MemberRect(at::kRects171, member); }

// original 0x427920 (tail kind 48): state 0: Cond_ByteFD 3 -
// Transition_Start(8), state 1; else disarmed. 1..3: HealStates. 10: the
// leader's +0x137 0 - ScriptFlags_Clear40, Party_DropIn(0), the tail ended.
// 20: message 2, Field_Request 2, state 21. 21: Field_Request not 2 -
// disarmed. Any other state: nothing.
extern "C" void __cdecl Area171_TailHealParty(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    switch (state) {
    case 0:
        if (Cond_ByteFD != 3) {
            Disarm();
            return;
        }
        AH_CALL(Transition_Start)(8);
        B(at::kTailState) = 1;
        return;
    case 10:
        if (B(at::kLeader137) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Party_DropIn)(0);
        EndTail();
        return;
    case 20:
        AH_CALL(Msg_OpenScript)(2);
        Field_Request = 2;
        B(at::kTailState) = 0x15;
        return;
    case 21:
        if (Field_Request == 2) return;
        Disarm();
        return;
    default: HealStates(state); return;
    }
}

// original 0x427A80 (Area_StepHook's case for area 171, 0x56E1D2):
// Cond_ByteFD 2 and story flag 0x73 clear: the scratch byte = the flag's
// answer (0), then + 1 for each map byte 0x89 at the cell (x, z) (high
// words), at (x + 1, z) when x has a fraction, at (x, z + 1) when z has one,
// at (x + 1, z + 1) when both; any counted: tail kind 48 armed at 20. al 0
// always.
extern "C" unsigned char __cdecl Area171_StepHook(long x, long z) {
    if (Cond_ByteFD != 2) return 0;
    const unsigned char flag = AH_CALL(Flags_Test)(Story(), 0x73);
    if (flag != 0) return 0;
    B(at::kScratch) = flag;
    const auto cx = static_cast<short>(High(x));
    const auto cz = static_cast<short>(High(z));
    if (AH_CALL(AreaMap_ByteAt)(cx, cz) == 0x89) ++B(at::kScratch);
    const bool fx = (static_cast<U>(x) & 0xFFFF) != 0;
    if (fx && AH_CALL(AreaMap_ByteAt)(static_cast<short>(cx + 1), cz) == 0x89) ++B(at::kScratch);
    const bool fz = (static_cast<U>(z) & 0xFFFF) != 0;
    if (fz && AH_CALL(AreaMap_ByteAt)(cx, static_cast<short>(cz + 1)) == 0x89) ++B(at::kScratch);
    if (fx && fz && AH_CALL(AreaMap_ByteAt)(static_cast<short>(cx + 1), static_cast<short>(cz + 1)) == 0x89) ++B(at::kScratch);
    if (B(at::kScratch) != 0) ArmTail(0x30, 0x14);
    return 0;
}

// original 0x427B50 (Area_ArriveHook's case for area 171, 0x56E593):
// Cond_ByteFD 3 and story flag 0x74 clear: tail kind 48 armed at 0, al 1;
// else al 0.
extern "C" unsigned char __cdecl Area171_ArriveHook(long, long) {
    if (Cond_ByteFD != 3) return 0;
    if (AH_CALL(Flags_Test)(Story(), 0x74) != 0) return 0;
    ArmTail(0x30, 0);
    return 1;
}

// original 0x427B90 (Field_ObjectTriggers id 55; a gap): Cond_ByteFE 1,
// MoveCmd_TestFB(0x1C, 0x4A), sound 0x109, al 0.
extern "C" unsigned char __cdecl Area171_Trigger55(unsigned char*, unsigned char*) {
    Cond_ByteFE = 1;
    AH_CALL(MoveCmd_TestFB)(0x1C, 0x4A);
    AH_CALL(Sound_PlayEffect)(0x109);
    return 0;
}

// ===========================================================================
// Area 172 (descriptor 0x63EED0; PSX 0x801F672C): eight handlers (each also a
// choice, one on), choice 0, the fall and slide state machines, tail kind 35,
// the step hook, the init, effect kind 0xA5 with its three states and two
// draws.
// ===========================================================================

// original 0x427BB0 (area 172 +0x3C[0] = +0x34[1]; PSX 0x801F4324): story
// flag 0x4E cleared.
extern "C" void __cdecl Area172_ClearFlag4E(void) { AH_CALL(Flags_Clear)(Story(), 0x4E); }

// original 0x427BC0 (area 172 +0x3C[1] = +0x34[2]; PSX 0x801F434C): the first
// member (below Field_MemberCount, read once) whose +0x89 is 4: the script
// position + 3.
extern "C" void __cdecl Area172_SkipIfMember89Is4(void) {
    const unsigned count = Field_MemberCount;
    for (unsigned m = 0; m < count; m = (m + 1) & 0xFF) {
        if (PartyRecord(m)[0x89] == 4) {
            AddWord(MoveScript_Object + 0xA, 3);
            return;
        }
    }
}

// original 0x427C10 (area 172 +0x3C[2] = +0x34[3]; PSX 0x801F43C8):
// Area172_FallStates by Sprite_Current[4]. The table's two entries run on
// into Area172_SlideStates' two (code: the fall's state 1 sets 2), then the
// drift steps (data): ours aborts at 4.
extern "C" void __cdecl Area172_RunFall(void) {
    StateEntry("Area172_RunFall", at::kArea172FallStates, at::kArea172FallReach, Sprite_Current[4])();
}

// original 0x427C30 (Area172_FallStates[0]): the running object's +0 bit
// 0x40 cleared and 0x20 set, the tint +0x5C 1 and +0x5D..+0x5F 0x80, the
// velocities +0xC, +0x10, +0x14 0, +0x20 -8, state 1; the script position - 2.
extern "C" void __cdecl Area172_FallStart(void) {
    unsigned char* const o = Sprite_Current;
    o[0] = static_cast<unsigned char>(o[0] & 0xBF);
    o[0] = static_cast<unsigned char>(o[0] | 0x20);
    o[0x5C] = 1;
    o[0x5D] = 0x80;
    o[0x5E] = 0x80;
    o[0x5F] = 0x80;
    SetLong(o + 0xC, 0);
    SetLong(o + 0x10, 0);
    SetLong(o + 0x14, 0);
    SetLong(o + 0x20, -8);
    o[4] = 1;
    AddWord(MoveScript_Object + 0xA, -2);
}

// original 0x427CB0 (Area172_FallStates[1]): +0x14 += +0x20;
// Field_LeaderStepTick; the ground at the object (MapView_GroundAt) above its
// height word +0x3E (16 bits, signed): landed - +0 bit 0x20 cleared, the tint
// 0, the height the ground, +0x14 and +0x20 0, state 2,
// Sprite_SetAnimation(0x39), the script object's +1 = 4; else the script
// position - 2. Then the field record's tint record (+0x149's) bytes +2..+4
// less 2; the object's +0x5D not 0x40: +0x5D..+0x5F + 4. Sprite_Kind2's
// height dword = the object's +0x3C, MapView_SetElevation(its word +0x3E).
extern "C" void __cdecl Area172_FallStep(void) {
    unsigned char* o = Sprite_Current;
    SetLong(o + 0x14, static_cast<std::int32_t>(static_cast<U>(Long(o + 0x14)) + static_cast<U>(Long(o + 0x20))));
    AH_CALL(Field_LeaderStepTick)();
    o = Sprite_Current;
    const long ground = AH_CALL(MapView_GroundAt)(Long(o + 0x34), Long(o + 0x38));
    o = Sprite_Current;
    if (static_cast<std::int16_t>(ground) > static_cast<std::int16_t>(Word(o + 0x3E))) {
        o[0] = static_cast<unsigned char>(o[0] & 0xDF);
        o[0x5C] = 0;
        o[0x5D] = 0;
        o[0x5E] = 0;
        o[0x5F] = 0;
        SetWord(o + 0x3E, static_cast<unsigned>(ground));
        SetLong(o + 0x14, 0);
        SetLong(o + 0x20, 0);
        o[4] = 2;
        AH_CALL(Sprite_SetAnimation)(0x39);
        MoveScript_Object[1] = 4;
    } else {
        AddWord(MoveScript_Object + 0xA, -2);
    }
    const unsigned char* const field = Field_State;
    unsigned char* const tint = Mem(at::kTintRecords + field[at::kFieldStateTint] * at::kTintStride);
    tint[2] = static_cast<unsigned char>(tint[2] + 0xFE);
    tint[3] = static_cast<unsigned char>(tint[3] + 0xFE);
    tint[4] = static_cast<unsigned char>(tint[4] + 0xFE);
    o = Sprite_Current;
    if (o[0x5D] != 0x40) {
        o[0x5D] = static_cast<unsigned char>(o[0x5D] + 4);
        o[0x5E] = static_cast<unsigned char>(o[0x5E] + 4);
        o[0x5F] = static_cast<unsigned char>(o[0x5F] + 4);
    }
    SetLong(Mem(at::kKind2Height), Long(o + 0x3C));
    AH_CALL(MapView_SetElevation)(static_cast<int>(Word(o + 0x3E)));
}

// original 0x427DF0 (area 172 +0x3C[3] = +0x34[4]; PSX 0x801F471C):
// Area172_SlideStates by Sprite_Current[4]; past its two entries is data
// (the drift steps): ours aborts.
extern "C" void __cdecl Area172_RunSlide(void) {
    StateEntry("Area172_RunSlide", at::kArea172SlideStates, at::kArea172SlideCount, Sprite_Current[4])();
}

// original 0x427E10 (Area172_SlideStates[0]): +0 bit 0x20 set and 0x40
// cleared, the tint +0x5C 1 and +0x5D..+0x5F 0x80; Area172_SlideStep once;
// state 1.
extern "C" void __cdecl Area172_SlideStart(void) {
    unsigned char* const o = Sprite_Current;
    o[0] = static_cast<unsigned char>(o[0] | 0x20);
    o[0] = static_cast<unsigned char>(o[0] & 0xBF);
    o[0x5C] = 1;
    o[0x5F] = 0x80;
    o[0x5E] = 0x80;
    o[0x5D] = 0x80;
    AH_CALL(Area172_SlideStep)();
    Sprite_Current[4] = 1;
}

// original 0x427E60 (Area172_SlideStates[1]; called by Area172_SlideStart):
// x += +0xC, z += +0x10; each of +0x5D..+0x5F below 0xC0 as a signed byte +
// 2; all three 0xC0: +0 bit 0x20 cleared, the tint and the state 0; else the
// active member's word +0x8A - 2.
extern "C" void __cdecl Area172_SlideStep(void) {
    unsigned char* const o = Sprite_Current;
    SetLong(o + 0x34, static_cast<std::int32_t>(static_cast<U>(Long(o + 0x34)) + static_cast<U>(Long(o + 0xC))));
    SetLong(o + 0x38, static_cast<std::int32_t>(static_cast<U>(Long(o + 0x38)) + static_cast<U>(Long(o + 0x10))));
    for (unsigned k = 0x5D; k <= 0x5F; ++k)
        if (static_cast<signed char>(o[k]) < static_cast<signed char>(0xC0)) o[k] = static_cast<unsigned char>(o[k] + 2);
    if (o[0x5D] == 0xC0 && o[0x5E] == 0xC0 && o[0x5F] == 0xC0) {
        o[0] = static_cast<unsigned char>(o[0] & 0xDF);
        o[0x5C] = 0;
        o[0x5F] = 0;
        o[0x5E] = 0;
        o[0x5D] = 0;
        o[4] = 0;
        return;
    }
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x427F20 (area 172 +0x3C[4] = +0x34[5]; PSX 0x801F491C): counter 0
// below 7: x += Area172_DriftSteps[+0xA & 0xF] << 11 and z -= it << 11 (the
// step signed), +0xA - 1, the field record's word +0x12E - 2; else x and z
// snapped to a half cell (& 0xFFFF8000).
extern "C" void __cdecl Area172_Drift(void) {
    unsigned char* const o = Sprite_Current;
    if (B(at::kCounter0) < 7) {
        U step = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kArea172DriftSteps + (o[0xA] & 0xF)))));
        SetLong(o + 0x34, static_cast<std::int32_t>(static_cast<U>(Long(o + 0x34)) + (step << 11)));
        step = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kArea172DriftSteps + (o[0xA] & 0xF)))));
        SetLong(o + 0x38, static_cast<std::int32_t>(static_cast<U>(Long(o + 0x38)) + ((0u - step) << 11)));
        o[0xA] = static_cast<unsigned char>(o[0xA] - 1);
        AddWord(Field_State + 0x12E, -2);
        return;
    }
    SetLong(o + 0x34, static_cast<std::int32_t>(static_cast<U>(Long(o + 0x34)) & 0xFFFF8000u));
    SetLong(o + 0x38, static_cast<std::int32_t>(static_cast<U>(Long(o + 0x38)) & 0xFFFF8000u));
}

// original 0x427FA0 (area 172 +0x3C[5] = +0x34[6]; PSX 0x801F49DC): an effect
// of kind 0x92 (+0x30 0, +0x29 4).
extern "C" void __cdecl Area172_SpawnEffect92(void) { SpawnEffect92(0, 4); }

// original 0x428010 (area 172 +0x3C[6] = +0x34[7]; PSX 0x801F4AAC): +0 bit
// 0x20 set, the tint +0x5C 1 and +0x5D..+0x5F 0xC0.
extern "C" void __cdecl Area172_TintOn(void) {
    unsigned char* const o = Sprite_Current;
    o[0] = static_cast<unsigned char>(o[0] | 0x20);
    o[0x5C] = 1;
    o[0x5F] = 0xC0;
    o[0x5E] = 0xC0;
    o[0x5D] = 0xC0;
}

// original 0x428050 (area 172 +0x3C[7] = +0x34[8]; PSX 0x801F4AF4): +0 bit
// 0x20 cleared, the tint 0.
extern "C" void __cdecl Area172_TintOff(void) {
    unsigned char* const o = Sprite_Current;
    o[0] = static_cast<unsigned char>(o[0] & 0xDF);
    o[0x5C] = 0;
    o[0x5F] = 0;
    o[0x5E] = 0;
    o[0x5D] = 0;
}

// original 0x428090 (area 172 +0x34[0]): message 0xFFFF; answer 0:
// ScriptFlags_Set40, then Cond row 14's flag 0x12 set - tail kind 35 at state
// 5; clear - MoveScript_Var7 6 and the byte after it 0 (the tail not
// written).
extern "C" void __cdecl Area172_ChoiceFlag12(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    if (AH_CALL(Flags_Test)(Mem(at::kCondRow14), 0x12) != 0) {
        B(at::kTailKind) = 0x23;
        B(at::kTailState) = 5;
        return;
    }
    B(at::kVar7) = 6;
    B(at::kVar7b) = 0;
}

// original 0x4280E0 (tail kind 35): 0: Party_DropIn(1), state 1; 1: counter 3
// at 0x14 - story flag 0x4E, Field_ChangeArea(0xAC, 0xA0000, 0x50000, 0x83),
// the tail ended; 5: Party_DropIn(0), state 6; 6: counter 3 at 0x14 - flag
// 0x4E, Field_ChangeArea(0xAC, 0x260000, 0x360000, 0x82), ended. Any other
// state: nothing.
extern "C" void __cdecl Area172_Tail35(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0:
        AH_CALL(Party_DropIn)(1);
        B(at::kTailState) = 1;
        return;
    case 1:
        if (B(at::kCounter3) != 0x14) return;
        AH_CALL(Flags_Set)(Story(), 0x4E);
        AH_CALL(Field_ChangeArea)(0xAC, 0xA0000, 0x50000, 0x83);
        EndTail();
        return;
    case 5:
        AH_CALL(Party_DropIn)(0);
        B(at::kTailState) = 6;
        return;
    case 6:
        if (B(at::kCounter3) != 0x14) return;
        AH_CALL(Flags_Set)(Story(), 0x4E);
        AH_CALL(Field_ChangeArea)(0xAC, 0x260000, 0x360000, 0x82);
        EndTail();
        return;
    default: return;
    }
}

// original 0x4281A0 (Area_StepHook's case for area 172, 0x56E1E2):
// Cond_ByteFD 0, x exactly 0x248000, z's high word 0x35..0x37 and the
// leader's pose 6, 7 or 0: tail kind 35 armed at 0, al 1; else al 0.
extern "C" unsigned char __cdecl Area172_StepHook(long x, long z) {
    if (Cond_ByteFD != 0) return 0;
    if (static_cast<U>(x) != 0x248000) return 0;
    if (!In3(High(z), 0x35)) return 0;
    const unsigned char pose = B(at::kLeaderPose);
    if (pose != 6 && pose != 7 && pose != 0) return 0;
    ArmTail(0x23, 0);
    return 1;
}

// original 0x4281F0 (area 172 +0x40; PSX 0x801F4D58): map cell (9, 3) 0x50.
extern "C" void __cdecl Area172_InitCell(void) { SetByte(9, 3, 0x50); }

// original 0x428200 (Effect_KindHandlers[0xA5]; a gap of the tool):
// Area172_EffectStates by Sprite_Current[1] (the running effect record);
// past its three entries is data: ours aborts.
extern "C" void __cdecl Area172_EffectA5Run(void) {
    StateEntry("Area172_EffectA5Run", at::kArea172EffectStates, at::kArea172EffectCount, Sprite_Current[1])();
}

// original 0x428220 (Area172_EffectStates[0]): the record's word +0x2E and
// dword +0xC 0, state 1.
extern "C" void __cdecl Area172_EffectA5Start(void) {
    unsigned char* const e = Sprite_Current;
    SetWord(e + 0x2E, 0);
    SetLong(e + 0xC, 0);
    e[1] = 1;
}

// original 0x428240 (Area172_EffectStates[1]): the word +0x2E + 10; above
// 0x190 (signed): the byte after MoveScript_Var7 + 1 and state 2. Then
// Area172_DrawPanel(0xDC, 0xC8) and Area172_DrawShade(+0x2E, 0xC8) (the word
// read again after the first draw).
extern "C" void __cdecl Area172_EffectA5Grow(void) {
    unsigned char* const e = Sprite_Current;
    AddWord(e + 0x2E, 10);
    if (static_cast<std::int16_t>(Word(e + 0x2E)) > 0x190) {
        B(at::kVar7b) = static_cast<unsigned char>(B(at::kVar7b) + 1);
        e[1] = 2;
    }
    AH_CALL(Area172_DrawPanel)(0xDC, 0xC8);
    AH_CALL(Area172_DrawShade)(static_cast<int>(Word(Sprite_Current + 0x2E)), 0xC8);
}

// original 0x428290 (Area172_EffectStates[2]): Area172_DrawPanel(0xDC, 0xC8).
extern "C" void __cdecl Area172_EffectA5Hold(void) { AH_CALL(Area172_DrawPanel)(0xDC, 0xC8); }

// original 0x4282B0 (called by effect kind 0xA5's states 1 and 2): a POLY_FT4
// at Gfx_PacketNext, the arguments' low words signed: (x, y)..(x + 0x40, y +
// 0x20) as floats, u 0..0x40, v 0..0x20, clut (0, 0x1EB), tpage (1, 1, 0x300,
// 0x100), colour 0x80 grey; committed to slot 1 (0x48 bytes).
extern "C" void __cdecl Area172_DrawPanel(int x, int y) {
    unsigned char* const p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(p);
    const int sx = static_cast<std::int16_t>(x);
    const int sy = static_cast<std::int16_t>(y);
    SetFloatInt(p + 8, sx);
    p[0x24] = 0x40;
    p[0x44] = 0x40;
    SetFloatInt(p + 0xC, sy);
    p[0x35] = 0x20;
    p[0x45] = 0x20;
    SetFloatInt(p + 0x18, sx + 0x40);
    SetFloatInt(p + 0x1C, sy);
    SetFloatInt(p + 0x38, sx + 0x40);
    SetFloatInt(p + 0x28, sx);
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x25] = 0;
    SetFloatInt(p + 0x2C, sy + 0x20);
    SetFloatInt(p + 0x3C, sy + 0x20);
    p[0x34] = 0;
    SetWord(p + 0x16, AH_CALL(Gpu_GetClut)(0, 0x1EB));
    SetWord(p + 0x26, AH_CALL(Gpu_GetTPage)(1, 1, 0x300, 0x100));
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    AH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// original 0x428370 (called by effect kind 0xA5's state 1): a draw-mode packet
// (dtd 1, tpage (0, 2, 0x3C0, 0), tw 0 - the fifth word is a push the
// original leaves from the tpage call) committed to slot 1 (0xC); then a
// POLY_G4 at Gfx_PacketNext (read again), the arguments' low words signed:
// (x - 0x40, y) black, (320, y) white, (x, y + 0x30) grey 0x40, (320, y +
// 0x30) white, as floats; semi-transparent, shade-tex 0; committed to slot 1
// (0x44 bytes).
extern "C" void __cdecl Area172_DrawShade(int x, int y) {
    const unsigned tpage = AH_CALL(Gpu_GetTPage)(0, 2, 0x3C0, 0) & 0xFFFF;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyG4)(p);
    const int sx = static_cast<std::int16_t>(x);
    const int sy = static_cast<std::int16_t>(y);
    constexpr std::int32_t k320 = 0x43A00000;   // 320.0f
    SetLong(p + 0x38, k320);
    SetLong(p + 0x18, k320);
    SetFloatInt(p + 8, sx - 0x40);
    SetFloatInt(p + 0x28, sx);
    p[4] = 0;
    SetFloatInt(p + 0x1C, sy);
    SetFloatInt(p + 0xC, sy);
    p[5] = 0;
    p[6] = 0;
    p[0x14] = 0xFF;
    p[0x15] = 0xFF;
    SetFloatInt(p + 0x3C, sy + 0x30);
    SetFloatInt(p + 0x2C, sy + 0x30);
    p[0x16] = 0xFF;
    p[0x24] = 0x40;
    p[0x25] = 0x40;
    p[0x26] = 0x40;
    p[0x34] = 0xFF;
    p[0x35] = 0xFF;
    p[0x36] = 0xFF;
    AH_CALL(Gpu_SetSemiTrans)(p, 1);
    AH_CALL(Gpu_SetShadeTex)(p, 0);
    AH_CALL(Gfx_CommitPrim)(1, 0x44);
}

void AreaW4b_Inject() {
    if (bof3::WantsShadow("area_w4b")) area_w4b::SelfTest();
    BOF3_INJECT(Area168_ChoiceFocusPair);
    BOF3_INJECT(Area168_ChoiceKeyItemC);
    BOF3_INJECT(Area168_ChoiceKeyItemD);
    BOF3_INJECT(Area168_ChoiceFlag8FArm59);
    BOF3_INJECT(Area168_ChoiceArm59At10);
    BOF3_INJECT(Area168_ChoiceMessage12);
    BOF3_INJECT(Area168_Tail59);
    BOF3_INJECT(Area169_InitClearFlag74);
    BOF3_INJECT(Area169_MembersFrame);
    BOF3_INJECT(Area169_MemberRect);
    BOF3_INJECT(Area169_TailHealParty);
    BOF3_INJECT(Area169_ArriveHook);
    BOF3_INJECT(Area170_ChoiceArm37At5);
    BOF3_INJECT(Area170_ChoiceArm37At15);
    BOF3_INJECT(Area170_ChoiceArm37At25);
    BOF3_INJECT(Area170_ChoiceState52Or55);
    BOF3_INJECT(Area170_ScriptFlagsSet1010);
    BOF3_INJECT(Area170_ScriptFlagsClear1010);
    BOF3_INJECT(Area170_Init);
    BOF3_INJECT(Area170_Tail37);
    BOF3_INJECT(Area170_StepHook);
    BOF3_INJECT(Area170_Trigger19);
    BOF3_INJECT(Area170_Trigger20);
    BOF3_INJECT(Area170_Trigger56);
    BOF3_INJECT(Area171_ChoiceArmTail10);
    BOF3_INJECT(Area171_Leader89Gate);
    BOF3_INJECT(Area171_SpawnEffect92);
    BOF3_INJECT(Area171_InitClearFlag74);
    BOF3_INJECT(Area171_MembersFrame);
    BOF3_INJECT(Area171_MemberRect);
    BOF3_INJECT(Area171_TailHealParty);
    BOF3_INJECT(Area171_StepHook);
    BOF3_INJECT(Area171_ArriveHook);
    BOF3_INJECT(Area171_Trigger55);
    BOF3_INJECT(Area172_ClearFlag4E);
    BOF3_INJECT(Area172_SkipIfMember89Is4);
    BOF3_INJECT(Area172_RunFall);
    BOF3_INJECT(Area172_FallStart);
    BOF3_INJECT(Area172_FallStep);
    BOF3_INJECT(Area172_RunSlide);
    BOF3_INJECT(Area172_SlideStart);
    BOF3_INJECT(Area172_SlideStep);
    BOF3_INJECT(Area172_Drift);
    BOF3_INJECT(Area172_SpawnEffect92);
    BOF3_INJECT(Area172_TintOn);
    BOF3_INJECT(Area172_TintOff);
    BOF3_INJECT(Area172_ChoiceFlag12);
    BOF3_INJECT(Area172_Tail35);
    BOF3_INJECT(Area172_StepHook);
    BOF3_INJECT(Area172_InitCell);
    BOF3_INJECT(Area172_EffectA5Run);
    BOF3_INJECT(Area172_EffectA5Start);
    BOF3_INJECT(Area172_EffectA5Grow);
    BOF3_INJECT(Area172_EffectA5Hold);
    BOF3_INJECT(Area172_DrawPanel);
    BOF3_INJECT(Area172_DrawShade);
}
