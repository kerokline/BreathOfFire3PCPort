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
using area_harness::Handler;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
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
// A word in [lo, lo + 3), compared as the originals' `sub; cmp r16, 3; jb`.
bool In3(std::uint16_t v, unsigned lo) { return static_cast<std::uint16_t>(v - lo) < 3; }

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

// ===========================================================================
// Area 168 (descriptor 0x63C900: +0x34 only; no init, no handlers): seven
// choices (choice 2 is 0x425C30, area 167's block) and tail kind 59.
// ===========================================================================

// ===========================================================================
// Area 169 (descriptor 0x63C9F8; PSX 0x801F3B58): the init, the member frame
// and its rectangle search, tail kind 47 and the arrive hook.
// ===========================================================================

// ===========================================================================
// Area 170 (descriptor 0x63D5F8; PSX 0x801F6474): six choices (two also its
// handlers), the init, tail kind 37, the step hook and object triggers 19,
// 20 and 56.
// ===========================================================================

// ===========================================================================
// Area 171 (descriptor 0x63D808; PSX 0x801F4144): three choices (two also its
// handlers), the init, the member frame and its rectangle search (area 169's
// code over its own rectangles), tail kind 48, the step and arrive hooks and
// object trigger 55.
// ===========================================================================

// ===========================================================================
// Area 172 (descriptor 0x63EED0; PSX 0x801F672C): eight handlers (each also a
// choice, one on), choice 0, the fall and slide state machines, tail kind 35,
// the step hook, the init, effect kind 0xA5 with its three states and two
// draws.
// ===========================================================================

void AreaW4b_Inject() {
    if (bof3::WantsShadow("area_w4b")) area_w4b::SelfTest();
}
