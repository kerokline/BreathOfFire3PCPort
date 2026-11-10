// Moved here by tools/move_funcs.py from area_w3a.cpp (docs/source-layout-round.md).
#include "game/area/area_117.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using at::TwinTables;

using move_script::At;

using move_script::SetWord;

using move_script::Word;

unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char* PartyRecord(U index) { return At(::bof3::Img(ObjTrio_addr) + index * at::kPartyStride); }

void SetMessage(unsigned id) { SetWord(At(::bof3::Img(at::kMessage.v)), id); }

// Areas 117 / 118's rectangle search, called by their member frames.
using RectFn = unsigned char (__cdecl*)(unsigned);

// Party record `member`'s words +0x30 (z) and +0x2E (x), the member's id from
// the first party list; Sprite_Current made that record; Effect_Spawn(kind, 0,
// the table's byte by the id (unchecked, as the original's), x, z); an answer
// other than 0xFF stored to Sprite_Current[0xB], Sprite_Current read again
// after the call (area 11's Area11_SpawnEffect is the same shape).
void SpawnAtMember(U member, U table, unsigned char kind) {
    const unsigned char* const rec = PartyRecord(member);
    const auto* const tRec = rv::AsConst<records::PartyMember>(rec);
    const auto z = static_cast<short>(static_cast<std::uint16_t>(tRec->screen_y));
    const auto x = static_cast<short>(static_cast<std::uint16_t>(tRec->screen_x));
    const unsigned id = At(::bof3::Img(Cond_Flags_addr) + 0xd2 + member)[0];
    Sprite_Current = PartyRecord(member);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(At(table + id)[0]), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// The rectangle search (areas 117 / 118's MemberRect): party record `member`
// (& 0xFF; unchecked against the three records)'s x +0x34 and z +0x38 against
// each (x0, z0, x1, z1) of the table, cell bytes << 16, signed and inclusive:
// the first containing it, or 0xFF.
unsigned char MemberRect(const TwinTables& t, unsigned member) {
    const unsigned char* const rec = PartyRecord(member & 0xFF);
    const auto* const tRec = rv::AsConst<records::PartyMember>(rec);
    const std::int32_t x = tRec->x;
    const std::int32_t z = tRec->z;
    unsigned index = 0;
    for (U r = t.rects; r < t.rects_end; r += 6, ++index) {
        const unsigned char* const e = At(r);
        if (x < static_cast<std::int32_t>(static_cast<U>(e[0]) << 16)) continue;
        if (x > static_cast<std::int32_t>(static_cast<U>(e[2]) << 16)) continue;
        if (z < static_cast<std::int32_t>(static_cast<U>(e[1]) << 16)) continue;
        if (z <= static_cast<std::int32_t>(static_cast<U>(e[3]) << 16)) return static_cast<unsigned char>(index);
    }
    return 0xFF;
}

// The member frame (areas 117 / 118's, called by EffectKind70_Run 0x46D780 every field frame
// in the area). Field_ScriptFlags bit 13 cleared; for each member m below
// Field_MemberCount (read again after each member; m a byte), with the bit b
// = 1 << (m & 31) and its low byte b8 (0 for m & 31 of 8 and more, as the
// original's 8-bit shift): Field_State and Sprite_Current made record m, then
// the rectangle search (through the copy's own address). Outside every
// rectangle: a member marked in the caller's +0xB (b8) is unmarked there and
// in Field_ScriptFlags2 (~b, 16 bits). Inside, not yet marked, and b in
// neither Field_ScriptFlags2 nor Field_ScriptFlags: Field_State +0x128 = 2,
// +9 = 0, marked in +0xB and Field_ScriptFlags2; +8 = the rectangle's facing,
// ^ 4 while its story flag is set; Sprite_EnsureAnimation(+8). Inside and
// marked: +9 0 - Field_JumpSetUp, then Field_JumpCamera for member 0 only;
// else the facing (^ 4 by the flag) and, when +8 is not it, +8 = it, the
// three dwords +0xC, +0x10, +0x14 negated and +9 = 8 - +9; then
// Field_LeaderStepTick, +9 - 1, Sprite_ScriptTick and Field_ScriptFlags bit
// 13 set. After the members: Field_Request 5 clears bit 13 again;
// Sprite_Current put back (Field_State is not). Every Sprite_Current use is a
// fresh read, as the original's.
void MembersFrame(const TwinTables& t) {
    unsigned count = Field_MemberCount;
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xDFFF);
    unsigned char* const caller = Sprite_Current;
    if (count != 0) {
        unsigned m = 0;
        do {
            unsigned char* const rec = PartyRecord(m);
            Field_State = rec;
            Sprite_Current = rec;
            const unsigned char found = AH_WORD(RectFn, t.fn_member_rect)(m);
            const U bit = 1u << (m & 31);
            const auto bit8 = static_cast<unsigned char>((m & 31) < 8 ? bit : 0);
            if (found == 0xFF) {
                const unsigned char marks = caller[0xB];
                if ((marks & bit8) != 0) {
                    caller[0xB] = static_cast<unsigned char>(marks & ~bit8);
                    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~bit);
                }
            } else if ((caller[0xB] & bit8) == 0) {
                if (((static_cast<U>(Field_ScriptFlags2) | static_cast<U>(Field_ScriptFlags)) & bit) == 0) {
                    Field_State[at::kFieldStateMember] = 2;
                    Sprite_Current[9] = 0;
                    caller[0xB] = static_cast<unsigned char>(caller[0xB] | bit8);
                    unsigned char* const cur = Sprite_Current;
                    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | bit);
                    const U e = t.rects + 4 + found * 6u;
                    cur[8] = At(e)[0];
                    if (AH_CALL(Flags_Test)(Bank(), At(e + 1)[0]) != 0) Sprite_Current[8] ^= 4;
                    AH_CALL(Sprite_EnsureAnimation)(Sprite_Current[8]);
                }
            } else {
                if (Sprite_Current[9] == 0) {
                    AH_CALL(Field_JumpSetUp)();
                    if (m == 0) AH_CALL(Field_JumpCamera)();
                } else {
                    const U e = t.rects + 4 + found * 6u;
                    unsigned char facing = At(e)[0];
                    if (AH_CALL(Flags_Test)(Bank(), At(e + 1)[0]) != 0) facing ^= 4;
                    unsigned char* o = Sprite_Current;
                    if (o[8] != facing) {
                        o[8] = facing;
                        o = Sprite_Current;
                        rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->step_x, -rv::As<records::SpriteHead>(o)->step_x);
                        o = Sprite_Current;
                        rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->step_z, -rv::As<records::SpriteHead>(o)->step_z);
                        o = Sprite_Current;
                        rv::Store<std::int32_t>(rv::As<records::SpriteHead>(o)->step_y, -rv::As<records::SpriteHead>(o)->step_y);
                        o = Sprite_Current;
                        o[9] = static_cast<unsigned char>(8 - o[9]);
                    }
                }
                AH_CALL(Field_LeaderStepTick)();
                Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
                AH_CALL(Sprite_ScriptTick)();
                Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x2000);
            }
            count = Field_MemberCount;
            m = (m + 1) & 0xFF;
        } while (m < count);
    }
    if (Field_Request == 5) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xDFFF);
    Sprite_Current = caller;
}

// The floor switch (areas 117 / 118's cell hook, Area_CellHooks): the first
// (x, z, facing, flag) record whose x and z are the argument bytes and facing
// the leader's +8 (read before the search); none (the index at the table's
// count) al 0. Story flag 0x1C set al 0. Else Flags_Toggle of the record's
// flag, Effect_HoldFlag1C(0xF), Sound_PlayEffect(0x203), al 1.
unsigned char SwitchHook(const TwinTables& t, unsigned x, unsigned z) {
    const auto cx = static_cast<unsigned char>(x);
    const auto cz = static_cast<unsigned char>(z);
    const unsigned char facing = ObjTrio[0].direction;
    unsigned index = 0;
    for (U r = t.switches; r < t.switches_end; r += 4, ++index) {
        const unsigned char* const e = At(r);
        if (e[0] == cx && e[1] == cz && e[2] == facing) break;
    }
    if (index == t.switch_count) return 0;
    if (AH_CALL(Flags_Test)(Bank(), 0x1C) != 0) return 0;
    AH_CALL(Flags_Toggle)(Bank(), At(t.switches + index * 4u + 3)[0]);
    AH_CALL(Effect_HoldFlag1C)(0xF);
    AH_CALL(Sound_PlayEffect)(0x203);
    return 1;
}

}  // namespace

// original 0x419E70 / 0x419EC0 / 0x419F10 / 0x419F60 (area 117's handlers 0..3,
// Area117_Handlers 0x621588; PSX 0x801F3D70 / 0x801F3DF0 / 0x801F3E70 /
// 0x801F3EF0): party member 2, kinds 3, 4, 5, 2.
extern "C" void __cdecl Area117_Spawn3AtMember2(void) { SpawnAtMember(2, ::bof3::Img(Area117_SpawnKinds3_addr), 3); }

extern "C" void __cdecl Area117_Spawn4AtMember2(void) { SpawnAtMember(2, ::bof3::Img(Area117_SpawnKinds45_addr), 4); }

extern "C" void __cdecl Area117_Spawn5AtMember2(void) { SpawnAtMember(2, ::bof3::Img(Area117_SpawnKinds45_addr), 5); }

extern "C" void __cdecl Area117_Spawn2AtMember2(void) { SpawnAtMember(2, ::bof3::Img(Area117_SpawnKinds2_addr), 2); }

// original 0x419FB0 (Field_ObjectTriggers id 33; a gap of the tool; PSX
// 0x801F3F70 by the gap): ScriptFlags_Set40; tail kind 0x2C (engine,
// 0x56DE50) armed, state 0, sub-kind 5. al 0.
extern "C" unsigned char __cdecl Area117_Trigger33(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    AreaW0a_TailKind = 0x2C;
    AreaW0c_TailState = 0;
    AreaW0c_TailSub = 5;
    return 0;
}

// original 0x419FD0 (area 117's choice 0 and handler 4; PSX 0x801F3FB0): no new
// message; the cursor 0 arms tail kind 0xA (state 0, argument 0x1C).
extern "C" void __cdecl Area117_ChoiceArmTailA(void) {
    const unsigned char cursor = static_cast<unsigned char>(AreaW0c_ChoiceAnswer);
    SetMessage(0xFFFF);
    if (cursor != 0) return;
    AreaW0a_TailKind = 0xA;
    AreaW0c_TailState = 0;
    AreaW0c_TailSub = 0x1C;
}

// original 0x41A000 (choice 1 and handler 5; PSX 0x801F3FF0): the message
// Area117_Messages1[the cursor, s8 - unchecked, a negative one reads before
// the table]; a cursor 0..4: counter 0 = 0xA and Sound_PlayEffect(0x205).
extern "C" void __cdecl Area117_ChoiceMessage1(void) {
    const auto cursor = AreaW0c_ChoiceAnswer;
    SetMessage(Word(At(::bof3::Img(Area117_Messages1_addr) + static_cast<U>(cursor * 2))));
    if (cursor < 0 || cursor > 4) return;
    At(::bof3::Img(at::kCounter0.v))[0] = 0xA;
    AH_CALL(Sound_PlayEffect)(0x205);
}

// original 0x41A040 (choice 2 and handler 6; PSX 0x801F4048): the message
// Area117_Messages2[the cursor, s8, unchecked]; then by a five-entry jump
// table in the function (0x41A080) for a cursor 0..4 (unsigned): 2 sets
// counter 0 = 0x14; 0, 1, 3, 4 counter 0 = 0xA and Sound_PlayEffect(0x205).
extern "C" void __cdecl Area117_ChoiceMessage2(void) {
    const int cursor = AreaW0c_ChoiceAnswer;
    SetMessage(Word(At(::bof3::Img(Area117_Messages2_addr) + static_cast<U>(cursor * 2))));
    if (static_cast<U>(cursor) > 4) return;
    if (cursor == 2) {
        At(::bof3::Img(at::kCounter0.v))[0] = 0x14;
        return;
    }
    At(::bof3::Img(at::kCounter0.v))[0] = 0xA;
    AH_CALL(Sound_PlayEffect)(0x205);
}

// original 0x41A0A0 (called by 0x46D7A9, EffectKind70_Run's case for area 117): the
// member frame over Area117_Rects.
extern "C" void __cdecl Area117_MembersFrame(void) { MembersFrame(at::kTw117); }

// original 0x41A2D0 (called by Area117_MembersFrame; a gap of the tool): one
// rectangle, Area117_Rects 0x6215A8.
extern "C" unsigned char __cdecl Area117_MemberRect(unsigned member) { return MemberRect(at::kTw117, member); }

// original 0x41A340 (Area_CellHooks' entry for area 0x75): one switch,
// Area117_Switches 0x62160C.
extern "C" unsigned char __cdecl Area117_SwitchHook(unsigned x, unsigned z) { return SwitchHook(at::kTw117, x, z); }

void Area117_Inject() {
    BOF3_INJECT(Area117_Spawn3AtMember2);
    BOF3_INJECT(Area117_Spawn4AtMember2);
    BOF3_INJECT(Area117_Spawn5AtMember2);
    BOF3_INJECT(Area117_Spawn2AtMember2);
    BOF3_INJECT(Area117_Trigger33);
    BOF3_INJECT(Area117_ChoiceArmTailA);
    BOF3_INJECT(Area117_ChoiceMessage1);
    BOF3_INJECT(Area117_ChoiceMessage2);
    BOF3_INJECT(Area117_MembersFrame);
    BOF3_INJECT(Area117_MemberRect);
    BOF3_INJECT(Area117_SwitchHook);
}
