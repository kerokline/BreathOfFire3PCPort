// The band 0x5372E0..0x537F1B - round fourteen, wave two, group R2A: the 19
// functions of the cut (analysis/round14_cut.tsv) and three starts no list
// had (0x537760, 0x537B10, 0x537CE0, each reached by an immediate or a .data
// cell), each read with capstone to its last instruction (docs/rest_2a.md
// section 1). The cut called 18 of them scenario code; none is. Four are
// field-engine helpers (a frame's screen pass, two character-record helpers,
// Area_ObjectHandler); the other 18 are the four object handlers of
// Area_ObjectHandler's fallback table and what they call.
//
// Every call out goes through the scenario harness (SH_CALL / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a .data table's handler is called by the address the cell holds. No
// divergence: each is a faithful replacement. Where the original indexes a
// table past its own count (a dispatcher's stack table, the fallback table,
// Area_Descriptors, the amounts table) or an effect record by an index no
// callee gives, ours aborts with a message (section 5); the one index play
// reaches - a sprite's +0xB of 0xFF, Effect_FindFree's "none free" - is read
// in place as the original reads it (section 6).
#include "game/rest_2a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_2a::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using scenario_harness::Phase;

// A typed data symbol's address (symbols.gen.h makes the name a macro).
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

constexpr unsigned kSpriteStride = 0xA4;
constexpr unsigned kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;

// An effect record by a sprite's +0xB as the handlers read it (`shl eax, 7`
// on the zero-extended byte): 0..19 a record; 0xFF - Effect_FindFree's "none
// free", which the spawns store unchecked - the byte the original reads past
// the pool (rest_2a::at::kEffectNone, inside Gfx_PacketPools), read in place;
// any other byte no code of this band stores, and ours aborts.
const unsigned char* EffectRead(unsigned index, const char* who) {
    if (index < kEffectCount) return Effect_Objects + index * kEffectStride;
    if (index == 0xFF) return At(at::kEffectNone);
    bof3::Fatal("%s: effect object index %u is past Effect_Objects' 20 records; the original reads at 0x%X", who, index,
                static_cast<unsigned>(Addr(Effect_Objects) + index * kEffectStride));
    return nullptr;
}
// The same for a write: only 0..19 (the spawns test 0xFF first).
unsigned char* EffectWrite(unsigned index, const char* who) {
    if (index >= kEffectCount)
        bof3::Fatal("%s: effect object index %u is past Effect_Objects' 20 records; the original writes at 0x%X", who, index,
                    static_cast<unsigned>(Addr(Effect_Objects) + index * kEffectStride));
    return Effect_Objects + index * kEffectStride;
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
    const unsigned char lead = At(at::kLeaderDirection)[0];
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
    const auto offset = static_cast<std::int32_t>(static_cast<U>(reinterpret_cast<std::uintptr_t>(Field_ActiveMember)) -
                                                  Addr(Sprite_Objects));
    return static_cast<U>(offset / static_cast<std::int32_t>(kSpriteStride));
}

// The message the "show" states open: Crt_sprintf(Text_Records,
// Area08_MessageFormat, amount), Msg_OpenSystem(5), Field_Request = 2,
// Sprite_Current (read after the calls) state 4.
void ShowAmount(unsigned amount) {
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(bof3::addr::Text_Records)),
                         reinterpret_cast<const char*>(At(bof3::addr::Area08_MessageFormat)), amount);
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
    At(at::kScriptFlagsHi)[0] |= 1;
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

// CharacterRecords' record n (stride 0xA4), unchecked as the originals index it.
unsigned char* Record(unsigned n) { return At(bof3::addr::CharacterRecords + n * at::kRecordStride); }

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

// original 0x5372E0 (PSX 0x80168778, pairs_propagated gap44, a hypothesis):
// for each of the 30 Sprite_Objects records with +0 bit 0, Sprite_Current =
// the record and: type +6 not 9 - draw slot +0x29 = 4 with +0x24 bit 4, else
// Draw_OtSlot; type 9 - nothing more when Draw_OtSlot is 4. Then type 0xA:
// Sprite_UpdateScreenA. Otherwise the first of the bank list's records
// (count byte 0x8C3580, the records at [0x7E0880], 8 bytes each) whose +7
// equals the sprite's u16 +0x2C (zero-extended); none: the record at the
// count (one past, read as the original reads it). Its bank word +0 in
// Mode11_ScreenBanks (to its 0xFFFF): Sprite_UpdateScreen. Sprite_Current is
// left at the last live record.
extern "C" void __cdecl Mode11_ListedSpriteScreens(void) {
    for (unsigned k = 0; k < 30; ++k) {
        unsigned char* const rec = Sprite_Objects + k * kSpriteStride;
        Sprite_Current = rec;
        if (!(rec[0] & 1)) continue;
        if (rec[6] != 9) {
            rec[0x29] = (rec[0x24] & 0x10) ? 4 : Draw_OtSlot;
        } else if (Draw_OtSlot == 4) {
            continue;
        }
        const unsigned char* const s = Sprite_Current;
        if (s[6] == 0xA) {
            SH_CALL(Sprite_UpdateScreenA)();
            continue;
        }
        const unsigned count = At(at::kBankCount)[0];
        const unsigned char* const records = reinterpret_cast<const unsigned char*>(
            static_cast<std::uintptr_t>(static_cast<U>(Long(At(at::kBankRecords)))));
        const unsigned bank_byte = Word(s + 0x2C);
        unsigned i = 0;
        while (i < count && records[i * 8 + 7] != bank_byte) ++i;
        const unsigned bank = Word(records + (i & 0xFF) * 8);
        for (unsigned j = 0;; ++j) {
            const unsigned listed = Mode11_ScreenBanks[j];
            if (listed == 0xFFFF) break;
            if (listed == bank) {
                SH_CALL(Sprite_UpdateScreen)();
                break;
            }
        }
    }
}

// original 0x5373F0 (PSX 0x80168954, pairs_propagated call-anchored): the
// member byte's record (MoveScript_EffectState, unchecked, as Char_LoseHp):
// with HP +0x18 below its maximum +0x20 (u16), HP = (HP + amount) as a u16,
// held at the maximum (a sum past 0xFFFF wraps first, as the original's
// 16-bit compare has it). Then, only then, the record Field_State +0x148
// names: HP above a quarter of its maximum (>> 2) clears 0x2000 of its state
// word +0x10 and of Field_State's word +0x90. Its eax is no answer (a
// record offset); Field_EquipTick reads none.
extern "C" void __cdecl Char_GainHp(unsigned amount, unsigned member) {
    const unsigned slot = MoveScript_EffectState[member & 0xFF];
    unsigned char* const rec = Record(slot);
    const unsigned max = Word(rec + 0x20);
    const unsigned hp = Word(rec + 0x18);
    if (hp >= max) return;
    const unsigned sum = (hp + amount) & 0xFFFF;
    SetWord(rec + 0x18, sum > max ? max : sum);
    unsigned char* const fs = Field_State;
    unsigned char* const actor = Record(fs[0x148]);
    if (Word(actor + 0x18) <= (Word(actor + 0x20) >> 2)) return;
    SetWord(actor + 0x10, Word(actor + 0x10) & 0xDFFF);
    SetWord(fs + 0x90, Word(fs + 0x90) & 0xDFFF);
}

// original 0x537500 (PSX 0x80168BC0, pairs_propagated call): the member
// byte's record's AP +0x1A down by the amount while it is more (16-bit
// compare), else to 1 (an AP of 0 becomes 1 too). Answers what it took: the
// amount whole, or AP - 1 (0xFFFFFFFF at AP 0). Char_LoseHp without its
// actor test.
extern "C" unsigned __cdecl Char_LoseAp(unsigned amount, unsigned member) {
    const unsigned slot = MoveScript_EffectState[member & 0xFF];
    unsigned char* const rec = Record(slot);
    const unsigned ap = Word(rec + 0x1A);
    unsigned took = amount;
    if (ap <= (amount & 0xFFFF)) took = ap - 1;
    SetWord(rec + 0x1A, ap - took);
    return took;
}

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
    At(at::kScriptFlagsHi)[0] |= 1;
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
    SetLong(EffectWrite(*index, who) + 0xC, 0);
    const unsigned char rise = SH_CALL(LinkedObject_EffectRise)();
    SetLong(EffectWrite(*index, who) + 0x10, static_cast<std::int32_t>(static_cast<signed char>(rise)) + 0x28);
    const unsigned char* const c = Sprite_Current;
    SetLong(EffectWrite(*index, who) + 0x34, Long(c + 0x34));
    SetLong(EffectWrite(*index, who) + 0x38, Long(c + 0x38));
    SetLong(EffectWrite(*index, who) + 0x3C, Long(c + 0x3C));
}

// original 0x537EA0: (s8)(Sprite_Current +0x30 - the leader's +0x30, bytes)
// / 28 (a signed division: imul by 0x92492493, add, sar 4, the sign added);
// negative, shifted left 2 (al only). Answers al.
extern "C" unsigned char __cdecl LinkedObject_EffectRise(void) {
    const auto d = static_cast<signed char>(static_cast<unsigned char>(Sprite_Current[0x30] - At(at::kLeader30)[0]));
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

void Rest2A_Inject() {
    if (bof3::WantsShadow("rest_2a")) rest_2a::SelfTest();
    BOF3_INJECT(Mode11_ListedSpriteScreens);
    BOF3_INJECT(Char_GainHp);
    BOF3_INJECT(Char_LoseAp);
    BOF3_INJECT(Area_ObjectHandler);
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
