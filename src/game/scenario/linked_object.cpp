// Moved here by tools/move_funcs.py from rest_2a.cpp, scena_sc15.cpp (docs/source-layout-round.md).
#include "game/scenario/linked_object.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2a.h"
#include "game/rest_2a_callees.h"
#include "game/scena_sc15.h"
#include "game/scena_sc15_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_2a::at;

}  // namespace

namespace {

using namespace scena_sc15;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using scenario_harness::Phase;

// A typed data symbol's address (symbols.gen.h makes the name a macro).
U Addr(const void* p) { return Low32(p); }

constexpr unsigned kSpriteStride = 0xA4;

constexpr unsigned kEffectStride = 0x80;

constexpr unsigned kEffectCount = 20;

// An effect record by a sprite's +0xB as the handlers read it (`shl eax, 7`
// on the zero-extended byte): 0..19 a record; 0xFF - Effect_FindFree's "none
// free", which the spawns store unchecked - the byte the original reads past
// the pool (rest_2a::at::kEffectNone, inside Gfx_PacketPools), read in place;
// any other byte no code of this band stores, and ours aborts.
const unsigned char* EffectRead(unsigned index, const char* who) {
    if (index < kEffectCount) return rv::Bytes(&Effect_Objects[index]);
    if (index == 0xFF) return At(::bof3::Img(Gfx_PacketPools_addr) + 0x7560);
    bof3::Fatal("%s: effect object index %u is past Effect_Objects' 20 records; the original reads at 0x%X", who, index,
                static_cast<unsigned>(Addr(rv::Bytes(Effect_Objects)) + index * kEffectStride));
    return nullptr;
}

// The same for a write: only 0..19 (the spawns test 0xFF first).
unsigned char* EffectWrite(unsigned index, const char* who) {
    if (index >= kEffectCount)
        bof3::Fatal("%s: effect object index %u is past Effect_Objects' 20 records; the original writes at 0x%X", who, index,
                    static_cast<unsigned>(Addr(rv::Bytes(Effect_Objects)) + index * kEffectStride));
    return rv::Bytes(Effect_Objects) + index * kEffectStride;
}

// A dispatcher's `call [esp + 4 * Sprite_Current[4]]` through the table it
// built on its own stack: past its entries the original calls whatever its
// frame holds above the table (saved registers, its return address); no state
// writes such a byte (section 5), so ours aborts.
void Dispatch(const U* states, unsigned count, const char* who) {
    const unsigned state = Sprite_Current[4];
    if (state >= count)
        bof3::Fatal("%s: state %u is past its stack table's %u entries; the original calls through its own frame", who, state,
                    count);
    Phase(states[state])();
}

// Sprite_Current's object turned away from the leader's direction:
// Sprite_FaceDirection((leader +8 ^ 0xFC) & 7). The original first compares
// the object's +8 with the leader's (setne, xor 4) and jumps on a result that
// is never 0, so the call is unconditional. It pushes eax whole, the upper
// bytes Rand's answer; Sprite_FaceDirection reads the byte.
void FaceAwayFromLeader() {
    const unsigned char lead = ObjTrio[0].direction;
    SH_CALL(Sprite_FaceDirection)(static_cast<unsigned char>((lead ^ 0xFC) & 7));
}

// The end of every object's event: Sprite_Current turned to its own +8 (unless
// +7 bit 3), the active member's context bit 0 cleared, Field_ScriptFlags'
// 0x100 cleared, Sprite_Current's state 0. `s` is Sprite_Current as the caller
// read it before.
void EndEvent(const unsigned char* s) {
    if (!(s[7] & 8)) SH_CALL(Sprite_FaceDirection)(s[8]);
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    unsigned char* const c = Sprite_Current;
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFEFF);
    c[4] = 0;
}

// Field_ActiveMember's index among Sprite_Objects, as the rolls compute it:
// (pointer - Sprite_Objects) / 0xA4, a signed division (imul by 0x63E7063F,
// sar 6, the sign added) - the member need not lie in the array.
U MemberSpriteIndex() {
    const auto offset = static_cast<std::int32_t>(Low32(Field_ActiveMember.Native()) -
                                                  Addr(rv::Bytes(Sprite_Objects)));
    return static_cast<U>(offset / static_cast<std::int32_t>(kSpriteStride));
}

// The message the "show" states open: Crt_sprintf(Text_Records,
// Area08_MessageFormat, amount), Msg_OpenSystem(5), Field_Request = 2,
// Sprite_Current (read after the calls) state 4.
void ShowAmount(unsigned amount) {
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(bof3::addr::Text_Records))),
                         reinterpret_cast<const char*>(At(::bof3::Img(bof3::addr::Area08_MessageFormat))), amount);
    SH_CALL(Msg_OpenSystem)(5);
    unsigned char* const c = Sprite_Current;
    Field_Request = 2;
    c[4] = 4;
}

// The three rolls' shared body. The cases of the original's switch on
// Rand & 0xF (an 8-entry jump table in each, the default for 8..15):
//   0..3   SpawnEffect19(1), state `quiet_state`;
//   4..6   SpawnEffect19(1), Zenny_Add(small, 0), SpawnEffect32(the member's
//          sprite index), state 1 (A, C) or 2 (D);
//   7      the same with Zenny_Add(large, 0), state 2 (A, C) or 3 (D);
//   8..15  SpawnEffect19(other), state `quiet_state`.
// Already marked (+0xA0 bit 7): SpawnEffect19(1) for 8..15, (other) for 0..7,
// state `quiet_state`.
struct Roll {
    unsigned other;          // the second sub-kind: 3 (A, D) or 5 (C)
    unsigned small, large;   // Zenny_Add's amounts
    unsigned char small_state, large_state, quiet_state;
};

void RollBody(const Roll& r) {
    const unsigned roll = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
    unsigned char* const s = Sprite_Current;
    if (!(s[7] & 8)) FaceAwayFromLeader();
    unsigned char* const member = Field_ActiveMember;
    At(::bof3::Img(at::kScriptFlagsHi.v))[0] |= 1;
    const unsigned char mark = member[0xA0];
    if (mark & 0x80) {
        SH_CALL(LinkedObject_SpawnEffect19)(roll > 7 ? 1 : r.other);
        Sprite_Current[4] = r.quiet_state;
        return;
    }
    member[0xA0] = static_cast<unsigned char>(mark | 0x80);
    if (roll <= 3) {
        SH_CALL(LinkedObject_SpawnEffect19)(1);
        Sprite_Current[4] = r.quiet_state;
    } else if (roll <= 7) {
        SH_CALL(LinkedObject_SpawnEffect19)(1);
        SH_CALL(Zenny_Add)(roll == 7 ? r.large : r.small, 0);
        const U index = MemberSpriteIndex();
        SH_CALL(LinkedObject_SpawnEffect32)(index);
        Sprite_Current[4] = roll == 7 ? r.large_state : r.small_state;
    } else {
        SH_CALL(LinkedObject_SpawnEffect19)(r.other);
        Sprite_Current[4] = r.quiet_state;
    }
}

// The states each Run builds on its stack, in the order it stores them.
const U kStatesA[] = {bof3::addr::LinkedObjectA_Roll, bof3::addr::LinkedObject_Show2, bof3::addr::LinkedObject_Show5,
                      bof3::addr::LinkedObject_EndAfterEffect, bof3::addr::LinkedObject_EndAfterMessage};

const U kStatesB[] = {bof3::addr::LinkedObjectB_Roll, bof3::addr::LinkedObject_EndAfterEffect};

const U kStatesC[] = {bof3::addr::LinkedObjectC_Roll, bof3::addr::LinkedObject_Show2, bof3::addr::LinkedObject_Show5,
                      bof3::addr::LinkedObject_EndAfterEffect, bof3::addr::LinkedObject_EndAfterMessage};

const U kStatesD[] = {bof3::addr::LinkedObjectD_Roll,        bof3::addr::LinkedObjectD_ShowAmount,
                      bof3::addr::LinkedObjectD_ShowAmount,  bof3::addr::LinkedObjectD_ShowAmount,
                      bof3::addr::LinkedObjectD_WaitMessage, bof3::addr::LinkedObjectD_ColourStep};

}  // namespace

#define SC15_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

using std::uint32_t;

unsigned char* AtScenaSc15(uint32_t a) { return move_script::At(a); }

std::uint16_t W(uint32_t a) { return move_script::Word(AtScenaSc15(a)); }

uint32_t L(uint32_t a) { return static_cast<uint32_t>(move_script::Long(AtScenaSc15(a))); }

uint32_t L(const unsigned char* p) { return static_cast<uint32_t>(move_script::Long(p)); }

std::uint16_t W(const unsigned char* p) { return move_script::Word(p); }

}  // namespace

// original 0x537540 (PSX 0x801ADE10): Field_ObjectLinked's call. With the
// active member's +0xA0 & 0x7F not 0x7F: a tail jmp through the area's
// handler table (Area_Descriptors[Game_AreaNumber] +0x3C) at that index;
// else through Area_ObjectFallbacks[(short)n]. The handler runs with the
// caller's argument where it was (ours passes n on). The area table has no
// count the code knows (its descriptor's), so its cell is read in place;
// past Area_Descriptors' 200 or the fallback table's 11 ours aborts.
extern "C" void __cdecl Area_ObjectHandler(unsigned short n) {
    using Handler = void (__cdecl*)(unsigned);
    const unsigned char bits = Field_ActiveMember[0xA0];
    U entry;
    if ((bits & 0x7F) != 0x7F) {
        const unsigned area = Game_AreaNumber;
        if (area >= Area_Descriptors_count)
            bof3::Fatal("Area_ObjectHandler (0x537540): area %u is past Area_Descriptors' %u", area, Area_Descriptors_count);
        const unsigned char* const descriptor = Area_Descriptors[area];
        const auto table = static_cast<U>(Long(descriptor + 0x3C));
        entry = static_cast<U>(Long(At(table + 4u * (bits & 0x7F))));
    } else {
        const int index = static_cast<short>(n);
        if (index < 0 || index >= static_cast<int>(Area_ObjectFallbacks_count))
            bof3::Fatal("Area_ObjectHandler (0x537540): %d is past Area_ObjectFallbacks' %u; the original jumps through 0x%X",
                        index, Area_ObjectFallbacks_count, static_cast<unsigned>(Addr(Area_ObjectFallbacks) + 4 * index));
        entry = static_cast<U>(Area_ObjectFallbacks[index]);
    }
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(entry))(n);
}

// original 0x537580: the word at [0x7E0880] + (index & 0xFFFF) * 8 - an
// 8-byte record's first word, the table a pointer in 0x7E0880; eax the word
// zero-extended. Called by area code (0x48A57A, 0x48A59D, testing 0x261 and
// 0x308) and Scena15_Run5 (0x308). Placed by the linker before chapter 0's
// block.
SC15_EXPORT unsigned __cdecl Scena15_RecordWord(unsigned index) {
    return W(L(::bof3::Img(0x7E0880)) + (index & 0xFFFFu) * 8u);
}

// original 0x5375A0 (Area_ObjectFallbacks 0, 7, 9): by Sprite_Current +4
// through its stack table of five.
extern "C" void __cdecl LinkedObjectA_Run(void) { Dispatch(kStatesA, 5, "LinkedObjectA_Run (0x5375A0)"); }

// original 0x5375E0 (LinkedObjectA_Run's state 0): RollBody with the second
// sub-kind 3, Zenny_Add 2 (state 1) or 5 (state 2), otherwise state 3.
extern "C" void __cdecl LinkedObjectA_Roll(void) { RollBody({3, 2, 5, 1, 2, 3}); }

// original 0x537760 (state 1 of A and C, reached only by the two stack
// tables' immediates): while the effect record at +0xB is in use (+0 bit 0)
// nothing; then the message with 2.
extern "C" void __cdecl LinkedObject_Show2(void) {
    const unsigned char* const s = Sprite_Current;
    if (EffectRead(s[0xB], "LinkedObject_Show2 (0x537760)")[0] & 1) return;
    ShowAmount(2);
}

// original 0x5377B0 (state 2 of A and C): the same with 5.
extern "C" void __cdecl LinkedObject_Show5(void) {
    const unsigned char* const s = Sprite_Current;
    if (EffectRead(s[0xB], "LinkedObject_Show5 (0x5377B0)")[0] & 1) return;
    ShowAmount(5);
}

// original 0x537800 (state 3 of A and C, 1 of B): while the effect record at
// +0xB is in use nothing; then the event's end.
extern "C" void __cdecl LinkedObject_EndAfterEffect(void) {
    const unsigned char* const s = Sprite_Current;
    if (EffectRead(s[0xB], "LinkedObject_EndAfterEffect (0x537800)")[0] & 1) return;
    EndEvent(s);
}

// original 0x537850 (state 4 of A and C): while a message is open
// (Field_Request 2) nothing; then the event's end.
extern "C" void __cdecl LinkedObject_EndAfterMessage(void) {
    if (Field_Request == 2) return;
    EndEvent(Sprite_Current);
}

// original 0x5378A0 (Area_ObjectFallbacks 1, 8): by +4 through its stack
// table of two.
extern "C" void __cdecl LinkedObjectB_Run(void) { Dispatch(kStatesB, 2, "LinkedObjectB_Run (0x5378A0)"); }

// original 0x5378D0 (LinkedObjectB_Run's state 0): Rand & 0xF; turned away
// from the leader (unless +7 bit 3); SpawnEffect19(2) above 6, else (3);
// Field_ScriptFlags |= 0x100; state 1. No mark, no zenny.
extern "C" void __cdecl LinkedObjectB_Roll(void) {
    const unsigned roll = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
    const unsigned char* const s = Sprite_Current;
    if (!(s[7] & 8)) FaceAwayFromLeader();
    SH_CALL(LinkedObject_SpawnEffect19)(roll > 6 ? 2 : 3);
    unsigned char* const c = Sprite_Current;
    At(::bof3::Img(at::kScriptFlagsHi.v))[0] |= 1;
    c[4] = 1;
}

// original 0x537950 (Area_ObjectFallbacks 3, 6): by +4 through its stack
// table of five.
extern "C" void __cdecl LinkedObjectC_Run(void) { Dispatch(kStatesC, 5, "LinkedObjectC_Run (0x537950)"); }

// original 0x537990 (LinkedObjectC_Run's state 0): RollBody with the second
// sub-kind 5, Zenny_Add 2 (state 1) or 5 (state 2), otherwise state 3.
extern "C" void __cdecl LinkedObjectC_Roll(void) { RollBody({5, 2, 5, 1, 2, 3}); }

// original 0x537B10 (Area_ObjectFallbacks 5; a start no list had): by +4
// through its stack table of six (states 1..3 one handler).
extern "C" void __cdecl LinkedObjectD_Run(void) { Dispatch(kStatesD, 6, "LinkedObjectD_Run (0x537B10)"); }

// original 0x537B50 (LinkedObjectD_Run's state 0): RollBody with the second
// sub-kind 3, Zenny_Add 1 (state 2) or 10 (state 3), otherwise state 1;
// then, whatever the path, Sprite_Current (re-read for each) +0x5E, +0x5F,
// +0x5D = 0xB0.
extern "C" void __cdecl LinkedObjectD_Roll(void) {
    RollBody({3, 1, 10, 2, 3, 1});
    Sprite_Current[0x5E] = 0xB0;
    Sprite_Current[0x5F] = 0xB0;
    Sprite_Current[0x5D] = 0xB0;
}

// original 0x537CE0 (LinkedObjectD_Run's states 1..3; a start no list had,
// reached by the stack table's immediate): while the effect record at +0xB is
// in use nothing; then LinkedObjectD_Amounts[+4]: not 0 the message with it,
// 0 state 5. The table is read by the state byte unchecked; past its four
// ours aborts (only 1..3 reach it).
extern "C" void __cdecl LinkedObjectD_ShowAmount(void) {
    const char* const who = "LinkedObjectD_ShowAmount (0x537CE0)";
    unsigned char* const s = Sprite_Current;
    if (EffectRead(s[0xB], who)[0] & 1) return;
    const unsigned state = s[4];
    if (state >= LinkedObjectD_Amounts_count)
        bof3::Fatal("%s: state %u is past LinkedObjectD_Amounts' %u; the original reads 0x%X", who, state,
                    LinkedObjectD_Amounts_count, static_cast<unsigned>(Addr(LinkedObjectD_Amounts) + state));
    const unsigned char amount = LinkedObjectD_Amounts[state];
    if (amount == 0) {
        s[4] = 5;
        return;
    }
    ShowAmount(amount);
}

// original 0x537D50 (LinkedObjectD_Run's state 4): Field_Request no longer 2
// - state 5.
extern "C" void __cdecl LinkedObjectD_WaitMessage(void) {
    if (Field_Request != 2) Sprite_Current[4] = 5;
}

// original 0x537D70 (LinkedObjectD_Run's state 5): with +0x5D not 0, +0x5D,
// +0x5F and +0x5E each up by 0x10 (in that order; 0xB0 reaches 0 in five
// frames); at 0 the event's end.
extern "C" void __cdecl LinkedObjectD_ColourStep(void) {
    unsigned char* const s = Sprite_Current;
    const unsigned char c = s[0x5D];
    if (c == 0) {
        EndEvent(s);
        return;
    }
    s[0x5D] = static_cast<unsigned char>(c + 0x10);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0x10);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0x10);
}

// original 0x537DE0: Effect_FindFree into Sprite_Current +0xB; 0xFF -
// nothing more. Else the record (by +0xB, re-read for each store): +0 = 1,
// +5 = 0x19 (the kind), +6 = the argument's byte, dword +0xC = 0; then
// LinkedObject_EffectRise, and with the index re-read through the same
// sprite's +0xB: dword +0x10 = its answer (movsx) + 0x28, dwords +0x34,
// +0x38, +0x3C = those of Sprite_Current read after the call.
extern "C" void __cdecl LinkedObject_SpawnEffect19(unsigned sub) {
    const char* const who = "LinkedObject_SpawnEffect19 (0x537DE0)";
    const unsigned char found = SH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = found;
    unsigned char* const s = Sprite_Current;
    if (s[0xB] == 0xFF) return;
    const unsigned char* const index = s + 0xB;
    EffectWrite(*index, who)[0] = 1;
    EffectWrite(*index, who)[5] = 0x19;
    EffectWrite(*index, who)[6] = static_cast<unsigned char>(sub);
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(EffectWrite(*index, who))->work_0c, 0);
    const unsigned char rise = SH_CALL(LinkedObject_EffectRise)();
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(EffectWrite(*index, who))->work_10, static_cast<std::int32_t>(static_cast<signed char>(rise)) + 0x28);
    const unsigned char* const c = Sprite_Current;
    rv::As<records::EffectObject>(EffectWrite(*index, who))->x = rv::As<records::SpriteObject>(c)->x;
    rv::As<records::EffectObject>(EffectWrite(*index, who))->z = rv::As<records::SpriteObject>(c)->z;
    rv::As<records::EffectObject>(EffectWrite(*index, who))->height = rv::As<records::SpriteObject>(c)->height;
}

// original 0x537EA0: (s8)(Sprite_Current +0x30 - the leader's +0x30, bytes)
// / 28 (a signed division: imul by 0x92492493, add, sar 4, the sign added);
// negative, shifted left 2 (al only). Answers al.
extern "C" unsigned char __cdecl LinkedObject_EffectRise(void) {
    const auto d = static_cast<signed char>(static_cast<unsigned char>(Sprite_Current[0x30] - rv::Bytes(ObjTrio)[0x30]));
    const auto q = static_cast<unsigned char>(static_cast<int>(d) / 28);
    return static_cast<signed char>(q) < 0 ? static_cast<unsigned char>(q << 2) : q;
}

// original 0x537ED0: Effect_FindFree into Sprite_Current +0xB; not 0xFF: the
// record +0 = 1, +5 = 0x32, +6 = the argument's byte.
extern "C" void __cdecl LinkedObject_SpawnEffect32(unsigned sub) {
    const char* const who = "LinkedObject_SpawnEffect32 (0x537ED0)";
    const unsigned char found = SH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = found;
    const unsigned char* const s = Sprite_Current;
    if (s[0xB] == 0xFF) return;
    EffectWrite(s[0xB], who)[0] = 1;
    EffectWrite(s[0xB], who)[5] = 0x32;
    EffectWrite(s[0xB], who)[6] = static_cast<unsigned char>(sub);
}

void LinkedObject_Inject() {
    BOF3_INJECT(Area_ObjectHandler);
    BOF3_INJECT(Scena15_RecordWord);
    BOF3_INJECT(LinkedObjectA_Run);
    BOF3_INJECT(LinkedObjectA_Roll);
    BOF3_INJECT(LinkedObject_Show2);
    BOF3_INJECT(LinkedObject_Show5);
    BOF3_INJECT(LinkedObject_EndAfterEffect);
    BOF3_INJECT(LinkedObject_EndAfterMessage);
    BOF3_INJECT(LinkedObjectB_Run);
    BOF3_INJECT(LinkedObjectB_Roll);
    BOF3_INJECT(LinkedObjectC_Run);
    BOF3_INJECT(LinkedObjectC_Roll);
    BOF3_INJECT(LinkedObjectD_Run);
    BOF3_INJECT(LinkedObjectD_Roll);
    BOF3_INJECT(LinkedObjectD_ShowAmount);
    BOF3_INJECT(LinkedObjectD_WaitMessage);
    BOF3_INJECT(LinkedObjectD_ColourStep);
    BOF3_INJECT(LinkedObject_SpawnEffect19);
    BOF3_INJECT(LinkedObject_EffectRise);
    BOF3_INJECT(LinkedObject_SpawnEffect32);
}
