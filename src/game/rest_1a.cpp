// Group R1A of round fourteen, wave one (docs/rest_1a.md): the 49 functions
// 0x51BA80..0x51D70C of the cut (analysis/round14_cut.tsv) - three
// party-member states and the field actions of party sets 0, 1 and 2, with the
// dispatchers that reach them. Each read with capstone to its last instruction.
//
// Every call out goes through the scenario harness (SH_CALL), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies; every
// table dispatch reads the table's word in place (CodeAt), which the fuzz
// swaps for recorders. No divergence: each is a faithful replacement.
// Sprite_Current and Field_State are read afresh at every use, as the
// original reads [0x937F88] / [0x905D98] (a callee may move either). Where the
// original would jump through a table word that is not code, or write an
// object record by an index its callee never answers, ours aborts with a
// message (docs/rest_1a.md section 5).
#include "game/rest_1a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_0a.h"
#include "game/rest_1a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_1a::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

unsigned char& B(U a) { return *At(a); }
std::int32_t L(U a) { return Long(At(a)); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Fs() { return Field_State; }

// ObjTrio's record i (stride 0x14C), the index unchecked as the original's.
const unsigned char* Member(unsigned i) { return ObjTrio + i * at::kMemberStride; }

// Field_DirectionSteps' row for a direction (x then z), read in place with the
// direction byte unmasked, as every reader of the table does.
U StepX(unsigned d) { return static_cast<U>(L(at::kSteps + d * 8)); }
U StepZ(unsigned d) { return static_cast<U>(L(at::kSteps + d * 8 + 4)); }

// `cdq; xor eax, edx; sub eax, edx`: the absolute value with the original's
// wrap (the most negative stays negative).
std::int32_t Abs32(std::int32_t v) {
    const auto u = static_cast<U>(v);
    const U s = static_cast<U>(v >> 31);
    return static_cast<std::int32_t>((u ^ s) - s);
}

// The animation offset of a direction: (d - 1) / 2 as a signed division
// (dec, cdq, sub, sar), so direction 0 gives 0.
unsigned char HalfTurn(unsigned char d) { return static_cast<unsigned char>((static_cast<int>(d) - 1) / 2); }

// A .data dispatch table read in place: the index unchecked, as the
// original's; where the word is not code (past the run of code-pointer tables
// the entry lies in) the original jumps into data - ours aborts. While the
// fuzz runs, the entries are its recorders (outside .text).
using Handler = void (__cdecl*)();
Handler CodeAt(U table, unsigned index, const char* who) {
    const U cell = table + 4u * index;
    const auto entry = static_cast<U>(L(cell));
    if (!scenario_harness::g_active && (entry < 0x401000 || entry >= 0x5C3000))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X, not code - past its table (the original jumps there)", who, index,
                    static_cast<unsigned>(entry), static_cast<unsigned>(cell));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(entry));
}

// Sprite_ObjectAt's answer marked: bit 0 of the object's +0x80 - 0..0x1D a
// Sprite_Objects record, 0x1E..0x21 a Sprite_ObjectsExtra one. The original
// compares the index signed (0x51C190's three copies, 0x51D2F0: movsx, jge)
// or unsigned (0x51C530: jae) and writes whatever record that names; the
// callee answers 0..0x21 or 0xFF only, so ours aborts on any other.
void MarkObject(unsigned char found, const char* who) {
    if (found > 0x21) bof3::Fatal("%s: Sprite_ObjectAt answered 0x%X, no object (it answers 0..0x21 or 0xFF); the original marks a record past the tables", who, found);
    if (found < 0x1E) B(at::kObjectFlags + found * at::kObjectStride) |= 1;
    else B(at::kExtraFlags + (found - 0x1Eu) * at::kObjectStride) |= 1;
}

// An Effect_Objects record by an index a callee answered (Field_EffectAhead,
// PartyAction_Kind30Ahead: 0..19 or 0xFF, tested first). The original uses it
// unchecked; ours aborts past the 20 records.
unsigned char* EffectByAnswer(unsigned char i, const char* who) {
    if (i >= 20) bof3::Fatal("%s: effect object index 0x%X is past Effect_Objects' 20 records (its callee answers 0..19)", who, i);
    return Effect_Objects + i * 0x80u;
}

// The party sets' cell helpers, which their callers reach by E8.
using CellFn = unsigned char (__cdecl*)(unsigned, unsigned);

// --- the shared bodies of the three sets' identical copies -------------------------

// 0x51BFD0 / 0x51C7C0 / 0x51CD10 (and set 5's 0x51E930, ours in field_hidden):
// an even direction +8 is turned one eighth back (-1, & 7); when
// PartyAction_TargetAhead finds nothing that way, two on (+2), and when it
// finds nothing there either, back to the first turn (-2). Then the slope one
// step ahead in +8 (MapView_SlopeAt with the whole direction byte): steep (the
// scratch flag set and the low word above 0x40, signed) - the pose
// (+8 - 1) / 2 + 0x46 and +2 one on; otherwise +0x2B = 1, the side probes in
// directions 3 and 5 (Field_DirectionSteps' rows read by address; each clears
// it again on a steep slope with the ground above the sprite's +0x3E),
// Sound_PlayEffect(u16 +0x2C + 0x100), the pose (+8 - 1) / 2 + 0x42 and +0xA =
// 5. Then +0xB = 0 and +2 one on - a steep slope moves +2 by two.
void SideProbe(U row, unsigned d) {
    const unsigned char* const s = Sc();
    const U x = static_cast<U>(Long(s + 0x34)) + static_cast<U>(L(row));
    const U z = static_cast<U>(Long(s + 0x38)) + static_cast<U>(L(row + 4));
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (B(at::kScratchFlag) == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    unsigned char* const c = Sc();
    if (static_cast<short>(Word(c + 0x3E)) < static_cast<short>(ground)) c[0x2B] = 0;
}

void Form0Begin() {
    if ((Sc()[8] & 1) == 0) {
        Sc()[8] = static_cast<unsigned char>((Sc()[8] - 1) & 7);
        if (SH_CALL(PartyAction_TargetAhead)() == 0) {
            Sc()[8] = static_cast<unsigned char>((Sc()[8] + 2) & 7);
            if (SH_CALL(PartyAction_TargetAhead)() == 0) Sc()[8] = static_cast<unsigned char>((Sc()[8] - 2) & 7);
        }
    }
    const unsigned char* const s = Sc();
    const unsigned d = s[8];
    const U x = static_cast<U>(Long(s + 0x34)) + StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + StepZ(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (B(at::kScratchFlag) != 0 && static_cast<short>(slope) > 0x40) {
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x46));
        ++Sc()[2];
    } else {
        Sc()[0x2B] = 1;
        SideProbe(at::kSideStep3, 3);
        SideProbe(at::kSideStep5, 5);
        SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(Sc() + 0x2C) + 0x100));
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x42));
        Sc()[0xA] = 5;
    }
    Sc()[0xB] = 0;
    ++Sc()[2];
}

// 0x51C190 / 0x51C980 / 0x51CED0 (set 5's 0x51EAF0): +0xA counted down; at 0,
// the point two steps ahead in +8. The object Sprite_ObjectAt(point, 0) finds
// there is marked (MarkObject, the index compared signed); then the set's cell
// pickup on the point's cell, and when that answers 0, on the cell one on in x
// (only when the point's x has a fraction), then, when that answers 0 too or
// was not asked, one on in z (when z has one); then +2 one on.
// Sprite_ScriptTickOnce every time. The cells are the point's high words; the
// original passes them as dwords whose upper halves are its own stack (the
// pickup reads only the low word: docs/rest_1a.md section 3).
void Form0Resolve(CellFn pickup, const char* who) {
    --Sc()[0xA];
    if (Sc()[0xA] == 0) {
        const unsigned char* const s = Sc();
        const unsigned d = s[8];
        const U x = static_cast<U>(Long(s + 0x34)) + 2u * StepX(d);
        const U z = static_cast<U>(Long(s + 0x38)) + 2u * StepZ(d);
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
        if (found != 0xFF) MarkObject(found, who);
        const unsigned cx = x >> 16, cz = z >> 16;
        if (pickup(cx, cz) == 0) {
            bool done = false;
            if ((x & 0xFFFF) != 0) done = pickup(cx + 1, cz) != 0;
            if (!done && (z & 0xFFFF) != 0) pickup(cx, cz + 1);
        }
        ++Sc()[2];
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x51C270 / 0x51CA60 / 0x51CFB0 (set 5's Field_CellPickup 0x51EBD0): what
// lies in the map cell (x, z) - AreaMap_ByteAt:
//   0xF2 - with an effect object free (Effect_FindFree): Effect_SpawnAtCell(0,
//          x, z); a Rand nibble of 13..15 finds zenny, 2 (5 on 15), ten times
//          that when Field_InputFlags has bit 1 or 2 and a second Rand & 3 is
//          0: Field_GiveZenny(the amount), Effect_SpawnAtCell(1, x, z); then
//          +0xB = 1. With no object free, none of that.
//   0xF8 - Effect_SpawnAtCell(0, x, z); the name of item 0x56 of category 0
//          (Item_NamePtr) copied, 16 bytes, into Text_Records; Inventory_Add(0,
//          0x56, 1): taken, Sound_PlayEffect(0x106) and Msg_OpenSystem(2), else
//          Msg_OpenSystem(3); Field_Request = 2, +0xB = 1.
// Both clear the cell (AreaMap_ClearCell) and answer 1; any other cell 0. x
// and z go on as given (every callee reads their low words); the original
// keeps the amount in its own z slot's low byte, which nothing reads again.
unsigned char CellPickup(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF2) {
        if (SH_CALL(Effect_FindFree)() != 0xFF) {
            SH_CALL(Effect_SpawnAtCell)(0, x, z);
            const unsigned roll = static_cast<unsigned char>(SH_CALL(Rand)()) & 0xFu;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (static_cast<unsigned char>(SH_CALL(Rand)()) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * 10);
                SH_CALL(Field_GiveZenny)(amount);
                SH_CALL(Effect_SpawnAtCell)(1, x, z);
            }
            Sc()[0xB] = 1;
        }
    } else if (cell == 0xF8) {
        SH_CALL(Effect_SpawnAtCell)(0, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x56);
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(bof3::addr::Text_Records + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x56, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Field_Request = 2;
        Sc()[0xB] = 1;
    } else {
        return 0;
    }
    SH_CALL(AreaMap_ClearCell)(x, z);
    return 1;
}

// 0x51D160 (by facings 1 and 7) and 0x51C490 (by 3 and 5): when +8 has come
// round to +3, the pose for the facing reached (0x40 / 0x41, none for any
// other) and +2 one on; else +9 counted down and, at 0, +9 = 2, +8 += +0xB
// (Sprite_TurnSense's step), & 7, and the pose +8.
void TurnToward(unsigned char lo, unsigned char lo_pose, unsigned char hi, unsigned char hi_pose) {
    unsigned char* const s = Sc();
    const unsigned char target = s[3];
    if (s[8] == target) {
        if (target == lo) SH_CALL(Sprite_EnsureAnimation)(lo_pose);
        else if (target == hi) SH_CALL(Sprite_EnsureAnimation)(hi_pose);
        ++Sc()[2];
        return;
    }
    --s[9];
    if (Sc()[9] != 0) return;
    Sc()[9] = 2;
    Sc()[8] = static_cast<unsigned char>(Sc()[8] + Sc()[0xB]);
    Sc()[8] = static_cast<unsigned char>(Sc()[8] & 7);
    SH_CALL(Sprite_EnsureAnimation)(Sc()[8]);
}

}  // namespace

// ===========================================================================
// The party members' states (Member_States 0x65F960, Field_MemberFrame's by +1)
// ===========================================================================

// original 0x51BA80 (0x14 bytes), state 4: back to state 1 (Member_Control)
// unless bit 11 of Field_ScriptFlags2 (test ah, 8 of the dword 0x905BA4) holds
// the member - Member_ResumeUnlessHeld (state 3) with bit 10.
extern "C" void __cdecl Member_ResumeUnlessHeld800(void) {
    if ((Field_ScriptFlags2 & 0x800) == 0) Sc()[1] = 1;
}

// original 0x51BAA0 (0x125 bytes), state 6, the member's form action: it
// stops (Field_State +0x137 = 0) when the leader (ObjTrio +0x89) is of kind 3
// or 6 in its state 0xA step 1, when Field_Request is set, when the member is
// more than a reach from the point the member it follows (+6) will be at -
// that record's +0x34 / +0x38 plus its +0xC / +0x10 times its +9; the reach
// 0x20000 when its +0x70 byte is set, else 0x18000 (each axis, |d| signed with
// the original's wrap) -, or when Field_ScriptFlags2 has a bit of 0x1C00; else
// the handler of Field_FormActions for the loaded party set (0x90412C & 0x7F,
// 19 entries, unchecked). Then, once Field_State +0x137 is 0: the pose +8, +9
// = 0, the member +5 cleared (Member_ClearState), +1 = 1, +0xB = 0 and
// Member_Follow (a tail jump). FE1's Field_FormActionState 0x52F4F0 is the
// leader's. As the original has it: +6 indexes ObjTrio unchecked.
extern "C" void __cdecl Member_FormActionState(void) {
    unsigned char stop = 0;
    const unsigned char kind = B(at::kLeaderKind);
    if ((kind == 3 || kind == 6) && B(at::kLeaderState) == 0xA && B(at::kLeaderStep) == 1) stop = 1;
    if (Field_Request != 0) stop = 1;
    const unsigned char* const s = Sc();
    const unsigned char* const r = Member(s[6]);
    const U k = r[9];
    const U ax = static_cast<U>(Long(r + 0xC)) * k + static_cast<U>(Long(r + 0x34));
    const U az = static_cast<U>(Long(r + 0x10)) * k + static_cast<U>(Long(r + 0x38));
    const std::int32_t reach = r[0x70] != 0 ? 0x20000 : 0x18000;
    if (Abs32(static_cast<std::int32_t>(static_cast<U>(Long(s + 0x34)) - ax)) > reach ||
        Abs32(static_cast<std::int32_t>(static_cast<U>(Long(s + 0x38)) - az)) > reach)
        stop = 1;
    if ((Field_ScriptFlags2 & 0x1C00) == 0 && stop == 0)
        CodeAt(at::kFormActions, B(at::kPartySet) & 0x7Fu, "Member_FormActionState (0x51BAA0)")();
    else
        Fs()[0x137] = 0;
    if (Fs()[0x137] != 0) return;
    SH_CALL(Sprite_EnsureAnimation)(Sc()[8]);
    Sc()[9] = 0;
    SH_CALL(Member_ClearState)(Sc()[5]);
    Sc()[1] = 1;
    Sc()[0xB] = 0;
    SH_CALL(Member_Follow)();
}

// original 0x51BCF0 (0x12 bytes), state 8, the member's jump: jmp
// Member_JumpSteps 0x65F9A4[+2] (Field_JumpBegin, Field_JumpOut,
// Member_JumpAir, Field_JumpIn - the leader's Field_JumpSteps with this
// group's step 2).
extern "C" void __cdecl Member_JumpState(void) { CodeAt(at::kMemberJumpSteps, Sc()[2], "Member_JumpState (0x51BCF0)")(); }

// original 0x51BD10 (0x8D bytes), jump step 2: the member against the one it
// follows (ObjTrio record +6, unchecked) - when that one's +0x137 is 3: more
// than 0x80 apart in height (+0x3E, s16; |d| of the sign-extended words),
// above it Leader_Sink, below or level Leader_Rise; within 0x80 nothing. Any
// other +0x137: at or below its height Leader_Rise, above it Leader_TurnBack.
extern "C" void __cdecl Member_JumpAir(void) {
    const unsigned char* const s = Sc();
    const unsigned char* const r = Member(s[6]);
    const auto theirs = static_cast<short>(Word(r + 0x3E));
    unsigned char how;
    if (r[0x137] == 3) {
        const auto mine = static_cast<short>(Word(s + 0x3E));
        if (Abs32(static_cast<std::int32_t>(mine) - static_cast<std::int32_t>(theirs)) > 0x80) how = mine > theirs ? 1 : 0;
        else how = 3;
    } else {
        how = static_cast<short>(Word(s + 0x3E)) <= theirs ? 0 : 2;
    }
    switch (how) {
    case 0: SH_CALL(Leader_Rise)(); break;
    case 1: SH_CALL(Leader_Sink)(); break;
    case 2: SH_CALL(Leader_TurnBack)(); break;
    default: break;
    }
}

// ===========================================================================
// The dispatchers: a table's word by the form (u16 +0x2C), the state (+2) or
// the step (+3), jumped to, the index unchecked
// ===========================================================================

// original 0x51C740: Field_FormActions[0] - PartyFormAction0_Forms 0x65F9F4 by u16 +0x2C.
extern "C" void __cdecl PartyFormAction0_ByForm(void) { CodeAt(at::kFormAction0Forms, Word(Sc() + 0x2C), "PartyFormAction0_ByForm (0x51C740)")(); }
// original 0x51C760: Field_ActionBySet[0] - PartyAction0_Forms 0x65FA00 by u16 +0x2C.
extern "C" void __cdecl PartyAction0_ByForm(void) { CodeAt(at::kAction0Forms, Word(Sc() + 0x2C), "PartyAction0_ByForm (0x51C760)")(); }
// original 0x51CC00: Field_FormActions[1] - 0x65FA50 by u16 +0x2C.
extern "C" void __cdecl PartyFormAction1_ByForm(void) { CodeAt(at::kFormAction1Forms, Word(Sc() + 0x2C), "PartyFormAction1_ByForm (0x51CC00)")(); }
// original 0x51CC20: Field_ActionBySet[1] - 0x65FA5C by u16 +0x2C.
extern "C" void __cdecl PartyAction1_ByForm(void) { CodeAt(at::kAction1Forms, Word(Sc() + 0x2C), "PartyAction1_ByForm (0x51CC20)")(); }

// original 0x51BE90: 0x65F9B4 by +2 (PartyFormAction_Form0Begin, PartyFormAction_Form0Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction0_Form0(void) { CodeAt(at::kFormAction0Form0States, Sc()[2], "PartyFormAction0_Form0 (0x51BE90)")(); }
// original 0x51C430: 0x65F9CC by +2 (PartyFormAction_Form1Begin, PartyFormAction_Form1Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction0_Form1(void) { CodeAt(at::kFormAction0Form1States, Sc()[2], "PartyFormAction0_Form1 (0x51C430)")(); }
// original 0x51C470: 0x65F9E0 by +2 (0x520840, PartyFormAction_Form2Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction0_Form2(void) { CodeAt(at::kFormAction0Form2States, Sc()[2], "PartyFormAction0_Form2 (0x51C470)")(); }
// original 0x51BFB0: PartyAction0_Form0States 0x65F9C0 by +2.
extern "C" void __cdecl PartyAction0_Form0(void) { CodeAt(at::kAction0Form0States, Sc()[2], "PartyAction0_Form0 (0x51BFB0)")(); }
// original 0x51C450: 0x65F9D8 by +2 (0x5252B0, 0x521A20).
extern "C" void __cdecl PartyAction0_Form1(void) { CodeAt(at::kAction0Form1States, Sc()[2], "PartyAction0_Form1 (0x51C450)")(); }
// original 0x51C510: 0x65F9EC by +2 (PartyAction0_Form2Begin, 0x521C40).
extern "C" void __cdecl PartyAction0_Form2(void) { CodeAt(at::kAction0Form2States, Sc()[2], "PartyAction0_Form2 (0x51C510)")(); }
// original 0x51C780: 0x65FA0C by +2 (PartyFormAction_Form0Begin, PartyFormAction_Form0Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction1_Form0(void) { CodeAt(at::kFormAction1Form0States, Sc()[2], "PartyFormAction1_Form0 (0x51C780)")(); }
// original 0x51CB80: 0x65FA24 by +2 (PartyFormAction_Form1Begin, PartyFormAction_Form1Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction1_Form1(void) { CodeAt(at::kFormAction1Form1States, Sc()[2], "PartyFormAction1_Form1 (0x51CB80)")(); }
// original 0x51CBC0: 0x65FA38 by +2 (0x520840, 0x51FC80, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction1_Form2(void) { CodeAt(at::kFormAction1Form2States, Sc()[2], "PartyFormAction1_Form2 (0x51CBC0)")(); }
// original 0x51C7A0: PartyAction1_Form0States 0x65FA18 by +2.
extern "C" void __cdecl PartyAction1_Form0(void) { CodeAt(at::kAction1Form0States, Sc()[2], "PartyAction1_Form0 (0x51C7A0)")(); }
// original 0x51CBA0: 0x65FA30 by +2 (0x5252B0, 0x521A20).
extern "C" void __cdecl PartyAction1_Form1(void) { CodeAt(at::kAction1Form1States, Sc()[2], "PartyAction1_Form1 (0x51CBA0)")(); }
// original 0x51CBE0: 0x65FA44 by +2 (0x5239F0, 0x51DE20, 0x520350).
extern "C" void __cdecl PartyAction1_Form2(void) { CodeAt(at::kAction1Form2States, Sc()[2], "PartyAction1_Form2 (0x51CBE0)")(); }
// original 0x51CC40: 0x65FA68 by +2 (PartyFormAction_Form0Begin, PartyFormAction_Form0Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction2_Form0(void) { CodeAt(at::kFormAction2Form0States, Sc()[2], "PartyFormAction2_Form0 (0x51CC40)")(); }
// original 0x51D0D0: 0x65FA80 by +2 (PartyFormAction_Form1Begin, PartyFormAction_Form1Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction2_Form1(void) { CodeAt(at::kFormAction2Form1States, Sc()[2], "PartyFormAction2_Form1 (0x51D0D0)")(); }
// original 0x51D200: 0x65FA94 by +2 (0x520840, 0x51FC80, BossOp_ScriptTick).
extern "C" void __cdecl PartyFormAction2_Form2(void) { CodeAt(at::kFormAction2Form2States, Sc()[2], "PartyFormAction2_Form2 (0x51D200)")(); }
// original 0x51CCF0: PartyAction2_Form0States 0x65FA74 by +2.
extern "C" void __cdecl PartyAction2_Form0(void) { CodeAt(at::kAction2Form0States, Sc()[2], "PartyAction2_Form0 (0x51CCF0)")(); }
// original 0x51D1E0: 0x65FA8C by +2 (0x5252B0, 0x521A20).
extern "C" void __cdecl PartyAction2_Form1(void) { CodeAt(at::kAction2Form1States, Sc()[2], "PartyAction2_Form1 (0x51D1E0)")(); }
// original 0x51D220: 0x65FAA0 by +2 (PartyAction2_Form2State0, PartyAction2_Form2State1).
extern "C" void __cdecl PartyAction2_Form2(void) { CodeAt(at::kAction2Form2States, Sc()[2], "PartyAction2_Form2 (0x51D220)")(); }
// original 0x51D240: 0x65FAA8 by +3 (PartyAction2_Form2Aim, PartyAction2_Form2Strike,
// PartyAction_FinishPalette, 0x51F850, 0x522DE0).
extern "C" void __cdecl PartyAction2_Form2State0(void) { CodeAt(at::kAction2Form2State0Steps, Sc()[3], "PartyAction2_Form2State0 (0x51D240)")(); }
// original 0x51D670: 0x65FABC by +3 (PartyAction2_Form2Reaim, 0x523F10, PartyAction_WaitEffect).
extern "C" void __cdecl PartyAction2_Form2State1(void) { CodeAt(at::kAction2Form2State1Steps, Sc()[3], "PartyAction2_Form2State1 (0x51D670)")(); }

// ===========================================================================
// The form actions' states (shared: each sits in several sets' tables)
// ===========================================================================

// original 0x51BEB0 (0xFC bytes): the form-0 tables' state 0 (seven cells,
// sets 0, 1, 2, 3 and 5). The pose +4: 0x41 unless bit 0x16 of Cond_Flags'
// row 1 (Flags_Test(0x903F98, 0x16)); with it, 0x50 when Party_Count(0) is 1
// and Field_InputFlags has no 0x40, else 0x40. Then by that pose: 0x40 -
// facing 3; 0x41 - facing 5; 0x50 - facing 3 when +8 is farther from 5 than
// from 3 (|d - 5| > |d - 3|), else facing 5 and the pose 0x52. A facing f:
// +0xB = Sprite_TurnSense(f), +3 = f. Then +9 = 2 and +2 one on.
extern "C" void __cdecl PartyFormAction_Form0Begin(void) {
    if (SH_CALL(Flags_Test)(At(at::kCondRow1), 0x16) != 0) {
        if (static_cast<unsigned char>(SH_CALL(Party_Count)(0)) == 1 && (Field_InputFlags & 0x40) == 0) Sc()[4] = 0x50;
        else Sc()[4] = 0x40;
    } else {
        Sc()[4] = 0x41;
    }
    unsigned facing = 0;
    bool pose52 = false;
    switch (Sc()[4]) {
    case 0x40: facing = 3; break;
    case 0x41: facing = 5; break;
    case 0x50: {
        const int d = Sc()[8];
        if (Abs32(d - 5) > Abs32(d - 3)) {
            facing = 3;
        } else {
            facing = 5;
            pose52 = true;
        }
        break;
    }
    default: break;
    }
    if (facing != 0) {
        const unsigned char sense = SH_CALL(Sprite_TurnSense)(facing);
        Sc()[0xB] = sense;
        Sc()[3] = static_cast<unsigned char>(facing);
        if (pose52) Sc()[4] = 0x52;
    }
    Sc()[9] = 2;
    ++Sc()[2];
}

// original 0x51CC60 (0x83 bytes): the form-0 tables' state 1. When +8 has come
// round to +3: the pose +4 - Sprite_SetAnimationAt(0x50, 2) for 0x50, else
// Sprite_SetAnimation(+4) - and +2 one on. Else +9 counted down and, at 0, +9 =
// 2, +8 += +0xB, & 7, and the pose +8 (Sprite_EnsureAnimation).
extern "C" void __cdecl PartyFormAction_Form0Turn(void) {
    unsigned char* const s = Sc();
    if (s[8] == s[3]) {
        const unsigned char pose = s[4];
        if (pose == 0x50) SH_CALL(Sprite_SetAnimationAt)(0x50, 2);
        else SH_CALL(Sprite_SetAnimation)(pose);
        ++Sc()[2];
        return;
    }
    --s[9];
    if (Sc()[9] != 0) return;
    Sc()[9] = 2;
    Sc()[8] = static_cast<unsigned char>(Sc()[8] + Sc()[0xB]);
    Sc()[8] = static_cast<unsigned char>(Sc()[8] & 7);
    SH_CALL(Sprite_EnsureAnimation)(Sc()[8]);
}

// original 0x51D0F0 (0x70 bytes): the form-1 tables' state 0 (sets 0, 1, 2):
// facing 7 when +8 is farther from 1 than from 7 (|d - 1| > |d - 7|), else
// facing 1: +0xB = Sprite_TurnSense(facing), +3 = facing; +9 = 2, +2 one on.
extern "C" void __cdecl PartyFormAction_Form1Begin(void) {
    const int d = Sc()[8];
    const unsigned facing = Abs32(d - 1) > Abs32(d - 7) ? 7 : 1;
    const unsigned char sense = SH_CALL(Sprite_TurnSense)(facing);
    Sc()[0xB] = sense;
    Sc()[3] = static_cast<unsigned char>(facing);
    Sc()[9] = 2;
    ++Sc()[2];
}

// original 0x51D160 (0x7C bytes): the form-1 tables' state 1 - TurnToward
// with facing 1 posing 0x40 and facing 7 posing 0x41.
extern "C" void __cdecl PartyFormAction_Form1Turn(void) { TurnToward(1, 0x40, 7, 0x41); }

// original 0x51C490 (0x7E bytes): state 1 of set 0's form-2 form action and of
// 13 more cells (sets 2 and 5) - TurnToward with facing 3 posing 0x41 and
// facing 5 posing 0x40.
extern "C" void __cdecl PartyFormAction_Form2Turn(void) { TurnToward(3, 0x41, 5, 0x40); }

// ===========================================================================
// The actions' states
// ===========================================================================

// originals 0x51BFD0 / 0x51C7C0 / 0x51CD10 (0x1B1 bytes each): sets 0, 1 and
// 2's form-0 state 0 (Form0Begin above).
extern "C" void __cdecl PartyAction0_Form0Begin(void) { Form0Begin(); }
extern "C" void __cdecl PartyAction1_Form0Begin(void) { Form0Begin(); }
extern "C" void __cdecl PartyAction2_Form0Begin(void) { Form0Begin(); }

// originals 0x51C270 / 0x51CA60 / 0x51CFB0 (0x11F bytes each): sets 0, 1 and
// 2's cell pickup (CellPickup above), each called by its own set's resolve.
extern "C" unsigned char __cdecl PartyAction0_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }
extern "C" unsigned char __cdecl PartyAction1_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }
extern "C" unsigned char __cdecl PartyAction2_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }

// originals 0x51C190 / 0x51C980 / 0x51CED0 (0xDB bytes each): sets 0, 1 and
// 2's form-0 state 1 (Form0Resolve above), each with its own set's pickup.
extern "C" void __cdecl PartyAction0_Form0Resolve(void) {
    Form0Resolve([](unsigned x, unsigned z) -> unsigned char { return SH_CALL(PartyAction0_CellPickup)(x, z); },
                 "PartyAction0_Form0Resolve (0x51C190)");
}
extern "C" void __cdecl PartyAction1_Form0Resolve(void) {
    Form0Resolve([](unsigned x, unsigned z) -> unsigned char { return SH_CALL(PartyAction1_CellPickup)(x, z); },
                 "PartyAction1_Form0Resolve (0x51C980)");
}
extern "C" void __cdecl PartyAction2_Form0Resolve(void) {
    Form0Resolve([](unsigned x, unsigned z) -> unsigned char { return SH_CALL(PartyAction2_CellPickup)(x, z); },
                 "PartyAction2_Form0Resolve (0x51CED0)");
}

// original 0x51C530 (0x166 bytes): set 0's form-2 state 0. With an effect
// object of kind 0x30 lined up ahead (PartyAction_Kind30Ahead, not 0xFF):
// when a member stands on it (PartyAction_MemberOnEffect) or beyond it
// (PartyAction_MemberBeyondEffect), Field_State +0x137 = 0 and nothing more;
// else its +0xB = 1, Sound_PlayEffect(u16 +0x2C + 0x100), the pose +8 + 8,
// Field_State +0x128 = 2, Field_ScriptFlags' byte 3 |= 0x10, Field_JumpStart,
// +9 one down, Field_LeaderStepTick, Field_State +0x137 = 1, +2 one on. With
// none: the point two steps ahead (computed first); unless Field_State +0x138
// bit 0, the object Sprite_ObjectAt(point, margin 1) finds is marked (the
// index compared unsigned); Field_State +0x137 = 0.
extern "C" void __cdecl PartyAction0_Form2Begin(void) {
    const unsigned char k = SH_CALL(PartyAction_Kind30Ahead)();
    if (k != 0xFF) {
        if (SH_CALL(PartyAction_MemberOnEffect)(k) != 0 || SH_CALL(PartyAction_MemberBeyondEffect)(k) != 0) {
            Fs()[0x137] = 0;
            return;
        }
        EffectByAnswer(k, "PartyAction0_Form2Begin (0x51C530)")[0xB] = 1;
        SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(Sc() + 0x2C) + 0x100));
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(Sc()[8] + 8));
        Fs()[0x128] = 2;
        B(at::kScriptFlags3) = static_cast<unsigned char>(B(at::kScriptFlags3) | 0x10);
        SH_CALL(Field_JumpStart)();
        --Sc()[9];
        SH_CALL(Field_LeaderStepTick)();
        Fs()[0x137] = 1;
        ++Sc()[2];
        return;
    }
    const unsigned char* const s = Sc();
    const unsigned d = s[8];
    const U x = static_cast<U>(Long(s + 0x34)) + 2u * StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + 2u * StepZ(d);
    if ((Fs()[0x138] & 1) == 0) {
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 1);
        if (found != 0xFF) MarkObject(found, "PartyAction0_Form2Begin (0x51C530)");
    }
    Fs()[0x137] = 0;
}

// original 0x51D260 (0x87 bytes): set 2's form-2 step 0 (+3). An even
// direction +8 is turned one eighth back (-1, & 7); when
// PartyAction_BlockedAhead finds that way open, two on (+2), and when that is
// open too, back to the first turn (-2). Then PartyAction_SideProbes, the pose
// (+8 - 1) / 2 + 0x42, +0xB = 0, +0xA = 0xB, +3 = 1.
extern "C" void __cdecl PartyAction2_Form2Aim(void) {
    if ((Sc()[8] & 1) == 0) {
        Sc()[8] = static_cast<unsigned char>((Sc()[8] - 1) & 7);
        if (SH_CALL(PartyAction_BlockedAhead)() == 0) {
            Sc()[8] = static_cast<unsigned char>((Sc()[8] + 2) & 7);
            if (SH_CALL(PartyAction_BlockedAhead)() == 0) Sc()[8] = static_cast<unsigned char>((Sc()[8] - 2) & 7);
        }
    }
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x42));
    Sc()[0xB] = 0;
    Sc()[0xA] = 0xB;
    Sc()[3] = 1;
}

// original 0x51D690 (0x35 bytes): set 2's form-2 state 1, step 0:
// PartyAction_SideProbes, the pose (+8 - 1) / 2 + 0x42, +0xA = 0xB, +3 = 1
// (PartyAction2_Form2Aim without the turn and +0xB).
extern "C" void __cdecl PartyAction2_Form2Reaim(void) {
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x42));
    Sc()[0xA] = 0xB;
    Sc()[3] = 1;
}

// original 0x51D4E0 (0x188 bytes): set 2's cell strike - what lies in the map
// cell (x, z), AreaMap_ByteAt:
//   0xF0, 0xF1, 0xF4 - Effect_SpawnAtCellHigh(0, x, z); a second, state 4,
//          when Rand & 7 is above 5; Sound_PlayEffect(0x10B); al 1.
//   0xF6, 0xF7 - Effect_SpawnAtCellHigh(0, x, z), Sound_PlayEffect(0x10B); a
//          Rand nibble below 7: Effect_SpawnAtCellHigh(3, x, z), the name of
//          item 0x29 of category 0 into Text_Records (16 bytes),
//          Inventory_Add(0, 0x29, 1): taken, Sound_PlayEffect(0x106) and
//          Msg_OpenSystem(2), else Msg_OpenSystem(3); +0xB = 2. Above 0xB:
//          Effect_SpawnAtCellHigh(2, x, z), Sprite_FlashClut(0), Char_LoseHp(1,
//          Field_State +0x89), Msg_OpenSystem(0xD9), +0xB = 1. 7..0xB nothing
//          more. Then Field_Request = 2; al 1.
//   any other - al 0.
// As the original has it: x and z go on as given (each callee reads a word);
// it stores the cell byte into its own z slot's low byte and pushes that dword
// as a fourth argument to Effect_SpawnAtCellHigh, which reads three;
// Inventory_Add is pushed a fourth dword 0, never read.
extern "C" unsigned char __cdecl PartyAction2_CellStrike(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF0 || cell == 0xF1 || cell == 0xF4) {
        SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
        if ((static_cast<unsigned char>(SH_CALL(Rand)()) & 7u) > 5) SH_CALL(Effect_SpawnAtCellHigh)(4, x, z);
        SH_CALL(Sound_PlayEffect)(0x10B);
        return 1;
    }
    if (cell != 0xF6 && cell != 0xF7) return 0;
    SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
    SH_CALL(Sound_PlayEffect)(0x10B);
    const unsigned roll = static_cast<unsigned char>(SH_CALL(Rand)()) & 0xFu;
    if (roll < 7) {
        SH_CALL(Effect_SpawnAtCellHigh)(3, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x29);
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(bof3::addr::Text_Records + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x29, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Sc()[0xB] = 2;
    } else if (roll > 0xB) {
        SH_CALL(Effect_SpawnAtCellHigh)(2, x, z);
        SH_CALL(Sprite_FlashClut)(0);
        SH_CALL(Char_LoseHp)(1, Fs()[0x89]);
        SH_CALL(Msg_OpenSystem)(0xD9);
        Sc()[0xB] = 1;
    }
    Field_Request = 2;
    return 1;
}

// original 0x51D2F0 (0x14C bytes): set 2's form-2 step 1 (+3). With +0xA not
// 0, it is counted down; reaching 0: Sound_PlayEffect(u16 +0x2C + 0x100); the
// effect object ahead (Field_EffectAhead): with one, Sound_PlayEffect(0x10B),
// its +8 = the sprite's +8 and +0xA = 1, the sprite's +6 = its index, and +3
// one on; with none, the object Sprite_ObjectAt(two steps ahead, margin 0)
// finds is marked (signed) with Sound_PlayEffect(0x10B), then
// PartyAction2_CellStrike on the point's cell, the cell one on in x (x with a
// fraction) and one on in z (z with one), each only while the earlier
// answered 0 - as the form-0 resolve. Then +3 one on (so two with an effect
// object ahead). Sprite_ScriptTickOnce every time.
extern "C" void __cdecl PartyAction2_Form2Strike(void) {
    const unsigned char t = Sc()[0xA];
    if (t != 0) {
        Sc()[0xA] = static_cast<unsigned char>(t - 1);
        if (Sc()[0xA] == 0) {
            SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(Sc() + 0x2C) + 0x100));
            const unsigned char e = SH_CALL(Field_EffectAhead)();
            if (e != 0xFF) {
                SH_CALL(Sound_PlayEffect)(0x10B);
                unsigned char* const s = Sc();
                unsigned char* const rec = EffectByAnswer(e, "PartyAction2_Form2Strike (0x51D2F0)");
                rec[8] = s[8];
                rec[0xA] = 1;
                s[6] = e;
                ++Sc()[3];
            } else {
                const unsigned char* const s = Sc();
                const unsigned d = s[8];
                const U x = static_cast<U>(Long(s + 0x34)) + 2u * StepX(d);
                const U z = static_cast<U>(Long(s + 0x38)) + 2u * StepZ(d);
                const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
                if (found != 0xFF) {
                    MarkObject(found, "PartyAction2_Form2Strike (0x51D2F0)");
                    SH_CALL(Sound_PlayEffect)(0x10B);
                }
                const unsigned cx = x >> 16, cz = z >> 16;
                if (SH_CALL(PartyAction2_CellStrike)(cx, cz) == 0) {
                    bool done = false;
                    if ((x & 0xFFFF) != 0) done = SH_CALL(PartyAction2_CellStrike)(cx + 1, cz) != 0;
                    if (!done && (z & 0xFFFF) != 0) SH_CALL(PartyAction2_CellStrike)(cx, cz + 1);
                }
            }
            ++Sc()[3];
        }
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x51D440 (0x94 bytes): an action's last state (nine cells: sets 2
// and 5; PSX twins in the PLP overlays, 0x801CEA04 among them, pairs by
// callers). With +0xB 1: Sprite_LoadPalette(0x80D380 + +5 * 0x40, 0) and +0xB
// = 2. Then by +0xB: 0 - Sprite_ScriptTickOnce, the end when it answers not 0;
// 2 - Sprite_ScriptTickOnce, and when it answers not 0 Sprite_SetAnimation(+8)
// and +0xB = 3; then the end unless Field_Request is set; any other -
// Sprite_ScriptTick, then the end unless Field_Request is set. The end: +0x2B
// = 0 and Field_State +0x137 = 0 (PartyAction_Finish with the palette put back
// first).
extern "C" void __cdecl PartyAction_FinishPalette(void) {
    if (Sc()[0xB] == 1) {
        SH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(static_cast<std::uintptr_t>(at::kPalettes + (static_cast<U>(Sc()[5]) << 6))), 0);
        Sc()[0xB] = 2;
    }
    const unsigned char b = Sc()[0xB];
    if (b == 0) {
        if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    } else if (b == 2) {
        if (SH_CALL(Sprite_ScriptTickOnce)() != 0) {
            SH_CALL(Sprite_SetAnimation)(Sc()[8]);
            Sc()[0xB] = 3;
        }
        if (Field_Request != 0) return;
    } else {
        SH_CALL(Sprite_ScriptTick)();
        if (Field_Request != 0) return;
    }
    Sc()[0x2B] = 0;
    Fs()[0x137] = 0;
}

// original 0x51D6D0 (0x3D bytes): a state of seven cells (sets 2 and 5, set
// 2's form-2 state 1 step 2): the effect object +0xB names - when it is free
// (+0) or in its state 1 (+1), and Field_Kind2Hold is 0, +0x2B = 0 and
// Field_State +0x137 = 0. Sprite_ScriptTick every time. As the original has
// it: +0xB indexes Effect_Objects unchecked (a read; docs/rest_1a.md section 5).
extern "C" void __cdecl PartyAction_WaitEffect(void) {
    unsigned char* const s = Sc();
    const unsigned char* const e = Effect_Objects + s[0xB] * 0x80u;
    if ((e[0] == 0 || e[1] == 1) && Field_Kind2Hold == 0) {
        s[0x2B] = 0;
        Fs()[0x137] = 0;
    }
    SH_CALL(Sprite_ScriptTick)();
}

void Rest1A_Inject() {
    if (bof3::WantsShadow("rest_1a")) rest_1a::SelfTest();
    BOF3_INJECT(Member_ResumeUnlessHeld800);
    BOF3_INJECT(Member_FormActionState);
    BOF3_INJECT(Member_JumpState);
    BOF3_INJECT(Member_JumpAir);
    BOF3_INJECT(PartyFormAction0_Form0);
    BOF3_INJECT(PartyFormAction_Form0Begin);
    BOF3_INJECT(PartyAction0_Form0);
    BOF3_INJECT(PartyAction0_Form0Begin);
    BOF3_INJECT(PartyAction0_Form0Resolve);
    BOF3_INJECT(PartyAction0_CellPickup);
    BOF3_INJECT(PartyFormAction0_Form1);
    BOF3_INJECT(PartyAction0_Form1);
    BOF3_INJECT(PartyFormAction0_Form2);
    BOF3_INJECT(PartyFormAction_Form2Turn);
    BOF3_INJECT(PartyAction0_Form2);
    BOF3_INJECT(PartyAction0_Form2Begin);
    BOF3_INJECT(PartyFormAction0_ByForm);
    BOF3_INJECT(PartyAction0_ByForm);
    BOF3_INJECT(PartyFormAction1_Form0);
    BOF3_INJECT(PartyAction1_Form0);
    BOF3_INJECT(PartyAction1_Form0Begin);
    BOF3_INJECT(PartyAction1_Form0Resolve);
    BOF3_INJECT(PartyAction1_CellPickup);
    BOF3_INJECT(PartyFormAction1_Form1);
    BOF3_INJECT(PartyAction1_Form1);
    BOF3_INJECT(PartyFormAction1_Form2);
    BOF3_INJECT(PartyAction1_Form2);
    BOF3_INJECT(PartyFormAction1_ByForm);
    BOF3_INJECT(PartyAction1_ByForm);
    BOF3_INJECT(PartyFormAction2_Form0);
    BOF3_INJECT(PartyFormAction_Form0Turn);
    BOF3_INJECT(PartyAction2_Form0);
    BOF3_INJECT(PartyAction2_Form0Begin);
    BOF3_INJECT(PartyAction2_Form0Resolve);
    BOF3_INJECT(PartyAction2_CellPickup);
    BOF3_INJECT(PartyFormAction2_Form1);
    BOF3_INJECT(PartyFormAction_Form1Begin);
    BOF3_INJECT(PartyFormAction_Form1Turn);
    BOF3_INJECT(PartyAction2_Form1);
    BOF3_INJECT(PartyFormAction2_Form2);
    BOF3_INJECT(PartyAction2_Form2);
    BOF3_INJECT(PartyAction2_Form2State0);
    BOF3_INJECT(PartyAction2_Form2Aim);
    BOF3_INJECT(PartyAction2_Form2Strike);
    BOF3_INJECT(PartyAction_FinishPalette);
    BOF3_INJECT(PartyAction2_CellStrike);
    BOF3_INJECT(PartyAction2_Form2State1);
    BOF3_INJECT(PartyAction2_Form2Reaim);
    BOF3_INJECT(PartyAction_WaitEffect);
}
