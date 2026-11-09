// Moved here by tools/move_funcs.py from area_w4b.cpp (docs/source-layout-round.md).
#include "game/area/area_169.h"

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

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

// ObjTrio record `m` (0x14C bytes; not checked, as the originals' index).
unsigned char* PartyRecord(unsigned m) { return Mem(::bof3::Img(ObjTrio_addr) + m * at::kPartyStride); }

unsigned char* Story() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

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

}  // namespace

// original 0x4267F0 (area 169 +0x40; PSX 0x801F3524): Cond_ByteFD not 1:
// story flag 0x74 cleared.
extern "C" void __cdecl Area169_InitClearFlag74(void) {
    if (Cond_ByteFD != 1) AH_CALL(Flags_Clear)(Story(), 0x74);
}

// original 0x426810 (called by 0x46D7C3, EffectKind70_Run's case for area 169; a gap
// of the tool): the member frame over Area169_Rects.
extern "C" void __cdecl Area169_MembersFrame(void) { MembersFrame(at::kRects169); }

// original 0x4269F0 (called by Area169_MembersFrame; a gap of the tool): one
// rectangle of Area169_Rects 0x63CA3C holding the member, or 0xFF.
extern "C" unsigned char __cdecl Area169_MemberRect(unsigned member) { return MemberRect(at::kRects169, member); }

// original 0x426A60 (tail kind 47): state 0: Transition_Start(8), state 1;
// 1..3: HealStates. Any other state: nothing.
extern "C" void __cdecl Area169_TailHealParty(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    if (state == 0) {
        AH_CALL(Transition_Start)(8);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
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

void Area169_Inject() {
    BOF3_INJECT(Area169_InitClearFlag74);
    BOF3_INJECT(Area169_MembersFrame);
    BOF3_INJECT(Area169_MemberRect);
    BOF3_INJECT(Area169_TailHealParty);
    BOF3_INJECT(Area169_ArriveHook);
}
