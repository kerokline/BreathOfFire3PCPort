// Moved here by tools/move_funcs.py from area_w4b.cpp (docs/source-layout-round.md).
#include "game/area/area_171.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4b::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

// ObjTrio record `m` (0x14C bytes; not checked, as the originals' index).
unsigned char* PartyRecord(unsigned m) { return Mem(::bof3::Img(ObjTrio_addr) + m * at::kPartyStride); }

// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* Story() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

// The tail armed: ScriptFlags_Set40, then kind and state.
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}

// The tail ended: kind and state 0.
void EndTail() {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// The tail disarmed: ScriptFlags_Clear40, kind and state 0.
void Disarm() {
    AH_CALL(ScriptFlags_Clear40)();
    EndTail();
}

// The map byte setter, as the originals pass it: x and z, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// Engine tail kind 10 armed at state 5 with sub-kind 0xFF (area 168's choice
// 1 on answer 3, area 171's choice 0 on answer 0).
void ArmTail10() {
    ArmTail(0xA, 5);
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0xFF;
}

// Areas 169 and 171's rectangle search (0x4269F0, 0x4278B0; one body over a
// Rects each): party record member & 0xFF's x and z (+0x34, +0x38) against
// each rectangle's (x0, z0, x1, z1) bytes << 16, signed, inclusive; the first
// that holds both: its index. None: 0xFF.
unsigned char MemberRect(const at::Rects& t, unsigned member) {
    const unsigned char* const record = PartyRecord(member & 0xFF);
    const auto* const tRecord = rv::AsConst<records::PartyMember>(record);
    const std::int32_t x = tRecord->x;
    const std::int32_t z = tRecord->z;
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
// engine's EffectKind70_Run 0x46D780 every field frame in the area, one body over a Rects
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
            const unsigned char found = AH_WORD(RectFn, t.fn_rect)(m);
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
                        rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->step_x, static_cast<std::int32_t>(0u - static_cast<U>(rv::As<records::SpriteHead>(o)->step_x)));
                        o = Sprite_Current;
                        rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->step_z, static_cast<std::int32_t>(0u - static_cast<U>(rv::As<records::SpriteHead>(o)->step_z)));
                        o = Sprite_Current;
                        rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->step_y, static_cast<std::int32_t>(0u - static_cast<U>(rv::As<records::SpriteHead>(o)->step_y)));
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
        if (Transition_Busy != 0) return;
        AH_CALL(Sound_PlayEffect)(0x202);
        AH_CALL(Party_HealJoined)();
        AH_CALL(Transition_Start)(9);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 2;
        return;
    case 2:
        if (Transition_Busy != 0) return;
        AH_CALL(Msg_OpenScript)(1);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 3;
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
    const auto distance = static_cast<std::int32_t>(Low32(Field_ActiveMember.Native()) - ::bof3::Img(Sprite_Objects_addr));
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x92;
    tE->screen_x = 0;
    tE->screen_y = y;
    e[0x29] = slot29;
    e[0xB] = static_cast<unsigned char>(distance / static_cast<std::int32_t>(at::kObjectStride));
}

}  // namespace

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
    if (B(::bof3::Img(ObjTrio_addr) + 0x89) == 5) {
        if (B(::bof3::Img(AreaW4b_GateKey_addr)) != 0x4B) return;
        AH_CALL(Flags_Set)(Story(), 0x73);
        SetByte(2, 0x21, 0xC0);
        SetByte(2, 0x22, 0xC0);
        SetByte(3, 0x21, 0);
        SetByte(3, 0x22, 0);
        AddWord(Field_ActiveMember + 0x8A, 1);
    }
    if (B(::bof3::Img(ObjTrio_addr) + 0x89) == 7) ArmTail(0x30, 0xA);
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

// original 0x4276D0 (called by 0x46D7D0, EffectKind70_Run's case for area 171; a gap
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
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    switch (state) {
    case 0:
        if (Cond_ByteFD != 3) {
            Disarm();
            return;
        }
        AH_CALL(Transition_Start)(8);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    case 10:
        if (B(::bof3::Img(ObjTrio_addr) + 0x137) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Party_DropIn)(0);
        EndTail();
        return;
    case 20:
        AH_CALL(Msg_OpenScript)(2);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x15;
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
    B(::bof3::Img(at::kScratch.v)) = flag;
    const auto cx = static_cast<short>(High(x));
    const auto cz = static_cast<short>(High(z));
    if (AH_CALL(AreaMap_ByteAt)(cx, cz) == 0x89) ++B(::bof3::Img(at::kScratch.v));
    const bool fx = (static_cast<U>(x) & 0xFFFF) != 0;
    if (fx && AH_CALL(AreaMap_ByteAt)(static_cast<short>(cx + 1), cz) == 0x89) ++B(::bof3::Img(at::kScratch.v));
    const bool fz = (static_cast<U>(z) & 0xFFFF) != 0;
    if (fz && AH_CALL(AreaMap_ByteAt)(cx, static_cast<short>(cz + 1)) == 0x89) ++B(::bof3::Img(at::kScratch.v));
    if (fx && fz && AH_CALL(AreaMap_ByteAt)(static_cast<short>(cx + 1), static_cast<short>(cz + 1)) == 0x89) ++B(::bof3::Img(at::kScratch.v));
    if (B(::bof3::Img(at::kScratch.v)) != 0) ArmTail(0x30, 0x14);
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

void Area171_Inject() {
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
}
